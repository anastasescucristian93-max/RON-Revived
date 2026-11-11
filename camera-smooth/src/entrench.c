/*
 * Entrenchment (entrenchment mod): ranged infantry dig a field fortification (the "Nest") and dig
 * into it. Inside they fight with their own weapons, standing still, with bonuses (more range, a
 * faster attack, more damage dealt), and the fortification takes every hit meant for them until
 * it is destroyed. Melee units cannot enter.
 *
 * The Nest takes the slot of the scenario-only Lenin Statue (type 0x1C2), available from The Art
 * of War.
 *  - Who builds it: ranged infantry, never Citizens or computer players. Options::add leaves it
 *    out of the Citizens' build menu; LeaderData::type_avail says "not available" to the
 *    computer players' planner (Leader::create_buildings); the infantry panel
 *    (UnitOut::make_options) gets the game's own build button for it, so the player gets the
 *    building cursor, the ghost and the price. Clicking a build button needs Citizens in the
 *    selection (GroupData::count(1) in Options::exec), which is answered for ranged infantry.
 *    Group::action_build (the command, on every machine) refuses the Nest for a group without
 *    ranged infantry, lays the site as usual and sends the squads beside it. The game gives
 *    build orders to Citizens only, so idle ranged foot soldiers near a site build it here
 *    (Wall::do_construct, as Unit::do_build does) and then dig into it.
 *  - Its hit points: Object::update_hits gives it hits_percent_of_tower of the Tower line's
 *    hits for the owner's age (Tower, Keep, Stockade, Bunker); the game adds its building bonuses.
 *  - Getting in: it has a garrison in buildingrules.xml and UnitTypeData::can_garrison lets ranged
 *    foot soldiers in (the engine lets land units into a few building lines only), so the game's
 *    garrison command (right-click, with its cursor) sends them to it, and the game's garrison
 *    button (code 0x1A, its cursor and hotkey), which the infantry panel gets once the player has
 *    a Nest. The soldiers never go inside:
 *  - Unit::do_garrison: when a squad that was sent to a Nest reaches it, its soldiers are put at
 *    their places on the Nest's floor (a triangle, see place; one squad per Nest) and get the game's own entrench flag (unit +0x68 0x2000000,
 *    the General's Entrench). The fee is paid here.
 *  - The game itself then keeps them in place (Unit::fight does not go after targets while the
 *    flag is set), clears the flag when they move, and saves it with the game. When the Nest is
 *    destroyed the flag is cleared here.
 *  - A soldier is "in a Nest" when it has the flag and stands inside one of its owner's Nests.
 *    Nothing else is kept, so saved games and multiplayer need no extra data. The table of
 *    soldiers in Nests is rebuilt after every simulation frame (Objects::process_all, shared with
 *    airdrop.c) and read by the hooks:
 *  - Object::do_damage (this = attacker): a hit on a soldier in a Nest hits the Nest instead, with
 *    the attacker's damage against the building.
 *  - UnitData::max_range: + range_bonus. UnitData::recharge: divided by 1 + fire_rate_percent.
 *    ObjectData::get_damage: + damage_dealt_percent from a soldier in a Nest.
 *  - UnitData::can_garrison: melee units cannot be sent to a Nest.
 *  - Leave Nest: the unused craft "Move Capital Here" (0x287) of craftrules.xml, with its own icon
 *    and the game's spell command (airdrop.c's spell hooks hand it over here), on a Nest with
 *    soldiers (BuildOut::make_options, shared with airdrop.c): they come out in front. The game's
 *    eject command (Group::action_eject_all) empties a Nest too.
 *
 * Enabled when entrench.ini is in the game folder (installed with the entrenchment mod's files).
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

/* hooked */
#define VA_DO_GARRISON    0x005E6B80u /* Unit::do_garrison(UnitOrder *), ret 4 */
#define VA_GET_DAMAGE     0x00644130u /* ObjectData::get_damage(target id, target player, angle, int, int, int *) const, ret 0x18 */
#define VA_RECHARGE       0x0060FDF0u /* UnitData::recharge(int *) const, virtual, ret 4 */
#define VA_MAX_RANGE      0x0060FE90u /* UnitData::max_range() const, virtual */
#define VA_CAN_GARRISON   0x006099A0u /* UnitData::can_garrison(building id, player) const, ret 8 */
#define VA_ACTION_EJECT   0x00710B40u /* Group::action_eject_all(int, int, int, int), ret 0x10 */
#define VA_DO_EJECT       0x0071C470u /* Options::do_eject_all(int), ret 4 */
#define VA_UNIT_OPTIONS   0x00615FC0u /* UnitOut::make_options() const, virtual */
#define VA_TYPE_GARRISON  0x0061D7D0u /* UnitTypeData::can_garrison(building type) const, ret 4 */
#define VA_OPTIONS_ADD    0x0072C290u /* Options::add(code, x, y, type, disabled, 0), ret 0x18 (also called) */
#define VA_GROUP_COUNT    0x00711720u /* GroupData::count(index, int, int) const, ret 0xc */
#define VA_TYPE_AVAIL     0x006E33A0u /* LeaderData::type_avail(type, int) const: 4 = available, ret 8 */
#define VA_ACTION_BUILD   0x00707510u /* Group::action_build(x0, y0, x1, y1, type, queue), ret 0x18 */
#define VA_UPDATE_HITS    0x00647010u /* Object::update_hits(int): the type's hits, into +0x20; ret 4 */
#define VA_DO_DAMAGE      0x0064A480u /* Object::do_damage(target id, target player, angle, ...) (8), ret 0x20 */
#define VA_AMMO_DAMAGE    0x00678060u /* Ammo::do_damage(): a shot lands, calls Object::do_damage per object hit ... */
#define VA_AMMO_CHECK_HIT 0x00678D90u /* ... up to the next function */
#define RA_EXEC_BUILD_COUNT 0x0071AE7Bu /* in Options::exec: after count(1) "has Citizens" for a build button */
#define VA_CREATE_BUILDINGS 0x006C1BE0u /* Leader::create_buildings: the computer players' planner ... */
#define VA_CREATE_UNITS     0x006C40A0u /* ... up to the next function */
/* called */
#define VA_DO_CONSTRUCT   0x006434D0u /* Wall::do_construct(amount): 1 when finished */
#define VA_GO_TO          0x005F7A50u /* Unit::go_to(x, y, 2, 0, 0x300) */
#define VA_HAS_TECH       0x006E0C80u /* LeaderData::has_tech(type) */
#define VT_AS_WALL    0xB0            /* Object: virtual, the building as the Wall do_construct works on */
#define OPT_CODE_GARRISON 0x1A        /* the game's garrison button: Options::do_garrison, cursor 7 */
#define VA_ISSUE_EJECT    0x00708B90u /* GroupOut::issue_eject_all(int, int) const, ret 8 */
#define VA_KILL_ORDER     0x005E2CB0u /* Unit::kill_current_order(int) */
#define VA_ADD_GARRISON   0x005E4080u /* Unit::add_garrison_order(building, player, int, QueuePos, int) */
#define VA_SET_GROUP      0x00605220u /* Unit::set_group(group, 0): the squad's move group, -1 = none */
#define VA_CLOSE_ORDERS   0x005E37F0u /* Unit::close_orders(int) */
#define VA_CLEAR_PATH     0x005E3920u /* Unit::clear_partial_path() */
#define VA_UPDATE_ACTION  0x0060A870u /* Unit::update_action() */
#define VA_SET_LOCATION   0x005F8D20u /* Unit::set_new_location(Coord x, Coord y, 1, 1), ret 0x10 */
#define VA_IS_IDLE        0x0046FA40u /* UnitData::is_idle() */
#define VA_REMOVE_ENTRENCH 0x008E45D0u /* GraphicEvents::remove_entrench(player, id), static */
#define VA_ACTIVE_BUILD   0x0046FB40u /* ObjectData::is_active_build() */
#define VA_FIND_Z         0x008544A0u /* TerrainOut::find_tcoord_z(Coord *out, TCoord, TCoord, 0) */
#define VT_ORDER_DATA     0xAC        /* UnitOrder: virtual, the order's data (+8 target id, +0xC target player) */
#define VT_IS         0xB8            /* ObjectData::is(int, int), virtual */
#define VT_IS_ON_MAP  0xBC            /* UnitData::is_on_map(), virtual */
#define VT_IS_CAPTAIN 0xE8            /* UnitData::is_captain(), virtual */
#define VT_ADJACENT   0x170           /* Object::adjacent_to(id, player), virtual */
#define TVT_GET_COST  0x78            /* TypeData::get_cost(r, player, -1, -1, 0, 1, -1), virtual, ret 0x1c */
/* data */
#define VA_OBJECTS        0x00C0618Cu
#define VA_OPTIONS        0x00C06204u
#define VA_CONSOLE        0x00C06210u
#define VA_GAME           0x00C061ECu
#define VA_TERRAIN        0x00C06218u
#define VA_DIV3           0x00CAE5FCu
#define VA_LEADERS        0x00E3A390u
#define VA_SELECTION      0x00E8D444u
#define VA_OBJTYPES       0x00C061F8u /* PtrArray<ObjectType> *: +0x10 = the types by index */
#define VA_CONSTANTS      0x00C061F0u /* Constants *: +0x22c = a builder's work per frame */
#define GROUP_SIZE        0x9F0
#define OPTS_PENDING      0x7C        /* Options: the clicked option (code, type, ...) */
#define T_HITS            0x210       /* ObjectType: HITS */

#define LEADER_SIZE  0x6EEC
#define LD_SECURE    0x6EB8 /* also the per-player state: +0xdc = age ^ AGE_KEY (LeaderData::get_age) */
#define STATE_AGE    0xDC
#define AGE_KEY      0x62766u
#define STOCK_KEY    0x8221
#define OBJ_FLAGS    0x08
#define OBJ_OWNER    0x09
#define OBJ_ID       0x0A
#define OBJ_X        0x10
#define OBJ_Y        0x14
#define OBJ_TYPE     0x18
#define OBJ_GUY      0xF4
#define COORD_KEY    0x63637
#define GUY_Z        0x14
#define GUY_X        0x5C   /* the model's map position, plain (Unit::set_new_location) */
#define GUY_Y        0x60
#define OBJS_UNITS_N 0x15C
#define OBJS_BUILD_N 0x184
#define FIRST_BUILD  2000
#define U_FLAGS      0x68   /* 0x2000000 = entrenched */
#define U_FLAGS2     0x6C   /* 0x20000 = the soft entrench of idle Marines; 0x1000 = Antipater's */
#define U_ANGLE      0x50
#define U_ENTRENCH_ANGLE 0x5C
#define U_CAPTAIN    0x8E   /* word: bit 15 set = captain, else the captain's id */
#define U_GROUP      0x80   /* short: the squad's move group (Groups), -1 = none */
#define U_NEXT       0x90   /* short: next soldier of the squad, -1 = last */
#define T_MAX_RANGE  0x1FC  /* the type's own maximum range */
#define T_OBJMASK    0x1E4  /* OBJ_MASK letters, A = bit 0: 0x20 = F, foot soldiers */
#define FOOT         0x20
#define ENTRENCHED   0x2000000
#define SOFT_BITS    0x21000

#define NEST         0x1C2  /* building 36 of buildingrules.xml, the Lenin Statue */
#define OPT_SPELL    4      /* Options::add code of a craft button */
#define CRAFT_LEAVE  0x287  /* "Move Capital Here", unused: the Leave Nest button on the Nest */
#define TOWER        0x1B7  /* the Tower line: Tower, Keep (0x1B8), Stockade (0x1B9), Bunker (0x1BA) */
#define BUILD_MILITARY 1    /* Options::add code of a build button (the build menu's category) */
#define ART_OF_WAR   0x23C  /* tech 28 of techrules.xml (techs start at 0x220): the Nest's prerequisite */
#define TILE         192
#define MAX_IDS      8192
#define MAX_NESTS    512
#define SQUADS       1       /* squads per Nest (GARRISON_MAX 3 in buildingrules.xml) */
#define SOLDIERS     3       /* places per squad */

/* settings (entrench.ini) */
int g_entrench_enabled;
static int g_range_bonus = 1, g_fire_rate = 40, g_dealt = 20;
static int g_hits_pct = 50; /* the Nest's hits: percent of the Tower line's for the owner's age */
static int g_fee = 35, g_fee_step = 10, g_fee_max = 150;
static int g_min_range = 5, g_sink = 0, g_slot_radius = 100;
static int g_place_age[9] = { 0, 0, 0, 0, 0, 55, 0, 0, 0 }; /* per age 1-8, 0 = place_radius */
static int g_inside = 170; /* a soldier this close to a Nest's centre is in it */
static int g_garrison_button; /* set while a unit panel is built: the game added its garrison button */
static int g_dig_col = 4, g_dig_row = 1;       /* the build button for the Nest */
static int g_dig_stand = 384;  /* where the squads stand while they dig: this far from the site's centre */
static int g_build_reach = 576; /* a soldier this close to a site's centre works on it */

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn3_t)(void *self, int a, int b, int c);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);
typedef int(__attribute__((thiscall)) *fn5_t)(void *self, int a, int b, int c, int d, int e);
typedef int(__attribute__((thiscall)) *fn6_t)(void *self, int a, int b, int c, int d, int e, int f);
typedef int(__attribute__((thiscall)) *fn7_t)(void *self, int a, int b, int c, int d, int e, int f, int g);
typedef int(__attribute__((stdcall)) *sfn2_t)(int a, int b);

static void *g_orig_do_garrison, *g_orig_get_damage, *g_orig_recharge, *g_orig_max_range, *g_orig_can_garrison,
    *g_orig_action_eject, *g_orig_do_eject, *g_orig_unit_options, *g_orig_type_garrison,
    *g_orig_options_add, *g_orig_group_count, *g_orig_type_avail, *g_orig_action_build, *g_orig_update_hits,
    *g_orig_do_damage;

/* ---- game access ---------------------------------------------------------------------- */

static char *game(void) { return *(char **)VA(VA_GAME); }
static int now(void) { return *(int *)(game() + 0x550); }
static char *leader(int p) { return (char *)VA(VA_LEADERS) + p * LEADER_SIZE; }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char *objs = *(char **)VA(VA_OBJECTS);
    char **arr = *(char ***)(objs + 0x14 + 0x1c * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

static int type_of(char *o) { return *(int *)(*(char **)(o + OBJ_TYPE) + 4); }
static int owner(char *o) { return (unsigned char)o[OBJ_OWNER]; }
static int id_of(char *o) { return *(short *)(o + OBJ_ID); }
static int ox(char *o) { return *(int *)(o + OBJ_X) ^ COORD_KEY; }
static int oy(char *o) { return *(int *)(o + OBJ_Y) ^ COORD_KEY; }
static int units_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_UNITS_N + 4 * p); }
static int builds_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_BUILD_N + 4 * p); }
static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }

static void *vfn(void *o, int off) { return (*(void ***)o)[off / 4]; }

static long long dist2(int x0, int y0, int x1, int y1)
{
    long long dx = x0 - x1, dy = y0 - y1;
    return dx * dx + dy * dy;
}

/* Integer square root (the same on every machine, unlike floating point). */
static long long isqrt(long long v)
{
    long long r = 0;
    for (long long bit = 1LL << 30; bit; bit >>= 1)
        if ((r + bit) * (r + bit) <= v)
            r += bit;
    return r;
}

static int stock(int p, int r) { return (*(int **)(leader(p) + LD_SECURE))[r] ^ STOCK_KEY; }
static void add_stock(int p, int r, int v)
{
    int *s = *(int **)(leader(p) + LD_SECURE);
    s[r] = (stock(p, r) + v) ^ STOCK_KEY;
}

static int ground_z(int x, int y)
{
    int *div3 = *(int **)VA(VA_DIV3);
    int z = 0;
    ((fn4_t)VA(VA_FIND_Z))(*(void **)VA(VA_TERRAIN), (int)&z, div3[x >> 6], div3[y >> 6], 0);
    return z;
}

static int is_nest(char *b) { return b && type_of(b) == NEST && ((fn0_t)VA(VA_ACTIVE_BUILD))(b); }
static int is_unit_obj(char *o) { return id_of(o) < FIRST_BUILD; }
static int is_captain(char *u) { return ((fn0_t)vfn(u, VT_IS_CAPTAIN))(u) != 0; }

static char *captain_of(char *u)
{
    if (is_captain(u))
        return u;
    char *c = object(owner(u), *(short *)(u + U_CAPTAIN));
    return c ? c : u;
}

static char *next_soldier(char *u)
{
    short n = *(short *)(u + U_NEXT);
    return n < 0 ? NULL : object(owner(u), n);
}

/* Ranged infantry only: foot soldiers (no tanks or guns) with a long enough range of their own
   (melee is 0, scouts 3, explorers 4). */
static int type_may_entrench(char *t)
{
    return (*(unsigned *)(t + T_OBJMASK) & FOOT) && *(int *)(t + T_MAX_RANGE) >= g_min_range;
}

static int may_entrench(char *u) { return type_may_entrench(*(char **)(u + OBJ_TYPE)); }

static int has_art_of_war(int p) { return ((fn1_t)VA(VA_HAS_TECH))(leader(p), ART_OF_WAR) != 0; }

/* ---- the table of soldiers in Nests ------------------------------------------------- */

static unsigned short g_in[8][MAX_IDS]; /* 1 + the Nest's index in g_nest (up to MAX_NESTS, past a byte) */
static int g_squads[8];                /* squads in Nests per player */
static struct { int player, id, x, y; } g_nest[MAX_NESTS];
static int g_nests;
static struct { int player, id; } g_site[MAX_NESTS]; /* Nests still being dug */
static int g_sites;
/* Each dug-in soldier's place, where refresh puts it back when the game has moved it. state 0 = none
   (dug in before a load, or not dug in), 1 = held, 2 = held and its first move logged. */
static struct { int x, y; unsigned char state; } g_spot[8][MAX_IDS];

static int in_nest(char *u)
{
    if (!g_entrench_enabled || !u)
        return 0;
    int p = owner(u), id = id_of(u);
    return p < 8 && id >= 0 && id < MAX_IDS && g_in[p][id];
}

/* For shove.c: a soldier the mod holds in a Nest must not be moved aside. */
int entrench_holds(char *u) { return in_nest(u); }

/* The Nest of this player that the point is inside, or -1. */
static int nest_at(int p, int x, int y)
{
    long long r2 = (long long)g_inside * g_inside;
    for (int i = 0; i < g_nests; i++)
        if (g_nest[i].player == p && dist2(x, y, g_nest[i].x, g_nest[i].y) <= r2)
            return i;
    return -1;
}

static void find_nests(void)
{
    g_nests = g_sites = 0;
    for (int p = 0; p < 8; p++)
        for (int id = FIRST_BUILD; id < builds_n(p) && g_nests < MAX_NESTS; id++) {
            char *b = object(p, id);
            if (!b || type_of(b) != NEST || owner(b) != p)
                continue;
            if (!((fn0_t)VA(VA_ACTIVE_BUILD))(b)) {
                if (g_sites < MAX_NESTS) {
                    g_site[g_sites].player = p;
                    g_site[g_sites].id = id;
                    g_sites++;
                }
                continue;
            }
            g_nest[g_nests].player = p;
            g_nest[g_nests].id = id;
            g_nest[g_nests].x = ox(b);
            g_nest[g_nests].y = oy(b);
            g_nests++;
        }
}

/* A Nest finished in the last frame is still a site in the table until the next refresh, but the
   squads sent to it when it was finished arrive first: it is added at the end, which keeps the
   other Nests' indices (g_in) valid. Its index, or -1. */
static int add_nest(int p, char *b)
{
    if (g_nests >= MAX_NESTS || !((fn0_t)VA(VA_ACTIVE_BUILD))(b))
        return -1;
    g_nest[g_nests].player = p;
    g_nest[g_nests].id = id_of(b);
    g_nest[g_nests].x = ox(b);
    g_nest[g_nests].y = oy(b);
    return g_nests++;
}

static void set_entrenched(char *u)
{
    ((fn1_t)VA(VA_CLOSE_ORDERS))(u, 0);
    ((fn0_t)VA(VA_CLEAR_PATH))(u);
    ((fn0_t)VA(VA_UPDATE_ACTION))(u);
    *(unsigned *)(u + U_FLAGS) = (*(unsigned *)(u + U_FLAGS) & ~0x4000000u) | ENTRENCHED;
    *(unsigned *)(u + U_FLAGS2) &= ~(unsigned)SOFT_BITS;
    *(int *)(u + 0xC0) = 0;
    *(int *)(u + U_ENTRENCH_ANGLE) = *(int *)(u + U_ANGLE);
}

static void clear_entrenched(char *u)
{
    *(unsigned *)(u + U_FLAGS) &= ~(unsigned)ENTRENCHED;
    *(unsigned *)(u + U_FLAGS2) &= ~(unsigned)SOFT_BITS;
    if (owner(u) < 8 && id_of(u) >= 0 && id_of(u) < MAX_IDS)
        g_spot[owner(u)][id_of(u)].state = 0;
    ((sfn2_t)VA(VA_REMOVE_ENTRENCH))(owner(u), id_of(u)); /* the game's dirt mound, if any */
}

/* ---- digging a Nest ------------------------------------------------------------------------ */

static int player_age(int p);

/* A member of the group that may dig in (a ranged foot soldier), or NULL. */
static char *digger_in(char *group)
{
    int p = (unsigned char)group[0x4a], n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n && p < 8; i++) {
        char *u = object(p, ids[i]);
        if (u && owner(u) == p && is_unit_obj(u) && may_entrench(captain_of(u)))
            return u;
    }
    return NULL;
}

/* Each squad of the group that may dig in walks beside the new site b, on its own side of it. */
static void send_diggers(char *group, char *b)
{
    int p = owner(b), n = *(int *)(group + 0xc), done[64], nd = 0;
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n; i++) {
        char *u = object(p, ids[i]);
        if (!u || !is_unit_obj(u))
            continue;
        char *cap = captain_of(u);
        int cid = id_of(cap), seen = 0;
        for (int j = 0; j < nd; j++)
            seen |= done[j] == cid;
        if (seen || !may_entrench(cap) || in_nest(cap) || nd >= 64)
            continue;
        done[nd++] = cid;
        /* clear of the footprint: a site with soldiers on it does not start */
        long long dx = ox(cap) - ox(b), dy = oy(cap) - oy(b), len = isqrt(dx * dx + dy * dy);
        if (len < 2) {
            dx = 0;
            dy = len = 1;
        }
        int tx = ox(b) + (int)(dx * g_dig_stand / len), ty = oy(b) + (int)(dy * g_dig_stand / len);
        ((fn5_t)VA(VA_GO_TO))(cap, tx, ty, 2, 0, 0x300);
    }
    logf_("entrench: player %d, Nest site %d at %d, %d (%d squads to dig it)", p, id_of(b), ox(b), oy(b), nd);
}

/* The Nest sites of player p, to find the one a build command lays. */
static int sites_of(int p, short *ids, int max)
{
    int n = 0;
    for (int id = FIRST_BUILD; id < builds_n(p) && n < max; id++) {
        char *b = object(p, id);
        if (b && type_of(b) == NEST && !((fn0_t)VA(VA_ACTIVE_BUILD))(b))
            ids[n++] = (short)id;
    }
    return n;
}

/* The Nest's hit points for player p: hits_percent_of_tower of the Tower line's building for the
   player's age (Tower in ages 1-2, Keep 3-4, Stockade 5-6, Bunker 7-8), read from the rules. */
static int nest_hits(int p)
{
    int age = player_age(p), line = age >= 1 ? (age - 1) / 2 : 0;
    if (line > 3)
        line = 3;
    char **types = *(char ***)(*(char **)VA(VA_OBJTYPES) + 0x10);
    char *tower = types ? types[TOWER + line] : NULL;
    int hits = tower ? *(int *)(tower + T_HITS) : 750;
    hits = hits * g_hits_pct / 100;
    return hits > 1 ? hits : 1;
}

/* Idle ranged foot soldiers of the site's owner close to it build it, one builder's work each per
   frame, as Unit::do_build does for Citizens. When it is finished, every squad of them by it digs
   in (as many as it has room for). */
static void work_on_site(int p, int site)
{
    char *b = object(p, site);
    if (!b)
        return;
    char *wall = (char *)((fn0_t)vfn(b, VT_AS_WALL))(b);
    int amount = *(int *)(*(char **)VA(VA_CONSTANTS) + 0x22c);
    long long r2 = (long long)g_build_reach * g_build_reach;
    int caps[32], nc = 0, finished = 0, n = units_n(p);
    for (int id = 0; id < n && id < MAX_IDS; id++) {
        char *u = object(p, id);
        if (!u || !((fn0_t)vfn(u, VT_IS_ON_MAP))(u) || (*(unsigned *)(u + U_FLAGS) & ENTRENCHED) ||
            dist2(ox(u), oy(u), ox(b), oy(b)) > r2 || !may_entrench(u))
            continue;
        if (!finished && ((fn0_t)VA(VA_IS_IDLE))(u))
            finished = ((fn1_t)VA(VA_DO_CONSTRUCT))(wall, amount) != 0;
        int cid = id_of(captain_of(u)), seen = 0;
        for (int j = 0; j < nc; j++)
            seen |= caps[j] == cid;
        if (!seen && nc < 32)
            caps[nc++] = cid;
    }
    if (!finished)
        return;
    for (int j = 0; j < nc; j++) {
        char *cap = object(p, caps[j]);
        if (cap)
            ((fn5_t)VA(VA_ADD_GARRISON))(cap, site, p, 0, 2, 1);
    }
    logf_("entrench: player %d, Nest %d dug, %d squads go in", p, site, nc);
}

/* Puts a dug-in soldier that the game moved back on its place in the Nest. The Nest's footprint
   is solid, and the soldiers were seen standing on its edge (2026-10-26); the first move of each
   soldier is logged with the model's position, to tell what moves it. */
static void hold_place(char *u, int p, int id)
{
    int x = g_spot[p][id].x, y = g_spot[p][id].y;
    if (g_spot[p][id].state == 1) {
        char **g = *(char ***)(u + OBJ_GUY);
        int gx = g && *g ? *(int *)(*g + GUY_X) : 0, gy = g && *g ? *(int *)(*g + GUY_Y) : 0;
        logf_("entrench: player %d, soldier %d moved off its place by %d, %d (model at %d, %d from it), put back",
              p, id, ox(u) - x, oy(u) - y, gx - x, gy - y);
        g_spot[p][id].state = 2;
    }
    unsigned flags = *(unsigned *)(u + U_FLAGS) & ENTRENCHED;
    ((fn4_t)VA(VA_SET_LOCATION))(u, x, y, 1, 1);
    *(unsigned *)(u + U_FLAGS) |= flags;
}

/* Rebuilds the table, takes the soldiers of a destroyed Nest out of cover, keeps the soldiers on
   their places and sinks them into the Nest's floor. Runs after every simulation frame, on every
   machine. */
static void refresh(void)
{
    static unsigned short was_in[8][MAX_IDS];
    memcpy(was_in, g_in, sizeof g_in);
    memset(g_in, 0, sizeof g_in);
    memset(g_squads, 0, sizeof g_squads);
    find_nests();
    for (int i = 0; i < g_sites; i++)
        work_on_site(g_site[i].player, g_site[i].id);
    for (int p = 0; p < 8; p++) {
        int n = units_n(p);
        if (n > MAX_IDS)
            n = MAX_IDS;
        for (int id = 0; id < n; id++) {
            char *u = object(p, id);
            if (!u || !((fn0_t)vfn(u, VT_IS_ON_MAP))(u)) {
                g_spot[p][id].state = 0;
                continue;
            }
            int dug = (*(unsigned *)(u + U_FLAGS) & ENTRENCHED) != 0;
            if (!dug)
                g_spot[p][id].state = 0;
            if (dug && g_spot[p][id].state && (ox(u) != g_spot[p][id].x || oy(u) != g_spot[p][id].y))
                hold_place(u, p, id);
            int k = nest_at(p, ox(u), oy(u));
            if (k < 0) {
                if (was_in[p][id] && dug) {
                    clear_entrenched(u); /* its Nest is gone */
                    logf_("entrench: player %d, soldier %d out of cover, the Nest is gone", p, id);
                }
                continue;
            }
            if (!dug)
                continue;
            g_in[p][id] = (unsigned short)(k + 1);
            if (is_captain(u)) {
                g_squads[p]++;
                if (*(short *)(u + U_GROUP) >= 0) /* dug in before the squad was taken out of its group */
                    ((fn2_t)VA(VA_SET_GROUP))(u, -1, 0);
            }
            char **g = *(char ***)(u + OBJ_GUY);
            if (g && *g && g_sink)
                *(int *)(*g + GUY_Z) = ground_z(ox(u), oy(u)) - g_sink;
        }
    }
}

/* ---- places inside the ring ------------------------------------------------------------ */

/* Current age of a player, 1 (Ancient) to 8 (Information); 0 when unknown (as nation_art.c). */
static int player_age(int p)
{
    char *state = *(char **)(leader(p) + LD_SECURE);
    if (!state)
        return 0;
    unsigned age = *(unsigned *)(state + STATE_AGE) ^ AGE_KEY;
    return age < 8 ? (int)age + 1 : 0;
}

/* The soldiers' distance from the centre for the Nest's owner: the Nest looks different in each
   age, and where its walls are high (age 5, the stone battery) the soldiers stand further in so
   the front walls do not hide them. */
static int place_radius(int p)
{
    int age = player_age(p);
    return age >= 1 && age <= 8 && g_place_age[age] > 0 ? g_place_age[age] : g_slot_radius;
}

/* The squad's three soldiers stand on an equilateral triangle around the Nest's centre, radius
   from it (x 1000, map x / y; the Nest is a square on the map, its front corner at +x +y): the
   captain towards the back corner, the other two towards the front, left and right on screen. */
static const int k_spot[SOLDIERS][2] = { { -707, -707 }, { 966, -259 }, { -259, 966 } };

static void place(int nx, int ny, int radius, int squad, int soldier, int *x, int *y)
{
    (void)squad; /* one squad per Nest */
    *x = nx + k_spot[soldier][0] * radius / 1000;
    *y = ny + k_spot[soldier][1] * radius / 1000;
}

static int nest_index(int p, int id);

/* A free squad place of the Nest, or -1 when it holds SQUADS squads. Each squad already in it
   (its captain in the table for this Nest) holds the place nearest to its captain; soldiers get
   pushed about a little, so a place is not found by distance alone. */
static int free_squad(int p, char *b)
{
    int k = nest_index(p, id_of(b)), nx = ox(b), ny = oy(b), radius = place_radius(p);
    int used[SQUADS] = { 0 }, count = 0, n = units_n(p);
    if (k < 0)
        k = add_nest(p, b);
    if (k < 0)
        return -1;
    for (int id = 0; id < n && id < MAX_IDS; id++) {
        char *u = g_in[p][id] == k + 1 ? object(p, id) : NULL;
        if (!u || !is_captain(u))
            continue;
        count++;
        int best = -1;
        long long bd = 0;
        for (int s = 0; s < SQUADS; s++) {
            int x, y;
            place(nx, ny, radius, s, 0, &x, &y);
            long long d = dist2(ox(u), oy(u), x, y);
            if (!used[s] && (best < 0 || d < bd)) {
                best = s;
                bd = d;
            }
        }
        if (best >= 0)
            used[best] = 1;
    }
    if (count >= SQUADS)
        return -1;
    for (int s = 0; s < SQUADS; s++)
        if (!used[s])
            return s;
    return -1;
}

/* ---- the fee ----------------------------------------------------------------------------- */

/* Percent of the squad's own price: fee, + fee_step per squad already in Nests, at most
   fee * (100 + fee_max) / 100. */
static int fee_percent(int p)
{
    int extra = g_fee_step * g_squads[p];
    if (extra > g_fee_max)
        extra = g_fee_max;
    return g_fee * (100 + extra) / 100;
}

static int pay_fee(int p, char *u)
{
    char *t = *(char **)(u + OBJ_TYPE);
    int pct = fee_percent(p), due[6];
    for (int r = 0; r < 6; r++) {
        int c = ((fn7_t)vfn(t, TVT_GET_COST))(t, r, p, -1, -1, 0, 1, -1);
        due[r] = c > 0 ? c * pct / 100 : 0;
        if (due[r] > stock(p, r))
            return 0;
    }
    for (int r = 0; r < 6; r++)
        if (due[r])
            add_stock(p, r, -due[r]);
    return 1;
}

/* ---- entering and leaving ------------------------------------------------------------------ */

static int nest_index(int p, int id)
{
    for (int i = 0; i < g_nests; i++)
        if (g_nest[i].player == p && g_nest[i].id == id)
            return i;
    return -1;
}

static void dig_in(char *cap, char *b, int squad)
{
    int p = owner(cap), k = 0, nx = ox(b), ny = oy(b), ni = nest_index(p, id_of(b)), radius = place_radius(p);
    for (char *u = cap; u && k < 8; u = next_soldier(u), k++) {
        int x, y;
        place(nx, ny, radius, squad, k < SOLDIERS ? k : SOLDIERS - 1, &x, &y);
        ((fn4_t)VA(VA_SET_LOCATION))(u, x, y, 1, 1);
        set_entrenched(u);
        if (id_of(u) < MAX_IDS) {
            g_in[p][id_of(u)] = (unsigned short)(ni + 1); /* the frame's table, until the next refresh */
            g_spot[p][id_of(u)].x = x;
            g_spot[p][id_of(u)].y = y;
            g_spot[p][id_of(u)].state = 1;
        }
    }
    ((fn2_t)VA(VA_SET_GROUP))(cap, -1, 0); /* out of its move group, or "All Military" skips it */
    g_squads[p]++;
    logf_("entrench: player %d, squad %d into Nest %d (place %d, %d soldiers)", p, id_of(cap), id_of(b), squad, k);
}

/* Everyone in this Nest leaves it, out in front of it. */
static void leave(char *b)
{
    int p = owner(b), nx = ox(b), ny = oy(b), out = 0;
    long long r2 = (long long)g_inside * g_inside;
    int n = units_n(p);
    for (int id = 0; id < n && id < MAX_IDS; id++) {
        char *u = object(p, id);
        if (!u || !is_unit_obj(u) || dist2(ox(u), oy(u), nx, ny) > r2)
            continue;
        if (!(*(unsigned *)(u + U_FLAGS) & ENTRENCHED) && !g_in[p][id])
            continue;
        clear_entrenched(u);
        g_in[p][id] = 0;
        /* a row in front of the ring, two tiles out */
        int x = nx + (out % 5 - 2) * 70, y = ny + 2 * TILE + (out / 5) * 70;
        ((fn4_t)VA(VA_SET_LOCATION))(u, x, y, 1, 1);
        out++;
    }
    logf_("entrench: player %d, Nest %d emptied (%d soldiers out)", p, id_of(b), out);
}

/* ---- hooks: simulation ------------------------------------------------------------------- */

void entrench_frame(void)
{
    static int last = -1;
    int t = now();
    if (t < last || t - last > 15) { /* another game, or a saved game loaded: the ids mean other units */
        memset(g_in, 0, sizeof g_in);
        memset(g_spot, 0, sizeof g_spot);
    }
    last = t;
    if (g_entrench_enabled)
        refresh();
}

__attribute__((used)) int c_en_do_garrison(char *u, int *a)
{
    char *order = (char *)a[0];
    int *d = order ? (int *)((fn0_t)vfn(order, VT_ORDER_DATA))(order) : NULL;
    char *b = d ? object(d[3], d[2]) : NULL;
    if (!is_nest(b) || owner(b) != owner(u))
        return ((fn1_t)g_orig_do_garrison)(u, a[0]);
    int p = owner(u);
    char *cap = captain_of(u);
    if (!may_entrench(cap)) {
        ((fn1_t)VA(VA_KILL_ORDER))(u, 0);
        return 0;
    }
    if (!((fn2_t)vfn(u, VT_ADJACENT))(u, d[2], d[3]))
        return ((fn1_t)g_orig_do_garrison)(u, a[0]); /* still walking there */
    int squad = free_squad(p, b);
    if (squad < 0 || !pay_fee(p, cap)) {
        logf_("entrench: player %d, squad %d refused at Nest %d (%s)", p, id_of(cap), id_of(b),
              squad < 0 ? "full" : "cannot pay the fee");
        ((fn1_t)VA(VA_KILL_ORDER))(u, 0);
        return 0;
    }
    dig_in(cap, b, squad);
    return 0;
}

__attribute__((used)) int c_en_get_damage(char *self, int *a)
{
    int dmg = ((fn6_t)g_orig_get_damage)(self, a[0], a[1], a[2], a[3], a[4], a[5]);
    if (is_unit_obj(self) && in_nest(self))
        dmg = dmg * (100 + g_dealt) / 100;
    return ranks_damage(self, dmg);
}

/* Whether this shot (an Ammo, or an attacker hitting without one) already hit this Nest during
   this frame; records it if not. */
static int nest_hit_before(void *shot, int p, int id)
{
    static struct { void *shot; int p, id, t; } hit[64];
    static int next;
    int t = now();
    for (int i = 0; i < 64; i++)
        if (hit[i].shot == shot && hit[i].t == t && hit[i].p == p && hit[i].id == id)
            return 1;
    hit[next].shot = shot;
    hit[next].p = p;
    hit[next].id = id;
    hit[next].t = t;
    next = (next + 1) % 64;
    return 0;
}

/* this = the attacker; a = target id, target player, angle, ..., a[6] = 1 for a splash hit (not the
   shot's own target, Ammo::do_damage), a[7] = 1 for the damage Object::do_damage spreads around.
   A hit on a soldier in a Nest hits the Nest: the game works out the attacker's damage against the
   building, its death and the alerts. The Nest counts as one target: splash on its soldiers is
   dropped, and one shot hits it once, however many soldiers or the Nest itself it catches. */
__attribute__((used)) int c_en_do_damage(char *self, int *a)
{
    char *t = object(a[1], a[0]), *b = NULL;
    int splash = a[6] || a[7];
    if (t && is_unit_obj(t) && in_nest(t)) {
        if (splash)
            return 0;
        int k = g_in[owner(t)][id_of(t)] - 1;
        b = k >= 0 && k < g_nests ? object(g_nest[k].player, g_nest[k].id) : NULL;
        if (b && is_nest(b)) {
            a[0] = g_nest[k].id;
            a[1] = g_nest[k].player;
        } else {
            b = NULL;
        }
    } else if (t && is_nest(t)) {
        b = t;
    }
    if (b) {
        unsigned ra = (unsigned)a[-1];
        /* from Ammo::do_damage, ebx (pushad, a[-5]) is the Ammo: one key per shot */
        void *shot = ra >= VA(VA_AMMO_DAMAGE) && ra < VA(VA_AMMO_CHECK_HIT) ? (void *)a[-5] : self;
        if (nest_hit_before(shot, owner(b), id_of(b)))
            return 0;
    }
    ((void(__attribute__((thiscall)) *)(void *, int, int, int, int, int, int, int, int))g_orig_do_damage)(
        self, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
    return 0;
}

/* The Nest's base hit points (the game then adds its building bonuses in Wall::update_hits). */
__attribute__((used)) int c_en_update_hits(char *self, int *a)
{
    int hits = ((fn1_t)g_orig_update_hits)(self, a[0]);
    if (g_entrench_enabled && type_of(self) == NEST && owner(self) < 8) {
        hits = nest_hits(owner(self));
        *(int *)(self + 0x20) = hits;
    }
    return hits;
}

__attribute__((used)) int c_en_recharge(char *self, int *a)
{
    int r = ((fn1_t)g_orig_recharge)(self, a[0]);
    if (r > 0 && in_nest(self)) {
        r = r * 100 / (100 + g_fire_rate);
        if (r < 1)
            r = 1;
    }
    return ranks_recharge(self, r);
}

__attribute__((used)) int c_en_max_range(char *self, int *a)
{
    (void)a;
    int r = ((fn0_t)g_orig_max_range)(self);
    return ranks_range(self, r > 0 && in_nest(self) ? r + g_range_bonus : r);
}

__attribute__((used)) int c_en_can_garrison(char *self, int *a)
{
    int ok = ((fn2_t)g_orig_can_garrison)(self, a[0], a[1]);
    char *b = object(a[1], a[0]);
    if (ok && b && type_of(b) == NEST)
        ok = may_entrench(captain_of(self));
    return ok;
}

__attribute__((used)) int c_en_action_eject(char *group, int *a)
{
    int p = (unsigned char)group[0x4a];
    int n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n && p < 8; i++) {
        char *b = object(p, ids[i]);
        if (is_nest(b) && owner(b) == p)
            leave(b);
    }
    return ((fn4_t)g_orig_action_eject)(group, a[0], a[1], a[2], a[3]);
}

/* ---- hooks: interface ------------------------------------------------------------------------ */

static int nest_has_soldiers(char *b)
{
    int p = owner(b), n = units_n(p);
    long long r2 = (long long)g_inside * g_inside;
    for (int id = 0; id < n && id < MAX_IDS; id++)
        if (g_in[p][id]) {
            char *u = object(p, id);
            if (u && dist2(ox(u), oy(u), ox(b), oy(b)) <= r2)
                return 1;
        }
    return 0;
}

static char *local_group(void)
{
    char *console = *(char **)VA(VA_CONSOLE);
    return console ? *(char **)VA(VA_SELECTION) + *(int *)(console + 0x2a0) * GROUP_SIZE : NULL;
}

/* The selection holds a Nest of the local player with soldiers in it. */
static int selection_has_nest(void)
{
    char *group = local_group();
    int p = local_player();
    if (!group || p < 0 || p > 7)
        return 0;
    int n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n; i++) {
        char *b = object(p, ids[i]);
        if (is_nest(b) && owner(b) == p && nest_has_soldiers(b))
            return 1;
    }
    return 0;
}

__attribute__((used)) int c_en_do_eject(char *self, int *a)
{
    if (selection_has_nest()) {
        ((fn2_t)VA(VA_ISSUE_EJECT))(local_group(), 0, a[0]);
        return 0;
    }
    return ((fn1_t)g_orig_do_eject)(self, a[0]);
}

/* The Nest's panel: the Leave Nest button, while soldiers are in it. */
void entrench_make_options(char *self)
{
    if (!g_entrench_enabled)
        return;
    int p = owner(self);
    if (p > 7 || p != local_player() || !is_nest(self) || !nest_has_soldiers(self))
        return;
    ((fn6_t)VA(VA_OPTIONS_ADD))(*(void **)VA(VA_OPTIONS), OPT_SPELL, 3, 0, CRAFT_LEAVE, 0, 0);
}

static int has_nest(int p)
{
    for (int i = 0; i < g_nests; i++)
        if (g_nest[i].player == p)
            return 1;
    return 0;
}

/* A unit's panel, for ranged infantry not yet dug in: from the Nest's tech the game's own build
   button for it (as the Citizens' build menu adds one: disabled while the type is not available),
   past the Options::add hook below; and once the player has a Nest, the game's own garrison button
   (code 0x1A, where the game puts it for the units that have it), with its cursor and hotkey. */
__attribute__((used)) int c_en_unit_options(char *self, int *a)
{
    (void)a;
    g_garrison_button = 0;
    ((fn0_t)g_orig_unit_options)(self);
    int p = owner(self);
    if (p > 7 || p != local_player())
        return 0;
    char *cap = captain_of(self);
    if (!may_entrench(cap) || in_nest(cap))
        return 0;
    void *opts = *(void **)VA(VA_OPTIONS);
    if (has_nest(p) && !g_garrison_button)
        ((fn6_t)g_orig_options_add)(opts, OPT_CODE_GARRISON, 1, 3, -1, 0, 0);
    if (has_art_of_war(p)) {
        int avail = ((fn2_t)VA(VA_TYPE_AVAIL))(leader(p), NEST, 1);
        ((fn6_t)g_orig_options_add)(opts, BUILD_MILITARY, g_dig_col, g_dig_row, NEST, avail < 4, 0);
    }
    return 0;
}

/* The Citizens' build menu: no Nest (ranged infantry dig it). Codes 0..2 are build buttons. */
__attribute__((used)) int c_en_options_add(char *opts, int *a)
{
    if ((g_entrench_enabled && a[0] >= 0 && a[0] <= 2 && a[3] == NEST) || dc_hides_option(a[0], a[3]))
        return 0;
    if (a[0] == OPT_CODE_GARRISON)
        g_garrison_button = 1; /* the game gave this unit its garrison button already */
    return ((fn6_t)g_orig_options_add)(opts, a[0], a[1], a[2], a[3], a[4], a[5]);
}

/* A build button clicked needs Citizens in the selection (GroupData::count(1) in Options::exec,
   the clicked option's type in Options + 0x80): for the Nest, ranged infantry do. */
__attribute__((used)) int c_en_group_count(char *group, int *a)
{
    int n = ((fn3_t)g_orig_group_count)(group, a[0], a[1], a[2]);
    if (n || a[0] != 1 || (unsigned)a[-1] != VA(RA_EXEC_BUILD_COUNT))
        return n;
    int type = *(int *)(*(char **)VA(VA_OPTIONS) + OPTS_PENDING + 4);
    if ((g_entrench_enabled && type == NEST && digger_in(group)) || dc_group_builds(group, type))
        n = 1; /* the Data Center orders its Expansions (datacenter.c) */
    return n;
}

/* The computer players' planner never builds a Nest, nor Tank Traps (it would scatter them
   around its cities and block its own armies). */
__attribute__((used)) int c_en_type_avail(char *ld, int *a)
{
    unsigned ra = (unsigned)a[-1];
    if (((g_entrench_enabled && a[0] == NEST) || a[0] == TANK_TRAPS || dc_is_expansion(a[0])) &&
        ra >= VA(VA_CREATE_BUILDINGS) && ra < VA(VA_CREATE_UNITS))
        return 0; /* nor a city district (only a city orders them) */
    return ((fn2_t)g_orig_type_avail)(ld, a[0], a[1]);
}

/* The build command, on every machine (a = x0, y0, x1, y1, type, queue). A Nest only for a group
   with ranged infantry; the game lays the site and pays, then the squads go and dig it. */
__attribute__((used)) int c_en_action_build(char *group, int *a)
{
    typedef void(__attribute__((thiscall)) * build_t)(void *, int, int, int, int, int, int);
    if (dc_action_build(group, a)) /* an Expansion: datacenter.c lays it, its technician builds it */
        return 0;
    if (!g_entrench_enabled || a[4] != NEST) {
        ((build_t)g_orig_action_build)(group, a[0], a[1], a[2], a[3], a[4], a[5]);
        return 0;
    }
    int p = (unsigned char)group[0x4a];
    if (p > 7 || !digger_in(group)) {
        logf_("entrench: player %d, a Nest refused to a group without ranged infantry", p);
        return 0;
    }
    short before[MAX_NESTS], after[MAX_NESTS];
    int nb = sites_of(p, before, MAX_NESTS);
    ((build_t)g_orig_action_build)(group, a[0], a[1], a[2], a[3], a[4], a[5]);
    int na = sites_of(p, after, MAX_NESTS);
    for (int i = 0; i < na; i++) {
        int old = 0;
        for (int j = 0; j < nb && !old; j++)
            old = before[j] == after[i];
        if (!old)
            send_diggers(group, object(p, after[i]));
    }
    return 0;
}

/* Ranged foot soldiers may garrison a Nest; the engine allows land units into a few building
   lines only (cities, forts and the like). */
__attribute__((used)) int c_en_type_garrison(char *utype, int *a)
{
    if (a[0] == NEST)
        return type_may_entrench(utype);
    return ((fn1_t)g_orig_type_garrison)(utype, a[0]);
}

/* ---- the buttons as crafts (airdrop.c's spell hooks hand these over) ----------------------- */

int entrench_is_craft(int type)
{
    return g_entrench_enabled && type == CRAFT_LEAVE;
}

/* A member of the group that can use the button, or -1: a Nest with soldiers. */
int entrench_caster(char *group, int type)
{
    int p = (unsigned char)group[0x4a], n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    if (p > 7 || type != CRAFT_LEAVE)
        return -1;
    for (int i = 0; i < n; i++) {
        char *o = object(p, ids[i]);
        if (!o || owner(o) != p)
            continue;
        if (is_nest(o) && nest_has_soldiers(o))
            return ids[i];
    }
    return -1;
}

/* GroupOut::validate_spell for the buttons (a = type, target id, target player, x, y): 0 = valid. */
int entrench_validate(char *group, int *a)
{
    return entrench_caster(group, a[0]) < 0;
}

/* The command, on every machine (a = type, target id, target player, x, y). Leave Nest: every
   Nest of the group is emptied. */
void entrench_action(char *group, int *a)
{
    int p = (unsigned char)group[0x4a], n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    if (p > 7)
        return;
    if (a[0] == CRAFT_LEAVE)
        for (int i = 0; i < n; i++) {
            char *b = object(p, ids[i]);
            if (is_nest(b) && owner(b) == p)
                leave(b);
        }
}

/* ---- install --------------------------------------------------------------------------------- */

void hk_en_do_garrison(void);
void hk_en_get_damage(void);
void hk_en_recharge(void);
void hk_en_max_range(void);
void hk_en_can_garrison(void);
void hk_en_action_eject(void);
void hk_en_do_eject(void);
void hk_en_unit_options(void);
void hk_en_type_garrison(void);
void hk_en_options_add(void);
void hk_en_group_count(void);
void hk_en_type_avail(void);
void hk_en_action_build(void);
void hk_en_update_hits(void);
void hk_en_do_damage(void);

/* thiscall wrappers: ecx = this; the C function gets (this, pointer to the stack arguments) */
#define EN_STUB(name, cfn, ret)                                                              \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    lea eax, [ebx+36]\n"                                                               \
    "    push eax\n"                                                                        \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

__asm__(".intel_syntax noprefix\n"
        EN_STUB("hk_en_do_garrison", "c_en_do_garrison", "ret 4")
        EN_STUB("hk_en_get_damage", "c_en_get_damage", "ret 0x18")
        EN_STUB("hk_en_recharge", "c_en_recharge", "ret 4")
        EN_STUB("hk_en_max_range", "c_en_max_range", "ret")
        EN_STUB("hk_en_can_garrison", "c_en_can_garrison", "ret 8")
        EN_STUB("hk_en_action_eject", "c_en_action_eject", "ret 0x10")
        EN_STUB("hk_en_do_eject", "c_en_do_eject", "ret 4")
        EN_STUB("hk_en_unit_options", "c_en_unit_options", "ret")
        EN_STUB("hk_en_type_garrison", "c_en_type_garrison", "ret 4")
        EN_STUB("hk_en_options_add", "c_en_options_add", "ret 0x18")
        EN_STUB("hk_en_group_count", "c_en_group_count", "ret 0xc")
        EN_STUB("hk_en_type_avail", "c_en_type_avail", "ret 8")
        EN_STUB("hk_en_action_build", "c_en_action_build", "ret 0x18")
        EN_STUB("hk_en_update_hits", "c_en_update_hits", "ret 4")
        EN_STUB("hk_en_do_damage", "c_en_do_damage", "ret 0x20")
        ".att_syntax prefix\n");

void entrench_load_config(const char *dir)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%sentrench.ini", dir);
    g_entrench_enabled = GetFileAttributesA(ini) != INVALID_FILE_ATTRIBUTES;
    if (!g_entrench_enabled)
        return;
    g_range_bonus = GetPrivateProfileIntA("bonus", "range", g_range_bonus, ini);
    g_fire_rate = GetPrivateProfileIntA("bonus", "fire_rate_percent", g_fire_rate, ini);
    g_dealt = GetPrivateProfileIntA("bonus", "damage_dealt_percent", g_dealt, ini);
    g_hits_pct = GetPrivateProfileIntA("nest", "hits_percent_of_tower", g_hits_pct, ini);
    g_fee = GetPrivateProfileIntA("fee", "percent_of_unit_cost", g_fee, ini);
    g_fee_step = GetPrivateProfileIntA("fee", "step_percent_per_squad", g_fee_step, ini);
    g_fee_max = GetPrivateProfileIntA("fee", "max_extra_percent", g_fee_max, ini);
    g_min_range = GetPrivateProfileIntA("rules", "min_unit_range", g_min_range, ini);
    g_sink = GetPrivateProfileIntA("look", "sink", g_sink, ini);
    g_slot_radius = GetPrivateProfileIntA("look", "place_radius", g_slot_radius, ini);
    for (int age = 1; age <= 8; age++) {
        char key[32];
        snprintf(key, sizeof key, "place_radius_age%d", age);
        g_place_age[age] = GetPrivateProfileIntA("look", key, g_place_age[age], ini);
    }
    g_inside = GetPrivateProfileIntA("look", "inside_radius", g_inside, ini);
    g_dig_col = GetPrivateProfileIntA("look", "dig_button_column", g_dig_col, ini);
    g_dig_row = GetPrivateProfileIntA("look", "dig_button_row", g_dig_row, ini);
    g_dig_stand = GetPrivateProfileIntA("look", "dig_stand", g_dig_stand, ini);
    g_build_reach = GetPrivateProfileIntA("rules", "dig_reach", g_build_reach, ini);
}

/* Options::add, GroupData::count, LeaderData::type_avail and Group::action_build are shared with
   datacenter.c (the Expansions are ordered at the Data Center), and get_damage, recharge and
   max_range with ranks.c (the rank bonuses), so they are installed when either feature is on. */
#define SHARED_DC    1
#define SHARED_RANKS 2

static int hook_needed(int shared)
{
    return g_entrench_enabled || ((shared & SHARED_DC) && g_dc_enabled) || ((shared & SHARED_RANKS) && g_ranks_enabled);
}

/* ---- the Tank Traps by age ---------------------------------------------------------------- */

/* The Tank Traps can be built in every age (the user, 2026-11-10: wooden stakes in the first
   ages, sandbags later, the steel hedgehogs from the Modern Age), with one price for all, but
   their strength follows their owner's age: hit points as a percent of the type's 1200 (ranks.c's
   Wall::update_hits hook asks here) and the armor (WallData::armor, the buildings' virtual
   +0x124, replaced in the vtables). Index = age, 1 Ancient .. 8 Information. */
static const int k_traps_hits[9] = { 100, 20, 25, 35, 45, 60, 80, 100, 100 };
static const int k_traps_armor[9] = { 8, 2, 2, 3, 4, 5, 6, 8, 8 };
#define VA_WALL_ARMOR 0x0063FA60u /* WallData::armor(void) const */
#define VT_ARMOR      0x124
static void *g_orig_armor;

int entrench_traps_hits_pct(char *o)
{
    if (!o || !*(char **)(o + OBJ_TYPE) || type_of(o) != TANK_TRAPS)
        return 100;
    int age = player_age(owner(o));
    return age >= 1 && age <= 8 ? k_traps_hits[age] : 100;
}

__attribute__((used, thiscall)) static int c_traps_armor(char *self)
{
    int armor = ((int(__attribute__((thiscall)) *)(void *))g_orig_armor)(self);
    if (self && *(char **)(self + OBJ_TYPE) && type_of(self) == TANK_TRAPS) {
        int age = player_age(owner(self));
        if (age >= 1 && age <= 8)
            armor = k_traps_armor[age];
    }
    return armor;
}

/* every vtable of the building classes whose armor slot is WallData::armor */
static void traps_armor_install(void)
{
    static const UINT_PTR vtables[] = { 0x00B42174u /* Build */, 0x00B42CF8u /* Wall */ };
    int n = 0;
    for (int i = 0; i < 2; i++) {
        void **slot = (void **)(VA(vtables[i]) + VT_ARMOR);
        if (*slot != (void *)VA(VA_WALL_ARMOR))
            continue;
        g_orig_armor = *slot;
        DWORD old;
        VirtualProtect(slot, 4, PAGE_READWRITE, &old);
        *slot = (void *)c_traps_armor;
        VirtualProtect(slot, 4, old, &old);
        n++;
    }
    logf_(n ? "tank traps: armor by age in %d building vtable(s)" : "tank traps: WallData::armor not found in the vtables (%d), one armor for every age", n);
}

void entrench_install(void)
{
    traps_armor_install();
    if (!g_entrench_enabled && !g_dc_enabled && !g_ranks_enabled)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[9];
        int sig_len, steal;
        void *hook;
        void **orig;
        int shared;
    } hooks[] = {
        /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
        { "Unit::do_garrison", VA_DO_GARRISON, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_en_do_garrison,
          &g_orig_do_garrison },
        /* push ebp / mov ebp,esp / sub esp,0x1c */
        { "ObjectData::get_damage", VA_GET_DAMAGE, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C }, 6, 6, hk_en_get_damage,
          &g_orig_get_damage, SHARED_RANKS },
        /* push ebp / mov ebp,esp / push ebx / mov ebx,[ebp+8] */
        { "UnitData::recharge", VA_RECHARGE, { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D, 0x08 }, 7, 7, hk_en_recharge,
          &g_orig_recharge, SHARED_RANKS },
        /* push esi / push edi / mov edi,ecx / mov ecx,[ecx+0x18] */
        { "UnitData::max_range", VA_MAX_RANGE, { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x4F, 0x18 }, 7, 7, hk_en_max_range,
          &g_orig_max_range, SHARED_RANKS },
        /* push ebp / mov ebp,esp / push ecx / mov eax,[ebp+0xc] */
        { "UnitData::can_garrison", VA_CAN_GARRISON, { 0x55, 0x8B, 0xEC, 0x51, 0x8B, 0x45, 0x0C }, 7, 7,
          hk_en_can_garrison, &g_orig_can_garrison },
        /* push ebp / mov ebp,esp / and esp,-8 */
        { "Group::action_eject_all", VA_ACTION_EJECT, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, 6, 6,
          hk_en_action_eject, &g_orig_action_eject },
        /* push ebp / mov ebp,esp / mov edx,[console] */
        { "Options::do_eject_all", VA_DO_EJECT, { 0x55, 0x8B, 0xEC, 0x8B, 0x15 }, 5, 9, hk_en_do_eject,
          &g_orig_do_eject },
        /* push ebp / mov ebp,esp / sub esp,0xc */
        { "UnitOut::make_options", VA_UNIT_OPTIONS, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C }, 6, 6, hk_en_unit_options,
          &g_orig_unit_options },
        /* push ebp / mov ebp,esp / push edi / mov edi,ecx */
        { "UnitTypeData::can_garrison", VA_TYPE_GARRISON, { 0x55, 0x8B, 0xEC, 0x57, 0x8B, 0xF9 }, 6, 6,
          hk_en_type_garrison, &g_orig_type_garrison },
        /* push ebp / mov ebp,esp / mov eax,fs:[0] */
        { "Options::add", VA_OPTIONS_ADD, { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 }, 9, 9,
          hk_en_options_add, &g_orig_options_add, SHARED_DC },
        /* push ebp / mov ebp,esp / sub esp,0x10 */
        { "GroupData::count", VA_GROUP_COUNT, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 }, 6, 6, hk_en_group_count,
          &g_orig_group_count, SHARED_DC },
        /* push ebp / mov ebp,esp / push ecx / push esi / push edi */
        { "LeaderData::type_avail", VA_TYPE_AVAIL, { 0x55, 0x8B, 0xEC, 0x51, 0x56, 0x57 }, 6, 6, hk_en_type_avail,
          &g_orig_type_avail, SHARED_DC },
        /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
        { "Group::action_build", VA_ACTION_BUILD, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_en_action_build,
          &g_orig_action_build, SHARED_DC },
        /* mov eax,[ecx+0x18] / mov edx,[eax+4] */
        { "Object::update_hits", VA_UPDATE_HITS, { 0x8B, 0x41, 0x18, 0x8B, 0x50, 0x04 }, 6, 6, hk_en_update_hits,
          &g_orig_update_hits },
        /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
        { "Object::do_damage", VA_DO_DAMAGE, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_en_do_damage,
          &g_orig_do_damage },
    };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if (hook_needed(hooks[i].shared) && memcmp((void *)VA(hooks[i].va), hooks[i].sig, hooks[i].sig_len) != 0) {
            logf_("entrench: signature mismatch at %s, entrenchment (and the Data Center's technician and the "
                  "ranks) turned off",
                  hooks[i].name);
            g_entrench_enabled = 0;
            g_ranks_enabled = 0;
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    for (int i = 0; i < n; i++) {
        if (!hook_needed(hooks[i].shared))
            continue;
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    if (!g_entrench_enabled)
        return;
    logf_("entrench: on (range +%d, attack rate +%d%%, damage +%d%%, Nest %d%% of a Tower's hits, fee %d%% +%d%% per "
          "squad up to +%d%%)",
          g_range_bonus, g_fire_rate, g_dealt, g_hits_pct, g_fee, g_fee_step, g_fee_max);
}
