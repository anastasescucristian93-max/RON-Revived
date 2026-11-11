/*
 * Survival mode support (the survival\ game script, scenario\Scripts\Survival):
 *  - the Survival button of the Solo menu (under Quick Battle): it opens the Quick Battle setup,
 *    and the game it starts runs the Survival script;
 *  - the Survival map: flat land without water, the human player in the middle (any map style
 *    chosen in the setup is replaced, mapstyles\SURVIVAL.xml gives its woods and rocks);
 *  - the wave bar at the right edge of the screen: a circle per wave down the game (45 minutes), the
 *    boss waves larger with a skull, the next one lit, a pip where the game is now;
 *  - the clocks at the top right: time left to survive and time to the next wave;
 *  - the no-build band along the four edges of the map, where the waves come in.
 *
 * The script talks to the DLL through the names of its timers. ScenarioFuncSet::set_timer
 * stores (name, game second at which it expires) with ScriptTimers::add_timer, which is
 * hooked here:
 *   surv_end                   the end of the game (45 minutes)
 *   surv_wave, surv_boss       the next wave / boss wave
 *   surv_plan <length> <first> [<every> <delay>] the bar's plan: the game's length and the first
 *                              wave, seconds; a boss <delay> s after every <every>-th wave
 *   surv_ring <band> <size>    the no-build band, tiles from each edge, and the map's size
 *   surv_zone x0 y0 x1 y1      the version 2 no-build zone (one side), still understood
 * The script sets them again every minute, so after loading a saved game (which restores
 * the timers without add_timer) the bar, the clocks and the band come back within a minute.
 *
 * A Survival game is one whose script is "survival" (Game+0x500, the script's file name, with
 * ScenarioData::load_scenario_script set). That is set by the setup when the script is picked
 * under Script, and by survival_setup_game when the game came from the Survival button. Every
 * machine of a multiplayer game has the same script, so all make the same map.
 *
 * The clocks and the bar are drawn after IFaceMainBase::draw_timers (the game's own clocks at
 * the top right, every frame). The band answers BuildTypeData::blocked_site with code 0x18
 * ("Can't build in enemy territory."): it is the placement check of the cursor, of the build
 * command on every machine and of the computer players, so all of them keep out and the game
 * stays in sync.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define VA_ADD_TIMER    0x00A049E0u /* ScriptTimers::add_timer(String &name, int end_second), ret 8 */
#define VA_DRAW_TIMERS  0x0080B060u /* IFaceMainBase::draw_timers(int), ret 4 */
#define VA_BLOCKED_SITE 0x00636A50u /* BuildTypeData::blocked_site(x, y, player, int, int *), ret 0x14 */
#define VA_CHECK_RESULT 0x0059B1E0u /* TopMenu::check_result(String const &button), ret 4 */
#define VA_LOAD_MAP     0x0069DAD0u /* Map::load_map_data(int, int), ret 8: reads [this+0x114].xml */
#define VA_MAKE_CALL    0x0068BDF6u /* Map::make: mov eax,[ebx] / mov ecx,ebx / push edi / call [eax+0x40] */
#define VA_GRASS_LAND   0x00695050u /* MapGrass::make_continents(int players), ret 4: all land, a ring of starts */
#define VA_BUILD_EMPIRE 0x005ABB80u /* Setup::build_empire(int player, int start), ret 8 */
#define VA_LOAD_SCRIPT  0x00CC21B0u /* int ScenarioData::load_scenario_script (0x80 = run Game+0x500) */
#define VA_WORLD_PTR    0x00C06188u /* World *: +0 / +4 size in tiles, +0x84 / +0x90 start x count / data,
                                       +0xAC start y data */
#define VA_ADD_START    0x006B2DE0u /* World::add_starting_location(WCoord const &x, WCoord const &y) */
#define VA_LEADERS      0x00E3A390u /* Leaders: 0x6EEC bytes a player, +0 flags (1 in game, 4 human) */
#define LEADER_SIZE     0x6EEC
#define VA_GAME_PTR     0x00C061ECu /* Game *: +0x560 game time in seconds (set_timer adds to it) */
#define VA_STR_STR      0x00A1F0A0u /* String::str(): the characters */
#define VA_STR_CTOR     0x00A1D660u /* String::String(char const *) */
#define VA_STR_ASSIGN   0x00A1EEB0u /* String::operator=(String const &) */
#define VA_STRING_CLOSE 0x00A1CF40u
#define VA_SET_ENABLED  0x00A54110u /* Window::set_enabled(int): flags +8, bit 8 = disabled */
#define VA_WIN_HIDE     0x00A54780u /* Window::hide() */
#define VA_COMBO_CHANGE 0x005BA570u /* SetupWin::make_combo_change(int combo id, int item id) */
#define SW_COMBOS       0x20BC      /* SetupWin: ComboBox * by combo id */
#define ID_OTHER_NATIONS 0x33       /* "Other Nations:" (SetupWin+0x2188), item id = the count */
#define ID_REVEAL        0x3C       /* "Reveal Map:" (Game+0x30), item 3 = All Visible */
#define ID_VICTORY      0x44        /* "Victory:" (SetupWin+0x21CC), rules.xml victories: 2 = Conquest */
#define VICTORY_CONQUEST 2
#define VA_INT_STRS     0x00C06378u /* int_str_array: +0x10 the strings, +0x1511C "timer_size" */
#define VA_FONTS        0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET     0x00522050u /* FontFamily::get(String const &, int, int, int), ret 0x10 */
#define VA_SCREEN       0x00ED8010u /* Buffer Window::screen */
#define VA_WRITE_RIGHT  0x0050F9C0u /* Buffer::write_right_l(String const &, x right, y, len), ret 0x10 */
#define VA_GRAPHSYS     0x00E335A0u /* GraphSys *: +0x360 screen width, +0x164 draw layer */
#define VA_LAYER_UI     0x00C8C33Cu /* the layer draw_timers draws in */
#define VA_RM_ADD_RECT  0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], 0xAABBGGRR, type) */
#define VA_IFACE_RM     0x00EBD1C0u /* IFaceRenderManager iface_render_manager */
#define VA_WHITE        0x00C8D26Cu
#define VA_BLACK        0x00C8D260u
#define VA_YELLOW       0x00C8D278u
#define VA_RED          0x00C8D248u

#define GAME_SCRIPT     0x500   /* String: the script's file name, without .bhs */
#define GAME_SCRIPT_DIR 0x528   /* String: its folder */
#define MAP_FILE        0x114   /* Map: String, the map style's file ("mapstyles\OLDWORLD") */
#define STR_SOLO        0x1DC54 /* "solo", the Quick Battle button */
#define STR_TIMER_FONT  0x1511C
#define BLOCK_ENEMY     0x18    /* "Can't build in enemy territory." */
#define TILE            192     /* world units per tile */
#define RENDER_SOLID    5       /* IFaceRenderType: untextured rectangle */
#define MAX_WAVES       96

#define ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))

static int g_enabled = 1;
static int g_hud_y;       /* 0 = under the game's own clock lines */
static int g_bar_on = 1;
static int g_bar_top;     /* the bar's top; 0 = under the clocks */
static int g_bar_len;     /* its length; 0 = down to BAR_BOTTOM % of the screen's height */
static int g_timer_y;     /* the large next-wave clock's top; 0 = TIMER_Y */
static int g_map_on = 1;

#define BAR_BELOW_CLOCKS 70 /* room under the clocks for the game's pictures of wonders being built */
#define BAR_BOTTOM       84 /* percent of the screen's height (the script hides the players' scores) */
#define BAR_RIGHT        30 /* the bar's middle, from the screen's right edge */
#define BAR_MIN          160

static int g_set_at = -1; /* game second of the last survival timer, to notice a new game */
static int g_end = -1, g_wave = -1, g_boss = -1;
static int g_plan_len, g_plan_first; /* surv_plan, 0 = no bar */
static int g_boss_every = 5, g_boss_delay = 30; /* a boss wave g_boss_delay s after every g_boss_every-th */
static int g_wave_spacing = 1;                  /* the script's WAVE_SPACING: the intervals times this */
static int g_zone[4], g_zone_on;     /* surv_zone, tiles */
static int g_ring, g_ring_size;      /* surv_ring: band width and map size, tiles; 0 = none */
static int g_zone_logged;

static int g_button;    /* the Survival button was the last one pressed in the menus */
static int g_map;       /* the map being made is a Survival map */
static int g_start_of[16], g_next_start, g_center_taken;
static int g_first_human = -1, g_next_near; /* c_surv_empire: co-op starts near the middle */

void *g_orig_add_timer, *g_orig_draw_timers, *g_orig_blocked, *g_orig_check_result, *g_orig_load_map,
    *g_orig_build_empire;

typedef const wchar_t *(__attribute__((thiscall)) *str_t)(void *s);
typedef void(__attribute__((thiscall)) *close_t)(void *s);
typedef void(__attribute__((thiscall)) *ctor_t)(void *s, const char *text);
typedef void *(__attribute__((thiscall)) *assign_t)(void *s, const void *from);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *fonts, void *name, int a, int b, int c);
typedef int(__attribute__((thiscall)) *write_right_t)(void *buf, void *s, int x, int y, int len);
typedef int(__attribute__((thiscall)) *font_h_t)(void *font);
typedef void(__attribute__((thiscall)) *continents_t)(void *map, int players);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);

/* Cooperative AI in the Survival setup: its own choice, kept in ron_camera.ini (the setup does
   not save to the profile, so the profile's box came back every time; the user, 2026-11-10).
   Off unless ticked in a Survival setup before. */
static char g_ini[MAX_PATH];
#define CHECK_COOP 4 /* the setup's Cooperative AI box, Game+0x20 bit 4 */

void survival_load_config(const char *ini_path)
{
    snprintf(g_ini, sizeof g_ini, "%s", ini_path);
    g_enabled = GetPrivateProfileIntA("gameplay", "survival_support", 1, ini_path);
    g_map_on = GetPrivateProfileIntA("gameplay", "survival_map", 1, ini_path);
    g_hud_y = GetPrivateProfileIntA("ui", "survival_clock_y", 0, ini_path);
    g_bar_on = GetPrivateProfileIntA("ui", "survival_bar", 1, ini_path);
    g_bar_top = GetPrivateProfileIntA("ui", "survival_bar_top", 0, ini_path);
    g_bar_len = GetPrivateProfileIntA("ui", "survival_bar_length", 0, ini_path);
    g_timer_y = GetPrivateProfileIntA("ui", "survival_timer_y", 0, ini_path);
}

static char *game(void) { return *(char **)VA(VA_GAME_PTR); }

static int game_second(void)
{
    char *g = game();
    return g ? *(int *)(g + 0x560) : 0;
}

/* a game String's text as ASCII (other characters as '?'), at most n - 1 characters */
static int string_ascii(void *s, char *out, int n)
{
    out[0] = 0;
    if (!s || !*(void **)s)
        return 0;
    const wchar_t *w = ((str_t)VA(VA_STR_STR))(s);
    int len = *(unsigned short *)((char *)s + 8), i;
    if (!w)
        return 0;
    for (i = 0; i < len && i < n - 1; i++)
        out[i] = w[i] < 128 ? (char)w[i] : '?';
    out[i] = 0;
    return i;
}

static void string_set(void *dst, const char *text)
{
    unsigned char tmp[0x14] __attribute__((aligned(4))) = { 0 };
    ((ctor_t)VA(VA_STR_CTOR))(tmp, text);
    ((assign_t)VA(VA_STR_ASSIGN))(dst, tmp);
    ((close_t)VA(VA_STRING_CLOSE))(tmp);
}

static void forget(void)
{
    cards_forget(); /* the reward cards (cards.c) */
    /* a new game: the last game's heroes go (their ids come back as other units: 2026-11-10, a
       new infantry hero was taken for the last game's rider too and drew its medallion) */
    ranks_forget_heroes();
    g_end = g_wave = g_boss = -1;
    g_plan_len = g_plan_first = 0;
    g_zone_on = 0;
    g_ring = g_ring_size = 0;
    g_set_at = -1;
}

/* a new game (or a load further back in time): the old timers mean nothing */
static void check_new_game(int now)
{
    if (g_set_at >= 0 && now < g_set_at)
        forget();
}

/* ---- the hero ---------------------------------------------------------------------------- */

/* The hero evolves with its player's age (the user, 2026-11-10): the script names the line's unit
   of the age now (survival.bhs, li[]); its soldiers become that unit, the player's nation's own
   version of it, as the game upgrades a unit when its line is researched (Leader::gain_tech:
   the unit's virtual set_type(type, 0), vtable +0x84). The rank stays (ranks.c keeps it in the
   unit's flags). In the simulation (a script timer), the same on every machine. */
#define VA_TYPE_INDEX   0x00A03480u /* ScenarioFuncSet::get_type_index(String const &, int): ecx, edx */
#define VA_GET_GRAFT    0x0047DF40u /* LeaderData::get_graft(int) const: the nation's own type */
#define VT_SET_TYPE     0x84        /* Unit::set_type(TypeIndex, SetTypeIndex) */

static int type_index(const char *name)
{
    unsigned char str[0x14] __attribute__((aligned(4))) = { 0 };
    ((ctor_t)VA(VA_STR_CTOR))(str, name);
    int r;
    void *f = (void *)VA(VA_TYPE_INDEX);
    void *ecx = str, *edx = 0;
    __asm__ volatile("call *%3" : "=a"(r), "+c"(ecx), "+d"(edx) : "r"(f) : "memory", "cc");
    ((close_t)VA(VA_STRING_CLOSE))(str);
    return r;
}

static void hero_evolve(int p, int slot, const char *name)
{
    char *soldiers[32];
    int n = ranks_hero_squad(p, slot, soldiers);
    if (n <= 0)
        return;
    int t = type_index(name);
    if (t < 0)
        return;
    char *leader = (char *)VA(VA_LEADERS + (UINT_PTR)p * LEADER_SIZE);
    int g = ((int(__attribute__((thiscall)) *)(void *, int))VA(VA_GET_GRAFT))(leader, t);
    if (g >= 0)
        t = g;
    int changed = 0;
    for (int i = 0; i < n; i++) {
        char *type = *(char **)(soldiers[i] + 0x18);
        if (!type || *(int *)(type + 4) == t)
            continue;
        ((void(__attribute__((thiscall)) *)(void *, int, int))(*(void ***)soldiers[i])[VT_SET_TYPE / 4])(soldiers[i], t,
                                                                                                         0);
        changed++;
    }
    if (changed)
        logf_("survival: hero %d (player %d) is now %s (type %d), %d soldiers", slot, p, name, t, changed);
}

/* The hero is created as one squad bigger than its type's (the user, 2026-11-10: six soldiers,
   two riders "sa se miste ca squadul de infanterie"): Objects::init_unit 0x0065E0C0 reads the
   squad size from the type (+0x308, UBER_SIZE; cavalry has 1) when it makes the unit, so
   "surv_uber P size name" sets it on the type and the player's own version of it (get_graft, as
   ScenarioFuncSet::add_unit picks) just before the script's create_unit, and "surv_uber P 0 name"
   puts the game's back right after. The game's types: [0x00E85DDC][index]. */
#define VA_TYPES_PTR 0x00E85DDCu
#define TYPE_UBER    0x308
static int g_uber_type[2] = { -1, -1 }, g_uber_old[2];

static void hero_uber(int p, int size, const char *name)
{
    char **types = *(char ***)VA(VA_TYPES_PTR);
    if (size <= 0) {
        for (int i = 0; i < 2; i++)
            if (g_uber_type[i] >= 0 && types && types[g_uber_type[i]])
                *(int *)(types[g_uber_type[i]] + TYPE_UBER) = g_uber_old[i];
        g_uber_type[0] = g_uber_type[1] = -1;
        return;
    }
    int t = type_index(name);
    if (t < 0 || !types || size > 30)
        return;
    char *leader = (char *)VA(VA_LEADERS + (UINT_PTR)p * LEADER_SIZE);
    int g = ((int(__attribute__((thiscall)) *)(void *, int))VA(VA_GET_GRAFT))(leader, t);
    int ts[2] = { t, g >= 0 && g != t ? g : -1 };
    for (int i = 0; i < 2; i++) {
        g_uber_type[i] = -1;
        if (ts[i] < 0 || !types[ts[i]])
            continue;
        g_uber_type[i] = ts[i];
        g_uber_old[i] = *(int *)(types[ts[i]] + TYPE_UBER);
        *(int *)(types[ts[i]] + TYPE_UBER) = size;
    }
    logf_("survival: %s (type %d, the nation's %d) made as a squad of %d", name, t, g, size);
}

/* ranks.c: the hero reached a new level */
void survival_hero_level(int p, int level) { logf_("survival: the hero (player %d) reached level %d", p, level); }

/* The game's length (the user, 2026-11-11: a dropdown "short, medium, long, very long, 30 min, 45,
   1h, 1,30h"): the setup's Time Limit combo (id 0x48, Game+0x3C, rules.xml "timelimits": 0 15 min,
   1 30, 2 45, 3 60, 4 90, 5.. 2 to 4 hours), shown in the Survival setup with items 1-4 only.
   The script asks "surv_len_30" / "surv_len_60" / "surv_len_90" (the chosen one expires at once;
   none: 45 minutes, also without the Survival button). */
#define ID_TIME_LIMIT 0x48
#define GAME_TIME_LIMIT 0x3C
static const int k_len_min[4] = { 30, 45, 60, 90 };

static int length_item(void)
{
    char *g = game();
    int i = g ? (unsigned char)g[GAME_TIME_LIMIT] : 2;
    return i >= 1 && i <= 4 ? i : 2;
}

static int length_minutes(void) { return k_len_min[length_item() - 1]; }

__attribute__((used)) int c_surv_add_timer(void *name, int end)
{
    char a[256];
    if (!name || string_ascii(name, a, sizeof a) < 5 || strncmp(a, "surv_", 5) != 0)
        return end;
    /* the hero choice: expired at once for the rider (a game from the Survival button only, so
       every machine of a multiplayer game, which has no such button, answers the same) */
    if (!strcmp(a, "surv_herochoice"))
        return g_button && survival_hero_choice() == 1 ? game_second() : end;
    /* the goal, the same way: "surv_goal_wonder" / "surv_goal_tech" expire at once when chosen;
       neither does in a game without the Survival button (the 45 minutes) */
    if (!strcmp(a, "surv_goal_wonder"))
        return g_button && survival_goal() == 0 ? game_second() : end;
    if (!strcmp(a, "surv_goal_tech"))
        return g_button && survival_goal() == 1 ? game_second() : end;
    if (!strncmp(a, "surv_len_", 9))
        return g_button && length_minutes() == atoi(a + 9) ? game_second() : end;
    int now = game_second();
    check_new_game(now);
    g_set_at = now;
    /* the reward cards (cards.c): picks, counts, boss cards, ruins */
    int cr = cards_timer(a, end, now);
    if (cr != -2)
        return cr;
    int p, q, r = 0;
    if (!strcmp(a, "surv_end"))
        g_end = end;
    else if (!strcmp(a, "surv_wave"))
        g_wave = end;
    else if (!strcmp(a, "surv_boss"))
        g_boss = end;
    else if (sscanf(a, "surv_plan %d %d", &p, &q) == 2 && p > 0 && q > 0) {
        int every, delay;
        g_plan_len = p;
        g_plan_first = q;
        int spacing = 1;
        if (sscanf(a, "surv_plan %*d %*d %*d %*d %d", &spacing) == 1 && spacing >= 1 && spacing <= 10)
            g_wave_spacing = spacing;
        if (sscanf(a, "surv_plan %*d %*d %d %d", &every, &delay) == 2 && every > 0 && delay > 0) {
            g_boss_every = every;
            g_boss_delay = delay;
        }
    } else if (sscanf(a, "surv_hero %d %d %d", &p, &q, &r) >= 2 && p >= 1 && p <= 8 && q >= 0) {
        /* "surv_hero P id [slot]": the script's player numbers start at 1, the game's at 0 */
        ranks_set_hero(p - 1, q, sscanf(a, "surv_hero %d %d %d", &p, &q, &r) == 3 ? r : 0);
    } else if (!strncmp(a, "surv_uber ", 10) && sscanf(a + 10, "%d %d", &p, &r) == 2 && p >= 1 && p <= 8) {
        /* "surv_uber P size name": the next create_unit's squad size; 0 puts the game's back */
        const char *name = strchr(a + 10, ' ');
        name = name ? strchr(name + 1, ' ') : NULL;
        hero_uber(p - 1, r, name ? name + 1 : "");
    } else if (!strncmp(a, "surv_heroage ", 13) && sscanf(a + 13, "%d %d", &p, &r) == 2 && p >= 1 && p <= 8) {
        /* "surv_heroage P slot name" */
        const char *name = strchr(a + 13, ' ');
        name = name ? strchr(name + 1, ' ') : NULL;
        if (name)
            hero_evolve(p - 1, r, name + 1);
    } else if (sscanf(a, "surv_ring %d %d", &p, &q) == 2 && p > 0 && q > 2 * p) {
        if (!g_ring || !g_zone_logged)
            logf_("survival: no-build band %d tiles along the edges of the %d-tile map", p, q);
        g_ring = p;
        g_ring_size = q;
        g_zone_logged = 1;
    } else if (sscanf(a, "surv_zone %d %d %d %d", &g_zone[0], &g_zone[1], &g_zone[2], &g_zone[3]) == 4) {
        if (!g_zone_on || !g_zone_logged)
            logf_("survival: no-build zone x %d..%d, y %d..%d (tiles)", g_zone[0], g_zone[2] - 1, g_zone[1],
                  g_zone[3] - 1);
        g_zone_on = 1;
        g_zone_logged = 1;
    }
    return end;
}

/* ---- the Survival button ------------------------------------------------------------- */

/* TopMenu::check_result(button): "survival" is answered as "solo" (the Quick Battle setup),
   and remembered for survival_setup_game; any other button forgets it. Returns the String to
   use instead, or NULL. */
#define VA_SUB_MENU     0x0059C030u /* TopMenu::sub_menu(String const &block): runs a topmenu.xml panel */
#define MENU_HANDLED    ((void *)1)
int g_menu_result; /* what check_result returns for a button handled here */

__attribute__((used)) void *c_surv_menu(void *self, void *name)
{
    char a[32];
    string_ascii(name, a, sizeof a);
    /* the main menu's Play button (ui-theme, 2026-11-10): the PLAY panel (Tutorial, Campaign, Solo,
       Multiplayer), as the game runs the SOLO panel for "solo_menu" (check_result: sub_menu("SOLO")) */
    if (!_stricmp(a, "play_menu") && self) {
        g_button = 0;
        unsigned char str[0x14] __attribute__((aligned(4))) = { 0 };
        ((ctor_t)VA(VA_STR_CTOR))(str, "PLAY");
        g_menu_result = ((int(__attribute__((thiscall)) *)(void *, void *))VA(VA_SUB_MENU))(self, str);
        ((close_t)VA(VA_STRING_CLOSE))(str);
        return MENU_HANDLED;
    }
    if (_stricmp(a, "survival") != 0) {
        g_button = 0;
        return NULL;
    }
    g_button = 1;
    logf_("survival: the Survival button, the Quick Battle setup opens for a Survival game");
    return *(char **)(*(char **)VA(VA_INT_STRS) + 0x10) + STR_SOLO;
}

/* nonukes.c's SetupWin::setup_game hook, after the game's: a game from the Survival button
   runs the Survival script, whatever is chosen under Script. Solo games only (the button is in
   the Solo menu); the script and its flag are part of the game's data from here on. */
void survival_setup_game(void)
{
    char *g = game();
    if (!g_enabled || !g_button || !g || (g[0x820] & 4))
        return;
    char dir[MAX_PATH + 32];
    snprintf(dir, sizeof dir, "%sscenario\\Scripts\\Survival", g_dir);
    string_set(g + GAME_SCRIPT_DIR, dir);
    string_set(g + GAME_SCRIPT, "survival");
    *(int *)VA(VA_LOAD_SCRIPT) = 0x80;
    /* the Cooperative AI box as it was left, for the next Survival setup */
    int coop = (*(unsigned *)(g + 0x20) >> CHECK_COOP) & 1;
    if (g_ini[0])
        WritePrivateProfileStringA("survival", "cooperative_ai", coop ? "1" : "0", g_ini);
    logf_("survival: the game runs %s\\survival.bhs", dir);
}

/* The Survival setup shows only your own row: the computer player that sends the waves (the
   "battle director", the user, 2026-11-10) is always exactly one, set by survival_setup_checks,
   and its rows and the Other Nations count are hidden. Combo ids (setup_combos): player 0..7,
   nation 8..15, colour 0x10..0x17, handicap 0x18..0x1F, difficulty 0x20..0x27, team 0x28..0x2F.
   Your Difficulty stays: it is the computer's, so it sets the waves. */
/* [survival] coop_test=1 (the user, 2026-11-10: "momentan sa pot adauga computer la co op pt
   test"): the computer rows and Other Nations stay in the Survival setup, and Other Nations
   counts your computer allies only: the battle director is added after them when Start is
   pressed (survival_before_game), so it is the last computer player, and survival.bhs makes
   every other one your ally. No teams are needed (the team column stays hidden; Teams & Allies
   stays Survival of the Fittest). Read when the setup opens. */
static int g_coop_test;

static int hidden_combo(int id)
{
    if (g_coop_test && (id == ID_OTHER_NATIONS || id < 0x28))
        return 0;
    if (id == ID_OTHER_NATIONS || (id >= 0x28 && id <= 0x2F))
        return 1;
    /* the other ways to win: Wonder Victory 0x45 (+0x21D0), Territory Goal 0x47 (+0x21D8), Income
       Goal 0x4A (+0x21E4); the whole map is yours in Survival, the territory goal would end it */
    if (id == 0x45 || id == 0x47 || id == 0x4A)
        return 1;
    return id >= 0 && id < 0x28 && (id & 7) != 0;
}

static void hide_rows(char *sw)
{
    for (int id = 0; id < 0x4E; id++) {
        char *c = *(char **)(sw + SW_COMBOS + 4 * id);
        if (c && hidden_combo(id) && (*(unsigned *)(c + 8) & 1))
            ((void(__attribute__((thiscall)) *)(void *))VA(VA_WIN_HIDE))(c);
    }
}

/* The Survival setup's rules, fixed and locked (the user, 2026-11-10): combo id, item id
   (rules.xml's categories, in order). Elimination Conquest: you lose with your last city. Victory
   Conquest: no wonder, territory or income victory (the battle director is never defeated, only
   the script's victory at the end counts); it reads "Survival". */
static struct {
    int id, item, game; /* the combo, its item, the Game byte setup_combos reads it from */
} k_rules[] = {
    { 0x30, 1, 0x24 }, /* Teams & Allies: Survival of the Fittest */
    { 0x32, 6, 0x26 }, /* Map Size: Big Huge, the largest (the user, 2026-11-10) */
    { 0x35, 2, 0x29 }, /* Game Speed: Normal */
    { 0x36, 1, 0x2A }, /* Game Rules: Custom (the rules below stand) */
    { 0x4D, 0, 0x41 }, /* Game Mods: none */
    { 0x38, 2, 0x2C }, /* Start With: Small Town */
    { 0x39, 1, 0x2D }, /* Resources: Standard */
    { 0x3B, 3, 0x2F }, /* Technology: Normal */
    { ID_REVEAL, 1, 0x30 }, /* Reveal Map: Normal, All Visible with [survival] reveal_map=1 */
    { 0x3D, 5, 0x31 }, /* Population: 200 */
    { 0x3E, 0, 0x32 }, /* Rush Rules: Standard */
    { 0x40, 0, 0x34 }, /* Start Age: Ancient */
    { 0x42, 6, 0x36 }, /* End Age: Information */
    { 0x43, 0, 0x37 }, /* Elimination: Conquest */
    { ID_OTHER_NATIONS, 1, 0x27 }, /* the battle director only (its row hidden) */
    { ID_VICTORY, VICTORY_CONQUEST, 0x38 },
};
#define N_RULES ((int)(sizeof k_rules / sizeof k_rules[0]))

/* Reveal Map (rules.xml "revealmaps": 0 No Ally LOS, 1 Normal, 2 Explored, 3 All Visible):
   Normal, or All Visible with [survival] reveal_map=1 in ron_camera.ini, to watch the waves
   march in (the user, 2026-11-10: "sa faci harta complet vizibila ca sa pot studia
   comportamentul lor"); read when the Survival setup opens. */
/* a rule the co-op test leaves to you */
static int rule_free(int i) { return g_coop_test && k_rules[i].id == ID_OTHER_NATIONS; }

static void reveal_rule_from_ini(void)
{
    int all = g_ini[0] ? GetPrivateProfileIntA("survival", "reveal_map", 0, g_ini) != 0 : 0;
    for (int i = 0; i < N_RULES; i++)
        if (k_rules[i].id == ID_REVEAL)
            k_rules[i].item = all ? 3 : 1;
}

/* One fixed rule on its combo, without the setup's change machinery (safe while drawing): the
   selection, the Game byte the screen fills the combo from again at every update, and the combo
   greyed, which the updates undo too. */
static void lock_rule(char *c, int i)
{
    if (*(int *)(c + 0x568) != k_rules[i].item)
        *(int *)(c + 0x568) = k_rules[i].item;
    char *g = game();
    if (g && (unsigned char)g[k_rules[i].game] != k_rules[i].item)
        g[k_rules[i].game] = (char)k_rules[i].item;
    if (!(*(unsigned *)(c + 8) & 8))
        ((void(__attribute__((thiscall)) *)(void *, int))VA(VA_SET_ENABLED))(c, 0);
}

#define VA_CHECK_CHANGE 0x005BBB50u /* SetupWin::make_check_change(int box id, int value) */
#define SW_CHECKS       0x21F4      /* SetupWin: CheckBox *[11] by box id */

int survival_setup_active(void) { return g_enabled && g_button; }

/* The check boxes a Survival game hides, unticked: Save to Profile 0 (the fixed rules must not
   become the Quick Battle's), Turbo Resources 1, No Nation Powers 2, No Unique Units 3, and the
   mod's Nukes slider (14, nonukes.c) and Units button (15, unitbans.c). Cooperative AI stays. */
int survival_hidden_check(int id) { return survival_setup_active() && (id <= 3 || id == 14 || id == 15); }

/* The hero you start with (the user, 2026-11-10): 0 the infantry squad, 1 the rider, chosen with
   the Survival setup's button (unitbans.c's Units button, which is there in its place), kept in
   ron_camera.ini. The script asks through a timer: it sets "surv_herochoice" far ahead and this
   DLL makes it expire at once for the rider (c_surv_add_timer), so timer_expired gives 1 then,
   0 for the infantry (ScriptTimers::check: -1 when the timer is not there at all). */
static int g_hero_choice = -1;

int survival_hero_choice(void)
{
    if (g_hero_choice < 0)
        g_hero_choice = g_ini[0] ? GetPrivateProfileIntA("survival", "hero", 0, g_ini) != 0 : 0;
    return g_hero_choice;
}

/* ---- the hero buttons of the Survival setup ------------------------------------------------- */

/* Two buttons in the solo players panel, under your own row (setupwin.xml, ui-theme's
   build_menu_theme.py: "hero_inf", "hero_cav", hidden): shown in the Survival setup, drawn here
   (the hero's medallion; the chosen one lit with a gold ring, the other dimmed), and a click
   chooses. The players panel is SetupWin+0x25D4 (in the window), its buttons found by name with
   its virtual +0x424; clicks reach SetupWin::on_button_clicked(String const &name) (vtable slot
   0x00B3ADF0), the drawing SetupWin::on_button_redraw (unitbans.c's hook asks here first). */
#define SW_PLAYERS        0x25D4
#define VT_FIND_BUTTON    0x424
#define VT_FIND_TEX       0xA4
#define VT_WIN_SHOW       0x18
#define VT_BUTTON_CLICKED 0x00B3ADF0u
#define VA_BUTTON_CLICKED 0x005C5B50u
#define WIN_RECT          0x126
static char *g_hero_btn[2];
static char *g_goal_btn[3]; /* the goal: 0 a Wonder, 1 the Information Age, 2 the 45 minutes */
static int g_goal = -1;

int survival_goal(void)
{
    if (g_goal < 0)
        g_goal = g_ini[0] ? GetPrivateProfileIntA("survival", "goal", 0, g_ini) : 0;
    return g_goal < 0 || g_goal > 2 ? 0 : g_goal;
}
static void *g_orig_clicked;

/* A child found by name: find_button(String const &, int) takes two arguments, find_tex(String
   const &, int, int *) three (thiscall, the callee pops them: the count must be right; a third
   argument to find_button crashed the setup, 2026-11-10). */
static void *find_named(char *win, int vt, const char *name)
{
    unsigned char str[0x14] __attribute__((aligned(4))) = { 0 };
    ((ctor_t)VA(VA_STR_CTOR))(str, name);
    void *r;
    if (vt == VT_FIND_TEX)
        r = ((void *(__attribute__((thiscall)) *)(void *, void *, int, void *))(*(void ***)win)[vt / 4])(win, str, -1,
                                                                                                       NULL);
    else
        r = ((void *(__attribute__((thiscall)) *)(void *, void *, int))(*(void ***)win)[vt / 4])(win, str, 0);
    ((close_t)VA(VA_STRING_CLOSE))(str);
    return r == (void *)-1 ? NULL : r;
}

void survival_hero_set(int cavalry)
{
    g_hero_choice = cavalry ? 1 : 0;
    if (g_ini[0])
        WritePrivateProfileStringA("survival", "hero", g_hero_choice ? "1" : "0", g_ini);
}

static void show_hero_buttons(char *sw)
{
    char *players = sw + SW_PLAYERS;
    for (int i = 0; i < 2; i++) {
        g_hero_btn[i] = find_named(players, VT_FIND_BUTTON, i ? "hero_cav" : "hero_inf");
        if (g_hero_btn[i])
            ((void(__attribute__((thiscall)) *)(void *, int))(*(void ***)g_hero_btn[i])[VT_WIN_SHOW / 4])(
                g_hero_btn[i], 0);
    }
    if (!g_hero_btn[0] || !g_hero_btn[1])
        logf_("survival: the hero buttons are not in setupwin.xml (ui-theme), the choice stays with ron_camera.ini");
    static const char *goals[3] = { "goal_wonder", "goal_tech", "goal_time" };
    for (int i = 0; i < 3; i++) {
        g_goal_btn[i] = find_named(players, VT_FIND_BUTTON, goals[i]);
        if (g_goal_btn[i])
            ((void(__attribute__((thiscall)) *)(void *, int))(*(void ***)g_goal_btn[i])[VT_WIN_SHOW / 4])(
                g_goal_btn[i], 0);
    }
}

/* unitbans.c's SetupWin::on_button_redraw hook: 1 when it drew one of the hero buttons */
/* a goal button: the game's alert picture, the chosen one lit with a glow, the others dimmed */
static int goal_button_redraw(char *sw, char *button, int i)
{
    static const char *icons[3] = { "goal_wonder_icon", "goal_tech_icon", "goal_time_icon" };
    void *icon = find_named(sw, VT_FIND_TEX, icons[i]);
    void *glow = find_named(sw, VT_FIND_TEX, "surv_glow");
    short *r = (short *)(button + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2], chosen = survival_goal() == i;
    add_rect_t add = (add_rect_t)VA(VA_RM_ADD_RECT);
    void *rm = (void *)VA(VA_IFACE_RM);
    if (chosen && glow) {
        short g[4] = { 0, (short)w, 0, (short)h };
        add(rm, glow, g, ABGR(255, 196, 90, 200), 1);
    }
    if (icon) {
        int sz = (w < h ? w : h) - 16, x = (w - sz) / 2, y = (h - sz) / 2;
        short q[4] = { (short)x, (short)(x + sz), (short)y, (short)(y + sz) };
        add(rm, icon, q, chosen ? ABGR(255, 255, 255, 255) : ABGR(110, 110, 110, 255), 1);
    }
    return 1;
}

int survival_button_redraw(char *sw, char *button)
{
    for (int g = 0; g < 3; g++)
        if (button && button == g_goal_btn[g] && survival_setup_active())
            return goal_button_redraw(sw, button, g);
    int i = button && button == g_hero_btn[0] ? 0 : button && button == g_hero_btn[1] ? 1 : -1;
    if (i < 0 || !survival_setup_active())
        return 0;
    void *icon = find_named(sw, VT_FIND_TEX, i ? "hero_cavalry" : "hero_infantry");
    void *ring = find_named(sw, VT_FIND_TEX, "surv_ring");
    void *glow = find_named(sw, VT_FIND_TEX, "surv_glow");
    short *r = (short *)(button + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2], chosen = survival_hero_choice() == i;
    int sz = (w < h ? w : h) - 8, x = (w - sz) / 2, y = (h - sz) / 2;
    add_rect_t add = (add_rect_t)VA(VA_RM_ADD_RECT);
    void *rm = (void *)VA(VA_IFACE_RM);
    if (chosen && glow) {
        short g[4] = { 0, (short)w, 0, (short)h };
        add(rm, glow, g, ABGR(255, 196, 90, 200), 1);
    }
    if (icon) {
        short q[4] = { (short)x, (short)(x + sz), (short)y, (short)(y + sz) };
        add(rm, icon, q, chosen ? ABGR(255, 255, 255, 255) : ABGR(120, 120, 120, 255), 1);
    }
    if (chosen && ring) {
        short q[4] = { (short)(x - 3), (short)(x + sz + 3), (short)(y - 3), (short)(y + sz + 3) };
        add(rm, ring, q, ABGR(255, 214, 120, 255), 1);
    }
    return 1;
}

/* SetupWin::on_button_clicked(String const &name): the hero buttons choose, the rest as before */
__attribute__((used, thiscall)) static void c_setup_clicked(char *sw, void *name)
{
    char a[32];
    string_ascii(name, a, sizeof a);
    if (survival_setup_active() && (!strcmp(a, "hero_inf") || !strcmp(a, "hero_cav"))) {
        survival_hero_set(!strcmp(a, "hero_cav"));
        return;
    }
    if (survival_setup_active() && !strncmp(a, "goal_", 5)) {
        g_goal = !strcmp(a, "goal_tech") ? 1 : !strcmp(a, "goal_time") ? 2 : 0;
        if (g_ini[0])
            WritePrivateProfileStringA("survival", "goal", g_goal == 1 ? "1" : g_goal == 2 ? "2" : "0", g_ini);
        return;
    }
    ((void(__attribute__((thiscall)) *)(void *, void *))g_orig_clicked)(sw, name);
}

static void hero_buttons_install(void)
{
    void **slot = (void **)VA(VT_BUTTON_CLICKED);
    if (*slot != (void *)VA(VA_BUTTON_CLICKED)) {
        logf_("survival: SetupWin::on_button_clicked slot not as expected, no hero buttons");
        return;
    }
    g_orig_clicked = *slot;
    DWORD old;
    VirtualProtect(slot, 4, PAGE_READWRITE, &old);
    *slot = (void *)c_setup_clicked;
    VirtualProtect(slot, 4, old, &old);
}

void survival_hero_toggle(void)
{
    g_hero_choice = !survival_hero_choice();
    if (g_ini[0])
        WritePrivateProfileStringA("survival", "hero", g_hero_choice ? "1" : "0", g_ini);
}

/* Sets the fixed rules and the boxes; from setup_checks and again just before setup_game reads
   them (the Reset button, or an update, may have changed them). */
/* the Time Limit combo's list: only 30, 45, 60 and 90 minutes (PullDown items, 64 bytes each: id
   +0x34, flags +0x38, bit 0 shown; nation_grid.c) */
#define COMBO_PULLDOWN 0x28C
#define PD_COUNT       0x278
#define PD_ITEMS       0x284

static void length_rows(char *combo)
{
    char *pd = combo + COMBO_PULLDOWN;
    int n = *(int *)(pd + PD_COUNT);
    char *items = *(char **)(pd + PD_ITEMS);
    for (int i = 0; items && i < n && i < 32; i++) {
        int id = *(int *)(items + 64 * i + 0x34);
        unsigned *f = (unsigned *)(items + 64 * i + 0x38);
        if (id < 1 || id > 4)
            *f &= ~1u;
    }
}

static void length_fix(char *sw)
{
    char *c = *(char **)(sw + SW_COMBOS + 4 * ID_TIME_LIMIT);
    if (!c)
        return;
    int sel = *(int *)(c + 0x568);
    if (sel < 1 || sel > 4)
        ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, ID_TIME_LIMIT, 2);
    length_rows(c);
}

/* SetupWin::update_interface shows the Time Limit combo only for the Time Limit victory
   (0x005B5F57: Victory +0x21CC item == 4, then the combo +0x21DC's show, vtable +0x18, else its
   hide, +0x1C); the hook shows it in the Survival setup too. */
#define VA_TL_TEST 0x005B5F57u
#define VA_TL_SHOW 0x005B5F6Eu
#define VA_TL_HIDE 0x005B5F75u
UINT_PTR g_tl_show, g_tl_hide;
void hk_surv_time_limit(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_surv_time_limit\n_hk_surv_time_limit:\n"
        "    mov eax, [esi+0x21CC]\n"
        "    mov ecx, [esi+0x21DC]\n"
        "    cmp dword ptr [eax+0x568], 4\n"
        "    je 1f\n"
        "    push ecx\n"
        "    push edx\n"
        "    call _survival_setup_active\n"
        "    pop edx\n"
        "    pop ecx\n"
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    mov eax, [ecx]\n"
        "    jmp dword ptr [_g_tl_hide]\n"
        "1:\n"
        "    mov eax, [ecx]\n"
        "    jmp dword ptr [_g_tl_show]\n"
        ".att_syntax prefix\n");

static void time_limit_install(void)
{
    static const unsigned char sig[23] = { 0x8B, 0x86, 0xCC, 0x21, 0x00, 0x00, 0x8B, 0x8E, 0xDC, 0x21, 0x00, 0x00,
                                           0x83, 0xB8, 0x68, 0x05, 0x00, 0x00, 0x04, 0x8B, 0x01, 0x75, 0x07 };
    unsigned char *at = (unsigned char *)VA(VA_TL_TEST);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("survival: signature mismatch in SetupWin::update_interface, no game length in the setup (45 minutes)");
        return;
    }
    g_tl_show = VA(VA_TL_SHOW);
    g_tl_hide = VA(VA_TL_HIDE);
    DWORD old;
    VirtualProtect(at, sizeof sig, PAGE_EXECUTE_READWRITE, &old);
    hook_jmp5(at, hk_surv_time_limit);
    memset(at + 5, 0x90, sizeof sig - 5);
    VirtualProtect(at, sizeof sig, old, &old);
}

void survival_fix_rules(char *sw)
{
    if (!survival_setup_active() || !sw)
        return;
    length_fix(sw);
    for (int i = 0; i < N_RULES; i++) {
        char *c = *(char **)(sw + SW_COMBOS + 4 * k_rules[i].id);
        if (!c || rule_free(i))
            continue;
        if (*(int *)(c + 0x568) != k_rules[i].item) /* as a click would: the screen follows */
            ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, k_rules[i].id,
                                                                                     k_rules[i].item);
        lock_rule(c, i);
    }
    for (int id = 0; id <= 3; id++)
        if (*(char **)(sw + SW_CHECKS + 4 * id))
            ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_CHECK_CHANGE))(sw, id, 0);
}

/* nonukes.c's setup_game hook, after survival_fix_rules, before the game reads the setup: in a
   co-op test the battle director is added after the allies; the director (the last computer row)
   plays in white (the user, 2026-11-10: "fa le cu culoarea gri", "sau alb": the game's colours
   are TeamColors::init's red 0, blue 1, pink 2, green 3, yellow 4, cyan 5, white 6, orange 7;
   grey is only in alternatecolors.xml, the colour-blind set). Colour combos: 0x10 + row. */
#define COLOR_WHITE 6
void survival_before_game(char *sw)
{
    if (!survival_setup_active() || !sw)
        return;
    char *other = *(char **)(sw + SW_COMBOS + 4 * ID_OTHER_NATIONS);
    int n = other ? *(int *)(other + 0x568) : 1;
    if (g_coop_test && other && n < 7) {
        ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, ID_OTHER_NATIONS, n + 1);
        n = *(int *)(other + 0x568);
    }
    /* the game's length, for the next Survival setup */
    char *tl = *(char **)(sw + SW_COMBOS + 4 * ID_TIME_LIMIT);
    if (tl && g_ini[0]) {
        char v[8];
        snprintf(v, sizeof v, "%d", *(int *)(tl + 0x568));
        WritePrivateProfileStringA("survival", "length", v, g_ini);
    }
    char *col = n >= 1 && n <= 7 ? *(char **)(sw + SW_COMBOS + 4 * (0x10 + n)) : NULL;
    if (col && *(int *)(col + 0x568) != COLOR_WHITE)
        ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, 0x10 + n, COLOR_WHITE);
    logf_("survival: %d other nation(s), the battle director (row %d) in colour %d", n, n,
          col ? *(int *)(col + 0x568) : -1);
}

/* The boxes above: hidden (nonukes.c's on_check_redraw hook and its setup_checks, for the
   slider and the Units button). */
void survival_hide_box(char *box)
{
    if (box && (*(unsigned *)(box + 8) & 1))
        ((void(__attribute__((thiscall)) *)(void *))VA(VA_WIN_HIDE))(box);
}

/* nonukes.c's SetupWin::setup_checks hook (SetupWin::exec, once the screen is built): in the
   setup the Survival button opened, one other nation, the rows hidden. */
void survival_setup_checks(char *sw)
{
    if (!g_enabled || !g_button || !sw)
        return;
    g_coop_test = g_ini[0] ? GetPrivateProfileIntA("survival", "coop_test", 0, g_ini) != 0 : 0;
    char *other = *(char **)(sw + SW_COMBOS + 4 * ID_OTHER_NATIONS);
    if (g_coop_test) { /* your allies (one to begin with), the director comes at Start */
        if (other && *(int *)(other + 0x568) != 1)
            ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, ID_OTHER_NATIONS, 1);
        if (other && (*(unsigned *)(other + 8) & 8))
            ((void(__attribute__((thiscall)) *)(void *, int))VA(VA_SET_ENABLED))(other, 1);
    } else if (other && *(int *)(other + 0x568) != 1)
        ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, ID_OTHER_NATIONS, 1);
    hide_rows(sw);
    reveal_rule_from_ini();
    /* the game's length as it was left (Time Limit combo; shown by hk_surv_time_limit) */
    char *tl = *(char **)(sw + SW_COMBOS + 4 * ID_TIME_LIMIT);
    if (tl) {
        int want = g_ini[0] ? GetPrivateProfileIntA("survival", "length", 2, g_ini) : 2;
        if (want < 1 || want > 4)
            want = 2;
        if (*(int *)(tl + 0x568) != want)
            ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_COMBO_CHANGE))(sw, ID_TIME_LIMIT, want);
        ((void(__attribute__((thiscall)) *)(void *, int))(*(void ***)tl)[VT_WIN_SHOW / 4])(tl, 0);
    }
    survival_fix_rules(sw);
    show_hero_buttons(sw);
    for (int id = 0; id <= 3; id++)
        survival_hide_box(*(char **)(sw + SW_CHECKS + 4 * id));
    if (*(char **)(sw + SW_CHECKS + 4 * CHECK_COOP)) {
        int coop = g_ini[0] ? GetPrivateProfileIntA("survival", "cooperative_ai", 0, g_ini) : 0;
        ((void(__attribute__((thiscall)) *)(void *, int, int))VA(VA_CHECK_CHANGE))(sw, CHECK_COOP, coop ? 1 : 0);
    }
    if (g_coop_test)
        logf_("survival: setup for a co-op test (computer rows and Other Nations free), other rules fixed");
    else
        logf_("survival: setup with the battle director only (one computer player, its rows hidden), rules fixed");
}

/* flags.c's hook on the setup combos' text (SetupWin::on_combo_redraw): in the setup the
   Survival button opened, the Map Style combo (id 0x31, SetupWin+0x2180) shows "Survival plain"
   and the Script combo (id 0x4C, SetupWin+0x21EC) "Survival", both disabled (Window flags +8
   & 8, as the game's own greyed Cannon Time): neither choice is used in that game. Fills
   str_out (a game String the caller closes) and returns 1, or returns 0. */
static const char *length_name(int item)
{
    switch (item) {
    case 1: return "Short: 30 minutes";
    case 3: return "Long: 1 hour";
    case 4: return "Very long: 1 hour 30";
    default: return "Medium: 45 minutes";
    }
}

/* flags.c: a row of an open list in the Survival setup (the Time Limit combo's names) */
int survival_list_text(char *combo, int item, void *str_out)
{
    if (!g_enabled || !g_button || !combo || *(int *)(combo + 0xC) != ID_TIME_LIMIT || item < 1 || item > 4)
        return 0;
    lang_string(str_out, lang_text(length_name(item)));
    return 1;
}

int survival_combo_text(char *combo, void *str_out)
{
    if (!g_enabled || !g_button || !combo)
        return 0;
    int id = *(int *)(combo + 0xC);
    for (int i = 0; i < N_RULES; i++) /* the screen's updates enable and refill them: again */
        if (k_rules[i].id == id && id != ID_VICTORY) {
            if (!rule_free(i))
                lock_rule(combo, i);
            return 0;
        }
    if (hidden_combo(id)) { /* shown again by the screen's updates: hidden again */
        char *sw = *(char **)VA(0x00CAB3A0u); /* SetupWin::current */
        if (sw)
            hide_rows(sw);
        return 0;
    }
    if (id == ID_TIME_LIMIT) {
        length_rows(combo);
        int sel = *(int *)(combo + 0x568);
        lang_string(str_out, lang_text(length_name(sel)));
        return 1;
    }
    const char *text = id == 0x31 && g_map_on ? "Survival plain" : id == 0x4C || id == ID_VICTORY ? "Survival" : NULL;
    if (!text)
        return 0;
    if (id == ID_VICTORY)
        lock_rule(combo, N_RULES - 1);
    else if (!(*(unsigned *)(combo + 8) & 8))
        ((void(__attribute__((thiscall)) *)(void *, int))VA(VA_SET_ENABLED))(combo, 0);
    lang_string(str_out, lang_text(text));
    return 1;
}

/* ---- the Survival map ---------------------------------------------------------------- */

static int survival_game(void)
{
    char *g = game();
    char a[16];
    if (!g || !*(int *)VA(VA_LOAD_SCRIPT) || (g[0x822] & 2)) /* no script, or a Conquer the World battle */
        return 0;
    string_ascii(g + GAME_SCRIPT, a, sizeof a);
    return !_stricmp(a, "survival");
}

/* Map::load_map_data, before it: a Survival game reads mapstyles\SURVIVAL.xml */
__attribute__((used)) void c_surv_load_map(char *map)
{
    g_map = g_enabled && g_map_on && survival_game();
    if (!g_map)
        return;
    char old[64];
    string_ascii(map + MAP_FILE, old, sizeof old);
    string_set(map + MAP_FILE, "mapstyles\\SURVIVAL");
    logf_("survival: Survival map, mapstyles\\SURVIVAL.xml in place of %s", old);
}

/* Map::make's call to make_continents, in a Survival game: all land (MapGrass, the game's own
   all-grass map), its ring of starts, and the first start moved to the middle for the human
   player (c_surv_empire). Any other game: the map style's own make_continents. */
__attribute__((used)) void c_surv_continents(void *map, int players)
{
    ((continents_t)VA(VA_GRASS_LAND))(map, players);
    char *w = *(char **)VA(VA_WORLD_PTR);
    int n = *(int *)(w + 0x84);
    /* every start in the middle while the map is made: the woods, rocks and resources the map
       style lays round each player's start all go round yours, not round the battle director's
       (the user, 2026-11-10: "doar intr-un colt"). Its own start comes after the map
       (c_surv_empire). The count stays: Map::make and place_resources read one start a player. */
    for (int i = 0; i < n; i++) {
        (*(int **)(w + 0x90))[i] = *(int *)(w + 0) / 2;
        (*(int **)(w + 0xAC))[i] = *(int *)(w + 4) / 2;
    }
    for (int i = 0; i < 16; i++)
        g_start_of[i] = -1;
    g_next_start = 1;
    g_center_taken = 0;
    g_first_human = -1;
    g_next_near = 0;
    logf_("survival: all-land map %dx%d, %d starts, the middle one for the human player", *(int *)(w + 0),
          *(int *)(w + 4), n);
}

/* A player's team: Game's player slots (Game+0x74, 0x8C bytes each: +0 flags & 1 in use, +3 the
   leader's index, +4 the team, 0..3; 4 and up none, 5 an observer; Game::init_teams 0x0058AE70).
   -1 when it has none. */
static int team_of(int player)
{
    char *g = game();
    if (!g)
        return -1;
    for (int s = 0; s < 8; s++) {
        unsigned char *slot = (unsigned char *)g + 0x74 + 0x8C * s;
        if ((*(unsigned short *)slot & 1) && slot[3] == player)
            return (signed char)slot[4] >= 0 && (signed char)slot[4] < 4 ? (signed char)slot[4] : -1;
    }
    return -1;
}

/* Setup::build_empire(player, start): in a Survival map the first human player gets the middle
   start; other humans and the computers on the first human's team (co-op) a start of their own
   round the middle; the other computers (the battle director) one at the top edge (each player
   keeps the one it got). */
__attribute__((used)) void c_surv_empire(int player, int *start)
{
    /* g_map is left from the last random map made: a scenario or a saved game after it makes
       none, so the game must still be a Survival game */
    if (!g_map || player < 0 || player >= 16 || !survival_game())
        return;
    char *w = *(char **)VA(VA_WORLD_PTR);
    int n = *(int *)(w + 0x84);
    if (g_start_of[player] < 0) {
        unsigned flags = *(unsigned *)VA(VA_LEADERS + (UINT_PTR)player * LEADER_SIZE);
        int team = team_of(player), friend = 0;
        /* without teams (Diplomacy, Survival of the Fittest, Assassin: Game+0x24 < 3) every
           computer but the last is an ally (survival.bhs makes them so), the last the director */
        char *gm = game();
        int no_teams = gm && (unsigned char)gm[0x24] < 3, last = -1;
        for (int i = 0; i < 8; i++) {
            unsigned f = *(unsigned *)VA(VA_LEADERS + (UINT_PTR)i * LEADER_SIZE);
            if ((f & 1) && !(f & 4))
                last = i;
        }
        /* the first human by index, not by the order the empires are built in (the computers
           can come first: 2026-11-10, an ally went to the edge) */
        if (g_first_human < 0)
            for (int i = 0; i < 8 && g_first_human < 0; i++) {
                unsigned f = *(unsigned *)VA(VA_LEADERS + (UINT_PTR)i * LEADER_SIZE);
                if ((f & 1) && (f & 4))
                    g_first_human = i;
            }
        if (player != g_first_human)
            friend = (flags & 4) || (no_teams ? player != last
                                              : team >= 0 && g_first_human >= 0 && team == team_of(g_first_human));
        if (player == g_first_human && !g_center_taken) {
            g_start_of[player] = 0;
            g_center_taken = 1;
        } else if (friend && n > 0) {
            /* round the middle, a tenth of the map from it, on the four sides in turn */
            int cx = *(int *)(w + 0) / 2, cy = *(int *)(w + 4) / 2, d = *(int *)(w + 0) / 10;
            int k = g_next_near++ % 4;
            int x = cx + (k == 0 ? d : k == 1 ? -d : 0), y = cy + (k == 2 ? d : k == 3 ? -d : 0);
            ((int(__attribute__((thiscall)) *)(void *, const int *, const int *))VA(VA_ADD_START))(w, &x, &y);
            g_start_of[player] = *(int *)(w + 0x84) - 1;
        } else if (n > 0) {
            /* a start of its own at the map's edge, in the no-build band (the script removes its
               city and units at once), added now that the map is made */
            int x = *(int *)(w + 0) / 2 + (g_next_start - 1) * 4, y = 4;
            ((int(__attribute__((thiscall)) *)(void *, const int *, const int *))VA(VA_ADD_START))(w, &x, &y);
            g_start_of[player] = *(int *)(w + 0x84) - 1;
            g_next_start++;
        } else
            g_start_of[player] = *start;
        logf_("survival: player %d (%s, team %d%s) starts at %d (the game chose %d)", player,
              (flags & 4) ? "human" : "computer", team, friend ? ", near the middle" : "", g_start_of[player], *start);
    }
    *start = g_start_of[player];
}

/* ---- the clocks ----------------------------------------------------------------------- */

/* a game Color (12 bytes: r, g, b, a, then the 16-bit forms and the format) of any r, g, b */
static void rgb_color(unsigned char *c, int r, int g, int b)
{
    memcpy(c, (void *)VA(VA_WHITE), 12);
    c[0] = (unsigned char)r;
    c[1] = (unsigned char)g;
    c[2] = (unsigned char)b;
}

static void write_line_rgb(const char *text, int x, int y, const unsigned char *color);

static void write_line(const char *text, int x, int y, unsigned color_va)
{
    write_line_rgb(text, x, y, (const unsigned char *)VA(color_va));
}

static void write_line_rgb(const char *text, int x, int y, const unsigned char *color)
{
    unsigned char str[0x14] __attribute__((aligned(4)));
    lang_string(str, text);
    memcpy((void *)VA(0x00ED80A0u), color, 12); /* the screen Buffer's text colour */
    memcpy((void *)VA(0x00ED80DCu), (void *)VA(VA_BLACK), 12); /* and its shadow */
    *(unsigned char *)VA(0x00ED8118u) = 1;
    *(unsigned char *)VA(0x00ED811Du) = 1;
    int len = *(unsigned short *)(str + 8);
    if (len)
        ((write_right_t)VA(VA_WRITE_RIGHT))((void *)VA(VA_SCREEN), str, x, y, len);
    ((close_t)VA(VA_STRING_CLOSE))(str);
}

static void mmss(char *out, size_t n, const char *label, int secs)
{
    if (secs < 0)
        secs = 0;
    snprintf(out, n, "%s  %d:%02d", lang_text(label), secs / 60, secs % 60);
}

/* ---- the wave bar --------------------------------------------------------------------- */

/* The waves' times, game seconds: the script's rule (survival.bhs: the next wave comes
   80 seconds less one per minute played after the last one, at least 45), from the first
   wave, but from the real next wave on once it is known, so the bar never drifts. */
static int wave_times(int start, int now, int *t, int max)
{
    int n = 0, s = g_plan_first;
    int real = g_wave > now ? g_wave - start : -1; /* the next wave the script has set */
    while (s < g_plan_len && n < max) {
        if (real > 0 && start + s > now) { /* the first one still ahead: the real one */
            s = real;
            real = -1;
            if (s >= g_plan_len)
                break;
        }
        t[n++] = start + s;
        int step = 80 - s / 60;
        s += (step < 45 ? 45 : step) * g_wave_spacing;
    }
    return n;
}

static void rect_at(short *r, int cx, int cy, int size)
{
    r[0] = (short)(cx - size / 2);
    r[1] = (short)(r[0] + size);
    r[2] = (short)(cy - size / 2);
    r[3] = (short)(r[2] + size);
}

static void put(const void *tex, int cx, int cy, int size, unsigned color)
{
    short r[4];
    rect_at(r, cx, cy, size);
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, r, color, tex ? 1 : RENDER_SOLID);
}

static void solid(int l, int r, int t, int b, unsigned color)
{
    short q[4] = { (short)l, (short)r, (short)t, (short)b };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, q, color, RENDER_SOLID);
}

/* one marker: a glow behind the next wave, the disc, its steel rim, the skull of a boss */
static void marker(int *depth, int base, int x, int cy, int size, unsigned disc, unsigned rim, int glow, int skull,
                   unsigned skull_color)
{
    const void *t_disc = qol_iface_tex("surv_disc"), *t_ring = qol_iface_tex("surv_ring");
    if (glow) {
        const void *t_glow = qol_iface_tex("surv_glow");
        unsigned phase = GetTickCount() % 1000, tri = phase < 500 ? phase : 1000 - phase;
        unsigned a = 110 + 120 * tri / 500; /* a slow pulse */
        *depth = base;
        if (t_glow)
            put(t_glow, x, cy, size * 5 / 2, (disc & 0x00FFFFFF) | (a << 24));
    }
    *depth = base + 1;
    put(t_disc, x, cy, size, disc);
    *depth = base + 2;
    if (t_ring)
        put(t_ring, x, cy, size, rim);
    if (skull) {
        const void *t_skull = qol_iface_tex("kills_skull");
        *depth = base + 3;
        if (t_skull)
            put(t_skull, x, cy, size * 7 / 10, skull_color);
    }
}

/* The bar stands at the right edge of the screen (the user, 2026-11-10: "mai bine o faci pe
   verticala in dreapta ca e mai mult loc"), from under the clocks and the wonders' pictures down to
   BAR_BOTTOM of the screen's height (the script hides the players' scores). It shows a window of
   the waves, not all 45 minutes ("arati mai mare doar cate o sectiune si wavesul din spate si cele
   care urmeaza"): BAR_BEHIND that have come and the next ones, BAR_SHOWN in all, evenly spaced,
   large, each with its number (bosses with a skull); the pointer moves from one to the next as
   the time passes; the window slides on at every wave. */
#define BAR_SHOWN  8
#define BAR_BEHIND 2

static void draw_bar(char *gs, int now, int h)
{
    int start = g_end - g_plan_len;
    int t[MAX_WAVES];
    int n = wave_times(start, now, t, MAX_WAVES);
    if (n <= 0)
        return;
    /* the waves and the boss waves in time order: a boss g_boss_delay seconds after every
       g_boss_every-th wave (survival.bhs), when that wave comes before the game's last minute */
    int et[MAX_WAVES * 2], en[MAX_WAVES * 2], ne = 0, done = 0;
    for (int i = 0; i < n; i++) {
        et[ne] = t[i];
        en[ne++] = i + 1;
        if (t[i] <= now)
            done++;
        if ((i + 1) % g_boss_every == 0 && t[i] - start < g_plan_len - 60) {
            int b = t[i] + g_boss_delay;
            if (i + 1 < n && b > t[i + 1])
                b = t[i + 1]; /* never after the next wave */
            et[ne] = b;
            en[ne++] = 0; /* a boss */
        }
    }
    int next = 0;
    while (next < ne && et[next] <= now)
        next++;
    int first = next - BAR_BEHIND, last;
    if (first < 0)
        first = 0;
    last = first + BAR_SHOWN - 1;
    if (last > ne - 1) {
        last = ne - 1;
        first = last - BAR_SHOWN + 1 < 0 ? 0 : last - BAR_SHOWN + 1;
    }

    int sw = *(int *)(gs + 0x360), sh = *(int *)(gs + 0x364);
    int y0 = g_bar_top > 0 ? g_bar_top : g_minimap_timer_top + 4 * h + BAR_BELOW_CLOCKS;
    int len = g_bar_len > 0 ? g_bar_len : sh * BAR_BOTTOM / 100 - y0;
    if (len < BAR_MIN)
        len = BAR_MIN;
    int step = len / (BAR_SHOWN - 1);
    int ms = step * 45 / 100; /* a wave's circle; the next +10, a boss +14 */
    if (ms > 30)
        ms = 30;
    if (ms < 12)
        ms = 12;
    int cx = sw - BAR_RIGHT - 8;
    int *depth = (int *)(gs + 0x164), base = *depth;
#define YOF(i) (y0 + ((i) - first) * step)

    /* where the game is now: between the last wave that came and the next one */
    int yn;
    if (next > last)
        yn = YOF(last);
    else {
        int t0 = next > 0 ? et[next - 1] : start, y_prev = next > first ? YOF(next - 1) : y0 - step / 2;
        int y_next = YOF(next), span = et[next] - t0;
        yn = span > 0 ? y_prev + (int)((long long)(now - t0) * (y_next - y_prev) / span) : y_next;
        if (yn < y0 - step / 2)
            yn = y0 - step / 2;
    }

    /* the track: a dark groove, amber down to now */
    int top = y0 - step / 2, bottom = YOF(last) + (last < ne - 1 ? step / 2 : 0);
    *depth = base;
    solid(cx - 7, cx + 7, top - 3, bottom + 3, ABGR(8, 8, 8, 190));
    solid(cx - 4, cx + 4, top, bottom, ABGR(52, 48, 40, 230));
    if (yn > top)
        solid(cx - 4, cx + 4, top, yn, ABGR(232, 170, 60, 255));

    char num[16];
    for (int i = first; i <= last; i++) {
        int y = YOF(i), past = et[i] <= now, is_next = i == next, boss = en[i] == 0;
        int size = boss ? ms + 14 : is_next ? ms + 10 : ms;
        if (boss)
            marker(depth, base + 1, cx, y, size + (is_next ? 6 : 0), past ? ABGR(80, 80, 80, 255) : ABGR(196, 34, 26, 255),
                   past ? ABGR(130, 130, 130, 255) : ABGR(220, 210, 200, 255), is_next && !past, 1,
                   past ? ABGR(150, 150, 150, 255) : ABGR(255, 255, 255, 255));
        else if (past)
            marker(depth, base + 1, cx, y, size, ABGR(118, 118, 118, 255), ABGR(140, 140, 140, 255), 0, 0, 0);
        else if (is_next)
            marker(depth, base + 1, cx, y, size, ABGR(232, 170, 60, 255), ABGR(240, 230, 210, 255), 1, 0, 0);
        else
            marker(depth, base + 1, cx, y, size, ABGR(46, 50, 54, 255), ABGR(200, 200, 200, 255), 0, 0, 0);
        /* its number (or "Boss") to the left, beyond the pointer's lane */
        *depth = base;
        if (boss)
            snprintf(num, sizeof num, "%s", lang_text("Boss"));
        else
            snprintf(num, sizeof num, "%d", en[i]);
        unsigned char grey[12] __attribute__((aligned(4)));
        rgb_color(grey, 150, 150, 150);
        if (past)
            write_line_rgb(num, cx - ms / 2 - 40, y - h / 2, grey);
        else
            write_line(num, cx - ms / 2 - 40, y - h / 2, boss ? VA_RED : is_next ? VA_YELLOW : VA_WHITE);
    }

    /* the pointer, left of the bar, pointing at it */
    *depth = base + 5;
    const void *t_pip = qol_iface_tex("surv_pip_r");
    if (t_pip)
        put(t_pip, cx - ms / 2 - 18, yn, 22, ABGR(255, 255, 255, 255));
    else
        solid(cx - 16, cx + 16, yn - 1, yn + 1, ABGR(255, 255, 255, 255));
    *depth = base;
#undef YOF

    /* "Wave 7 / 55" under the bar, right-aligned with the clocks */
    char line[64];
    snprintf(line, sizeof line, "%s %d / %d", lang_text("Wave"), done, n);
    write_line(line, sw - 10, bottom + 20, VA_WHITE);
}

/* The next wave's clock in large letters at the top centre, under the nation's title bar (the
   user, 2026-11-10): "Next wave in 1:23", yellow for a boss wave, red in the last 10 seconds. */
#define VA_FONT_SIZE    0x00522810u /* FontFamily::get(int size, int style) const: style 1 bold */
#define VA_WRITE_CENT   0x0050F850u /* Buffer::write_cent_l(String const &, short rect[4], len) */
#define TIMER_Y         46 /* 58 at first: the user wanted it nearer the title bar */
#define TIMER_SIZE      22          /* Georgia 22 bold, in ui-theme's fonts.xml */

static void draw_big_timer(char *gs, int now)
{
    int next = -1, boss = 0;
    if (g_wave > now && g_wave <= g_end)
        next = g_wave;
    if (g_boss > now && g_boss <= g_end && (next < 0 || g_boss < next)) {
        next = g_boss;
        boss = 1;
    }
    if (next < 0)
        return;
    void *font = ((void *(__attribute__((thiscall)) *)(void *, int, int))VA(VA_FONT_SIZE))((void *)VA(VA_FONTS),
                                                                                       TIMER_SIZE, 1);
    if (!font)
        return;
    unsigned saved[3];
    memcpy(saved, (void *)VA(0x00ED808Cu), 12); /* the screen Buffer's font */
    *(void **)VA(0x00ED808Cu) = font;
    *(int *)VA(0x00ED8090u) = 0;
    *(int *)VA(0x00ED8094u) = 0;
    int in = next - now;
    char line[96];
    mmss(line, sizeof line, boss ? "Boss wave in" : "Next wave in", in);
    unsigned char str[0x14] __attribute__((aligned(4)));
    lang_string(str, line);
    memcpy((void *)VA(0x00ED80A0u), (void *)VA(in <= 10 ? VA_RED : boss ? VA_YELLOW : VA_WHITE), 12);
    memcpy((void *)VA(0x00ED80DCu), (void *)VA(VA_BLACK), 12);
    *(unsigned char *)VA(0x00ED8118u) = 1;
    *(unsigned char *)VA(0x00ED811Du) = 1;
    int sw = *(int *)(gs + 0x360), y = g_timer_y > 0 ? g_timer_y : TIMER_Y;
    short r[4] = { 0, (short)sw, (short)y, (short)(y + TIMER_SIZE + 6) }; /* centred in it both ways */
    int len = *(unsigned short *)(str + 8);
    if (len)
        ((int(__attribute__((thiscall)) *)(void *, void *, short *, int))VA(VA_WRITE_CENT))((void *)VA(VA_SCREEN), str, r,
                                                                                          len);
    ((close_t)VA(VA_STRING_CLOSE))(str);
    memcpy((void *)VA(0x00ED808Cu), saved, 12);
}

__attribute__((used)) void c_surv_draw(void)
{
    if (g_end < 0)
        return;
    int now = game_second();
    check_new_game(now);
    if (g_end < 0 || now > g_end + 20)
        return;
    char *gs = *(char **)VA(VA_GRAPHSYS);
    if (!gs)
        return;

    /* the font and layer draw_timers uses for its own lines */
    char *strs = *(char **)(*(char **)VA(VA_INT_STRS) + 0x10);
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), strs + STR_TIMER_FONT, 0xC, -1, 1);
    if (font && *(void **)font) {
        *(void **)VA(0x00ED808Cu) = font;
        *(int *)VA(0x00ED8090u) = 0;
        *(int *)VA(0x00ED8094u) = 0;
    }
    void *cur = *(void **)VA(0x00ED808Cu);
    void *inner = cur ? *(void **)cur : 0;
    int h = inner ? ((font_h_t)(*(void ***)inner)[0x24 / 4])(inner) : 16;
    if (h <= 0 || h > 64)
        h = 16;

    int saved = *(int *)(gs + 0x164);
    *(int *)(gs + 0x164) = *(int *)VA(VA_LAYER_UI);
    if (g_bar_on && g_plan_len > 0 && now < g_end)
        draw_bar(gs, now, h);
    int x = *(int *)(gs + 0x360) - 10;
    int y = g_hud_y > 0 ? g_hud_y : g_minimap_timer_top + 2 * h + 6; /* under the game's lines */

    char line[96];
    int left = g_end - now;
    if (left > 0) {
        mmss(line, sizeof line, "Final assault", left);
        write_line(line, x, y, VA_WHITE);
        /* the next wave: the nearer of the two timers still ahead and before the end */
        int next = -1, boss = 0;
        if (g_wave > now && g_wave <= g_end)
            next = g_wave;
        if (g_boss > now && g_boss <= g_end && (next < 0 || g_boss < next)) {
            next = g_boss;
            boss = 1;
        }
        (void)next;
        (void)boss;
        draw_big_timer(gs, now); /* the next wave: large, at the top centre */
    } else {
        write_line(lang_text("Survived!"), x, y, VA_YELLOW);
    }
    *(int *)(gs + 0x164) = saved;
}

/* ---- the no-build band ---------------------------------------------------------------- */

__attribute__((used)) int c_surv_blocked(int x, int y, int player)
{
    (void)player;
    if (!g_zone_on && !g_ring)
        return 0;
    int now = game_second();
    check_new_game(now);
    int tx = x / TILE, ty = y / TILE;
    if (g_ring && (tx < g_ring || ty < g_ring || tx >= g_ring_size - g_ring || ty >= g_ring_size - g_ring))
        return BLOCK_ENEMY;
    if (g_zone_on && tx >= g_zone[0] && tx < g_zone[2] && ty >= g_zone[1] && ty < g_zone[3])
        return BLOCK_ENEMY;
    return 0;
}

void hk_surv_add_timer(void);
void hk_surv_draw_timers(void);
void hk_surv_blocked(void);
void hk_surv_check_result(void);
void hk_surv_load_map(void);
void hk_surv_continents(void);
void hk_surv_empire(void);

__asm__(".intel_syntax noprefix\n"
        /* add_timer(String &name, int end): note it, then the original */
        ".globl _hk_surv_add_timer\n_hk_surv_add_timer:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    call _c_surv_add_timer\n"
        "    add esp, 16\n"
        "    mov [ebx+40], eax\n" HOOK_LEAVE /* the end second, as c_surv_add_timer gives it back */
        "    jmp dword ptr [_g_orig_add_timer]\n"

        /* draw_timers(int): the original, then the clocks; its result is kept */
        ".globl _hk_surv_draw_timers\n_hk_surv_draw_timers:\n"
        "    push dword ptr [esp+4]\n"
        "    call dword ptr [_g_orig_draw_timers]\n"
        "    push eax\n" HOOK_ENTER
        "    call _c_surv_draw\n" HOOK_LEAVE
        "    pop eax\n"
        "    ret 4\n"

        /* blocked_site(x, y, player, int, int *): the band's code, or the original */
        ".globl _hk_surv_blocked\n_hk_surv_blocked:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+44]\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    call _c_surv_blocked\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp dword ptr [_g_orig_blocked]\n"
        "1:\n"
        "    ret 0x14\n"

        /* check_result(String const &button): "survival" becomes "solo", then the original */
        ".globl _hk_surv_check_result\n_hk_surv_check_result:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n"
        "    push dword ptr [ebx+24]\n"
        "    call _c_surv_menu\n"
        "    add esp, 16\n"
        "    cmp eax, 1\n"
        "    je 4f\n"
        "    test eax, eax\n"
        "    jz 2f\n"
        "    mov [ebx+36], eax\n"
        "2:\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_check_result]\n"
        "4:\n" HOOK_LEAVE /* the button was handled here: its result, as check_result returns it */
        "    mov eax, [_g_menu_result]\n"
        "    ret 4\n"

        /* load_map_data(int, int), this = the Map: the Survival file, then the original */
        ".globl _hk_surv_load_map\n_hk_surv_load_map:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push dword ptr [ebx+24]\n"
        "    call _c_surv_load_map\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_load_map]\n"

        /* Map::make's `call [eax+0x40]` (make_continents(int players)), ecx = the Map, the count
           on the stack: ours in a Survival map, else the map style's own (a tail jump, its
           ret 4 returns to Map::make) */
        ".globl _hk_surv_continents\n_hk_surv_continents:\n"
        "    cmp dword ptr [_g_map], 0\n"
        "    jne 3f\n"
        "    mov eax, [ecx]\n"
        "    jmp dword ptr [eax+0x40]\n"
        "3:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n"
        "    push dword ptr [ebx+24]\n"
        "    call _c_surv_continents\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    ret 4\n"

        /* build_empire(int player, int start): the start may change, then the original */
        ".globl _hk_surv_empire\n_hk_surv_empire:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    lea eax, [ebx+40]\n"
        "    push eax\n"
        "    push dword ptr [ebx+36]\n"
        "    call _c_surv_empire\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_build_empire]\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version. */
void survival_install(void)
{
    if (!g_enabled)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[9];
        int sig_len; /* bytes checked; the rest of the stolen ones hold an address */
        int steal;
        void *hook;
        void **orig;
    } hooks[] = {
        /* push ebp / mov ebp,esp / mov eax,fs:[0] */
        { "ScriptTimers::add_timer", VA_ADD_TIMER, { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0 }, 9, 9,
          hk_surv_add_timer, &g_orig_add_timer },
        /* push ebp / mov ebp,esp / push -1 */
        { "draw_timers", VA_DRAW_TIMERS, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5, 5, hk_surv_draw_timers,
          &g_orig_draw_timers },
        /* push ebp / mov ebp,esp / sub esp,0x1C */
        { "blocked_site", VA_BLOCKED_SITE, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C }, 6, 6, hk_surv_blocked,
          &g_orig_blocked },
        /* push ebp / mov ebp,esp / mov eax,fs:[0] */
        { "TopMenu::check_result", VA_CHECK_RESULT, { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0 }, 9, 9,
          hk_surv_check_result, &g_orig_check_result },
        /* push ebp / mov ebp,esp / push -1 */
        { "Map::load_map_data", VA_LOAD_MAP, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5, 5, hk_surv_load_map,
          &g_orig_load_map },
        /* push ebp / mov ebp,esp / mov eax,[Game *] */
        { "Setup::build_empire", VA_BUILD_EMPIRE, { 0x55, 0x8B, 0xEC, 0xA1 }, 4, 8, hk_surv_empire,
          &g_orig_build_empire },
    };
    /* Map::make: mov eax,[ebx] / mov ecx,ebx / push edi / call [eax+0x40] */
    static const unsigned char make_sig[8] = { 0x8B, 0x03, 0x8B, 0xCB, 0x57, 0xFF, 0x50, 0x40 };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if (memcmp((void *)VA(hooks[i].va), hooks[i].sig, hooks[i].sig_len) != 0) {
            logf_("survival: signature mismatch at %s, survival support turned off", hooks[i].name);
            g_enabled = 0;
            return;
        }
    int map_ok = memcmp((void *)VA(VA_MAKE_CALL), make_sig, sizeof make_sig) == 0;
    if (!map_ok)
        logf_("survival: signature mismatch at Map::make, no Survival map (the chosen map is played)");
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    for (int i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    hero_buttons_install();
    time_limit_install();
    cards_install();
    if (map_ok && g_map_on) {
        /* mov ecx,ebx / push edi / call hk_surv_continents: the same 8 bytes */
        unsigned char *at = (unsigned char *)VA(VA_MAKE_CALL);
        DWORD old;
        VirtualProtect(at, 8, PAGE_EXECUTE_READWRITE, &old);
        at[0] = 0x8B;
        at[1] = 0xCB;
        at[2] = 0x57;
        at[3] = 0xE8;
        *(INT32 *)(at + 4) = (INT32)((UINT_PTR)hk_surv_continents - (UINT_PTR)(at + 8));
        VirtualProtect(at, 8, old, &old);
    }
    logf_("survival: menu button, %smap, wave bar, clocks and no-build band installed", map_ok && g_map_on ? "" : "NO ");
}
