/*
 * City repair: a Large or Major City repairs the damaged buildings of its own city by itself, one
 * at a time, the most damaged first, and pays for it the way a Citizen does. A building waits
 * [gameplay] city_repair_wait seconds (45) after its last hit. Wonders within the city's radius
 * are repaired too. The city's panel has a button that turns it off and on (on by default).
 *
 * Which buildings: those whose city (BuildData +0x72, set by Build::find_city when they were
 * placed) is this city, plus any wonder (types 0x20E..0x21E, BuildData::is_wonder) within the
 * city's radius (LeaderData::get_radius, the circle buildings must stand in; distance as
 * vector_dist measures it). Not other cities: those heal by themselves (CITY_HEAL_RATE). A
 * building another city repairs this frame is left to that one.
 *
 * Speed and price are Unit::do_repair's, the Citizen's (data-center\ENGINE_NOTES.md, "City
 * repair"): time per hit point (1/256 frame) = construct_time * 512 / (max hits * ACCEL_CONSTRUCT)
 * times (helpers + 1), helpers being the building's count of repairers this frame (byte +0x64,
 * cleared by Wall::process); Koreans KOREAN_REPAIR less. Every hit point repaired costs
 * max hits / price of each resource, one unit at a time, from the treasury. A Large City
 * repairs as one Citizen, a Major City as two (the second as the game's second Citizen does,
 * at half speed). Without the resources it waits.
 *
 * The switch is the game's own build mask command (GroupOut::issue_buildmask, the Infinite
 * Queue's), so it reaches every machine and is part of the simulation: the mask CR_MASK has bit
 * 0x80 (Repeat Orders of the buildings that hold aircraft, never set or read on a city) and bit
 * 16, which the game never uses (the command carries a dword, the field is a word), so
 * WallData::valid_buildmask lets it through only for a Large or Major City. Group::action_buildmask
 * then sets bit 0x80 of the city's +0x60 (= repair off) when one selected city has it clear and
 * clears it otherwise. WallData::walk_data saves +0x60, so the setting is in saved games.
 *
 * The button: BuildOut::make_options (airdrop.c's hook) adds Repair/Build (option 3, its icon) to
 * the local player's Large and Major Cities. Options::exec sends option 3 to Options::do_repair,
 * which does nothing for a selection with buildings; here it sends the command instead. The
 * tooltip (Options::describe: ranks.c's hook, or ours when ranks are off) is replaced with this
 * feature's text, and panel.c draws the button darker while it is off.
 *
 * The last hit of every building is kept here (its damage grew, or the game marks it under
 * attack), from airdrop.c's Objects::process_all hook, the same on every machine. A new game
 * or a loaded one starts every building at "just hit", so nothing is repaired in the first
 * seconds. While a city repairs, small pliers float over the building, bobbing and squeezing in
 * a warm glow and throwing sparks (rankfx.c's map window redraw), for every player who sees it.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x184 + 4 * p = end of the building ids */
#define VA_OBJ_ARRAYS   0x00C0AB84u /* Object ** per player, stride 28 */
#define VA_GAME         0x00C061ECu /* Game *: +0x550 = the frame (15 a second) */
#define VA_GAMEC        0x00C061E8u /* +0x820: bits 0x42 = everything visible (replay, observer) */
#define VA_CONSTANTS    0x00C061F0u /* Constants * */
#define VA_LEADERS      0x00E3A390u /* LeaderData[8], 0x6EEC each */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player, +0x2A0 selection index */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each */
#define VA_OPTIONS      0x00C06204u /* Options *: +0x10 entries, +0x20 count */
#define VA_OPTIONS_ADD  0x0072C290u /* Options::add(code, x, y, type, disabled, 0) */
#define VA_ACTIVE_BUILD 0x0046FB40u /* ObjectData::is_active_build() */
#define VA_TRIBE_BONUS  0x006E1370u /* LeaderData::has_tribe_bonus(int), ret 4 */
#define VA_TYPE_AVAIL   0x006E33A0u /* LeaderData::type_avail(type, 1), ret 8 */
#define VA_CITY_RADIUS  0x006DB790u /* LeaderData::get_radius(city type), tiles, ret 4 */
#define VA_SCRATCH      0x00CB195Cu /* LeaderDataEncrypt::scratch */
#define VA_DO_REPAIR    0x0071CCB0u /* Options::do_repair(int), ret 4 */
#define VA_VALID_MASK   0x0063E2A0u /* WallData::valid_buildmask(int) const, ret 4 */
#define VA_ISSUE_MASK   0x00708820u /* GroupOut::issue_buildmask(mask, int) const, ret 8 */
#define VA_SOUND        0x0097F770u /* SoundGlobal::play(cat) const, ret 4 (ecx unused) */
#define VA_DESCRIBE     0x00724460u /* Options::describe(Option *, int, int, int), ret 0x10 */
#define VA_SCENE        0x00C0620Cu /* Scene *: +0xDC the tooltip's title, +0x104 its lines */
#define VA_LIST_ADD     0x00A29420u /* StringList::add(const String &, const String &, int, int, int) */
#define VA_LIST_CLEAR   0x00A298F0u /* StringList::clear() */
#define VA_STR_ASSIGN   0x00A1EEB0u /* String::operator=(const String &), ret 4 */
#define VA_STRING_CLOSE 0x00A1CF40u
#define VA_EMPTY_STRING 0x00EB437Cu
#define VA_BUILD_VT     0x00B42174u /* Build::`vftable' */

#define KEY          0x63637 /* x, y at +0x10, +0x14 xor this */
#define TILE         192
#define FIRST_BUILD  2000
#define MAX_B        4096    /* building ids 2000 .. 2000 + MAX_B - 1 per player */
#define LARGE_CITY   0x19F
#define MAJOR_CITY   0x1A0
#define WONDER_FIRST 0x20E
#define WONDER_END   0x21F
#define TRIBE_KOREAN 0x10
#define OPT_REPAIR   3       /* Repair/Build */
#define OPT_SIZE     0x14
#define CR_MASK      0x10080 /* bit 16: ours; bit 0x80: the stored "off" */
#define CR_OFF       0x80
#define SND_ON       0x3C    /* the Infinite Queue's on / off sounds */
#define SND_OFF      0x3D

#define OBJ_FLAGS    0x08 /* bit 0 slot in use, 0x20 a city */
#define OBJ_OWNER    0x09
#define OBJ_ID       0x0A
#define OBJ_TYPE     0x18
#define OBJ_DAMAGE   0x24
#define B_MASKS      0x60 /* word, build_masks */
#define B_HELPERS    0x64 /* byte, repairers this frame */
#define B_CITY       0x72 /* short, the city it belongs to */
#define VT_SEEN      0x48
#define VT_GET_COST  0x78  /* TypeData::get_cost(r, player, -1, -1, 0, 1, -1) */
#define VT_HITS_LEFT 0x114
#define VT_HITS      0x11C /* hits(0) = max hits */
#define VT_REPAIR    0x168 /* Build::repair_damage(amount, 0, 1) */
#define VT_UNDER_ATK 0x188
#define VT_CTIME     0x18C /* construct_time(1) */
#define C_ACCEL      0x22C /* ACCEL_CONSTRUCT */
#define C_KOR_FIRE   0x7DC /* KOREAN_BUILD_UNDER_FIRE */
#define C_KOR_REPAIR 0x7E0 /* KOREAN_REPAIR, percent less time */
#define L_TREASURY   0x6EB8
#define L_SPENT      0x468 /* 6 ints Unit::do_repair also lowers (not below 0) */

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn3_t)(void *self, int a, int b, int c);
typedef int(__attribute__((thiscall)) *fn6_t)(void *self, int a, int b, int c, int d, int e, int f);
typedef int(__attribute__((thiscall)) *fn7_t)(void *self, int a, int b, int c, int d, int e, int f, int g);
typedef void(__attribute__((thiscall)) *describe_t)(void *self, void *opt, int a, int b, int c);
typedef int(__attribute__((thiscall)) *list_add_t)(void *list, const void *a, const void *b, int c, int d, int e);

static int g_on = 1, g_wait_s = 45;
static void *g_orig_do_repair, *g_orig_valid, *g_orig_describe;

/* per player and building id - 2000 */
static int g_last_dmg[8][MAX_B], g_last_hit[8][MAX_B], g_done_at[8][MAX_B], g_claim[8][MAX_B];
static char *g_seen_obj[8][MAX_B];
/* per city (its building id - 2000): what it repairs, and why it does not */
static short g_target[8][MAX_B];
static unsigned char g_status[8][MAX_B]; /* 0 nothing to do, 1 repairing, 2 short of resources */
static int g_last_frame = -1, g_fresh;

enum { ST_IDLE, ST_WORKING, ST_POOR };

void cityrepair_load_config(const char *ini_path)
{
    g_on = GetPrivateProfileIntA("gameplay", "city_repair", 1, ini_path);
    g_wait_s = GetPrivateProfileIntA("gameplay", "city_repair_wait", 45, ini_path);
    if (g_wait_s < 0 || g_wait_s > 600)
        g_wait_s = 45;
}

int cityrepair_wanted(void) { return g_on; }

static int now(void)
{
    char *game = *(char **)VA(VA_GAME);
    return game ? *(int *)(game + 0x550) : 0;
}

static char *leader(int p) { return (char *)VA(VA_LEADERS) + p * 0x6EEC; }

static int local_player(void)
{
    char *con = *(char **)VA(VA_CONSOLE);
    return con ? *(int *)(con + 0x298) : -1;
}

/* a building of player p by id (also used for the selection's members), or NULL */
static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(VA(VA_OBJ_ARRAYS) + 28 * p);
    char *o = arr ? arr[id] : NULL;
    /* Build objects only: every real building (cities and wonders too); a safety check */
    return o && (o[OBJ_FLAGS] & 1) && *(UINT_PTR *)o == VA(VA_BUILD_VT) ? o : NULL;
}

static int build_end(int p)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    int n = objs ? *(int *)(objs + 0x184 + 4 * p) : 0;
    return n > FIRST_BUILD + MAX_B ? FIRST_BUILD + MAX_B : n;
}

static int type_of(char *o)
{
    char *t = *(char **)(o + OBJ_TYPE);
    return t ? *(int *)(t + 4) : -1;
}

static void **vt(char *o) { return *(void ***)o; }

static int active(char *o) { return ((fn0_t)VA(VA_ACTIVE_BUILD))(o) != 0; }

/* a finished Large or Major City: the ones that repair */
static int repairing_city(char *o)
{
    int t = o ? type_of(o) : -1;
    return (t == LARGE_CITY || t == MAJOR_CITY) && active(o);
}

static int is_off(char *city) { return (*(unsigned short *)(city + B_MASKS) & CR_OFF) != 0; }

static int max_hits(char *o) { return ((fn1_t)vt(o)[VT_HITS / 4])(o, 0); }

static int tile(int coord) { return (coord ^ KEY) / TILE; }

/* vector_dist 0x0046CFF0, the distance find_city_at measures a city's radius with */
static int dist_tiles(char *a, char *b)
{
    int dx = tile(*(int *)(a + 0x10)) - tile(*(int *)(b + 0x10));
    int dy = tile(*(int *)(a + 0x14)) - tile(*(int *)(b + 0x14));
    unsigned x = dx < 0 ? -dx : dx, y = dy < 0 ? -dy : dy, big = x > y ? x : y, small = x > y ? y : x;
    if (!big)
        return 0;
    if (small >= 60000)
        return (int)((small + big * 2) >> 1);
    return (int)(small * small / (big * 2) + big);
}

static int city_radius(int p, char *city)
{
    return ((fn1_t)VA(VA_CITY_RADIUS))(leader(p), type_of(city));
}

/* ---- the simulation --------------------------------------------------------------------- */

/* The building's last hit: its damage grew, or the game marks it under attack. */
static void track_hits(int p, int t)
{
    int end = build_end(p);
    for (int id = FIRST_BUILD; id < end; id++) {
        char *o = object(p, id);
        int i = id - FIRST_BUILD;
        if (!o) {
            g_seen_obj[p][i] = NULL;
            continue;
        }
        int dmg = *(int *)(o + OBJ_DAMAGE);
        if (o != g_seen_obj[p][i]) { /* a building new in this slot */
            g_seen_obj[p][i] = o;
            g_last_dmg[p][i] = dmg;
            g_last_hit[p][i] = g_fresh ? t : t - g_wait_s * 15;
            g_done_at[p][i] = -1000000;
            g_claim[p][i] = -1;
            continue;
        }
        if (dmg > g_last_dmg[p][i] || ((fn0_t)vt(o)[VT_UNDER_ATK / 4])(o))
            g_last_hit[p][i] = t;
        g_last_dmg[p][i] = dmg;
    }
}

static int is_wonder(char *o)
{
    int t = type_of(o);
    return t >= WONDER_FIRST && t < WONDER_END;
}

/* Can this city repair building b now? */
static int eligible(int p, char *city, char *b, int t)
{
    int id = *(short *)(b + OBJ_ID), i = id - FIRST_BUILD;
    if (b == city || i < 0 || i >= MAX_B || (b[OBJ_FLAGS] & 0x20) || *(int *)(b + OBJ_DAMAGE) <= 0 || !active(b))
        return 0;
    if (t - g_last_hit[p][i] < g_wait_s * 15 || g_claim[p][i] == t)
        return 0;
    if (*(short *)(b + B_CITY) == *(short *)(city + B_CITY))
        return 1;
    return is_wonder(b) && dist_tiles(city, b) <= city_radius(p, city);
}

static char *pick(int p, char *city, int t)
{
    char *best = NULL;
    long long best_q = 0;
    int end = build_end(p);
    for (int id = FIRST_BUILD; id < end; id++) {
        char *b = object(p, id);
        if (!b || !eligible(p, city, b, t))
            continue;
        int full = max_hits(b);
        if (full <= 0)
            continue;
        long long q = (long long)*(int *)(b + OBJ_DAMAGE) * 1000000 / full; /* share lost */
        if (q > best_q) {
            best_q = q;
            best = b;
        }
    }
    return best;
}

/* One Citizen's repair of b for this frame, as Unit::do_repair does it (0x005EE6B0..0x005EEA4F).
   1 = repaired (or nothing due this frame), 0 = the owner is short of a resource. */
static int repair_once(int p, char *b, int t)
{
    char *c = *(char **)VA(VA_CONSTANTS), *l = leader(p);
    int dmg = *(int *)(b + OBJ_DAMAGE);
    if (!c || dmg <= 0)
        return 1;
    int full = max_hits(b);
    unsigned ctime = (unsigned)((fn1_t)vt(b)[VT_CTIME / 4])(b, 1);
    unsigned per = (unsigned)(full * *(int *)(c + C_ACCEL));
    if (!per)
        return 1;
    int x = (int)((ctime << 9) / per);
    x *= (unsigned char)b[B_HELPERS] + 1;
    int korean = ((fn1_t)VA(VA_TRIBE_BONUS))(l, TRIBE_KOREAN);
    if (korean && *(int *)(c + C_KOR_REPAIR))
        x = (100 - *(int *)(c + C_KOR_REPAIR)) * x / 100;
    if (!korean || !*(int *)(c + C_KOR_FIRE)) {
        if (((fn0_t)vt(b)[VT_UNDER_ATK / 4])(b))
            x <<= 2;
        if (b[B_MASKS] & 0x10)
            x <<= 2;
    }
    b[B_HELPERS]++;
    int amount;
    if (x <= 0)
        amount = dmg;
    else {
        int f = t << 8;
        amount = f / x - (f - 0x100) / x;
    }
    if (!amount)
        return 1;
    int left = ((fn0_t)vt(b)[VT_HITS_LEFT / 4])(b), need[6] = { 0 }, avail[6] = { 0 };
    char *type = *(char **)(b + OBJ_TYPE);
    int *treasury = *(int **)(l + L_TREASURY);
    for (int r = 0; r < 6; r++) {
        avail[r] = ((fn2_t)VA(VA_TYPE_AVAIL))(l, r, 1) != 0;
        if (!avail[r])
            continue;
        int cost = ((fn7_t)(*(void ***)type)[VT_GET_COST / 4])(type, r, p, -1, -1, 0, 1, -1);
        if (!cost)
            continue;
        float hp_per = (float)max_hits(b) / (float)cost;
        if ((int)((float)left / hp_per) < (int)((float)(amount + left) / hp_per))
            need[r] = 1;
        if ((treasury[r] ^ 0x8221) < need[r])
            return 0;
    }
    for (int r = 0; r < 6; r++) {
        if (!avail[r])
            continue;
        int v = (treasury[r] ^ 0x8221) - need[r];
        *(int *)VA(VA_SCRATCH) = v;
        treasury[r] = v ^ 0x8221;
        int *spent = (int *)(l + L_SPENT) + r;
        *spent = *spent - need[r] < 0 ? 0 : *spent - need[r];
    }
    ((fn3_t)vt(b)[VT_REPAIR / 4])(b, amount, 0, 1);
    return 1;
}

static void city_work(int p, char *city, int t)
{
    int ci = *(short *)(city + OBJ_ID) - FIRST_BUILD;
    if (ci < 0 || ci >= MAX_B)
        return;
    if (is_off(city)) {
        g_status[p][ci] = ST_IDLE;
        g_target[p][ci] = -1;
        return;
    }
    char *b = g_target[p][ci] >= FIRST_BUILD ? object(p, g_target[p][ci]) : NULL;
    if (!b || !eligible(p, city, b, t)) /* the last one is done (or hit again): the next */
        b = pick(p, city, t);
    if (!b) {
        g_status[p][ci] = ST_IDLE;
        g_target[p][ci] = -1;
        return;
    }
    int bi = *(short *)(b + OBJ_ID) - FIRST_BUILD;
    g_target[p][ci] = *(short *)(b + OBJ_ID);
    g_claim[p][bi] = t;
    int hands = type_of(city) == MAJOR_CITY ? 2 : 1, ok = 1;
    for (int k = 0; k < hands && ok && *(int *)(b + OBJ_DAMAGE) > 0; k++)
        ok = repair_once(p, b, t);
    g_status[p][ci] = ok ? ST_WORKING : ST_POOR;
    if (ok)
        g_done_at[p][bi] = t;
}

/* From airdrop.c's Objects::process_all hook, every simulation frame. */
void cityrepair_frame(void)
{
    if (!g_on)
        return;
    int t = now();
    if (t < g_last_frame || t - g_last_frame > 15) { /* a new game or a loaded one */
        memset(g_seen_obj, 0, sizeof g_seen_obj);
        memset(g_target, 0xFF, sizeof g_target);
        memset(g_status, 0, sizeof g_status);
        g_fresh = 1;
    }
    g_last_frame = t;
    for (int p = 0; p < 8; p++)
        if (*(int *)leader(p) & 1)
            track_hits(p, t);
    g_fresh = 0;
    for (int p = 0; p < 8; p++) {
        if (!(*(int *)leader(p) & 1))
            continue;
        int end = build_end(p);
        for (int id = FIRST_BUILD; id < end; id++) {
            char *o = object(p, id);
            if (o && repairing_city(o))
                city_work(p, o, t);
        }
    }
}

/* ---- the button ------------------------------------------------------------------------- */

static int taken(char *opts, int col, int row)
{
    char *e = *(char **)(opts + 0x10);
    int n = *(int *)(opts + 0x20);
    for (int i = 0; i < n; i++, e += OPT_SIZE)
        if (e[0x10] == col && e[0x11] == row)
            return 1;
    return 0;
}

/* From airdrop.c's BuildOut::make_options hook. */
void cityrepair_make_options(char *self)
{
    if (!g_on || (unsigned char)self[OBJ_OWNER] != local_player() || !repairing_city(self))
        return;
    char *opts = *(char **)VA(VA_OPTIONS);
    if (!opts)
        return;
    static const signed char cells[][2] = { { 4, 2 }, { 3, 2 }, { 4, 1 }, { 3, 1 }, { 2, 3 }, { 2, 2 }, { 4, 0 } };
    for (int i = 0; i < (int)(sizeof cells / sizeof cells[0]); i++)
        if (!taken(opts, cells[i][0], cells[i][1])) {
            ((fn6_t)VA(VA_OPTIONS_ADD))(opts, OPT_REPAIR, cells[i][0], cells[i][1], -1, 0, 0);
            static int logged;
            if (!logged++)
                logf_("city repair: button on the city panel at (%d, %d)", cells[i][0], cells[i][1]);
            return;
        }
}

static char *local_selection(void)
{
    char *con = *(char **)VA(VA_CONSOLE), *sel = *(char **)VA(VA_SELECTIONS);
    return con && sel ? sel + *(int *)(con + 0x2A0) * 0x9F0 : NULL;
}

/* The local player's first selected Large or Major City; *any_on = one of them repairs. */
static char *selected_city(int *any_on)
{
    char *g = local_selection();
    int p = local_player();
    if (any_on)
        *any_on = 0;
    if (!g || p < 0 || p > 7 || (unsigned char)g[0x4A] != p)
        return NULL;
    char *first = NULL;
    int n = *(int *)(g + 0xC);
    for (int i = 0; i < n; i++) {
        char *o = object(p, *(short *)(g + 0x8CC + 2 * i));
        if (!o || !repairing_city(o))
            continue;
        if (!first)
            first = o;
        if (any_on && !is_off(o))
            *any_on = 1;
    }
    return first;
}

/* Options::do_repair(int): Repair/Build. With a city selected, the switch; otherwise the game's
   (Citizens' repair cursor). */
__attribute__((used)) int c_cr_do_repair(void *self, int *a)
{
    int any_on;
    char *g = local_selection();
    if (g_on && g && g[0x49] && selected_city(&any_on)) {
        ((fn2_t)VA(VA_ISSUE_MASK))(g, CR_MASK, 0);
        ((fn1_t)VA(VA_SOUND))(NULL, any_on ? SND_OFF : SND_ON);
        return 0;
    }
    return ((fn1_t)g_orig_do_repair)(self, a[0]);
}

/* WallData::valid_buildmask(int): our mask is valid for a Large or Major City only. */
__attribute__((used)) int c_cr_valid(char *self, int *a)
{
    if (a[0] & 0x10000)
        return g_on && repairing_city(self);
    return ((fn1_t)g_orig_valid)(self, a[0]);
}

/* For panel.c: this button is the switch of cities that are all off (drawn darker). */
int cityrepair_button_off(char *btn)
{
    if (!g_on)
        return 0;
    int idx = *(int *)(btn + 0x330);
    char *opts = *(char **)VA(VA_OPTIONS);
    if (idx < 0 || !opts || idx >= *(int *)(opts + 0x20))
        return 0;
    if (*(int *)(*(char **)(opts + 0x10) + idx * OPT_SIZE) != OPT_REPAIR)
        return 0;
    int any_on;
    return selected_city(&any_on) && !any_on;
}

/* ---- the tooltip ------------------------------------------------------------------------ */

static void tip_line(char *scene, const char *text)
{
    unsigned char str[64] = { 0 };
    lang_string(str, text);
    ((list_add_t)VA(VA_LIST_ADD))(scene + 0x104, str, (void *)VA(VA_EMPTY_STRING), 0, 0x5E, 0);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
}

/* After Options::describe: Repair/Build on a city panel gets this feature's text. 1 = done. */
int cityrepair_describe(const int *opt)
{
    char *scene = *(char **)VA(VA_SCENE);
    int any_on;
    char *city = g_on && opt && opt[0] == OPT_REPAIR && scene ? selected_city(&any_on) : NULL;
    char *g = local_selection();
    if (!city || !g || !g[0x49])
        return 0;
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text(any_on ? "City Repair: On" : "City Repair: Off"));
    ((fn1_t)VA(VA_STR_ASSIGN))(scene + 0xDC, (int)(UINT_PTR)str);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    ((fn0_t)VA(VA_LIST_CLEAR))(scene + 0x104);
    char line[512];
    tip_line(scene, lang_text("The city repairs its damaged buildings by itself, one at a time, the most damaged "
                              "first, and pays for it as a Citizen does."));
    tip_line(scene, lang_text("Large City: as fast as 1 Citizen. Major City: as fast as 2 Citizens."));
    snprintf(line, sizeof line,
             lang_text("A building waits %d seconds after its last hit. Wonders within the city's radius are "
                       "repaired too."),
             g_wait_s);
    tip_line(scene, line);
    int p = (unsigned char)city[OBJ_OWNER], ci = *(short *)(city + OBJ_ID) - FIRST_BUILD;
    if (any_on && ci >= 0 && ci < MAX_B) {
        char *b = g_status[p][ci] != ST_IDLE ? object(p, g_target[p][ci]) : NULL;
        int full = b ? max_hits(b) : 0;
        if (b && full > 0 && g_status[p][ci] == ST_POOR)
            tip_line(scene, lang_text("Waiting: not enough resources to repair."));
        else if (b && full > 0) {
            int pct = (full - *(int *)(b + OBJ_DAMAGE)) * 100 / full;
            snprintf(line, sizeof line, lang_text("Repairing a building: %d%% of its hit points."), pct);
            tip_line(scene, line);
        } else
            tip_line(scene, lang_text("Nothing to repair right now."));
    }
    tip_line(scene, lang_text(any_on ? "Click to turn it off." : "Click to turn it on."));
    return 1;
}

/* our own Options::describe hook, when ranks.c has none */
__attribute__((used)) int c_cr_describe(void *self, int *a)
{
    ((describe_t)g_orig_describe)(self, (void *)a[0], a[1], a[2], a[3]);
    cityrepair_describe((const int *)a[0]);
    return 0;
}

/* ---- the pliers over the map ------------------------------------------------------------ */

static int seen_by(char *o, int local)
{
    if ((unsigned char)o[OBJ_OWNER] == local)
        return 1;
    const char *game = *(const char **)VA(VA_GAMEC);
    if (game && (*(const unsigned char *)(game + 0x820) & 0x42))
        return 1;
    return ((fn2_t)vt(o)[VT_SEEN / 4])(o, local, 0) != 0;
}

static void tinted(const void *tex, float cx, float cy, float w, float h, unsigned rgb, float alpha)
{
    if (!tex || alpha <= 0.0f || w < 0.5f)
        return;
    int a = (int)(alpha * 255.0f + 0.5f);
    rf_add(tex, cx - w * 0.5f, cy - h * 0.5f, w, h, (unsigned)(a > 255 ? 255 : a) << 24 | rgb);
}

/* From rankfx.c's map window redraw: over every building a city repaired just now, pliers that
   bob and squeeze in a warm glow, throwing small sparks from their jaws. */
void cityrepair_draw_overhead(void)
{
    if (!g_on)
        return;
    int local = local_player(), t = now();
    const void *tex = local >= 0 ? qol_iface_tex("repair_pliers") : NULL;
    if (!tex)
        return;
    const void *spark = qol_iface_tex("rank_spark"), *glow = qol_iface_tex("rank_glow");
    float s = 26.0f * rankfx_zoom();
    s = s < 14.0f ? 14.0f : s > 30.0f ? 30.0f : s;
    float secs = (float)(GetTickCount() % 600000) / 1000.0f;
    for (int p = 0; p < 8; p++) {
        int end = build_end(p);
        for (int id = FIRST_BUILD; id < end; id++) {
            int i = id - FIRST_BUILD;
            if (t - g_done_at[p][i] > 8)
                continue;
            char *b = object(p, id);
            float x, y;
            if (!b || b != g_seen_obj[p][i] || !seen_by(b, local) || !rankfx_anchor(b, &x, &y))
                continue;
            float phase = (float)((unsigned)id * 2654435761u >> 22) / 1024.0f; /* 0..1, per building */
            float k = secs + phase * 3.0f;
            /* the work: a quick squeeze about three times a second, a slow bob and sway */
            float squeeze = sinf(k * 6.2831853f * 2.8f);
            squeeze = squeeze > 0.0f ? squeeze * squeeze : 0.0f; /* short grips, rests between */
            float bob = sinf(k * 6.2831853f * 0.9f) * s * 0.08f, sway = sinf(k * 6.2831853f * 0.6f) * s * 0.06f;
            float cx = x + sway, cy = y - s * 1.1f + bob;
            float w = s * (1.0f - 0.14f * squeeze), h = s * (1.0f + 0.06f * squeeze);
            tinted(glow, cx, cy, s * (1.7f + 0.25f * squeeze), s * (1.7f + 0.25f * squeeze), 0xFFC060,
                   0.30f + 0.20f * squeeze);
            tinted(tex, cx, cy, w, h, 0xFFFFFF, 1.0f);
            /* sparks from the jaws (top right of the picture): out, then down, fading */
            float jx = cx + s * 0.22f, jy = cy - s * 0.30f;
            for (int n = 0; n < 5; n++) {
                float f = k * 1.7f + (float)n / 5.0f;
                int round_no = (int)floorf(f);
                f -= floorf(f); /* 0..1 through this spark's flight */
                unsigned seed = (unsigned)id * 2246822519u + (unsigned)(round_no * 5 + n) * 3266489917u;
                float dir = (float)(seed >> 22) / 1024.0f * 3.1415926f - 0.35f; /* up, left or right */
                float speed = s * (0.9f + (float)((seed >> 12) & 255) / 255.0f * 0.8f);
                float sx = jx + cosf(dir) * speed * f, sy = jy - sinf(dir) * speed * f + s * 1.4f * f * f;
                float size = s * (0.70f - 0.40f * f), alpha = f < 0.1f ? f / 0.1f : 1.0f - f;
                tinted(spark, sx, sy, size, size, n & 1 ? 0xFFE070 : 0xFF9830, alpha);
            }
        }
    }
}

/* ---- hooks ------------------------------------------------------------------------------ */

#define CR_STUB(name, cfn, ret)                                                              \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    lea eax, [ebx+36]\n"                                                               \
    "    push eax\n"                                                                        \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

__asm__(".intel_syntax noprefix\n"
        CR_STUB("hk_cr_do_repair", "c_cr_do_repair", "ret 4")
        CR_STUB("hk_cr_valid", "c_cr_valid", "ret 4")
        CR_STUB("hk_cr_describe", "c_cr_describe", "ret 16")
        ".att_syntax prefix\n");
void hk_cr_do_repair(void);
void hk_cr_valid(void);
void hk_cr_describe(void);

/* After ranks_install: Options::describe is ranks.c's when it hooked it (it calls
   cityrepair_describe), else ours. */
void cityrepair_install(void)
{
    if (!g_on)
        return;
    static const unsigned char repair_sig[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    static const unsigned char valid_sig[7] = { 0x55, 0x8B, 0xEC, 0xF6, 0x45, 0x08, 0x40 };
    static const unsigned char desc_sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    unsigned char *rep = (unsigned char *)VA(VA_DO_REPAIR), *val = (unsigned char *)VA(VA_VALID_MASK),
                  *desc = (unsigned char *)VA(VA_DESCRIBE);
    if (memcmp(rep, repair_sig, sizeof repair_sig) || memcmp(val, valid_sig, sizeof valid_sig)) {
        logf_("city repair: signature mismatch at Options::do_repair or WallData::valid_buildmask, turned off");
        g_on = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        g_on = 0;
        return;
    }
    g_orig_do_repair = hook_trampoline(rep, 9, mem);
    hook_jmp5(rep, hk_cr_do_repair);
    g_orig_valid = hook_trampoline(val, 7, mem + 32);
    hook_jmp5(val, hk_cr_valid);
    const char *tips = "ranks.c's tooltip hook";
    if (memcmp(desc, desc_sig, sizeof desc_sig) == 0) {
        g_orig_describe = hook_trampoline(desc, 5, mem + 64);
        hook_jmp5(desc, hk_cr_describe);
        tips = "its own tooltip hook";
    } else if (desc[0] != 0xE9)
        tips = "no tooltip (Options::describe not recognised)";
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("city repair: on (Large and Major Cities, %d s after the last hit, %s)", g_wait_s, tips);
}
