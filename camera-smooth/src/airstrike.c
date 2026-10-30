/*
 * Airbase strike. Two parts:
 *
 *  - A strike button on the Airbase. The game gives an Airbase only Scramble, Repeat and Recall;
 *    its strike is a right-click. BuildOut::make_options (airdrop.c's hook) now also adds the
 *    aircraft's own Air Mission option (0x24) to a finished Airbase of the local player, in the
 *    first free cell of the panel. The game does the rest: Options::do_airstrike starts the
 *    airstrike cursor, the click goes through Options::picked_spot to Console::execute_at_cursor
 *    (a strike, GroupOut::issue_flight) or GroupOut::issue_launch_patrol (a patrol), the same
 *    commands as the right-click. Its icon is Attack's (ui-theme's options.xml).
 *
 *  - Aircraft take off together. Object::do_launch lets a base launch one aircraft only every
 *    FRAMES_BETWEEN_LAUNCHES frames (15, a constant in .data, also read by
 *    Object::attempt_launch), so a strike left the base one plane at a time and arrived strung
 *    out. The value is set from air_launch_frames. Game rule: every player of a multiplayer
 *    game needs the same value.
 *
 *  - Area strike: with only Airbases selected, a right-click and drag draws a circle on the
 *    ground (centre where the button went down, edge under the cursor) and marks the enemy
 *    targets in it; letting go sends one aircraft to each. Bombers take the buildings in the
 *    circle (the units only when it holds no building), fighters the units (the buildings only
 *    when it holds no unit). A plain right-click is the game's as before; an empty circle sends
 *    every aircraft to patrol its centre. The game's right-drag (a move with a facing) is taken
 *    over in Console::on_right_up only in that case. Each aircraft gets its target through the
 *    game's own command, GroupOut::issue_flight with the Airbases' group, one plane (all = 0)
 *    and the Ctrl / Alt filter (bombers / fighters only): Group::action_launch_flight picks the
 *    nearest idle, refuelled plane of that kind and passes over planes that already have an
 *    order, so the commands go to different planes. The commands are queued and sent a few
 *    per frame, as the command package holds 0x200 bytes. Only commands, so multiplayer-safe.
 *
 * Engine notes: ui-theme\ENGINE_NOTES.md (options.xml) and camera-smooth\ENGINE_NOTES.md.
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_LAUNCH_FRAMES 0x00C06248u /* int FRAMES_BETWEEN_LAUNCHES (.data, 15) */
#define VA_OPTIONS       0x00C06204u /* Options *: +0x10 entries, +0x20 count */
#define VA_OPTIONS_ADD   0x0072C290u /* Options::add(code, x, y, type, disabled, 0) */
#define VA_ACTIVE_BUILD  0x0046FB40u /* ObjectData::is_active_build() */
#define VA_CONSOLE       0x00C06210u /* Console *: +0x298 = local player */
#define OPT_ENTRIES      0x10
#define OPT_COUNT        0x20
#define OPT_SIZE         0x14 /* code, type, count, disabled, column (byte +0x10), row (byte +0x11) */
#define OPT_AIR_MISSION  0x24
#define LAUNCH_FRAMES    15
#define AIRBASE          0x1BF

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn6_t)(void *self, int a, int b, int c, int d, int e, int f);

static int g_button = 1, g_launch_frames = 1, g_area = 1;

void airstrike_load_config(const char *ini_path)
{
    g_button = GetPrivateProfileIntA("qol", "airbase_strike", 1, ini_path);
    g_area = GetPrivateProfileIntA("qol", "airbase_area_strike", 1, ini_path);
    g_launch_frames = GetPrivateProfileIntA("gameplay", "air_launch_frames", 1, ini_path);
    if (g_launch_frames < 0 || g_launch_frames > 127) /* do_launch keeps the count in a signed byte */
        g_launch_frames = LAUNCH_FRAMES;
}

int airstrike_wanted(void) { return g_button; }

static int taken(char *opts, int col, int row)
{
    char *e = *(char **)(opts + OPT_ENTRIES);
    int n = *(int *)(opts + OPT_COUNT);
    for (int i = 0; i < n; i++, e += OPT_SIZE)
        if (e[0x10] == col && e[0x11] == row)
            return 1;
    return 0;
}

void airstrike_make_options(char *self)
{
    if (!g_button || (unsigned char)self[9] != *(int *)(*(char **)VA(VA_CONSOLE) + 0x298))
        return;
    if (*(int *)(*(char **)(self + 0x18) + 4) != AIRBASE || !((fn0_t)VA(VA_ACTIVE_BUILD))(self))
        return;
    char *opts = *(char **)VA(VA_OPTIONS);
    /* the bottom row has Scramble, Repeat, Recall, (Rally Point) and Delete; row 0 the aircraft
       and the Airdrop */
    static const signed char cells[][2] = { { 3, 3 }, { 3, 2 }, { 4, 2 }, { 3, 1 }, { 4, 1 }, { 4, 0 } };
    for (int i = 0; i < (int)(sizeof cells / sizeof cells[0]); i++)
        if (!taken(opts, cells[i][0], cells[i][1])) {
            ((fn6_t)VA(VA_OPTIONS_ADD))(opts, OPT_AIR_MISSION, cells[i][0], cells[i][1], -1, 0, 0);
            static int logged;
            if (!logged++)
                logf_("airstrike: strike button on the Airbase at (%d, %d)", cells[i][0], cells[i][1]);
            return;
        }
}

/* ---- area strike ------------------------------------------------------------------------ */

#define VA_RIGHT_UP_AT  0x007CD595u /* Console::on_right_up: cmp dword [edi+0x2f0], 0 / je (9 bytes) */
#define VA_RIGHT_CLICK  0x007CD59Eu /* the plain click: mov ecx, edi / call Console::right_click */
#define VA_RIGHT_DRAG   0x007CD5AAu /* the game's drag: a move with a facing */
#define VA_RIGHT_DONE   0x007CD8C8u /* clears Console +0x2DC, +0x2F0 and returns */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each; Console +0x2A0 = the local one */
#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object **, +0x15C / +0x184 counts */
#define VA_LEADERS      0x00C061E0u /* LeaderData[8], 0x6EEC each */
#define VA_IS_ENEMY     0x006EBAA0u /* LeaderData::is_enemy(player) const, ret 4 */
#define VA_IS_BUSY      0x0060A370u /* UnitData::is_busy() const */
#define VA_GROUP_CTOR   0x007140C0u /* Group::Group(int most_derived), ret 4 */
#define VA_GROUP_ADD    0x00714350u /* Group::add(id, player, 0, 0), ret 0x10 */
#define VA_ISSUE_FLIGHT 0x00708B10u /* GroupOut::issue_flight(id, player, order, all, bombers, fighters), ret 0x18 */
#define VA_ISSUE_PATROL 0x0070ADC0u /* GroupOut::issue_launch_patrol(x, y, QueuePos, all, bombers, fighters) */
#define VA_PING_TARGET  0x007CB870u /* Console::ping_target(id, player, Color), ret 0x14 */
#define VA_PING_DEST    0x007CB3D0u /* Console::ping_dest(x, y, 0, 0), ret 0x10 */
#define VA_PLAY_SOUND   0x0070D060u /* GroupData::play_sound(SoundTypeCat, int) const, ret 8 */
#define VA_SOUND_GLOBAL 0x0097F770u /* SoundGlobal::play(cat) const, ret 4 (ecx unused) */
#define VA_PACKAGE      0x00E8FF88u /* CommandPackage: word +0x10 = bytes used of 0x200 */
#define VA_TERRAIN      0x00C06218u
#define VA_FIND_Z       0x00866560u /* TerrainOut::find_data_z(Coord *out, x, y, 1), ret 0x10 */
#define VA_DRAW_CIRCLE  0x008C6040u /* draw_circle, through rally.c's rally_circle */
#define VA_RED          0x00C8D248u /* Color RED: 12 bytes, first dword 0xAABBGGRR */
#define VA_GREEN        0x00C8D1D0u

#define KEY          0x63637 /* x, y at +0x10, +0x14 xor this */
#define TILE         192
#define FIRST_BUILD  2000
#define ORDER_ATTACK 0xA
#define BOMBER       0x130 /* the Bomber line (Bomber, Strategic, Stealth ...) for ObjectData::is */
#define FIGHTER      0x11F /* the Biplane line (Fighter, Jet Fighter ...) */
#define MAX_BASES    32
#define MAX_TARGETS  256
#define MAX_QUEUE    128
#define PKG_ROOM     (0x200 - 0x60) /* send while the package holds no more than this */
#define MAX_PINGS    16

typedef struct { unsigned v[3]; } color_t;
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);
typedef void(__attribute__((thiscall)) *ping_t)(void *self, int id, int player, color_t c);

void rally_circle(int x, int y, int z, int radius, const void *color); /* rally.c */
extern UINT_PTR g_draw_circle;

UINT_PTR g_as_click, g_as_drag, g_as_done;
static int g_area_on;
static char g_group[0xC00] __attribute__((aligned(16))); /* a Group of the Airbases (0x9F0 and its vbase) */
static struct { short id; signed char p, filter; } g_queue[MAX_QUEUE];
static int g_qn, g_qi, g_pings;
static DWORD g_qtime;

typedef struct { int id, p, x, y; long long d; int bomb, fight, building; } target_t;
static target_t g_t[MAX_TARGETS];
static int g_tn;

static char *console(void) { return *(char **)VA(VA_CONSOLE); }
static int me(void) { return *(int *)(console() + 0x298); }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[8] & 1) ? o : NULL;
}

static int is_type(char *o, int line) /* ObjectData::is(type, 0), virtual +0xB8 */
{
    return ((fn2_t)(*(void ***)o)[0xB8 / 4])(o, line, 0);
}

static char *local_selection(void)
{
    char *sel = *(char **)VA(VA_SELECTIONS);
    return sel ? sel + *(int *)(console() + 0x2A0) * 0x9F0 : NULL;
}

/* The ids of the selected Airbases, or 0 when the selection holds anything else. */
static int selected_bases(int *ids)
{
    char *sel = local_selection();
    if (!sel || (unsigned char)sel[0x4A] != me())
        return 0;
    int n = *(int *)(sel + 0xC), k = 0;
    for (int i = 0; i < n && k < MAX_BASES; i++) {
        int id = *(short *)(sel + 0x8CC + 2 * i);
        char *o = object(me(), id);
        if (!o || id < FIRST_BUILD || *(int *)(*(char **)(o + 0x18) + 4) != AIRBASE ||
            !((fn0_t)VA(VA_ACTIVE_BUILD))(o))
            return 0;
        ids[k++] = id;
    }
    return n == k ? k : 0;
}

/* The aircraft waiting in the Airbases (idle and refuelled, as Group::action_launch_flight wants
   them for a one-plane mission): how many bombers, fighters and others, and one of each kind
   (ready or not) to ask ObjectData::valid_target_const what it may attack. */
static void planes(const int *bases, int nb, int *nbomb, int *nfight, int *nother, char **rb, char **rf)
{
    *nbomb = *nfight = *nother = 0;
    *rb = *rf = NULL;
    for (int b = 0; b < nb; b++) {
        char *o = object(me(), bases[b]);
        int id = o ? *(short *)(o + 0x28) : -1, p = o ? (signed char)o[0x3E] : -1;
        for (int guard = 0; id >= 0 && guard < 200; guard++) {
            char *u = object(p, id);
            if (!u)
                break;
            char *type = *(char **)(u + 0x18);
            if (type && *(int *)(type + 0x218) == 2) { /* the air domain */
                int ready = !((fn0_t)VA(VA_IS_BUSY))(u) && *(short *)(u + 0x96) == 0;
                if (is_type(u, BOMBER)) {
                    *nbomb += ready;
                    if (!*rb)
                        *rb = u;
                } else if (is_type(u, FIGHTER)) {
                    *nfight += ready;
                    if (!*rf)
                        *rf = u;
                } else {
                    *nother += ready;
                    if (!*rf)
                        *rf = u;
                }
            }
            id = *(short *)(u + 0x28);
            p = (signed char)u[0x3E];
        }
    }
}

static int valid_for(char *plane, int id, int p) /* ObjectData::valid_target_const, virtual +0x138 */
{
    return plane && ((fn2_t)(*(void ***)plane)[0x138 / 4])(plane, id, p);
}

static void add_target(char *o, int id, int p, int cx, int cy, long long r2, char *rb, char *rf)
{
    if (g_tn >= MAX_TARGETS)
        return;
    int building = id >= FIRST_BUILD;
    int x = *(int *)(o + 0x10) ^ KEY, y = *(int *)(o + 0x14) ^ KEY;
    if (building) { /* the corner: measure from the middle */
        char *t = *(char **)(o + 0x18);
        x += *(int *)(t + 0x234) * TILE / 2;
        y += *(int *)(t + 0x238) * TILE / 2;
    }
    long long dx = x - cx, dy = y - cy, d = dx * dx + dy * dy;
    if (d > r2)
        return;
    int bomb = valid_for(rb, id, p), fight = valid_for(rf, id, p);
    if (!bomb && !fight)
        return;
    target_t *t = &g_t[g_tn++];
    t->id = id, t->p = p, t->x = x, t->y = y, t->d = d, t->bomb = bomb, t->fight = fight, t->building = building;
}

/* Every enemy target in the circle that one of the kinds may attack (valid_target_const also
   asks that the local player sees it), nearest to the centre first. */
static void find_targets(int cx, int cy, int r, char *rb, char *rf)
{
    g_tn = 0;
    long long r2 = (long long)r * r;
    char *leader = *(char **)VA(VA_LEADERS) + me() * 0x6EEC;
    char *objs = *(char **)VA(VA_OBJECTS);
    for (int p = 0; p < 8; p++) {
        if (p == me() || !((fn1_t)VA(VA_IS_ENEMY))(leader, p))
            continue;
        int units = *(int *)(objs + 0x15C + 4 * p), builds = *(int *)(objs + 0x184 + 4 * p);
        if (units > FIRST_BUILD)
            units = FIRST_BUILD;
        for (int id = 0; id < units; id++) {
            char *u = object(p, id);
            if (u && *(char **)(u + 0x18) && *(short *)(u + 0x8E) < 0) /* captains: one per squad */
                add_target(u, id, p, cx, cy, r2, rb, rf);
        }
        for (int id = FIRST_BUILD; id < builds; id++) {
            char *b = object(p, id);
            if (b && *(char **)(b + 0x18))
                add_target(b, id, p, cx, cy, r2, rb, rf);
        }
    }
    for (int i = 1; i < g_tn; i++) /* insertion sort, a few dozen at most */
        for (int j = i; j > 0 && g_t[j].d < g_t[j - 1].d; j--) {
            target_t s = g_t[j];
            g_t[j] = g_t[j - 1];
            g_t[j - 1] = s;
        }
}

/* The targets of one kind: bombers want buildings, fighters units; the other sort only when
   the circle holds none of the wanted one. Returns how many, indices into g_t in list. */
static int pick(int bombers, int *list)
{
    int n = 0;
    for (int pass = 0; pass < 2 && !n; pass++)
        for (int i = 0; i < g_tn; i++) {
            int ok = bombers ? g_t[i].bomb : g_t[i].fight;
            int wanted = bombers ? g_t[i].building : !g_t[i].building;
            if (ok && (pass ? !wanted : wanted))
                list[n++] = i;
        }
    return n;
}

typedef struct { int bases[MAX_BASES], nb, cx, cy, r, nbomb, nfight, nother, bl[MAX_TARGETS], fl[MAX_TARGETS], nbl, nfl; } plan_t;

/* The circle being dragged now, or 0. */
static int make_plan(plan_t *pl)
{
    char *c = console();
    if (!g_area_on || !c || *(int *)(c + 0x2DC) != 2 || *(int *)(c + 0x2F0) != 0)
        return 0;
    pl->nb = selected_bases(pl->bases);
    if (!pl->nb)
        return 0;
    pl->cx = *(int *)(c + 0x308), pl->cy = *(int *)(c + 0x30C);
    double dx = *(int *)(c + 0x310) - pl->cx, dy = *(int *)(c + 0x314) - pl->cy;
    pl->r = (int)sqrt(dx * dx + dy * dy);
    char *rb, *rf;
    planes(pl->bases, pl->nb, &pl->nbomb, &pl->nfight, &pl->nother, &rb, &rf);
    find_targets(pl->cx, pl->cy, pl->r, rb, rf);
    pl->nbl = pick(1, pl->bl);
    pl->nfl = pick(0, pl->fl);
    return 1;
}

static int ground_z(int x, int y)
{
    void *terrain = *(void **)VA(VA_TERRAIN);
    int z = 0;
    if (terrain)
        ((fn4_t)VA(VA_FIND_Z))(terrain, (int)&z, x, y, 1);
    return z;
}

static void ring(int x, int y, int r, unsigned alpha)
{
    color_t c;
    memcpy(&c, (void *)VA(VA_RED), sizeof c);
    c.v[0] = (c.v[0] & 0xFFFFFF) | alpha << 24;
    rally_circle(x, y, ground_z(x, y), r, &c);
}

static void issue_next(void);

/* From nukefx.c's hook of Nuke::draw_underlays, once a frame in the map's ground pass: the
   circle and its targets while dragging, and the queued commands. */
void airstrike_draw(void)
{
    if (!g_area_on || !console())
        return;
    issue_next();
    static plan_t pl;
    if (!make_plan(&pl))
        return;
    ring(pl.cx, pl.cy, pl.r, 0xE0);
    if (pl.r > 24)
        ring(pl.cx, pl.cy, pl.r - 12, 0x80);
    /* the targets that will be hit: one ring each, under the aircraft's targets only */
    int n = pl.nbomb < pl.nbl ? pl.nbomb : pl.nbl;
    for (int i = 0; i < n; i++)
        ring(g_t[pl.bl[i]].x, g_t[pl.bl[i]].y, g_t[pl.bl[i]].building ? TILE : TILE / 3, 0xE0);
    n = pl.nfight + pl.nother < pl.nfl ? pl.nfight + pl.nother : pl.nfl;
    for (int i = 0; i < n; i++)
        ring(g_t[pl.fl[i]].x, g_t[pl.fl[i]].y, g_t[pl.fl[i]].building ? TILE : TILE / 3, 0xE0);
}

static void ping(int id, int p)
{
    if (g_pings++ >= MAX_PINGS)
        return;
    color_t c;
    memcpy(&c, (void *)VA(VA_GREEN), sizeof c);
    ((ping_t)VA(VA_PING_TARGET))(console(), id, p, c);
}

/* Sends queued commands while the command package has room; drops what is left after 10 s. */
static void issue_next(void)
{
    if (g_qi >= g_qn)
        return;
    if (GetTickCount() - g_qtime > 10000) {
        logf_("airstrike: %d area strike orders not sent (the command package stayed full)", g_qn - g_qi);
        g_qi = g_qn = 0;
        return;
    }
    while (g_qi < g_qn && *(short *)((char *)VA(VA_PACKAGE) + 0x10) <= PKG_ROOM) {
        int f = g_queue[g_qi].filter;
        typedef void(__attribute__((thiscall)) * flight_t)(void *, int, int, int, int, int, int);
        ((flight_t)VA(VA_ISSUE_FLIGHT))(g_group, g_queue[g_qi].id, g_queue[g_qi].p, ORDER_ATTACK, 0, f == 1, f == 2);
        ping(g_queue[g_qi].id, g_queue[g_qi].p);
        g_qi++;
    }
}

static void queue(int id, int p, int filter)
{
    if (g_qn < MAX_QUEUE) {
        g_queue[g_qn].id = (short)id;
        g_queue[g_qn].p = (signed char)p;
        g_queue[g_qn].filter = (signed char)filter;
        g_qn++;
    }
}

/* Console::on_right_up, before its click / drag test: 1 = the drag was an area strike (the
   game's own drag is skipped). */
__attribute__((used)) int c_as_right_up(char *c)
{
    static plan_t pl;
    if (c != console() || !make_plan(&pl))
        return 0;
    g_qi = g_qn = g_pings = 0;
    g_qtime = GetTickCount();
    ((fn1_t)VA(VA_GROUP_CTOR))(g_group, 1);
    for (int i = 0; i < pl.nb; i++)
        ((fn4_t)VA(VA_GROUP_ADD))(g_group, pl.bases[i], me(), 0, 0);
    /* one command per ready plane, round the kind's targets */
    for (int i = 0; pl.nbl && i < pl.nbomb; i++)
        queue(g_t[pl.bl[i % pl.nbl]].id, g_t[pl.bl[i % pl.nbl]].p, 1);
    for (int i = 0; pl.nfl && i < pl.nfight; i++)
        queue(g_t[pl.fl[i % pl.nfl]].id, g_t[pl.fl[i % pl.nfl]].p, 2);
    for (int i = 0; pl.nfl && i < pl.nother; i++) /* neither line (Fighter Bombers ...): no filter */
        queue(g_t[pl.fl[i % pl.nfl]].id, g_t[pl.fl[i % pl.nfl]].p, 0);
    logf_("airstrike: area strike r %d tiles, %d targets (%d for bombers, %d for fighters), planes %d / %d / %d, "
          "%d orders",
          pl.r / TILE, g_tn, pl.nbl, pl.nfl, pl.nbomb, pl.nfight, pl.nother, g_qn);
    ((fn1_t)VA(VA_SOUND_GLOBAL))(NULL, 0x13D); /* what the game plays for an Airbase's strike */
    ((fn2_t)VA(VA_PLAY_SOUND))(g_group, 0xB, 0);
    if (!g_qn) { /* nothing to hit (or no plane ready): every plane patrols the centre */
        typedef void(__attribute__((thiscall)) * patrol_t)(void *, int, int, int, int, int, int);
        ((patrol_t)VA(VA_ISSUE_PATROL))(g_group, pl.cx, pl.cy, 2, 1, 0, 0);
        ((fn4_t)VA(VA_PING_DEST))(c, pl.cx, pl.cy, 0, 0);
        return 1;
    }
    issue_next();
    return 1;
}

__asm__(".intel_syntax noprefix\n"
        /* in place of cmp dword [edi+0x2f0], 0 / je drag; edi = the Console */
        "_hk_as_right_up:\n"
        "    push edi\n"
        "    call _c_as_right_up\n"
        "    add esp, 4\n"
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    cmp dword ptr [edi+0x2f0], 0\n"
        "    je 2f\n"
        "    jmp dword ptr [_g_as_click]\n"
        "2:  jmp dword ptr [_g_as_drag]\n"
        "1:  jmp dword ptr [_g_as_done]\n"
        ".att_syntax prefix\n");
void hk_as_right_up(void);

int airstrike_area_wanted(void) { return g_area; }

/* Called by nukefx_install with whether its Nuke::draw_underlays hook (the circle, the queue)
   is in place. */
void airstrike_area_install(int drawing)
{
    if (!g_area)
        return;
    static const unsigned char sig[9] = { 0x83, 0xBF, 0xF0, 0x02, 0x00, 0x00, 0x00, 0x74, 0x0C };
    unsigned char *at = (unsigned char *)VA(VA_RIGHT_UP_AT);
    if (!drawing || memcmp(at, sig, sizeof sig) != 0) {
        logf_("airstrike: %s, no area strike", drawing ? "signature mismatch in Console::on_right_up"
                                                         : "no Nuke::draw_underlays hook");
        return;
    }
    g_as_click = VA(VA_RIGHT_CLICK);
    g_as_drag = VA(VA_RIGHT_DRAG);
    g_as_done = VA(VA_RIGHT_DONE);
    if (!g_draw_circle)
        g_draw_circle = VA(VA_DRAW_CIRCLE);
    unsigned char code[9] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90 };
    *(INT32 *)(code + 1) = (INT32)((UINT_PTR)hk_as_right_up - (UINT_PTR)(at + 5));
    DWORD old;
    VirtualProtect(at, sizeof code, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, code, sizeof code);
    VirtualProtect(at, sizeof code, old, &old);
    g_area_on = 1;
    logf_("airstrike: area strike on a right-drag from the Airbases");
}

void airstrike_install(void)
{
    int *frames = (int *)VA(VA_LAUNCH_FRAMES);
    if (g_launch_frames == LAUNCH_FRAMES)
        return;
    if (*frames != LAUNCH_FRAMES) {
        logf_("airstrike: FRAMES_BETWEEN_LAUNCHES is %d, not %d; left alone", *frames, LAUNCH_FRAMES);
        return;
    }
    *frames = g_launch_frames;
    logf_("airstrike: aircraft take off every %d frames (the game: %d)", g_launch_frames, LAUNCH_FRAMES);
}
