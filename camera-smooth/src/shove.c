/*
 * Units making way: a moving unit blocked by an idle unit of its own player asks it to step
 * aside, instead of walking around it or giving up.
 *
 * The game never moves the blocker. Unit::move_step finds the way blocked with
 * Unit::detect_unit_collision, which notes who blocks: word [mover + 0x8A] = its id, byte
 * [mover + 0xB3] = its player (-1 / 0xFF when free). Unit::resolve_unit_collision then decides:
 * an enemy is attacked; a blocker that is moving itself (move-type orders, see is_move) is waited
 * for ([mover + 0x68] |= 0x40, up to 32 frames each, 128 with many units); anything else (an
 * idle unit) is walked around with a short local path (CollCheck::collide_here,
 * PathFinder::find_upath), which fails in a crowd, a gap or a column of idle units.
 *
 * resolve_unit_collision is hooked at its entry. When the blocker is a land unit of the mover's
 * own player with no orders at all ([unit + 0xDC] == 0), it gets a move order to a free spot
 * beside it: UnitType::find_nearby_spot (as Animal::do_idle finds where to wander) from the
 * direction square to the mover's approach, then Unit::add_move_order, for every soldier of its
 * squad by the same step. Then the game's own code runs and, seeing a moving blocker, waits for
 * it. A unit that was just moved aside is left alone for [make_way] cooldown frames.
 *
 * Not moved: other players' units (allies too), decoys, units dug in (the General's Entrench,
 * Antipater, idle Marines) or in an entrenchment Nest, ships and aircraft, a soldier of the
 * mover's own squad or move group.
 *
 * Units passing through each other ([gameplay] pass_through): detect_unit_collision walks the
 * objects around the next step and already lets units of one move group that are both moving
 * walk through each other ([ebp-4] = 1, then the next object, 0x00617870). It is hooked where it
 * reaches that test (0x00617676, ecx = the unit found, ebx = the mover): a land unit of the
 * mover's own player or of an ally (LeaderData::is_ally, [make_way] pass_allies) is walked
 * through when it is moving itself, or, when it stands, as long as
 * the mover is more than [make_way] pass_clear tiles from the end of its path (the bottom of its
 * path stack, +0xB8 / count +0xC0, PathData +0 x, +4 y). Near the end a standing unit still
 * blocks, so arriving units do not stop on top of the ones already there; make_way moves it.
 *
 * It is part of the simulation (the frame clock, no random): every player of a multiplayer game
 * needs the same settings ([gameplay] make_way, pass_through).
 */
#include "ron.h"
#include <string.h>

#define VA_RESOLVE      0x005F9D30u /* Unit::resolve_unit_collision(Coord, Coord), thiscall, ret 8 */
#define VA_ADD_MOVE     0x00616ED0u /* Unit::add_move_order(Coord x, Coord y, int, int, QueuePos, int, int, Coord, Coord), ret 0x24 */
#define VA_NEARBY_SPOT  0x0061DE70u /* UnitType::find_nearby_spot(x, y, *ox, *oy, min, max, step, angle, FilterIndex, id, player, ...), ret 0x40; 0 = found */
#define VA_FIND_ANGLE   0x0092D130u /* find_angle(dx in ecx, dy in edx): binary angle (2^32 = full turn) */
#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object ** */
#define VA_GAME         0x00C061ECu /* Game *: +0x550 = the frame (15 a second) */
#define VA_PEER         0x00617676u /* detect_unit_collision: movzx eax, word [ebx+0x80] (the move group test) */
#define VA_PEER_NEXT    0x0061767Du /* ... cmp ax, [ecx+0x80] */
#define VA_PEER_PASS    0x00617870u /* [ebp-4] = 1, on to the next object: walked through */
#define VA_ACTION_TYPE  0x0060A850u /* UnitData::action_type(): the current order's OrderIndex, 0 without one */
#define VA_IS_ALLY      0x006EDB50u /* LeaderData::is_ally(int player) const, ret 4: allied both ways */
#define VA_LEADERS_PTR  0x00C061E0u /* LeaderData[8], 0x6EEC each */
#define LEADER_SIZE     0x6EEC

#define KEY          0x63637 /* x, y at +0x10, +0x14 xor this */
#define OBJ_FLAGS    0x08    /* bit 0: alive */
#define OBJ_OWNER    0x09
#define OBJ_ID       0x0A
#define OBJ_TYPE     0x18
#define U_FLAGS      0x68    /* bit 0 decoy, 0x2000000 dug in (General's Entrench) */
#define U_FLAGS2     0x6C    /* 0x1000 Antipater's entrench, 0x20000 idle Marines dug in */
#define U_GROUP      0x80    /* short: move group, -1 none */
#define U_BLOCKER    0x8A    /* short: the unit that blocks it (detect_unit_collision) */
#define U_CAPTAIN    0x8E    /* word: bit 15 = captain, else the captain's id */
#define U_NEXT       0x90    /* short: next soldier of the squad, -1 = last */
#define U_BLOCKER_P  0xB3    /* char: the blocker's player */
#define U_PATH       0xB8    /* path stack: PathData (16 bytes: +0 x, +4 y) *, the end at the bottom */
#define U_PATH_N     0xC0    /* waypoints in it */
#define U_ORDERS     0xDC    /* the order list, 0 = no orders */
#define T_DOMAIN     0x218   /* 0 land, 1 water, 2 air */
#define FIRST_BUILD  2000
#define QUEUE_NOW    2       /* QueuePos the game's own idle moves use */

typedef int(__attribute__((thiscall)) *resolve_t)(void *self, int x, int y);
typedef void(__attribute__((thiscall)) *add_move_t)(void *self, int x, int y, int a, int b, int queue, int c, int d,
                                                     int fx, int fy);
typedef int(__attribute__((thiscall)) *nearby_t)(void *type, int x, int y, int *ox, int *oy, int min, int max, int step,
                                                 unsigned angle, int filter, int id, int player, int a, int b, int c,
                                                 int d, int e);

typedef int(__attribute__((thiscall)) *action_type_t)(void *self);
typedef int(__attribute__((thiscall)) *is_ally_t)(void *leader, int player);

int g_make_way_enabled, g_pass_enabled;
static int g_cooldown = 45, g_min = 0x100, g_max = 0x240, g_pass_clear = 2 * 192, g_pass_allies = 1;
UINT_PTR g_peer_next, g_peer_pass;
static void *g_orig_resolve;
UINT_PTR g_va_find_angle;
static int g_moved[8][FIRST_BUILD]; /* frame + 1 of the last time the unit was moved aside */

/* find_angle's own convention: dx in ecx, dy in edx. */
unsigned sh_find_angle(int dx, int dy);
__asm__(".intel_syntax noprefix\n"
        ".globl _sh_find_angle\n"
        "_sh_find_angle:\n"
        "    mov ecx, [esp+4]\n"
        "    mov edx, [esp+8]\n"
        "    jmp dword ptr [_g_va_find_angle]\n"
        ".att_syntax prefix\n");

void make_way_load_config(const char *ini_path)
{
    g_make_way_enabled = GetPrivateProfileIntA("gameplay", "make_way", 1, ini_path);
    g_pass_enabled = GetPrivateProfileIntA("gameplay", "pass_through", 1, ini_path);
    g_cooldown = GetPrivateProfileIntA("make_way", "cooldown_frames", g_cooldown, ini_path);
    g_pass_clear = (int)(ini_float(ini_path, "make_way", "pass_clear", 2.0f) * 192);
    g_pass_allies = GetPrivateProfileIntA("make_way", "pass_allies", g_pass_allies, ini_path);
}

static int now(void) { return *(int *)(*(char **)VA(VA_GAME) + 0x550); }
static int owner(char *o) { return (unsigned char)o[OBJ_OWNER]; }
static int id_of(char *o) { return *(short *)(o + OBJ_ID); }
static int ox(char *o) { return *(int *)(o + 0x10) ^ KEY; }
static int oy(char *o) { return *(int *)(o + 0x14) ^ KEY; }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0 || id >= FIRST_BUILD)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

static char *captain_of(char *u)
{
    if (*(unsigned short *)(u + 0x8E) & 0x8000)
        return u;
    char *c = object(owner(u), *(short *)(u + U_CAPTAIN));
    return c ? c : u;
}

/* The squad from its captain on (a single unit is a squad of one); at most 32. */
static int squad(char *cap, char **out)
{
    int n = 0;
    for (char *m = cap; m && n < 32; n++) {
        out[n] = m;
        short next = *(short *)(m + U_NEXT);
        m = next >= 0 ? object(owner(cap), next) : NULL;
    }
    return n;
}

/* A soldier that must stay where it is. */
static int pinned(char *u)
{
    char *t = *(char **)(u + OBJ_TYPE);
    return !t || *(int *)(t + T_DOMAIN) != 0 || *(int *)(u + U_ORDERS) != 0 ||
           (*(unsigned *)(u + U_FLAGS) & 0x2000001) || (*(unsigned *)(u + U_FLAGS2) & 0x21000) || entrench_holds(u);
}

static void make_way(char *mover)
{
    int p = owner(mover), bid = *(short *)(mover + U_BLOCKER), bp = (signed char)mover[U_BLOCKER_P];
    if (p > 7 || bp != p || bid < 0 || bid == id_of(mover))
        return;
    char *b = object(bp, bid), *mt = *(char **)(mover + OBJ_TYPE);
    if (!b || !mt || *(int *)(mt + T_DOMAIN) != 0)
        return;
    char *cap = captain_of(b);
    if (cap == captain_of(mover))
        return; /* its own squad */
    short group = *(short *)(b + U_GROUP);
    if (group != -1 && group == *(short *)(mover + U_GROUP))
        return; /* moving together: the game sorts that out */
    int t = now(), cid = id_of(cap);
    if (cid < 0 || cid >= FIRST_BUILD || (g_moved[p][cid] && t - (g_moved[p][cid] - 1) < g_cooldown))
        return;
    char *soldiers[32];
    int n = squad(cap, soldiers);
    for (int i = 0; i < n; i++)
        if (pinned(soldiers[i]))
            return;

    /* square to the mover's approach, the side picked by the blocker's id */
    int bx = ox(cap), by = oy(cap);
    unsigned a = sh_find_angle(bx - ox(mover), by - oy(mover)) + ((cid & 1) ? 0x40000000u : 0xC0000000u);
    int sx = bx, sy = by;
    if (((nearby_t)VA(VA_NEARBY_SPOT))(*(char **)(cap + OBJ_TYPE), bx, by, &sx, &sy, g_min, g_max, 0, a, 3, cid, p, 0,
                                       0, -1, 0, -1) != 0)
        return; /* no free spot nearby */
    int dx = sx - bx, dy = sy - by;
    for (int i = 0; i < n; i++)
        ((add_move_t)VA(VA_ADD_MOVE))(soldiers[i], ox(soldiers[i]) + dx, oy(soldiers[i]) + dy, 1, 0, QUEUE_NOW, 0, 0,
                                      -1, -1);
    g_moved[p][cid] = t + 1;
    static int logged;
    if (logged < 20) {
        logged++;
        logf_("make way: player %d unit %d moves squad %d (%d) aside by (%d, %d)", p, id_of(mover), cid, n, dx, dy);
    }
}

/* ---- passing through ------------------------------------------------------------------- */

static int is_moving(char *u)
{
    if (!*(int *)(u + U_ORDERS))
        return 0;
    switch (((action_type_t)VA(VA_ACTION_TYPE))(u)) {
    case 0: case 1: case 2: case 3: case 4: case 0x12: case 0x13: case 0x15:
        return 1; /* Patrol, Move, AttackTo, ExploreTo, FleeTo, Form, GroupMove, GroupAttackTo */
    }
    return 0;
}

/* Farther than pass_clear from the end of its path (no path: on its last stretch). */
static int far_from_end(char *u)
{
    if (*(int *)(u + U_PATH_N) < 1)
        return 0;
    int *end = *(int **)(u + U_PATH);
    int dx = end[0] - ox(u), dy = end[1] - oy(u);
    if (dx < 0)
        dx = -dx;
    if (dy < 0)
        dy = -dy;
    return (dx > dy ? dx : dy) > g_pass_clear;
}

/* detect_unit_collision found `other` beside `mover`'s next step: 1 = walk through it. Enemies
   always block: resolve_unit_collision attacks them. */
__attribute__((used)) int c_pass_through(char *mover, char *other)
{
    int p = owner(mover), q = owner(other);
    if (p > 7 || q > 7)
        return 0;
    if (q != p) { /* an ally (player 0 is nature) */
        char *leaders = *(char **)VA(VA_LEADERS_PTR);
        if (!g_pass_allies || q <= 0 || !leaders || !((is_ally_t)VA(VA_IS_ALLY))(leaders + p * LEADER_SIZE, q))
            return 0;
    }
    char *mt = *(char **)(mover + OBJ_TYPE), *t = *(char **)(other + OBJ_TYPE);
    if (!mt || !t || *(int *)(mt + T_DOMAIN) != 0 || *(int *)(t + T_DOMAIN) != 0)
        return 0;
    return is_moving(other) || far_from_end(mover);
}

void hk_peer(void);
/* ecx = the unit found, ebx = the mover, both kept for the game's code */
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_peer\n"
        "_hk_peer:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+24]\n"
        "    push dword ptr [ebx+16]\n"
        "    call _c_pass_through\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz Lpeer_pass\n"
        "    movzx eax, word ptr [ebx+0x80]\n"
        "    jmp dword ptr [_g_peer_next]\n"
        "Lpeer_pass:\n"
        "    jmp dword ptr [_g_peer_pass]\n"
        ".att_syntax prefix\n");

/* ---- making way ------------------------------------------------------------------------ */

__attribute__((thiscall)) static int shove_resolve(char *mover, int x, int y)
{
    if (g_make_way_enabled)
        make_way(mover);
    return ((resolve_t)g_orig_resolve)(mover, x, y);
}

/* Called from install_hooks once the exe is known to be the supported version. */
static void pass_install(void)
{
    static const unsigned char sig[7] = { 0x0F, 0xB7, 0x83, 0x80, 0x00, 0x00, 0x00 };
    static const unsigned char at_sig[5] = { 0x8B, 0x0C, 0x02, 0x0F, 0xB7 }; /* mov ecx,[edx+eax] before it */
    unsigned char *at = (unsigned char *)VA(VA_PEER);
    if (memcmp(at, sig, sizeof sig) || memcmp(at - 3, at_sig, sizeof at_sig)) {
        logf_("pass through: signature mismatch, units do not pass through each other");
        g_pass_enabled = 0;
        return;
    }
    g_peer_next = VA(VA_PEER_NEXT);
    g_peer_pass = VA(VA_PEER_PASS);
    hook_jmp5(at, hk_peer);
    logf_("pass through: own%s land units walk through each other (standing ones up to %d units from the end)",
          g_pass_allies ? " and allied" : "", g_pass_clear);
}

void make_way_install(void)
{
    if (g_pass_enabled)
        pass_install();
    if (!g_make_way_enabled)
        return;
    /* push ebp / mov ebp,esp / push -1 (then the exception handler) */
    static const unsigned char sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    /* find_angle: push ebp / mov ebp,esp / push ecx / push edi; find_nearby_spot: push ebp / mov ebp,esp / sub esp,0x3c */
    static const unsigned char angle_sig[5] = { 0x55, 0x8B, 0xEC, 0x51, 0x57 };
    static const unsigned char spot_sig[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x3C };
    unsigned char *at = (unsigned char *)VA(VA_RESOLVE);
    if (memcmp(at, sig, sizeof sig) || memcmp((void *)VA(VA_FIND_ANGLE), angle_sig, sizeof angle_sig) ||
        memcmp((void *)VA(VA_NEARBY_SPOT), spot_sig, sizeof spot_sig)) {
        logf_("make way: signature mismatch, units do not make way");
        g_make_way_enabled = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    g_va_find_angle = VA(VA_FIND_ANGLE);
    g_orig_resolve = hook_trampoline(at, 5, mem);
    hook_jmp5(at, shove_resolve);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("make way: idle units step aside for their own moving units (cooldown %d frames)", g_cooldown);
}
