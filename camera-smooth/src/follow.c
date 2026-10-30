/*
 * Units that walk after a unit that moves keep up with it, instead of walking to where it was.
 *
 * Out of range, an order on a unit puts a move in front of itself, to a spot by where the target
 * stands at that moment, and walks there. Unit::work looks at the target again only every 16
 * frames (Unit::check_target_path) and, for a few kinds, not even then: the unit walks to an old
 * spot, stops, plans a new one and walks again, turning back when the target turned.
 *  - Casts (cast_follow; Bribe, Counterintelligence, ...): check_target_path only checks that the
 *    target is still valid. Its call in Unit::work (0x0060D707) is pointed at c_check_target:
 *    after the game's check, the walk is dropped (Unit::repath, as the attack does) when the
 *    target is already in casting range, so the cast starts now, or when the walk ends out of
 *    its range, so Unit::do_cast plans a new spot by where the target is now.
 *  - Follow (escort_follow): check_target_path plans again only when the target is 12 tiles off
 *    the walk's goal. c_check_target drops the walk once its end is more than 2 tiles beyond the
 *    distance Unit::do_follow keeps (computed as do_follow does).
 *  - Guard (escort_follow; the player's escort, and the units around siege in an attack with
 *    artillery): Unit::work never calls check_target_path for it, it repaths the guard every 64
 *    frames (about 4 s). That branch (0x0060D694) calls c_guard_tick instead, which keeps the
 *    64-frame repath and, every 16 frames between, drops the walk once its end is more than
 *    4 tiles beyond the guard's place by the guarded unit, so Unit::do_guard plans a new one.
 * The end of the walk is where the action starts, `+0x70/+0x74`, which Unit::update_action sets
 * just before these checks. Nothing is dropped while the target stands still. Simulation only,
 * from the game's state: every player needs the same settings.
 */
#include "ron.h"
#include <string.h>

#define VA_TARGET_SITE  0x0060D707u /* Unit::work: check_target_path(TargetOrder *), every 16 frames */
#define VA_GUARD_SITE   0x0060D694u /* Unit::work: action kind 0xC, the 64-frame repath (0x28 bytes) */
#define VA_GUARD_SKIP   0x0060D72Du /* Unit::work: after the checks */
#define VA_GUARD_AGAIN  0x0060D710u /* Unit::work: orders changed, update_order again */
#define VA_CHECK_TARGET 0x005E22D0u /* Unit::check_target_path(TargetOrder *), ret 4: 1 = orders changed */
#define VA_REPATH       0x005E29B0u /* Unit::repath(): drops the moves in front of the action */
#define VA_UPDATE_ORDER 0x006179D0u /* Unit::update_order(): the current (front) order */
#define VA_GET_RANGE    0x00676A80u /* SpellTypeData::get_range(int, me, id, player), ret 0x10 */
#define VA_SPEED        0x0060AAE0u /* UnitData::speed() const */
#define VA_SPELLTYPES   0x00C061A8u /* PtrArray<SpellType> *: +0x10 = the spells by type */
#define VA_GAME         0x00C061ECu /* Game *: +0x550 = the frame */
#define VA_OBJECTS      0x00C0AB84u /* per player, 0x1C apart: ObjectData *[id] */
#define VA_UNITS        0x00C0AEC0u /* per player, 0x1C apart: the same objects as do_follow reads them */

#define ORDER_FOLLOW    0xB         /* UnitOrder kinds (virtual +0x10) */
#define ORDER_GUARD     0xC
#define ORDER_CAST      0xE
#define OVT_KIND        0x10
#define OVT_GUARD_DATA  0x74        /* guard order: +8 id, +0xC player, +0x14/+0x18 the place's offset */
#define OVT_CAST_DATA   0x7C        /* cast order: +0x20 = the craft */
#define SPELL_FLAGS     0x1C8       /* 0xE: has a range; 8: cast on a spot */
#define T_RADIUS        0x244       /* a unit type's radius, added to the range as do_cast does */
#define VT_IS_BUILDING  0x1C
#define VT_IS_ON_MAP    0xBC
#define VT_D8           0xD8        /* do_follow keeps twice the margin when it is set on the target */
#define VT_SIZE         0x128       /* do_follow's distances are this times 384 (and 96 or 153) */
#define U_ACTION_X      0x70        /* plain x, y where the action starts: the end of the walk */
#define U_ACTION_Y      0x74
#define COORD_KEY       0x63637
#define TILE            0xC0

typedef int(__attribute__((thiscall)) *obj_fn_t)(void *obj);
typedef int(__attribute__((thiscall)) *check_t)(void *unit, void *order);
typedef void(__attribute__((thiscall)) *unit_fn_t)(void *unit);
typedef char *(__attribute__((thiscall)) *order_fn_t)(void *unit);
typedef int(__attribute__((thiscall)) *range_t)(void *spell, int a, int me, int id, int player);

static int g_cast_follow;
static int g_escort_follow;

void follow_load_config(const char *ini_path)
{
    g_cast_follow = GetPrivateProfileIntA("gameplay", "cast_follow", 1, ini_path);
    g_escort_follow = GetPrivateProfileIntA("gameplay", "escort_follow", 1, ini_path);
}

static int vcall(void *obj, int off)
{
    return ((obj_fn_t)(*(void ***)obj)[off / 4])(obj);
}

/* vector_dist 0x0046CFF0 (it takes dx, dy in ecx, edx): the distance the game measures ranges with */
static int game_dist(int dx, int dy)
{
    unsigned a = dx < 0 ? -dx : dx, b = dy < 0 ? -dy : dy;
    if (a < b) {
        unsigned t = a;
        a = b;
        b = t;
    }
    if (!a)
        return 0;
    if (b >= 60000)
        return (int)((b + a * 2) >> 1);
    return (int)(b * b / (a * 2) + a);
}

static int is_move(int kind)
{
    /* the kinds Unit::repath drops in front of an action */
    return (kind >= 1 && kind <= 4) || kind == 0x12 || kind == 0x13 || kind == 0x15;
}

static char *object(UINT_PTR table, int id, int player)
{
    if (id < 0 || player < 0 || player >= 8)
        return 0;
    char **objects = *(char ***)(VA(table) + player * 0x1C);
    char *obj = objects ? objects[id] : 0;
    return obj && (*(unsigned char *)(obj + 8) & 1) ? obj : 0; /* bit 0: the id still holds it */
}

/* The unit walks for its action (a move in front of it). */
static int walking(void *unit)
{
    char *cur = ((order_fn_t)VA(VA_UPDATE_ORDER))(unit);
    return cur && is_move(vcall(cur, OVT_KIND));
}

/* Distance from the end of the walk to the object. */
static int walk_end_dist(char *me, char *obj)
{
    return game_dist(*(int *)(me + U_ACTION_X) - (*(int *)(obj + 0x10) ^ COORD_KEY),
                     *(int *)(me + U_ACTION_Y) - (*(int *)(obj + 0x14) ^ COORD_KEY));
}

/* 1 = drop the walk: the target is already in range, or the walk ends out of it. */
static int cast_stale(char *me, char *base, int id, int player)
{
    int craft = *(int *)(((order_fn_t)(*(void ***)base)[OVT_CAST_DATA / 4])(base) + 0x20);
    if (craft < 0x275 || craft >= 0x2AC)
        return 0;
    char **spells = *(char ***)(*(char **)VA(VA_SPELLTYPES) + 0x10);
    char *spell = spells ? spells[craft] : 0;
    if (!spell || !(*(int *)(spell + SPELL_FLAGS) & 0xE) || (*(int *)(spell + SPELL_FLAGS) & 8))
        return 0;
    char *obj = object(VA_OBJECTS, id, player);
    if (!obj || vcall(obj, VT_IS_BUILDING) || !vcall(obj, VT_IS_ON_MAP))
        return 0;
    int range = ((range_t)VA(VA_GET_RANGE))(spell, 0, *(unsigned char *)(me + 9), id, player);
    if (!range)
        return 0;
    range += *(int *)(*(char **)(obj + 0x18) + T_RADIUS);
    int dx = (*(int *)(me + 0x10) ^ COORD_KEY) - (*(int *)(obj + 0x10) ^ COORD_KEY);
    int dy = (*(int *)(me + 0x14) ^ COORD_KEY) - (*(int *)(obj + 0x14) ^ COORD_KEY);
    return game_dist(dx, dy) <= range || walk_end_dist(me, obj) > range;
}

/* 1 = the walk ends more than 2 tiles beyond the distance do_follow keeps from the target. */
static int follow_stale(char *me, int id, int player)
{
    char *obj = object(VA_OBJECTS, id, player), *unit = object(VA_UNITS, id, player);
    if (!obj || !unit || vcall(obj, VT_IS_BUILDING) || !vcall(obj, VT_IS_ON_MAP))
        return 0;
    obj_fn_t speed = (obj_fn_t)VA(VA_SPEED);
    int size = vcall(me, VT_SIZE);
    int margin = speed(me) > speed(unit) ? size * 96 : size * 768 / 5;
    if (vcall(unit, VT_D8))
        margin *= 2;
    int keep = size * 384 - margin;
    if (keep < 0x180)
        keep = 0x180;
    if (keep > 0x600)
        keep = 0x600;
    return walk_end_dist(me, obj) > keep + 2 * TILE;
}

__attribute__((used, thiscall)) int c_check_target(void *unit, void *order)
{
    int r = ((check_t)VA(VA_CHECK_TARGET))(unit, order);
    if (r || !order)
        return r;
    char *to = order;
    int id = *(int *)(to + 8), player = *(int *)(to + 0xC);
    char *base = to + 4 + (*(int **)(to + 4))[1]; /* the order behind the TargetOrder, as the game finds it */
    int kind = vcall(base, OVT_KIND);
    if (kind == ORDER_CAST ? !g_cast_follow : kind != ORDER_FOLLOW || !g_escort_follow)
        return 0;
    if (!walking(unit))
        return 0;
    if (kind == ORDER_CAST ? !cast_stale(unit, base, id, player) : !follow_stale(unit, id, player))
        return 0;
    ((unit_fn_t)VA(VA_REPATH))(unit);
    return 1;
}

/* Unit::work's guard branch, inside its 16-frame check: 1 = orders changed. */
__attribute__((used, thiscall)) int c_guard_tick(void *unit, void *order)
{
    char *me = unit;
    int t = *(int *)(*(char **)VA(VA_GAME) + 0x550) + *(short *)(me + 0xA);
    if (t % 64 == 0) {
        ((unit_fn_t)VA(VA_REPATH))(unit); /* the game's own */
        return 1;
    }
    if (!order || !walking(unit))
        return 0;
    char *data = ((order_fn_t)(*(void ***)order)[OVT_GUARD_DATA / 4])(order);
    char *obj = object(VA_OBJECTS, *(int *)(data + 8), *(int *)(data + 0xC));
    if (!obj || vcall(obj, VT_IS_BUILDING) || !vcall(obj, VT_IS_ON_MAP))
        return 0;
    int place = game_dist(*(int *)(data + 0x14), *(int *)(data + 0x18));
    if (walk_end_dist(me, obj) <= place + 4 * TILE)
        return 0;
    ((unit_fn_t)VA(VA_REPATH))(unit);
    return 1;
}

static int patch_call(UINT_PTR site, UINT_PTR callee, void *to)
{
    unsigned char *at = (unsigned char *)VA(site);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(callee))
        return 0;
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    return 1;
}

/* mov eax, [Game]; movsx ecx, [ebx+0xA]; ... and eax, 0x8000003F ...; jne skip; mov ecx, ebx;
   call repath; jmp again. Bytes 1-4 (the Game pointer) and the call are checked apart. */
static const unsigned char guard_orig[0x28] = {
    0xA1, 0, 0, 0, 0, 0x0F, 0xBF, 0x4B, 0x0A, 0x8B, 0x80, 0x50, 0x05, 0x00, 0x00, 0x03, 0xC1,
    0x25, 0x3F, 0x00, 0x00, 0x80, 0x79, 0x05, 0x48, 0x83, 0xC8, 0xC0, 0x40, 0x75, 0x7A, 0x8B,
    0xCB, 0xE8, 0, 0, 0, 0, 0xEB, 0x54,
};

static int patch_guard(void)
{
    unsigned char *at = (unsigned char *)VA(VA_GUARD_SITE);
    if (memcmp(at + 5, guard_orig + 5, 0x21 - 5) != 0 || at[0] != 0xA1 ||
        *(UINT_PTR *)(at + 1) != VA(VA_GAME) || memcmp(at + 0x26, guard_orig + 0x26, 2) != 0 ||
        at[0x21] != 0xE8 || (UINT_PTR)(at + 0x26 + *(INT32 *)(at + 0x22)) != VA(VA_REPATH))
        return 0;
    unsigned char code[0x28];
    memset(code, 0x90, sizeof code);
    code[0] = 0x8B; code[1] = 0xCB;               /* mov ecx, ebx (the unit) */
    code[2] = 0x56;                               /* push esi (the action order) */
    code[3] = 0xE8;                               /* call c_guard_tick */
    *(INT32 *)(code + 4) = (INT32)((UINT_PTR)c_guard_tick - (UINT_PTR)(at + 8));
    code[8] = 0x85; code[9] = 0xC0;               /* test eax, eax */
    code[10] = 0x0F; code[11] = 0x84;             /* je skip */
    *(INT32 *)(code + 12) = (INT32)(VA(VA_GUARD_SKIP) - (UINT_PTR)(at + 16));
    code[16] = 0xE9;                              /* jmp again */
    *(INT32 *)(code + 17) = (INT32)(VA(VA_GUARD_AGAIN) - (UINT_PTR)(at + 21));
    DWORD old;
    VirtualProtect(at, sizeof code, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, code, sizeof code);
    VirtualProtect(at, sizeof code, old, &old);
    return 1;
}

/* Called from install_hooks once the exe is known to be the supported version. */
void follow_install(void)
{
    if (g_cast_follow || g_escort_follow) {
        if (patch_call(VA_TARGET_SITE, VA_CHECK_TARGET, (void *)c_check_target))
            logf_("follow: casts %s, follow %s", g_cast_follow ? "on" : "off", g_escort_follow ? "on" : "off");
        else {
            logf_("follow: call site mismatch, casts and follow keep the game's chase");
            g_cast_follow = 0;
        }
    }
    if (g_escort_follow) {
        if (patch_guard())
            logf_("follow: escorts keep up with the unit they guard");
        else
            logf_("follow: guard branch mismatch, escorts keep the game's 64-frame repath");
    }
}
