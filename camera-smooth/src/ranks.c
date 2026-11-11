/*
 * Veteran ranks: military units earn experience from the damage they deal and rise through 10
 * levels in 3 tiers (bronze, silver, gold; level 10 is gold with a star). One selected unit shows
 * its rank, a large experience bar, its kills and its bonuses in the selection panel's right half
 * (ranks_draw_panel). Each level gives +2 % damage, +3 % hit points and +3 % attack rate (the
 * user, 2026-11-10: armor is taken off every hit as a flat amount, tools/COMBAT_NOTES.md, so a
 * percent on the attack beat armor by more than its percent; the attack rate scales fairly);
 * level 3 +1 line of sight, 6 -10 % damage taken, 7 +1 range (ranged units), 8 slow healing out
 * of combat, 10 -20 % damage taken. [ranks] xp_gain_percent scales every gain (115 = 15 % faster). The rank stays when the unit's line is
 * upgraded. A selected or hovered unit shows its rank as 1-3 small bars beside its health bar;
 * its portrait in the selection panel shows the bars in the top-right corner and a yellow experience
 * bar along the bottom; its tooltip (portrait or map) names the rank, the progress to the next
 * one and the bonuses it has now.
 *
 * Units without a weapon earn experience from their abilities instead (SpellType::cast): a Spy's
 * (or the Senator's) successful Bribe is worth the bribed unit's value, as a kill; any other
 * ability that costs mana (a General's Decoys, Ambush, Forced March, Entrench; a Spy's
 * Counterintelligence, Informer) gives [ranks] ability_xp_percent of the caster's own cost per
 * 1000 mana.
 *
 * Defensive buildings (every building whose type attacks: Cities, Towers, Forts, Lookouts, air
 * defence, the Red Fort and the Forbidden City) rank up the same way from the damage they deal,
 * against [ranks] building_min_cost when their own base cost is lower (a City's is 20). They get
 * the same bonuses (no healing); hit points through Wall::update_hits, the attack rate where
 * Build::do_attack and Object::do_launch read the type's RECHARGE (+0x1F4) directly, sight
 * through WallData::los, range through BuildData::max_range. Their experience is the byte
 * WallData +0x65 ("demolition" in WallData::log_data: zeroed by Wall::init and never read for a
 * building; saved by WallData::walk_data with +0x48..+0x66) in units of 16; the remainder below
 * 16 is kept in memory only.
 *
 * Experience: a hit worth `applied` hit points on a victim gives
 *     applied x (victim's cost / its squad size) / victim's max hits
 * in units of 1/256 of the attacker's own cost (cost = the type's base COST, Type +0x18, six
 * resources), so a kill is worth the victim's value. A squad ("uber unit": separate soldier
 * objects, see entrenchment/ENGINE_NOTES.md) shares one count: every soldier's hits add to it and
 * every soldier carries it.
 *
 * Where it lives: bits 20-31 of the unit flags +0x6C, which no code of the game touches (checked
 * over the whole exe, camera-smooth/ENGINE_NOTES.md "Veteran ranks"). Unit::walk_data saves
 * +0x48..+0xB7 as raw bytes, so the experience is in saved games, and it is part of the
 * simulation, the same on every machine.
 *
 * Hooks: Object::take_damage (kills.c: experience, damage taken), ObjectData::get_damage,
 * UnitData::recharge and UnitData::max_range (entrench.c), Objects::process_all (airdrop.c:
 * healing), IFaceButton::on_redraw (panel.c: the portrait); here Unit::update_hits (hit points),
 * UnitData::los, the call of GraphicPieces::draw_health_bar in UnitOut::draw_health_bar(int, int)
 * (the insignia), SpellType::cast (abilities), Options::describe(Option *) (the tooltip) and
 * the attack() calls of IFaceSelected::draw_stat (the panel shows the attack bonus as "48+13").
 * The rules change the game itself: every player needs the same setting ([gameplay] ranks).
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_UNIT_HITS    0x0060E930u /* Unit::update_hits(int), virtual +0x15C, ret 4 */
#define VA_UNIT_LOS     0x006100C0u /* UnitData::los() const, virtual +0x128 */
#define VA_HB_CALL      0x00613CA0u /* UnitOut::draw_health_bar(int, int): call GraphicPieces::draw_health_bar */
#define VA_GP_BAR       0x00908650u /* GraphicPieces::draw_health_bar(rect *, float (xmm2), Color &, Color &), ret 0xC */
#define VA_GRAPHIC_PIECES 0x00C06214u
#define VA_BLACK        0x00C8D260u
#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object ** */
#define VA_GAME         0x00C061ECu /* Game *: +0x550 = the frame (15 a second) */
#define VA_CAST         0x00676CE0u /* SpellType::cast(caster id, caster player, target id, target player), ret 0x10 */
#define VA_IS_CASTABLE  0x00675BC0u /* SpellTypeData::is_castable(id, player, 1) const: 3 = it is cast, ret 0xC */
#define VA_DESCRIBE     0x00724460u /* Options::describe(Option *, int, int, int), ret 0x10 */
#define VA_MAX_RANGE    0x0060FE90u /* UnitData::max_range() const (entrench.c's hook adds the rank) */
#define VA_SCENE        0x00C0620Cu /* Scene *: +0x104 = the tooltip's lines (StringList) */
#define VA_LIST_ADD     0x00A29420u /* StringList::add(const String &, const String &, int, int, int), ret 0x14 */
#define VA_EMPTY_STRING 0x00EB437Cu
#define VA_STRING_CTOR  0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE 0x00A1CF40u
#define VA_MOUSEOVER    0x00C06230u /* MouseOver *: +4 bit 4 object, +0x4C player, +0x58 id */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x2A0 the local selection's index */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each: +0xC count, +0x4A player, +0x8CC ids */
#define VA_IFACE_RM     0x00EBD1C0u /* IFaceRenderManager */
#define VA_RM_ADD_RECT  0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], 0xAABBGGRR, type) */
#define VA_GRAPHSYS     0x00E335A0u /* GraphSys *: +0x164 the depth of what is added (+2 per window) */
#define GS_DEPTH        0x164
#define VA_WHITE        0x00C8D26Cu
#define VA_WALL_HITS    0x0063F0D0u /* Wall::update_hits(int), buildings' virtual +0x15C, ret 4 */
#define VA_WALL_LOS     0x0063FA50u /* WallData::los() const: movsx eax, byte [ecx+0x3C] / ret */
#define VA_BUILD_RANGE  0x0062E470u /* BuildData::max_range() const */
#define VA_RECH_ATTACK  0x00622BF8u /* Build::do_attack: mov eax, [eax+0x1F4] (esi = the building) */
#define VA_RECH_LAUNCH  0x0064F79Du /* Object::do_launch: mov edi, [eax+0x1F4] (ecx = the object) */
/* IFaceSelected::draw_stat: call [reg+0x120], the object's attack(), for the "+N" after the base */
static const UINT_PTR g_shown_attack_at[4] = { 0x008217F9u, 0x0082185Bu, 0x00821AA1u, 0x00821B03u };

#define SPELL_TYPE   0x04
#define SPELL_MANA   0x1D0 /* mana the ability costs */
#define SPELL_BRIBE  0x275
#define SPELL_HEAL   0x279 /* "Rally!", the General's Heal (heal.c) */
#define OPT_GROUP_ME 0x98  /* tooltip of the selection panel's portrait (one unit of yours) */
#define OPT_GROUP    0xA7  /* ... of a unit of the selection by its index */
#define OPT_OBJECT   0xA9  /* ... of the object under the mouse (map or portrait) */
#define OPT_OBJECT2  0xAA
#define BTN_KIND     0x32C /* 3 = a unit */
#define BTN_OBJ_ID   0x35C
#define BTN_OBJ_P    0x360
#define BTN_FLAGS    0x3BC /* word; bit 3: the button is not drawn */
#define WIN_RECT     0x126 /* short left, right, top, bottom */
#define RENDER_SOLID 5

#define OBJ_FLAGS    0x08 /* bit 0: alive */
#define OBJ_OWNER    0x09
#define OBJ_ID       0x0A
#define OBJ_TYPE     0x18
#define OBJ_MAXHITS  0x20
#define OBJ_DAMAGE   0x24 /* hit points lost */
#define U_FLAGS      0x68 /* bit 0 = decoy */
#define U_FLAGS2     0x6C /* bits 20-31: experience (this mod) */
#define U_CAPTAIN    0x8E /* word: bit 15 = captain, else the soldier before it in the chain (the
                              captain for the first one only: UnitData::get_captain 0x00610AB0 walks
                              up the chain) */
#define U_NEXT       0x90 /* short: next soldier of the squad, -1 = last */
#define T_COST       0x18 /* int[6]: the base cost */
#define T_ATTACK     0x1E8
#define T_UBER       0x308 /* UBER_SIZE: soldiers per squad */
#define T_RECHARGE   0x1F4
#define T_CAT        0x14  /* CAT in unitrules.xml: 0 Foot, 1 Mounted, 2 Mech, 3 Artillery, 4 Command,
                              5 Civilian, 6 Sail, 7 Naval, 8 Air */
#define B_XP         0x65  /* buildings: experience / 16 (this mod) */
#define B_CONSTRUCT  0x54  /* buildings: construct_hits, the hit points so far built */
#define MAX_BUILDS   8192  /* building ids FIRST_BUILD .. FIRST_BUILD + MAX_BUILDS - 1 kept */
#define VT_MAX_HITS  0x11C
#define VT_UPDATE_HITS 0x15C
#define OBJS_UNITS_N 0x15C /* + 4 * p: unit ids 0 .. n-1 */
#define OBJS_BUILDS_END 0x184 /* + 4 * p: building ids FIRST_BUILD .. n-1 (cityrepair.c) */
#define FIRST_BUILD  2000
#define XP_SHIFT     20
#define XP_MAX       0xFFF
#define LEVELS       10
#define HEAL_LEVEL   8

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);

int g_ranks_enabled;
static int g_mana_bar = 1;
static int g_insignia_pct = 100; /* [ranks] insignia_size: the insignia beside the health bar */
static int g_attack_step = 2, g_hits_step = 3, g_rate_step = 3, g_heal_pct = 1, g_calm_s = 10, g_ability_pct = 35;
static int g_build_min_cost = 150, g_xp_pct = 115;
static void *g_orig_unit_hits, *g_orig_los, *g_orig_cast, *g_orig_describe, *g_orig_wall_hits, *g_orig_brange;
UINT_PTR g_rk_gp_bar;

/* experience (1/256 of the unit's cost) needed for levels 1..10; level 10 is the most the 12 bits
   hold (XP_MAX) */
static const int g_need[LEVELS + 1] = { 0, 128, 256, 512, 768, 1152, 1536, 2048, 2688, 3456, XP_MAX };
static const char *const g_name[LEVELS + 1] = { "Private", "Private First Class", "Corporal", "Sergeant",
                                                "Staff Sergeant", "Master Sergeant", "Lieutenant", "Captain",
                                                "Major", "Colonel", "Brigadier General" };
/* The Survival hero (survival.c, the user, 2026-11-10): one squad that ranks on to level
   HERO_LEVELS. Its 12 bits count HERO_XP_UNIT / 256 of its cost each (32760 / 256 at most), the
   rest below that is kept in memory. Levels 11-30: +2 % damage, +3 % hit points, +2 % attack rate
   each. Who it is: the ids of its soldiers, from the script (create_unit's id) and kept up to date
   once a second (a new captain when the first one falls); gone when the whole squad is. */
#define HERO_LEVELS  30
#define HERO_XP_UNIT 8
static int g_hero_need[HERO_LEVELS + 1];
/* the heroes, by slot: 4 * player + kind + 2 * k (survival.bhs: kind 0 an infantry squad, 1 a
   rider; k 0 or 1: every hero is two of them, the user, 2026-11-10: six soldiers or two riders),
   so every player of a co-op game can have one; the two halves (slots h and h + 2) keep the same
   experience (hero_refresh) */
#define MAX_HEROES 32
#define HERO_RIDER(slot) ((slot) >= 0 && ((slot) & 1))
#define HERO_SQUAD(slot) ((slot) >= 0 && !((slot) & 1))
static struct {
    int p, ids[32], n, rem; /* p < 0: none */
} g_heroes[MAX_HEROES] = { [0 ... MAX_HEROES - 1] = { -1 } };
static int is_hero(char *u);
static int hero_power(int l);

static int g_last[8][FIRST_BUILD]; /* frame of each unit's last hit given or taken (not saved) */
static unsigned char g_bfrac[8][MAX_BUILDS]; /* buildings: experience below 16 (not saved) */

void ranks_load_config(const char *ini_path)
{
    g_ranks_enabled = GetPrivateProfileIntA("gameplay", "ranks", 1, ini_path);
    g_mana_bar = GetPrivateProfileIntA("qol", "mana_bar", 1, ini_path);
    g_insignia_pct = GetPrivateProfileIntA("ranks", "insignia_size", 100, ini_path);
    if (g_insignia_pct < 50 || g_insignia_pct > 300)
        g_insignia_pct = 100;
    /* new key names since 2026-11-10 (4 / 4 before, no attack rate): an old ron_camera.ini, kept
       by the installer, must not bring the old values back; they are game rules, the same for
       every player */
    g_attack_step = GetPrivateProfileIntA("ranks", "damage_percent_per_level", g_attack_step, ini_path);
    g_hits_step = GetPrivateProfileIntA("ranks", "hit_points_percent_per_level", g_hits_step, ini_path);
    g_rate_step = GetPrivateProfileIntA("ranks", "attack_rate_percent_per_level", g_rate_step, ini_path);
    g_heal_pct = GetPrivateProfileIntA("ranks", "heal_percent_per_second", g_heal_pct, ini_path);
    g_calm_s = GetPrivateProfileIntA("ranks", "heal_after_seconds", g_calm_s, ini_path);
    g_ability_pct = GetPrivateProfileIntA("ranks", "ability_xp_percent", g_ability_pct, ini_path);
    g_build_min_cost = GetPrivateProfileIntA("ranks", "building_min_cost", g_build_min_cost, ini_path);
    g_xp_pct = GetPrivateProfileIntA("ranks", "xp_gain_percent", g_xp_pct, ini_path);
    if (g_xp_pct < 0)
        g_xp_pct = 0;
}

/* ---- game access ---------------------------------------------------------------------- */

static int now(void) { return *(int *)(*(char **)VA(VA_GAME) + 0x550); }
static int owner(char *o) { return (unsigned char)o[OBJ_OWNER]; }
static int id_of(char *o) { return *(short *)(o + OBJ_ID); }
static char *type(char *o) { return *(char **)(o + OBJ_TYPE); }
static int is_unit(char *o) { return id_of(o) >= 0 && id_of(o) < FIRST_BUILD; }
static int max_hits(char *o) { return ((fn1_t)(*(void ***)o)[VT_MAX_HITS / 4])(o, 0); }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

/* A building that can rank: one whose type attacks. */
static int ranked_build(char *o)
{
    int id = id_of(o);
    return id >= FIRST_BUILD && id < FIRST_BUILD + MAX_BUILDS && owner(o) < 8 && type(o) &&
           *(int *)(type(o) + T_ATTACK) > 0;
}

static int xp_of(char *u)
{
    if (is_unit(u))
        return (int)(*(unsigned *)(u + U_FLAGS2) >> XP_SHIFT) * (is_hero(u) ? HERO_XP_UNIT : 1);
    if (!ranked_build(u))
        return 0;
    return ((unsigned char)u[B_XP] << 4) + g_bfrac[owner(u)][id_of(u) - FIRST_BUILD];
}

static void set_xp(char *u, int xp)
{
    if (is_unit(u)) {
        unsigned *f = (unsigned *)(u + U_FLAGS2);
        if (is_hero(u))
            xp /= HERO_XP_UNIT;
        if (xp > XP_MAX)
            xp = XP_MAX;
        *f = (*f & ((1u << XP_SHIFT) - 1)) | ((unsigned)xp << XP_SHIFT);
    } else if (ranked_build(u)) {
        if (xp > 0xFFF)
            xp = 0xFFF;
        u[B_XP] = (char)(xp >> 4);
        g_bfrac[owner(u)][id_of(u) - FIRST_BUILD] = (unsigned char)(xp & 15);
    }
}

static int level_of_xp(int xp)
{
    int l = 0;
    while (l < LEVELS && xp >= g_need[l + 1])
        l++;
    return l;
}

static int hero_level_of_xp(int xp)
{
    int l = 0;
    while (l < HERO_LEVELS && xp >= g_hero_need[l + 1])
        l++;
    return l;
}

static int level_u(char *o) { return is_hero(o) ? hero_level_of_xp(xp_of(o)) : level_of_xp(xp_of(o)); }
static int max_level(char *o) { return is_hero(o) ? HERO_LEVELS : LEVELS; }

/* The unit's or defensive building's level, 0 for other objects and when the mod is off. */
static int level(char *o)
{
    if (!g_ranks_enabled || !o || !(is_unit(o) || ranked_build(o)))
        return 0;
    int l = level_u(o);
    /* the Survival card War School (cards.c): the army is at least this level; not the hero */
    if (g_cards_on && is_unit(o) && owner(o) < 8) {
        int m = cards_min_level(owner(o));
        if (m > l && !is_hero(o))
            l = m;
    }
    return l;
}

/* the attack rate bonus: g_rate_step a level to 10, then 2 % a level (the hero) */
static int rate_pct(int l) { return g_rate_step * (l < LEVELS ? l : LEVELS) + 2 * (l > LEVELS ? l - LEVELS : 0); }
/* the insignia: the hero's levels above 10 show level 10's */
static int ins_level(int l) { return l > LEVELS ? LEVELS : l; }
static const char *rank_name(int l) { return l <= LEVELS ? g_name[l] : "Hero"; }

static int is_captain(char *u) { return (*(unsigned short *)(u + U_CAPTAIN) & 0x8000) != 0; }
static char *captain_of(char *u)
{
    /* up the chain to the captain (+0x8E is the soldier before, not the captain: a squad's third
       soldier pointed at the second, 2026-11-10) */
    for (int i = 0; i < 32 && is_unit(u) && !is_captain(u); i++) {
        char *c = object(owner(u), *(short *)(u + U_CAPTAIN));
        if (!c)
            break;
        u = c;
    }
    return u;
}

/* The squad's soldiers from the captain on; at most 32. A building is its own squad. */
static int squad(char *cap, char **out)
{
    int n = 0;
    if (!is_unit(cap)) {
        out[0] = cap;
        return 1;
    }
    for (char *m = cap; m && n < 32; n++) {
        out[n] = m;
        short next = *(short *)(m + U_NEXT);
        m = next >= 0 ? object(owner(cap), next) : NULL;
    }
    return n;
}

/* the hero slot of u, -1 when it is no hero */
static int hero_slot(char *u)
{
    if (!u || !is_unit(u))
        return -1;
    int id = id_of(u), p = owner(u);
    for (int h = 0; h < MAX_HEROES; h++)
        if (g_heroes[h].p == p)
            for (int i = 0; i < g_heroes[h].n; i++)
                if (g_heroes[h].ids[i] == id)
                    return h;
    return -1;
}

static int is_hero(char *u) { return hero_slot(u) >= 0; }

/* The Survival reward cards' bonus (cards.c) for this object and stat: tenths of a percent for
   damage, damage taken, attack rate and hit points, tiles for range and sight, per mille a second
   for healing. 0 outside a Survival game with cards. */
static int card(char *o, int stat)
{
    if (!g_cards_on || !o || owner(o) > 7)
        return 0;
    if (!is_unit(o))
        return cards_bonus(owner(o), CC_BUILD, -1, stat);
    char *t = type(o);
    return cards_bonus(owner(o), hero_slot(captain_of(o)) >= 0 ? CC_HERO : CC_UNIT, t ? *(int *)(t + T_CAT) : -1,
                       stat);
}

static void hero_table(void)
{
    if (g_hero_need[1])
        return;
    for (int l = 0; l <= LEVELS; l++)
        g_hero_need[l] = g_need[l];
    /* 11..30 from level 10's 4095 to 32000, steeper towards the end */
    for (int l = LEVELS + 1; l <= HERO_LEVELS; l++) {
        double f = (double)(l - LEVELS) / (HERO_LEVELS - LEVELS);
        g_hero_need[l] = XP_MAX + (int)((32000 - XP_MAX) * f * (0.4 + 0.6 * f));
    }
}

/* a Survival hero: player p's squad (or single unit) with the soldier `id`, in hero slot `slot`
   (survival.c) */
void ranks_set_hero(int p, int id, int slot)
{
    hero_table();
    char *u = object(p, id);
    if (!u || !is_unit(u) || slot < 0 || slot >= MAX_HEROES)
        return;
    char *soldiers[32];
    int n = squad(captain_of(u), soldiers);
    int same = g_heroes[slot].p == p && g_heroes[slot].n == n;
    for (int i = 0; i < n; i++) {
        if (same && g_heroes[slot].ids[i] != id_of(soldiers[i]))
            same = 0;
        g_heroes[slot].ids[i] = id_of(soldiers[i]);
    }
    g_heroes[slot].n = n;
    g_heroes[slot].p = p;
    int top = 0;
    for (int i = 0; i < n; i++)
        if (xp_of(soldiers[i]) > top)
            top = xp_of(soldiers[i]);
    for (int i = 0; i < n; i++)
        if (xp_of(soldiers[i]) < top)
            set_xp(soldiers[i], top);
    if (!same)
        logf_("ranks: Survival hero %d is player %d's unit of %d (captain %d)", slot, p, n, id_of(captain_of(u)));
    g_heroes[slot].p = p;
}

/* a new game: no heroes (survival.c) */
void ranks_forget_heroes(void)
{
    for (int h = 0; h < MAX_HEROES; h++) {
        g_heroes[h].p = -1;
        g_heroes[h].n = 0;
        g_heroes[h].rem = 0;
    }
}

/* ---- for the Survival cards (cards.c) ------------------------------------------------------ */

/* player p has a hero standing */
int ranks_has_hero(int p)
{
    for (int h = 0; h < MAX_HEROES; h++)
        if (g_heroes[h].p == p)
            for (int i = 0; i < g_heroes[h].n; i++)
                if (object(p, g_heroes[h].ids[i]))
                    return 1;
    return 0;
}

/* every unit and building of player p works out its hit points again (a card changed them) */
void ranks_refresh_hits(int p)
{
    if (p < 0 || p > 7)
        return;
    char *objs = *(char **)VA(VA_OBJECTS);
    int n = *(int *)(objs + OBJS_UNITS_N + 4 * p), nb = *(int *)(objs + OBJS_BUILDS_END + 4 * p);
    if (n > FIRST_BUILD)
        n = FIRST_BUILD;
    for (int id = 0; id < n; id++) {
        char *u = object(p, id);
        if (u && is_captain(u))
            ((fn1_t)(*(void ***)u)[VT_UPDATE_HITS / 4])(u, 0);
    }
    for (int id = FIRST_BUILD; id < nb && id < FIRST_BUILD + MAX_BUILDS; id++) {
        char *b = object(p, id);
        if (b)
            ((fn1_t)(*(void ***)b)[VT_UPDATE_HITS / 4])(b, 0);
    }
}

/* the card War Veteran: q quarters of a level more for player p's hero (both halves alike) */
void ranks_hero_quarter_levels(int p, int q)
{
    hero_table();
    for (int h = 0; h < MAX_HEROES; h++) {
        if (g_heroes[h].p != p)
            continue;
        char *cap = NULL;
        for (int i = 0; i < g_heroes[h].n && !cap; i++)
            cap = object(p, g_heroes[h].ids[i]);
        if (!cap)
            continue;
        cap = captain_of(cap);
        int xp = xp_of(cap), l = hero_level_of_xp(xp);
        if (l >= HERO_LEVELS)
            continue;
        int step = g_hero_need[l + 1] - g_hero_need[l];
        int want = xp + step * q / 4, top = XP_MAX * HERO_XP_UNIT;
        if (want > top)
            want = top;
        char *soldiers[32];
        int n = squad(cap, soldiers);
        for (int i = 0; i < n; i++)
            set_xp(soldiers[i], want);
        int is = hero_level_of_xp(want);
        ((fn1_t)(*(void ***)cap)[VT_UPDATE_HITS / 4])(cap, 0);
        if (is > l) {
            rankfx_promoted(cap, ins_level(is));
            survival_hero_level(p, is);
        }
        logf_("ranks: card: hero %d of player %d from %d to %d experience (level %d -> %d)", h, p, xp, want, l, is);
    }
}

/* the two halves of one hero (slots a and a + 2) at the higher experience of the two */
static void hero_pair_sync(int a, int b)
{
    if (g_heroes[a].p < 0 || g_heroes[b].p < 0 || g_heroes[a].p != g_heroes[b].p)
        return;
    int p = g_heroes[a].p, top = 0;
    for (int k = 0; k < 2; k++) {
        int h = k ? b : a;
        for (int i = 0; i < g_heroes[h].n; i++) {
            char *u = object(p, g_heroes[h].ids[i]);
            if (u && is_unit(u) && xp_of(u) > top)
                top = xp_of(u);
        }
    }
    for (int k = 0; k < 2; k++) {
        int h = k ? b : a;
        for (int i = 0; i < g_heroes[h].n; i++) {
            char *u = object(p, g_heroes[h].ids[i]);
            if (u && is_unit(u) && xp_of(u) < top)
                set_xp(u, top);
        }
    }
}

/* once a second: each hero as it is now (a fallen captain, soldiers back from healing) */
static void hero_refresh(void)
{
    for (int h = 0; h < MAX_HEROES; h++) {
        if (g_heroes[h].p < 0)
            continue;
        int found = 0;
        for (int i = 0; i < g_heroes[h].n && !found; i++) {
            char *u = object(g_heroes[h].p, g_heroes[h].ids[i]);
            if (u && is_unit(u)) {
                ranks_set_hero(g_heroes[h].p, id_of(u), h);
                found = 1;
            }
        }
        if (!found) {
            logf_("ranks: Survival hero %d has fallen", h);
            g_heroes[h].p = -1;
            g_heroes[h].n = 0;
        }
    }
    for (int h = 0; h + 2 < MAX_HEROES; h++)
        if (!(h & 2))
            hero_pair_sync(h, h + 2);
}

/* ---- the infantry hero's extra soldiers ----------------------------------------------------- */

/* The infantry hero's squad grows with its power (hero_power): 3, 4 at level 15, 6 at 20, 7 at
   25, 9 at 30 (the user, 2026-11-10). The game brings a squad's fallen soldiers back while it heals, up to its
   type's UBER_SIZE (Unit::repair_damage 0x0060E027: mov esi, [type+0x308] / curr_uber_size /
   jge); that read asks here, so the hero's squad is filled up to its own size, new soldiers
   included, and every other unit keeps its type's. */
#define VA_REFILL_UBER 0x0060E027u /* mov esi, [eax+0x308] (6 bytes): eax = the type, edi = the unit */

__attribute__((used)) int c_hero_uber(char *u, char *t)
{
    int base = t ? *(int *)(t + T_UBER) : 1;
    char *cap = u ? captain_of(u) : NULL;
    int slot = hero_slot(cap);
    if (slot < 0)
        return base;
    /* the Survival card Reinforcements (cards.c): more soldiers, brought in as the squad heals */
    int extra = g_cards_on ? cards_hero_extra(g_heroes[slot].p, HERO_RIDER(slot)) : 0;
    if (HERO_RIDER(slot)) /* the riders: a squad of two (made so by survival.c), refilled to two */
        return 2 + extra;
    return 2 * base * hero_power(level(cap)) / 100 + extra; /* the infantry: twice its type's 3, then 8, 12, 14, 18 */
}

void hk_hero_uber(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_hero_uber\n_hk_hero_uber:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+28]\n" /* eax: the type */
        "    push dword ptr [ebx+0]\n"  /* edi: the unit */
        "    call _c_hero_uber\n"
        "    add esp, 16\n"
        "    mov [ebx+4], eax\n" HOOK_LEAVE /* esi */
        "    ret\n"
        ".att_syntax prefix\n");

static void uber_install(void)
{
    static const unsigned char sig[6] = { 0x8B, 0xB0, 0x08, 0x03, 0x00, 0x00 };
    unsigned char *at = (unsigned char *)VA(VA_REFILL_UBER);
    if (memcmp(at, sig, 6) != 0) {
        logf_("ranks: signature mismatch in Unit::repair_damage, the hero's squad stays at its type's size");
        return;
    }
    DWORD old;
    VirtualProtect(at, 6, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE8;
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_hero_uber - (UINT_PTR)(at + 5));
    at[5] = 0x90;
    VirtualProtect(at, 6, old, &old);
    logf_("ranks: the infantry hero's squad grows at levels 15, 20, 25 and 30");
}

/* ---- big squads stand in a ring --------------------------------------------------------- */

/* Where a squad's soldiers stop (the user, 2026-11-10: the hero's six soldiers "ocupa 3 locuri si se
   stacheaza"): Group::action_move_near gives each soldier a spot beside its captain's destination
   (UnitType::find_nearby_spot at the type's size, +0x224) in the direction 0x00705CC0 picks: the
   soldier right after the captain (its +0x8E is the captain) to the left, +90 degrees, every other
   one to the right, -90 degrees. That is a triangle for the game's squads of 3, but the extra
   soldiers of a bigger squad all go to the right-hand spot. For a squad of more than 3 the
   soldiers are spread round the captain instead, one angle each, starting on the left;
   squads of 3 and less keep the game's two sides. Angles are the game's 32-bit turns. */
#define VA_SQUAD_SIDE   0x00705CC0u /* movsx eax, word [eax+0x8E] ... 26 bytes, on to 0x00705CDA */
#define VA_SQUAD_SIDE_TO 0x00705CDAu

__attribute__((used)) unsigned c_squad_angle(char *soldier, int captain_id, unsigned base)
{
    unsigned side = *(short *)(soldier + U_CAPTAIN) == captain_id ? base + 0x40000000u : base + 0xC0000000u;
    char *list[32];
    int n = squad(captain_of(soldier), list), k = -1;
    if (n <= 3)
        return side;
    for (int i = 1; i < n; i++)
        if (list[i] == soldier)
            k = i;
    if (k < 1)
        return side;
    return base + 0x40000000u + (unsigned)(k - 1) * (unsigned)(0x100000000ull / (unsigned)(n - 1));
}

void hk_squad_angle(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_squad_angle\n_hk_squad_angle:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    mov ecx, [ebx+8]\n"         /* the function's ebp */
        "    push dword ptr [ecx+0x1c]\n" /* the facing */
        "    push dword ptr [ebx+16]\n"   /* ebx: the captain's id */
        "    push dword ptr [ebx+28]\n"   /* eax: the soldier */
        "    call _c_squad_angle\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret\n"
        ".att_syntax prefix\n");

static void squad_ring_install(void)
{
    static const unsigned char sig[9] = { 0x0F, 0xBF, 0x80, 0x8E, 0x00, 0x00, 0x00, 0x3B, 0xC3 };
    unsigned char *at = (unsigned char *)VA(VA_SQUAD_SIDE);
    if (memcmp(at, sig, 9) != 0) {
        logf_("ranks: signature mismatch in Group::action_move_near, big squads keep the game's two sides");
        return;
    }
    DWORD old;
    VirtualProtect(at, 26, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE8; /* call hk_squad_angle: eax = the angle */
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_squad_angle - (UINT_PTR)(at + 5));
    at[5] = 0xEB; /* jmp short to 0x00705CDA */
    at[6] = (unsigned char)(VA(VA_SQUAD_SIDE_TO) - (UINT_PTR)(at + 7));
    for (int i = 7; i < 26; i++)
        at[i] = 0x90;
    VirtualProtect(at, 26, old, &old);
}

/* The same in formations (the moves the player orders): Form::compute_dests 0x0072CBA0 puts a
   squad's soldiers beside its captain's place, the type's size (+0x224) to the side: the first
   soldier after the captain on one side (its flag [ebp-0x3C] is still 1 from the captain), every
   other one on the other side (0x0072D375 .. 0x0072D38F), so the soldiers of a squad bigger than
   3 pile up there (the user, 2026-11-10: "tot 3 se stacheaza"). Here the soldiers after the first
   go out further each, side by side: +1, -1, +2, -2, +3 sizes, a row; a squad of 3 is +1, -1 as
   before. The count runs in the order the game takes the soldiers (the same on every machine). */
#define VA_FORM_SIDE 0x0072D385u /* mov eax, [eax+0x224] / jne +2 / neg eax: 10 bytes */
static int g_form_k;

__attribute__((used)) int c_form_side(char *type, int first)
{
    int size = type ? *(int *)(type + 0x224) : 0;
    g_form_k = first ? 1 : g_form_k + 1;
    int k = g_form_k, steps = (k + 1) / 2;
    return k & 1 ? size * steps : -size * steps;
}

void hk_form_side(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_form_side\n_hk_form_side:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    mov ecx, [ebx+8]\n"          /* the function's ebp */
        "    push dword ptr [ecx-0x3c]\n" /* 1: the first soldier after the captain */
        "    push dword ptr [ebx+28]\n"   /* eax: the soldier's type */
        "    call _c_form_side\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret\n"
        ".att_syntax prefix\n");

static void form_side_install(void)
{
    static const unsigned char sig[10] = { 0x8B, 0x80, 0x24, 0x02, 0x00, 0x00, 0x75, 0x02, 0xF7, 0xD8 };
    unsigned char *at = (unsigned char *)VA(VA_FORM_SIDE);
    if (memcmp(at, sig, 10) != 0) {
        logf_("ranks: signature mismatch in Form::compute_dests, big squads pile up in formations");
        return;
    }
    DWORD old;
    VirtualProtect(at, 10, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE8;
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_form_side - (UINT_PTR)(at + 5));
    for (int i = 5; i < 10; i++)
        at[i] = 0x90;
    VirtualProtect(at, 10, old, &old);
    logf_("ranks: squads bigger than 3 stand in a row in formations");
}

/* ---- the hero's mark ------------------------------------------------------------------------ */

/* Over each hero's captain, every frame (rankfx.c's map window redraw): its medallion (interface
   IFACETEX "hero_infantry" / "hero_cavalry", ui-theme's survival_bar.tga), above the health bar's
   place, sized with the zoom. Only the drawing. */
void ranks_draw_hero_marks(void)
{
    for (int h = 0; h < MAX_HEROES; h++) {
        if (g_heroes[h].p < 0)
            continue;
        const void *tex = qol_iface_tex(HERO_RIDER(h) ? "hero_cavalry" : "hero_infantry");
        if (!tex)
            continue;
        char *u = NULL;
        for (int i = 0; i < g_heroes[h].n && !u; i++)
            u = object(g_heroes[h].p, g_heroes[h].ids[i]);
        if (!u)
            continue;
        char *cap = captain_of(u);
        float x, y;
        if (!rankfx_anchor(cap, &x, &y))
            continue;
        float s = 30.0f * rankfx_zoom();
        s = s < 20.0f ? 20.0f : s > 40.0f ? 40.0f : s;
        rf_add(tex, x - s * 0.5f, y - s * 1.45f, s, s, 0xFFFFFFFFu);
    }
}

/* ---- the hero's light ------------------------------------------------------------------- */

/* The Survival hero wears the Patriots' light (the user, 2026-11-10: "lumina de deasupra de la
   patriot"): UnitOut::draw_overlays draws the "CTWGlow" column (GraphicPieces+0x1048) over a
   unit when it is a government hero (UnitData::is_gov_hero, type FLAGS & 0x4000000) or in some
   Conquer the World cases. Its three ways to "no light" (0x006159A9 jge, 0x006159E3 test / jne /
   jmp, 0x00615A20 test / je, all to 0x00615B5C, "light" being 0x00615A28) jump here instead,
   with esi = the player and edi = the unit's id as there: the hero gets the light too. Only the
   drawing changes: the hero does not become a Patriot. */
#define VA_GLOW_A    0x006159A9u /* jge 0x00615B5C (6 bytes) */
#define VA_GLOW_A_GO 0x006159AFu /* the government hero test */
#define VA_GLOW_B    0x006159E3u /* test eax, eax / jne 0x00615A28 / jmp 0x00615B5C (9 bytes) */
#define VA_GLOW_C    0x00615A20u /* test eax, eax / je 0x00615B5C (8 bytes) */
#define VA_GLOW_YES  0x00615A28u
#define VA_GLOW_NO   0x00615B5Cu
void *g_glow_yes, *g_glow_no, *g_glow_a_go;

__attribute__((used)) int c_glow_hero(int p, int id)
{
    char *u = object(p, id);
    return u && is_hero(u);
}

void hk_glow_a(void);
void hk_glow_bc(void);
__asm__(".intel_syntax noprefix\n"
        /* A: in the game's "jge no light": light for the hero, else on as before */
        ".globl _hk_glow_a\n_hk_glow_a:\n"
        "    jl 3f\n"
        "    xor eax, eax\n"
        "    jmp _hk_glow_bc\n"
        "3:\n"
        "    jmp dword ptr [_g_glow_a_go]\n"
        /* B, C: eax = the game's answer; the hero is a yes too */
        ".globl _hk_glow_bc\n_hk_glow_bc:\n"
        "    test eax, eax\n"
        "    jnz 1f\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+0]\n" /* edi: the id */
        "    push dword ptr [ebx+4]\n" /* esi: the player */
        "    call _c_glow_hero\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp dword ptr [_g_glow_no]\n"
        "1:\n"
        "    jmp dword ptr [_g_glow_yes]\n"
        ".att_syntax prefix\n");

static void glow_install(void)
{
    static const unsigned char a[6] = { 0x0F, 0x8D, 0xAD, 0x01, 0x00, 0x00 };
    static const unsigned char b[9] = { 0x85, 0xC0, 0x75, 0x41, 0xE9, 0x70, 0x01, 0x00, 0x00 };
    static const unsigned char c[8] = { 0x85, 0xC0, 0x0F, 0x84, 0x34, 0x01, 0x00, 0x00 };
    if (memcmp((void *)VA(VA_GLOW_A), a, 6) || memcmp((void *)VA(VA_GLOW_B), b, 9) ||
        memcmp((void *)VA(VA_GLOW_C), c, 8)) {
        logf_("ranks: signature mismatch in UnitOut::draw_overlays, the hero has no light");
        return;
    }
    g_glow_yes = (void *)VA(VA_GLOW_YES);
    g_glow_no = (void *)VA(VA_GLOW_NO);
    g_glow_a_go = (void *)VA(VA_GLOW_A_GO);
    hook_jmp5((unsigned char *)VA(VA_GLOW_A), hk_glow_a);
    hook_jmp5((unsigned char *)VA(VA_GLOW_B), hk_glow_bc);
    hook_jmp5((unsigned char *)VA(VA_GLOW_C), hk_glow_bc);
    logf_("ranks: the Survival hero wears the Patriots' light");
}

/* hero `slot`'s soldiers now (survival.c: its evolution); 0 when it has fallen */
int ranks_hero_squad(int p, int slot, char **out)
{
    if (slot < 0 || slot >= MAX_HEROES || g_heroes[slot].p != p)
        return 0;
    for (int i = 0; i < g_heroes[slot].n; i++) {
        char *u = object(p, g_heroes[slot].ids[i]);
        if (u && is_unit(u))
            return squad(captain_of(u), out);
    }
    return 0;
}

static long long cost(char *t)
{
    long long c = 0;
    for (int r = 0; r < 6; r++)
        if (((int *)(t + T_COST))[r] > 0)
            c += ((int *)(t + T_COST))[r];
    return c;
}

/* ---- the bonuses (also called from entrench.c and kills.c) ------------------------------- */

/* The heroes' growth (the user, 2026-11-10: "sa fie echilibrat si la infanterie si la
   cavalerie"): a power factor, percent, by level: 100 to 14, 133 at 15, 200 at 20, 233 at 25,
   300 at 30. The infantry squad has that many times its 3 soldiers (3, 4, 6, 7, 9: c_hero_uber).
   The rider stays one unit (the game refills squads only, Unit::repair_damage) and gets the same
   power as hit points x power and damage and attack rate x its square root each (damage x rate =
   power: the same damage a second as the squad, without a rider that hits absurdly fast). Both
   earn their experience divided by the power, so more soldiers do not rank faster. */
static int hero_power(int l) { return l >= 30 ? 300 : l >= 25 ? 233 : l >= 20 ? 200 : l >= 15 ? 133 : 100; }
static int isqrt100(int p) /* sqrt(p / 100) x 100 */
{
    int r = 100;
    while ((r + 1) * (r + 1) <= p * 100)
        r++;
    return r;
}
/* the rider's power (hero slot 1), 100 for anyone else */
static int rider_power(char *u)
{
    char *cap = u ? captain_of(u) : NULL;
    return HERO_RIDER(hero_slot(cap)) ? hero_power(level(cap)) : 100;
}

int ranks_damage(char *attacker, int dmg)
{
    if (dmg <= 0)
        return dmg;
    int l = level(attacker);
    if (l) {
        dmg = dmg * (100 + g_attack_step * l) / 100;
        int pw = rider_power(attacker);
        if (pw > 100)
            dmg = dmg * isqrt100(pw) / 100;
    }
    int c = card(attacker, CS_DMG);
    return c ? dmg * (1000 + c) / 1000 : dmg;
}

int ranks_recharge(char *u, int r)
{
    if (r <= 0)
        return r;
    int l = level(u), rate = rate_pct(l), c = card(u, CS_RATE);
    if (rate <= 0 && !c)
        return r;
    if (rate > 0) {
        r = r * 100 / (100 + rate);
        int pw = rider_power(u);
        if (pw > 100)
            r = r * 100 / isqrt100(pw);
    }
    if (c)
        r = r * 1000 / (1000 + c);
    return r < 1 ? 1 : r;
}

int ranks_range(char *u, int r)
{
    if (r <= 0)
        return r;
    return r + (level(u) >= 7 ? 1 : 0) + card(u, CS_RANGE);
}

/* Before Object::take_damage: -10 % from level 6, -20 % at level 10, for a hit by an object (not
   attrition), and the Survival cards' armour. *whole = hit points, *part = sixteenths. */
void ranks_before_hit(char *victim, int *whole, int *part, int attacker_id)
{
    if (attacker_id < 0)
        return;
    int l = level(victim), c = card(victim, CS_TAKEN);
    if (l < 6 && !c)
        return;
    int t = *whole * 16 + (signed char)*part;
    if (t <= 0)
        return;
    if (l >= 6)
        t = t * (l >= 10 ? 80 : 90) / 100;
    if (c)
        t = t * (1000 - (c > 900 ? 900 : c)) / 1000;
    *whole = t >> 4;
    *part = t & 15;
}

/* g / 256 of the unit's cost more experience for its whole squad. */
static void add_xp(char *att, long long g)
{
    if (g <= 0)
        return;
    char *cap = captain_of(att), *soldiers[32];
    int slot = hero_slot(cap), hero = slot >= 0, top = hero ? XP_MAX * HERO_XP_UNIT : XP_MAX;
    if (hero)
        g = g * 100 / hero_power(level(cap));
    if (hero) { /* below HERO_XP_UNIT is carried here */
        g += g_heroes[slot].rem;
        g_heroes[slot].rem = (int)(g % HERO_XP_UNIT);
    }
    int old = xp_of(cap), xp = old + (int)(g > top ? top : g);
    if (xp > top)
        xp = top;
    if (xp == old)
        return;
    int n = squad(cap, soldiers);
    for (int i = 0; i < n; i++)
        set_xp(soldiers[i], xp);
    int was = hero ? hero_level_of_xp(old) : level_of_xp(old), is = hero ? hero_level_of_xp(xp) : level_of_xp(xp);
    if (is != was) {
        ((fn1_t)(*(void ***)cap)[VT_UPDATE_HITS / 4])(cap, 0); /* the new hit points */
        if (is > was)
            rankfx_promoted(cap, ins_level(is)); /* the insignia over its head (rankfx.c) */
        if (hero && is > was)
            survival_hero_level(owner(cap), is); /* survival.c: the notice */
        static int logged;
        if (logged < 40) {
            logged++;
            logf_("ranks: player %d unit %d (squad of %d) is now level %d, %s (%d / 256 of its cost)", owner(cap),
                  id_of(cap), n, is, rank_name(is), xp);
        }
    }
}

/* What a hit is worth below one 1/256 of the attacker's cost, in 1/65536, carried to its next
   hit (by the captain's or building's id; not saved). Without it a hit on something with many
   hit points (a building: a Fort has 2000 and up) was worth 0.2 and rounded to nothing, so a
   Fort or the Red Fort shooting at buildings never ranked up (seen 2026-10-27). */
static unsigned short g_xfrac[8][FIRST_BUILD + MAX_BUILDS];

static void gain(char *att, char *victim, int applied, int vmax)
{
    char *at = type(att), *vt = type(victim);
    if (!at || !vt || !*(int *)(at + T_ATTACK) || (is_unit(att) && (*(int *)(att + U_FLAGS) & 1)))
        return;
    long long acost = cost(at), vcost = cost(vt);
    if (!is_unit(att) && acost < g_build_min_cost)
        acost = g_build_min_cost;
    int vuber = is_unit(victim) ? *(int *)(vt + T_UBER) : 1;
    if (acost <= 0 || vcost <= 0 || vmax <= 0)
        return;
    if (vuber < 1)
        vuber = 1;
    long long g = (long long)applied * vcost * 256 * 65536 * g_xp_pct / ((long long)vuber * vmax * acost * 100);
    char *who = captain_of(att);
    int p = owner(who), id = id_of(who);
    if (p < 8 && id >= 0 && id < FIRST_BUILD + MAX_BUILDS) {
        g += g_xfrac[p][id];
        g_xfrac[p][id] = (unsigned short)(g & 0xFFFF);
    }
    add_xp(att, g >> 16);
}

/* After Object::take_damage: experience for the attacker and the combat clock for both.
   before = the victim's hit points lost before the hit, vmax = its max hits then. */
void ranks_after_hit(char *victim, int before, int vmax, int was_decoy, int attacker_id, int attacker_p)
{
    if (!g_ranks_enabled || attacker_id < 0 || attacker_p < 0 || attacker_p > 7)
        return;
    int t = now();
    if (is_unit(victim) && owner(victim) < 8)
        g_last[owner(victim)][id_of(victim)] = t;
    char *att = object(attacker_p, attacker_id);
    if (!att || !(is_unit(att) || ranked_build(att)))
        return;
    if (is_unit(att))
        g_last[attacker_p][attacker_id] = t;
    if (was_decoy || attacker_p == owner(victim))
        return;
    int after = (victim[OBJ_FLAGS] & 1) ? *(int *)(victim + OBJ_DAMAGE) : vmax;
    if (after > vmax)
        after = vmax;
    if (after > before)
        gain(att, victim, after - before, vmax);
}

/* Once per simulation frame (airdrop.c's Objects::process_all): level 9 and up heal once a second. */
void ranks_frame(void)
{
    if (!g_ranks_enabled)
        return;
    int t = now();
    if (t % 15)
        return;
    hero_refresh();
    /* the Survival card Iron Blood (cards.c): the hero heals a little every second, in combat too */
    if (g_cards_on)
        for (int h = 0; h < MAX_HEROES; h++) {
            int hp = g_heroes[h].p;
            if (hp < 0)
                continue;
            char *first = NULL;
            for (int i = 0; i < g_heroes[h].n && !first; i++)
                first = object(hp, g_heroes[h].ids[i]);
            int pm = first ? card(first, CS_REGEN) : 0;
            for (int i = 0; pm > 0 && i < g_heroes[h].n; i++) {
                char *u = object(hp, g_heroes[h].ids[i]);
                int lost = u ? *(int *)(u + OBJ_DAMAGE) : 0;
                if (lost <= 0)
                    continue;
                int heal = max_hits(u) * pm / 1000;
                if (heal < 1)
                    heal = 1;
                *(int *)(u + OBJ_DAMAGE) = lost > heal ? lost - heal : 0;
            }
        }
    if (g_heal_pct <= 0)
        return;
    char *objs = *(char **)VA(VA_OBJECTS);
    for (int p = 0; p < 8; p++) {
        int n = *(int *)(objs + OBJS_UNITS_N + 4 * p);
        if (n > FIRST_BUILD)
            n = FIRST_BUILD;
        for (int id = 0; id < n; id++) {
            char *u = object(p, id);
            if (!u || level(u) < HEAL_LEVEL || t - g_last[p][id] < g_calm_s * 15)
                continue;
            int lost = *(int *)(u + OBJ_DAMAGE);
            if (lost <= 0)
                continue;
            int heal = max_hits(u) * g_heal_pct / 100;
            if (heal < 1)
                heal = 1;
            *(int *)(u + OBJ_DAMAGE) = lost > heal ? lost - heal : 0;
        }
    }
}

/* ---- abilities: experience for Spies and Generals --------------------------------------- */

typedef void(__attribute__((thiscall)) *cast_t)(void *spell, int a, int b, int c, int d);
typedef int(__attribute__((thiscall)) *castable_t)(void *spell, int id, int p, int one);

/* SpellType::cast(caster id, caster player, target id, target player), in the simulation. A
   bribed unit moves to the briber's side as a new object, so the target's old slot no longer
   holds it afterwards. */
__attribute__((used)) int c_rk_cast(char *spell, int *a)
{
    char *caster = g_ranks_enabled || g_heal_enabled ? object(a[1], a[0]) : NULL;
    int cast = caster && is_unit(caster) && ((castable_t)VA(VA_IS_CASTABLE))(spell, a[0], a[1], 1) == 3;
    if (cast && *(int *)(spell + SPELL_TYPE) == SPELL_HEAL)
        heal_start(caster); /* the game itself has nothing for this craft (heal.c) */
    cast = cast && g_ranks_enabled;
    int bribe = cast && *(int *)(spell + SPELL_TYPE) == SPELL_BRIBE;
    char *victim = bribe ? object(a[3], a[2]) : NULL;
    long long vcost = victim && type(victim) ? cost(type(victim)) : 0;
    ((cast_t)g_orig_cast)(spell, a[0], a[1], a[2], a[3]);
    if (!cast || object(a[1], a[0]) != caster || !type(caster))
        return 0;
    long long own = cost(type(caster));
    if (own <= 0)
        return 0;
    if (bribe) {
        if (victim && object(a[3], a[2]) != victim && vcost > 0)
            add_xp(caster, vcost * 256 * g_xp_pct / (own * 100));
    } else if (!*(int *)(type(caster) + T_ATTACK) && *(int *)(spell + SPELL_MANA) > 0)
        add_xp(caster, (long long)256 * g_ability_pct * g_xp_pct * *(int *)(spell + SPELL_MANA) / (100 * 100 * 1000));
    return 0;
}

/* ---- the selection panel: the portrait and the tooltip ---------------------------------- */

typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef void(__attribute__((thiscall)) *str_ctor_t)(void *self, const char *s);
typedef void(__attribute__((thiscall)) *str_close_t)(void *self);
typedef int(__attribute__((thiscall)) *list_add_t)(void *list, const void *a, const void *b, int c, int d, int e);
typedef void(__attribute__((thiscall)) *describe_t)(void *self, void *opt, int a, int b, int c);

/* 0xAABBGGRR, as IFaceRenderManager::add takes them */
static const unsigned g_tier_rgba[3] = { 0xFF327FCD, 0xFFDCD2CD, 0xFF28CDFF }; /* bronze, silver, gold */

/* Levels 1-9: 3 tiers of 1-3 bars; level 10: 4 gold bars. */
static int tier_of(int l) { return l >= 10 ? 2 : (l - 1) / 3; }
static int bars_of(int l) { return l >= 10 ? 4 : (l - 1) % 3 + 1; }

/* Experience shown for this object at all: a unit with a weapon or with some experience
   already, or a defensive building. */
static int shows_rank(char *u)
{
    if (!g_ranks_enabled || !u || !type(u))
        return 0;
    if (!is_unit(u))
        return ranked_build(u);
    return *(int *)(type(u) + T_ATTACK) || xp_of(u) > 0;
}

/* 0..1000 of the way from the current level to the next; 1000 at the last level. */
static int progress_u(char *u)
{
    int xp = xp_of(u), l = level_u(u);
    const int *need = is_hero(u) ? g_hero_need : g_need;
    if (l >= max_level(u))
        return 1000;
    return (xp - need[l]) * 1000 / (need[l + 1] - need[l]);
}

static void solid(const short *r, unsigned color)
{
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, r, color, RENDER_SOLID);
}

static char *g_panel_unit;      /* the unit ranks_draw_panel last drew, */
static unsigned g_panel_tick;   /* and when: its portrait then goes without the experience bar */

/* After IFaceButton::on_redraw (panel.c), with the render manager's origin at the button: a unit
   portrait (kind 3, a building's kind 4) gets its insignia in the top-right corner and its
   experience bar along the bottom. */
void ranks_draw_button(char *btn)
{
    int kind = *(int *)(btn + BTN_KIND);
    if (!g_ranks_enabled || (kind != 3 && kind != 4) || (*(unsigned short *)(btn + BTN_FLAGS) & 8))
        return;
    char *u = object(*(int *)(btn + BTN_OBJ_P), *(int *)(btn + BTN_OBJ_ID));
    if (!shows_rank(u))
        return;
    short *wr = (short *)(btn + WIN_RECT);
    int w = wr[1] - wr[0], h = wr[3] - wr[2];
    if (w < 16 || h < 12)
        return;
    int l = level_u(u);
    char name[16];
    snprintf(name, sizeof name, "rank_%d", ins_level(l));
    void *tex = l ? qol_iface_tex(name) : NULL;
    static int logged;
    if (l && !logged) {
        logged = 1;
        logf_("ranks: insignia textures %s", tex ? "found" : "missing (interface.xml), bars drawn instead");
    }
    if (tex) { /* the insignia (ui-theme rank_icons.tga, 48 x 56), one depth step in front */
        int ih = h * 9 / 20 < 14 ? 14 : h * 9 / 20, iw = ih * 48 / 56;
        short r[4] = { (short)(w - 2 - iw), (short)(w - 2), 2, (short)(2 + ih) };
        int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
        (*depth)++;
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, r, *(unsigned *)VA(VA_WHITE), 1);
        (*depth)--;
    } else if (l) {
        int tier = tier_of(l), bars = bars_of(l);
        int bw = w / 5 < 8 ? 8 : w / 5, bh = h / 14 < 2 ? 2 : h / 14;
        for (int k = 0; k < bars; k++) {
            int top = 3 + k * (bh + 3);
            short out[4] = { (short)(w - 4 - bw), (short)(w - 2), (short)(top - 1), (short)(top + bh + 1) };
            short in[4] = { (short)(w - 3 - bw), (short)(w - 3), (short)top, (short)(top + bh) };
            solid(out, 0xFF101010);
            solid(in, g_tier_rgba[tier]);
        }
    }
    if (u == g_panel_unit && GetTickCount() - g_panel_tick < 250)
        return; /* the selection panel's right half shows its experience bar (ranks_draw_panel) */
    int bh = h / 12 < 3 ? 3 : h / 12, x0 = 2, x1 = w - 2, y1 = h - 2, y0 = y1 - bh;
    short back[4] = { (short)x0, (short)x1, (short)y0, (short)y1 };
    solid(back, 0xC0101010);
    int fill = (x1 - x0 - 2) * progress_u(u) / 1000;
    if (fill > 0) {
        short bar[4] = { (short)(x0 + 1), (short)(x0 + 1 + fill), (short)(y0 + 1), (short)(y1 - 1) };
        solid(bar, l >= max_level(u) ? g_tier_rgba[2] : 0xFF20E6FF); /* yellow, gold when complete */
    }
}

/* One unit selected (the user, 2026-11-02): the right half of the selection panel, empty for a
   unit but for the game's "Strong vs." text at its bottom, shows the rank's insignia and name with
   the kills on the right (kills.c), the level and a large experience bar with the way to the next
   rank in per cent, and the bonuses the rank gives now, one per line. Called after on_redraw drew
   the stats, with the render manager's origin at the panel; panel units (interface.xml
   IFACESELECTED: the right box spans x 133..292, the game's "text" rect starts at y 125; an
   enemy's player name is at y 23..45 and its nation at 98..115). Returns 0 when the right half is
   taken (a production queue, a transport's cargo): the kills then stay among the stats. */
#define VA_FONTS     0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET   0x00522810u /* FontFamily::get(int size, int style): 1 bold, -1 the default */
#define VA_WRITE_L    0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, int len) */
#define VA_WRITE_R    0x0050FA40u /* Buffer::write_right_l(const String &, const Rect *, int len) */
#define SEL_PROGRESS  0x4DC       /* IFaceSelected: short l, r of the production bar (empty: none) */
#define SEL_BUFFER    0x154       /* the Buffer it writes with; +0x7C its fonts (normal, bold, italic) */
#define BUF_FONTS     0x7C
#define VT_OBJ_IS     0xB8        /* ObjectData::is(int type, int): the object is that type or in its line */
#define TYPE_CARRIER  0x15F       /* ...asked by on_redraw (0x00826A46) before it writes the cargo from y 26 */
#define U_CARGO       0xA0        /* word: units aboard */
#define PX0 139
#define PX1 287
#define PTEXT 124

typedef void *(__attribute__((thiscall)) *font_get_t)(void *ff, int size, int style);
typedef int(__attribute__((thiscall)) *write_rect_t)(void *buf, const void *str, const short *rect, int len);

static void panel_text(char *panel, const char *utf8, int size, int bold, int right, int l, int t, int r, int b)
{
    char *buf = panel + SEL_BUFFER;
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), size, bold ? 1 : -1);
    if (!font || !*(void **)font)
        return;
    void *saved[3], *set[3] = { font, NULL, NULL };
    memcpy(saved, buf + BUF_FONTS, sizeof saved);
    memcpy(buf + BUF_FONTS, set, sizeof set);
    unsigned char str[64] = { 0 };
    lang_string(str, utf8);
    int len = *(unsigned short *)(str + 8);
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    if (len)
        ((write_rect_t)VA(right ? VA_WRITE_R : VA_WRITE_L))(buf, str, rect, len);
    ((str_close_t)VA(VA_STRING_CLOSE))(str);
    memcpy(buf + BUF_FONTS, saved, sizeof saved);
}

/* A tooltip piece (" +%d%% attack,") as a line of its own: without the spaces and commas round it. */
static void bonus_line(char *out, int size, const char *en, int v)
{
    char t[160];
    snprintf(t, sizeof t, lang_text(en), v);
    char *s = t;
    while (*s == ' ' || *s == ',')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == ','))
        s[--n] = 0;
    snprintf(out, size, "%s", s);
}

int ranks_draw_panel(char *panel, char *u, int kills, int show_kills)
{
    char *t = type(u), *console = *(char **)VA(VA_CONSOLE);
    short *prog = (short *)(panel + SEL_PROGRESS);
    if (!t || prog[1] > prog[0])
        return 0;
    /* Two arguments, thiscall (ret 8): called without them, the stack came back 8 bytes off and the
       next SSE store faulted (one object selected, 2026-11-05). */
    if (((fn2_t)(*(void ***)u)[VT_OBJ_IS / 4])(u, TYPE_CARRIER, 0) && *(short *)(u + U_CARGO) > 0)
        return 0;
    int ranked = shows_rank(u);
    if (!ranked && !show_kills)
        return 1;
    int own = console && owner(u) == *(int *)(console + 0x298);
    int top = own ? 22 : 47, bottom = own ? PTEXT : 96;
    if (show_kills)
        kills_draw_count(panel, kills, PX1, top + 1, 17);
    if (!ranked)
        return 1;
    g_panel_unit = u;
    g_panel_tick = GetTickCount();

    /* the insignia and the rank's name */
    int l = level_u(u), x = PX0;
    char text[160];
    if (l) {
        snprintf(text, sizeof text, "rank_%d", ins_level(l));
        void *tex = qol_iface_tex(text);
        if (tex) {
            short r[4] = { (short)PX0, (short)(PX0 + 17), (short)top, (short)(top + 20) };
            ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, r, *(unsigned *)VA(VA_WHITE), 1);
            x = PX0 + 21;
        }
    }
    const char *name = lang_text(rank_name(l));
    panel_text(panel, name, strlen(name) <= 12 ? 12 : 10, 1, 0, x, top, PX1 - (show_kills ? 46 : 0), top + 20);

    /* the level, the experience bar and the way to the next rank */
    int y = top + 23;
    snprintf(text, sizeof text, lang_text("Lv %d"), l);
    panel_text(panel, text, 10, 1, 0, PX0, y, PX0 + 34, y + 12);
    int b0 = PX0 + 35, b1 = PX1 - 31;
    short out[4] = { (short)b0, (short)b1, (short)y, (short)(y + 12) };
    short in[4] = { (short)(b0 + 1), (short)(b1 - 1), (short)(y + 1), (short)(y + 11) };
    solid(out, 0xFF080808);
    solid(in, 0xFF34302C);
    int fill = (b1 - b0 - 2) * progress_u(u) / 1000;
    if (fill > 0) {
        short bar[4] = { (short)(b0 + 1), (short)(b0 + 1 + fill), (short)(y + 1), (short)(y + 11) };
        solid(bar, l >= max_level(u) ? g_tier_rgba[2] : 0xFF20E6FF); /* yellow, gold at the last rank */
        short shine[4] = { bar[0], bar[1], bar[2], (short)(y + 4) };
        solid(shine, 0x50FFFFFF);
    }
    snprintf(text, sizeof text, "%d%%", progress_u(u) / 10);
    panel_text(panel, text, 10, 1, 1, PX1 - 30, y, PX1, y + 12);

    /* the bonuses, one per line, the tooltip's pieces */
    if (!l)
        return 1;
    int armed = *(int *)(t + T_ATTACK) != 0, n = 0;
    char lines[6][160];
    if (armed)
        bonus_line(lines[n++], sizeof lines[0], " +%d%% attack,", g_attack_step * l);
    bonus_line(lines[n++], sizeof lines[0], " +%d%% hit points", g_hits_step * l);
    if (l >= 3) {
        bonus_line(lines[n], sizeof lines[0], ", +1 sight", 0);
        if (armed && l >= 7 && ((fn0_t)VA(VA_MAX_RANGE))(u) > 0) {
            char r[160];
            bonus_line(r, sizeof r, "+1 range,", 0);
            size_t k = strlen(lines[n]);
            snprintf(lines[n] + k, sizeof lines[0] - k, ", %s", r);
        }
        n++;
    }
    if (armed && g_rate_step > 0)
        bonus_line(lines[n++], sizeof lines[0], "+%d%% attack rate,", rate_pct(l));
    if (l >= 6)
        bonus_line(lines[n++], sizeof lines[0], "-%d%% damage taken,", l >= 10 ? 20 : 10);
    if (l >= HEAL_LEVEL)
        bonus_line(lines[n++], sizeof lines[0], "heals out of combat,", 0);
    y = top + 39;
    for (int i = 0; i < n && y + 10 <= bottom; i++, y += 10)
        panel_text(panel, lines[i], 10, 0, 0, PX0, y, PX1, y + 10);
    return 1;
}

static void tip_line(const char *text)
{
    char *scene = *(char **)VA(VA_SCENE);
    if (!scene)
        return;
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text(text));
    ((list_add_t)VA(VA_LIST_ADD))(scene + 0x104, str, (void *)VA(VA_EMPTY_STRING), 0, 0x5E, 0);
    ((str_close_t)VA(VA_STRING_CLOSE))(str);
}

/* The unit a tooltip describes: a member of the local selection by index (the portrait), or the
   object under the mouse (the map, or the portrait of one selected unit). */
static char *described_unit(const int *opt)
{
    if (opt[0] == OPT_GROUP || opt[0] == OPT_GROUP_ME) {
        char *console = *(char **)VA(VA_CONSOLE), *sel = *(char **)VA(VA_SELECTIONS);
        if (!console || !sel)
            return NULL;
        char *g = sel + *(int *)(console + 0x2A0) * 0x9F0;
        int i = opt[1] < 0 ? 0 : opt[1];
        if (i >= *(int *)(g + 0xC))
            return NULL;
        return object((unsigned char)g[0x4A], *(short *)(g + 0x8CC + 2 * i));
    }
    if (opt[0] == OPT_OBJECT || opt[0] == OPT_OBJECT2) {
        char *mo = *(char **)VA(VA_MOUSEOVER);
        if (!mo || !(*(int *)(mo + 4) & 4))
            return NULL;
        return object(*(int *)(mo + 0x4C), *(int *)(mo + 0x58));
    }
    return NULL;
}

/* After Options::describe: the rank, the way to the next one and the bonuses it gives now. */
__attribute__((used)) int c_rk_describe(void *self, int *a)
{
    ((describe_t)g_orig_describe)(self, (void *)a[0], a[1], a[2], a[3]);
    const int *opt = (const int *)a[0];
    if (cityrepair_describe(opt)) /* cityrepair.c shares this hook: its switch's own text */
        return 0;
    char *u = opt && g_ranks_enabled ? described_unit(opt) : NULL;
    if (!shows_rank(u))
        return 0;
    int l = level_u(u);
    char line[512];
    snprintf(line, sizeof line, lang_text("Rank: {%s} (level %d of %d)"), lang_text(rank_name(l)), l,
             max_level(u));
    tip_line(line);
    if (l < max_level(u))
        snprintf(line, sizeof line, lang_text("Experience: %d%% of the way to %s"), progress_u(u) / 10,
                 lang_text(rank_name(l + 1)));
    else
        snprintf(line, sizeof line, "%s", lang_text("Experience: highest rank"));
    tip_line(line);
    if (!l) {
        tip_line(*(int *)(type(u) + T_ATTACK) ? "Earns experience from the damage it deals."
                                              : "Earns experience from its abilities.");
        return 0;
    }
    int armed = *(int *)(type(u) + T_ATTACK) != 0, n = 0;
    n += snprintf(line + n, sizeof line - n, "%s", lang_text("Bonuses:"));
    if (armed)
        n += snprintf(line + n, sizeof line - n, lang_text(" +%d%% attack,"), g_attack_step * l);
    n += snprintf(line + n, sizeof line - n, lang_text(" +%d%% hit points"), g_hits_step * l);
    if (l >= 3)
        n += snprintf(line + n, sizeof line - n, "%s", lang_text(", +1 sight"));
    tip_line(line);
    n = 0;
    line[0] = 0;
    if (armed && g_rate_step > 0)
        n += snprintf(line + n, sizeof line - n, lang_text("+%d%% attack rate, "), rate_pct(l));
    if (l >= 6)
        n += snprintf(line + n, sizeof line - n, lang_text("-%d%% damage taken, "), l >= 10 ? 20 : 10);
    if (armed && l >= 7 && (!is_unit(u) || ((fn0_t)VA(VA_MAX_RANGE))(u) > 0))
        n += snprintf(line + n, sizeof line - n, "%s", lang_text("+1 range, "));
    if (l >= HEAL_LEVEL && is_unit(u))
        n += snprintf(line + n, sizeof line - n, "%s", lang_text("heals out of combat, "));
    if (n > 2) {
        line[n - 2] = 0; /* the last ", " */
        tip_line(line);
    }
    return 0;
}

/* ---- defensive buildings ---------------------------------------------------------------- */

/* Wall::update_hits(int): Object::update_hits recomputes the max hits (+0x20) from the type and
   the techs, then the hit points built so far (+0x54) by the construction progress; it returns
   +0x20 for a non-zero argument, else +0x54. Both grow by the rank, so a finished building stays
   finished (damage at or above +0x54 empties the garrison). */
__attribute__((used)) int c_rk_wall_hits(char *self, int *a)
{
    int hits = ((fn1_t)g_orig_wall_hits)(self, a[0]);
    /* the Tank Traps' hit points by their owner's age, the rank and the Survival card Thick
       Walls (cards.c, tenths of a percent), all at once */
    int pct = entrench_traps_hits_pct(self), l = level(self), c = card(self, CS_HITS);
    if (pct == 100 && !l && !c)
        return hits;
    long long m = (long long)pct * (100 + g_hits_step * l) * (1000 + c); /* / 10^7 */
    int full = *(int *)(self + OBJ_MAXHITS), built = *(int *)(self + B_CONSTRUCT);
    int up = (int)(full * m / 10000000);
    built = built >= full ? up : (int)(built * m / 10000000);
    *(int *)(self + OBJ_MAXHITS) = up;
    *(int *)(self + B_CONSTRUCT) = built;
    return a[0] ? up : built;
}

/* WallData::los(), replaced whole (5 bytes): its sight byte, +1 from level 3. */
__attribute__((used)) int c_rk_wall_los(char *self)
{
    int r = (signed char)self[0x3C];
    return (level(self) >= 3 ? r + 1 : r) + card(self, CS_LOS);
}

__attribute__((used)) int c_rk_brange(char *self, int *a)
{
    (void)a;
    return ranks_range(self, ((fn0_t)g_orig_brange)(self));
}

/* The insignia over a building's health bar (prodbar.c's hook of WallOut::draw_health_bar). */
void c_rk_bar(char *u, short *rect);
void ranks_draw_insignia(char *o, short *rect)
{
    if (g_ranks_enabled && o)
        c_rk_bar(o, rect);
}

/* ---- hooks -------------------------------------------------------------------------------- */

/* The captain works out the squad's hit points and copies them to its soldiers (+0x20); the
   others ask the captain. */
__attribute__((used)) int c_hero_uber(char *u, char *t);

__attribute__((used)) int c_rk_unit_hits(char *self, int *a)
{
    int hits = ((fn1_t)g_orig_unit_hits)(self, a[0]);
    int l = level(self);
    if (!is_captain(self))
        return hits;
    /* a hero's squad is bigger than its type's (survival.c, c_hero_uber): the game
       counts the squad's hit points for the type's size (the user, 2026-11-10: "170/85"), so they
       grow in the same proportion */
    int big = hero_slot(self) >= 0; /* the infantry's six and the riders' two alike */
    if (big) {
        char *t = type(self);
        int base = t ? *(int *)(t + T_UBER) : 1, want = c_hero_uber(self, t);
        if (base > 0 && want > base)
            hits = hits * want / base;
    }
    int c = card(self, CS_HITS); /* the Survival cards (cards.c), tenths of a percent */
    if (!l && !big && !c)
        return hits;
    hits = hits * (100 + g_hits_step * l) / 100;
    if (c)
        hits = hits * (1000 + c) / 1000;
    if (rider_power(self) > 100)
        hits = hits * rider_power(self) / 100;
    char *soldiers[32];
    int n = squad(self, soldiers);
    for (int i = 0; i < n; i++)
        *(int *)(soldiers[i] + OBJ_MAXHITS) = hits;
    return hits;
}

__attribute__((used)) int c_rk_los(char *self, int *a)
{
    (void)a;
    int r = ((fn0_t)g_orig_los)(self);
    return (level(self) >= 3 ? r + 1 : r) + card(self, CS_LOS);
}

/* Colors as the game's: r, g, b, a, then 555 and 565 words, then 2. */
static void make_color(unsigned char *c, int r, int g, int b)
{
    c[0] = (unsigned char)r, c[1] = (unsigned char)g, c[2] = (unsigned char)b, c[3] = 0xFF;
    *(unsigned short *)(c + 4) = (unsigned short)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    *(unsigned short *)(c + 6) = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    *(int *)(c + 8) = 2;
}

void rk_draw(void *gp, short *rect, float frac, const void *fill, const void *back);
__asm__(".intel_syntax noprefix\n"
        "_rk_draw:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    movss xmm2, dword ptr [ebp+16]\n"
        "    push dword ptr [ebp+24]\n"
        "    push dword ptr [ebp+20]\n"
        "    push dword ptr [ebp+12]\n"
        "    mov ecx, [ebp+8]\n"
        "    call [_g_rk_gp_bar]\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

/* The insignia beside the health bars drawn since the last map window redraw: the bar's left
   edge and middle, in the coordinates the health bar is drawn in (the console's). They are
   textures of the interface (ui-theme rank_icons.tga), so they are drawn in that redraw
   (rankfx.c), in front of the map, like the production icons. */
#define MAX_INSIGNIA 256
static struct {
    short left, mid;
    char level;
} g_ins[MAX_INSIGNIA];
static int g_ins_n;

void ranks_draw_insignia_queue(void)
{
    int n = g_ins_n;
    g_ins_n = 0;
    if (!n)
        return;
    const void *tex[LEVELS + 1] = { 0 };
    for (int l = 1; l <= LEVELS; l++) {
        char name[16];
        snprintf(name, sizeof name, "rank_%d", l);
        tex[l] = qol_iface_tex(name);
    }
    float h = 28.0f * rankfx_zoom(); /* 28 px at the closest zoom, as tall as a portrait's insignia */
    h = (h < 18.0f ? 18.0f : h > 30.0f ? 30.0f : h) * (float)g_insignia_pct / 100.0f;
    float w = h * 48.0f / 56.0f;
    for (int i = 0; i < n; i++) {
        if (!tex[(int)g_ins[i].level])
            continue;
        float x = g_ins[i].left, y = g_ins[i].mid;
        rankfx_console_to_rm(&x, &y);
        rf_add(tex[(int)g_ins[i].level], x - 3.0f - w, y - h * 0.5f, w, h, 0xFFFFFFFFu);
    }
}

/* After the unit's health bar (rect: left, right, top, bottom): the rank's insignia on its left.
   Without the insignia textures (no interface theme), 1-3 bars of the tier's colour, stacked
   upwards (4 gold ones at level 10). */
__attribute__((used)) void c_rk_bar(char *u, short *rect)
{
    int l = level(u);
    if (!l)
        return;
    static int have_icons = -1;
    if (have_icons < 0) {
        have_icons = qol_iface_tex("rank_1") != NULL;
        logf_("ranks: insignia beside the health bars %s", have_icons ? "as icons" : "as bars (no rank_icons)");
    }
    if (have_icons) {
        if (g_ins_n < MAX_INSIGNIA) {
            g_ins[g_ins_n].left = rect[0];
            g_ins[g_ins_n].mid = (short)((rect[2] + rect[3]) / 2);
            g_ins[g_ins_n].level = (char)ins_level(l);
            g_ins_n++;
        }
        return;
    }
    static unsigned char tiers[3][12];
    if (!tiers[0][3]) {
        make_color(tiers[0], 205, 127, 50);  /* bronze */
        make_color(tiers[1], 205, 210, 220); /* silver */
        make_color(tiers[2], 255, 205, 40);  /* gold */
    }
    int tier = tier_of(l), bars = bars_of(l);
    int w = rect[1] - rect[0], h = rect[3] - rect[2];
    int bw = 3 + w / 5, bh = h < 2 ? 2 : h;
    void *gp = *(void **)VA(VA_GRAPHIC_PIECES);
    for (int k = 0; k < bars; k++) {
        short bottom = (short)(rect[3] - k * (bh + 1));
        short r[4] = { (short)(rect[0] - 2 - bw), (short)(rect[0] - 2), (short)(bottom - bh), bottom };
        rk_draw(gp, r, 1.0f, tiers[tier], (void *)VA(VA_BLACK));
    }
}

/* A unit with craft points (General, Spy, patriots, ...): a blue bar of what is left right
   under its health bar, as wide. UnitData::mana() is the full bar (0 = none), +0x96 what has
   been spent (a decoy's age instead, so decoys get none). */
#define VA_UNIT_MANA 0x00609A50u /* UnitData::mana() const, thiscall */
#define U_MANA_USED  0x96
static void mana_bar(char *u, short *rect)
{
    if (!g_mana_bar || (*(unsigned *)(u + U_FLAGS) & 1))
        return;
    int full = ((fn0_t)VA(VA_UNIT_MANA))(u);
    if (full <= 0)
        return;
    int left = full - *(short *)(u + U_MANA_USED);
    if (left < 0)
        left = 0;
    static unsigned char blue[12];
    if (!blue[3])
        make_color(blue, 60, 140, 255);
    int h = rect[3] - rect[2];
    short r[4] = { rect[0], rect[1], (short)(rect[3] + 1), (short)(rect[3] + 1 + (h < 2 ? 2 : h)) };
    rk_draw(*(void **)VA(VA_GRAPHIC_PIECES), r, left >= full ? 1.0f : (float)left / full, blue,
            (void *)VA(VA_BLACK));
}

/* After a unit's health bar (hk_rk_hb): its rank and its craft points. */
__attribute__((used)) void c_rk_unit_bar(char *u, short *rect)
{
    c_rk_bar(u, rect);
    mana_bar(u, rect);
}

void hk_rk_unit_hits(void);
void hk_rk_los(void);
void hk_rk_hb(void);
void hk_rk_cast(void);
void hk_rk_describe(void);
void hk_rk_wall_hits(void);
void hk_rk_wall_los(void);
void hk_rk_brange(void);
void hk_rk_rech_attack(void);
void hk_rk_rech_launch(void);
void hk_rk_shown_attack(void);

#define RK_STUB(name, cfn, ret)                                                              \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    lea eax, [ebx+36]\n"                                                               \
    "    push eax\n"                                                                        \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

__asm__(".intel_syntax noprefix\n"
        RK_STUB("hk_rk_unit_hits", "c_rk_unit_hits", "ret 4")
        RK_STUB("hk_rk_los", "c_rk_los", "ret")
        RK_STUB("hk_rk_cast", "c_rk_cast", "ret 16")
        RK_STUB("hk_rk_describe", "c_rk_describe", "ret 16")
        RK_STUB("hk_rk_wall_hits", "c_rk_wall_hits", "ret 4")
        RK_STUB("hk_rk_brange", "c_rk_brange", "ret")
        /* WallData::los(): ecx = the building; only eax changes */
        ".globl _hk_rk_wall_los\n"
        "_hk_rk_wall_los:\n"
        "    push ecx\n"
        "    push edx\n"
        "    push ecx\n"
        "    call _c_rk_wall_los\n"
        "    add esp, 4\n"
        "    pop edx\n"
        "    pop ecx\n"
        "    ret\n"
        /* in place of `mov eax, [eax+0x1F4]` in Build::do_attack (esi = the building) */
        ".globl _hk_rk_rech_attack\n"
        "_hk_rk_rech_attack:\n"
        "    push ecx\n"
        "    push edx\n"
        "    push dword ptr [eax+0x1F4]\n"
        "    push esi\n"
        "    call _ranks_recharge\n"
        "    add esp, 8\n"
        "    pop edx\n"
        "    pop ecx\n"
        "    ret\n"
        /* in place of `mov edi, [eax+0x1F4]` in Object::do_launch (ecx = the shooting object) */
        ".globl _hk_rk_rech_launch\n"
        "_hk_rk_rech_launch:\n"
        "    push eax\n"
        "    push ecx\n"
        "    push edx\n"
        "    push dword ptr [eax+0x1F4]\n"
        "    push ecx\n"
        "    call _ranks_recharge\n"
        "    add esp, 8\n"
        "    mov edi, eax\n"
        "    pop edx\n"
        "    pop ecx\n"
        "    pop eax\n"
        "    ret\n"
        /* in place of `call [reg+0x120]` (attack()) in IFaceSelected::draw_stat, ecx = the object:
           the panel shows the bonus of the rank too; the caller keeps only eax */
        ".globl _hk_rk_shown_attack\n"
        "_hk_rk_shown_attack:\n"
        "    push ecx\n"
        "    mov eax, [ecx]\n"
        "    call [eax+0x120]\n"
        "    pop ecx\n"
        "    push eax\n"
        "    push ecx\n"
        "    call _ranks_damage\n"
        "    add esp, 8\n"
        "    ret\n"
        /* in place of GraphicPieces::draw_health_bar(rect, fill, back) with xmm2 = fraction;
           ebx = the unit */
        ".globl _hk_rk_hb\n"
        "_hk_rk_hb:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    push dword ptr [ebp+16]\n"
        "    push dword ptr [ebp+12]\n"
        "    push dword ptr [ebp+8]\n"
        "    call [_g_rk_gp_bar]\n"
        "    push eax\n"
        "    push dword ptr [ebp+8]\n"
        "    push ebx\n"
        "    call _c_rk_unit_bar\n"
        "    add esp, 8\n"
        "    pop eax\n"
        "    pop ebp\n"
        "    ret 12\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version (after
   kills_install, entrench_install and airdrop_install, which install the shared hooks). */
void ranks_install(void)
{
    /* glow_install(): the Patriots' light, off since 2026-11-10 (the user); the medallion over the
       hero's head marks it instead (ranks_draw_hero_marks) */
    uber_install();
    squad_ring_install();
    form_side_install();
    if (!g_ranks_enabled && !g_heal_enabled)
        return;
    unsigned char *mem = VirtualAlloc(NULL, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    /* SpellType::cast, shared with heal.c. push ebp / mov ebp,esp / push ecx / push ebx */
    static const unsigned char cast_sig[5] = { 0x55, 0x8B, 0xEC, 0x51, 0x53 };
    unsigned char *cast = (unsigned char *)VA(VA_CAST);
    if (memcmp(cast, cast_sig, sizeof cast_sig) == 0) {
        g_orig_cast = hook_trampoline(cast, 5, mem + 64);
        hook_jmp5(cast, hk_rk_cast);
    } else {
        logf_("ranks: signature mismatch at SpellType::cast, no experience from abilities and no Heal");
        g_heal_enabled = 0;
    }
    if (g_heal_enabled)
        logf_("heal: on");
    if (!g_ranks_enabled) {
        FlushInstructionCache(GetCurrentProcess(), NULL, 0);
        return;
    }
    /* push ebp / mov ebp,esp / push esi / mov esi,ecx */
    static const unsigned char hits_sig[6] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0xF1 };
    /* push esi / push edi / mov edi,ecx / movzx eax,byte [edi+9] */
    static const unsigned char los_sig[8] = { 0x56, 0x57, 0x8B, 0xF9, 0x0F, 0xB6, 0x47, 0x09 };
    unsigned char *hits = (unsigned char *)VA(VA_UNIT_HITS), *los = (unsigned char *)VA(VA_UNIT_LOS),
                  *hb = (unsigned char *)VA(VA_HB_CALL);
    if (memcmp(hits, hits_sig, sizeof hits_sig) != 0 || memcmp(los, los_sig, sizeof los_sig) != 0 || hb[0] != 0xE8 ||
        (UINT_PTR)(hb + 5 + *(INT32 *)(hb + 1)) != VA(VA_GP_BAR)) {
        logf_("ranks: signature mismatch, veteran ranks turned off");
        g_ranks_enabled = 0;
        return;
    }
    g_rk_gp_bar = VA(VA_GP_BAR);
    g_orig_unit_hits = hook_trampoline(hits, 6, mem);
    g_orig_los = hook_trampoline(los, 8, mem + 32);
    hook_jmp5(hits, hk_rk_unit_hits);
    hook_jmp5(los, hk_rk_los);
    /* push ebp / mov ebp,esp / push -1 (then the exception handler) */
    static const unsigned char desc_sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    unsigned char *desc = (unsigned char *)VA(VA_DESCRIBE);
    if (memcmp(desc, desc_sig, sizeof desc_sig) == 0) {
        g_orig_describe = hook_trampoline(desc, 5, mem + 96);
        hook_jmp5(desc, hk_rk_describe);
    } else
        logf_("ranks: signature mismatch at Options::describe, no rank in the tooltips");

    /* Defensive buildings. push ebp / mov ebp,esp / push ecx / push ebx / push esi / push edi */
    static const unsigned char bfn_sig[7] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x56, 0x57 };
    static const unsigned char blos_sig[5] = { 0x0F, 0xBE, 0x41, 0x3C, 0xC3 };
    static const unsigned char rech_a_sig[6] = { 0x8B, 0x80, 0xF4, 0x01, 0x00, 0x00 };
    static const unsigned char rech_l_sig[6] = { 0x8B, 0xB8, 0xF4, 0x01, 0x00, 0x00 };
    unsigned char *whits = (unsigned char *)VA(VA_WALL_HITS), *wlos = (unsigned char *)VA(VA_WALL_LOS),
                  *brange = (unsigned char *)VA(VA_BUILD_RANGE), *ra = (unsigned char *)VA(VA_RECH_ATTACK),
                  *rl = (unsigned char *)VA(VA_RECH_LAUNCH);
    if (memcmp(whits, bfn_sig, 7) || memcmp(brange, bfn_sig, 7) || memcmp(wlos, blos_sig, 5) ||
        memcmp(ra, rech_a_sig, 6) || memcmp(rl, rech_l_sig, 6)) {
        logf_("ranks: signature mismatch in the building code, defensive buildings do not rank");
    } else {
        g_orig_wall_hits = hook_trampoline(whits, 5, mem + 128);
        g_orig_brange = hook_trampoline(brange, 5, mem + 160);
        hook_jmp5(whits, hk_rk_wall_hits);
        hook_jmp5(brange, hk_rk_brange);
        hook_jmp5(wlos, hk_rk_wall_los);
        const void *rech_to[2] = { (const void *)hk_rk_rech_attack, (const void *)hk_rk_rech_launch };
        unsigned char *rech_at[2] = { ra, rl };
        for (int i = 0; i < 2; i++) {
            DWORD prot;
            VirtualProtect(rech_at[i], 6, PAGE_EXECUTE_READWRITE, &prot);
            rech_at[i][0] = 0xE8;
            *(INT32 *)(rech_at[i] + 1) = (INT32)((UINT_PTR)rech_to[i] - (UINT_PTR)(rech_at[i] + 5));
            rech_at[i][5] = 0x90;
            VirtualProtect(rech_at[i], 6, prot, &prot);
        }
        logf_("ranks: defensive buildings rank too (reference cost at least %d)", g_build_min_cost);
    }
    /* The panel's attack: base + (attack() - base), now with the rank's bonus in attack(). */
    int shown = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(g_shown_attack_at[i]);
        if (at[0] != 0xFF || (at[1] != 0x90 && at[1] != 0x92) || *(INT32 *)(at + 2) != 0x120)
            continue;
        DWORD prot;
        VirtualProtect(at, 6, PAGE_EXECUTE_READWRITE, &prot);
        at[0] = 0xE8;
        *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_rk_shown_attack - (UINT_PTR)(at + 5));
        at[5] = 0x90;
        VirtualProtect(at, 6, prot, &prot);
        shown++;
    }
    if (shown < 4)
        logf_("ranks: signature mismatch in IFaceSelected::draw_stat, %d of 4 attack values show the rank", shown);
    DWORD old;
    VirtualProtect(hb, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(hb + 1) = (INT32)((UINT_PTR)hk_rk_hb - (UINT_PTR)(hb + 5));
    VirtualProtect(hb, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("ranks: on (damage +%d%%, hit points +%d%%, attack rate +%d%% per level, healing %d%%/s after %d s, "
          "abilities %d%%)",
          g_attack_step, g_hits_step, g_rate_step, g_heal_pct, g_calm_s, g_ability_pct);
}
