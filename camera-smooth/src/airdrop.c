/*
 * Airdrop (paradrop mod): an Airbase ability. A transport helicopter flies from the Airbase to
 * the spot the player picked, comes down, three Paratroopers appear, it lifts off, flies back
 * and lands at its Airbase, where it disappears and a third of the price comes back.
 *
 * The game's own "Paradrop" craft (type 0x284) is unfinished (casting it crashes in
 * Unit::do_cast), but its targeting works: area cursor, the spell command over the network. So:
 *  - BuildOut::make_options: an Airbase of the local player gets the spell option for 0x284
 *    (Options::add(4, 3, 0, 0x284, disabled, 0), the aircraft row); disabled before the Information Age, during the
 *    cooldown or without the resources.
 *  - GroupData::get_spellcaster / get_valid_spellcaster: for 0x284 they answer with a ready
 *    Airbase of the group, so Options::do_spell starts the targeting.
 *  - GroupOut::validate_spell: for 0x284, 0 (valid) when the group has a ready Airbase and the
 *    spot is on the map.
 *  - Group::action_spell (the command, run on every machine): for 0x284 the drop starts here
 *    and the game's own code (Unit::do_cast) is never reached.
 *  - Objects::process_all (every simulation frame, after the units moved): the missions' state
 *    machine, and the computer players' use of the ability. While it lands, the helicopter's
 *    model height (Guy +0x14) is set here, after the air physics set its own: down to the
 *    ground, a moment there, back up to flying height. Drawing interpolates it like any move.
 *  - SelectGroup::add: the helicopter cannot be selected, so it cannot be given orders.
 *  - Interface: Options::draw_cursor_follows draws the landing zone under the cursor while
 *    aiming; Options::target_spell (the click) counts the drop as pending and, while the
 *    selected Airbases have drops left (airdrop_charges, also shown on the button by panel.c),
 *    the targeting starts again on the next frame (airdrop_ui_tick).
 * Everything that changes the game happens in the simulation from the same command on every
 * machine, so it stays in sync in multiplayer (every player needs the mod). The button only
 * reads.
 *
 * Missions are not in saved games: a helicopter found without a mission (after loading a game)
 * is removed. Cooldowns and missions are cleared when a new game starts or a game is loaded (the
 * game time jumps), so nothing carries over from the previous game.
 *
 * Enabled when airdrop.ini is in the game folder (installed with the paradrop mod's files).
 * Engine notes: paradrop\ENGINE_NOTES.md.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

/* hooked */
#define VA_MAKE_OPTIONS   0x00629ED0u /* BuildOut::make_options() const, virtual */
#define VA_GET_CASTER     0x0070CA10u /* GroupData::get_spellcaster(int), ret 4 */
#define VA_GET_VCASTER    0x0070C770u /* GroupData::get_valid_spellcaster(int), ret 4 */
#define VA_VALIDATE_SPELL 0x007095E0u /* GroupOut::validate_spell(type, int, int, Coord, Coord): 0 = ok, ret 0x14 */
#define VA_ACTION_SPELL   0x006FE1A0u /* Group::action_spell(type, int, int, Coord, Coord), ret 0x14 */
#define VA_SELECT_ADD     0x00716C10u /* SelectGroup::add(id, player, int), virtual, ret 0xc */
#define VA_OBJ_PROCESS    0x0065DCE0u /* Objects::process_all(), once per simulation frame, from Game::do_frame */
#define VA_TARGET_SPELL   0x0071DDA0u /* Options::target_spell(type, int, int, Coord, Coord): the click, ret 0x14 */
#define VA_CURSOR_FOLLOWS 0x00723C00u /* Options::draw_cursor_follows(), from Scene::render: what follows the cursor */
/* called */
#define VA_OPTIONS_ADD    0x0072C290u /* Options::add(code, x, y, type, disabled, 0) */
#define VA_INIT_UNIT      0x0065E0C0u /* Objects::init_unit(player, type, x, y, -1, -1, -1): id */
#define VA_GO_TO          0x005F7A50u /* Unit::go_to(x, y, 2, 0, 0x300) */
#define VA_DISBAND        0x006455C0u /* Object::disband(0) */
#define VA_GO_INSIDE      0x0061A2E0u /* Unit::go_inside(building id, player, 0) */
#define VA_ACTIVE_BUILD   0x0046FB40u /* ObjectData::is_active_build(): a finished building, not a site */
#define VT_IS_ON_MAP      0xBC        /* UnitData::is_on_map(), virtual: 0 when inside a building */
#define VA_IS_ENEMY       0x006EBAA0u /* LeaderData::is_enemy(player) */
#define VA_HAS_TECH       0x006E0C80u /* LeaderData::has_tech(type) */
#define VA_FIND_Z         0x008544A0u /* TerrainOut::find_tcoord_z(Coord *out, TCoord, TCoord, 0) */
#define VA_DO_SPELL       0x0071D7A0u /* Options::do_spell(type, 0): the button, starts the targeting, ret 8 */
#define VA_DRAW_CIRCLE    0x008C6040u /* draw_circle(x, y, z, Color, -1, 1, 0, 0.0f), radius in ecx, CircleType in edx */
#define VA_TC_GET_UNIT    0x008C2DB0u /* TeamColor::get_unit(Color *out), ret 4 */
#define VA_TEAM_COLORS    0x00E39CE0u /* TeamColor[], 0x64 each, by [leader + 0x6928] */
/* data */
#define VA_OBJECTS        0x00C0618Cu /* Objects *: per player +0x14 + 0x1c*p = object pointers by id */
#define VA_OPTIONS        0x00C06204u /* Options * */
#define VA_CONSOLE        0x00C06210u /* Console *: +0x298 = local player */
#define VA_GAME           0x00C061ECu /* Game *: +0x550 = game time (1/15 s), +0x854 = frame fraction */
#define VA_TERRAIN        0x00C06218u /* Terrain * */
#define VA_DIV3           0x00CAE5FCu /* int *div_3_table */
#define VA_WORLD          0x00C06188u /* World *: [0], [1] = map size; times 768 = Coord */
#define VA_LEADERS        0x00E3A390u /* Leader[8], 0x6EEC each */
#define VA_TYPEDATA       0x00C0A274u /* TypeData *[]: by TypeIndex; + TD_POP = population it takes */
#define VA_SELECTION      0x00E8D444u /* SelectGroup[]: the local selection is [console + 0x2a0] * GROUP_SIZE in */
#define GROUP_SIZE        0x9F0

#define LEADER_SIZE  0x6EEC
#define LD_FLAGS     0x000  /* bit 2: human */
#define LD_POP_CAP   0x7E4  /* population limit (Leader::calc_pop_cap) */
#define LD_POP       0x940  /* population now (LeaderData::check_population, draw_pop_cap) */
#define TD_POP       0x2F0  /* TypeData: POP of unitrules.xml */
#define LD_SECURE    0x6EB8 /* encrypted values: + 4 * r = treasury ^ STOCK_KEY */
#define STOCK_KEY    0x8221
#define OBJ_FLAGS    0x08   /* byte, bit 0: slot in use */
#define OBJ_OWNER    0x09   /* byte */
#define OBJ_ID       0x0A   /* short */
#define OBJ_X        0x10   /* ^ COORD_KEY */
#define OBJ_Y        0x14
#define OBJ_TYPE     0x18   /* TypeData *, type index at +4 */
#define OBJ_DAMAGE   0x24
#define OBJ_GUY      0xF4   /* units: Guy ** */
#define VT_MAX_HITS  0x11C  /* ObjectData::max hits(0), virtual */
#define COORD_KEY    0x63637
#define GUY_Z        0x14   /* int, current; +0x70 = previous frame */
#define OBJS_UNITS_N 0x15C  /* + 4 * p: unit ids 0 .. n-1 */
#define OBJS_BUILD_N 0x184  /* + 4 * p: building ids 2000 .. n-1 */
#define FIRST_BUILD  2000

#define SPELL_PARADROP 0x284
#define OPT_SPELL      4
#define AD_COL         3 /* the Airbase's panel: aircraft at columns 0..2 of row 0 */
#define AD_ROW         0
#define AIRBASE        0x1BF /* building 33 of buildingrules.xml */
#define CITY_FIRST     0x19E /* Small, Large, Major City */
#define CITY_LAST      0x1A0
#define INFORMATION_AGE 0x226 /* the tech of age 8 (Modern Age 0x225); the Airdrop is an age-8 ability (the user, 2026-10-28) */
#define HELI           (0x32 + 338) /* unit 338 "Harrier Jet" = Transport Helicopter */
#define PARA           (0x32 + 330) /* unit 330 "Guerilla Fighters" = Paratroopers */
#define CITIZEN_FIRST  0x32
#define CITIZEN_LAST   0x35
#define TILE           192 /* Coord per tile: TCoord = div3[Coord >> 6] */
#define RES_FOOD       0
#define RES_OIL        5

/* settings (airdrop.ini) */
int g_airdrop_enabled;
static int g_cost_food = 400, g_cost_oil = 350, g_refund_pct = 33, g_cooldown_s = 45;
static int g_squad = 2; /* Paratrooper units per drop; each unit is a squad of 3 soldiers (UBER_SIZE 3) */
static int g_ai = 1, g_ai_food_reserve = 1500, g_ai_oil_reserve = 1000, g_ai_city_damage = 45, g_ai_max_defenders = 4,
           g_ai_range_tiles = 80;

/* ---- game access ---------------------------------------------------------------------- */

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn3_t)(void *self, int a, int b, int c);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);
typedef int(__attribute__((thiscall)) *fn5_t)(void *self, int a, int b, int c, int d, int e);
typedef int(__attribute__((thiscall)) *fn6_t)(void *self, int a, int b, int c, int d, int e, int f);
typedef int(__attribute__((thiscall)) *fn7_t)(void *self, int a, int b, int c, int d, int e, int f, int g);

static void *g_orig_make_options, *g_orig_get_caster, *g_orig_get_vcaster, *g_orig_validate, *g_orig_action,
    *g_orig_select_add, *g_orig_objects, *g_orig_target, *g_orig_follows;

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

/* A finished Airbase (not one still being built). */
static int is_airbase(char *o)
{
    return o && type_of(o) == AIRBASE && ((fn0_t)VA(VA_ACTIVE_BUILD))(o);
}
static int ox(char *o) { return *(int *)(o + OBJ_X) ^ COORD_KEY; }
static int oy(char *o) { return *(int *)(o + OBJ_Y) ^ COORD_KEY; }

static long long dist2(int x0, int y0, int x1, int y1)
{
    long long dx = x0 - x1, dy = y0 - y1;
    return dx * dx + dy * dy;
}

static int units_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_UNITS_N + 4 * p); }
static int builds_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_BUILD_N + 4 * p); }

static int stock(int p, int r) { return (*(int **)(leader(p) + LD_SECURE))[r] ^ STOCK_KEY; }

static void add_stock(int p, int r, int v)
{
    int *s = *(int **)(leader(p) + LD_SECURE);
    s[r] = (stock(p, r) + v) ^ STOCK_KEY;
}

static int is_human(int p) { return *(int *)(leader(p) + LD_FLAGS) & 4; }
static int has_info_age(int p) { return ((fn1_t)VA(VA_HAS_TECH))(leader(p), INFORMATION_AGE) != 0; }

static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }

static int on_map(int x, int y)
{
    int *w = *(int **)VA(VA_WORLD);
    return x >= 0 && y >= 0 && x < w[0] * 768 && y < w[1] * 768;
}

static int ground_z(int x, int y)
{
    int *div3 = *(int **)VA(VA_DIV3);
    int z = 0;
    ((fn4_t)VA(VA_FIND_Z))(*(void **)VA(VA_TERRAIN), (int)&z, div3[x >> 6], div3[y >> 6], 0);
    return z;
}

static char *guy_of(char *unit)
{
    char **g = *(char ***)(unit + OBJ_GUY);
    return g ? *g : NULL;
}

/* ---- cooldowns and missions --------------------------------------------------------- */

#define MAX_BASES    64
#define MAX_MISSIONS 32

static struct {
    int player, id, ready; /* game time from which the Airbase may launch again */
} g_cool[MAX_BASES];

enum { M_FREE, M_OUT, M_DOWN, M_HOLD, M_UP, M_BACK, M_LAND, M_STOW };

static struct {
    int state, player, heli, base, tx, ty, t0, last_order;
    int ground, air; /* model heights: the ground under it, its flying height when it began to land */
} g_m[MAX_MISSIONS];

/* phases of the landing, in frames (1/15 s) */
#define T_DOWN  30
#define T_HOLD  12
#define T_UP    30
#define WHEELS  6 /* chinook.bh3: the wheels are 6 under the model's origin */
#define T_STOW  45 /* longest wait for it to be inside its Airbase */
#define ARRIVE  (TILE * 3 / 2)
#define REORDER 45

static int cooldown_left(int p, int base)
{
    for (int i = 0; i < MAX_BASES; i++)
        if (g_cool[i].ready && g_cool[i].player == p && g_cool[i].id == base)
            return g_cool[i].ready > now() ? g_cool[i].ready - now() : 0;
    return 0;
}

static void start_cooldown(int p, int base)
{
    int slot = 0, t = now();
    for (int i = 0; i < MAX_BASES; i++) {
        if (g_cool[i].ready && g_cool[i].player == p && g_cool[i].id == base) {
            slot = i;
            break;
        }
        if (g_cool[i].ready <= t)
            slot = i; /* a free or expired entry */
    }
    g_cool[slot].player = p;
    g_cool[slot].id = base;
    g_cool[slot].ready = t + g_cooldown_s * 15;
}

static int can_pay(int p) { return stock(p, RES_FOOD) >= g_cost_food && stock(p, RES_OIL) >= g_cost_oil; }

static int type_pop(int type)
{
    char *td = (*(char ***)VA(VA_TYPEDATA))[type];
    return td ? *(int *)(td + TD_POP) : 0;
}

/* Room under the population limit for the helicopter and the whole squad, which take it as the
   game's own units do (Unit::set_type adds a type's POP, the squad's leader counts for it). */
static int has_room(int p)
{
    int need = type_pop(HELI) + g_squad * type_pop(PARA);
    return *(int *)(leader(p) + LD_POP) + need <= *(int *)(leader(p) + LD_POP_CAP);
}

static int mission_count(int p)
{
    int n = 0;
    for (int i = 0; i < MAX_MISSIONS; i++)
        n += g_m[i].state != M_FREE && g_m[i].player == p;
    return n;
}

/* An Airbase of the group that has the ability, ready or not, or -1: GroupData::get_spellcaster's
   answer. The tooltip (Options::describe) shows the price only when there is one, and
   Options::do_spell does nothing at all without one. */
static int any_base(char *group)
{
    int p = (unsigned char)group[0x4a];
    if (p > 7 || !has_info_age(p))
        return -1;
    int n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n; i++) {
        char *o = object(p, ids[i]);
        if (is_airbase(o) && (unsigned char)o[OBJ_OWNER] == p)
            return ids[i];
    }
    return -1;
}

/* An Airbase of the group that can launch now, or -1: get_valid_spellcaster's answer; without
   one, Options::do_spell gives the game's red refusal and its sound. */
static int ready_base(char *group)
{
    int p = (unsigned char)group[0x4a];
    if (p > 7 || !has_info_age(p) || !can_pay(p) || !has_room(p))
        return -1;
    int n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n; i++) {
        char *o = object(p, ids[i]);
        if (is_airbase(o) && (unsigned char)o[OBJ_OWNER] == p && !cooldown_left(p, ids[i]))
            return ids[i];
    }
    return -1;
}

static void order_go(char *unit, int x, int y)
{
    ((fn5_t)VA(VA_GO_TO))(unit, x, y, 2, 0, 0x300);
}

/* Pays, makes the helicopter at the Airbase and sends it. Simulation. */
static int launch(int p, int base, int tx, int ty)
{
    char *b = object(p, base);
    if (!b || !can_pay(p) || !has_room(p))
        return 0;
    int slot = -1;
    for (int i = 0; i < MAX_MISSIONS && slot < 0; i++)
        if (g_m[i].state == M_FREE)
            slot = i;
    if (slot < 0)
        return 0;
    int id = ((fn7_t)VA(VA_INIT_UNIT))(*(void **)VA(VA_OBJECTS), p, HELI, ox(b), oy(b), -1, -1, -1);
    char *h = object(p, id);
    if (!h) {
        logf_("airdrop: player %d, the helicopter could not be made", p);
        return 0;
    }
    add_stock(p, RES_FOOD, -g_cost_food);
    add_stock(p, RES_OIL, -g_cost_oil);
    start_cooldown(p, base);
    memset(&g_m[slot], 0, sizeof g_m[slot]);
    g_m[slot].state = M_OUT;
    g_m[slot].player = p;
    g_m[slot].heli = id;
    g_m[slot].base = base;
    g_m[slot].tx = tx;
    g_m[slot].ty = ty;
    g_m[slot].last_order = now();
    order_go(h, tx, ty);
    logf_("airdrop: player %d, Airbase %d sends helicopter %d to (%d, %d)", p, base, id, tx / TILE, ty / TILE);
    return 1;
}

static void drop_squad(int slot, char *h)
{
    int p = g_m[slot].player;
    static const int off[6][2] = { { -1, 0 }, { 1, 0 }, { 0, 1 }, { 0, -1 }, { -1, 1 }, { 1, -1 } };
    int x = ox(h), y = oy(h), made = 0, first = -1;
    for (int i = 0; i < g_squad && i < 6; i++) {
        int id = ((fn7_t)VA(VA_INIT_UNIT))(*(void **)VA(VA_OBJECTS), p, PARA, x + off[i][0] * 110,
                                           y + off[i][1] * 110, -1, -1, -1);
        if (object(p, id)) {
            made++;
            if (first < 0)
                first = id;
        }
    }
    logf_("airdrop: player %d, %d Paratroopers down at (%d, %d)", p, made, x / TILE, y / TILE);
    /* a computer player's squad goes for the city it was dropped at */
    if (!is_human(p))
        for (int id = first; id >= 0 && id < units_n(p); id++) {
            char *u = object(p, id);
            if (u && type_of(u) == PARA && dist2(ox(u), oy(u), x, y) < (long long)ARRIVE * ARRIVE * 4)
                order_go(u, g_m[slot].tx, g_m[slot].ty);
        }
}

/* The Airbase it lands at: its own one, or the nearest one the player still has. */
static char *home(int slot, char *h)
{
    int p = g_m[slot].player;
    char *b = object(p, g_m[slot].base);
    if (is_airbase(b) && (unsigned char)b[OBJ_OWNER] == p)
        return b;
    long long best = -1;
    char *pick = NULL;
    for (int id = FIRST_BUILD; id < builds_n(p); id++) {
        char *o = object(p, id);
        if (!is_airbase(o) || (unsigned char)o[OBJ_OWNER] != p)
            continue;
        long long d = dist2(ox(o), oy(o), ox(h), oy(h));
        if (best < 0 || d < best) {
            best = d;
            pick = o;
            g_m[slot].base = id;
        }
    }
    return pick;
}

static void finish(int slot, char *h, int refund)
{
    int p = g_m[slot].player;
    if (h)
        ((fn1_t)VA(VA_DISBAND))(h, 0);
    if (refund) {
        add_stock(p, RES_FOOD, g_cost_food * g_refund_pct / 100);
        add_stock(p, RES_OIL, g_cost_oil * g_refund_pct / 100);
    }
    logf_("airdrop: player %d, helicopter %d %s", p, g_m[slot].heli,
          refund ? "back at its Airbase, refund paid" : h ? "removed" : "lost");
    g_m[slot].state = M_FREE;
}

static void begin_landing(int slot, char *h, int state)
{
    char *guy = guy_of(h);
    g_m[slot].state = state;
    g_m[slot].t0 = now();
    g_m[slot].ground = ground_z(ox(h), oy(h)) + WHEELS;
    g_m[slot].air = guy ? *(int *)(guy + GUY_Z) : g_m[slot].ground;
}

/* After the air physics: the model's height while landing, eased in and out. Integers only,
   the same on every machine. */
static void set_height(int slot, char *h)
{
    int s = g_m[slot].state;
    char *guy = guy_of(h);
    if (!guy || (s != M_DOWN && s != M_HOLD && s != M_UP && s != M_LAND))
        return;
    int len = s == M_UP ? T_UP : T_DOWN, dt = now() - g_m[slot].t0;
    int f = s == M_HOLD ? 1024 : dt >= len ? 1024 : dt * 1024 / len; /* 0 .. 1024 */
    if (s == M_UP)
        f = 1024 - f;
    long long e = (long long)f * f * (3 * 1024 - 2 * f) / (1024 * 1024); /* smoothstep */
    int span = g_m[slot].air - g_m[slot].ground;
    if (span > 0)
        *(int *)(guy + GUY_Z) = g_m[slot].air - (int)(span * e / 1024);
}

static void step(int slot)
{
    int p = g_m[slot].player, t = now();
    char *h = object(p, g_m[slot].heli);
    if (!h || type_of(h) != HELI) {
        finish(slot, NULL, 0); /* shot down */
        return;
    }
    int x = ox(h), y = oy(h);
    switch (g_m[slot].state) {
    case M_OUT:
        if (dist2(x, y, g_m[slot].tx, g_m[slot].ty) <= (long long)ARRIVE * ARRIVE) {
            begin_landing(slot, h, M_DOWN);
        } else if (t - g_m[slot].last_order >= REORDER) { /* keep it on course */
            order_go(h, g_m[slot].tx, g_m[slot].ty);
            g_m[slot].last_order = t;
        }
        break;
    case M_DOWN:
        if (t - g_m[slot].t0 >= T_DOWN) {
            drop_squad(slot, h);
            g_m[slot].state = M_HOLD;
            g_m[slot].t0 = t;
        }
        break;
    case M_HOLD:
        if (t - g_m[slot].t0 >= T_HOLD) {
            g_m[slot].state = M_UP;
            g_m[slot].t0 = t;
        }
        break;
    case M_UP:
        if (t - g_m[slot].t0 >= T_UP) {
            g_m[slot].state = M_BACK;
            g_m[slot].last_order = t - REORDER;
        }
        break;
    case M_BACK: {
        char *b = home(slot, h);
        if (!b) {
            finish(slot, h, 0); /* no Airbase left to land at */
            return;
        }
        if (dist2(x, y, ox(b), oy(b)) <= (long long)ARRIVE * ARRIVE) {
            begin_landing(slot, h, M_LAND);
        } else if (t - g_m[slot].last_order >= REORDER) {
            order_go(h, ox(b), oy(b));
            g_m[slot].last_order = t;
        }
        break;
    }
    case M_LAND:
        if (t - g_m[slot].t0 >= T_DOWN) {
            /* into the hangar, like a new aircraft (Build::train): removed once it is inside, so
               nothing is seen to die; Object::disband on a unit out on the map plays its death */
            char *b = home(slot, h);
            if (b)
                ((fn3_t)VA(VA_GO_INSIDE))(h, *(short *)(b + OBJ_ID), p, 0);
            g_m[slot].state = M_STOW;
            g_m[slot].t0 = t;
        }
        break;
    case M_STOW: {
        int inside = !((fn0_t)(*(void ***)h)[VT_IS_ON_MAP / 4])(h);
        if (inside || t - g_m[slot].t0 >= T_STOW) {
            finish(slot, h, home(slot, h) != NULL);
            return;
        }
        break;
    }
    }
    set_height(slot, h);
}

/* Helicopters with no mission (a loaded game) are removed. */
static void sweep(void)
{
    for (int p = 0; p < 8; p++)
        for (int id = 0; id < units_n(p); id++) {
            char *u = object(p, id);
            if (!u || type_of(u) != HELI)
                continue;
            int known = 0;
            for (int i = 0; i < MAX_MISSIONS; i++)
                known |= g_m[i].state != M_FREE && g_m[i].player == p && g_m[i].heli == id;
            if (!known) {
                logf_("airdrop: player %d, helicopter %d has no mission, removed", p, id);
                ((fn1_t)VA(VA_DISBAND))(u, 0);
            }
        }
}

/* ---- computer players ------------------------------------------------------------------ */

static int hits_left_pct(char *o)
{
    int max = ((fn1_t)(*(void ***)o)[VT_MAX_HITS / 4])(o, 0);
    if (max <= 0)
        return 100;
    int left = max - *(int *)(o + OBJ_DAMAGE);
    return left <= 0 ? 0 : left * 100 / max;
}

static int defenders(int q, int x, int y)
{
    int n = 0;
    long long r2 = (long long)(8 * TILE) * (8 * TILE);
    for (int id = 0; id < units_n(q); id++) {
        char *u = object(q, id);
        if (!u)
            continue;
        int t = type_of(u);
        if ((t < CITIZEN_FIRST || t > CITIZEN_LAST) && dist2(ox(u), oy(u), x, y) <= r2)
            n++;
    }
    return n;
}

/* An opportunity: an enemy city bombed down to under half, with few soldiers around, within
   reach of a ready Airbase; only with plenty of food and oil to spare, one mission at a time. */
static void ai_think(int p)
{
    if (is_human(p) || !has_info_age(p) || mission_count(p))
        return;
    if (stock(p, RES_FOOD) < g_cost_food + g_ai_food_reserve || stock(p, RES_OIL) < g_cost_oil + g_ai_oil_reserve)
        return;
    long long range2 = (long long)(g_ai_range_tiles * TILE) * (g_ai_range_tiles * TILE);
    int best_base = -1, best_x = 0, best_y = 0;
    long long best = -1;
    for (int bid = FIRST_BUILD; bid < builds_n(p); bid++) {
        char *b = object(p, bid);
        if (!is_airbase(b) || cooldown_left(p, bid))
            continue;
        for (int q = 0; q < 8; q++) {
            if (q == p || !((fn1_t)VA(VA_IS_ENEMY))(leader(p), q))
                continue;
            for (int cid = FIRST_BUILD; cid < builds_n(q); cid++) {
                char *c = object(q, cid);
                if (!c || type_of(c) < CITY_FIRST || type_of(c) > CITY_LAST)
                    continue;
                if (hits_left_pct(c) > 100 - g_ai_city_damage)
                    continue;
                long long d = dist2(ox(b), oy(b), ox(c), oy(c));
                if (d > range2 || (best >= 0 && d >= best))
                    continue;
                if (defenders(q, ox(c), oy(c)) > g_ai_max_defenders)
                    continue;
                best = d;
                best_base = bid;
                best_x = ox(c);
                best_y = oy(c);
            }
        }
    }
    if (best_base >= 0 && launch(p, best_base, best_x, best_y))
        logf_("airdrop: computer player %d drops on a damaged city", p);
}

/* Interface only: drops the local player has ordered whose command has not yet run in the
   simulation (the network delay), so the charges and the re-armed targeting do not count an
   Airbase twice; and a request to start the targeting again on the next frame. */
static int g_pending, g_rearm;
static DWORD g_pending_ms;

/* ---- hooks: simulation ------------------------------------------------------------------ */

__attribute__((used)) int c_ad_objects(void *self)
{
    ((fn0_t)g_orig_objects)(self); /* the units move first, the air physics set their heights */
    entrench_frame();              /* entrench.c shares this hook */
    dc_frame();                    /* and datacenter.c (the Expansions' technician) */
    ranks_frame();                 /* and ranks.c (healing) */
    heal_frame();                  /* and heal.c (the General's Heal) */
    nukefx_frame();                /* and nukefx.c (interceptors, radiation) */
    nonukes_frame();               /* and nonukes.c (the chosen Armageddon count) */
    romania_frame();               /* and romania.c (unit names by age) */
    cityrepair_frame();            /* and cityrepair.c (Large and Major Cities repair) */
    mem_frame();                   /* and ron_camera.c (memory in the log) */
    if (!g_airdrop_enabled)
        return 0;
    static int last = -1;
    int t = now();
    if (t < last || t - last > 15) { /* another game, or a saved game loaded: old cooldowns and */
        memset(g_cool, 0, sizeof g_cool); /* missions would hold back or steer the new units */
        memset(g_m, 0, sizeof g_m);
    }
    last = t;
    for (int i = 0; i < MAX_MISSIONS; i++)
        if (g_m[i].state != M_FREE)
            step(i);
    if (t % 150 == 0)
        sweep();
    if (g_ai)
        for (int p = 0; p < 8; p++)
            if ((t + p * 17) % 150 == 0 && *(int *)(leader(p) + 8) == p)
                ai_think(p);
    return 0;
}

__attribute__((used)) int c_ad_get_caster(char *group, int *args)
{
    if (entrench_is_craft(args[0]))
        return entrench_caster(group, args[0]);
    if (g_airdrop_enabled && args[0] == SPELL_PARADROP)
        return any_base(group);
    return ((fn1_t)g_orig_get_caster)(group, args[0]);
}

__attribute__((used)) int c_ad_get_vcaster(char *group, int *args)
{
    if (entrench_is_craft(args[0]))
        return entrench_caster(group, args[0]);
    if (g_airdrop_enabled && args[0] == SPELL_PARADROP)
        return ready_base(group);
    return ((fn1_t)g_orig_get_vcaster)(group, args[0]);
}

__attribute__((used)) int c_ad_validate(char *group, int *a)
{
    if (entrench_is_craft(a[0]))
        return entrench_validate(group, a);
    if (!g_airdrop_enabled || a[0] != SPELL_PARADROP)
        return ((fn5_t)g_orig_validate)(group, a[0], a[1], a[2], a[3], a[4]);
    return ready_base(group) >= 0 && on_map(a[3], a[4]) ? 0 : 1;
}

__attribute__((used)) int c_ad_action(char *group, int *a)
{
    if (entrench_is_craft(a[0])) {
        entrench_action(group, a);
        return 0;
    }
    if (!g_airdrop_enabled || a[0] != SPELL_PARADROP)
        return ((fn5_t)g_orig_action)(group, a[0], a[1], a[2], a[3], a[4]);
    int base = ready_base(group);
    if (base >= 0 && on_map(a[3], a[4]))
        launch((unsigned char)group[0x4a], base, a[3], a[4]);
    if ((unsigned char)group[0x4a] == local_player() && g_pending > 0)
        g_pending--; /* the interface's count only, the same on every machine is not needed */
    return 0;
}

/* ---- hooks: interface -------------------------------------------------------------------- */

__attribute__((used)) int c_ad_make_options(char *self)
{
    ((fn0_t)g_orig_make_options)(self);
    entrench_make_options(self); /* entrench.c shares this hook */
    dc_make_options(self);       /* and datacenter.c (the Data Center's Expansion buttons) */
    nukefx_make_options(self);   /* and nukefx.c (the silo's Interceptor button) */
    cityrepair_make_options(self); /* and cityrepair.c (the city's repair switch) */
    int p = (unsigned char)self[OBJ_OWNER];
    if (!g_airdrop_enabled || p > 7 || p != local_player() || !is_airbase(self) || !has_info_age(p)) {
        airstrike_make_options(self); /* and airstrike.c (the Airbase's strike button) */
        return 0;
    }
    int id = *(short *)(self + OBJ_ID);
    /* never disabled: the picture stays clear under panel.c's cooldown clock, and a click before
       it is ready gets the game's own refusal */
    int off = 0;
    /* top row, after the fighter, bomber and helicopter (columns 0..2) */
    ((fn6_t)VA(VA_OPTIONS_ADD))(*(void **)VA(VA_OPTIONS), OPT_SPELL, AD_COL, AD_ROW, SPELL_PARADROP, off, 0);
    static int logged;
    if (!logged++)
        logf_("airdrop: button added to Airbase %d of player %d (%s)", id, p, off ? "disabled" : "ready");
    airstrike_make_options(self); /* after the Airdrop, so it takes another cell */
    return 0;
}

__attribute__((used)) int c_ad_select_add(char *self, int *a)
{
    char *o = object(a[1], a[0]);
    if (o && ((g_airdrop_enabled && type_of(o) == HELI) || dc_is_technician(type_of(o))))
        return 0; /* the transport and the Data Center's technician are not the player's to command */
    int r = ((fn3_t)g_orig_select_add)(self, a[0], a[1], a[2]);
    bigsel_after_add(self, a[0], a[1]); /* bigsel.c: what no longer fits */
    return r;
}

/* For panel.c: is this option the Airdrop button, and how much of the cooldown is left for the
   local player's selected Airbases (0 = one is ready, 1000 = just launched). Reads only. */
int airdrop_is_option(int code, int type)
{
    return g_airdrop_enabled && code == OPT_SPELL && type == SPELL_PARADROP;
}

int airdrop_cooldown_permille(void)
{
    char *console = *(char **)VA(VA_CONSOLE);
    int p = local_player(), total = g_cooldown_s * 15, best = -1;
    if (!console || p < 0 || p > 7 || total <= 0)
        return 0;
    char *group = *(char **)VA(VA_SELECTION) + *(int *)(console + 0x2a0) * GROUP_SIZE;
    int n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n; i++) {
        char *o = object(p, ids[i]);
        if (!is_airbase(o))
            continue;
        int left = cooldown_left(p, ids[i]);
        if (best < 0 || left < best)
            best = left;
    }
    return best <= 0 ? 0 : best >= total ? 1000 : best * 1000 / total;
}

static char *local_group(void)
{
    char *console = *(char **)VA(VA_CONSOLE);
    return console ? *(char **)VA(VA_SELECTION) + *(int *)(console + 0x2a0) * GROUP_SIZE : NULL;
}

/* Drops the local player can call now with the selection: Airbases off cooldown, as far as the
   food, the oil and the population room go, less the drops already ordered. *bases = how many
   Airbases are selected. Reads only. */
int airdrop_charges(int *bases)
{
    char *group = local_group();
    int p = local_player(), n_bases = 0, ready = 0;
    if (bases)
        *bases = 0;
    if (!group || p < 0 || p > 7 || (unsigned char)group[0x4a] != p)
        return 0;
    int n = *(int *)(group + 0xc);
    short *ids = (short *)(group + 0x8cc);
    for (int i = 0; i < n; i++) {
        char *o = object(p, ids[i]);
        if (!is_airbase(o) || (unsigned char)o[OBJ_OWNER] != p)
            continue;
        n_bases++;
        ready += !cooldown_left(p, ids[i]);
    }
    if (bases)
        *bases = n_bases;
    if (!has_info_age(p))
        return 0;
    if (g_pending && GetTickCount() - g_pending_ms > 5000)
        g_pending = 0; /* a command that never came back (a game paused, ...) */
    int c = ready;
    if (g_cost_food > 0 && stock(p, RES_FOOD) / g_cost_food < c)
        c = stock(p, RES_FOOD) / g_cost_food;
    if (g_cost_oil > 0 && stock(p, RES_OIL) / g_cost_oil < c)
        c = stock(p, RES_OIL) / g_cost_oil;
    int need = type_pop(HELI) + g_squad * type_pop(PARA);
    int room = *(int *)(leader(p) + LD_POP_CAP) - *(int *)(leader(p) + LD_POP);
    if (need > 0 && room / need < c)
        c = room / need;
    c -= g_pending;
    return c > 0 ? c : 0;
}

/* Is the local player aiming the Airdrop: the targeting is on (Options +0x24, cleared by
   Options::reset) for the pending option (+0x7C code, +0x80 type) of the Airdrop craft. */
static int aiming(char *opts)
{
    return opts && *(int *)(opts + 0x24) && *(int *)(opts + 0x7C) == OPT_SPELL &&
           *(int *)(opts + 0x80) == SPELL_PARADROP;
}

/* Once per rendered frame (ron_camera.c, Camera::check_scroll): after a drop was ordered, the
   targeting starts again while drops are left, as if the button had been clicked once more. */
void airdrop_ui_tick(void)
{
    if (!g_rearm)
        return;
    g_rearm = 0;
    char *opts = *(char **)VA(VA_OPTIONS);
    if (!opts || *(int *)(opts + 0x24) || *(int *)(opts + 0x7C) != OPT_SPELL ||
        *(int *)(opts + 0x80) != SPELL_PARADROP || airdrop_charges(NULL) <= 0)
        return;
    ((fn2_t)VA(VA_DO_SPELL))(opts, SPELL_PARADROP, 0);
}

/* The click that orders a drop (interface): counted as pending, and the targeting re-armed. */
__attribute__((used)) int c_ad_target(char *opts, int *a)
{
    char *group = local_group();
    int ours = g_airdrop_enabled && a[0] == SPELL_PARADROP && group && !(*(unsigned char *)(game() + 0x820) & 0x10) &&
               ready_base(group) >= 0 && on_map(a[3], a[4]);
    ((fn5_t)g_orig_target)(opts, a[0], a[1], a[2], a[3], a[4]);
    if (ours) {
        g_pending++;
        g_pending_ms = GetTickCount();
        g_rearm = 1;
    }
    return 0;
}

/* draw_circle's own convention: radius in ecx, CircleType in edx, cdecl stack. */
void ad_draw_circle(int x, int y, int z, int radius, const void *color);
UINT_PTR g_va_draw_circle;
__asm__(".intel_syntax noprefix\n"
        ".globl _ad_draw_circle\n"
        "_ad_draw_circle:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    push esi\n"
        "    push edi\n"
        "    push ebx\n"
        "    call _circle_batch_room\n" /* rally.c */
        "    push 0\n"  /* float 0 */
        "    push 0\n"
        "    push 1\n"
        "    push -1\n" /* -1: the default line width */
        "    mov esi, [ebp+24]\n"
        "    push dword ptr [esi+8]\n" /* Color, 12 bytes */
        "    push dword ptr [esi+4]\n"
        "    push dword ptr [esi]\n"
        "    push dword ptr [ebp+16]\n" /* z */
        "    push dword ptr [ebp+12]\n" /* y */
        "    push dword ptr [ebp+8]\n"  /* x */
        "    mov ecx, [ebp+20]\n"       /* radius */
        "    xor edx, edx\n"            /* CircleType 0 */
        "    call dword ptr [_g_va_draw_circle]\n"
        "    add esp, 40\n"
        "    pop ebx\n"
        "    pop edi\n"
        "    pop esi\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

/* While aiming: where the squad lands, as rings on the ground under the cursor in the player's
   colour (like the territory circle of a city being placed): the landing zone and its centre. */
__attribute__((used)) int c_ad_follows(char *opts)
{
    ((fn0_t)g_orig_follows)(opts);
    placement_draw_ghosts(opts, g_orig_follows); /* placement.c: a Tank Trap line */
    int p = local_player();
    if (!g_airdrop_enabled || !aiming(opts) || p < 0 || p > 7)
        return 0;
    int x = *(int *)(opts + 0xb4), y = *(int *)(opts + 0xb8), z = *(int *)(opts + 0xbc);
    if (!on_map(x, y))
        return 0;
    unsigned char color[12] = { 0 };
    void *tc = (char *)VA(VA_TEAM_COLORS) + 0x64 * *(unsigned char *)(leader(p) + 0x6928);
    ((fn1_t)VA(VA_TC_GET_UNIT))(tc, (int)color);
    unsigned rgb = *(unsigned *)color & 0xFFFFFF;
    *(unsigned *)color = rgb | 0xD0000000u;
    ad_draw_circle(x, y, z, 2 * TILE, color);
    *(unsigned *)color = rgb | 0x70000000u;
    ad_draw_circle(x, y, z, TILE / 2, color);
    return 0;
}

/* ---- install ------------------------------------------------------------------------------ */

void hk_ad_objects(void);
void hk_ad_get_caster(void);
void hk_ad_get_vcaster(void);
void hk_ad_validate(void);
void hk_ad_action(void);
void hk_ad_make_options(void);
void hk_ad_select_add(void);
void hk_ad_target(void);
void hk_ad_follows(void);

/* thiscall wrappers: ecx = this; the C function gets (this, pointer to the stack arguments) */
#define AD_STUB(name, cfn, ret)                                                              \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    lea eax, [ebx+36]\n"                                                               \
    "    push eax\n"                                                                        \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

__asm__(".intel_syntax noprefix\n"
        AD_STUB("hk_ad_objects", "c_ad_objects", "ret")
        AD_STUB("hk_ad_get_caster", "c_ad_get_caster", "ret 4")
        AD_STUB("hk_ad_get_vcaster", "c_ad_get_vcaster", "ret 4")
        AD_STUB("hk_ad_validate", "c_ad_validate", "ret 0x14")
        AD_STUB("hk_ad_action", "c_ad_action", "ret 0x14")
        AD_STUB("hk_ad_make_options", "c_ad_make_options", "ret")
        AD_STUB("hk_ad_select_add", "c_ad_select_add", "ret 0xc")
        AD_STUB("hk_ad_target", "c_ad_target", "ret 0x14")
        AD_STUB("hk_ad_follows", "c_ad_follows", "ret")
        ".att_syntax prefix\n");

void airdrop_load_config(const char *dir)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%sairdrop.ini", dir);
    g_airdrop_enabled = GetFileAttributesA(ini) != INVALID_FILE_ATTRIBUTES;
    if (!g_airdrop_enabled)
        return;
    g_cost_food = GetPrivateProfileIntA("airdrop", "cost_food", g_cost_food, ini);
    g_cost_oil = GetPrivateProfileIntA("airdrop", "cost_oil", g_cost_oil, ini);
    g_refund_pct = GetPrivateProfileIntA("airdrop", "refund_percent", g_refund_pct, ini);
    g_cooldown_s = GetPrivateProfileIntA("airdrop", "cooldown_seconds", g_cooldown_s, ini);
    g_squad = GetPrivateProfileIntA("airdrop", "squad", g_squad, ini);
    g_ai = GetPrivateProfileIntA("ai", "enabled", g_ai, ini);
    g_ai_food_reserve = GetPrivateProfileIntA("ai", "food_reserve", g_ai_food_reserve, ini);
    g_ai_oil_reserve = GetPrivateProfileIntA("ai", "oil_reserve", g_ai_oil_reserve, ini);
    g_ai_city_damage = GetPrivateProfileIntA("ai", "city_damage_percent", g_ai_city_damage, ini);
    g_ai_max_defenders = GetPrivateProfileIntA("ai", "max_defenders", g_ai_max_defenders, ini);
    g_ai_range_tiles = GetPrivateProfileIntA("ai", "range_tiles", g_ai_range_tiles, ini);
}

/* Objects::process_all, BuildOut::make_options and the four spell hooks are shared with
   entrench.c (its buttons are crafts), and process_all, make_options and SelectGroup::add with
   datacenter.c (the Expansions' technician), process_all with ranks.c and heal.c (healing), and
   make_options with airstrike.c, draw_cursor_follows with placement.c (the ghosts of a Tank Trap
   line), so they are installed when any of them is on. */
void airdrop_install(void)
{
    if (!g_airdrop_enabled && !g_entrench_enabled && !g_dc_enabled && !g_ranks_enabled && !g_heal_enabled &&
        !g_nuke_sim_enabled && !airstrike_wanted() && !g_romania_enabled && !cityrepair_wanted() &&
        !g_bigsel_enabled && !g_ttline_enabled)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[8];
        int sig_len, steal;
        void *hook;
        void **orig;
        int shared;
    } hooks[] = {
        /* push ebx / mov ebx,esp / sub esp,8 */
        { "Objects::process_all", VA_OBJ_PROCESS, { 0x53, 0x8B, 0xDC, 0x83, 0xEC, 0x08 }, 6, 6, hk_ad_objects,
          &g_orig_objects, 1 },
        /* push ebp / mov ebp,esp / sub esp,0x18 */
        { "get_spellcaster", VA_GET_CASTER, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18 }, 6, 6, hk_ad_get_caster,
          &g_orig_get_caster, 1 },
        /* push ebp / mov ebp,esp / sub esp,0x14 */
        { "get_valid_spellcaster", VA_GET_VCASTER, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14 }, 6, 6, hk_ad_get_vcaster,
          &g_orig_get_vcaster, 1 },
        /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
        { "validate_spell", VA_VALIDATE_SPELL, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_ad_validate,
          &g_orig_validate, 1 },
        { "action_spell", VA_ACTION_SPELL, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_ad_action,
          &g_orig_action, 1 },
        /* push ebp / mov ebp,esp / sub esp,0x20 */
        { "BuildOut::make_options", VA_MAKE_OPTIONS, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20 }, 6, 6,
          hk_ad_make_options, &g_orig_make_options, 1 },
        { "SelectGroup::add", VA_SELECT_ADD, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5, hk_ad_select_add,
          &g_orig_select_add, 1 },
        /* push ebx / mov ebx,esp / sub esp,8 */
        { "Options::target_spell", VA_TARGET_SPELL, { 0x53, 0x8B, 0xDC, 0x83, 0xEC, 0x08 }, 6, 6, hk_ad_target,
          &g_orig_target, 0 },
        /* push ebp / mov ebp,esp / sub esp,0x28 */
        { "Options::draw_cursor_follows", VA_CURSOR_FOLLOWS, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x28 }, 6, 6,
          hk_ad_follows, &g_orig_follows, 1 },
    };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if ((g_airdrop_enabled || hooks[i].shared) &&
            memcmp((void *)VA(hooks[i].va), hooks[i].sig, hooks[i].sig_len) != 0) {
            logf_("airdrop: signature mismatch at %s, airdrop and entrenchment turned off", hooks[i].name);
            g_airdrop_enabled = 0;
            g_entrench_enabled = 0;
            return;
        }
    g_va_draw_circle = VA(VA_DRAW_CIRCLE);
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    for (int i = 0; i < n; i++) {
        if (!g_airdrop_enabled && !hooks[i].shared)
            continue;
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    if (g_airdrop_enabled)
        logf_("airdrop: on (%d Food + %d Oil, %d%% back, %d s cooldown, %d Paratroopers; computer players %s)",
              g_cost_food, g_cost_oil, g_refund_pct, g_cooldown_s, g_squad, g_ai ? "on" : "off");
}
