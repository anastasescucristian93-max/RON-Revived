/*
 * "Units" in the game setup screen: a button beside the Nukes slider opens a window with every
 * unit of the game in columns, where units can be turned off for the game. Turned off, nobody
 * can train it or research it (the computer players neither).
 *
 *  - Two pages: Standard units (one upgrade line per column, a few short lines together) and
 *    Unique units (one column per nation, with its flag). The table is unitbans_data.h, made by
 *    camera-smooth\tools\gen_unitbans.py from unitrules.xml. Units are named by their internal
 *    NAME, so the several unit types of one NAME (the Tank in each nation's art) are one entry.
 *  - A click on a unit turns it off and every later upgrade of it with it (the game offers an
 *    upgrade only after the one before, so they could not be had anyway), or turns it back on
 *    with the units it is upgraded from. The X above a column turns the whole column off, or
 *    on again when it is all off. "Allow all" turns everything on.
 *  - The rule: LeaderData::type_eligible (datacenter.c's hook) asks unitbans_type_blocked; 0 =
 *    not offered, as the Nukes slider's None. The game asks it about the type a nation really
 *    gets (LeaderData::get_graft first), so a nation's unique unit is its own entry.
 *
 * Where the choice lives:
 *  - the window's list: ron_camera.ini [unit_bans] list = "<table signature>:<hex bits>";
 *  - multiplayer: the host puts the same text in the lobby as the key "ron_unit_bans"
 *    (MultiplayerManager::LobbySetKey, after each ConnectionData::send_game); every other
 *    machine reads it in GameSpy::UpdateLobbyFromDTO (the lobby's key / value list). Only the
 *    host can change it; the others see it, greyed;
 *  - the game: SetupWin::setup_game (nonukes.c's hook, every machine) copies the list into the
 *    game's and sets Game+0x20 bit 22 (units turned off) and bits 23..30 (a hash of the list).
 *    Saved games keep the flags dword, not the list: a loaded game uses the list of this session
 *    when its hash matches, else (single player) the window's list when that one matches.
 * Engine notes: camera-smooth/ENGINE_NOTES.md, "Units window".
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>
#include "unitbans_data.h"

#define VA_STRING_CTOR   0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_STRING_AS_CHAR 0x0042D710u /* String::str_as_char() const */
#define VA_STRING_ASSIGN 0x00A1EEB0u
#define VA_EMPTY_STRING  0x00EB437Cu
#define VA_TYPES         0x00C0A274u /* Type ** by type index: +0x60 internal name, +0x74 shown name */
#define VA_TRIBES        0x00E7FA34u /* Tribe *: 0x5F0 each, +0x18 the name (Tribe::get_name) */
#define VA_GAME_PTR      0x00C061ECu
#define VA_NETSYS        0x00E335C8u /* NetSys *: vtable +0x10 is the host */
#define VA_MP_INSTANCE   0x00CC4B40u /* MultiplayerManager::m_instance */
#define VA_LOBBY_SET_KEY 0x00A30B70u /* MultiplayerManager::LobbySetKey(const char *key, const char *value): ecx, edx */
#define VA_SEND_GAME     0x0094E200u /* ConnectionData::send_game() */
#define VA_LOBBY_DTO     0x00962C70u /* GameSpy::UpdateLobbyFromDTO(LobbyManager * ecx, const LobbyDTO & edx) */
#define VA_CALL_JOIN_INIT 0x0096290Fu /* GameSpy::OnLobbyJoined: call ConnectionData::init */
#define VA_CD_INIT       0x0094F440u /* ConnectionData::init(int) */
#define VT_BUTTON_REDRAW 0x00B3ACBCu /* SetupWin vtable slot: on_button_redraw(Button *, int) */
#define VA_ON_BUTTON_REDRAW 0x005C28E0u
#define VA_BUTTON_CTOR   0x005249A0u /* Button::Button() */
#define VA_BUTTON_INIT   0x00524E60u /* Button::init(const String &, id, x, y, w, h, Window *, int) */
#define VA_WINDOW_MOVE   0x00A54080u /* Window::move(x, y) */
#define VA_WINDOW_SHOW   0x00A55B30u /* Window::show(int) */
#define VA_WINDOW_HIDE   0x00A54780u /* Window::hide() */
#define VA_BRING_TO_TOP  0x00A53F90u
#define VA_SET_ID        0x00A542E0u
#define VA_SET_ENABLED   0x00A54110u
#define VA_GET_MOUSE_POS 0x00A54170u /* Window::get_mouse_pos(int *x, int *y): client coordinates */
#define VA_SOUND_PLAY    0x0097F770u /* SoundGlobal::play(cat) with ecx = the SetupWin: 0x111 the click */
#define VA_SET_CHANGED   0x005AE7D0u /* SetupWin::set_local_game_data_changed() */
#define VA_IFACE_RM      0x00EBD1C0u
#define VA_RM_ADD_RECT   0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_WRITE_L       0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len) */
#define VA_WRITE_CENT    0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_TEXT_WIDTH    0x0050EF40u /* Buffer::text_width(const String &) */
#define VA_SET_TEXT_COLOR 0x0050DAF0u
#define VA_FONTS         0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET      0x00522810u /* FontFamily::get(size, style) const */
#define VA_GRAPHSYS      0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define VA_WHITE         0x00C8D26Cu
#define VA_BLACK         0x00C8D260u
#define VA_COLOR_NONE    0x00EBEA04u

#define SW_MODE     0x1F24 /* & 2: single player */
#define SW_FONT     0x2268
#define SW_RULES    0x2A6C /* the rules panel, inside SetupWin */
#define VT_FIND     0x438
#define VT_SHOW     0x18
#define VT_RESIZE   0x20   /* GraphicWin::resize(w, h, PixFormat, int) */
#define VT_MOVE     0xD0
#define VT_CLICK    0xD8
#define VT_LDOWN    0xDC
#define VT_LUP      0xE0
#define VT_RCLICK   0xF0
#define VT_RDOWN    0xF4
#define VT_RUP      0xF8
#define VT_COPY     0x600
#define WIN_FLAGS   0x08   /* bit 0 visible, & 8 disabled */
#define WIN_RECT    0x126  /* short left, right, top, bottom (screen) */
#define WIN_BUFFER  0x154
#define BUF_FONT    0x7C
#define BUF_COLORS  0x90
#define BUF_COLORS_N 0x80
#define CB_FONT     0x1D0
#define GS_DEPTH    0x164
#define RENDER_SOLID 5
#define GAME_FLAGS  0x20
#define GAME_MP     0x820  /* byte, bit 2: multiplayer game */
#define BIT_ON      22     /* Game+0x20: units turned off in this game */
#define HASH_SHIFT  23     /* Game+0x20 bits 23..30: the list's hash */
#define BOX_ID      15
#define CLICK_SOUND 0x111
#define FIRST_TYPE  0x32
#define END_TYPE    0x19E
#define LOBBY_KEY   "ron_unit_bans"
#define UB_BYTES    ((UB_ENTRIES + 7) / 8)

/* colours: 0xAABBGGRR */
#define C_BACK    0xF6141A20u
#define C_TITLE   0xFF2A3644u
#define C_BORDER  0xFF3CBEE6u /* gold */
#define C_COLUMN  0xFF1E252Cu
#define C_HEAD    0xFF2A323Au
#define C_HOVER   0x5A3CBEE6u
#define C_OFF     0xC0202A70u /* a unit turned off: dark red */
#define C_OFF_X   0xFF3040D0u
#define C_TAB     0xFF262E36u
#define C_TAB_ON  0xFF3A4652u
#define C_BUTTON  0xFF2E3842u
/* text colours: 0xRRGGBB */
#define T_GOLD  0xE6BE3Cu
#define T_WHITE 0xF0F0F0u
#define T_GREY  0x8A9098u
#define T_DIM   0x5A6068u
#define T_RED   0xE06A5Au

typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_i)(void *self);
typedef void(__attribute__((thiscall)) *fn_xy_t)(void *self, int x, int y);
typedef void(__attribute__((thiscall)) *str_ctor_t)(void *self, const char *s);
typedef const char *(__attribute__((thiscall)) *as_char_t)(const void *self);
typedef void *(__attribute__((thiscall)) *find_t)(void *self, const void *name, int a);
typedef unsigned char(__attribute__((thiscall)) *is_host_t)(void *self);
typedef void(__attribute__((thiscall)) *mouse_t)(void *self, int *x, int *y);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef int(__attribute__((thiscall)) *text_width_t)(void *self, const void *str);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);
typedef int(__attribute__((thiscall)) *button_init_t)(void *self, const void *text, int id, int x, int y, int w, int h,
                                                      void *parent, int flag);
typedef int(__attribute__((thiscall)) *resize_t)(void *self, int w, int h, int a, int b);
typedef int(__attribute__((thiscall)) *move_t)(void *self, int x, int y);
typedef int(__attribute__((thiscall)) *button_redraw_t)(void *self, void *button, int id);
typedef void(__attribute__((thiscall)) *send_game_t)(void *self);
typedef void(__attribute__((thiscall)) *cd_init_t)(void *self, int a);
typedef void(__attribute__((fastcall)) *set_key_t)(const char *key, const char *value);

int g_unitbans_enabled;
static char g_ini[MAX_PATH];
static unsigned char g_local[UB_BYTES]; /* the window's list (single player, the host) */
static unsigned char g_lobby[UB_BYTES]; /* the host's, as the lobby gave it (the others) */
static int g_lobby_seen;
static unsigned char g_game[UB_BYTES]; /* the game's, set as it starts */
static int g_checked = -1;             /* the game's list hash already checked (each frame) */
static signed short g_entry_of[END_TYPE]; /* unit type -> entry, -1 none */
static signed char g_present[UB_ENTRIES]; /* the entry has a unit type in this game */
static short g_first_type[UB_ENTRIES];    /* its first unit type (for the shown name) */
static int g_mapped;
static void *g_orig_send_game; /* the lobby hooks are in */

/* ---- the list ------------------------------------------------------------------------------- */

static int bit(const unsigned char *l, int e) { return l[e >> 3] >> (e & 7) & 1; }
static void set_bit(unsigned char *l, int e, int on)
{
    if (on)
        l[e >> 3] |= (unsigned char)(1 << (e & 7));
    else
        l[e >> 3] &= (unsigned char)~(1 << (e & 7));
}

static int any(const unsigned char *l)
{
    for (int i = 0; i < UB_BYTES; i++)
        if (l[i])
            return 1;
    return 0;
}

static unsigned hash8(const unsigned char *l)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < UB_BYTES; i++)
        h = (h ^ l[i]) * 16777619u;
    return (h ^ h >> 8 ^ h >> 16 ^ h >> 24) & 0xFF;
}

/* A unit off takes its later upgrades with it. */
static void normalize(unsigned char *l)
{
    for (int changed = 1; changed;) {
        changed = 0;
        for (int e = 0; e < UB_ENTRIES; e++)
            if (k_ub_from[e] >= 0 && bit(l, k_ub_from[e]) && !bit(l, e)) {
                set_bit(l, e, 1);
                changed = 1;
            }
    }
}

static void turn_off(unsigned char *l, int e)
{
    set_bit(l, e, 1);
    normalize(l);
}

/* On again with the units it is upgraded from. */
static void turn_on(unsigned char *l, int e)
{
    for (int n = 0; e >= 0 && n < UB_ENTRIES; n++, e = k_ub_from[e])
        set_bit(l, e, 0);
}

static void to_text(const unsigned char *l, char *out, int size)
{
    int n = snprintf(out, size, "%08X:", UB_SIG);
    for (int i = 0; i < UB_BYTES && n + 3 < size; i++)
        n += snprintf(out + n, size - n, "%02X", l[i]);
}

/* 1 when the text is a list of this table (an empty text is an empty list) */
static int from_text(const char *s, unsigned char *l)
{
    memset(l, 0, UB_BYTES);
    if (!s || !*s)
        return 1;
    unsigned sig = 0;
    int at = 0;
    if (sscanf(s, "%8x:%n", &sig, &at) != 1 || !at || sig != UB_SIG)
        return 0;
    s += at;
    for (int i = 0; i < UB_BYTES && s[0] && s[1]; i++, s += 2) {
        unsigned v;
        if (sscanf(s, "%2x", &v) != 1)
            return 0;
        l[i] = (unsigned char)v;
    }
    normalize(l);
    return 1;
}

static void save_local(void)
{
    char text[2 * UB_BYTES + 16];
    to_text(g_local, text, sizeof text);
    WritePrivateProfileStringA("unit_bans", "list", any(g_local) ? text : "", g_ini);
}

void unitbans_load_config(const char *ini_path)
{
    snprintf(g_ini, sizeof g_ini, "%s", ini_path);
    g_unitbans_enabled = GetPrivateProfileIntA("gameplay", "unit_bans_option", 1, ini_path);
    char text[2 * UB_BYTES + 16];
    GetPrivateProfileStringA("unit_bans", "list", "", text, sizeof text, ini_path);
    if (!from_text(text, g_local))
        logf_("units: the saved list is of another unit table, all units on");
    memcpy(g_game, g_local, UB_BYTES); /* for a saved game loaded before any setup */
}

/* ---- unit types ----------------------------------------------------------------------------- */

static char *type_of(int t)
{
    char **types = *(char ***)VA(VA_TYPES);
    return types ? types[t] : NULL;
}

static void map_types(void)
{
    if (g_mapped)
        return;
    if (!type_of(FIRST_TYPE))
        return; /* the rules are not loaded yet */
    memset(g_entry_of, 0xFF, sizeof g_entry_of);
    memset(g_present, 0, sizeof g_present);
    int n = 0;
    for (int t = FIRST_TYPE; t < END_TYPE; t++) {
        char *type = type_of(t);
        const char *name = type ? ((as_char_t)VA(VA_STRING_AS_CHAR))(type + 0x60) : NULL;
        if (!name)
            continue;
        for (int e = 0; e < UB_ENTRIES; e++)
            if (!_stricmp(name, k_ub_name[e])) {
                g_entry_of[t] = (short)e;
                if (!g_present[e]) {
                    n++;
                    g_first_type[e] = (short)t;
                }
                g_present[e] = 1;
                break;
            }
    }
    g_mapped = 1;
    logf_("units: %d of %d units of the table found among the unit types", n, UB_ENTRIES);
}

/* The first unit type of an entry (for its shown name). */
static char *entry_type(int e) { return g_present[e] ? type_of(g_first_type[e]) : NULL; }

/* ---- the rule ------------------------------------------------------------------------------- */

static unsigned *game_flags(void)
{
    char *g = *(char **)VA(VA_GAME_PTR);
    return g ? (unsigned *)(g + GAME_FLAGS) : NULL;
}

static int multiplayer_game(void)
{
    char *g = *(char **)VA(VA_GAME_PTR);
    return g && (g[GAME_MP] & 4);
}

int unitbans_type_blocked(int type)
{
    if (!g_unitbans_enabled || type < FIRST_TYPE || type >= END_TYPE)
        return 0;
    unsigned *f = game_flags();
    if (!f || !(*f >> BIT_ON & 1))
        return 0;
    if (!g_mapped) {
        map_types();
        if (!g_mapped)
            return 0;
    }
    int e = g_entry_of[type];
    return e >= 0 && bit(g_game, e);
}

/* Every simulation frame (nonukes_frame): a loaded game whose list is not this session's. */
void unitbans_frame(void)
{
    unsigned *f = game_flags();
    if (!g_unitbans_enabled || !f || !(*f >> BIT_ON & 1)) {
        g_checked = -1;
        return;
    }
    int want = (int)(*f >> HASH_SHIFT & 0xFF);
    if (want == g_checked)
        return;
    g_checked = want;
    if ((int)hash8(g_game) == want)
        return;
    if (!multiplayer_game() && (int)hash8(g_local) == want) {
        memcpy(g_game, g_local, UB_BYTES);
        logf_("units: a saved game, its units off are the window's list");
        return;
    }
    logf_("units: a saved game with units off, but its list is not known here; keeping this session's");
}

/* ---- the setup screen ----------------------------------------------------------------------- */

static char *g_sw, *g_box;
static void *g_box_vt[VT_COPY / 4 + 1];
static int g_box_down;
static _Alignas(16) unsigned char g_panel[0x400];
static void *g_panel_vt[VT_COPY / 4 + 1];
static int g_panel_made, g_panel_ready, g_panel_down;

static int solo(void) { return g_sw && (*(int *)(g_sw + SW_MODE) & 2); }

static int is_host(void)
{
    char *net = *(char **)VA(VA_NETSYS);
    return net && ((is_host_t)(*(void ***)net)[0x10 / 4])(net);
}

static int may_change(void) { return g_sw && (solo() || is_host()); }

/* The list the window shows: its own, or in multiplayer on a machine that is not the host, the
   lobby's. */
static unsigned char *shown_list(void) { return may_change() ? g_local : g_lobby; }

static int count_off(const unsigned char *l, int *total)
{
    int n = 0, all = 0;
    for (int e = 0; e < UB_ENTRIES; e++)
        if (g_present[e]) {
            all++;
            n += bit(l, e);
        }
    if (total)
        *total = all;
    return n;
}

static void *game_string(const char *s, unsigned char str[64])
{
    memset(str, 0, 64);
    ((str_ctor_t)VA(VA_STRING_CTOR))(str, s);
    return str;
}

/* host: the list into the lobby */
static void send_lobby(void)
{
    char **mm = *(char ***)VA(VA_MP_INSTANCE);
    if (!mm || !*mm)
        return; /* no lobby */
    char text[2 * UB_BYTES + 16];
    to_text(g_local, text, sizeof text);
    ((set_key_t)VA(VA_LOBBY_SET_KEY))(LOBBY_KEY, text);
    memcpy(g_lobby, g_local, UB_BYTES);
    g_lobby_seen = 1;
    static char last[2 * UB_BYTES + 16];
    if (strcmp(last, text)) {
        snprintf(last, sizeof last, "%s", text);
        logf_("units: list sent to the lobby (%d off)", count_off(g_local, NULL));
    }
}

static void changed(void)
{
    save_local();
    if (g_sw && !solo()) {
        ((fn_v)VA(VA_SET_CHANGED))(g_sw); /* the host's next send_game sends the list too */
        if (is_host())
            send_lobby();
    }
}

static void click_sound(void)
{
    if (g_sw)
        ((fn_v_i)VA(VA_SOUND_PLAY))(g_sw, CLICK_SOUND);
}

/* ---- text ----------------------------------------------------------------------------------- */

/* Text into a window's Buffer in a font size and colour (0xRRGGBB); `str` a game String. */
static void put_string(char *win, int l, int t, int r, int b, const void *str, int size, int bold, unsigned rgb,
                       int centred)
{
    int len = str ? *(unsigned short *)((const char *)str + 8) : 0;
    if (!len)
        return;
    char *buf = win + WIN_BUFFER;
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), size, bold ? 1 : 0);
    if (!font && !*(void **)(buf + BUF_FONT))
        return;
    unsigned char saved[12], saved_colors[BUF_COLORS_N];
    memcpy(saved, buf + BUF_FONT, 12);
    memcpy(saved_colors, buf + BUF_COLORS, BUF_COLORS_N);
    if (font) {
        *(void **)(buf + BUF_FONT) = font;
        *(int *)(buf + BUF_FONT + 4) = 0;
        *(int *)(buf + BUF_FONT + 8) = 0;
    }
    _Alignas(4) unsigned char color[12];
    memcpy(color, (void *)VA(VA_WHITE), 12);
    color[0] = (unsigned char)(rgb >> 16);
    color[1] = (unsigned char)(rgb >> 8);
    color[2] = (unsigned char)rgb;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, (void *)VA(VA_BLACK), -1, 0);
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((write_t)VA(centred ? VA_WRITE_CENT : VA_WRITE_L))(buf, str, rect, len);
    memcpy(buf + BUF_FONT, saved, 12);
    memcpy(buf + BUF_COLORS, saved_colors, BUF_COLORS_N);
}

/* English UTF-8 text, shown in the chosen language (lang_text) */
static void put_text(char *win, int l, int t, int r, int b, const char *s, int size, int bold, unsigned rgb,
                     int centred)
{
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text(s));
    put_string(win, l, t, r, b, str, size, bold, rgb, centred);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
}

static int string_width(char *win, const void *str, int size, int bold)
{
    char *buf = win + WIN_BUFFER;
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), size, bold ? 1 : 0);
    if (!str || (!font && !*(void **)(buf + BUF_FONT)))
        return 0;
    unsigned char saved[12];
    memcpy(saved, buf + BUF_FONT, 12);
    if (font) {
        *(void **)(buf + BUF_FONT) = font;
        *(int *)(buf + BUF_FONT + 4) = 0;
        *(int *)(buf + BUF_FONT + 8) = 0;
    }
    int w = ((text_width_t)VA(VA_TEXT_WIDTH))(buf, str);
    memcpy(buf + BUF_FONT, saved, 12);
    return w;
}

static int text_width(char *win, const char *s, int size, int bold)
{
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text(s));
    int w = string_width(win, str, size, bold);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    return w;
}

/* Romania's own units: their names from romania_units.ini [names] (the setup screen shows the
   game's names of the campaign slots they live in). */
static unsigned char g_ro_name[UB_ENTRIES][32];
static signed char g_ro_named[UB_ENTRIES]; /* 0 not looked up, 1 named, -1 none */

static const void *entry_name(int e)
{
    if (k_ub_romania_only[e]) {
        if (!g_ro_named[e]) {
            char ini[MAX_PATH], v[96];
            snprintf(ini, sizeof ini, "%sromania_units.ini", g_dir);
            GetPrivateProfileStringA("names", k_ub_name[e], "", v, sizeof v, ini);
            g_ro_named[e] = v[0] ? 1 : -1;
            if (v[0])
                ((str_ctor_t)VA(VA_STRING_CTOR))(g_ro_name[e], v);
        }
        if (g_ro_named[e] > 0)
            return g_ro_name[e];
    }
    char *type = entry_type(e);
    return type ? type + 0x74 : NULL;
}

static const void *tribe_name(int t)
{
    char *tribes = *(char **)VA(VA_TRIBES);
    return tribes && t >= 0 && t < g_num_tribes ? tribes + t * 0x5F0 + 0x18 : NULL;
}

/* ---- drawing helpers ------------------------------------------------------------------------ */

#define VT_FIND_TEX    0xA4  /* GraphicWinXML::find_tex(const String &, int, int *) */
#define VT_FIND_BORDER 0x494 /* GraphicWinXML::find_border(const String &, int, int *) */
#define VA_DRAW_BORDER 0x0051D260u /* IFaceRenderManager::draw_border(Rect *, corners, edges, extras, Color) */
#define STYLE_TOP      0x2000 /* Window +4: kept in front of its siblings (Window::bring_child_to_top) */

typedef void *(__attribute__((thiscall)) *find_xml_t)(void *self, const void *name, int a, int *b);
typedef void(__attribute__((thiscall)) *border_t)(void *rm, const short *rect, const void *corners, const void *edges,
                                                   const void *extras, unsigned c0, unsigned c1, unsigned c2);

/* the art: setupwin.xml's SETUPWIN block (ui-theme\tools\build_unit_bans.py) */
enum {
    P_PLATE, P_CELL, P_X_OFF, P_X_OFF_H, P_X_ON, P_X_ON_H, P_GLOW, P_WELL, P_BAND, P_LINE, P_BAN, P_TAB, P_TAB_ON,
    P_BUTTON, P_BUTTON_H, P_COUNT
};
static const char *const k_piece[P_COUNT] = { "ub_plate", "ub_cell",  "ub_x_off", "ub_x_off_hover", "ub_x_on",
                                              "ub_x_on_hover", "ub_glow", "ub_well", "ub_band", "ub_line",
                                              "ub_ban", "ub_tab", "ub_tab_on", "ub_button", "ub_button_hover" };
static const unsigned char *g_piece[P_COUNT], *g_icons[4];
static const char *g_border;
static int g_art; /* every piece found */

static const void *find_xml(char *sw, int slot, const char *name)
{
    unsigned char str[64];
    memset(str, 0, sizeof str);
    ((str_ctor_t)VA(VA_STRING_CTOR))(str, name);
    const void *r = ((find_xml_t)(*(void ***)sw)[slot / 4])(sw, str, -1, NULL);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    return r;
}

static void find_art(char *sw)
{
    g_art = 1;
    for (int i = 0; i < P_COUNT; i++)
        if (!(g_piece[i] = find_xml(sw, VT_FIND_TEX, k_piece[i])))
            g_art = 0;
    for (int i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof name, "ub_icons_%d", i);
        g_icons[i] = find_xml(sw, VT_FIND_TEX, name);
    }
    g_border = find_xml(sw, VT_FIND_BORDER, "ub_border");
    static int said;
    if (!said++)
        logf_("units: window art %s, icon pages %s%s%s%s, frame %s", g_art ? "found" : "MISSING (setupwin.xml)",
              g_icons[0] ? "0" : "-", g_icons[1] ? "1" : "-", g_icons[2] ? "2" : "-", g_icons[3] ? "3" : "-",
              g_border ? "found" : "missing");
}

static void add(const void *tex, int l, int t, int r, int b, unsigned color, int type)
{
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, type);
}

static void fill(int l, int t, int r, int b, unsigned color) { add(NULL, l, t, r, b, color, RENDER_SOLID); }

static void frame(int l, int t, int r, int b, unsigned color)
{
    fill(l, t, r, t + 1, color);
    fill(l, b - 1, r, b, color);
    fill(l, t, l + 1, b, color);
    fill(r - 1, t, r, b, color);
}

/* a piece stretched over the rectangle (a flat colour when the art is missing) */
static void piece(int p, int l, int t, int r, int b, unsigned flat)
{
    if (g_piece[p])
        add(g_piece[p], l, t, r, b, 0xFFFFFFFFu, 1);
    else if (flat)
        fill(l, t, r, b, flat);
}

/* a piece repeated over the rectangle, the last row and column cut short (as selpanel.c) */
static void tile(int p, int l, int t, int r, int b, unsigned flat)
{
    const unsigned char *tex = g_piece[p];
    if (!tex) {
        fill(l, t, r, b, flat);
        return;
    }
    int tw = *(short *)(tex + 6) - *(short *)(tex + 4), th = *(short *)(tex + 10) - *(short *)(tex + 8);
    if (tw <= 0 || th <= 0)
        return;
    static _Alignas(16) unsigned char part[16];
    for (int y = t; y < b; y += th)
        for (int x = l; x < r; x += tw) {
            int w = r - x < tw ? r - x : tw, h = b - y < th ? b - y : th;
            memcpy(part, tex, 16);
            *(short *)(part + 6) = (short)(*(short *)(tex + 4) + w);
            *(short *)(part + 10) = (short)(*(short *)(tex + 8) + h);
            add(part, x, y, x + w, y + h, 0xFFFFFFFFu, 1);
        }
}

/* a unit's icon (its cell on an icon page, as IFaceButton::on_redraw cuts it); 0 without one */
static int icon(int e, int l, int t, int r, int b, unsigned color)
{
    int page = k_ub_icon[e][0];
    if (page < 0 || page > 3 || !g_icons[page])
        return 0;
    static _Alignas(16) unsigned char cell[16];
    memcpy(cell, g_icons[page], 16);
    short *q = (short *)(cell + 4); /* x0, x1, y0, y1 */
    int dx = (q[1] - q[0] + 1) * k_ub_icon[e][1], dy = (q[3] - q[2] + 1) * k_ub_icon[e][2];
    q[0] = (short)(q[0] + dx), q[1] = (short)(q[1] + dx), q[2] = (short)(q[2] + dy), q[3] = (short)(q[3] + dy);
    add(cell, l, t, r, b, color, 1);
    return 1;
}

static int *depth(void) { return (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH); }

static int inside(int x, int y, const short *r) { return x >= r[0] && x < r[1] && y >= r[2] && y < r[3]; }

/* ---- the window's layout -------------------------------------------------------------------- */

enum { K_UNIT, K_COLUMN, K_TAB, K_ALLOW, K_CLOSE };

typedef struct {
    short r[4]; /* left, right, top, bottom in the window */
    char kind;
    short arg;  /* entry, column, tab */
    short x[4]; /* a column's X button */
} cell_t;

#define MAX_COLS  32
#define MAX_CELLS 400
static struct {
    int tab;                  /* 0 standard, 1 unique */
    int w, h;                 /* the window (the whole setup screen) */
    short box[4];             /* the panel inside it, from the top of the screen */
    int title_b, foot_t;
    int pct;                  /* the scale, percent */
    int ncols;
    short col[MAX_COLS][4];   /* each column's rectangle, header included */
    short col_list[MAX_COLS]; /* the column's start in k_ub_std / k_ub_nation */
    short col_tribe[MAX_COLS];
    cell_t cells[MAX_CELLS];
    int ncells;
} L;

/* The columns of a page: starts in their table and, for nations, the tribe. */
static int page_columns(int tab, short *start, short *tribe)
{
    int n = 0;
    const short *tbl = tab ? k_ub_nation : k_ub_std;
    int count = tab ? UB_NATIONS : UB_STD_COLS;
    int at = 0;
    for (int c = 0; c < count; c++) {
        int first = at, shown = 0;
        while (tbl[at] >= 0) {
            shown += g_present[tbl[at]];
            at++;
        }
        at++;
        if (!shown || n >= MAX_COLS || (tab && c >= g_num_tribes))
            continue;
        start[n] = (short)first;
        tribe[n] = (short)c;
        n++;
    }
    return n;
}

static int column_rows(int tab, int start)
{
    const short *tbl = tab ? k_ub_nation : k_ub_std;
    int n = 0;
    for (int i = start; tbl[i] >= 0; i++)
        n += g_present[tbl[i]];
    return n;
}

static cell_t *add_cell(int l, int t, int r, int b, int kind, int arg)
{
    static cell_t spare;
    if (L.ncells >= MAX_CELLS)
        return &spare;
    cell_t *c = &L.cells[L.ncells++];
    c->r[0] = (short)l, c->r[1] = (short)r, c->r[2] = (short)t, c->r[3] = (short)b;
    c->kind = (char)kind;
    c->arg = (short)arg;
    return c;
}

/* sizes at 100 %: the icons are the game's 61 x 39 */
#define PAD      16
#define TITLE    44
#define TABS     38
#define FOOT     50
#define GAP      6
#define BAND_GAP 12
#define HEAD     32
#define ICON_W   61
#define ICON_H   39
#define SLOT     3  /* the slot's rim round the icon */
#define ROW_GAP  4
#define COL_IN   4  /* the column's margin round the slots */
#define XB       24 /* the round X */

static int S(int v) { return (v * L.pct + 50) / 100; }

static int fits(int n, short *start, int *per_out, int *h_out)
{
    int cw = S(ICON_W + 2 * SLOT + 2 * COL_IN), gap = S(GAP);
    int per = (L.w - 16 - 2 * S(PAD) + gap) / (cw + gap);
    if (per < 1)
        per = 1;
    if (per > n)
        per = n;
    int bands = n ? (n + per - 1) / per : 1;
    per = n ? (n + bands - 1) / bands : 1;
    int h = S(TITLE) + S(TABS) + S(FOOT) + S(PAD) + (bands - 1) * S(BAND_GAP);
    for (int b = 0; b < bands; b++) {
        int most = 0;
        for (int c = b * per; c < n && c < (b + 1) * per; c++) {
            int r = column_rows(L.tab, start[c]);
            if (r > most)
                most = r;
        }
        h += S(HEAD) + most * S(ICON_H + 2 * SLOT + ROW_GAP) + S(COL_IN);
    }
    *per_out = per;
    *h_out = h;
    return h <= L.h - 4;
}

static void layout(void)
{
    short *sr = (short *)(g_sw + WIN_RECT);
    L.w = sr[1] - sr[0];
    L.h = sr[3] - sr[2];
    short start[MAX_COLS], tribe[MAX_COLS];
    int n = page_columns(L.tab, start, tribe);
    L.ncols = n;
    L.ncells = 0;
    /* the largest scale at which the page fits under the top of the screen */
    int per = 1, ph = 0;
    for (L.pct = 100; L.pct > 45; L.pct -= 5)
        if (fits(n, start, &per, &ph))
            break;
    int bands = n ? (n + per - 1) / per : 1;
    int cw = S(ICON_W + 2 * SLOT + 2 * COL_IN), gap = S(GAP);
    int pw = 2 * S(PAD) + per * cw + (per - 1) * gap;
    char *win = (char *)g_panel;
    int need = text_width(win, "Standard units", 12, 1) + text_width(win, "Unique units", 12, 1) +
               text_width(win, "Allow all", 12, 1) + 3 * 40 + 2 * S(PAD) + 40;
    if (pw < need)
        pw = need;
    if (pw < 560)
        pw = 560;
    if (pw > L.w)
        pw = L.w;
    if (ph > L.h)
        ph = L.h;
    int pl = (L.w - pw) / 2, pt = 0; /* anchored to the top of the screen */
    L.box[0] = (short)pl, L.box[1] = (short)(pl + pw), L.box[2] = (short)pt, L.box[3] = (short)(pt + ph);
    L.title_b = pt + S(TITLE);
    L.foot_t = L.box[3] - S(FOOT);

    /* the close X; the two pages; Allow all */
    int xb = XB, xt = pt + (S(TITLE) - xb) / 2;
    add_cell(pl + pw - S(PAD) - xb, xt, pl + pw - S(PAD), xt + xb, K_CLOSE, 0);
    int tt = L.title_b + (S(TABS) - 26) / 2, tl = pl + S(PAD);
    for (int t = 0; t < 2; t++) {
        int w = text_width(win, t ? "Unique units" : "Standard units", 12, 1) + 40;
        add_cell(tl, tt, tl + w, tt + 26, K_TAB, t);
        tl += w + 8;
    }
    int aw = text_width(win, "Allow all", 12, 1) + 40;
    add_cell(pl + pw - S(PAD) - aw, tt, pl + pw - S(PAD), tt + 26, K_ALLOW, 0);

    /* the columns, band by band: a header (the X, a nation's flag) over the unit slots */
    const short *tbl = L.tab ? k_ub_nation : k_ub_std;
    int y = L.title_b + S(TABS);
    int band_w = per * cw + (per - 1) * gap, x0 = pl + (pw - band_w) / 2;
    int sw_ = S(ICON_W + 2 * SLOT), sh = S(ICON_H + 2 * SLOT), step = S(ICON_H + 2 * SLOT + ROW_GAP);
    for (int b = 0; b < bands; b++) {
        int most = 0;
        for (int c = b * per; c < n && c < (b + 1) * per; c++) {
            int r = column_rows(L.tab, start[c]);
            if (r > most)
                most = r;
        }
        for (int c = b * per; c < n && c < (b + 1) * per; c++) {
            int x = x0 + (c - b * per) * (cw + gap);
            L.col[c][0] = (short)x, L.col[c][1] = (short)(x + cw), L.col[c][2] = (short)y;
            L.col[c][3] = (short)(y + S(HEAD) + most * step + S(COL_IN));
            L.col_list[c] = start[c];
            L.col_tribe[c] = tribe[c];
            cell_t *h = add_cell(x, y, x + cw, y + S(HEAD), K_COLUMN, c);
            int bl = L.tab ? x + cw - S(COL_IN) - xb : x + (cw - xb) / 2, bt = y + (S(HEAD) - xb) / 2;
            h->x[0] = (short)bl, h->x[1] = (short)(bl + xb), h->x[2] = (short)bt, h->x[3] = (short)(bt + xb);
            int ry = y + S(HEAD), rx = x + (cw - sw_) / 2;
            for (int i = start[c]; tbl[i] >= 0; i++)
                if (g_present[tbl[i]]) {
                    add_cell(rx, ry, rx + sw_, ry + sh, K_UNIT, tbl[i]);
                    ry += step;
                }
        }
        y += S(HEAD) + most * step + S(COL_IN) + S(BAND_GAP);
    }
}

/* ---- the window: drawing -------------------------------------------------------------------- */

static int column_all_off(const unsigned char *l, int c)
{
    const short *tbl = L.tab ? k_ub_nation : k_ub_std;
    for (int i = L.col_list[c]; tbl[i] >= 0; i++)
        if (g_present[tbl[i]] && !bit(l, tbl[i]))
            return 0;
    return 1;
}

static void round_x(const short *r, int red, int hover)
{
    int p = red ? (hover ? P_X_ON_H : P_X_ON) : (hover ? P_X_OFF_H : P_X_OFF);
    if (g_piece[p]) {
        piece(p, r[0], r[2], r[1], r[3], 0);
        return;
    }
    fill(r[0], r[2], r[1], r[3], red ? 0xFF2020A0u : 0xFF202020u);
    frame(r[0], r[2], r[1], r[3], 0xFF3CBEE6u);
}

static void draw_panel(void)
{
    char *win = (char *)g_panel;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(win, &mx, &my);
    const unsigned char *l = shown_list();
    int edit = may_change();
    short *B = L.box;
    int base = *depth();

    /* the steel plate, the title band, the gold lines (one atlas: drawn in this order) */
    tile(P_PLATE, B[0], B[2], B[1], B[3], 0xFF241C1Au);
    piece(P_BAND, B[0], B[2], B[1], L.title_b, 0);
    piece(P_LINE, B[0] + 12, L.title_b - 1, B[1] - 12, L.title_b + 1, 0xFF3CBEE6u);
    piece(P_LINE, B[0] + 12, L.foot_t, B[1] - 12, L.foot_t + 2, 0xFF3CBEE6u);
    for (int i = 0; i < L.ncols; i++)
        piece(P_WELL, L.col[i][0], L.col[i][2], L.col[i][1], L.col[i][3], 0x80000000u);

    int hover_kind = -1, hover_arg = -1;
    for (int i = 0; i < L.ncells; i++) {
        cell_t *c = &L.cells[i];
        int hover = inside(mx, my, c->r);
        if (hover)
            hover_kind = c->kind, hover_arg = c->arg;
        switch (c->kind) {
        case K_CLOSE:
            round_x(c->r, 0, hover);
            break;
        case K_TAB:
            piece(c->arg == L.tab ? P_TAB_ON : P_TAB, c->r[0], c->r[2], c->r[1], c->r[3],
                  c->arg == L.tab ? 0xFF3A4652u : 0xFF262E36u);
            break;
        case K_ALLOW:
            piece(edit && hover ? P_BUTTON_H : P_BUTTON, c->r[0], c->r[2], c->r[1], c->r[3], 0xFF2E3842u);
            break;
        case K_COLUMN:
            round_x(c->x, column_all_off(l, c->arg), edit && inside(mx, my, c->x));
            break;
        case K_UNIT:
            piece(P_CELL, c->r[0], c->r[2], c->r[1], c->r[3], 0xFF101010u);
            break;
        }
    }

    /* the frame: the setup panels' own steel border */
    *depth() = base + 1;
    if (g_border) {
        const unsigned *white = (const unsigned *)VA(VA_WHITE);
        ((border_t)VA(VA_DRAW_BORDER))((void *)VA(VA_IFACE_RM), B, g_border, g_border + 0x40, g_border + 0x80,
                                       white[0], white[1], white[2]);
    } else {
        frame(B[0], B[2], B[1], B[3], 0xFF3CBEE6u);
    }

    /* the icons, in front of the slots; a unit that is off darker */
    int in = S(SLOT);
    for (int i = 0; i < L.ncells; i++) {
        cell_t *c = &L.cells[i];
        if (c->kind == K_UNIT && !icon(c->arg, c->r[0] + in, c->r[2] + in, c->r[1] - in, c->r[3] - in,
                                       bit(l, c->arg) ? 0xFF707070u : 0xFFFFFFFFu))
            put_string(win, c->r[0] + 4, c->r[2] + 4, c->r[1] - 2, c->r[3], entry_name(c->arg), 11, 0, T_WHITE, 0);
    }

    /* over them: the red X of a unit that is off, the gold wash under the mouse */
    *depth() = base + 2;
    for (int i = 0; i < L.ncells; i++) {
        cell_t *c = &L.cells[i];
        if (c->kind == K_UNIT && bit(l, c->arg))
            piece(P_BAN, c->r[0] + in, c->r[2] + in, c->r[1] - in, c->r[3] - in, 0x802020C0u);
        if (c->kind == K_UNIT && edit && inside(mx, my, c->r))
            piece(P_GLOW, c->r[0], c->r[2], c->r[1], c->r[3], 0x403CBEE6u);
    }
    if (L.tab)
        for (int i = 0; i < L.ncells; i++) {
            cell_t *c = &L.cells[i];
            void *flag = c->kind == K_COLUMN ? flags_tex(L.col_tribe[c->arg]) : NULL;
            if (flag) {
                int s = c->r[3] - c->r[2] - 4;
                short fr[4] = { (short)(c->r[0] + S(COL_IN)), (short)(c->r[0] + S(COL_IN) + s), (short)(c->r[2] + 2),
                                (short)(c->r[2] + 2 + s) };
                flags_draw(flag, fr, *(unsigned *)VA(VA_WHITE));
            }
        }

    /* the words: the title, the tabs, the name of what the mouse is on, the count and the hint */
    *depth() = base + 4;
    put_text(win, B[0], B[2], B[1], L.title_b, "UNITS IN THIS GAME", 14, 1, T_GOLD, 1);
    for (int i = 0; i < L.ncells; i++) {
        cell_t *c = &L.cells[i];
        if (c->kind == K_TAB)
            put_text(win, c->r[0], c->r[2], c->r[1], c->r[3], c->arg ? "Unique units" : "Standard units", 12, 1,
                     c->arg == L.tab ? T_GOLD : T_GREY, 1);
        else if (c->kind == K_ALLOW)
            put_text(win, c->r[0], c->r[2], c->r[1], c->r[3], "Allow all", 12, 1, edit ? T_WHITE : T_DIM, 1);
    }
    int ft = L.foot_t + 6, fl = B[0] + S(PAD), fr = B[1] - S(PAD);
    if (hover_kind == K_UNIT)
        put_string(win, fl, ft, fr, ft + 18, entry_name(hover_arg), 14, 1, bit(l, hover_arg) ? T_RED : T_GOLD, 0);
    else if (hover_kind == K_COLUMN && L.tab)
        put_string(win, fl, ft, fr, ft + 18, tribe_name(L.col_tribe[hover_arg]), 14, 1, T_GOLD, 0);
    int total = 0, n = count_off(l, &total);
    char s[160];
    snprintf(s, sizeof s, lang_text("%d of %d units turned off"), n, total);
    unsigned char str[64] = { 0 };
    lang_string(str, s);
    int sw2 = string_width(win, str, 12, 1);
    put_string(win, fr - sw2, ft, fr, ft + 18, str, 12, 1, n ? T_RED : T_GREY, 0);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    put_text(win, fl, ft + 20, fr, B[3] - 2,
             edit ? "Click a unit to turn it off, with its later upgrades. The X turns off a whole column."
                  : "Only the host can change them.",
             11, 0, T_GREY, 0);
    *depth() = base;
}

/* ---- the window: open, close, clicks -------------------------------------------------------- */

static int is_open(void) { return g_panel_ready && (*(unsigned *)(g_panel + WIN_FLAGS) & 1); }

static void panel_close(void)
{
    if (is_open())
        ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
}

static void panel_open(void)
{
    if (!g_panel_ready || !g_sw)
        return;
    map_types();
    find_art(g_sw);
    short *sr = (short *)(g_sw + WIN_RECT);
    ((resize_t)(*(void ***)g_panel)[VT_RESIZE / 4])(g_panel, sr[1] - sr[0], sr[3] - sr[2], 0, 0);
    ((move_t)VA(VA_WINDOW_MOVE))(g_panel, 0, 0);
    layout();
    g_panel_down = 0;
    *(unsigned *)(g_panel + 4) |= STYLE_TOP; /* in front of the setup screen's own TOP panels */
    ((fn_v_i)VA(VA_WINDOW_SHOW))(g_panel, 1);
    ((fn_v)VA(VA_BRING_TO_TOP))(g_panel);
    int total = 0, n = count_off(shown_list(), &total);
    logf_("units: window open (%d of %d off, %d columns, scale %d%%, %dx%d)", n, total, L.ncols, L.pct,
          L.box[1] - L.box[0], L.box[3] - L.box[2]);
}

static void panel_click(int x, int y)
{
    if (!inside(x, y, L.box)) {
        panel_close(); /* a click beside the window closes it */
        return;
    }
    for (int i = 0; i < L.ncells; i++) {
        cell_t *c = &L.cells[i];
        if (!inside(x, y, c->r))
            continue;
        if (c->kind == K_CLOSE) {
            click_sound();
            panel_close();
            return;
        }
        if (c->kind == K_TAB) {
            if (c->arg != L.tab) {
                click_sound();
                L.tab = c->arg;
                layout();
            }
            return;
        }
        if (!may_change())
            return;
        if (c->kind == K_ALLOW) {
            memset(g_local, 0, UB_BYTES);
        } else if (c->kind == K_COLUMN) {
            const short *tbl = L.tab ? k_ub_nation : k_ub_std;
            int off = column_all_off(g_local, c->arg);
            for (int k = L.col_list[c->arg]; tbl[k] >= 0; k++)
                if (g_present[tbl[k]]) {
                    if (off)
                        turn_on(g_local, tbl[k]);
                    else
                        set_bit(g_local, tbl[k], 1);
                }
            normalize(g_local);
        } else if (c->kind == K_UNIT) {
            if (bit(g_local, c->arg))
                turn_on(g_local, c->arg);
            else
                turn_off(g_local, c->arg);
        }
        click_sound();
        changed();
        return;
    }
}

/* the window's own mouse slots (a copy of the Button vtable) */
__attribute__((used, thiscall)) static void c_panel_nothing(char *w, int x, int y) { (void)w, (void)x, (void)y; }

__attribute__((used, thiscall)) static void c_panel_ldown(char *w, int x, int y)
{
    (void)x, (void)y;
    if (w != (char *)g_panel || !is_open())
        return;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(w, &mx, &my);
    g_panel_down = 1;
    panel_click(mx, my);
}

__attribute__((used, thiscall)) static void c_panel_rdown(char *w, int x, int y)
{
    (void)x, (void)y;
    if (w == (char *)g_panel)
        panel_close(); /* right-click: close, as most of the game's windows */
}

/* SetupWin::on_button_redraw: the window is drawn by hand. */
__attribute__((used, thiscall)) static int c_ub_button_redraw(char *sw, char *button, int id)
{
    if (survival_button_redraw(sw, button)) /* the Survival setup's hero buttons */
        return 1;
    if (button == (char *)g_panel && g_panel_ready) {
        if (g_sw)
            draw_panel();
        return 1;
    }
    return ((button_redraw_t)VA(VA_ON_BUTTON_REDRAW))(sw, button, id);
}

static void own_vtable(char *win, void **copy, void (*ldown)(void), void (*lup)(void), void (*rdown)(void))
{
    void **vt = *(void ***)win;
    if (vt == copy + 1)
        return;
    memcpy(copy, vt - 1, (VT_COPY / 4 + 1) * sizeof(void *));
    void **mine = copy + 1;
    mine[VT_MOVE / 4] = (void *)c_panel_nothing;
    mine[VT_CLICK / 4] = (void *)c_panel_nothing;
    mine[VT_LDOWN / 4] = (void *)ldown;
    mine[VT_LUP / 4] = (void *)lup;
    mine[VT_RCLICK / 4] = (void *)c_panel_nothing;
    mine[VT_RDOWN / 4] = (void *)rdown;
    mine[VT_RUP / 4] = (void *)c_panel_nothing;
    *(void ***)win = mine;
}

static void make_panel(char *sw)
{
    if (!g_panel_made) {
        ((fn_v)VA(VA_BUTTON_CTOR))(g_panel);
        g_panel_made = 1;
    }
    g_panel_ready = 0;
    int err = ((button_init_t)VA(VA_BUTTON_INIT))(g_panel, (void *)VA(VA_EMPTY_STRING), 0, 0, 0, 1, 1, sw, 0);
    if (err) {
        logf_("units: window init error %d", err);
        return;
    }
    own_vtable((char *)g_panel, g_panel_vt, (void (*)(void))c_panel_ldown, (void (*)(void))c_panel_nothing,
               (void (*)(void))c_panel_rdown);
    void **font = (void **)(g_panel + WIN_BUFFER + BUF_FONT);
    void **sw_font = *(void ***)(sw + SW_FONT);
    if (!*font && sw_font && *sw_font)
        *font = sw_font;
    ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
    g_panel_ready = 1;
}

/* ---- the button beside the Nukes slider ---------------------------------------------------- */

__attribute__((used, thiscall)) static void c_box_ldown(char *box, int x, int y)
{
    (void)x, (void)y;
    if (box == g_box)
        g_box_down = 1;
}

__attribute__((used, thiscall)) static void c_box_lup(char *box, int x, int y)
{
    (void)x, (void)y;
    if (box != g_box || !g_box_down)
        return;
    g_box_down = 0;
    click_sound();
    if (survival_setup_active()) { /* the Survival setup: the button chooses the hero */
        survival_hero_toggle();
        return;
    }
    panel_open();
}

static char *find_box(char *sw)
{
    char *rules = sw + SW_RULES;
    unsigned char str[64];
    game_string("unitbans", str);
    char *box = ((find_t)(*(void ***)rules)[VT_FIND / 4])(rules, str, 0);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    return box;
}

/* nonukes.c's c_nn_setup_checks, after the game's setup_checks */
void unitbans_setup(char *sw)
{
    g_sw = sw;
    g_box = NULL;
    g_box_down = 0;
    if (!g_unitbans_enabled)
        return;
    map_types();
    g_box = find_box(sw);
    if (!g_box) {
        static int said;
        if (!said++)
            logf_("units: no unitbans box in setupwin.xml (ui-theme not installed?)");
        return;
    }
    ((fn_v_i)VA(VA_SET_ID))(g_box, BOX_ID);
    void **font = *(void ***)(sw + SW_FONT);
    if (font) {
        if (*font)
            *(void **)(g_box + CB_FONT) = font;
        *(int *)(g_box + CB_FONT + 4) = 0;
        *(int *)(g_box + CB_FONT + 8) = 0;
    }
    char *buf = g_box + WIN_BUFFER;
    memcpy(buf + 0x90, (void *)VA(VA_WHITE), 12);
    memcpy(buf + 0xCC, (void *)VA(VA_COLOR_NONE), 12);
    buf[0x108] = 1;
    buf[0x10D] = 1;
    own_vtable(g_box, g_box_vt, (void (*)(void))c_box_ldown, (void (*)(void))c_box_lup,
               (void (*)(void))c_panel_nothing);
    ((fn_v_i)(*(void ***)g_box)[VT_SHOW / 4])(g_box, 0);
    make_panel(sw);
    if (!solo() && is_host())
        send_lobby();
    static int said;
    if (!said++)
        logf_("units: button in the game setup (%d units in the table)", UB_ENTRIES);
}

/* nonukes.c's c_nn_check_redraw: 1 when it drew the box */
int unitbans_check_redraw(char *sw, char *box, int id)
{
    (void)sw;
    if (!box || box != g_box || id != BOX_ID)
        return 0;
    if (survival_setup_active()) { /* "Hero: Infantry" / "Hero: Cavalry" (survival.c) */
        int hx = -1, hy = -1;
        ((mouse_t)VA(VA_GET_MOUSE_POS))(box, &hx, &hy);
        short *hr = (short *)(box + WIN_RECT);
        int hw = hr[1] - hr[0], hh = hr[3] - hr[2];
        short hall[4] = { 0, (short)hw, 0, (short)hh };
        int hover = inside(hx, hy, hall);
        fill(0, 1, hw, hh - 1, hover ? 0xF0384450u : 0xE0262E36u);
        frame(0, 1, hw, hh - 1, C_BORDER);
        char text[96];
        snprintf(text, sizeof text, "%s: %s", lang_text("Hero"),
                 lang_text(survival_hero_choice() ? "Cavalry" : "Infantry"));
        (*depth())++;
        unsigned char hs[64] = { 0 };
        lang_string(hs, text);
        put_string(box, 0, 0, hw, hh, hs, 12, 1, T_GOLD, 1);
        ((fn_v)VA(VA_STRING_CLOSE))(hs);
        (*depth())--;
        return 1;
    }
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(box, &mx, &my);
    short *r = (short *)(box + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2];
    short all[4] = { 0, (short)w, 0, (short)h };
    int hover = inside(mx, my, all) && !is_open();
    const unsigned char *l = shown_list();
    int n = count_off(l, NULL);
    fill(0, 1, w, h - 1, hover ? 0xF0384450u : 0xE0262E36u);
    frame(0, 1, w, h - 1, hover || n ? C_BORDER : 0xFF5A6470u);
    /* "Units: 12 off" / "Units: all allowed", or "Units (12)" / "Units" when that does not fit */
    char text[96], final[128];
    if (n)
        snprintf(text, sizeof text, "%s: %s", lang_text("Units"), lang_text("%d off"));
    else
        snprintf(text, sizeof text, "%s: %s", lang_text("Units"), lang_text("all allowed"));
    snprintf(final, sizeof final, text, n);
    if (text_width(box, final, 12, 1) > w - 8) {
        if (n)
            snprintf(final, sizeof final, "%s (%d)", lang_text("Units"), n);
        else
            snprintf(final, sizeof final, "%s", lang_text("Units"));
    }
    (*depth())++;
    unsigned char str[64] = { 0 };
    lang_string(str, final);
    put_string(box, 0, 0, w, h, str, 12, 1, n ? T_GOLD : T_WHITE, 1);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    (*depth())--;
    return 1;
}

/* nonukes.c's c_nn_setup_game, every machine, as the game starts */
void unitbans_hide_box(void) { survival_hide_box(g_box); }

void unitbans_setup_game(char *sw)
{
    if (!g_unitbans_enabled)
        return;
    g_sw = sw;
    int has_box = find_box(sw) != NULL;
    const unsigned char *l = NULL;
    if (*(int *)(sw + SW_MODE) & 2)
        l = has_box && !survival_setup_active() ? g_local : NULL; /* Survival: its button is hidden */
    else if (!g_orig_send_game)
        l = NULL; /* without the lobby hooks the other machines would not know the list */
    else if (is_host())
        l = g_local;
    else
        l = g_lobby_seen ? g_lobby : NULL;
    static const unsigned char none[UB_BYTES];
    memcpy(g_game, l ? l : none, UB_BYTES);
    normalize(g_game);
    unsigned *f = game_flags();
    if (f) {
        *f &= ~(1u << BIT_ON | 0xFFu << HASH_SHIFT);
        if (any(g_game))
            *f |= 1u << BIT_ON | hash8(g_game) << HASH_SHIFT;
    }
    g_checked = -1;
    panel_close();
    int total = 0, n = count_off(g_game, &total);
    if (n) {
        char text[2 * UB_BYTES + 16];
        to_text(g_game, text, sizeof text);
        logf_("units: the game starts with %d of %d units turned off (%s)", n, total, text);
    } else {
        logf_("units: the game starts with every unit");
    }
}

/* ---- the lobby ------------------------------------------------------------------------------ */


__attribute__((used, thiscall)) static void c_ub_send_game(void *cd)
{
    ((send_game_t)g_orig_send_game)(cd);
    char *net = *(char **)VA(VA_NETSYS);
    if (g_unitbans_enabled && net && is_host())
        send_lobby();
}

/* A std::wstring (MSVC: 16-byte buffer or pointer, size +0x10, capacity +0x14) into ASCII. */
static int wstring_ascii(const char *ws, char *out, int size)
{
    unsigned n = *(const unsigned *)(ws + 0x10), cap = *(const unsigned *)(ws + 0x14);
    const unsigned short *p = cap >= 8 ? *(const unsigned short *const *)ws : (const unsigned short *)ws;
    if (!p || n >= (unsigned)size)
        return 0;
    for (unsigned i = 0; i < n; i++)
        out[i] = p[i] < 128 ? (char)p[i] : '?';
    out[n] = 0;
    return 1;
}

/* the lobby's keys: a list of (wstring key, wstring value) at LobbyDTO + 0x6C */
__attribute__((used)) void c_ub_dto(const char *dto)
{
    if (!g_unitbans_enabled || !dto)
        return;
    const char *head = *(const char *const *)(dto + 0x6C);
    if (!head)
        return;
    int guard = 0;
    for (const char *node = *(const char *const *)head; node && node != head && guard < 512;
         node = *(const char *const *)node, guard++) {
        char key[32], value[2 * UB_BYTES + 16];
        if (!wstring_ascii(node + 8, key, sizeof key) || strcmp(key, LOBBY_KEY))
            continue;
        if (!wstring_ascii(node + 8 + 0x18, value, sizeof value))
            return;
        unsigned char l[UB_BYTES];
        if (!from_text(value, l)) {
            static int said;
            if (!said++)
                logf_("units: the host's list is of another unit table (another mod version?)");
            return;
        }
        if (!g_lobby_seen || memcmp(l, g_lobby, UB_BYTES)) {
            memcpy(g_lobby, l, UB_BYTES);
            logf_("units: the host's list from the lobby (%d off)", count_off(g_lobby, NULL));
        }
        g_lobby_seen = 1;
        return;
    }
}

/* a lobby joined: no list until the lobby gives one */
__attribute__((used, thiscall)) static void c_ub_join_init(void *cd, int a)
{
    g_lobby_seen = 0;
    memset(g_lobby, 0, UB_BYTES);
    ((cd_init_t)VA(VA_CD_INIT))(cd, a);
}

void hk_ub_dto(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_ub_dto\n_hk_ub_dto:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push edx\n"
        "    call _c_ub_dto\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_ub_dto_tramp]\n"
        ".att_syntax prefix\n");
void *g_ub_dto_tramp;

static int call_to(UINT_PTR at, UINT_PTR target)
{
    unsigned char *p = (unsigned char *)VA(at);
    return p[0] == 0xE8 && (UINT_PTR)(p + 5 + *(INT32 *)(p + 1)) == VA(target);
}

static void patch_call(UINT_PTR at, void *to)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(p + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(p + 5));
    VirtualProtect(p, 5, old, &old);
}

/* From nonukes_install, once its setup screen hooks are in. */
void unitbans_install(void)
{
    if (!g_unitbans_enabled)
        return;
    static const unsigned char send_sig[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    static const unsigned char dto_sig[6] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
    unsigned char *send = (unsigned char *)VA(VA_SEND_GAME), *dto = (unsigned char *)VA(VA_LOBBY_DTO);
    if (*(UINT_PTR *)VA(VT_BUTTON_REDRAW) != VA(VA_ON_BUTTON_REDRAW)) {
        logf_("units: setup screen code differs, the Units window is off");
        g_unitbans_enabled = 0;
        return;
    }
    void **slot = (void **)VA(VT_BUTTON_REDRAW);
    DWORD old;
    VirtualProtect(slot, 4, PAGE_READWRITE, &old);
    *slot = (void *)c_ub_button_redraw;
    VirtualProtect(slot, 4, old, &old);
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    int lobby = mem && !memcmp(send, send_sig, 9) && !memcmp(dto, dto_sig, 6) &&
                call_to(VA_CALL_JOIN_INIT, VA_CD_INIT);
    if (lobby) {
        g_orig_send_game = hook_trampoline(send, 9, mem);
        g_ub_dto_tramp = hook_trampoline(dto, 10, mem + 32); /* push ebp / mov ebp,esp / push -1 / push imm32 */
        hook_jmp5(send, (void *)c_ub_send_game);
        hook_jmp5(dto, (void *)hk_ub_dto);
        patch_call(VA_CALL_JOIN_INIT, (void *)c_ub_join_init);
    } else {
        logf_("units: lobby code differs, units are turned off in single player games only");
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("units: Units window in the game setup (%d units, multiplayer %s)", UB_ENTRIES, lobby ? "yes" : "no");
}
