/*
 * Nuclear weapons (nuke-fx mod): missile defence, radiation, and a blast that is easier on the
 * eyes. Engine notes: camera-smooth\ENGINE_NOTES.md, "Nuclear weapons (nukefx.c)".
 *
 * Simulation (every machine, the same in multiplayer; every player needs the same [nuke] values):
 *  - Interceptors: the nuke-fx data makes unit 337 (type 0x183) the Interceptor, trained at the
 *    Missile Silo. Each frame (airdrop.c's Objects::process_all hook) a finished Interceptor is
 *    taken out of its silo (Object::disband) and counted for that silo, at most 2. A nuke in
 *    flight aimed at the silo's owner (its target object or the target point's territory) is
 *    intercepted; nukes flying past to someone else are not. A nuke in
 *    flight is an Ammo record following a spline to its target; an enemy silo with an
 *    Interceptor fires when the nuke is within interceptor_reach_tiles and the Interceptor (after
 *    a quarter second's lift-off, interceptor_speed_percent of the nuke's ground speed, 4 times
 *    by default) can still meet it before it lands. At the meeting frame the Ammo is closed
 *    (Ammo::close, what the game's Missile Shield does): no blast, no damage, no crater, no
 *    Armageddon count. A small fireball (interception_burst_size) where they meet, a faint light
 *    over the screen (interception_flash_percent of a nuke's flash, some of it even off the
 *    screen), and nuke-fx's sounds interceptorlaunch / interceptorhit (soundfiles.xml) for the
 *    two players concerned.
 *  - Radiation: every blast that goes off (the call of Nuke::add_nuke in Ammo::do_damage) leaves a
 *    zone of the blast's radius for radiation_seconds. Once a second every unit inside (any
 *    player; not aircraft, not decoys, not buildings) loses a share of its max hit points through
 *    take_damage with nobody credited, like the game's attrition: radiation_percent_per_second at
 *    first, falling to 0 at the end.
 * Drawing only:
 *  - The white flash: Nuke::draw (called by Scene::render at 0x008B3B0C) writes the tone mapper
 *    for every young blast wherever it is; here it fades with the blast's distance from the
 *    screen centre (flash_full_tiles .. flash_zero_tiles).
 *  - Over a nuke's target while it flies, the radiation sign (underlay type NukeTarget of the
 *    nuke-fx effects_graphics.xml), shrinking towards the centre and turning slowly; over a
 *    radiation zone a green haze (NukeHaze). Underlay instances are written once a frame.
 *  - The scorched earth stays scorch_percent as long: the fade span in NukeOut::draw_char_earth
 *    is patched, and blasts the game has already dropped from its list are drawn again from here.
 * Zones, interceptor counts and interceptions are not in saved games.
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_CALL_NUKE_DRAW 0x008B3B0Cu /* Scene::render: call Nuke::draw */
#define VA_NUKE_DRAW      0x0092B370u /* Nuke::draw() const */
#define VA_CALL_UNDERLAYS 0x008B3732u /* Scene::render: call Nuke::draw_underlays (the scorch) */
#define VA_NUKE_UNDERLAYS 0x0092B2F0u
#define VA_CHAR_EARTH     0x009277E0u /* NukeOut::draw_char_earth: x xmm1, y xmm2, t xmm3 */
#define VA_SCORCH_SPAN    0x009279A4u /* add eax, 0x140 in draw_char_earth (the fade span) */
#define VA_CALL_ADD_NUKE  0x00678516u /* Ammo::do_damage: call Nuke::add_nuke (its only caller) */
#define VA_ADD_NUKE       0x0092BA30u /* Nuke::add_nuke(x, y, r0, r1, player, id), ret 0x18 */
#define VA_AMMO_CLOSE     0x006791A0u /* Ammo::close() */
#define VA_DISBAND        0x006455C0u /* Object::disband(0) */
#define VA_IS_ENEMY       0x006EBAA0u /* LeaderData::is_enemy(player) */
#define VA_SOUND_PLAY     0x0097F770u /* SoundGlobal::play(cat), ret 4 (ecx unused) */
#define VA_OBJ_ARRAY_ADD  0x004A9D20u /* ObjectArray<UnderlayStruct>::add(const UnderlayStruct &) */
#define VA_FIND_Z         0x00866560u /* TerrainOut::find_data_z(Coord *out, x, y, 1), ret 0x10 */
#define VA_CURR_UBER      0x0060A760u /* UnitData::curr_uber_size(0): soldiers left in the squad */
#define VA_QUEUE_UP       0x00620F40u /* Build::queue_up(type, flag): 0 = queued, ret 8 */
#define VA_CAN_CARRY      0x00645E00u /* ObjectData::can_carry(type): the silo's one-missile rule, ret 4 */
#define VA_ACTIVE_BUILD   0x0046FB40u /* ObjectData::is_active_build(): finished, not a site */
#define VA_TYPE_AVAIL     0x006E33A0u /* LeaderData::type_avail(type, 1): 4 = can make it now, ret 8 */
#define VA_GET_NUKES      0x006EBE50u /* LeaderData::get_nukes(): nuclear missiles built or queued */
#define VA_CALL_SCEN_WALK 0x005A34A0u /* WalkDataGame::walk_data: call ScenarioData::walk_data (ecx = walker) */
#define VA_SCEN_WALK      0x00997AD0u
#define VA_TEMP_SAVE      0x00E88AFCu /* String ScenarioData::temp_save: saved and loaded, never used */
#define VA_STR_CTOR       0x00A1EE30u /* String::String() */
#define VA_STR_SET        0x00A1EEB0u /* String::operator=(const String &), ret 4 */
#define VA_STR_SET_W      0x00A1DB60u /* String::operator=(const wchar_t *), ret 4 */
#define VA_STR_STR        0x00A1F0A0u /* String::str(): the characters */
#define VA_IS_ALLY        0x006EDB50u /* LeaderData::is_ally(player), ret 4 */
#define VA_WORLD          0x00C06188u /* World *: [0], [1] = size in 4 x 4 tile cells, +0x134 the cells */
#define VA_DIV3           0x00CAE5FCu /* int *div_3_table */
#define VA_GET_INSIDE     0x00651A80u /* ObjectData::get_inside(int *player): the building it is in, -1, ret 4 */
#define VA_INIT_UNIT      0x0065E0C0u /* Objects::init_unit(player, type, x, y, -1, -1, -1): id */
#define VA_ADD_AMMO       0x00658B10u /* Objects::add_ammo(GameDataPackage &): slot, ret 4 */
#define VA_BUILD_NORMALS  0x00911F60u /* Spline::build_normals(): the directions the drawing uses */
#define VA_ADD_EXPLOSION  0x00904CC0u /* GraphicPieces::add_explosion: x, y, z xmm1..3; scale, debris, kind; ret 0xC */
#define VA_ADD_PULSEWAVE  0x00904DD0u /* GraphicPieces::add_pulsewave: x, y, z xmm1..3 */
#define VA_GPIECES        0x00C06214u /* GraphicPieces * */
#define VA_DEAD_HOLD      0x00C0A888u /* int: the frames Object::die keeps a missile's slot after its ammo lands */
#define VT_DIE            0x158       /* die(0, -1, 0.0f), ret 0xC */
#define TD_AMMO_PIECE     0x2CC       /* the unit type's ammo graphic */
#define MISSILE_Z         388         /* effects_graphics.xml MISSILEOFFSET z */
#define VA_GRAPHSYS       0x00E335A0u /* GraphSys *graphsys */
#define VA_CAMERA_PTR     0x00C06200u /* MiscAccess::camera */
#define VA_GAME_PTR       0x00C061ECu /* Game *: +0x550 game time, +0x854 fraction of the frame */
#define VA_OBJECTS        0x00C0618Cu /* Objects * */
#define VA_TERRAIN        0x00C06218u
#define VA_LEADERS        0x00E3A390u
#define VA_CONSOLE        0x00C06210u /* +0x298 local player, +0x2A0 local selection */
#define VA_SELECTION      0x00E8D444u
#define VA_OPTIONS        0x00C06204u
#define VA_NUKE_POS       0x00C0A7D0u /* float (*)[2]: blast x, y in world units */
#define VA_NUKE_COUNT     0x00C0A7FCu
#define VA_NUKE_START     0x00C0A808u /* int *: game time of each blast */
#define VA_NUKE_FRAMES    0x00C0A890u /* int: frames a blast's screen effect lasts (110) */
#define VA_NUKE_MODE      0x00C0A88Cu /* int: draw_underlays draws the scorch when 0, 7 or 9 */
#define VA_UL_TYPES_N     0x00C8C6FCu /* underlay types: count, data (0x14 each) */
#define VA_UL_TYPES       0x00C8C708u
#define VA_UL_INSTANCES   0x00C8C720u /* ObjectArray<UnderlayStruct> *[type] */

#define GAME_TIME   0x550
#define GAME_FRAC   0x854
#define CAM_LOC_X   0x304
#define CAM_LOC_Y   0x308
#define CAM_DIST    0x2F0
#define TONE_DIRTY  0x04
#define TONE_FLASH  0x0C
#define TONE_SAT    0x1C
#define TONE_ALPHA  0x2B
#define OBJS_AMMO_N 0x120
#define OBJS_AMMO   0x12C
#define OBJS_UNITS_N 0x15C
#define OBJS_BUILD_N 0x184
#define FIRST_BUILD 2000
#define LEADER_SIZE 0x6EEC
#define LD_RESERVED 0x468  /* + 4 r: what the AI sets aside */
#define LD_SECURE   0x6EB8 /* int *: + r = treasury ^ STOCK_KEY */
#define STOCK_KEY   0x8221
#define RES_KNOWLEDGE 3
#define RES_OIL     5
#define B_QCOUNT    0x82   /* byte: items in the production queue */
#define B_QUEUE     0x88   /* BuildQueue: [0] capacity, [1] items, 0x14 each, short type at +4 */
#define GROUP_SIZE  0x9F0
#define OBJ_FLAGS   0x08
#define OBJ_OWNER   0x09
#define OBJ_ID      0x0A
#define OBJ_X       0x10
#define OBJ_Y       0x14
#define OBJ_TYPE    0x18
#define U_FLAGS     0x68   /* bit 0: decoy */
#define VT_MAX_HITS 0x11C
#define VT_TAKE_DMG 0x16C  /* take_damage(hits, sixteenths, kind, ammo, quiet, 0x55555555, who id, who p, 0) */
#define VT_ON_MAP   0xBC
#define TD_TYPE     0x04
#define TD_MASK     0x1E4  /* & 0x8000000: a missile */
#define TD_SPLASH   0x200  /* tiles */
#define TD_DOMAIN   0x218  /* 2 = air */
#define TD_UBER     0x308  /* soldiers in a squad */
#define AM_FLAGS    0x04   /* & 3: in use */
#define AM_SX       0x0C
#define AM_SY       0x10
#define AM_EX       0x18
#define AM_EY       0x1C
#define AM_CUR      0x24
#define AM_TOTAL    0x28
#define AM_WHO      0x3C
#define AM_O        0x40
#define COORD_KEY   0x63637
#define TILE        192
#define REF_DIST    18544.0f /* zoom distance at level 5 (ron_zoom_trace.csv, 2026-10-23) */

#define NUKE        0x13B
#define ICBM        0x13C
#define SILO        0x208
#define INTERCEPTOR 0x183    /* unit 337 */
#define SILO_MAX    2
#define LIFTOFF     4        /* frames before an Interceptor leaves the silo (about a quarter second) */
#define SND_SHIELD  0x9A     /* the game's sound when nuke-fx's own interception sound is missing */
#define VA_SND_FIND 0x0097E9F0u /* SoundFileLookup::find_index(const String &tag): soundfiles.xml order */
#define VA_SND_PLAY 0x0097EAC0u /* SoundFileLookup::play(int index), ret 4 (both use globals only) */
#define VA_CSTR     0x00A1D660u /* String::String(const char *) */
#define VA_STR_CLOSE 0x00A1CF40u
#define MAX_BURSTS  8
#define SPAN_GAME   0x140    /* the game's scorch span after the 110 effect frames */
#define MAX_SILOS   256
#define MAX_SHOTS   64
#define MAX_ZONES   64

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);
typedef int(__attribute__((thiscall)) *add_nuke_t)(void *self, int x, int y, int r0, int r1, int p, int id);
typedef int(__attribute__((thiscall)) *take_damage_t)(void *self, int hits, int sixteenths, int kind, int ammo,
                                                       int quiet, unsigned mask, int who, int who_p, int z);
typedef char *(__attribute__((thiscall)) *get_tone_t)(void *graphsys);

/* settings ([nuke] in ron_camera.ini) */
static int g_flash = 1;
static float g_full_tiles = 12.0f, g_zero_tiles = 40.0f, g_far = 0.0f, g_strength = 1.0f;
int g_nuke_sim_enabled;
static int g_interceptors = 1, g_reach_tiles = 40, g_speed_pct = 400, g_protect_allies = 0;
static float g_burst_scale = 2.0f, g_burst_flash = 0.08f; /* the airburst: fireball size, light (of a nuke's) */
static int g_radiation = 1, g_rad_pct = 2, g_rad_secs = 60;
static int g_marker = 1, g_scorch_pct = 300;
static int g_ai = 1, g_ai_knowledge = 500, g_ai_oil = 550;

/* simulation state */
static struct silo { signed char p; short id; signed char n; } g_silo[MAX_SILOS];
static int g_n_silos;
static struct shot { int ammo, who, o, ex, ey, at, sp, sid; } g_shot[MAX_SHOTS];
static struct zone { int x, y, r, start; } g_zone[MAX_ZONES];
static int g_next_zone, g_last_frame = -1;

static int g_logged[8] = { -1, -1, -1, -1, -1, -1, -1, -1 }, g_next_logged; /* blast starts the log has named */

void nukefx_load_config(const char *ini_path)
{
    g_flash = GetPrivateProfileIntA("nuke", "flash_by_distance", 1, ini_path);
    g_full_tiles = ini_float(ini_path, "nuke", "flash_full_tiles", 12.0f);
    g_zero_tiles = ini_float(ini_path, "nuke", "flash_zero_tiles", 40.0f);
    g_far = ini_float(ini_path, "nuke", "flash_far_percent", 0.0f) / 100.0f;
    g_strength = ini_float(ini_path, "nuke", "flash_percent", 100.0f) / 100.0f;
    if (g_zero_tiles <= g_full_tiles)
        g_zero_tiles = g_full_tiles + 1.0f;
    g_far = g_far < 0.0f ? 0.0f : g_far > 1.0f ? 1.0f : g_far;
    g_strength = g_strength < 0.0f ? 0.0f : g_strength > 1.0f ? 1.0f : g_strength;
    g_interceptors = GetPrivateProfileIntA("nuke", "interceptors", 1, ini_path);
    g_reach_tiles = GetPrivateProfileIntA("nuke", "interceptor_reach_tiles", 40, ini_path);
    g_speed_pct = GetPrivateProfileIntA("nuke", "interceptor_speed_percent", 400, ini_path);
    if (g_speed_pct == 250) /* the old default, kept in the ini of every earlier install (2026-11-05) */
        g_speed_pct = 400;
    g_burst_scale = ini_float(ini_path, "nuke", "interception_burst_size", 2.0f);
    g_burst_flash = ini_float(ini_path, "nuke", "interception_flash_percent", 8.0f) / 100.0f;
    g_burst_flash = g_burst_flash < 0.0f ? 0.0f : g_burst_flash > 1.0f ? 1.0f : g_burst_flash;
    g_protect_allies = GetPrivateProfileIntA("nuke", "interceptor_protect_allies", 0, ini_path);
    g_radiation = GetPrivateProfileIntA("nuke", "radiation", 1, ini_path);
    g_rad_pct = GetPrivateProfileIntA("nuke", "radiation_percent_per_second", 2, ini_path);
    g_rad_secs = GetPrivateProfileIntA("nuke", "radiation_seconds", 60, ini_path);
    g_marker = GetPrivateProfileIntA("nuke", "target_marker", 1, ini_path);
    g_scorch_pct = GetPrivateProfileIntA("nuke", "scorch_percent", 300, ini_path);
    g_ai = GetPrivateProfileIntA("nuke", "ai_interceptors", 1, ini_path);
    g_ai_knowledge = GetPrivateProfileIntA("nuke", "ai_knowledge_spare", 500, ini_path);
    g_ai_oil = GetPrivateProfileIntA("nuke", "ai_oil_spare", 550, ini_path);
    if (g_speed_pct < 110)
        g_speed_pct = 110;
    if (g_rad_secs < 1)
        g_rad_secs = 1;
    if (g_scorch_pct < 100 || g_scorch_pct > 1000)
        g_scorch_pct = g_scorch_pct < 100 ? 100 : 1000;
    g_nuke_sim_enabled = g_interceptors || g_radiation;
}

/* ---- game access ---------------------------------------------------------------------- */

static char *game(void) { return *(char **)VA(VA_GAME_PTR); }
static int now(void) { return *(int *)(game() + GAME_TIME); }
static char *leader(int p) { return (char *)VA(VA_LEADERS) + p * LEADER_SIZE; }
static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }
static char *type_data(char *o) { return *(char **)(o + OBJ_TYPE); }
static int type_of(char *o) { return *(int *)(type_data(o) + TD_TYPE); }

static char *slot(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    return arr ? arr[id] : NULL;
}

static char *object(int p, int id)
{
    char *o = slot(p, id);
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

static int obj_x(char *o) { return *(int *)(o + OBJ_X) ^ COORD_KEY; }
static int obj_y(char *o) { return *(int *)(o + OBJ_Y) ^ COORD_KEY; }

static long long sq(long long v) { return v * v; }

/* Items of a type in a building's production queue (only the first byte [+0x82] slots are real:
   a finished item's slot keeps its old type). */
static int queued(char *b, int type)
{
    char *items = *(char **)(b + B_QUEUE + 4);
    int n = 0;
    for (int i = 0; items && i < (unsigned char)b[B_QCOUNT]; i++)
        n += *(short *)(items + i * 0x14 + 4) == type;
    return n;
}

/* The nuke an ammo record carries, or 0: its shooter (the missile, dead once it fired) keeps its
   type, as ObjectsData::count_nukes_in_flight reads it. */
static int nuke_type(char *a)
{
    if (!a || !(a[AM_FLAGS] & 3))
        return 0;
    char *sh = slot(*(int *)(a + AM_WHO), *(int *)(a + AM_O));
    if (!sh || !type_data(sh) || !(*(unsigned *)(type_data(sh) + TD_MASK) & 0x8000000))
        return 0;
    int t = type_of(sh);
    return t == NUKE || t == ICBM ? t : 0;
}

/* For nonukes.c: Nuclear Missiles and ICBMs in the air now (an Interceptor's kill leaves the list). */
int nukefx_nukes_in_flight(void)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    int n = objs ? *(int *)(objs + OBJS_AMMO_N) : 0, k = 0;
    char **am = objs ? *(char ***)(objs + OBJS_AMMO) : NULL;
    for (int i = 0; i < n && am; i++)
        k += nuke_type(am[i]) != 0;
    return k;
}

static int nuke_radius(char *a)
{
    return *(int *)(type_data(slot(*(int *)(a + AM_WHO), *(int *)(a + AM_O))) + TD_SPLASH) * TILE;
}

/* Ground position of an ammo at frame f of its flight (the spline's x, y run straight). */
static void ammo_at(char *a, int f, int *x, int *y)
{
    int tot = *(int *)(a + AM_TOTAL);
    int sx = *(int *)(a + AM_SX), sy = *(int *)(a + AM_SY), ex = *(int *)(a + AM_EX), ey = *(int *)(a + AM_EY);
    if (tot <= 0 || f >= tot) {
        *x = ex;
        *y = ey;
        return;
    }
    *x = sx + (int)((long long)(ex - sx) * f / tot);
    *y = sy + (int)((long long)(ey - sy) * f / tot);
}

/* ---- simulation: interceptors -------------------------------------------------------- */

static struct silo *silo_rec(int p, int id, int make)
{
    for (int i = 0; i < g_n_silos; i++)
        if (g_silo[i].p == p && g_silo[i].id == id)
            return &g_silo[i];
    if (!make)
        return NULL;
    for (int i = 0; i < g_n_silos; i++) /* a slot of a silo that is gone */
        if (!g_silo[i].n) {
            char *b = object(g_silo[i].p, g_silo[i].id);
            if (!b || type_of(b) != SILO) {
                g_silo[i] = (struct silo){ (signed char)p, (short)id, 0 };
                return &g_silo[i];
            }
        }
    if (g_n_silos >= MAX_SILOS)
        return NULL;
    g_silo[g_n_silos] = (struct silo){ (signed char)p, (short)id, 0 };
    return &g_silo[g_n_silos++];
}

/* The silo a finished Interceptor of player p belongs to: the one it is in, else the nearest. */
static char *home_silo(char *u, int p)
{
    int in_p = -1, in = ((fn1_t)VA(VA_GET_INSIDE))(u, (int)(INT_PTR)&in_p);
    char *b = in >= 0 ? object(in_p, in) : NULL;
    if (b && type_of(b) == SILO && (unsigned char)b[OBJ_OWNER] == p)
        return b;
    char *best = NULL;
    long long bd = 0;
    int n = *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_BUILD_N + 4 * p);
    for (int id = FIRST_BUILD; id < n; id++) {
        char *s = object(p, id);
        if (!s || type_of(s) != SILO)
            continue;
        long long d = sq(obj_x(s) - obj_x(u)) + sq(obj_y(s) - obj_y(u));
        if (!best || d < bd)
            best = s, bd = d;
    }
    return best;
}

static void collect_interceptors(void)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    for (int p = 0; p < 8; p++) {
        int n = *(int *)(objs + OBJS_UNITS_N + 4 * p);
        for (int id = 0; id < n && id < FIRST_BUILD; id++) {
            char *u = object(p, id);
            if (!u || type_of(u) != INTERCEPTOR)
                continue;
            char *b = home_silo(u, p);
            struct silo *s = b ? silo_rec(p, *(short *)(b + OBJ_ID), 1) : NULL;
            if (s && s->n < SILO_MAX)
                s->n++;
            logf_("nuke: Interceptor %d of player %d into silo %d (%d there)", id, p, b ? *(short *)(b + OBJ_ID) : -1,
                  s ? s->n : -1);
            ((fn1_t)VA(VA_DISBAND))(u, 0);
        }
    }
}

/* A shot knows its nuke by owner, missile id and end point (the slot index may differ after a
   saved game is loaded). */
static int same_nuke(const struct shot *s, char *a)
{
    return nuke_type(a) && *(int *)(a + AM_WHO) == s->who && *(int *)(a + AM_O) == s->o &&
           *(int *)(a + AM_EX) == s->ex && *(int *)(a + AM_EY) == s->ey;
}

static char *shot_nuke(struct shot *s)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    int n = *(int *)(objs + OBJS_AMMO_N);
    char **am = *(char ***)(objs + OBJS_AMMO);
    if (!am)
        return NULL;
    if (s->ammo >= 0 && s->ammo < n && same_nuke(s, am[s->ammo]))
        return am[s->ammo];
    for (int i = 0; i < n; i++)
        if (same_nuke(s, am[i])) {
            s->ammo = i;
            return am[i];
        }
    return NULL;
}

static int is_targeted(char *a)
{
    for (int i = 0; i < MAX_SHOTS; i++)
        if (g_shot[i].at && same_nuke(&g_shot[i], a))
            return 1;
    return 0;
}

/* nuke-fx's sounds, soundfiles.xml tags (the index is the entry's place in that file) */
static int sound_index(const char *tag, int *cache)
{
    if (*cache == -1) {
        unsigned char str[64] = { 0 };
        ((void(__attribute__((thiscall)) *)(void *, const char *))VA(VA_CSTR))(str, tag);
        *cache = ((fn1_t)VA(VA_SND_FIND))(NULL, (int)(INT_PTR)str);
        ((fn0_t)VA(VA_STR_CLOSE))(str);
        logf_("nuke: sound %s %s", tag, *cache >= 0 ? "found" : "missing from soundfiles.xml (nuke-fx data not installed?)");
        if (*cache < 0)
            *cache = -2;
    }
    return *cache;
}

static int g_snd_launch = -1, g_snd_hit = -1;

/* Heard by the two players it concerns: the silo's owner and the nuke's. */
static void local_sound(int p1, int p2, int hit)
{
    int me = local_player();
    if (me != p1 && me != p2)
        return;
    int i = hit ? sound_index("interceptorhit", &g_snd_hit) : sound_index("interceptorlaunch", &g_snd_launch);
    if (i >= 0)
        ((fn1_t)VA(VA_SND_PLAY))(NULL, i);
    else if (hit)
        ((fn1_t)VA(VA_SOUND_PLAY))(NULL, SND_SHIELD);
}

/* Where and when Interceptors met their nukes, for the soft light (drawing only). */
static struct burst { float x, y; int start; } g_burst[MAX_BURSTS];
static int g_next_burst;

/* Frames from now until an Interceptor from (bx, by) meets the nuke, or 0 when it cannot before
   the nuke lands. Integer maths only. */
static int meet_frames(char *a, int bx, int by)
{
    int cur = *(int *)(a + AM_CUR), tot = *(int *)(a + AM_TOTAL);
    if (tot <= 0 || cur >= tot)
        return 0;
    long long len = (long long)sqrt((double)(sq(*(int *)(a + AM_EX) - *(int *)(a + AM_SX)) +
                                             sq(*(int *)(a + AM_EY) - *(int *)(a + AM_SY))));
    long long v = len * g_speed_pct / (100LL * tot); /* the Interceptor's world units per frame */
    if (v < 64)
        v = 64;
    for (int k = LIFTOFF + 1; cur + k < tot; k++) {
        int x, y;
        ammo_at(a, cur + k, &x, &y);
        if (sq((k - LIFTOFF) * v) >= sq(x - bx) + sq(y - by))
            return k;
    }
    return 0;
}

/* A point of an ammo's spline (one per frame), clamped to its last. */
static void spline_point(char *a, int f, float out[3])
{
    char *s = *(char **)(a + 0x68);
    int n = s ? *(int *)(s + 0xD8) : 0;
    if (n <= 0) {
        out[0] = (float)*(int *)(a + AM_EX), out[1] = (float)*(int *)(a + AM_EY), out[2] = 0.0f;
        return;
    }
    const float *p = *(const float **)(s + 0xE4) + 3 * (f < 0 ? 0 : f >= n ? n - 1 : f);
    out[0] = p[0], out[1] = p[1], out[2] = p[2];
}

/* The visible Interceptor: a missile Ammo of the Interceptor type (the Cruise Missile's look and
   smoke trail) from the silo, whose spline is rewritten to rise out of the silo and reach the
   nuke's own point at the meeting frame k, where the game draws its explosion (flag 0x10: no
   damage). The game makes a missile Ammo only for a living missile shooter, so a short-lived
   Interceptor unit is made at the silo for it and dies at once (its slot stays reserved while the
   Ammo flies, as Object::die does for a missile). The interception itself does not depend on it. */
static void launch_visual(char *b, int p, char *nuke, int k)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    int id = ((int(__attribute__((thiscall)) *)(void *, int, int, int, int, int, int, int))VA(VA_INIT_UNIT))(
        objs, p, INTERCEPTOR, obj_x(b), obj_y(b), -1, -1, -1);
    char *u = object(p, id);
    if (!u) {
        logf_("nuke: the Interceptor's missile could not be made (player %d)", p);
        return;
    }
    char pkg[0x50];
    memset(pkg, 0, sizeof pkg);
    *(int *)pkg = *(int *)(type_data(u) + TD_AMMO_PIECE);
    *(short *)(pkg + 0x32) = (short)id; /* shooter */
    pkg[0x36] = (char)p;
    *(short *)(pkg + 0x34) = *(short *)(b + OBJ_ID); /* a live target, or the game makes no Ammo */
    pkg[0x37] = (char)p;
    int i = ((fn1_t)VA(VA_ADD_AMMO))(objs, (int)(INT_PTR)pkg);
    char **am = *(char ***)(objs + OBJS_AMMO);
    char *a = am && i >= 0 && i < *(int *)(objs + OBJS_AMMO_N) ? am[i] : NULL;
    char *s = a ? *(char **)(a + 0x68) : NULL;
    if (a && (a[AM_FLAGS] & 3) && *(int *)(a + AM_WHO) == p && *(int *)(a + AM_O) == id && s) {
        float m[3], p0[3] = { (float)obj_x(b), (float)obj_y(b), 0.0f };
        spline_point(nuke, *(int *)(nuke + AM_CUR) + k, m);
        int gz = 0;
        void *terrain = *(void **)VA(VA_TERRAIN);
        if (terrain)
            ((fn4_t)VA(VA_FIND_Z))(terrain, (int)(INT_PTR)&gz, obj_x(b), obj_y(b), 1);
        p0[2] = (float)(gz + MISSILE_Z);
        *(int *)(a + AM_SX) = (int)p0[0], *(int *)(a + AM_SY) = (int)p0[1], *(int *)(a + AM_SX + 8) = (int)p0[2];
        *(int *)(a + AM_EX) = (int)m[0], *(int *)(a + AM_EY) = (int)m[1], *(int *)(a + AM_EX + 8) = (int)m[2];
        *(int *)(a + AM_CUR) = 0;
        *(int *)(a + AM_TOTAL) = k;
        a[AM_FLAGS] |= 0x10;
        *(int *)(a + 0x48) = *(int *)(a + 0x4C) = -1; /* no homing */
        /* the points: a quadratic curve leaving the silo straight up */
        char *arr = s + 0xD4;
        for (int tries = 0; tries < 4 && *(int *)(s + 0xDC) < k + 1; tries++)
            ((fn1_t)(*(void ***)arr)[0])(arr, k + 1 - *(int *)(s + 0xDC) + 16);
        if (*(int *)(s + 0xDC) >= k + 1) {
            float c[3] = { p0[0], p0[1], fmaxf(p0[2], m[2]) + 2000.0f };
            float *pt = *(float **)(s + 0xE4);
            for (int j = 0; j <= k; j++) {
                float t = (float)j / (float)k, u0 = (1 - t) * (1 - t), u1 = 2 * t * (1 - t), u2 = t * t;
                for (int d = 0; d < 3; d++)
                    pt[3 * j + d] = u0 * p0[d] + u1 * c[d] + u2 * m[d];
            }
            *(int *)(s + 0xD8) = k + 1;
            ((fn0_t)VA(VA_BUILD_NORMALS))(s);
        } else
            logf_("nuke: the Interceptor's path could not be made longer (%d points)", *(int *)(s + 0xDC));
        static int logged;
        if (logged++ < 5)
            logf_("nuke: Interceptor missile, ammo %d, %d frames, from (%.0f, %.0f, %.0f) to (%.0f, %.0f, %.0f)", i, k,
                  p0[0], p0[1], p0[2], m[0], m[1], m[2]);
    } else
        logf_("nuke: the game made no missile for the Interceptor (slot %d)", i);
    ((int(__attribute__((thiscall)) *)(void *, int, int, float))(*(void ***)u)[VT_DIE / 4])(u, 0, -1, 0.0f);
    int hold = k + *(int *)VA(VA_DEAD_HOLD) + 1;
    if (*(unsigned short *)(u + 0x32) < hold)
        *(unsigned short *)(u + 0x32) = (unsigned short)hold;
}

/* The airburst where they meet: a large fireball and a shockwave ring (graphics only, no random). */
static void airburst(float x, float y, float z)
{
    void *gp = *(void **)VA(VA_GPIECES);
    if (!gp)
        return;
    void *fx = (void *)VA(VA_ADD_EXPLOSION), *fp = (void *)VA(VA_ADD_PULSEWAVE);
    float scale = g_burst_scale;
    int sbits;
    memcpy(&sbits, &scale, 4);
    __asm__ volatile("movss %0, %%xmm1\n\t"
                     "movss %1, %%xmm2\n\t"
                     "movss %2, %%xmm3\n\t"
                     "push $0\n\t"
                     "push $0\n\t"
                     "push %4\n\t"
                     "mov %3, %%ecx\n\t"
                     "call *%5"
                     :
                     : "m"(x), "m"(y), "m"(z), "r"(gp), "r"(sbits), "r"(fx)
                     : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory",
                       "cc");
    __asm__ volatile("movss %0, %%xmm1\n\t"
                     "movss %1, %%xmm2\n\t"
                     "movss %2, %%xmm3\n\t"
                     "mov %3, %%ecx\n\t"
                     "call *%4"
                     :
                     : "m"(x), "m"(y), "m"(z), "r"(gp), "r"(fp)
                     : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory",
                       "cc");
}

/* Whose territory a point is in (-1: nobody, or the sea), as Ammo::do_damage reads it for the
   Missile Shield: the 4 x 4 tile cell div3[x >> 8], div3[y >> 8] of World +0x134 (0x1C bytes a
   cell, owner at +0xF). */
static int territory_owner(int x, int y)
{
    char *world = *(char **)VA(VA_WORLD);
    const int *div3 = *(const int **)VA(VA_DIV3);
    if (!world || !div3 || x < 0 || y < 0)
        return -1;
    int wx = div3[x >> 8], wy = div3[y >> 8], w = *(int *)world, h = *(int *)(world + 4);
    if (wx >= w || wy >= h)
        return -1;
    return *(signed char *)(*(char **)(world + 0x134) + (wy * w + wx) * 0x1C + 0xF);
}

/* A silo of player p fires only at a nuke aimed at p (or, with interceptor_protect_allies, at an
   ally): its target object is p's, or its end point is in p's territory. */
static int aimed_at(char *a, int p)
{
    int whom = *(int *)(a + 0x48);
    if (whom < 0 || whom > 7)
        whom = territory_owner(*(int *)(a + AM_EX), *(int *)(a + AM_EY));
    if (whom == p)
        return 1;
    return g_protect_allies && whom >= 0 && whom <= 7 && ((fn1_t)VA(VA_IS_ALLY))(leader(p), whom);
}

static void fire_interceptors(void)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    int n = *(int *)(objs + OBJS_AMMO_N), t = now();
    char **am = *(char ***)(objs + OBJS_AMMO);
    long long reach = (long long)g_reach_tiles * TILE;
    for (int i = 0; i < n && am; i++) {
        char *a = am[i];
        if (!nuke_type(a) || is_targeted(a))
            continue;
        int who = *(int *)(a + AM_WHO), x, y;
        ammo_at(a, *(int *)(a + AM_CUR), &x, &y);
        int best = -1, bk = 0;
        for (int s = 0; s < g_n_silos; s++) {
            if (g_silo[s].n <= 0 || g_silo[s].p == who || !((fn1_t)VA(VA_IS_ENEMY))(leader(g_silo[s].p), who) ||
                !aimed_at(a, g_silo[s].p))
                continue;
            char *b = object(g_silo[s].p, g_silo[s].id);
            if (!b || type_of(b) != SILO) {
                g_silo[s].n = 0;
                continue;
            }
            if (sq(obj_x(b) - x) + sq(obj_y(b) - y) > sq(reach))
                continue;
            int k = meet_frames(a, obj_x(b), obj_y(b));
            if (k && (best < 0 || k < bk))
                best = s, bk = k;
        }
        if (best < 0)
            continue;
        for (int j = 0; j < MAX_SHOTS; j++)
            if (!g_shot[j].at) {
                g_silo[best].n--;
                g_shot[j] = (struct shot){ i, who, *(int *)(a + AM_O), *(int *)(a + AM_EX), *(int *)(a + AM_EY), t + bk,
                                           g_silo[best].p, g_silo[best].id };
                logf_("nuke: silo %d of player %d fires at the nuke of player %d (ammo %d, frame %d of %d), meets it in %d "
                      "frames, %d left",
                      g_silo[best].id, g_silo[best].p, who, i, *(int *)(a + AM_CUR), *(int *)(a + AM_TOTAL), bk,
                      g_silo[best].n);
                launch_visual(object(g_silo[best].p, g_silo[best].id), g_silo[best].p, a, bk);
                local_sound(who, g_silo[best].p, 0);
                break;
            }
    }
}

static void meet_interceptors(void)
{
    int t = now();
    for (int j = 0; j < MAX_SHOTS; j++) {
        struct shot *s = &g_shot[j];
        if (!s->at || t < s->at)
            continue;
        char *a = shot_nuke(s);
        if (a) {
            float m[3];
            spline_point(a, *(int *)(a + AM_CUR), m);
            ((fn0_t)VA(VA_AMMO_CLOSE))(a);
            airburst(m[0], m[1], m[2]);
            g_burst[g_next_burst] = (struct burst){ m[0], m[1], t };
            g_next_burst = (g_next_burst + 1) % MAX_BURSTS;
            logf_("nuke: the nuke of player %d shot down by silo %d of player %d", s->who, s->sid, s->sp);
            local_sound(s->who, s->sp, 1);
        }
        s->at = 0;
    }
}

/* ---- simulation: radiation ------------------------------------------------------------ */

__attribute__((thiscall)) static int rad_add_nuke(void *self, int x, int y, int r0, int r1, int p, int id)
{
    g_zone[g_next_zone] = (struct zone){ x, y, r1, now() };
    g_next_zone = (g_next_zone + 1) % MAX_ZONES;
    logf_("nuke: blast of player %d at (%d, %d) tiles, radius %d tiles", p, x / TILE, y / TILE, r1 / TILE);
    return ((add_nuke_t)VA(VA_ADD_NUKE))(self, x, y, r0, r1, p, id);
}

/* Once a second, every unit in a young zone loses pct(age) of its max hit points, pct falling
   from radiation_percent_per_second to 0. In sixteenths, per soldier of a squad. */
static void radiate(void)
{
    int t = now(), life = g_rad_secs * 15;
    char *objs = *(char **)VA(VA_OBJECTS);
    for (int z = 0; z < MAX_ZONES; z++) {
        struct zone *zn = &g_zone[z];
        int age = t - zn->start;
        if (!zn->r || age <= 0 || age > life || age % 15)
            continue;
        long long r2 = sq(zn->r);
        for (int p = 0; p < 8; p++) {
            int n = *(int *)(objs + OBJS_UNITS_N + 4 * p);
            for (int id = 0; id < n && id < FIRST_BUILD; id++) {
                char *u = object(p, id);
                if (!u || (*(unsigned *)(u + U_FLAGS) & 1) || *(int *)(type_data(u) + TD_DOMAIN) == 2 ||
                    !((fn0_t)(*(void ***)u)[VT_ON_MAP / 4])(u) || sq(obj_x(u) - zn->x) + sq(obj_y(u) - zn->y) > r2)
                    continue;
                int soldiers = *(int *)(type_data(u) + TD_UBER) > 1 ? ((fn1_t)VA(VA_CURR_UBER))(u, 0) : 1;
                long long hits = ((fn1_t)(*(void ***)u)[VT_MAX_HITS / 4])(u, 0);
                long long d16 = hits * 16 * g_rad_pct * (life - age) / (100LL * life);
                if (soldiers > 1)
                    d16 /= soldiers;
                if (d16 < 1)
                    d16 = 1;
                ((take_damage_t)(*(void ***)u)[VT_TAKE_DMG / 4])(u, (int)(d16 / 16), (int)(d16 % 16), 1, -1, 1,
                                                                  0x55555555, -1, -1, 0);
            }
        }
    }
}

static void ai_buy(int p);
static void clear_state(void);

/* Once per simulation frame (airdrop.c's Objects::process_all), on every machine. */
void nukefx_frame(void)
{
    if (!g_nuke_sim_enabled)
        return;
    int t = now();
    if (t < g_last_frame || t - g_last_frame > 15) /* another game (a saved game sets g_last_frame) */
        clear_state();
    g_last_frame = t;
    if (g_interceptors) {
        collect_interceptors();
        meet_interceptors();
        fire_interceptors();
    }
    if (g_radiation)
        radiate();
    if (g_interceptors && g_ai)
        for (int p = 0; p < 8; p++)
            if ((t + p * 17 + 5) % 150 == 0)
                ai_buy(p);
}

/* ---- simulation: computer players ------------------------------------------------------ */

/* The game's own AI offers Interceptors like Cruise Missiles, without a limit (assistant.c's
   make_me hook drops those offers, nukefx_drops_offer); this buys them instead. Every 150
   frames per computer player: when a living enemy has or can build nukes, the first finished
   silo with fewer than SILO_MAX, nothing queued and room for a missile (can_carry: a silo makes
   or holds one missile at a time) queues one, paid from money the AI has not set aside. */
static int stock(int p, int r) { return (*(int **)(leader(p) + LD_SECURE))[r] ^ STOCK_KEY; }
static int spare(int p, int r) { return stock(p, r) - *(int *)(leader(p) + LD_RESERVED + 4 * r); }

static int alive_player(int q)
{
    int f = *(int *)leader(q);
    return (f & 2) && !(f & 0x40) && *(int *)(leader(q) + 8) == q;
}

static int nuclear_threat(int p)
{
    for (int q = 0; q < 8; q++) {
        if (q == p || !alive_player(q) || !((fn1_t)VA(VA_IS_ENEMY))(leader(p), q))
            continue;
        char *lq = leader(q);
        if (((fn0_t)VA(VA_GET_NUKES))(lq) > 0 || ((fn2_t)VA(VA_TYPE_AVAIL))(lq, NUKE, 1) == 4 ||
            ((fn2_t)VA(VA_TYPE_AVAIL))(lq, ICBM, 1) == 4)
            return 1;
    }
    return 0;
}

static void ai_buy(int p)
{
    if (!alive_player(p) || (*(int *)leader(p) & 4) || ((fn2_t)VA(VA_TYPE_AVAIL))(leader(p), INTERCEPTOR, 1) != 4)
        return;
    if (spare(p, RES_KNOWLEDGE) < g_ai_knowledge || spare(p, RES_OIL) < g_ai_oil || !nuclear_threat(p))
        return;
    int n = *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_BUILD_N + 4 * p);
    for (int id = FIRST_BUILD; id < n; id++) {
        char *b = object(p, id);
        if (!b || type_of(b) != SILO || !((fn0_t)VA(VA_ACTIVE_BUILD))(b) || (unsigned char)b[B_QCOUNT])
            continue;
        struct silo *s = silo_rec(p, id, 0);
        if ((s && s->n >= SILO_MAX) || !((fn1_t)VA(VA_CAN_CARRY))(b, INTERCEPTOR))
            continue;
        int r = ((fn2_t)VA(VA_QUEUE_UP))(b, INTERCEPTOR, 0);
        logf_("nuke: computer player %d orders an Interceptor at silo %d (%s)", p, id, r ? "refused" : "queued");
        return;
    }
}

int nukefx_drops_offer(int type) { return g_interceptors && type == INTERCEPTOR; }

/* ---- saved games -------------------------------------------------------------------------- */

/* The silos' Interceptors, the interceptions in flight and the radiation zones go into the saved
   game inside ScenarioData::temp_save, a String the game saves and loads but never uses: the
   call of ScenarioData::walk_data in WalkDataGame::walk_data (the save / load walker only; walker
   +4 = 1 when loading) goes through c_scen_walk. On a save the String holds the payload for that
   call only; afterwards it gets its own value back on every machine (it is part of the
   multiplayer checksums). The payload travels with the save to the other players of a
   multiplayer game and into replays. Each byte is stored as the character 0x100 | byte. */
#define SAVE_MAGIC 0x3158464E /* "NFX1" */

struct save {
    int magic, n_silos, next_zone;
    struct silo silo[MAX_SILOS];
    struct shot shot[MAX_SHOTS];
    struct zone zone[MAX_ZONES];
};

static struct save g_save;
static wchar_t g_save_text[sizeof(struct save) + 1];
static char g_keep[0x40]; /* a String: temp_save's own value while the payload is in it */

static void clear_state(void)
{
    memset(g_silo, 0, sizeof g_silo);
    memset(g_shot, 0, sizeof g_shot);
    memset(g_zone, 0, sizeof g_zone);
    memset(g_burst, 0, sizeof g_burst);
    g_n_silos = g_next_zone = g_next_burst = 0;
}

static void encode(void)
{
    g_save.magic = SAVE_MAGIC;
    g_save.n_silos = g_n_silos;
    g_save.next_zone = g_next_zone;
    memcpy(g_save.silo, g_silo, sizeof g_silo);
    memcpy(g_save.shot, g_shot, sizeof g_shot);
    memcpy(g_save.zone, g_zone, sizeof g_zone);
    const unsigned char *b = (const unsigned char *)&g_save;
    size_t n = sizeof g_save;
    while (n > 4 * sizeof(int) && !b[n - 1]) /* the tables' empty ends stay out of the save */
        n--;
    for (size_t i = 0; i < n; i++)
        g_save_text[i] = (wchar_t)(0x100 | b[i]);
    g_save_text[n] = 0;
}

static void decode(const wchar_t *s, int len)
{
    clear_state();
    memset(&g_save, 0, sizeof g_save);
    unsigned char *b = (unsigned char *)&g_save;
    for (int i = 0; s && i < len && i < (int)sizeof g_save; i++) {
        if ((s[i] & 0xFF00) != 0x100)
            return; /* the game's own text: a save without the mod's data */
        b[i] = (unsigned char)s[i];
    }
    if (g_save.magic != SAVE_MAGIC || g_save.n_silos < 0 || g_save.n_silos > MAX_SILOS ||
        g_save.next_zone < 0 || g_save.next_zone >= MAX_ZONES)
        return;
    g_n_silos = g_save.n_silos;
    g_next_zone = g_save.next_zone;
    memcpy(g_silo, g_save.silo, sizeof g_silo);
    memcpy(g_shot, g_save.shot, sizeof g_shot);
    memcpy(g_zone, g_save.zone, sizeof g_zone);
    int silos = 0, ints = 0, zones = 0;
    for (int i = 0; i < g_n_silos; i++)
        silos += g_silo[i].n > 0, ints += g_silo[i].n;
    for (int i = 0; i < MAX_ZONES; i++)
        zones += g_zone[i].r != 0;
    logf_("nuke: loaded from the saved game: %d Interceptors in %d silos, %d blasts remembered", ints, silos, zones);
}

static void __attribute__((fastcall)) c_scen_walk(char *walker)
{
    typedef void(__attribute__((fastcall)) * walk_t)(void *walker);
    typedef int(__attribute__((thiscall)) * set_t)(void *self, const void *src);
    char *ts = (char *)VA(VA_TEMP_SAVE);
    ((set_t)VA(VA_STR_SET))(g_keep, ts);
    if (!*(int *)(walker + 4)) {
        encode();
        ((set_t)VA(VA_STR_SET_W))(ts, g_save_text);
        ((walk_t)VA(VA_SCEN_WALK))(walker);
    } else {
        ((walk_t)VA(VA_SCEN_WALK))(walker);
        const wchar_t *s = ((const wchar_t *(__attribute__((thiscall)) *)(void *))VA(VA_STR_STR))(ts);
        decode(s, *(unsigned short *)(ts + 8));
        g_last_frame = now(); /* the time jump of the load is not a new game */
    }
    ((set_t)VA(VA_STR_SET))(ts, g_keep);
}

/* ---- interface: the silo's button ------------------------------------------------------ */

/* After BuildOut::make_options (airdrop.c's hook): the Interceptor button of a silo that holds
   or is making SILO_MAX is disabled. Options entries: 0x14 bytes, +4 type, +0xC disabled. */
void nukefx_make_options(char *b)
{
    if (!g_interceptors || type_of(b) != SILO)
        return;
    struct silo *s = silo_rec((unsigned char)b[OBJ_OWNER], *(short *)(b + OBJ_ID), 0);
    if ((s ? s->n : 0) + queued(b, INTERCEPTOR) < SILO_MAX)
        return;
    char *opts = *(char **)VA(VA_OPTIONS);
    for (int i = 0; opts && i < *(int *)(opts + 0x20); i++) {
        char *e = *(char **)(opts + 0x10) + i * 0x14;
        if (*(int *)(e + 4) == INTERCEPTOR)
            *(int *)(e + 0xC) = 1;
    }
}

int nukefx_is_option(int code, int type)
{
    (void)code;
    return g_interceptors && type == INTERCEPTOR;
}

/* For rally.c: the radius (world units) of the interception ring drawn on the map around a
   selected Missile Silo of type `type`, 0 for any other building or with interceptors off. */
int nukefx_reach_ring(int type)
{
    return g_nuke_sim_enabled && g_interceptors && type == SILO ? g_reach_tiles * TILE : 0;
}

/* For panel.c: Interceptors ready in the local player's selected silos. */
int nukefx_charges(void)
{
    char *console = *(char **)VA(VA_CONSOLE);
    if (!console)
        return 0;
    char *group = *(char **)VA(VA_SELECTION) + *(int *)(console + 0x2A0) * GROUP_SIZE;
    int p = (unsigned char)group[0x4A], n = *(int *)(group + 0xC), sum = 0;
    short *ids = (short *)(group + 0x8CC);
    for (int i = 0; i < n; i++) {
        struct silo *s = silo_rec(p, ids[i], 0);
        sum += s ? s->n : 0;
    }
    return sum;
}

/* ---- drawing ---------------------------------------------------------------------------- */

/* 1 near the screen centre, g_far from g_zero_tiles on, smooth in between. The radii grow
   with the zoom distance, so a zoomed-out view counts a wider area as "looking at it". */
static float distance_factor(float bx, float by)
{
    char *cam = *(char **)VA(VA_CAMERA_PTR);
    if (!cam)
        return 1.0f;
    float zoom = *(float *)(cam + CAM_DIST) / REF_DIST;
    zoom = zoom < 0.5f ? 0.5f : zoom > 3.0f ? 3.0f : zoom;
    float dx = bx - *(float *)(cam + CAM_LOC_X), dy = by - *(float *)(cam + CAM_LOC_Y);
    float d = sqrtf(dx * dx + dy * dy) / (TILE * zoom);
    float u = (d - g_full_tiles) / (g_zero_tiles - g_full_tiles);
    if (u <= 0.0f)
        return 1.0f;
    if (u >= 1.0f)
        return g_far;
    u = u * u * (3.0f - 2.0f * u);
    return 1.0f - u * (1.0f - g_far);
}

/* Per blast i, t = (game time - start[i]) / duration, the game writes while t <= 1:
     a = t < 0.05 ? (t / 0.05)^2 : 1 - min(1, (t - 0.05) / 0.45)^2
     b = t < 0.05 ? (t / 0.05)^2 :     min(1, (t - 0.05) / 0.65)^2
     tone +0x0C = 32 a^2 (the flash), +0x1C = b (saturation), tint alpha = (0.4 (1 - b))^2 * 255.
   At rest 0, 1 and 0. This writes the strongest of the blasts scaled by their distance factor. */
static int g_tone_ours; /* this wrote the tone mapper last frame (an airburst's light) */

static void soften_flash(void)
{
    void *gs = *(void **)VA(VA_GRAPHSYS);
    int n = *(int *)VA(VA_NUKE_COUNT), frames = *(int *)VA(VA_NUKE_FRAMES);
    if (!gs)
        return;
    char *tone = ((get_tone_t)(*(void ***)gs)[0xBC / 4])(gs);
    if (!tone)
        return;
    int t0 = now(), active = 0, blasts = 0;
    float flash = 0.0f, sat = 1.0f;
    if (g_flash && n > 0 && frames > 0) {
        const float(*pos)[2] = *(const float(**)[2])VA(VA_NUKE_POS);
        const int *start = *(const int **)VA(VA_NUKE_START);
        for (int i = 0; i < n; i++) {
            float t = (float)(t0 - start[i]) / (float)frames;
            if (t < 0.0f || t > 1.0f)
                continue;
            float a, b, f = distance_factor(pos[i][0], pos[i][1]) * g_strength;
            if (t < 0.05f) {
                a = b = (t / 0.05f) * (t / 0.05f);
            } else {
                float ua = fminf(1.0f, (t - 0.05f) / 0.45f), ub = fminf(1.0f, (t - 0.05f) / 0.65f);
                a = 1.0f - ua * ua;
                b = ub * ub;
            }
            flash = fmaxf(flash, f * 32.0f * a * a);
            sat = fminf(sat, 1.0f - f * (1.0f - b));
            active = blasts = 1;
            int seen = 0; /* once per blast, also while several are lit at once */
            for (int k = 0; k < 8; k++)
                seen |= g_logged[k] == start[i];
            if (!seen) {
                g_logged[g_next_logged++ & 7] = start[i];
                logf_("nuke flash: blast at (%.0f, %.0f), flash %.0f%%", pos[i][0] / TILE, pos[i][1] / TILE, f * 100.0f);
            }
        }
    }
    /* An Interceptor's kill: a short, faint light, as from high in the air and far off; a little
       of it even when the kill is off the screen. */
    float now_f = (float)t0 + *(float *)(game() + GAME_FRAC);
    for (int i = 0; i < MAX_BURSTS && g_burst_flash > 0.0f; i++) {
        float age = (now_f - (float)g_burst[i].start) / 15.0f, dur = 0.9f;
        if (!g_burst[i].start || age < 0.0f || age > dur)
            continue;
        float e = age < 0.06f ? age / 0.06f : 1.0f - (age - 0.06f) / (dur - 0.06f);
        e = e * e;
        float f = fmaxf(0.4f, distance_factor(g_burst[i].x, g_burst[i].y));
        flash = fmaxf(flash, 32.0f * g_burst_flash * e * f);
        sat = fminf(sat, 1.0f - 0.1f * e * f);
        active = 1;
    }
    if (!active) {
        if (g_tone_ours) { /* back to the resting values the game leaves */
            *(float *)(tone + TONE_FLASH) = 0.0f;
            *(float *)(tone + TONE_SAT) = 1.0f;
            *(unsigned char *)(tone + TONE_ALPHA) = 0;
            *(unsigned char *)(tone + TONE_DIRTY) = 1;
            g_tone_ours = 0;
        }
        return;
    }
    g_tone_ours = !blasts;
    float w = 0.4f * (1.0f - sat);
    *(float *)(tone + TONE_FLASH) = flash;
    *(float *)(tone + TONE_SAT) = sat;
    *(unsigned char *)(tone + TONE_ALPHA) = (unsigned char)(int)(w * w * 255.0f);
    *(unsigned char *)(tone + TONE_DIRTY) = 1;
}

/* The nuke-fx underlay types, found by their textures (appended after the game's 20). */
static int g_ul_target = -1, g_ul_haze = -1, g_ul_glow = -1, g_ul_core = -1, g_ul_core_glow = -1;

static void find_underlays(void)
{
    if (g_ul_target >= 0 || g_ul_haze == -2)
        return;
    int n = *(int *)VA(VA_UL_TYPES_N);
    char *types = *(char **)VA(VA_UL_TYPES);
    for (int i = 20; types && i < n; i++) {
        char *ty = types + i * 0x14;
        if (*(float *)(ty + 8) <= 0.0f)
            continue;
        if ((unsigned char)ty[0xE] == 14)
            g_ul_target = i;
        else if ((unsigned char)ty[0xE] == 15)
            g_ul_haze = i;
        else if ((unsigned char)ty[0xE] == 16)
            g_ul_glow = i; /* the blade's glow, Shockwave Blend (adds light); older data has none */
        else if ((unsigned char)ty[0xE] == 17)
            g_ul_core = i; /* the disc in the middle (2026-11-08); older data has none */
        else if ((unsigned char)ty[0xE] == 18)
            g_ul_core_glow = i;
    }
    if (g_ul_target < 0 || g_ul_haze < 0) {
        logf_("nuke: the NukeTarget / NukeHaze underlays are not loaded (nuke-fx effects_graphics.xml missing?), "
              "no target sign and no haze");
        g_ul_target = -1;
        g_ul_haze = -2;
    } else
        logf_("nuke: underlays NukeTarget %d, NukeHaze %d, NukeTargetGlow %d, NukeCore %d, NukeCoreGlow %d",
              g_ul_target, g_ul_haze, g_ul_glow, g_ul_core, g_ul_core_glow);
}

struct underlay { float x, y, z; unsigned angle; int remaining, total; unsigned width; short a, b; };

static void underlay(int type, float x, float y, float deg, float width, float alpha)
{
    char **inst = *(char ***)VA(VA_UL_INSTANCES);
    if (!inst || !inst[type] || alpha <= 0.0f)
        return;
    int z = 0;
    void *terrain = *(void **)VA(VA_TERRAIN);
    if (terrain)
        ((fn4_t)VA(VA_FIND_Z))(terrain, (int)(INT_PTR)&z, (int)x, (int)y, 1);
    deg = fmodf(deg, 360.0f);
    if (deg < 0)
        deg += 360.0f;
    /* growth 1: alpha = 10 * remaining / total under a tenth, so remaining = alpha * total / 10 */
    struct underlay u = { x, y, (float)z + 6.0f, (unsigned)(deg / 360.0f * 4294967296.0f),
                          (int)(fminf(alpha, 1.0f) * 1000.0f) + 1, 10000, (unsigned)width, -1, -1 };
    ((fn1_t)VA(VA_OBJ_ARRAY_ADD))(inst[type], (int)(INT_PTR)&u);
}

static void clear_underlays(int type)
{
    char **inst = *(char ***)VA(VA_UL_INSTANCES);
    if (type >= 0 && inst && inst[type])
        *(int *)(inst[type] + 4) = 0;
}

static void draw_markers(void)
{
    if (!g_marker && !g_radiation)
        return;
    find_underlays();
    if (g_ul_target < 0)
        return;
    clear_underlays(g_ul_target);
    clear_underlays(g_ul_haze);
    clear_underlays(g_ul_glow);
    clear_underlays(g_ul_core);
    clear_underlays(g_ul_core_glow);
    float secs = (now() + *(float *)(game() + GAME_FRAC)) / 15.0f;
    if (g_marker) {
        char *objs = *(char **)VA(VA_OBJECTS);
        int n = *(int *)(objs + OBJS_AMMO_N);
        char **am = *(char ***)(objs + OBJS_AMMO);
        for (int i = 0; i < n && am; i++) {
            char *a = am[i];
            if (!nuke_type(a))
                continue;
            int tot = *(int *)(a + AM_TOTAL);
            float f = tot > 0 ? fminf(1.0f, (float)*(int *)(a + AM_CUR) / tot) : 1.0f;
            float r = (float)nuke_radius(a);
            float pulse = 0.65f + 0.35f * sinf(secs * 5.0f);
            float x = (float)*(int *)(a + AM_EX), y = (float)*(int *)(a + AM_EY);
            /* The sign in parts (the user, 2026-11-08): a disc in the middle that stays, and three
               blades pushed out from it, closing in as the nuke flies and meeting it as it lands.
               Every part is drawn at the sign's width w (each texture has the sign's centre in its
               middle); the closed sign reaches 0.45 w = 0.6 r. A blade lies along the texture's +u,
               which Underlays::draw lays along (sin a, -cos a) for the angle a, so a blade pointing
               at phi in the world takes a = phi + 90. */
            float w = 1.34f * r, out = 0.75f * r * (1.0f - f);
            for (int k = 0; k < 3; k++) {
                float phi = 90.0f + 120.0f * (float)k, rad = phi * 3.14159265f / 180.0f;
                float bx = x + out * cosf(rad), by = y + out * sinf(rad);
                underlay(g_ul_target, bx, by, phi + 90.0f, w, 0.95f);
                if (g_ul_glow >= 0) /* the glow pulses, so the sign seems to throb on any ground */
                    underlay(g_ul_glow, bx, by, phi + 90.0f, w, pulse);
            }
            if (g_ul_core >= 0)
                underlay(g_ul_core, x, y, 0.0f, w, 0.95f);
            if (g_ul_core_glow >= 0)
                underlay(g_ul_core_glow, x, y, 0.0f, w, pulse);
        }
    }
    if (g_radiation) {
        int t = now(), life = g_rad_secs * 15;
        for (int z = 0; z < MAX_ZONES; z++) {
            struct zone *zn = &g_zone[z];
            int age = t - zn->start;
            if (!zn->r || age < 0 || age > life)
                continue;
            float a = fminf(1.0f, age / 15.0f) * fminf(1.0f, (life - age) / (life * 0.25f)) * 0.85f;
            underlay(g_ul_haze, (float)zn->x, (float)zn->y, secs * 4.0f, 2.3f * zn->r, a);
            underlay(g_ul_haze, (float)zn->x, (float)zn->y, 140.0f - secs * 3.0f, 1.8f * zn->r, a * 0.8f);
        }
    }
}

static int c_nuke_draw(void)
{
    int r = ((fn0_t)VA(VA_NUKE_DRAW))(NULL);
    if (game()) {
        soften_flash();
        draw_markers();
    }
    return r;
}

/* NukeOut::draw_char_earth takes x, y and t (age / 110) in xmm1..xmm3 and keeps ebx, esi, edi. */
static void char_earth(float x, float y, float t)
{
    void *fn = (void *)VA(VA_CHAR_EARTH);
    __asm__ volatile("movss %0, %%xmm1\n\t"
                     "movss %1, %%xmm2\n\t"
                     "movss %2, %%xmm3\n\t"
                     "call *%3"
                     :
                     : "m"(x), "m"(y), "m"(t), "r"(fn)
                     : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory",
                       "cc");
}

/* After the game's scorch: blasts it has dropped from its list (after 110 + 0x140 frames) are
   drawn here until scorch_percent of that. */
static int c_nuke_underlays(void)
{
    int r = ((fn0_t)VA(VA_NUKE_UNDERLAYS))(NULL);
    airstrike_draw(); /* airstrike.c: the area strike's circle (the hook is shared) */
    int mode = *(int *)VA(VA_NUKE_MODE);
    if (!game() || g_scorch_pct <= 100 || (mode != 0 && mode != 7 && mode != 9))
        return r;
    int t = now(), frames = *(int *)VA(VA_NUKE_FRAMES);
    int game_end = frames + SPAN_GAME, end = game_end * g_scorch_pct / 100;
    for (int z = 0; z < MAX_ZONES; z++) {
        int age = t - g_zone[z].start;
        if (g_zone[z].r && age >= game_end && age < end)
            char_earth((float)g_zone[z].x, (float)g_zone[z].y, (float)age / (float)frames);
    }
    return r;
}

static int patch_call(UINT_PTR at_va, UINT_PTR target_va, void *to, const char *what)
{
    unsigned char *at = (unsigned char *)VA(at_va);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5) + *(INT32 *)(at + 1) != VA(target_va)) {
        logf_("nuke: signature mismatch at %s", what);
        return 0;
    }
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    return 1;
}

/* Called from install_hooks once the exe is known to be the supported version. The per-frame
   part runs from airdrop.c's Objects::process_all hook (installed when g_nuke_sim_enabled). */
void nukefx_install(void)
{
    if ((g_flash || g_marker || g_radiation || g_interceptors) &&
        patch_call(VA_CALL_NUKE_DRAW, VA_NUKE_DRAW, (void *)c_nuke_draw, "Scene::render (Nuke::draw)"))
        logf_("nuke flash: full within %.0f tiles of the screen centre, %.0f%% from %.0f tiles", g_full_tiles,
              g_far * 100.0f, g_zero_tiles);
    if (g_radiation || g_scorch_pct > 100) {
        if (!patch_call(VA_CALL_ADD_NUKE, VA_ADD_NUKE, (void *)rad_add_nuke, "Ammo::do_damage (add_nuke)"))
            g_radiation = 0, g_scorch_pct = 100;
    }
    /* the draw_underlays hook also draws airstrike.c's area strike circle */
    int under = (g_scorch_pct > 100 || airstrike_area_wanted()) &&
                patch_call(VA_CALL_UNDERLAYS, VA_NUKE_UNDERLAYS, (void *)c_nuke_underlays, "Scene::render (draw_underlays)");
    airstrike_area_install(under);
    if (g_scorch_pct > 100) {
        static const unsigned char sig[5] = { 0x05, 0x40, 0x01, 0x00, 0x00 };
        unsigned char *at = (unsigned char *)VA(VA_SCORCH_SPAN);
        if (under && memcmp(at, sig, 5) == 0) {
            DWORD old;
            VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
            *(INT32 *)(at + 1) = (110 + SPAN_GAME) * g_scorch_pct / 100 - 110;
            VirtualProtect(at, 5, old, &old);
            logf_("nuke: scorched earth stays %d%% as long", g_scorch_pct);
        } else {
            logf_("nuke: signature mismatch in draw_char_earth, the scorch fades as in the game");
            g_scorch_pct = 100;
        }
    }
    if (g_nuke_sim_enabled) {
        ((fn0_t)VA(VA_STR_CTOR))(g_keep);
        if (patch_call(VA_CALL_SCEN_WALK, VA_SCEN_WALK, (void *)c_scen_walk, "WalkDataGame::walk_data (saved games)"))
            logf_("nuke: Interceptors and radiation zones go into saved games");
    }
    logf_("nuke: interceptors %s (reach %d tiles, %d%% of the nuke's speed, airburst size %.1f, light %.0f%% of a "
          "nuke's), radiation %s (%d%% a second for %d s), target sign %s",
          g_interceptors ? "on" : "off", g_reach_tiles, g_speed_pct, g_burst_scale, g_burst_flash * 100.0f,
          g_radiation ? "on" : "off", g_rad_pct, g_rad_secs, g_marker ? "on" : "off");
}
