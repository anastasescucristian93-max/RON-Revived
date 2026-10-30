/*
 * Veteran ranks: military units earn experience from the damage they deal and rise through 10
 * levels in 3 tiers (bronze, silver, gold; level 10 is gold with a star). Each level gives +4 %
 * attack and +4 % hit points; level 3 +1 line of sight, 5 +10 % attack rate, 6 -10 % damage taken,
 * 7 +1 range (ranged units), 8 +20 % attack rate, 9 slow healing out of combat, 10 -20 % damage
 * taken. [ranks] xp_gain_percent scales every gain (115 = 15 % faster). The rank stays when the unit's line is
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
#define U_CAPTAIN    0x8E /* word: bit 15 = captain, else the captain's id */
#define U_NEXT       0x90 /* short: next soldier of the squad, -1 = last */
#define T_COST       0x18 /* int[6]: the base cost */
#define T_ATTACK     0x1E8
#define T_UBER       0x308 /* UBER_SIZE: soldiers per squad */
#define T_RECHARGE   0x1F4
#define B_XP         0x65  /* buildings: experience / 16 (this mod) */
#define B_CONSTRUCT  0x54  /* buildings: construct_hits, the hit points so far built */
#define MAX_BUILDS   8192  /* building ids FIRST_BUILD .. FIRST_BUILD + MAX_BUILDS - 1 kept */
#define VT_MAX_HITS  0x11C
#define VT_UPDATE_HITS 0x15C
#define OBJS_UNITS_N 0x15C /* + 4 * p: unit ids 0 .. n-1 */
#define FIRST_BUILD  2000
#define XP_SHIFT     20
#define XP_MAX       0xFFF
#define LEVELS       10
#define HEAL_LEVEL   9

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);

int g_ranks_enabled;
static int g_mana_bar = 1;
static int g_insignia_pct = 100; /* [ranks] insignia_size: the insignia beside the health bar */
static int g_attack_step = 4, g_hits_step = 4, g_heal_pct = 1, g_calm_s = 10, g_ability_pct = 35;
static int g_build_min_cost = 150, g_xp_pct = 115;
static void *g_orig_unit_hits, *g_orig_los, *g_orig_cast, *g_orig_describe, *g_orig_wall_hits, *g_orig_brange;
UINT_PTR g_rk_gp_bar;

/* experience (1/256 of the unit's cost) needed for levels 1..10; level 10 is the most the 12 bits
   hold (XP_MAX) */
static const int g_need[LEVELS + 1] = { 0, 128, 256, 512, 768, 1152, 1536, 2048, 2688, 3456, XP_MAX };
static const char *const g_name[LEVELS + 1] = { "Private", "Private First Class", "Corporal", "Sergeant",
                                                "Staff Sergeant", "Master Sergeant", "Lieutenant", "Captain",
                                                "Major", "Colonel", "Brigadier General" };
static int g_last[8][FIRST_BUILD]; /* frame of each unit's last hit given or taken (not saved) */
static unsigned char g_bfrac[8][MAX_BUILDS]; /* buildings: experience below 16 (not saved) */

void ranks_load_config(const char *ini_path)
{
    g_ranks_enabled = GetPrivateProfileIntA("gameplay", "ranks", 1, ini_path);
    g_mana_bar = GetPrivateProfileIntA("qol", "mana_bar", 1, ini_path);
    g_insignia_pct = GetPrivateProfileIntA("ranks", "insignia_size", 100, ini_path);
    if (g_insignia_pct < 50 || g_insignia_pct > 300)
        g_insignia_pct = 100;
    g_attack_step = GetPrivateProfileIntA("ranks", "attack_percent_per_level", g_attack_step, ini_path);
    g_hits_step = GetPrivateProfileIntA("ranks", "hits_percent_per_level", g_hits_step, ini_path);
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
        return (int)(*(unsigned *)(u + U_FLAGS2) >> XP_SHIFT);
    if (!ranked_build(u))
        return 0;
    return ((unsigned char)u[B_XP] << 4) + g_bfrac[owner(u)][id_of(u) - FIRST_BUILD];
}

static void set_xp(char *u, int xp)
{
    if (is_unit(u)) {
        unsigned *f = (unsigned *)(u + U_FLAGS2);
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

/* The unit's or defensive building's level, 0 for other objects and when the mod is off. */
static int level(char *o)
{
    return g_ranks_enabled && o && (is_unit(o) || ranked_build(o)) ? level_of_xp(xp_of(o)) : 0;
}

static int is_captain(char *u) { return (*(unsigned short *)(u + U_CAPTAIN) & 0x8000) != 0; }
static char *captain_of(char *u)
{
    if (!is_unit(u) || is_captain(u))
        return u;
    char *c = object(owner(u), *(short *)(u + U_CAPTAIN));
    return c ? c : u;
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

static long long cost(char *t)
{
    long long c = 0;
    for (int r = 0; r < 6; r++)
        if (((int *)(t + T_COST))[r] > 0)
            c += ((int *)(t + T_COST))[r];
    return c;
}

/* ---- the bonuses (also called from entrench.c and kills.c) ------------------------------- */

int ranks_damage(char *attacker, int dmg)
{
    int l = level(attacker);
    return l && dmg > 0 ? dmg * (100 + g_attack_step * l) / 100 : dmg;
}

int ranks_recharge(char *u, int r)
{
    int l = level(u), rate = l >= 8 ? 20 : l >= 5 ? 10 : 0;
    if (!rate || r <= 0)
        return r;
    r = r * 100 / (100 + rate);
    return r < 1 ? 1 : r;
}

int ranks_range(char *u, int r) { return r > 0 && level(u) >= 7 ? r + 1 : r; }

/* Before Object::take_damage: -10 % from level 6, -20 % at level 10, for a hit by an object (not
   attrition). *whole = hit points, *part = sixteenths. */
void ranks_before_hit(char *victim, int *whole, int *part, int attacker_id)
{
    if (attacker_id < 0 || level(victim) < 6)
        return;
    int t = *whole * 16 + (signed char)*part;
    if (t <= 0)
        return;
    t = t * (level(victim) >= 10 ? 80 : 90) / 100;
    *whole = t >> 4;
    *part = t & 15;
}

/* g / 256 of the unit's cost more experience for its whole squad. */
static void add_xp(char *att, long long g)
{
    if (g <= 0)
        return;
    char *cap = captain_of(att), *soldiers[32];
    int old = xp_of(cap), xp = old + (int)(g > XP_MAX ? XP_MAX : g);
    if (xp > XP_MAX)
        xp = XP_MAX;
    if (xp == old)
        return;
    int n = squad(cap, soldiers);
    for (int i = 0; i < n; i++)
        set_xp(soldiers[i], xp);
    int was = level_of_xp(old), is = level_of_xp(xp);
    if (is != was) {
        ((fn1_t)(*(void ***)cap)[VT_UPDATE_HITS / 4])(cap, 0); /* the new hit points */
        if (is > was)
            rankfx_promoted(cap, is); /* the insignia over its head (rankfx.c) */
        static int logged;
        if (logged < 40) {
            logged++;
            logf_("ranks: player %d unit %d (squad of %d) is now level %d, %s (%d / 256 of its cost)", owner(cap),
                  id_of(cap), n, is, g_name[is], xp);
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
    if (!g_ranks_enabled || g_heal_pct <= 0)
        return;
    int t = now();
    if (t % 15)
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
static int progress_permille(int xp)
{
    int l = level_of_xp(xp);
    if (l >= LEVELS)
        return 1000;
    return (xp - g_need[l]) * 1000 / (g_need[l + 1] - g_need[l]);
}

static void solid(const short *r, unsigned color)
{
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, r, color, RENDER_SOLID);
}

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
    int xp = xp_of(u), l = level_of_xp(xp);
    char name[16];
    snprintf(name, sizeof name, "rank_%d", l);
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
    int bh = h / 12 < 3 ? 3 : h / 12, x0 = 2, x1 = w - 2, y1 = h - 2, y0 = y1 - bh;
    short back[4] = { (short)x0, (short)x1, (short)y0, (short)y1 };
    solid(back, 0xC0101010);
    int fill = (x1 - x0 - 2) * progress_permille(xp) / 1000;
    if (fill > 0) {
        short bar[4] = { (short)(x0 + 1), (short)(x0 + 1 + fill), (short)(y0 + 1), (short)(y1 - 1) };
        solid(bar, l >= LEVELS ? g_tier_rgba[2] : 0xFF20E6FF); /* yellow, gold when complete */
    }
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
    char *u = opt && g_ranks_enabled ? described_unit(opt) : NULL;
    if (!shows_rank(u))
        return 0;
    int xp = xp_of(u), l = level_of_xp(xp);
    char line[512];
    snprintf(line, sizeof line, lang_text("Rank: {%s} (level %d of %d)"), lang_text(g_name[l]), l, LEVELS);
    tip_line(line);
    if (l < LEVELS)
        snprintf(line, sizeof line, lang_text("Experience: %d%% of the way to %s"), progress_permille(xp) / 10,
                 lang_text(g_name[l + 1]));
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
    if (armed && l >= 5)
        n += snprintf(line + n, sizeof line - n, lang_text("+%d%% attack rate, "), l >= 8 ? 20 : 10);
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
    int l = level(self);
    if (!l)
        return hits;
    int full = *(int *)(self + OBJ_MAXHITS), built = *(int *)(self + B_CONSTRUCT);
    int up = full * (100 + g_hits_step * l) / 100;
    built = built >= full ? up : built * (100 + g_hits_step * l) / 100;
    *(int *)(self + OBJ_MAXHITS) = up;
    *(int *)(self + B_CONSTRUCT) = built;
    return a[0] ? up : built;
}

/* WallData::los(), replaced whole (5 bytes): its sight byte, +1 from level 3. */
__attribute__((used)) int c_rk_wall_los(char *self)
{
    int r = (signed char)self[0x3C];
    return level(self) >= 3 ? r + 1 : r;
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
__attribute__((used)) int c_rk_unit_hits(char *self, int *a)
{
    int hits = ((fn1_t)g_orig_unit_hits)(self, a[0]);
    int l = level(self);
    if (!l || !is_captain(self))
        return hits;
    hits = hits * (100 + g_hits_step * l) / 100;
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
    return level(self) >= 3 ? r + 1 : r;
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
            g_ins[g_ins_n].level = (char)(l > LEVELS ? LEVELS : l);
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
    logf_("ranks: on (attack +%d%% and hit points +%d%% per level, healing %d%%/s after %d s, abilities %d%%)",
          g_attack_step, g_hits_step, g_heal_pct, g_calm_s, g_ability_pct);
}
