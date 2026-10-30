/*
 * Data Center (data-center mod): the end-game building and its three Expansions, small 1x1
 * buildings that each add a bonus for as long as they stand:
 *
 *   Population Expansion   +7 population limit      (goes past the game's 200)
 *   Commerce Expansion     +25 resource-rate cap    (goes past the hard 999)
 *   Efficiency Expansion   +3% to all income
 *
 * Any number can be built and the bonuses add up; a destroyed (or captured) Expansion takes its
 * bonus with it. The game cannot have new building types, so the Data Center took the
 * Washington Monument's slot and the Expansions the slots of three scenario-only buildings
 * (DC_EXP_* in ron.h). The data files (data-center\tools\build_data.py) give them their rules,
 * art and names. Here:
 *  - the bonus of a player is (Expansions of that kind the player owns, finished) x the step in
 *    data_center.ini. The game keeps that count itself (word [leader + 0x5222 + 2 * type]), so it
 *    is in saved games and drops when one is destroyed or captured.
 *  - Leader::calc_pop_cap: the population bonus is added after the game's clamp to the pop limit.
 *    calc_resource_caps (called every gather) recomputes the pop cap when a player's count of
 *    Population Expansions has changed, so a lost one is felt at once.
 *  - Leader::calc_resource_caps: the commerce bonus is added after the clamp to 999, then the cap
 *    is scaled by the efficiency bonus. The game's other fixed 999s (the cap shown at the top,
 *    gather tooltips, the Dutch interest path of do_gather) are lifted (lift_999).
 *  - Leader::calc_gather: freshly computed rates are scaled by the efficiency bonus. With the cap
 *    scaled too, the real income grows by exactly that much.
 *  - LeaderData::type_eligible: the Data Center needs at least one of the four last Library
 *    technologies (Missile Shield, World Government, Global Prosperity, Artificial
 *    Intelligence); PREQ in the data can only ask for all of them. A player who owns a Data
 *    Center, built or being built, cannot start another. Captured ones count too. The
 *    Expansions need a finished Data Center. Until then each is shown, disabled (padlock).
 *  - LeaderData::type_eligible also keeps the AI from building the Data Center early: a computer
 *    player is offered it only once its Knowledge and Wealth income both reach ai_min_income
 *    (data_center.ini). The AI picks buildings through type_avail, which asks type_eligible.
 *  - The Expansions are ordered at the Data Center, not by Citizens, and built by its technician
 *    (unit 341 of unitrules.xml, a copy of the Citizen): see "the technician" below. The hooks
 *    are entrench.c's and airdrop.c's, which call in here.
 * Everything runs in the simulation on every machine from the same state, so it stays in sync in
 * multiplayer (every player needs the mod).
 *
 * Enabled when data_center.ini is in the game folder (installed with the data files).
 * Engine notes: data-center\ENGINE_NOTES.md.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VA_TYPE_ELIGIBLE 0x006DBD10u /* LeaderData::type_eligible(TypeIndex, int), ret 8 */
#define VA_CALC_POP_CAP  0x006DC490u /* Leader::calc_pop_cap(), returns [this+0x7e4] */
#define VA_CALC_CAPS     0x006CE900u /* Leader::calc_resource_caps() */
#define VA_CALC_GATHER   0x006CEEE0u /* Leader::calc_gather(int *rates), ret 4 */
#define VA_LEADERS_INIT  0x006ED850u /* Leaders::init(), returns int */
#define VA_LEADERS       0x00E3A390u /* Leader[8], 0x6EEC each */
#define VA_HAS_TECH      0x006E0C80u /* LeaderData::has_tech(type) */

#define LEADER_SIZE    0x6EEC
#define LD_PLAYER      0x008  /* int */
#define LD_GATHER_TIME 0x7AC  /* game time of the last full calc_gather */
#define LD_POP_CAP     0x7E4
#define LD_FLAGS       0x000  /* bit 2: human (LeaderData::is_human) */
#define LD_SECURE      0x6EB8 /* encrypted values */
#define SEC_CAP        0x30   /* + 4 * resource: (cap << 4) ^ CAP_KEY */
#define CAP_KEY        0x1281
#define SEC_INCOME     0x64   /* + 4 * resource: (rate << 4) ^ INCOME_KEY (LeaderData::resources_get) */
#define INCOME_KEY     0x872
#define RES_WEALTH     2      /* resource order: food, timber, wealth, knowledge, metal, oil */
#define RES_KNOWLEDGE  3
#define DATA_CENTER    0x1C1  /* building type: Washington Monument, building 35 of buildingrules.xml */
#define LD_BUILT       0x5222 /* word per type: buildings owned */
#define LD_BUILDING    0x5A22 /* word per type: being built or queued (Leader::track_queued) */
#define NUM_RES        6

/* the last technology of each Library line (techs 35-38 of techrules.xml, techs start at 0x220):
   Missile Shield, World Government, Global Prosperity, Artificial Intelligence */
static const int k_final_tech[4] = { 0x243, 0x244, 0x245, 0x246 };

#define KINDS 3
static const int k_exp_type[KINDS] = { DC_EXP_POPULATION, DC_EXP_COMMERCE, DC_EXP_EFFICIENCY };

/* bonus of one Expansion (efficiency in tenths of a percent) */
static int g_step[KINDS] = { 10, 30, 30 };
static int g_ai_min_income = 400;

/* ---- city districts (2026-10-29: the user replaced the Data Center with them) ----------------
 * The three Expansions became 2x2 districts, built from the panel of any city of the player (the
 * buttons and the build command were the Data Center's), on a plot next to one of the player's
 * cities or districts with at most g_gap empty tiles between them. The Data Center is no longer
 * offered. They unlock once every Library technology up to the game's last Age is researched (the
 * four last ones excepted): Library line L (0..6) has its level-n technology from Age n (Ancient =
 * 0), and the game's last Age is the one whose next Age the game will not offer
 * (type_eligible 0) - the setup's "last Age", or the Information Age. With need_last_age = 0 the
 * current Age's technologies are enough. */
#define CITY_FIRST 0x19E /* Small, Large, Major City */
#define CITY_LAST  0x1A0
#define AGE_FIRST  0x220 /* Classical Age .. Information Age: 0x220 .. 0x226 */
#define AGES       7
static const int k_lib_line[4] = { 0x227, 0x22E, 0x235, 0x23C }; /* science, commerce, civic, military: 7 each */
#define CITY_HALF     (192 * 7 / 2) /* a city's plot, Coord from its centre to a side (7x7) */
#define DISTRICT_HALF 192           /* a district's (2x2) */
static int g_gap = 2;
static int g_need_last_age = 1;
static int g_unlock_seen[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

static int is_city(int type) { return type >= CITY_FIRST && type <= CITY_LAST; }

int g_dc_enabled;
static int g_pop_seen[8]; /* Population Expansions when the pop cap was last recomputed */

void *g_orig_type_eligible, *g_orig_calc_pop_cap, *g_orig_calc_caps, *g_orig_calc_gather, *g_orig_leaders_init;

typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn0_t)(void *self);

static int owned(char *ld, int type)
{
    return *(unsigned short *)(ld + LD_BUILT + 2 * type);
}

static int bonus(char *ld, int k)
{
    return owned(ld, k_exp_type[k]) * g_step[k];
}

static int income(char *ld, int r)
{
    int *v = (int *)(*(char **)(ld + LD_SECURE) + SEC_INCOME);
    return (v[r] ^ INCOME_KEY) >> 4;
}

static int has_final_tech(char *ld)
{
    for (int i = 0; i < 4; i++)
        if (((fn1_t)VA(VA_HAS_TECH))(ld, k_final_tech[i]))
            return 1;
    return 0;
}

static int is_expansion(int type)
{
    return type == DC_EXP_POPULATION || type == DC_EXP_COMMERCE || type == DC_EXP_EFFICIENCY;
}

/* The districts' unlock: every Library technology up to the game's last Age (see above). */
static int districts_unlocked(char *ld, int a)
{
    int age = 0;
    while (age < AGES && ((fn1_t)VA(VA_HAS_TECH))(ld, AGE_FIRST + age))
        age++;
    int last = age == AGES;
    int next = last ? 0 : ((fn2_t)g_orig_type_eligible)(ld, AGE_FIRST + age, a);
    if (!last && !next)
        last = 1; /* the game will not offer the next Age: this is the game's last */
    int missing = 0;
    for (int l = 0; l < 4; l++)
        for (int n = 0; n <= age && n < 7; n++)
            if (!((fn1_t)VA(VA_HAS_TECH))(ld, k_lib_line[l] + n))
                missing++;
    int ok = !missing && (last || !g_need_last_age);
    int p = *(int *)(ld + LD_PLAYER);
    if (p >= 0 && p < 8 && g_unlock_seen[p] != ok * 1000 + missing * 10 + last) {
        g_unlock_seen[p] = ok * 1000 + missing * 10 + last;
        logf_("districts: player %d, Age %d, next Age eligible %d (last Age %s), Library technologies missing %d: %s", p,
              age, next, last ? "yes" : "no", missing, ok ? "UNLOCKED" : "locked");
    }
    return ok;
}

__attribute__((used)) int c_type_eligible(char *self, int type, int a)
{
    if (nonukes_type_blocked(type)) /* nonukes.c: No Nukes ticked in the game setup */
        return 0;
    if (arm_type_blocked(type)) /* armageddon.c: the clock is out */
        return ((fn2_t)g_orig_type_eligible)(self, type, a) ? 1 : 0;
    if (!g_dc_enabled)
        return ((fn2_t)g_orig_type_eligible)(self, type, a);
    if (type == DATA_CENTER)
        return 0; /* replaced by the districts */
    int r = ((fn2_t)g_orig_type_eligible)(self, type, a);
    if (r && is_expansion(type) && !districts_unlocked(self, a))
        return 1; /* shown, disabled (padlock) until the Library is done */
    return r;
}

__attribute__((used)) int c_calc_pop_cap(char *self)
{
    int r = ((fn0_t)g_orig_calc_pop_cap)(self);
    int p = *(int *)(self + LD_PLAYER);
    if (p < 0 || p > 7)
        return r;
    g_pop_seen[p] = owned(self, DC_EXP_POPULATION);
    *(int *)(self + LD_POP_CAP) += g_pop_seen[p] * g_step[0];
    return *(int *)(self + LD_POP_CAP);
}

__attribute__((used)) void c_calc_caps(char *self)
{
    ((fn0_t)g_orig_calc_caps)(self);
    int p = *(int *)(self + LD_PLAYER);
    if (p < 0 || p > 7)
        return;
    if (owned(self, DC_EXP_POPULATION) != g_pop_seen[p]) /* through the hook, so the new bonus is in */
        ((fn0_t)VA(VA_CALC_POP_CAP))(self);
    int add = bonus(self, 1), eff = bonus(self, 2);
    if (!add && !eff)
        return;
    int *caps = (int *)(*(char **)(self + LD_SECURE) + SEC_CAP);
    for (int r = 0; r < NUM_RES; r++) {
        long long cap = ((caps[r] ^ CAP_KEY) >> 4) + add;
        cap = cap * (1000 + eff) / 1000;
        caps[r] = (int)(cap << 4) ^ CAP_KEY;
    }
}

__attribute__((used)) void c_calc_gather(char *self, int *rates)
{
    int before = *(int *)(self + LD_GATHER_TIME);
    ((fn1_t)g_orig_calc_gather)(self, (int)rates);
    int p = *(int *)(self + LD_PLAYER);
    if (p < 0 || p > 7 || *(int *)(self + LD_GATHER_TIME) == before)
        return; /* not recomputed this time: the rates are last time's, already scaled */
    int eff = bonus(self, 2);
    if (eff)
        for (int r = 0; r < NUM_RES; r++)
            rates[r] = (int)((long long)rates[r] * (1000 + eff) / 1000);
}

/* ---- the technician ------------------------------------------------------------------------
 *
 * The Data Center's panel has the game's own build buttons for the three Expansions (the build
 * cursor, the ghost and the price come with them); the Citizens' build menu has none. A build
 * button needs Citizens in the selection, which a selected Data Center answers for, and the build
 * command (Group::action_build, on every machine) lays the site here and pays, as the game would,
 * without sending anyone. Every few frames (Objects::process_all) each player's technician is
 * looked after: while the player has Expansion sites, one comes out of the Data Center nearest to
 * them (Build::train, as a trained unit) and builds them one after the other with the game's own
 * build order, so it walks and works like a Citizen. Then it walks back to a Data Center and
 * disappears. Killed, the next one comes out RESPAWN later. It cannot be selected. Saved games
 * need nothing: the technician is found again by its type. */

#define VA_OBJECTS      0x00C0618Cu /* Objects *: per player +0x14 + 0x1c*p = object pointers by id */
#define VA_BUILDTYPES   0x00C061F4u /* PtrArray<BuildType> *: +0x10 = BuildType * by TypeIndex */
#define VA_OPTIONS      0x00C06204u /* Options * */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 = local player */
#define VA_GAME         0x00C061ECu /* Game *: +0x550 = game time (1/15 s) */
#define VA_OPTIONS_ADD  0x0072C290u /* Options::add(code, x, y, type, disabled, 0) */
#define VA_TYPE_AVAIL   0x006E33A0u /* LeaderData::type_avail(type, 1): 4 = available */
#define VA_SNAP_CENTER  0x00636190u /* BuildTypeData::snap_center(x, y, &x, &y, player) */
#define VA_BLOCKED_SITE 0x00636A50u /* BuildTypeData::blocked_site(x, y, player, -1, 0): 0 = fine */
#define VA_FIND_CITY_AT 0x0065B870u /* ObjectsData::find_city_at(x, y, player, -1, 0) */
#define VA_INIT_BUILD   0x0065D190u /* Objects::init_build(player, type, x, y, 0, -1): the site's id */
#define VA_TRAIN        0x0062F9B0u /* Build::train(type): the unit made at the building, its id */
#define VA_ADD_BUILD    0x005E5210u /* Unit::add_build_order(building id, player, QueuePos, int) */
#define VA_GO_TO        0x005F7A50u /* Unit::go_to(x, y, 2, 0, 0x300) */
#define VA_DISBAND      0x006455C0u /* Object::disband(0) */
#define VA_IS_IDLE      0x0046FA40u /* UnitData::is_idle() */
#define VA_ACTIVE_BUILD 0x0046FB40u /* ObjectData::is_active_build(): finished, not a site */
#define BT_CAN_PAY      0x84        /* BuildType, virtual: can_pay(player, city, -1, 1) */
#define BT_PAY          0xD4        /* BuildType, virtual: pay(player, -1, city, 1, 0) */
#define OBJ_FLAGS       0x08        /* byte, bit 0: slot in use */
#define OBJ_OWNER       0x09
#define OBJ_X           0x10        /* ^ COORD_KEY */
#define OBJ_Y           0x14
#define OBJ_TYPE        0x18        /* TypeData *, type index at +4 */
#define COORD_KEY       0x63637
#define OBJS_UNITS_N    0x15C       /* + 4 * p: unit ids 0 .. n-1 */
#define OBJS_BUILD_N    0x184       /* + 4 * p: building ids 2000 .. n-1 */
#define FIRST_BUILD     2000
#define GROUP_PLAYER    0x4A        /* Group: byte */
#define GROUP_N         0x0C        /* Group / SelectGroup: member count, ids (short) at +0x8cc */
#define GROUP_IDS       0x8CC
#define TECHNICIAN      (0x32 + 341) /* unit 341 of unitrules.xml (was the campaign's Barbarian Archers) */
#define OPT_BUILD_ECO   0           /* Options::add code of an economy build button */
#define QUEUE_REPLACE   2
#define TILE            192         /* Coord per tile */
#define RESPAWN         (20 * 15)   /* frames before a killed technician's successor comes out */
#define HOME_REACH      (TILE * 6)  /* this close to a Data Center's centre, it goes in */
#define THINK           8           /* frames between two looks at a player's technician */

static int g_tech[8] = { -1, -1, -1, -1, -1, -1, -1, -1 }; /* its id, -1 = none (or not known yet) */
static int g_home[8];                                         /* walking back */
static int g_respawn_at[8];                                   /* game time the next may come out */
static int g_adding;                                          /* dc_make_options is adding buttons */

typedef int(__attribute__((thiscall)) *fn3_t)(void *self, int a, int b, int c);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);
typedef int(__attribute__((thiscall)) *fn5_t)(void *self, int a, int b, int c, int d, int e);
typedef int(__attribute__((thiscall)) *fn6_t)(void *self, int a, int b, int c, int d, int e, int f);

static int now(void) { return *(int *)(*(char **)VA(VA_GAME) + 0x550); }
static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }
static int units_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_UNITS_N + 4 * p); }
static int builds_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_BUILD_N + 4 * p); }
static int type_of(char *o) { return *(int *)(*(char **)(o + OBJ_TYPE) + 4); }
static int ox(char *o) { return *(int *)(o + OBJ_X) ^ COORD_KEY; }
static int oy(char *o) { return *(int *)(o + OBJ_Y) ^ COORD_KEY; }
static int finished(char *o) { return ((fn0_t)VA(VA_ACTIVE_BUILD))(o) != 0; }
static void *buildtype(int type) { return (*(void ***)(*(char **)VA(VA_BUILDTYPES) + 0x10))[type]; }
static void *vfn(void *o, int off) { return (*(void ***)o)[off / 4]; }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1c * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) && (unsigned char)o[OBJ_OWNER] == p ? o : NULL;
}

static long long dist2(int x0, int y0, int x1, int y1)
{
    long long dx = x0 - x1, dy = y0 - y1;
    return dx * dx + dy * dy;
}

int dc_is_technician(int type) { return g_dc_enabled && type == TECHNICIAN; }
int dc_is_expansion(int type) { return g_dc_enabled && is_expansion(type); }

/* A finished city of player p in the group (a command group or the selection). */
static int group_has_dc(char *group, int p)
{
    int n = *(int *)(group + GROUP_N);
    short *ids = (short *)(group + GROUP_IDS);
    for (int i = 0; i < n; i++) {
        char *o = object(p, ids[i]);
        if (o && is_city(type_of(o)) && finished(o))
            return 1;
    }
    return 0;
}

/* A new district at (x, y) (its centre) is next to one of player p's cities or districts: at most
   g_gap empty tiles between the plots, on both axes (half a tile of slack for the snapping). */
static int next_to_town(int p, int x, int y)
{
    for (int id = FIRST_BUILD; id < builds_n(p); id++) {
        char *o = object(p, id);
        if (!o)
            continue;
        int t = type_of(o), half;
        if (is_city(t))
            half = CITY_HALF;
        else if (is_expansion(t))
            half = DISTRICT_HALF;
        else
            continue;
        int reach = half + DISTRICT_HALF + g_gap * TILE + TILE / 2;
        if (abs(ox(o) - x) <= reach && abs(oy(o) - y) <= reach)
            return 1;
    }
    return 0;
}

/* The finished city of player p nearest to (x, y), or NULL: the districts' builder comes out of it. */
static char *nearest_dc(int p, int x, int y)
{
    char *best = NULL;
    long long bd = 0;
    for (int id = FIRST_BUILD; id < builds_n(p); id++) {
        char *o = object(p, id);
        if (!o || !is_city(type_of(o)) || !finished(o))
            continue;
        long long d = dist2(ox(o), oy(o), x, y);
        if (!best || d < bd) {
            best = o;
            bd = d;
        }
    }
    return best;
}

/* The unfinished Expansion site of player p nearest to (x, y), or -1. */
static int next_site(int p, int x, int y)
{
    int best = -1;
    long long bd = 0;
    for (int id = FIRST_BUILD; id < builds_n(p); id++) {
        char *o = object(p, id);
        if (!o || !is_expansion(type_of(o)) || finished(o))
            continue;
        long long d = dist2(ox(o), oy(o), x, y);
        if (best < 0 || d < bd) {
            best = id;
            bd = d;
        }
    }
    return best;
}

static int find_technician(int p)
{
    for (int id = 0; id < units_n(p); id++) {
        char *u = object(p, id);
        if (u && type_of(u) == TECHNICIAN)
            return id;
    }
    return -1;
}

/* entrench.c's Options::add hook: no Expansion in the Citizens' build menu (codes 0..2 are build
   buttons); the Data Center's own buttons pass. */
int dc_hides_option(int code, int type)
{
    return g_dc_enabled && !g_adding && code >= 0 && code <= 2 && is_expansion(type);
}

/* airdrop.c's BuildOut::make_options hook: the Data Center's panel, for its owner on this machine. */
void dc_make_options(char *b)
{
    int p = (unsigned char)b[OBJ_OWNER];
    if (!g_dc_enabled || p > 7 || p != local_player() || !is_city(type_of(b)) || !finished(b))
        return;
    char *ld = (char *)VA(VA_LEADERS) + p * LEADER_SIZE;
    g_adding = 1;
    for (int k = 0; k < KINDS; k++) { /* the city panel's bottom row, left */
        int avail = ((fn2_t)VA(VA_TYPE_AVAIL))(ld, k_exp_type[k], 1);
        ((fn6_t)VA(VA_OPTIONS_ADD))(*(void **)VA(VA_OPTIONS), OPT_BUILD_ECO, k, 2, k_exp_type[k], avail < 4, 0);
    }
    g_adding = 0;
}

/* entrench.c's GroupData::count hook, for a clicked build button: an Expansion's button counts a
   selected Data Center as the Citizens it asks for. */
int dc_group_builds(char *group, int type)
{
    return g_dc_enabled && is_expansion(type) && group_has_dc(group, local_player());
}

/* entrench.c's Group::action_build hook (a = x0, y0, x1, y1, type, queue), on every machine. An
   Expansion: laid here and paid, as the game's own code does, for a group with a finished Data
   Center (refused otherwise); nobody is sent, the technician comes. 1 = handled. */
int dc_action_build(char *group, int *a)
{
    int type = a[4];
    if (!g_dc_enabled || !is_expansion(type))
        return 0;
    int p = (unsigned char)group[GROUP_PLAYER];
    if (p > 7 || !group_has_dc(group, p)) {
        logf_("districts: player %d, a district refused to a group without a city", p);
        return 1;
    }
    char *ld = (char *)VA(VA_LEADERS) + p * LEADER_SIZE;
    if (((fn2_t)VA(VA_TYPE_AVAIL))(ld, type, 1) != 4)
        return 1;
    void *bt = buildtype(type), *objs = *(void **)VA(VA_OBJECTS);
    int x = a[0], y = a[1];
    ((fn5_t)VA(VA_SNAP_CENTER))(bt, x, y, (int)&x, (int)&y, p);
    if (((fn5_t)VA(VA_BLOCKED_SITE))(bt, x, y, p, -1, 0))
        return 1;
    if (!next_to_town(p, x, y)) {
        logf_("districts: player %d, (%d, %d) is not next to a city or district of theirs", p, x / TILE, y / TILE);
        return 1;
    }
    int city = ((fn5_t)VA(VA_FIND_CITY_AT))(objs, x, y, p, -1, 0);
    if (!((fn4_t)vfn(bt, BT_CAN_PAY))(bt, p, city, -1, 1))
        return 1;
    ((fn5_t)vfn(bt, BT_PAY))(bt, p, -1, city, 1, 0);
    int id = ((fn6_t)VA(VA_INIT_BUILD))(objs, p, type, x, y, 0, -1);
    logf_("data center: player %d, Expansion site %d (type %#x) at (%d, %d)", p, id, type, x / TILE, y / TILE);
    return 1;
}

static void look_after(int p, int t)
{
    char *u = object(p, g_tech[p]);
    if (u && type_of(u) != TECHNICIAN)
        u = NULL;
    if (!u && g_tech[p] >= 0) { /* it was out and is gone: killed */
        g_respawn_at[p] = t + RESPAWN;
        logf_("data center: player %d, technician %d lost, the next in %d s", p, g_tech[p], RESPAWN / 15);
        g_tech[p] = -1;
    }
    if (!u && (g_tech[p] = find_technician(p)) >= 0) /* after loading a game */
        u = object(p, g_tech[p]);
    int site = next_site(p, u ? ox(u) : 0, u ? oy(u) : 0);
    if (site >= 0) {
        if (!u) {
            char *s = object(p, site), *dc = s ? nearest_dc(p, ox(s), oy(s)) : NULL;
            if (!dc || t < g_respawn_at[p])
                return;
            g_tech[p] = ((fn1_t)VA(VA_TRAIN))(dc, TECHNICIAN);
            if (!(u = object(p, g_tech[p]))) {
                g_tech[p] = -1;
                return;
            }
            g_home[p] = 1; /* new: whatever order it came out with is replaced below */
            logf_("data center: player %d, technician %d out", p, g_tech[p]);
        }
        if (g_home[p] || ((fn0_t)VA(VA_IS_IDLE))(u)) {
            ((fn4_t)VA(VA_ADD_BUILD))(u, site, p, QUEUE_REPLACE, 0);
            g_home[p] = 0;
        }
        return;
    }
    if (!u)
        return;
    char *dc = nearest_dc(p, ox(u), oy(u));
    if (!dc || dist2(ox(u), oy(u), ox(dc), oy(dc)) <= (long long)HOME_REACH * HOME_REACH) {
        ((fn1_t)VA(VA_DISBAND))(u, 0);
        logf_("data center: player %d, technician %d done, gone %s", p, g_tech[p], dc ? "in" : "(no Data Center)");
        g_tech[p] = -1;
        g_home[p] = 0;
    } else if (!g_home[p] || ((fn0_t)VA(VA_IS_IDLE))(u)) {
        ((fn5_t)VA(VA_GO_TO))(u, ox(dc), oy(dc), 2, 0, 0x300);
        g_home[p] = 1;
    }
}

/* airdrop.c's Objects::process_all hook: once per simulation frame, after the units moved. */
void dc_frame(void)
{
    if (!g_dc_enabled)
        return;
    int t = now();
    for (int p = 0; p < 8; p++)
        if ((t + p) % THINK == 0)
            look_after(p, t);
}

__attribute__((used)) int c_leaders_init(void *self)
{
    memset(g_pop_seen, 0, sizeof g_pop_seen);
    memset(g_unlock_seen, 0xFF, sizeof g_unlock_seen);
    for (int p = 0; p < 8; p++) {
        g_tech[p] = -1;
        g_home[p] = g_respawn_at[p] = 0;
    }
    return ((fn0_t)g_orig_leaders_init)(self);
}

/* ---- hooks ---------------------------------------------------------------------------- */

void hk_dc_type_eligible(void);
void hk_dc_calc_pop_cap(void);
void hk_dc_calc_caps(void);
void hk_dc_calc_gather(void);
void hk_dc_leaders_init(void);

/* thiscall wrappers: ecx = this, stack arguments at [ebx+36], [ebx+40] after HOOK_ENTER */
#define DC_STUB0(name, cfn, ret)                                                            \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 12\n"                         \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"
#define DC_STUB1(name, cfn, ret)                                                            \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    push dword ptr [ebx+36]\n"                                                         \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"
#define DC_STUB2(name, cfn, ret)                                                            \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 4\n"                          \
    "    push dword ptr [ebx+40]\n"                                                         \
    "    push dword ptr [ebx+36]\n"                                                         \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

__asm__(".intel_syntax noprefix\n"
        DC_STUB2("hk_dc_type_eligible", "c_type_eligible", "ret 8")
        DC_STUB0("hk_dc_calc_pop_cap", "c_calc_pop_cap", "ret")
        DC_STUB0("hk_dc_calc_caps", "c_calc_caps", "ret")
        DC_STUB1("hk_dc_calc_gather", "c_calc_gather", "ret 4")
        DC_STUB0("hk_dc_leaders_init", "c_leaders_init", "ret")
        ".att_syntax prefix\n");

void dc_load_config(const char *dir)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%sdata_center.ini", dir);
    g_dc_enabled = GetFileAttributesA(ini) != INVALID_FILE_ATTRIBUTES;
    if (!g_dc_enabled)
        return;
    static const char *names[KINDS] = { "population", "commerce", "efficiency" };
    for (int k = 0; k < KINDS; k++)
        g_step[k] = GetPrivateProfileIntA("expansions", names[k], g_step[k], ini);
    g_ai_min_income = GetPrivateProfileIntA("ai", "min_income", g_ai_min_income, ini);
    g_gap = GetPrivateProfileIntA("districts", "gap", g_gap, ini);
    g_need_last_age = GetPrivateProfileIntA("districts", "need_last_age", g_need_last_age, ini);
}

/* The game's hard 999 (0x3E70: rates are stored times 16) outside calc_resource_caps, which
   the Commerce Expansions go past. Each is the immediate of a mov; it becomes "no limit", so
   the cap itself (from calc_resource_caps) is what counts. */
static void lift_999(void)
{
    static const struct {
        UINT_PTR at;
        const char *what;
    } k_sites[] = {
        { 0x006CE707u, "Leader::do_gather, nations with the interest bonus (Dutch)" },
        { 0x0081938Du, "IFaceResources::draw_cap, the commerce cap shown at the top" },
        { 0x0072A803u, "Options::describe, gather rate in tooltips" },
        { 0x0072A817u, "Options::describe, gather rate in tooltips" },
    };
    for (int i = 0; i < (int)(sizeof k_sites / sizeof k_sites[0]); i++) {
        INT32 *imm = (INT32 *)VA(k_sites[i].at);
        if (*imm != 0x3E70) {
            logf_("data center: 999 not found at %s, left as it is", k_sites[i].what);
            continue;
        }
        DWORD old;
        VirtualProtect(imm, 4, PAGE_EXECUTE_READWRITE, &old);
        *imm = 0x7FFFFFF0;
        VirtualProtect(imm, 4, old, &old);
    }
}

void dc_install(void)
{
    /* the type_eligible hook also serves armageddon.c and nonukes.c */
    if (!g_dc_enabled && !g_arm_enabled && !g_nonukes_enabled)
        return;
    /* `steal` whole instructions */
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[8];
        int sig_len, steal;
        void *hook;
        void **orig;
    } hooks[] = {
        /* push ebp / mov ebp,esp / sub esp,8 | push esi */
        { "type_eligible", VA_TYPE_ELIGIBLE, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x56 }, 7, 6,
          hk_dc_type_eligible, &g_orig_type_eligible },
        /* push ebp / mov ebp,esp / push ecx / push esi */
        { "calc_pop_cap", VA_CALC_POP_CAP, { 0x55, 0x8B, 0xEC, 0x51, 0x56 }, 5, 5, hk_dc_calc_pop_cap,
          &g_orig_calc_pop_cap },
        /* push ebp / mov ebp,esp / sub esp,8 */
        { "calc_resource_caps", VA_CALC_CAPS, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 }, 6, 6, hk_dc_calc_caps,
          &g_orig_calc_caps },
        /* mov eax,[game] */
        { "calc_gather", VA_CALC_GATHER, { 0xA1 }, 1, 5, hk_dc_calc_gather, &g_orig_calc_gather },
        /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
        { "Leaders::init", VA_LEADERS_INIT, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_dc_leaders_init,
          &g_orig_leaders_init },
    };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if (memcmp((void *)VA(hooks[i].va), hooks[i].sig, hooks[i].sig_len) != 0) {
            logf_("data center: signature mismatch at %s, turned off", hooks[i].name);
            g_dc_enabled = g_arm_enabled = g_nonukes_enabled = 0;
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    for (int i = 0; i < n; i++) {
        if (!g_dc_enabled && hooks[i].hook != (void *)hk_dc_type_eligible)
            continue;
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    if (g_dc_enabled)
        lift_999();
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    if (!g_dc_enabled)
        return;
    logf_("districts: on (population +%d, commerce cap +%d, efficiency +%d.%d%% each; gap %d tiles, need last Age %d)",
          g_step[0], g_step[1], g_step[2] / 10, g_step[2] % 10, g_gap, g_need_last_age);
}
