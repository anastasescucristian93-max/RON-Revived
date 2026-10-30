/*
 * The Senator bribes on right-click, like the Spy: with a Spy selected, the cursor over an
 * enemy unit that can be bribed is the bribe cursor and a right-click bribes it. The Senator
 * (a patriot) casts the same Bribe (its FROM2 in craftrules.xml), but the game gave it only
 * the button: Console::execute_at_cursor opens its spy block only when
 * GroupData::count(0x11, Spy, 0) finds a Spy in the selection.
 *
 * That one count call is pointed at c_bribe_count. When it finds no Spy, but the selection
 * has a unit of Bribe's FROM2 and the object under the cursor is an enemy unit the spy block
 * would bribe, it answers with that count. The game's own code then shows the bribe cursor
 * (0x1F) and, on a right-click, issues the Bribe with GroupOut::issue_spell, as the button
 * does. The spy block's other targets (enemy spies, citizens, buildings) are left out, so
 * the Senator keeps its normal cursor over them.
 *
 * Only the local player's right-click changes, and it sends a normal game order.
 */
#include "ron.h"
#include <string.h>

#define VA_COUNT_SITE   0x007C947Au /* Console::execute_at_cursor: count(0x11, Spy, 0) */
#define VA_GROUP_COUNT  0x00711720u /* GroupData::count(index, int, int) const, ret 0xC */
#define VA_FIND_TARGET  0x007CBA20u /* Console::find_target(int *player, 8 more), ret 0x24 */
#define VA_IS_ENEMY     0x006EBAA0u /* LeaderData::is_enemy(int player) const, ret 4 */
#define VA_IS_WORKER    0x0046FA10u /* ObjectData::is_worker() const */
#define VA_IS_PEASANT   0x0046D310u /* ObjectData::is_peasant() const */
#define VA_VALID_TARGET 0x006763C0u /* SpellTypeData::is_valid_target(me, id, player, int *), ret 0x10 */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player */
#define VA_LEADERS_PTR  0x00C061E0u /* LeaderData[8], 0x6EEC each */
#define VA_TYPES        0x00C0AB10u /* TypeData *[type] */
#define VA_OBJECTS      0x00C0AB84u /* per player, 0x1C apart: ObjectData *[id] */
#define LEADER_SIZE     0x6EEC

#define COUNT_IS_TYPE 0x11  /* GroupData::count: members that are (ObjectData::is) the type */
#define TYPE_SPY      0x3A
#define CRAFT_BRIBE   0x275
#define SPELL_FROM2   0x1DC /* SpellTypeData: FROM2 type (FROM is +0x3C) */
#define TYPE_218      0x218 /* in a unit's type data; the spy block bribes only when it is 0 */

/* ObjectData virtuals (UnitData's vtable) */
#define VT_IS_UNIT    0x18
#define VT_IS         0xB8 /* is(type, int) */
#define VT_IS_HERO    0xC4
#define VT_IS_CARAVAN 0xD0
#define VT_ATTACK     0x120

typedef int(__attribute__((thiscall)) *count_t)(void *group, int index, int a, int b);
typedef int(__attribute__((thiscall)) *find_t)(void *console, int *player, int, int, int, int, int, int, int, int);
typedef int(__attribute__((thiscall)) *is_enemy_t)(void *leader, int player);
typedef int(__attribute__((thiscall)) *obj_fn_t)(void *obj);
typedef int(__attribute__((thiscall)) *obj_is_t)(void *obj, int type, int b);
typedef int(__attribute__((thiscall)) *valid_t)(void *spell, int me, int id, int player, int *out);

int g_bribe_enabled;

void bribe_load_config(const char *ini_path)
{
    g_bribe_enabled = GetPrivateProfileIntA("qol", "senator_bribe", 1, ini_path);
}

static int vcall(void *obj, int off)
{
    return ((obj_fn_t)(*(void ***)obj)[off / 4])(obj);
}

/* The spy block's bribe conditions for the object under the cursor (its cursor branch, which
   asks more than its right-click branch; a citizen would get the Informer instead). */
static int bribe_target(void)
{
    char *console = *(char **)VA(VA_CONSOLE);
    char *leaders = *(char **)VA(VA_LEADERS_PTR);
    char **types = *(char ***)VA(VA_TYPES);
    if (!console || !leaders || !types || !types[CRAFT_BRIBE])
        return 0;
    int me = *(int *)(console + 0x298);
    int player = -1;
    int id = ((find_t)VA(VA_FIND_TARGET))(console, &player, 0, 3, 0, 0, 0, 0, 0, 0);
    if (id < 0 || me < 0 || me >= 8 || player < 0 || player >= 8 || player == me)
        return 0;
    if (!((is_enemy_t)VA(VA_IS_ENEMY))(leaders + me * LEADER_SIZE, player))
        return 0;
    char **objects = *(char ***)(VA(VA_OBJECTS) + player * 0x1C);
    char *obj = objects ? objects[id] : 0;
    if (!obj || !vcall(obj, VT_IS_UNIT))
        return 0;
    if (((obj_is_t)(*(void ***)obj)[VT_IS / 4])(obj, TYPE_SPY, 0))
        return 0;
    if (((obj_fn_t)VA(VA_IS_WORKER))(obj) || ((obj_fn_t)VA(VA_IS_PEASANT))(obj) || vcall(obj, VT_IS_CARAVAN))
        return 0;
    if (!vcall(obj, VT_ATTACK) && !vcall(obj, VT_IS_HERO))
        return 0;
    if (*(int *)(*(char **)(obj + 0x18) + TYPE_218) != 0)
        return 0;
    return ((valid_t)VA(VA_VALID_TARGET))(types[CRAFT_BRIBE], me, id, player, 0) != 0;
}

__attribute__((used, thiscall)) int c_bribe_count(void *group, int index, int type, int b)
{
    count_t count = (count_t)VA(VA_GROUP_COUNT);
    int r = count(group, index, type, b);
    if (r)
        return r;
    char **types = *(char ***)VA(VA_TYPES);
    int from2 = types && types[CRAFT_BRIBE] ? *(int *)(types[CRAFT_BRIBE] + SPELL_FROM2) : -1;
    if (from2 < 0 || from2 == TYPE_SPY)
        return 0;
    int n = count(group, COUNT_IS_TYPE, from2, 0);
    return n && bribe_target() ? n : 0;
}

/* Called from install_hooks once the exe is known to be the supported version. */
void bribe_install(void)
{
    if (!g_bribe_enabled)
        return;
    unsigned char *at = (unsigned char *)VA(VA_COUNT_SITE);
    /* push 0 / push 0x3A / push 0x11 at 0x007C9464, then the call */
    static const unsigned char push[6] = { 0x6A, 0x00, 0x6A, 0x3A, 0x6A, 0x11 };
    if (memcmp(at - 0x16, push, 6) != 0 || at[0] != 0xE8 ||
        (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(VA_GROUP_COUNT)) {
        logf_("bribe: call site mismatch, Senator bribe on right-click turned off");
        g_bribe_enabled = 0;
        return;
    }
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)c_bribe_count - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    logf_("Senator bribe on right-click installed");
}
