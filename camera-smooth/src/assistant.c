/*
 * AI assistant settings: what the computer may do when it plays your nation for you (the
 * game's "computer <playername>" console command), and how much of each resource it must
 * leave you.
 *
 * The AI plans purchases in Leader::production_ai, a step machine whose steps
 * (found_cities, research_techs, upgrade_units, create_units, create_buildings) each offer
 * candidates to MakeList::make_me(type, priority, ...); make_stuff then buys the best ones.
 * The hook on make_me drops a candidate when its kind is switched off or when buying it
 * would take a resource under the reserve. The kind comes from the step that offers it
 * (make_me's return address) and, for units and buildings, from the type:
 *   economy    citizens, scholars, caravans, merchants, fishermen; economic buildings
 *              (cities, farms, camps, mines, University, Library, Market, Temple, Senate,
 *              Docks, ...)
 *   military   every other unit; Barracks, Stable, Siege Factory, Airbase, towers, forts,
 *              Missile Silo, air defence
 *   research   technologies and unit upgrades
 *   expansion  new cities
 *   wonders    wonders and the Data Center
 *
 * It applies to the local player (the AI plans purchases for them only while it plays for
 * them, however the assistance was switched on), and only in single player games: computer players run on every machine of a multiplayer game, and settings
 * that only one machine knows would put the game out of sync.
 *
 * Settings: ron_camera.ini [assistant], read again when the file changes, so they can be
 * tuned while playing (the in-game panel will edit the same values).
 *
 * The same hook also keeps the Generals (types 0x36-0x39) and Supply Wagons (0x3F-0x41) of every
 * computer player in proportion to its army: the AI offers them again and again and ends up
 * with a crowd of them instead of soldiers. A player may have one General per
 * ai_army_per_general land army units and one Supply Wagon per ai_army_per_supply_wagon, at
 * most ai_max_generals / ai_max_supply_wagons ([gameplay]). Army = living land units of the
 * player that are not economic, Generals, Supply Wagons or Spies, decoys not counted, a squad
 * counted once (its captain). An offer is also dropped when an offer of the same group went
 * through less than ai_support_seconds ago, so several cannot be queued before the first one
 * exists. Game state only, so every machine of a multiplayer game decides the same, given the
 * same settings.
 *
 * While the assistant plays for you, the log gets a summary every game minute: how many
 * offers of each kind, how many were dropped (kind off, reserve, cap), and the military units
 * offered most.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_MAKE_ME      0x006C9BE0u /* MakeList::make_me(TypeIndex, 9 more), ret 0x28 */
#define VA_GAME_PTR     0x00C061ECu
#define VA_CONSOLE_PTR  0x00C06210u /* Console*: +0x298 local player */
#define VA_LEADERS      0x00E3A390u
#define VA_TYPE_TABLE   0x00E85DDCu /* TypeData** */
#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object **, +0x15C + 4 * p = unit count */

#define LEADER_SIZE   0x6EEC
#define LD_MAKELIST   0x6EC8 /* MakeList inside the Leader */
#define LD_PLAYER     0x008
#define LD_SECURE     0x6EB8
#define STOCK_KEY     0x8221 /* [secure + 4 * r] ^ STOCK_KEY = resources in the treasury */
#define GAME_FLAGS    0x820  /* byte, bit 2: multiplayer game */
#define VT_GET_COST   0x78   /* TypeData::get_cost(resource, player, -1, -1, 0, 1, -1) */
#define NUM_RES       6
#define GAME_TIME     0x550  /* frames, 15 a second */

#define SPY           0x3A
#define OBJ_FLAGS     0x08   /* bit 0: alive */
#define OBJ_ID        0x0A
#define OBJ_TYPE      0x18   /* TypeData *, type index at +4 */
#define U_FLAGS       0x68   /* bit 0: decoy */
#define U_CAPTAIN     0x8E   /* word: bit 15 = the captain, else the captain's id */
#define T_DOMAIN      0x218  /* 0 land, 1 water, 2 air */

/* support units kept in proportion to the army: unitrules.xml entry i is type 0x32 + i */
enum { CAP_GENERALS, CAP_WAGONS, CAP_COUNT };
static const struct {
    int first, last, per, max;
    const char *name, *per_key, *max_key;
} k_caps[CAP_COUNT] = {
    { 0x36, 0x39, 20, 3, "Generals", "ai_army_per_general", "ai_max_generals" }, /* the four General lines */
    { 0x3F, 0x41, 15, 4, "Supply Wagons", "ai_army_per_supply_wagon", "ai_max_supply_wagons" }, /* + Armed */
};
#define OBJS_UNITS_N  0x15C
#define FIRST_BUILD   2000

/* return addresses of the make_me calls in the production steps */
#define RET_BUILDINGS 0x006C3FBEu /* create_buildings */
#define RET_UNITS     0x006C63A4u /* create_units */
#define RET_UPGRADES  0x006C6B69u /* upgrade_units */
#define RET_TECHS     0x006C79BFu /* research_techs */
#define RET_CITIES    0x006C80AEu /* found_cities */

enum { K_ECONOMY, K_MILITARY, K_RESEARCH, K_EXPANSION, K_WONDERS, K_COUNT };
static const char *k_names[K_COUNT] = { "economy", "military", "research", "expansion", "wonders" };

int g_assist_enabled;
void *g_orig_make_me;
static int g_allow[K_COUNT] = { 1, 1, 1, 1, 0 };
static int g_reserve[NUM_RES]; /* food, timber, wealth, knowledge, metal, oil */
static char g_ini[MAX_PATH];
static FILETIME g_ini_time;
static DWORD g_last_check;
static int g_cap_per[CAP_COUNT], g_cap_max[CAP_COUNT], g_cap_frames = 60 * 15;
static int g_cap_offer[CAP_COUNT][8]; /* game time + 1 of the last offer let through */

/* summary for the log while the assistant plays (local player only) */
enum { D_OFFERED, D_KIND_OFF, D_RESERVE, D_CAPPED, D_N };
static int g_diag[K_COUNT + 1][D_N]; /* last row: offers from no known step */
static struct { int type, n; } g_diag_mil[24];
static int g_diag_start = -1;

typedef void(__attribute__((thiscall)) *make_me_t)(void *self, int type, int a1, int a2, int a3, int a4, int a5,
                                                   int a6, int a7, int a8, int a9);
typedef int(__attribute__((thiscall)) *get_cost_t)(void *type, int r, int player, int a, int b, int c, int d, int e);

static void read_settings(void)
{
    static const char *res[NUM_RES] = { "food", "timber", "wealth", "knowledge", "metal", "oil" };
    for (int k = 0; k < K_COUNT; k++)
        g_allow[k] = GetPrivateProfileIntA("assistant", k_names[k], k == K_WONDERS ? 0 : 1, g_ini);
    int all = GetPrivateProfileIntA("assistant", "reserve", 0, g_ini);
    for (int r = 0; r < NUM_RES; r++) {
        char key[32];
        snprintf(key, sizeof key, "reserve_%s", res[r]);
        g_reserve[r] = GetPrivateProfileIntA("assistant", key, all, g_ini);
    }
    logf_("assistant: economy %d military %d research %d expansion %d wonders %d; reserve %d %d %d %d %d %d",
          g_allow[0], g_allow[1], g_allow[2], g_allow[3], g_allow[4], g_reserve[0], g_reserve[1], g_reserve[2],
          g_reserve[3], g_reserve[4], g_reserve[5]);
}

/* Re-read the settings when ron_camera.ini changes (checked at most every 2 seconds). */
static void refresh_settings(void)
{
    DWORD now = GetTickCount();
    if (now - g_last_check < 2000)
        return;
    g_last_check = now;
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (GetFileAttributesExA(g_ini, GetFileExInfoStandard, &a) &&
        CompareFileTime(&a.ftLastWriteTime, &g_ini_time) != 0) {
        g_ini_time = a.ftLastWriteTime;
        read_settings();
    }
}

/* ---- for the in-game panel (qol.c) ------------------------------------------------------ */

static const char *k_labels[K_COUNT] = { "Economy", "Military", "Research", "New cities", "Wonders and Data Center" };

int assistant_kinds(void) { return K_COUNT; }
const char *assistant_kind_label(int k) { return k >= 0 && k < K_COUNT ? k_labels[k] : ""; }
int assistant_allowed(int k) { return k >= 0 && k < K_COUNT && g_allow[k]; }
int assistant_reserve(void) { return g_reserve[0]; }

/* Writes ron_camera.ini and notes its new time, so refresh_settings does not read it back. */
static void save_setting(const char *key, int v)
{
    char s[16];
    snprintf(s, sizeof s, "%d", v);
    WritePrivateProfileStringA("assistant", key, s, g_ini);
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (GetFileAttributesExA(g_ini, GetFileExInfoStandard, &a))
        g_ini_time = a.ftLastWriteTime;
}

void assistant_set_allowed(int k, int on)
{
    if (k < 0 || k >= K_COUNT)
        return;
    g_allow[k] = on ? 1 : 0;
    save_setting(k_names[k], g_allow[k]);
}

/* The panel sets one reserve for all six resources. */
void assistant_set_reserve(int v)
{
    static const char *res[NUM_RES] = { "food", "timber", "wealth", "knowledge", "metal", "oil" };
    if (v < 0)
        v = 0;
    save_setting("reserve", v);
    for (int r = 0; r < NUM_RES; r++) {
        char key[32];
        snprintf(key, sizeof key, "reserve_%s", res[r]);
        g_reserve[r] = v;
        save_setting(key, v);
    }
}

void assistant_load_config(const char *ini_path)
{
    snprintf(g_ini, sizeof g_ini, "%s", ini_path);
    g_assist_enabled = GetPrivateProfileIntA("assistant", "enabled", 1, ini_path);
    for (int c = 0; c < CAP_COUNT; c++) {
        g_cap_per[c] = GetPrivateProfileIntA("gameplay", k_caps[c].per_key, k_caps[c].per, ini_path);
        g_cap_max[c] = GetPrivateProfileIntA("gameplay", k_caps[c].max_key, k_caps[c].max, ini_path);
        if (g_cap_per[c] < 1)
            g_cap_per[c] = 1;
    }
    int secs = GetPrivateProfileIntA("gameplay", "ai_support_seconds", 60, ini_path);
    g_cap_frames = (secs < 0 ? 0 : secs) * 15;
}

static int is_economy_unit(int t)
{
    return (t >= 0x32 && t <= 0x35) || (t >= 0x3B && t <= 0x3E) || t == 0x13D || t == 0x13E || t == 0x13F ||
           t == 0x190;
}

static int building_kind(int t)
{
    if ((t >= 0x20E && t < 0x21F) || t == DC_EXP_POPULATION || t == DC_EXP_COMMERCE ||
        t == DC_EXP_EFFICIENCY) /* wonders and the city districts */
        return K_WONDERS;
    if ((t >= 0x1AB && t <= 0x1AF) || (t >= 0x1B7 && t <= 0x1BF) || (t >= 0x208 && t <= 0x20D) || t == TANK_TRAPS)
        return K_MILITARY;
    return K_ECONOMY;
}

static int kind_of(UINT_PTR ret, int type)
{
    ret -= g_delta;
    if (ret == RET_TECHS || ret == RET_UPGRADES)
        return K_RESEARCH;
    if (ret == RET_CITIES)
        return K_EXPANSION;
    if (ret == RET_UNITS)
        return is_economy_unit(type) ? K_ECONOMY : K_MILITARY;
    if (ret == RET_BUILDINGS)
        return building_kind(type);
    return -1;
}

static int assisted(char *leader)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    char *console = *(char **)VA(VA_CONSOLE_PTR);
    if (!game || !console || (game[GAME_FLAGS] & 4))
        return 0;
    /* the AI plans purchases for the local player only while it plays for them */
    return *(int *)(leader + LD_PLAYER) == *(int *)(console + 0x298);
}

/* Would buying `type` take any resource under its reserve? */
static int breaks_reserve(char *leader, int type)
{
    char *td = (*(char ***)VA(VA_TYPE_TABLE))[type];
    if (!td)
        return 0;
    int *stock = *(int **)(leader + LD_SECURE);
    int p = *(int *)(leader + LD_PLAYER);
    get_cost_t get_cost = (get_cost_t)(*(void ***)td)[VT_GET_COST / 4];
    for (int r = 0; r < NUM_RES; r++) {
        if (!g_reserve[r])
            continue;
        int cost = get_cost(td, r, p, -1, -1, 0, 1, -1);
        if (cost > 0 && (stock[r] ^ STOCK_KEY) - cost < g_reserve[r])
            return 1;
    }
    return 0;
}

static int game_time(void)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    return game ? *(int *)(game + GAME_TIME) : 0;
}

static int cap_of(int type)
{
    for (int c = 0; c < CAP_COUNT; c++)
        if (type >= k_caps[c].first && type <= k_caps[c].last && g_cap_max[c] >= 0)
            return c;
    return -1;
}

static int in_cap(int type, int c) { return type >= k_caps[c].first && type <= k_caps[c].last; }

/* Living units of player p: the land army (squads counted once) and each cap group, decoys not
   counted. */
static void count_units(int p, int *army, int have[CAP_COUNT])
{
    char *objs = *(char **)VA(VA_OBJECTS);
    char **arr = *(char ***)(objs + 0x14 + 0x1C * p);
    int count = *(int *)(objs + OBJS_UNITS_N + 4 * p);
    *army = 0;
    memset(have, 0, CAP_COUNT * sizeof *have);
    if (!arr)
        return;
    if (count > FIRST_BUILD)
        count = FIRST_BUILD;
    for (int id = 0; id < count; id++) {
        char *u = arr[id];
        char *td = u ? *(char **)(u + OBJ_TYPE) : NULL;
        if (!td || !(u[OBJ_FLAGS] & 1) || (*(int *)(u + U_FLAGS) & 1))
            continue;
        int t = *(int *)(td + 4), capped = 0;
        for (int c = 0; c < CAP_COUNT; c++)
            if (in_cap(t, c))
                have[c]++, capped = 1;
        if (capped || t == SPY || is_economy_unit(t) || *(int *)(td + T_DOMAIN) != 0)
            continue;
        /* a squad once: its captain, or a soldier whose captain is gone */
        unsigned short cap = *(unsigned short *)(u + U_CAPTAIN);
        short cid = (short)cap;
        if ((cap & 0x8000) || cid == *(short *)(u + OBJ_ID) || cid < 0 || cid >= count || !arr[cid] ||
            !(arr[cid][OBJ_FLAGS] & 1))
            ++*army;
    }
}

/* May this computer player get one more unit of cap group c? */
static int cap_allows(char *leader, int c)
{
    int p = *(int *)(leader + LD_PLAYER);
    if (p < 0 || p > 7)
        return 1;
    int t = game_time(), last = g_cap_offer[c][p] - 1;
    if (last > t) /* another game, or an earlier saved game */
        last = -1, g_cap_offer[c][p] = 0;
    int army, have[CAP_COUNT];
    count_units(p, &army, have);
    int allowed = army / g_cap_per[c];
    if (allowed > g_cap_max[c])
        allowed = g_cap_max[c];
    if (have[c] >= allowed || (last >= 0 && t - last < g_cap_frames)) {
        static int logged;
        if (logged < 30) {
            logged++;
            logf_("ai support: player %d, army %d, has %d %s (allowed %d), last one %d s ago: none more", p, army,
                  have[c], k_caps[c].name, allowed, last >= 0 ? (t - last) / 15 : -1);
        }
        return 0;
    }
    g_cap_offer[c][p] = t + 1;
    return 1;
}

static void diag_note(int kind, int what, int type)
{
    g_diag[kind >= 0 ? kind : K_COUNT][what]++;
    if (what != D_OFFERED || kind != K_MILITARY)
        return;
    for (int i = 0; i < 24; i++) {
        if (!g_diag_mil[i].n || g_diag_mil[i].type == type) {
            g_diag_mil[i].type = type;
            g_diag_mil[i].n++;
            return;
        }
    }
}

/* Once a game minute: what the assistant was offered and what was dropped. */
static void diag_flush(int p)
{
    int t = game_time();
    if (g_diag_start < 0 || t < g_diag_start || t - g_diag_start >= 60 * 15) {
        if (g_diag_start >= 0 && t >= g_diag_start) {
            char line[512];
            int n = snprintf(line, sizeof line, "assistant minute %d, player %d (offered/kind off/reserve/capped):",
                             t / 900, p);
            for (int k = 0; k <= K_COUNT && n < (int)sizeof line; k++)
                n += snprintf(line + n, sizeof line - n, " %s %d/%d/%d/%d", k < K_COUNT ? k_names[k] : "other",
                              g_diag[k][0], g_diag[k][1], g_diag[k][2], g_diag[k][3]);
            logf_("%s", line);
            n = snprintf(line, sizeof line, "assistant minute %d: military types offered:", t / 900);
            for (int i = 0; i < 24 && g_diag_mil[i].n && n < (int)sizeof line; i++)
                n += snprintf(line + n, sizeof line - n, " 0x%x x%d", g_diag_mil[i].type, g_diag_mil[i].n);
            logf_("%s", line);
        }
        g_diag_start = t;
        memset(g_diag, 0, sizeof g_diag);
        memset(g_diag_mil, 0, sizeof g_diag_mil);
    }
}

__attribute__((used)) void c_make_me(char *list, UINT_PTR ret, int *args)
{
    char *leader = list - LD_MAKELIST;
    if (nukefx_drops_offer(args[0]))
        return; /* Interceptors: nukefx.c buys them for computer players, at most 2 a silo */
    int mine = g_assist_enabled && assisted(leader);
    int kind = mine ? kind_of(ret, args[0]) : -1;
    if (mine) {
        diag_flush(*(int *)(leader + LD_PLAYER));
        diag_note(kind, D_OFFERED, args[0]);
    }
    int c = cap_of(args[0]);
    if (c >= 0 && !cap_allows(leader, c)) {
        if (mine)
            diag_note(kind, D_CAPPED, args[0]);
        return;
    }
    if (mine) {
        refresh_settings();
        static int logged;
        if (logged < 20) {
            logged++;
            logf_("assistant: player %d offered type 0x%x as %s", *(int *)(leader + LD_PLAYER), args[0],
                  kind >= 0 ? k_names[kind] : "?");
        }
        if (kind >= 0 && !g_allow[kind]) {
            diag_note(kind, D_KIND_OFF, args[0]);
            return;
        }
        if (breaks_reserve(leader, args[0])) {
            diag_note(kind, D_RESERVE, args[0]);
            return;
        }
    }
    ((make_me_t)g_orig_make_me)(list, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
                                args[8], args[9]);
}

void hk_make_me(void);

__asm__(".intel_syntax noprefix\n"
        /* MakeList::make_me: ecx = the list, [esp] = return address, 10 stack arguments */
        ".globl _hk_make_me\n"
        "_hk_make_me:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    lea eax, [ebx+36]\n"
        "    push eax\n"
        "    push dword ptr [ebx+32]\n"
        "    push ecx\n"
        "    call _c_make_me\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    ret 0x28\n"
        ".att_syntax prefix\n");

void assistant_install(void)
{
    if (!g_assist_enabled && g_cap_max[CAP_GENERALS] < 0 && g_cap_max[CAP_WAGONS] < 0 && !g_nuke_sim_enabled)
        return;
    /* push ebp / mov ebp,esp / push ebx / mov ebx,ecx */
    static const unsigned char sig[6] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0xD9 };
    unsigned char *at = (unsigned char *)VA(VA_MAKE_ME);
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem || memcmp(at, sig, sizeof sig) != 0) {
        logf_("assistant: signature mismatch, the assistant and the Generals cap keep the game's behaviour");
        g_assist_enabled = 0;
        g_cap_max[CAP_GENERALS] = g_cap_max[CAP_WAGONS] = -1;
        return;
    }
    read_settings();
    g_orig_make_me = hook_trampoline(at, 6, mem);
    hook_jmp5(at, hk_make_me);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("hook MakeList::make_me (assistant settings; computer players: a General per %d army units, at most "
          "%d; a Supply Wagon per %d, at most %d; one per %d s) installed",
          g_cap_per[CAP_GENERALS], g_cap_max[CAP_GENERALS], g_cap_per[CAP_WAGONS], g_cap_max[CAP_WAGONS],
          g_cap_frames / 15);
}
