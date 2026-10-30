/*
 * Quality of life: a row of icon buttons in the bottom interface and automatic trading.
 *
 *   Military buildings           select every Barracks, Stable / Auto Plant, Siege Factory /
 *                                Factory and Airbase
 *   Army (Hoplites icon)         select every military unit (the game's "All Military")
 *   Airbases                     select every Airbase (shown only once one is built)
 *   Docks                        select every Dock (Anchorage, Shipyard in later ages; shown
 *                                only once one is built)
 *   AI assistant (Senate icon)   open the assistant panel (assist_panel.c)
 *
 * The row sits just above the bottom interface, as children of the Console window.
 *
 * Auto-trade (always on unless auto_trade=0): merchants idle for `auto_trade_delay`
 * seconds go trade on their own.
 *
 * Selection uses the game's own hotkey functions (Options::do_cycle_*), which only touch
 * the local selection. Trade orders go through GroupOut::issue_trade, the same command a
 * right-click sends, so every player in a multiplayer game receives it.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* globals */
#define VA_CONSOLE_PTR   0x00C06210u /* Console*: +0x298 local player, +0x2a0 selection slot */
#define VA_OPTIONS_PTR   0x00C06204u /* Options* */
#define VA_IFACE_PTR     0x00E39964u /* IFaceMainBase* while a game is shown */
#define VA_SELECT_GROUPS 0x00E8D444u /* SelectGroup array, 0x9f0 bytes each; +0xc = count */
#define VA_LEADERS       0x00E3A390u /* LeaderData[8], 0x6eec each; +0 flags */
#define VA_OBJECTS_PTR   0x00C0618Cu /* Objects*: +0x200 city table index */
#define VA_CITIES        0x00C09970u /* per table: City** at +28*n */
#define VA_GROUP_VFTABLE 0x00B47C34u
#define VA_GROUP_VBTABLE 0x00B47C4Cu

/* functions */
#define VA_DO_CYCLE_BUILDTYPE 0x0071F440u /* Options::do_cycle_buildtype(type, 0, 0, all) */
#define VA_DO_CYCLE_MILITARY  0x0071FDE0u /* Options::do_cycle_military(mode, 0, 0) */
#define VA_FIND_IDLE_CARAVAN  0x009EAF70u /* ScenarioFuncSet::find_idle_caravan(const int &player1) */
#define VA_NUM_ROUTES         0x009F3240u /* ScenarioFuncSet::num_caravan_routes(const int &player1) */
#define VA_MAX_ROUTES         0x009F32B0u /* ScenarioFuncSet::max_caravan_routes(const int &player1) */
#define VA_FIND_CITY          0x0065BA90u /* ObjectsData::find_city(...) */
#define VA_GROUP_CLEAR        0x00713E80u /* Group::clear(int) */
#define VA_GROUP_ADD          0x00714350u /* Group::add(id, player, 0, 0) */
#define VA_ISSUE_TRADE        0x0070ACE0u /* GroupOut::issue_trade(id, player, id2, player2, queue) */
#define VA_BUTTON_CTOR        0x005249A0u /* Button::Button() */
#define VA_BUTTON_INIT        0x00524E60u /* Button::init(const String &, id, x, y, w, h, Window *, int) */
#define VA_STRING_CTOR        0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE       0x00A1CF40u /* String::close() */
#define VA_STRING_ASSIGN      0x00A1CF90u /* String::operator=(const char *) */
#define VA_WINDOW_MOVE        0x00A54080u /* Window::move(x, y) */
#define VA_WINDOW_SHOW        0x00A55B30u /* Window::show(int) */
#define VA_WINDOW_HIDE        0x00A54780u /* Window::hide() */

#define WIN_FLAGS   0x08  /* bit 0 = visible */
#define WIN_PARENT  0x74
#define WIN_RECT    0x126 /* short left, right, top, bottom */

typedef int(__attribute__((thiscall)) *fn_i_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_i_pi)(void *self, const int *a);
typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i4)(void *self, int a, int b, int c, int d);
typedef void(__attribute__((thiscall)) *fn_v_i3)(void *self, int a, int b, int c);
typedef void(__attribute__((thiscall)) *fn_v_i5)(void *self, int a, int b, int c, int d, int e);
typedef int(__attribute__((thiscall)) *fn_find_city)(void *self, int x, int y, int search, int player, int y2,
                                                     int a, int filter, int b, int c);
typedef int(__attribute__((thiscall)) *fn_button_init)(void *self, const void *text, int id, int x, int y, int w,
                                                       int h, void *parent, int flag);
typedef void(__attribute__((thiscall)) *fn_v_cstr)(void *self, const char *s);
typedef int(__attribute__((thiscall)) *fn_move)(void *self, int x, int y);
typedef void(__attribute__((thiscall)) *fn_v_p)(void *self, void *p);

#define CALL(type, va) ((type)VA(va))

/* ---- config ------------------------------------------------------------------------ */

int g_qol_enabled;
int g_show_hp;
int g_group_selection;
static int g_buttons_on;
static int g_auto_trade;
static float g_auto_trade_delay;
static int g_btn_x, g_btn_y, g_btn_w, g_btn_h, g_btn_on_top;

void qol_load_config(const char *ini_path)
{
    g_buttons_on = GetPrivateProfileIntA("qol", "buttons", 1, ini_path);
    g_auto_trade = GetPrivateProfileIntA("qol", "auto_trade", 1, ini_path);
    g_auto_trade_delay = ini_float(ini_path, "qol", "auto_trade_delay", 5.0f);
    g_btn_x = GetPrivateProfileIntA("qol", "buttons_x", -1, ini_path);
    g_btn_on_top = GetPrivateProfileIntA("qol", "buttons_on_top", 1, ini_path);
    g_btn_y = GetPrivateProfileIntA("qol", "buttons_y", -1, ini_path);
    g_btn_w = GetPrivateProfileIntA("qol", "button_width", 40, ini_path);
    g_btn_h = GetPrivateProfileIntA("qol", "button_height", 40, ini_path);
    g_show_hp = GetPrivateProfileIntA("qol", "show_hp", 1, ini_path);
    g_group_selection = GetPrivateProfileIntA("qol", "group_selection", 1, ini_path);
    panel_load_config(ini_path);
    g_qol_enabled = g_buttons_on || g_auto_trade;
}

/* ---- game state ------------------------------------------------------------------------ */

static char *console(void) { return *(char **)VA(VA_CONSOLE_PTR); }

/* Local player (0-based), or -1 when there is no running game we can act in. */
static int local_player(void)
{
    char *c = console();
    if (!c || !*(void **)VA(VA_IFACE_PTR))
        return -1;
    int p = *(int *)(c + 0x298);
    if (p < 0 || p > 7)
        return -1;
    unsigned flags = *(unsigned *)(VA(VA_LEADERS) + p * 0x6EEC);
    if ((flags & 3) != 3) /* alive, human */
        return -1;
    return p;
}

static int selection_count(void)
{
    char *c = console();
    char *groups = *(char **)VA(VA_SELECT_GROUPS);
    if (!c || !groups)
        return 0;
    return *(int *)(groups + *(int *)(c + 0x2A0) * 0x9F0 + 0xC);
}

/* ---- actions ------------------------------------------------------------------------ */

/* do_cycle_buildtype(type, 0, 0, 1) clears the selection, then adds every building of
   `type`. To select several types together, the clear (`push -1; ... call
   SelectGroup::clear`) is turned into NOPs while the second and later types are added. */
#define VA_CYCLE_CLEAR_PUSH 0x0071F562u /* 6A FF */
#define VA_CYCLE_CLEAR_CALL 0x0071F574u /* E8 F7 7C FF FF */

static void set_cycle_clear(int on)
{
    static const unsigned char push[2] = { 0x6A, 0xFF }, call[5] = { 0xE8, 0xF7, 0x7C, 0xFF, 0xFF };
    static const unsigned char nop2[2] = { 0x66, 0x90 }, nop5[5] = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    unsigned char *a = (unsigned char *)VA(VA_CYCLE_CLEAR_PUSH), *b = (unsigned char *)VA(VA_CYCLE_CLEAR_CALL);
    DWORD old;
    VirtualProtect(a, (b + 5) - a, PAGE_EXECUTE_READWRITE, &old);
    memcpy(a, on ? push : nop2, 2);
    memcpy(b, on ? call : nop5, 5);
    VirtualProtect(a, (b + 5) - a, old, &old);
    FlushInstructionCache(GetCurrentProcess(), a, (b + 5) - a);
}

static int cycle_clear_patchable(void)
{
    static const unsigned char push[2] = { 0x6A, 0xFF }, call[5] = { 0xE8, 0xF7, 0x7C, 0xFF, 0xFF };
    return !memcmp((void *)VA(VA_CYCLE_CLEAR_PUSH), push, 2) && !memcmp((void *)VA(VA_CYCLE_CLEAR_CALL), call, 5);
}

/* Selects every building of all the given types (-1 entries are skipped). */
static void select_building_types(const int *types, int n)
{
    void *opt = *(void **)VA(VA_OPTIONS_PTR);
    if (!opt || local_player() < 0)
        return;
    int first = 1, can_add = cycle_clear_patchable();
    for (int i = 0; i < n; i++) {
        if (types[i] < 0)
            continue;
        if (!first && !can_add)
            break;
        if (!first)
            set_cycle_clear(0);
        CALL(fn_v_i4, VA_DO_CYCLE_BUILDTYPE)(opt, types[i], 0, 0, 1);
        if (!first)
            set_cycle_clear(1);
        first = 0;
    }
}

/* Units[p][id] */
static char *unit_ptr(int p, int id)
{
    char **arr = *(char ***)(VA(0x00C0AEC0u) + 28 * p);
    return arr ? arr[id] : NULL;
}

/* The city a caravan would pick on its own (same search as Unit::think_caravan). */
static int find_trade_city(char *unit, int *city_id, int *city_player)
{
    int x = *(int *)(unit + 0x10) ^ 0x63637, y = *(int *)(unit + 0x14) ^ 0x63637;
    int player = *(unsigned char *)(unit + 9);
    char *objects = *(char **)VA(VA_OBJECTS_PTR);
    if (!objects)
        return 0;
    int idx = CALL(fn_find_city, VA_FIND_CITY)(objects, x, y, 4, player, y, 0, 0x12, 0, 0);
    if (idx < 0)
        return 0;
    int table = *(int *)(objects + 0x200);
    char **cities = *(char ***)(VA(VA_CITIES) + 28 * table);
    if (!cities)
        return 0;
    char *city = cities[idx];
    if (!city || !(city[4] & 1))
        return 0;
    *city_id = *(short *)(city + 8);
    *city_player = *(signed char *)(city + 0x5E);
    return 1;
}

static _Alignas(16) unsigned char g_group[0xA00];

static void send_trade(int p, int unit_id)
{
    char *unit = unit_ptr(p, unit_id);
    int city, city_player;
    if (!unit || !find_trade_city(unit, &city, &city_player))
        return;
    memset(g_group, 0, sizeof g_group);
    *(UINT_PTR *)g_group = VA(VA_GROUP_VFTABLE);
    *(UINT_PTR *)(g_group + 0x9CC) = VA(VA_GROUP_VBTABLE);
    CALL(fn_i_i, VA_GROUP_CLEAR)(g_group, -1);
    CALL(fn_v_i4, VA_GROUP_ADD)(g_group, unit_id, p, 0, 0);
    if (*(int *)(g_group + 0xC) <= 0)
        return;
    CALL(fn_v_i5, VA_ISSUE_TRADE)(g_group, city, city_player, -1, -1, 2 /* replace orders */);
    logf_("auto-trade: merchant %d -> city %d (player %d)", unit_id, city, city_player);
}

/* Idle merchants: unit id -> time first seen idle. */
#define MAX_TRACK 64
static struct {
    int id;
    double since;
    double last_order;
} g_idle[MAX_TRACK];
static int g_idle_n;
static int g_routes = -1, g_route_max = -1;

static void scan_merchants(double now, int send_all_now)
{
    int p = local_player();
    if (p < 0) {
        g_idle_n = 0;
        return;
    }
    int p1 = p + 1;
    g_routes = CALL(fn_i_pi, VA_NUM_ROUTES)(NULL, &p1);
    g_route_max = CALL(fn_i_pi, VA_MAX_ROUTES)(NULL, &p1);

    /* find_idle_caravan cycles through idle caravans and returns -1 after the last. */
    int seen[MAX_TRACK], n = 0;
    for (int i = 0; i < 200 && n < MAX_TRACK; i++) {
        int id = CALL(fn_i_pi, VA_FIND_IDLE_CARAVAN)(NULL, &p1);
        if (id < 0)
            break;
        int dup = 0;
        for (int k = 0; k < n; k++)
            dup |= seen[k] == id;
        if (dup)
            break;
        seen[n++] = id;
    }

    /* Keep the timers of merchants that are still idle, start new ones. */
    typeof(g_idle) next;
    int nn = 0;
    for (int k = 0; k < n; k++) {
        int found = -1;
        for (int j = 0; j < g_idle_n; j++)
            if (g_idle[j].id == seen[k])
                found = j;
        next[nn].id = seen[k];
        next[nn].since = found >= 0 ? g_idle[found].since : now;
        next[nn].last_order = found >= 0 ? g_idle[found].last_order : 0.0;
        nn++;
    }
    memcpy(g_idle, next, sizeof(next[0]) * nn);
    g_idle_n = nn;

    if (!g_auto_trade && !send_all_now)
        return;
    for (int k = 0; k < g_idle_n; k++) {
        int due = send_all_now || now - g_idle[k].since >= g_auto_trade_delay;
        if (due && now - g_idle[k].last_order >= 5.0) { /* don't spam a merchant with no route */
            g_idle[k].last_order = now;
            send_trade(p, g_idle[k].id);
        }
    }
}

/* ---- buttons ------------------------------------------------------------------------ */

/* Native IFaceButtons (the game's icon buttons, like the ones in the selection panel):
   kind 6 draws the icon of the type in +0x370, +0x3be is the number drawn on the icon. */
#define VA_IFACEBUTTON_CTOR 0x007FC190u /* IFaceButton::IFaceButton() */
#define VA_EMPTY_STRING     0x00EB437Cu
#define VA_TYPES            0x00E85DC8u /* Types */
#define VA_UNIT_KEY         0x006694A0u /* Types::unit_key(const String &, int quiet) */
#define VA_BUILD_KEY        0x00669330u /* Types::build_key(const String &, int quiet) */
#define IFB_KIND    0x32C
#define IFB_OBJ2    0x358
#define IFB_OBJ     0x35C
#define IFB_PLAYER  0x360
#define IFB_TYPE    0x370
#define IFB_FLAGS   0x3BC /* word, bit 0 set by the game on active buttons */
#define IFB_NUMBER  0x3BE /* word */
#define IFB_SHOWNUM 0x3C1 /* byte */

typedef int(__attribute__((thiscall)) *fn_key)(void *self, const void *str, int quiet);
typedef int(__attribute__((thiscall)) *fn_resize)(void *self, int w, int h, int a, int b);

/* The index is also the round button's texture, "qol_<index>_<state>" (ui-theme's sheet):
   new buttons go at the end; g_btn_order is the order on screen. */
enum { B_MILITARY, B_AIRBASE, B_DOCK, B_ASSIST, B_ARMY, B_COUNT };
static const struct {
    const char *icon; /* type whose icon the button shows */
    int is_unit;
} g_btn_def[B_COUNT] = {
    { "Barracks", 0 }, { "Airbase", 0 }, { "Dock", 0 }, { "Senate", 0 }, { "Hoplites", 1 },
};
static const int g_btn_order[B_COUNT] = { B_MILITARY, B_ARMY, B_AIRBASE, B_DOCK, B_ASSIST };
static _Alignas(16) unsigned char g_btn[B_COUNT][0x400];
static int g_btn_made;
static void *g_btn_parent; /* the bottom interface (IFaceMain) the row is placed against */
static char *g_btn_host;   /* the buttons' parent window: the Console, or the interface */
static int g_btn_pos_x = -10000, g_btn_pos_y = -10000;
static unsigned g_btn_shown = ~0u;

/* Building types: the button's own type and its later-age equivalent. */
static int g_type_barracks = -1, g_type_stable = -1, g_type_auto_plant = -1, g_type_siege = -1, g_type_factory = -1,
           g_type_airbase = -1, g_type_dock = -1, g_type_anchorage = -1, g_type_shipyard = -1;

/* Command panel row for a military building type (BuildTypeData*): 0 infantry, 1 cavalry
   and tanks, 2 artillery, 3 air; -1 for any other building. Used by panel.c. */
int qol_military_row(const void *bt)
{
    char **types = *(char ***)VA(0x00E85DDCu); /* Types + 0x14: TypeData* by type index */
    const int rows[][2] = {
        { g_type_barracks, 0 }, { g_type_stable, 1 }, { g_type_auto_plant, 1 },
        { g_type_siege, 2 },    { g_type_factory, 2 }, { g_type_airbase, 3 },
    };
    for (unsigned i = 0; types && i < sizeof rows / sizeof rows[0]; i++)
        if (rows[i][0] >= 0 && types[rows[i][0]] == bt)
            return rows[i][1];
    return -1;
}

static int type_key(const char *name, int is_unit)
{
    unsigned char str[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(str, name);
    int t = CALL(fn_key, is_unit ? VA_UNIT_KEY : VA_BUILD_KEY)((void *)VA(VA_TYPES), str, 1);
    CALL(fn_v, VA_STRING_CLOSE)(str);
    return t;
}

static void resolve_types(void)
{
    g_type_barracks = type_key("Barracks", 0);
    g_type_stable = type_key("Stable", 0);
    g_type_auto_plant = type_key("Auto Plant", 0);
    g_type_siege = type_key("Siege Factory", 0);
    g_type_factory = type_key("Factory", 0);
    g_type_airbase = type_key("Airbase", 0);
    g_type_dock = type_key("Dock", 0);
    g_type_anchorage = type_key("Anchorage", 0);
    g_type_shipyard = type_key("Shipyard", 0);
    logf_("types: barracks %d, stable %d, auto plant %d, siege %d, factory %d, airbase %d", g_type_barracks,
          g_type_stable, g_type_auto_plant, g_type_siege, g_type_factory, g_type_airbase);
}

/* Buildings of `type` the player owns (built, not under construction). */
#define LD_BUILT 0x5222 /* LeaderData: word per type */

static int built(int p, int type)
{
    if (p < 0 || type < 0)
        return 0;
    return *(unsigned short *)(VA(VA_LEADERS) + p * 0x6EEC + LD_BUILT + 2 * type);
}

static int button_wanted(int i, int p)
{
    switch (i) {
    case B_AIRBASE: return built(p, g_type_airbase) > 0;
    case B_DOCK:    return built(p, g_type_dock) + built(p, g_type_anchorage) + built(p, g_type_shipyard) > 0;
    default:        return 1;
    }
}

/* A Button tells its parent about a click through vtable +0x2D8. The Console's own entry
   (0x0041D760) passes it to the Console's parent, not to IFaceMain, so for our buttons
   (and the assistant panel), children of the Console, the entry is replaced. */
#define VA_CONSOLE_VTABLE   0x00B25354u
#define VT_BUTTON_CLICKED   0x2D8
#define VA_CONSOLE_CLICKED  0x0041D760u

static void *g_orig_console_clicked;

static void __attribute__((thiscall)) console_clicked(void *self, void *button)
{
    if (!qol_button_clicked(button))
        ((fn_v_p)g_orig_console_clicked)(self, button);
}

static int patch_console_clicks(char *con)
{
    if (g_orig_console_clicked)
        return 1;
    if (!con || *(UINT_PTR *)con != VA(VA_CONSOLE_VTABLE)) {
        logf_("console vtable %08lX, not the expected one: buttons stay in the interface",
              con ? (unsigned long)(*(UINT_PTR *)con - g_delta) : 0ul);
        return 0;
    }
    void **slot = (void **)(VA(VA_CONSOLE_VTABLE) + VT_BUTTON_CLICKED);
    if ((UINT_PTR)*slot != VA(VA_CONSOLE_CLICKED)) {
        logf_("console click entry %08lX, not the expected one: buttons stay in the interface",
              (unsigned long)((UINT_PTR)*slot - g_delta));
        return 0;
    }
    DWORD old;
    VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old);
    g_orig_console_clicked = *slot;
    *slot = (void *)console_clicked;
    VirtualProtect(slot, sizeof *slot, old, &old);
    logf_("console click entry patched");
    return 1;
}

/* Called right after IFaceMain::add_windows: the game interface was (re)built. */
void qol_create_buttons(void *iface)
{
    if (!g_buttons_on)
        return;
    resolve_types();
    char *con = console();
    g_btn_host = patch_console_clicks(con) ? con : iface;
    if (!g_btn_made) {
        for (int i = 0; i < B_COUNT; i++)
            /* IFaceButton has a virtual base: its constructor takes the hidden
               "most derived" flag on the stack (ret 4); 1 = also build the base. */
            CALL(fn_i_i, VA_IFACEBUTTON_CTOR)(g_btn[i], 1);
        g_btn_made = 1;
    }
    for (int i = 0; i < B_COUNT; i++) {
        unsigned char *b = g_btn[i];
        int err = CALL(fn_button_init, VA_BUTTON_INIT)(b, (void *)VA(VA_EMPTY_STRING), 0, 0, 0, 1, 1, g_btn_host, 0);
        if (err) {
            logf_("button %d: init error %d", i, err);
            continue;
        }
        int type = type_key(g_btn_def[i].icon, g_btn_def[i].is_unit);
        *(int *)(b + IFB_KIND) = 6;
        *(int *)(b + IFB_TYPE) = type;
        *(int *)(b + IFB_OBJ) = -1;
        *(int *)(b + IFB_OBJ2) = -1;
        *(int *)(b + IFB_PLAYER) = local_player() < 0 ? 0 : local_player();
        *(unsigned short *)(b + IFB_FLAGS) |= 1;
        b[IFB_SHOWNUM] = 0;
        void **vt = *(void ***)b;
        ((fn_resize)vt[8])(b, g_btn_w, g_btn_h, 0, 0); /* GraphicWin::resize */
        logf_("button %d: icon %s = type %d", i, g_btn_def[i].icon, type);
    }
    g_btn_parent = iface;
    g_btn_pos_x = g_btn_pos_y = -10000;
    g_btn_shown = ~0u;
    assist_panel_create(iface, con);
}

static int is_our_button(void *b)
{
    for (int i = 0; i < B_COUNT; i++)
        if (b == g_btn[i])
            return i;
    return -1;
}

/* Round buttons: IFACETEX "qol_<button>_<n|h|p>" (normal, hover, pressed) declared in the
   IFACEMAINBASE block of interface.xml on art\revived\qol_buttons.tga (both made by
   ui-theme\tools\build_qol_buttons.py). Called from IFaceButton::on_redraw (panel.c): returns
   1 when it drew one of ours, which then replaces the game's square button and type icon.
   Without the textures the buttons keep the game's look. */
#define VA_IFACE_RM     0x00EBD1C0u
#define VA_RM_ADD_RECT  0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_WHITE        0x00C8D26Cu
#define VT_FIND_TEX     0xA4        /* GraphicWinXML::find_tex(name, id, int *) */
#define BTN_INSIDE      0x2B0       /* Button: 1 while the mouse is over it (on_mouse_move / on_mouse_leave) */
#define BTN_STATE       0x318       /* Button: bit 0 while the left button holds it down */

typedef void *(__attribute__((thiscall)) *find_tex_t)(void *self, const void *name, int id, void *out);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);

static void *find_tex(char *win, const char *name)
{
    unsigned char str[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(str, name);
    void *r = ((find_tex_t)(*(void ***)win)[VT_FIND_TEX / 4])(win, str, -1, NULL);
    CALL(fn_v, VA_STRING_CLOSE)(str);
    return r;
}

/* An IFACETEX of the IFACEMAINBASE block (ranks.c: the rank insignia), or NULL. */
void *qol_iface_tex(const char *name)
{
    return g_btn_parent ? find_tex(g_btn_parent, name) : NULL;
}

int qol_button_redraw(void *button)
{
    int i = g_btn_made ? is_our_button(button) : -1;
    if (i < 0 || !g_btn_parent)
        return 0;
    char *b = button;
    char state = (*(unsigned *)(b + BTN_STATE) & 1) ? 'p' : *(int *)(b + BTN_INSIDE) == 1 ? 'h' : 'n';
    char name[16];
    snprintf(name, sizeof name, "qol_%d_%c", i, state);
    void *tex = find_tex(g_btn_parent, name);
    static int logged;
    if (!logged) {
        logged = 1;
        logf_("qol buttons: round button textures %s", tex ? "found" : "missing (interface.xml), game look kept");
    }
    if (!tex)
        return 0;
    short *r = (short *)(b + WIN_RECT);
    short rect[4] = { 0, (short)(r[1] - r[0]), 0, (short)(r[3] - r[2]) };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, *(unsigned *)VA(VA_WHITE), 1);
    return 1;
}

/* Hook on IFaceMainBase::on_button_clicked: returns 1 when the click was ours. */
int qol_button_clicked(void *button)
{
    if (assist_panel_is(button)) {
        assist_panel_click();
        return 1;
    }
    int i = is_our_button(button);
    if (i < 0 || !g_btn_made)
        return 0;
    switch (i) {
    case B_MILITARY: {
        /* Barracks, Stable and Siege Factory, their later-age forms, and the Airbase. */
        int types[] = { g_type_barracks, g_type_stable, g_type_auto_plant, g_type_siege, g_type_factory, g_type_airbase };
        select_building_types(types, 6);
        break;
    }
    case B_ARMY: { /* the game's "All Military" option (0x69) */
        void *opt = *(void **)VA(VA_OPTIONS_PTR);
        if (opt && local_player() >= 0)
            CALL(fn_v_i3, VA_DO_CYCLE_MILITARY)(opt, 1, 1, 1);
        break;
    }
    case B_AIRBASE:  select_building_types(&g_type_airbase, 1); break;
    case B_DOCK: {
        int types[] = { g_type_dock, g_type_anchorage, g_type_shipyard };
        select_building_types(types, 3);
        break;
    }
    case B_ASSIST:   assist_panel_toggle(console()); break;
    }
    return 1;
}

/* ---- tooltips ---------------------------------------------------------------------------- */

/* IFaceMainBase::on_button_move(Button *) shows the help of the option whose index is
   the button id; ours have none. MouseOver::set_help(String) shows a help.xml entry
   instead: MouseOver::describe passes the string to Scene::load_help_key as the entry's
   name, so plain text showed nothing (2026-10-25). Ours pass "QOL_HELP_<button>", and that
   one call of load_help_key in describe is redirected to help_key, which fills the
   tooltip with the button's text and leaves every other key to the game. */
#define VA_ON_BUTTON_MOVE 0x00811500u
#define VA_SET_HELP       0x007D07A0u /* MouseOver::set_help(const String &), ignores `this` */
#define VA_MOUSEOVER_PTR  0x00C06230u
#define VA_LOAD_HELP_KEY  0x008BD7F0u /* Scene::load_help_key(StringList *, const String &key, int), ret 0xC */
#define AT_LOAD_HELP_KEY  0x007D0BB6u /* its call in MouseOver::describe, for set_help */
#define VA_LIST_ADD       0x00A29420u /* StringList::add(const String &, const String &, int, int, int) */
#define VA_EMPTY_STRING   0x00EB437Cu
#define VA_STRING_AS_CHAR 0x0042D710u
#define HELP_PREFIX       "QOL_HELP_"

void *g_orig_button_move;
typedef void(__attribute__((thiscall)) *fn_set_help)(void *self, const void *str);
typedef void(__attribute__((thiscall)) *load_help_key_t)(void *self, void *list, const void *key, int n);
typedef int(__attribute__((thiscall)) *list_add_t)(void *self, const void *s, const void *s2, int a, int b, int c);
typedef const char *(__attribute__((thiscall)) *as_char_t)(void *self);

static void button_help(int i, char *text, int size)
{
    switch (i) {
    case B_MILITARY:
        snprintf(text, size, "%s", lang_text("Military buildings: select all your Barracks, Stables, Siege Factories and "
                                             "Airbases (Auto Plants and Factories in later ages), and train from all of them."));
        break;
    case B_ARMY:
        snprintf(text, size, "%s", lang_text("Army: select all your military units."));
        break;
    case B_AIRBASE:
        snprintf(text, size, "%s", lang_text("Airbases: select all your Airbases, and build aircraft from all of them."));
        break;
    case B_DOCK:
        snprintf(text, size, "%s", lang_text("Docks: select all your Docks (Anchorages and Shipyards in later ages), and build "
                                             "ships from all of them."));
        break;
    case B_ASSIST:
        snprintf(text, size, "%s", lang_text("AI assistant: let the computer play your nation (Full control) or take it back "
                                             "(No control) with one click, choose what it may buy (economy, military, "
                                             "research, new cities, wonders) and how much of each resource it must leave you."));
        break;
    default:
        text[0] = 0;
    }
}

__attribute__((used)) void c_button_move(void *self, void *button)
{
    if (assist_panel_is(button)) /* no help over the assistant panel */
        return;
    int i = g_btn_made ? is_our_button(button) : -1;
    if (i < 0) {
        ((fn_v_p)g_orig_button_move)(self, button);
        return;
    }
    char key[32];
    snprintf(key, sizeof key, HELP_PREFIX "%d", i);
    unsigned char str[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(str, key);
    CALL(fn_set_help, VA_SET_HELP)(*(void **)VA(VA_MOUSEOVER_PTR), str);
    CALL(fn_v, VA_STRING_CLOSE)(str);
}

__attribute__((thiscall)) static void help_key(void *self, void *list, void *key, int n)
{
    const char *k = CALL(as_char_t, VA_STRING_AS_CHAR)(key);
    if (!k || strncmp(k, HELP_PREFIX, sizeof HELP_PREFIX - 1)) {
        CALL(load_help_key_t, VA_LOAD_HELP_KEY)(self, list, key, n);
        return;
    }
    char text[1024];
    button_help(atoi(k + sizeof HELP_PREFIX - 1), text, sizeof text);
    unsigned char str[64] = { 0 };
    lang_string(str, text);
    CALL(list_add_t, VA_LIST_ADD)(list, str, (void *)VA(VA_EMPTY_STRING), 0, 0, 0);
    CALL(fn_v, VA_STRING_CLOSE)(str);
}

void hk_button_move(void);
__asm__(".intel_syntax noprefix\n"
        /* IFaceMainBase::on_button_move(Button *), ret 4 */
        ".globl _hk_button_move\n"
        "_hk_button_move:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_button_move\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    ret 4\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version. */
void qol_install(void)
{
    panel_install();
    if (!g_buttons_on)
        return;
    /* on_button_move: push ebp / mov ebp,esp / mov eax,[ebp+8] (stolen) | mov ecx,[eax+0xc] */
    static const unsigned char sig[9] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x8B, 0x48, 0x0C };
    unsigned char *at = (unsigned char *)VA(VA_ON_BUTTON_MOVE);
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem || memcmp(at, sig, sizeof sig) != 0) {
        logf_("signature mismatch at on_button_move, button tooltips turned off");
        return;
    }
    unsigned char *call = (unsigned char *)VA(AT_LOAD_HELP_KEY);
    if (call[0] != 0xE8 || (UINT_PTR)(call + 5 + *(INT32 *)(call + 1)) != VA(VA_LOAD_HELP_KEY)) {
        logf_("signature mismatch at MouseOver::describe, button tooltips turned off");
        return;
    }
    DWORD old;
    VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(call + 1) = (INT32)((UINT_PTR)help_key - (UINT_PTR)(call + 5));
    VirtualProtect(call, 5, old, &old);
    g_orig_button_move = hook_trampoline(at, 6, mem);
    hook_jmp5(at, hk_button_move);
    logf_("hook on_button_move installed");
}

/* Window children in draw order: Array<Window*> at Window+0x80 (+0x84 count, +0x90 data),
   index 0 drawn on top. Window::add_child inserts new children at index 0; the game
   re-parents its own interface buttons now and then, which puts them above ours. */
#define WIN_ZCHILDREN_COUNT 0x84
#define WIN_ZCHILDREN_DATA  0x90
#define VA_WINDOW_BRING_TO_TOP 0x00A53F90u /* Window::bring_to_top() */

static void bring_buttons_to_front(void)
{
    char *host = g_btn_host;
    int count = *(int *)(host + WIN_ZCHILDREN_COUNT);
    char **kids = *(char ***)(host + WIN_ZCHILDREN_DATA);
    if (!kids || count < B_COUNT)
        return;
    /* ours first; the assistant panel may sit among them */
    int ours = 0;
    for (int k = 0; k < count && ours < B_COUNT; k++) {
        if (is_our_button(kids[k]) >= 0)
            ours++;
        else if (!assist_panel_is(kids[k]))
            break;
    }
    if (ours == B_COUNT)
        return;
    for (int i = B_COUNT - 1; i >= 0; i--)
        CALL(fn_v, VA_WINDOW_BRING_TO_TOP)(g_btn[i]);
}

/* Just above the bottom interface, on the right, ending where the game's own buttons on
   the top of the interface begin. Buttons the player has no use for yet are hidden and
   the others close up. */
static void place_buttons(int p)
{
    char *iface = g_btn_parent;
    short *ir = (short *)(iface + WIN_RECT), *hr = (short *)(g_btn_host + WIN_RECT);
    int w = ir[1] - ir[0], h = ir[3] - ir[2];
    if (w <= 0 || h <= 0)
        return;
    unsigned shown = 0;
    int n = 0;
    for (int i = 0; i < B_COUNT; i++)
        if (button_wanted(i, p))
            shown |= 1u << i, n++;
    int total = n * (g_btn_w + 2) - 2;
    /* y relative to the top of the interface; -1 = automatic, just above it (or at the
       top of the interface when the buttons could not be made Console children) */
    int y = g_btn_y != -1 ? g_btn_y : g_btn_host != iface ? -g_btn_h - 6 : 4;
    int strip_y = 4; /* the game's buttons on the top of the interface */
    int x = g_btn_x;
    if (x < 0) {
        /* The game's in-game buttons (diplomacy, menu, ...) are children of the Console
           window (IFaceButtonPool parents them there), not of IFaceMain. Rects are in
           screen coordinates; our strip is the top rows of the bottom interface. */
        static int logged;
        int left_edge = w; /* leftmost game element in the right half of our strip */
        char *con = console();
        int count = con ? *(int *)(con + WIN_ZCHILDREN_COUNT) : 0;
        char **kids = con ? *(char ***)(con + WIN_ZCHILDREN_DATA) : NULL;
        int strip_t = ir[2] + strip_y, strip_b = ir[2] + strip_y + g_btn_h;
        for (int k = 0; kids && k < count; k++) {
            char *c = kids[k];
            if (!c || is_our_button(c) >= 0 || c == iface)
                continue;
            short *r = (short *)(c + WIN_RECT);
            int l = r[0], rr = r[1], t = r[2], b = r[3];
            int visible = *(unsigned *)(c + WIN_FLAGS) & 1;
            int in_strip = b > strip_t && t < strip_b && t >= ir[2] - 4;
            if (!logged && in_strip)
                logf_("bar element %p (vtable %08lX): x %d-%d, y %d-%d (bar top %d) visible %d", c,
                      (unsigned long)(*(UINT_PTR *)c - g_delta), l, rr, t, b, ir[2], visible);
            if (!visible || rr <= l || b <= t || rr - l >= w - 50 || !in_strip)
                continue;
            int lrel = l - ir[0];
            if (lrel > w / 2 && lrel < left_edge)
                left_edge = lrel;
        }
        logged = 1;
        x = left_edge - total - 12;
        if (left_edge == w) /* nothing found: stay clear of the right-hand cluster anyway */
            x = w - total - 320;
    }
    if (x == g_btn_pos_x && y == g_btn_pos_y && shown == g_btn_shown)
        return;
    /* Window::move takes coordinates inside the parent; ours are relative to the interface */
    int ox = ir[0] - hr[0], oy = ir[2] - hr[2];
    for (int j = 0, k = 0; j < B_COUNT; j++) {
        int i = g_btn_order[j];
        if (!(shown & 1u << i)) {
            CALL(fn_v, VA_WINDOW_HIDE)(g_btn[i]);
            continue;
        }
        CALL(fn_move, VA_WINDOW_MOVE)(g_btn[i], ox + x + k++ * (g_btn_w + 2), oy + y);
        CALL(fn_i_i, VA_WINDOW_SHOW)(g_btn[i], 1);
    }
    g_btn_pos_x = x;
    g_btn_pos_y = y;
    g_btn_shown = shown;
    logf_("buttons at %d,%d from the interface's corner (%dx%d), shown mask %X", x, y, w, h, shown);
}

/* Once per rendered frame. */
void qol_tick(double now)
{
    static double next_scan;
    if (now >= next_scan) {
        next_scan = now + 0.5;
        scan_merchants(now, 0);
    }
    if (!g_buttons_on || !g_btn_made || !g_btn_parent || g_btn_parent != *(void **)VA(VA_IFACE_PTR))
        return;
    int p = local_player();
    place_buttons(p);
    /* The game re-creates or re-parents its own interface windows now and then, which puts
       them above ours: checked every frame (cheap) so ours never stay hidden behind. */
    if (g_btn_on_top)
        bring_buttons_to_front();

    /* The icons follow the local player's age. */
    if (p >= 0)
        for (int i = 0; i < B_COUNT; i++)
            *(int *)(g_btn[i] + IFB_PLAYER) = p;
}
