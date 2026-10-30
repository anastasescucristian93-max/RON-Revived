/*
 * Hover highlight: the unit or building under the cursor gets the same mark as a selected
 * one (the circle under a unit, the rectangle around a building), coloured by owner: your
 * own white, allies yellow, everyone else red.
 *
 * UnitOut::draw_underlays and WallOut::draw_underlays (buildings and walls) draw the
 * selection mark when GroupData::member says the object is in the local selection, in the
 * owner's team colour (TeamColor::get_iface for units, get_unit for buildings). Those two
 * member calls are pointed at hover_member, which also answers yes for the object under
 * the cursor (MouseOver: +4 bit 4 = an object, +0x58 its id, +0x4c its player), and the
 * colour call right after it is pointed at hover_color, which then gives the relation
 * colour instead. Nothing else reads those two answers. For a unit, any soldier of the hovered
 * squad counts (squad_hovered, squadhp.c), so the whole squad gets the circle.
 *
 * Only the drawing is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <string.h>

#define VA_MEMBER       0x0070F8F0u /* GroupData::member(id, player, alive) const, ret 0xC */
#define VA_GET_IFACE    0x008C3010u /* TeamColor::get_iface(Color *out) const, ret 4 */
#define VA_GET_UNIT     0x008C2DB0u /* TeamColor::get_unit(Color *out) const, ret 4 */
#define VA_IS_ALLY      0x006EDB50u /* LeaderData::is_ally(int player) const, ret 4 */
#define VA_MOUSEOVER    0x00C06230u /* MouseOver * */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player */
#define VA_LEADERS_PTR  0x00C061E0u /* LeaderData[8], 0x6EEC each */
#define LEADER_SIZE     0x6EEC

/* Color is 12 bytes (0xAABBGGRR, then 16-bit forms); the game's own constants */
#define VA_IFACE_WHITE  0x00C8CF0Cu
#define VA_IFACE_YELLOW 0x00C8D08Cu
#define VA_IFACE_RED    0x00C8CF18u
#define VA_UNIT_WHITE   0x00C8CE1Cu
#define VA_UNIT_YELLOW  0x00C8CF9Cu
#define VA_UNIT_RED     0x00C8CE28u

#define MO_FLAGS  0x04 /* bit 4: the cursor is over an object */
#define MO_PLAYER 0x4C
#define MO_ID     0x58

typedef int(__attribute__((thiscall)) *member_t)(void *group, int id, int player, int alive);
typedef void *(__attribute__((thiscall)) *color_t)(void *team, void *out);
typedef int(__attribute__((thiscall)) *is_ally_t)(void *leader, int player);

int g_hover_enabled;
static int g_pending = -1; /* owner of the hovered object whose mark is being drawn */

void hover_load_config(const char *ini_path)
{
    g_hover_enabled = GetPrivateProfileIntA("qol", "hover_highlight", 1, ini_path);
}

static int hovered(int id, int player)
{
    char *mo = *(char **)VA(VA_MOUSEOVER);
    return mo && (*(int *)(mo + MO_FLAGS) & 4) && *(int *)(mo + MO_ID) == id && *(int *)(mo + MO_PLAYER) == player;
}

__attribute__((used, thiscall)) int c_hover_member(void *group, int id, int player, int alive)
{
    g_pending = -1;
    int r = ((member_t)VA(VA_MEMBER))(group, id, player, alive);
    if (r || !hovered(id, player))
        return r;
    g_pending = player;
    return 1;
}

/* the same for units: every soldier of the hovered squad (squadhp.c) */
__attribute__((used, thiscall)) int c_hover_member_unit(void *group, int id, int player, int alive)
{
    g_pending = -1;
    int r = ((member_t)VA(VA_MEMBER))(group, id, player, alive);
    if (r || !squad_hovered(id, player))
        return r;
    g_pending = player;
    return 1;
}

/* 0 = own, 1 = ally, 2 = anyone else (enemies, nature) */
static int relation(int player)
{
    char *console = *(char **)VA(VA_CONSOLE);
    char *leaders = *(char **)VA(VA_LEADERS_PTR);
    if (!console || !leaders)
        return 2;
    int me = *(int *)(console + 0x298);
    if (player == me)
        return 0;
    if (me < 0 || me >= 8 || player <= 0)
        return 2;
    return ((is_ally_t)VA(VA_IS_ALLY))(leaders + me * LEADER_SIZE, player) ? 1 : 2;
}

int hover_relation(int player) { return relation(player); }

static void *hover_color(void *team, void *out, UINT_PTR orig, const UINT_PTR colors[3])
{
    ((color_t)VA(orig))(team, out);
    if (g_pending >= 0) {
        memcpy(out, (void *)VA(colors[relation(g_pending)]), 12);
        g_pending = -1;
    }
    return out;
}

__attribute__((used, thiscall)) void *c_hover_iface(void *team, void *out)
{
    static const UINT_PTR c[3] = { VA_IFACE_WHITE, VA_IFACE_YELLOW, VA_IFACE_RED };
    return hover_color(team, out, VA_GET_IFACE, c);
}

__attribute__((used, thiscall)) void *c_hover_unit(void *team, void *out)
{
    static const UINT_PTR c[3] = { VA_UNIT_WHITE, VA_UNIT_YELLOW, VA_UNIT_RED };
    return hover_color(team, out, VA_GET_UNIT, c);
}

static const struct {
    UINT_PTR at, target;
    void *repl;
} k_sites[4] = {
    { 0x00614319u, VA_MEMBER, (void *)c_hover_member_unit }, /* UnitOut::draw_underlays */
    { 0x00614346u, VA_GET_IFACE, (void *)c_hover_iface }, /* its circle colour */
    { 0x006412DFu, VA_MEMBER, (void *)c_hover_member },   /* WallOut::draw_underlays */
    { 0x0064130Du, VA_GET_UNIT, (void *)c_hover_unit },   /* its rectangle colour */
};

/* Called from install_hooks once the exe is known to be the supported version. */
void hover_install(void)
{
    if (!g_hover_enabled)
        return;
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(k_sites[i].target)) {
            logf_("hover: call site %d mismatch, hover highlight turned off", i);
            g_hover_enabled = 0;
            return;
        }
    }
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        DWORD old;
        VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(at + 1) = (INT32)((UINT_PTR)k_sites[i].repl - (UINT_PTR)(at + 5));
        VirtualProtect(at, 5, old, &old);
    }
    logf_("hover highlight installed");
}
