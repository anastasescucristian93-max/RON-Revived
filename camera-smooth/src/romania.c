/*
 * Romania, a 25th nation (romania mod). The exe was built for 24 nations (enum TribeIndex:
 * NUM_TRIBES = 24, then RANDOM_TRIBE = 24, RANDOM_OFFENSIVE 25, RANDOM_DEFENSIVE 26,
 * RANDOM_ECONOMIC 27, RANDOM_XPACK 28). Romania becomes tribe 24 and every Random choice
 * moves up by one:
 *
 *  - Tribes: the array is allocated with 25 slots, the last one the "Random" pseudo-nation
 *    (named in Tribes::init, shown for a player whose random nation is not picked yet). It
 *    gets 26; Tribes::init accepts exactly 25 <TRIBE> entries in rules.xml (it wanted 24),
 *    reads 25 files and names slot 25 "Random". Loops over all nations (stride 0x5F0, end
 *    0x8E80) end at 0x9470.
 *  - Random choices: the ids 24..28 in the setup screen's nation combos, the player
 *    profile's defaults, Game::init_tribes (which turns them into nations: plain random is
 *    "% 24", now "% 25") become 25..29. Romania is also one of the Random Defensive nations
 *    (an eighth entry in that switch).
 *  - Nation power: the game asks LeaderData::has_tribe_bonus(tribe) wherever a power
 *    applies, each call site one piece of one nation's power. For a Romanian player the
 *    hook answers yes at the call sites of the pieces Romania borrows (k_bonus below, the
 *    switches in romania.ini); everything else is the game's own answer.
 *
 * The data side (rules.xml with Romania's <TRIBE>, tribes\romania.xml, 25-bit TRIBE_MASKs,
 * help.xml TRIBE24) is romania\tools\build_data.py's. The patches go in only when romania.ini
 * is in the game folder and the rules.xml the game will read lists 25 nations; with the
 * game's own rules.xml (24) nothing changes, or the game would refuse to start.
 * Everything runs before the exe's static constructors (the Tribes array is built by one).
 * Every player of a multiplayer game needs the mod.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define ROMANIA        24
#define VA_HAS_BONUS   0x006E1370u /* LeaderData::has_tribe_bonus(int tribe), thiscall, ret 4 */
#define VA_DEF_COUNT   0x0058A7CEu /* Game::init_tribes, Random Defensive: mov ecx, 7 (count) */
#define VA_DEF_LAST    0x0058A7D5u /* cmp edx, 6 */
#define VA_DEF_SWITCH  0x0058A7DEu /* jmp [edx*4 + 0x58AB84] */
#define VA_DEF_TABLE   0x0058AB84u /* the 7 cases: Maya, Koreans, Chinese, Russians, Indians, Persians, Iroquois */
#define VA_PICKED      0x0058A995u /* where every case goes after storing the nation */
#define VA_LEADER_TRIBE 0x00E3A39Cu /* leaders + 0xC: the case code stores [esi + this] */
#define DEF_CASES      7

int g_romania_enabled;
int g_num_tribes = 24; /* 25 once the patches are in: flags.c and nation_grid.c ask */
void *g_orig_has_bonus;

typedef int(__attribute__((thiscall)) *has_bonus_t)(void *leader, int tribe);

typedef struct {
    UINT_PTR at;
    unsigned char orig[7], repl[7];
    int len;
    const char *what;
} rpatch_t;

#define CMP_END(reg) { 0x81, reg, 0x80, 0x8E, 0x00, 0x00 }, { 0x81, reg, 0x70, 0x94, 0x00, 0x00 }, 6 /* end 24 -> 25 nations */
#define ESI 0xFE
#define EDI 0xFF
#define EBX 0xFB

static const rpatch_t k_patches[] = {
    /* the Tribes array: 25 -> 26 slots (Tribes::Tribes, a static constructor) */
    { 0x006F131Du, { 0x83, 0x7E, 0x08, 0x19 }, { 0x83, 0x7E, 0x08, 0x1A }, 4, "Tribes::Tribes capacity test" },
    { 0x006F1332u, { 0x6A, 0x19 }, { 0x6A, 0x1A }, 2, "Tribes::Tribes init(25)" },
    { 0x006F133Eu, { 0xBF, 0x19, 0x00, 0x00, 0x00 }, { 0xBF, 0x1A, 0x00, 0x00, 0x00 }, 5, "Tribes::Tribes size" },
    /* Tribes::init: 25 <TRIBE> entries, 25 files, "Random" into slot 25 */
    { 0x006EF576u, { 0x83, 0x7D, 0xC8, 0x18 }, { 0x83, 0x7D, 0xC8, 0x19 }, 4, "Tribes::init count" },
    { 0x006EF73Au, CMP_END(EDI), "Tribes::init FILE / KEY loop" },
    { 0x006EF8DDu, CMP_END(ESI), "Tribes::init parse loop" },
    { 0x006EF8FDu, { 0x81, 0xC1, 0x98, 0x8E, 0x00, 0x00 }, { 0x81, 0xC1, 0x88, 0x94, 0x00, 0x00 }, 6,
      "Tribes::init Random name (slot 24 -> 25)" },
    { 0x006EF404u, CMP_END(EDI), "Tribes::find" },
    /* rules data walked for saved games and checksums, and its log */
    { 0x005895E6u, CMP_END(EBX), "Game::walk_rules_data" },
    { 0x00588867u, CMP_END(EDI), "Game::log_rules_data" },
    /* Game::init_tribes: ids 25..29 are the Random choices, plain random picks one of 25 */
    { 0x0058A67Cu, { 0x83, 0xFA, 0x18 }, { 0x83, 0xFA, 0x19 }, 3, "init_tribes: nation or Random" },
    { 0x0058A690u, { 0x8D, 0x42, 0xE7 }, { 0x8D, 0x42, 0xE6 }, 3, "init_tribes: Random kind switch" },
    { 0x0058A6C1u, { 0xB9, 0x18, 0x00, 0x00, 0x00 }, { 0xB9, 0x19, 0x00, 0x00, 0x00 }, 5, "init_tribes: Offensive, any nation" },
    { 0x0058A7A2u, { 0xB9, 0x18, 0x00, 0x00, 0x00 }, { 0xB9, 0x19, 0x00, 0x00, 0x00 }, 5, "init_tribes: Defensive, any nation" },
    { 0x0058A84Bu, { 0xB9, 0x18, 0x00, 0x00, 0x00 }, { 0xB9, 0x19, 0x00, 0x00, 0x00 }, 5, "init_tribes: Economic, any nation" },
    { 0x0058A921u, { 0xB9, 0x18, 0x00, 0x00, 0x00 }, { 0xB9, 0x19, 0x00, 0x00, 0x00 }, 5, "init_tribes: X-Pack, any nation" },
    { 0x0058A988u, { 0xB9, 0x18, 0x00, 0x00, 0x00 }, { 0xB9, 0x19, 0x00, 0x00, 0x00 }, 5, "init_tribes: Random" },
    /* the setup screen: the combos' Random items and the nation list */
    { 0x005BFAE2u, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "setup_combos: Random item" },
    { 0x005BFB2Du, { 0x6A, 0x19 }, { 0x6A, 0x1A }, 2, "setup_combos: Random Offensive item" },
    { 0x005BFB78u, { 0x6A, 0x1A }, { 0x6A, 0x1B }, 2, "setup_combos: Random Defensive item" },
    { 0x005BFBC3u, { 0x6A, 0x1B }, { 0x6A, 0x1C }, 2, "setup_combos: Random Economic item" },
    { 0x005BFC0Eu, { 0x6A, 0x1C }, { 0x6A, 0x1D }, 2, "setup_combos: Random X-Pack item" },
    { 0x005BFC9Cu, CMP_END(ESI), "setup_combos: nation items" },
    { 0x005BFCBFu, { 0xB9, 0x18, 0x00, 0x00, 0x00 }, { 0xB9, 0x19, 0x00, 0x00, 0x00 }, 5, "setup_combos: Random selected" },
    { 0x005B5A5Eu, { 0x83, 0xB9, 0x68, 0x05, 0x00, 0x00, 0x18 }, { 0x83, 0xB9, 0x68, 0x05, 0x00, 0x00, 0x19 }, 7,
      "update_interface: nation combo on Random" },
    { 0x005BA66Eu, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "make_combo_change: nation to Random" },
    { 0x005BA8EBu, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "make_combo_change: nation to Random (2)" },
    { 0x005BBAC7u, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "process_check_change: nations to Random" },
    { 0x005BBB0Fu, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "process_check_change: nations to Random (2)" },
    { 0x005C33B5u, { 0x83, 0xFF, 0x18 }, { 0x83, 0xFF, 0x19 }, 3, "on_pulldown_change: Random tooltips" },
    { 0x005C33BEu, { 0x8D, 0x47, 0xE7 }, { 0x8D, 0x47, 0xE6 }, 3, "on_pulldown_change: Random kind" },
    { 0x005C380Eu, { 0x83, 0xFB, 0x18 }, { 0x83, 0xFB, 0x19 }, 3, "on_combo_move: Random tooltips" },
    { 0x005C3817u, { 0x8D, 0x43, 0xE7 }, { 0x8D, 0x43, 0xE6 }, 3, "on_combo_move: Random kind" },
    /* a nation's tooltip text is parsed (Tribe::parse) only for ids 0..23 */
    { 0x005C348Bu, { 0x83, 0xFF, 0x17 }, { 0x83, 0xFF, 0x18 }, 3, "on_pulldown_change: nation tooltip" },
    { 0x005C38FAu, { 0x83, 0xFB, 0x17 }, { 0x83, 0xFB, 0x18 }, 3, "on_combo_move: nation tooltip" },
    /* unit icons by the player's unit style, read only for tribes 0..23 */
    { 0x0090D4F1u, { 0x83, 0xF8, 0x18 }, { 0x83, 0xF8, 0x19 }, 3, "GraphicPieces::update_all_icons: unit style" },
    /* player profile defaults: Random */
    { 0x005CBA23u, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "PlayerProfile::load XPACK_QM_TRIBE" },
    { 0x005CDA0Du, { 0x6A, 0x18 }, { 0x6A, 0x19 }, 2, "PlayerProfile::load xpack_last_tribe" },
    /* end-of-game report nation lists, unit balance lookup by name */
    { 0x007B37F0u, CMP_END(ESI), "GameReportWin::exec nations (1)" },
    { 0x007B38A4u, CMP_END(ESI), "GameReportWin::exec nations (2)" },
    { 0x009B75D0u, CMP_END(ESI), "UnitBalance::init nation by name" },
    { 0x009B75D8u, { 0x83, 0xFB, 0x18 }, { 0x83, 0xFB, 0x19 }, 3, "UnitBalance::init not found" },
};

/* ---- the nation power ---------------------------------------------------------------- */

/* A piece of another nation's power that Romania has: the call sites (return addresses of
   their has_tribe_bonus calls) and the nation they ask about. */
typedef struct {
    const char *key; /* romania.ini [power] switch */
    int tribe;
    int on;
    UINT_PTR ret[12];
} piece_t;

static piece_t k_bonus[] = {
    /* Russian: attrition in the player's territory x (100 + RUSSIAN_ATTRITION) / 100
       (Leader::calc_attrition, asked of the territory's owner); the four attrition techs
       (Allegiance .. Nationalism, 0x254-0x257) gained free when their last preq comes
       (Leader::gain_tech) */
    { "attrition", 13, 1, { 0x006CDF37u, 0x006DF594u } },
    /* Russian: Oil rate x (100 + RUSSIAN_OIL) / 100 (LeaderData::calc_resource_bonuses).
       Left out: the Refinery without Electronics (has_preq 0x006DB914). */
    { "oil", 13, 1, { 0x006DB042u } },
    /* Roman: Forts and Towers ROMAN_FORT_COST % cheaper (TypeData::get_cost), Forts
       ROMAN_FORT_SPEED % faster to build (Wall::update_construct_time), +ROMAN_FORT_BORDERS
       (World::compute_reg_territory), the Fort's techs free (Leader::gain_tech). Left out:
       the Romans' early Forts and Towers (has_preq 0x006DB9AB, special_preq 0x006D7064). */
    { "forts", 6, 1, { 0x00665E5Au, 0x0063D6EEu, 0x006B0ECDu, 0x006DF6DAu } },
    /* Korean: Militia, Minuteman, Partisan gained as soon as their Age comes (Leader::gain_tech).
       Left out: Towers KOREAN_TOWER_COST % cheaper (0x00665D8A), which would stack with the
       Roman discount above. */
    { "partisans", 16, 1, { 0x006DF249u } },
};
#define PIECES (int)(sizeof k_bonus / sizeof k_bonus[0])

__attribute__((used)) int c_has_bonus(char *leader, UINT_PTR ret, int tribe)
{
    has_bonus_t orig = (has_bonus_t)g_orig_has_bonus;
    /* the game's own rules (nation powers switched off, ...) through Romania's own power */
    if (tribe != ROMANIA && orig(leader, ROMANIA)) {
        UINT_PTR r = ret - g_delta;
        for (int p = 0; p < PIECES; p++)
            if (k_bonus[p].on && k_bonus[p].tribe == tribe)
                for (int i = 0; i < 12 && k_bonus[p].ret[i]; i++)
                    if (k_bonus[p].ret[i] == r)
                        return 1;
    }
    return orig(leader, tribe);
}

void hk_has_bonus(void);

/* ecx = LeaderData, [esp] = return address, [esp+4] = tribe. Only Romanian players (tribe at
   +0xC) go to C; the call is frequent. */
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_has_bonus\n"
        "_hk_has_bonus:\n"
        "    cmp dword ptr [ecx+12], 24\n"
        "    jne 1f\n"
        "    push ecx\n"
        "    push edx\n"
        "    push dword ptr [esp+12]\n" /* tribe */
        "    push dword ptr [esp+12]\n" /* return address */
        "    push ecx\n"
        "    call _c_has_bonus\n"
        "    add esp, 12\n"
        "    pop edx\n"
        "    pop ecx\n"
        "    ret 4\n"
        "1:  jmp dword ptr [_g_orig_has_bonus]\n"
        ".att_syntax prefix\n");

/* ---- Romania's own units ---------------------------------------------------------------- */

/* A unit whose <GRAFT> names a generic one (a unique unit, or the same unit in other art) is
   what its nations get instead of the generic: UnitType::init writes it into each nation's map
   (Tribe +0x70, an int per unit type from 0x32: the type that nation builds for it) for every
   bit of its TRIBE_MASK, in a loop unrolled for exactly 24 nations; Types::finalize_grafting then
   takes the generic away from those nations. Romania (bit 24) lost the generic and never got its
   own: before finalize_grafting runs (after loading the rules, and after loading a saved game),
   Romania's map entries are written here the same way. */
#define VA_FINALIZE   0x00669840u /* Types::finalize_grafting(), thiscall */
#define VA_UNITTYPES  0x00C061FCu /* PtrArray<UnitType> &: +0x10 = UnitType * by type index */
#define VA_TRIBE_DATA 0x00E7FA34u /* Tribe data: 0x5F0 bytes each */
#define UT_MASK       0x10        /* TRIBE_MASK */
#define UT_GRAFT      0x25C       /* type index of the generic unit it stands for, -1 none */
#define TRIBE_MAP     0x70
#define FIRST_UNIT    0x32
#define END_UNIT      0x192

typedef int(__attribute__((thiscall)) *finalize_t)(void *self);
typedef void(__attribute__((thiscall)) *fn_self_t)(void *self);
void *g_orig_finalize;

__attribute__((thiscall)) static int finalize_grafting(void *self)
{
    char **units = *(char ***)(*(char **)VA(VA_UNITTYPES) + 0x10);
    int *map = (int *)(*(char **)VA(VA_TRIBE_DATA) + ROMANIA * 0x5F0 + TRIBE_MAP);
    int n = 0;
    for (int t = FIRST_UNIT; t < END_UNIT; t++) {
        char *u = units[t];
        if (!u)
            continue;
        int g = *(int *)(u + UT_GRAFT);
        if (g >= FIRST_UNIT && g < END_UNIT && g != t && (*(unsigned *)(u + UT_MASK) >> ROMANIA & 1)) {
            map[g - FIRST_UNIT] = t;
            n++;
        }
    }
    /* UnitType::init's third pass, also unrolled for 24: the other units that stand for a
       generic one Romania has its own of (other nations' looks of it) map to Romania's too */
    for (int g = FIRST_UNIT; g < END_UNIT; g++) {
        int own = map[g - FIRST_UNIT];
        if (own == g || !units[g] || (*(unsigned *)(units[g] + UT_MASK) >> ROMANIA & 1))
            continue;
        for (int t = FIRST_UNIT; t < END_UNIT; t++)
            if (t != g && units[t] && *(int *)(units[t] + UT_GRAFT) == g && !(*(unsigned *)(units[t] + UT_MASK) >> ROMANIA & 1))
                map[t - FIRST_UNIT] = own;
    }
    static int logged;
    if (!logged++)
        logf_("romania: %d units of Romania's own stand for generic ones", n);
    return ((finalize_t)g_orig_finalize)(self);
}

/* ---- loading screen ------------------------------------------------------------------- */

/* The loading screen shows the local player's unique units, one per "unique_rect" of its layout
   (the game's interface.xml has 8, Romania's 12: build_data.py). LoadingWin::prepare_unique_list
   lists every unique the nation can build; on_mouse_move then asks for a rect per entry and uses
   it without a NULL check, so a nation with more uniques than rects crashes the game as soon as
   the mouse moves there. The list is cut to the rects there are.
   The screen is drawn before Setup::build_game runs update_all_icons, so the units would show the
   icons of the generic ones they were copied from: a Romanian player's go in here already. */
#define VA_PREP_UNIQUES 0x00836830u /* LoadingWin::prepare_unique_list(), thiscall */
#define LW_UNIQUE_N     0x4D0       /* count on_mouse_move walks */
#define LW_UNIQUE_LIST  0x4B8       /* the list's count (data at +0x4C4), draw_special_unit_info walks it */
#define UNIQUE_RECTS    12

void *g_orig_prep_uniques;
static void loading_icons(void);

__attribute__((thiscall)) static void prepare_unique_list(char *self)
{
    ((fn_self_t)g_orig_prep_uniques)(self);
    loading_icons();
    if (*(int *)(self + LW_UNIQUE_N) > UNIQUE_RECTS)
        *(int *)(self + LW_UNIQUE_N) = UNIQUE_RECTS;
    if (*(int *)(self + LW_UNIQUE_LIST) > UNIQUE_RECTS)
        *(int *)(self + LW_UNIQUE_LIST) = UNIQUE_RECTS;
}

/* ---- unit names and icons ------------------------------------------------------------- */

/* Romania shares its unit types with the other nations, so its names and icons are put in
   per player. romania_units.ini (built by romania\tools\build_data.py): [names] internal NAME =
   shown name, [icons] NAME = atlas (0 / 1: TEX_ROMANIA_0 / _1 in interface.xml), column, row.
   - Icons: each unit type keeps an icon per player (8 x 3 entries: texture index, the _HALF
     texture's, column, row, x, y, clip), which GraphicPieces::update_all_icons fills from
     unit_graphics.xml by the player's unit style. Its wrapper puts Romania's in for every
     Romanian player.
   - Names: a type's shown name is one String (+0x74; +0x60 is the internal name the rules use).
     When the player at this computer is Romanian it holds the Romanian name, otherwise the
     game's. UnitData::say_name (the selected unit's name) gives every unit its owner's name.
   - By age: [late] NAME = first age (1 Ancient .. 8 Information), name. From that age on the
     unit has that name, and the icons of the game's graphics tiers that start there (AGE0 for
     ages 1-3, AGE3 for 4-5, AGE5 for 6-8: the three icon slots) stay the game's own. The Supply
     Wagon is an ox cart until the Enlightenment and the game's truck from the Industrial Age
     (the user, 2026-10-29). The shown name follows the age of the player here, checked every
     frame (romania_frame); say_name follows the owner's age. */
#define VA_TYPES        0x00C0A274u /* Type ** by type index */
#define VA_UPDATE_ICONS 0x0090D3F0u /* GraphicPieces::update_all_icons(int player, int), thiscall, ret 8 */
#define VA_SAY_NAME     0x006108A0u /* UnitData::say_name(String &), thiscall, ret 4 */
#define VA_TEX_LIST     0x00EBD1C4u /* NamedObjectArray<IFaceTexPacket *>: the interface textures */
#define VA_TEX_FIND     0x00459850u /* its find(const String &, int) */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 = the player at this computer */
#define VA_LEADERS      0x00E3A390u
#define VA_STR_CTOR     0x00A1D660u /* String::String(const char *) */
#define VA_STR_COPY     0x00A1D590u /* String::String(const String &) */
#define VA_STR_ASSIGN   0x00A1EEB0u /* String::operator=(const String &) */
#define VA_STR_CLOSE    0x00A1CF40u
#define VA_STR_AS_CHAR  0x0042D710u
#define TYPE_NAME       0x60 /* internal name (rules) */
#define TYPE_SHOWN      0x74 /* shown name */
#define ICON_TEX        0xF0 /* + 3 * player + slot; the fields below the same way */
#define ICON_HALF       0x108
#define ICON_COL        0x120
#define ICON_ROW        0x138
#define ICON_X          0x150 /* short, + 6 * player + 2 * slot */
#define ICON_Y          0x180
#define ICON_CLIP       0x1B0
#define LEADER_SIZE     0x6EEC
#define LD_SECURE       0x6EB8 /* the per-player state: +0xDC = age ^ AGE_KEY (LeaderData::get_age) */
#define STATE_AGE       0xDC
#define AGE_KEY         0x62766u
#define MAX_UNITS       (END_UNIT - FIRST_UNIT) /* every unit type: 160 cut off Romania's own (slots 335..349) */

typedef void(__attribute__((thiscall)) *fn_v_t)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_p_t)(void *self, const void *p);
typedef void *(__attribute__((thiscall)) *assign_t)(void *self, const void *src);
typedef const char *(__attribute__((thiscall)) *as_char_t)(const void *self);
typedef int(__attribute__((thiscall)) *find_t)(void *self, const void *name, int a);
typedef void(__attribute__((thiscall)) *icons_t)(void *self, int player, int b);
typedef void(__attribute__((thiscall)) *say_name_t)(void *self, void *out);

typedef struct {
    int type;
    int named;
    unsigned char game_name[32], ro_name[32]; /* game Strings (0x14 bytes) */
    int atlas, col, row;                      /* atlas -1: no icon */
    int late_age;                             /* 0: none; else the first age of late_name */
    unsigned char late_name[32];
} unit_t;

static unit_t g_units[MAX_UNITS];
static int g_unit_n = -1; /* -1: not matched to the types yet */
static int g_names_ro;    /* the shown names are Romanian now */
static int g_names_age;   /* the age the shown names were set for */
static char g_units_ini[MAX_PATH];
void *g_orig_update_icons, *g_orig_say_name;

static char *type_of(int t) { return (*(char ***)VA(VA_TYPES))[t]; }
static int tribe_of(int p) { return *(int *)(VA(VA_LEADERS) + p * 0x6EEC + 0xC); }
static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }

/* Current age of a player, 1 (Ancient) to 8 (Information); 0 when unknown (as entrench.c). */
static int player_age(int p)
{
    if (p < 0 || p > 7)
        return 0;
    char *state = *(char **)(VA(VA_LEADERS) + p * LEADER_SIZE + LD_SECURE);
    if (!state)
        return 0;
    unsigned age = *(unsigned *)(state + STATE_AGE) ^ AGE_KEY;
    return age < 8 ? (int)age + 1 : 0;
}

/* The first age of icon slot s (the graphics tiers AGE0, AGE3, AGE5). */
static int slot_age(int s) { return s == 0 ? 1 : s == 1 ? 4 : 6; }

static unit_t *unit_of(int t)
{
    for (int i = 0; i < g_unit_n; i++)
        if (g_units[i].type == t)
            return &g_units[i];
    return NULL;
}

/* Matches romania_units.ini to the unit types by internal name, once the types are loaded. */
static void match_units(void)
{
    static char names[32768], icons[16384], late[4096];
    GetPrivateProfileSectionA("names", names, sizeof names, g_units_ini);
    GetPrivateProfileSectionA("icons", icons, sizeof icons, g_units_ini);
    GetPrivateProfileSectionA("late", late, sizeof late, g_units_ini);
    g_unit_n = 0;
    for (int t = FIRST_UNIT; t < END_UNIT && g_unit_n < MAX_UNITS; t++) {
        char *type = type_of(t);
        if (!type)
            continue;
        const char *internal = ((as_char_t)VA(VA_STR_AS_CHAR))(type + TYPE_NAME);
        size_t len = strlen(internal);
        const char *ro = NULL, *icon = NULL;
        for (char *p = names; *p; p += strlen(p) + 1)
            if (!_strnicmp(p, internal, len) && p[len] == '=')
                ro = p + len + 1;
        for (char *p = icons; *p; p += strlen(p) + 1)
            if (!_strnicmp(p, internal, len) && p[len] == '=')
                icon = p + len + 1;
        if (!ro && !icon)
            continue;
        unit_t *u = &g_units[g_unit_n++];
        u->type = t;
        u->named = ro != NULL;
        if (ro) {
            ((fn_v_p_t)VA(VA_STR_COPY))(u->game_name, type + TYPE_SHOWN);
            ((fn_v_p_t)VA(VA_STR_CTOR))(u->ro_name, ro);
        }
        u->atlas = -1;
        if (icon && sscanf(icon, "%d,%d,%d", &u->atlas, &u->col, &u->row) != 3)
            u->atlas = -1;
        u->late_age = 0;
        for (char *p = late; *p; p += strlen(p) + 1) {
            int age, at = 0;
            if (_strnicmp(p, internal, len) || p[len] != '=' || sscanf(p + len + 1, "%d,%n", &age, &at) != 1 || !at ||
                age < 2 || age > 8 || !u->named)
                continue;
            u->late_age = age;
            ((fn_v_p_t)VA(VA_STR_CTOR))(u->late_name, p + len + 1 + at);
        }
    }
    logf_("romania: %d unit types with a Romanian name or icon", g_unit_n);
}

/* The name of u for a Romanian player in `age`. */
static const void *ro_name(const unit_t *u, int age)
{
    return u->late_age && age >= u->late_age ? u->late_name : u->ro_name;
}

static void set_names(int ro)
{
    int age = ro ? player_age(local_player()) : 0;
    for (int i = 0; i < g_unit_n; i++)
        if (g_units[i].named)
            ((assign_t)VA(VA_STR_ASSIGN))(type_of(g_units[i].type) + TYPE_SHOWN,
                                          ro ? ro_name(&g_units[i], age) : g_units[i].game_name);
    g_names_ro = ro;
    g_names_age = age;
}

/* Every frame (airdrop.c's Objects::process_all): the shown names follow the age of the player
   here once it changes. */
void romania_frame(void)
{
    if (!g_romania_enabled || g_unit_n <= 0 || !g_names_ro)
        return;
    if (player_age(local_player()) != g_names_age)
        set_names(1);
}

static int texture(const char *name)
{
    unsigned char s[32] = { 0 };
    ((fn_v_p_t)VA(VA_STR_CTOR))(s, name);
    int i = ((find_t)VA(VA_TEX_FIND))((void *)VA(VA_TEX_LIST), s, 0);
    ((fn_v_t)VA(VA_STR_CLOSE))(s);
    return i;
}

static void set_icons(int p)
{
    int tex[2][2]; /* atlas, full / half */
    for (int a = 0; a < 2; a++) {
        char name[32];
        snprintf(name, sizeof name, "TEX_ROMANIA_%d", a);
        tex[a][0] = texture(name);
        snprintf(name, sizeof name, "TEX_ROMANIA_%d_HALF", a);
        tex[a][1] = texture(name);
    }
    int n = 0;
    for (int i = 0; i < g_unit_n; i++) {
        unit_t *u = &g_units[i];
        if (u->atlas < 0 || u->atlas > 1 || tex[u->atlas][0] < 0 || tex[u->atlas][1] < 0)
            continue;
        char *type = type_of(u->type);
        for (int s = 0; s < 3; s++) {
            if (u->late_age && slot_age(s) >= u->late_age)
                continue; /* the game's icon of that tier (the truck) */
            int k = 3 * p + s;
            type[ICON_TEX + k] = (char)tex[u->atlas][0];
            type[ICON_HALF + k] = (char)tex[u->atlas][1];
            type[ICON_COL + k] = (char)u->col;
            type[ICON_ROW + k] = (char)u->row;
            *(short *)(type + ICON_X + 2 * k) = -1;
            *(short *)(type + ICON_Y + 2 * k) = -1;
            type[ICON_CLIP + k] = 1;
        }
        n++;
    }
    logf_("romania: player %d, %d Romanian icons", p, n);
}

/* After the game has given every player the icons of their unit style (start of a game, a
   loaded game): Romania's icons for Romanian players, the names for the player here. */
__attribute__((thiscall)) static void update_icons(void *self, int player, int b)
{
    ((icons_t)g_orig_update_icons)(self, player, b);
    if (g_unit_n < 0)
        match_units();
    for (int p = 0; p < 8; p++)
        if ((player < 0 || p == player) && (*(unsigned *)(VA(VA_LEADERS) + p * 0x6EEC) & 1) && tribe_of(p) == ROMANIA)
            set_icons(p);
    int me = local_player();
    set_names(me >= 0 && me < 8 && tribe_of(me) == ROMANIA);
}

/* The loading screen (above): Romania's icons for the player here, if Romanian. */
static void loading_icons(void)
{
    if (!g_orig_update_icons)
        return;
    if (g_unit_n < 0)
        match_units();
    int me = local_player();
    if (me >= 0 && me < 8 && tribe_of(me) == ROMANIA)
        set_icons(me);
}

/* The selected unit's name: its owner's nation decides, not this computer's player. */
__attribute__((thiscall)) static void say_name(char *unit, void *out)
{
    ((say_name_t)g_orig_say_name)(unit, out);
    if (g_unit_n <= 0)
        return;
    char *type = *(char **)(unit + 0x18);
    int t = *(int *)(type + 4), owner = *(unsigned char *)(unit + 9);
    if (t == 0x3D || t == 0x3E || t == 0x190) /* named by the game itself (Merchants) */
        return;
    int ro = owner < 8 && tribe_of(owner) == ROMANIA;
    unit_t *u = unit_of(t);
    if (!u || !u->named)
        return;
    if (ro != g_names_ro || (ro && u->late_age && (player_age(owner) >= u->late_age) != (g_names_age >= u->late_age)))
        ((assign_t)VA(VA_STR_ASSIGN))(out, ro ? ro_name(u, player_age(owner)) : u->game_name);
}

static void units_install(unsigned char *mem)
{
    snprintf(g_units_ini, sizeof g_units_ini, "%sromania_units.ini", g_dir);
    if (GetFileAttributesA(g_units_ini) == INVALID_FILE_ATTRIBUTES) {
        logf_("romania: no romania_units.ini, units keep the game's names and icons");
        return;
    }
    unsigned char *icons = (unsigned char *)VA(VA_UPDATE_ICONS), *name = (unsigned char *)VA(VA_SAY_NAME);
    static const unsigned char icons_sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    static const unsigned char name_sig[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    if (memcmp(icons, icons_sig, 5) || memcmp(name, name_sig, 9)) {
        logf_("romania: update_all_icons / say_name signature mismatch, units keep the game's names and icons");
        return;
    }
    g_orig_update_icons = hook_trampoline(icons, 5, mem);
    g_orig_say_name = hook_trampoline(name, 9, mem + 32);
    hook_jmp5(icons, update_icons);
    hook_jmp5(name, say_name);
    logf_("romania: unit names and icons hooked");
}

/* ---- install ------------------------------------------------------------------------- */

/* <TRIBE> entries in the TRIBES list of the rules.xml the game will read */
static int rules_tribes(void)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%smod_override\\Data\\rules.xml", g_dir);
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof path, "%sData\\rules.xml", g_dir);
        f = fopen(path, "rb");
    }
    if (!f)
        return -1;
    static char buf[256 * 1024];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    char *s = strstr(buf, "<TRIBES>"), *e = s ? strstr(s, "</TRIBES>") : NULL;
    if (!e)
        return -1;
    int count = 0;
    for (char *p = s; (p = strstr(p, "<TRIBE>")) && p < e; p++)
        count++;
    return count;
}

void romania_load_config(const char *dir)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%sromania.ini", dir);
    g_romania_enabled = GetFileAttributesA(ini) != INVALID_FILE_ATTRIBUTES;
    if (!g_romania_enabled)
        return;
    for (int p = 0; p < PIECES; p++)
        k_bonus[p].on = GetPrivateProfileIntA("power", k_bonus[p].key, 1, ini);
}

static void write_code(void *at, const void *bytes, int len)
{
    DWORD old;
    VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, len);
    VirtualProtect(at, len, old, &old);
}

/* Random Defensive: an eighth case, Romania. The switch reads its own copy of the table. */
static int add_defensive(unsigned char *mem)
{
    unsigned char *count = (unsigned char *)VA(VA_DEF_COUNT), *last = (unsigned char *)VA(VA_DEF_LAST);
    unsigned char *sw = (unsigned char *)VA(VA_DEF_SWITCH);
    static const unsigned char count_sig[5] = { 0xB9, 0x07, 0x00, 0x00, 0x00 }, last_sig[3] = { 0x83, 0xFA, 0x06 };
    if (memcmp(count, count_sig, 5) || memcmp(last, last_sig, 3) || sw[0] != 0xFF || sw[1] != 0x24 ||
        sw[2] != 0x95 || *(UINT32 *)(sw + 3) != VA(VA_DEF_TABLE))
        return 0;
    /* mov dword ptr [esi + leaders+0xC], 24 / jmp picked */
    unsigned char *stub = mem;
    stub[0] = 0xC7;
    stub[1] = 0x86;
    *(UINT32 *)(stub + 2) = (UINT32)VA(VA_LEADER_TRIBE);
    *(UINT32 *)(stub + 6) = ROMANIA;
    stub[10] = 0xE9;
    *(INT32 *)(stub + 11) = (INT32)(VA(VA_PICKED) - ((UINT_PTR)stub + 15));
    UINT32 *table = (UINT32 *)(mem + 16);
    memcpy(table, (void *)VA(VA_DEF_TABLE), 4 * DEF_CASES);
    table[DEF_CASES] = (UINT32)(UINT_PTR)stub;

    unsigned char c[5] = { 0xB9, DEF_CASES + 1, 0, 0, 0 }, l[3] = { 0x83, 0xFA, DEF_CASES };
    unsigned char s[7] = { 0xFF, 0x24, 0x95 };
    *(UINT32 *)(s + 3) = (UINT32)(UINT_PTR)table;
    write_code(count, c, 5);
    write_code(last, l, 3);
    write_code(sw, s, 7);
    return 1;
}

void romania_install(void)
{
    if (!g_romania_enabled)
        return;
    int n = rules_tribes();
    if (n != 25) {
        logf_("romania: romania.ini is here but rules.xml lists %d nations (25 needed), Romania left out", n);
        g_romania_enabled = 0;
        return;
    }
    int np = (int)(sizeof k_patches / sizeof k_patches[0]);
    for (int i = 0; i < np; i++)
        if (memcmp((void *)VA(k_patches[i].at), k_patches[i].orig, k_patches[i].len)) {
            logf_("romania: bytes differ at %s (%08X), Romania left out", k_patches[i].what, (unsigned)k_patches[i].at);
            g_romania_enabled = 0;
            return;
        }
    unsigned char *at = (unsigned char *)VA(VA_HAS_BONUS);
    static const unsigned char bonus_sig[3] = { 0x55, 0x8B, 0xEC }; /* push ebp / mov ebp,esp / mov eax,[gamec] */
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem || memcmp(at, bonus_sig, 3) || at[3] != 0xA1) {
        logf_("romania: has_tribe_bonus signature mismatch, Romania left out");
        g_romania_enabled = 0;
        return;
    }
    for (int i = 0; i < np; i++)
        write_code((void *)VA(k_patches[i].at), k_patches[i].repl, k_patches[i].len);
    if (!add_defensive(mem + 64))
        logf_("romania: Random Defensive switch differs, Romania is not one of its nations");
    g_orig_has_bonus = hook_trampoline(at, 8, mem);
    hook_jmp5(at, hk_has_bonus);
    unsigned char *fin = (unsigned char *)VA(VA_FINALIZE);
    static const unsigned char fin_sig[6] = { 0x33, 0xC0, 0x56, 0x57, 0x8B, 0x3D }; /* xor eax,eax / push esi / push edi / mov edi,[tribes] */
    if (!memcmp(fin, fin_sig, 6)) {
        g_orig_finalize = hook_trampoline(fin, 10, mem + 192);
        hook_jmp5(fin, finalize_grafting);
    } else
        logf_("romania: finalize_grafting signature mismatch, Romania keeps only the generic units");
    unsigned char *prep = (unsigned char *)VA(VA_PREP_UNIQUES);
    static const unsigned char prep_sig[7] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x8B, 0xD9 }; /* push ebp / mov ebp,esp / push ecx / push ebx / mov ebx,ecx */
    if (!memcmp(prep, prep_sig, 7)) {
        g_orig_prep_uniques = hook_trampoline(prep, 7, mem + 320);
        hook_jmp5(prep, prepare_unique_list);
    } else
        logf_("romania: prepare_unique_list signature mismatch, the loading screen may crash with Romania");
    units_install(mem + 256);
    g_num_tribes = 25;
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    for (int p = 0; p < PIECES; p++)
        logf_("romania: power piece %s (from tribe %d) %s", k_bonus[p].key, k_bonus[p].tribe, k_bonus[p].on ? "on" : "off");
    logf_("romania: 25 nations, %d patches in", np);
}
