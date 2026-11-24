/*
 * The Game Browser's game list (the multiplayer lobby, interface.xml GAMESPYGROUP "sessions"),
 * laid out like a modern lobby list (the user, 2026-11-24, after Age of Empires IV's):
 *  - one line per game: the room name and its host side by side (the host had a second line,
 *    "Host: name", under the room name; team style, map, size, rules and ELO sat on that second
 *    line too), every column left-aligned and the row's text vertically centred;
 *  - a larger font (Arial 16 instead of 12; ui-theme's fonts.xml adds it) and rows one line tall;
 *  - a thin grey line under each game that fades out toward both ends; the selected game a
 *    steel-blue band with white text;
 *  - the column titles plain soft gold labels on the panel's grain (no plate), each starting
 *    where its column's text starts; the sorted column's title a lighter gold (no underline);
 *    a "Host" title over the host column. Clicking a title still sorts.
 *
 * GameSpyGroup::on_listbox_entry_redraw (0x0096D4B0) draws every row of the list in code:
 * players "n/max", the room name, "Host: " + Lobby::GetHostName one line lower, then team
 * style, map style, map size, game rules and ELO, each with Buffer::write / write_cent into a
 * column rect of the sessionwin window (5 in from both sides, team style and later one line
 * down; the room name's rect 2.5 times as wide), the selected row first filled with
 * GraphicWin::fill. Here:
 *  - the function itself (g_hooks "lobby_row") notes the row's rect and whether it is selected,
 *    runs the game's drawing, then draws the line under the row;
 *  - its eight text calls go to c_ll_write: the same text, left-aligned in the column (the room
 *    name's rect back to its column, its left SPLIT_PCT percent; the host, without "Host: ", in
 *    the rest), centred in the whole row's height (Buffer::write_l centres in its rect) and in
 *    this module's colours, with the buffer's clip set to that rect;
 *  - its fill of the selected row gets the band's colour; the padlock of a game with a password
 *    (IFaceRenderManager::add) is centred in the row's height;
 *  - GameSpyGroup::pre_modal's FontFamily::get(12, any) for the list is FontFamily::get(16,
 *    regular), and its row height font height * 2 + 10 is font height + ROW_PAD.
 * The column titles are BUTTON "sort" drawn by GameSpyChatBase::on_button_redraw (a plate and
 * centred text); GameSpyGroup::on_button_redraw's call of it (0x0096EBCC) comes here first: a
 * "sort" button gets the panel's grain (a window draws no background under its child windows),
 * then its title left-aligned. ui-theme gives the title buttons the column rects (same per mille
 * widths, a POSITIONING 25 to the right: the list box at 19, its rows 1 in, the text 5 in), so a
 * title's left edge is its column's text. The Description title covers the room name and host
 * columns; the "Host" label is drawn at the same split.
 * The chat's player list (GameSpyGroup::on_listbox_entry_custom_redraw_player_list, called from
 * the same row function) is drawn here in the same way: the font, rows one line tall, the band,
 * the lines; the player details keep the game's drawing in the same font.
 * Interface only, client-side: nothing here reaches the simulation or other players.
 */
#include "ron.h"
#include <string.h>
#include <wchar.h>

#define VA_ROW_FN        0x0096D4B0u /* GameSpyGroup::on_listbox_entry_redraw(ListBox2 *, const Rect *, ListBoxEntry *) */
#define VA_WRITE         0x0041AFD0u /* Buffer::write(const String &, const Rect *) */
#define VA_WRITE_CENT_S  0x0041AF90u /* Buffer::write_cent(const String &, const Rect *) */
#define VA_WRITE_L       0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len): centred in the rect's height */
#define VA_FILL          0x0051A520u /* GraphicWin::fill(const Rect *, const Color &, float in xmm3) */
#define VA_RM_ADD_XY     0x0051F440u /* IFaceRenderManager::add(IFaceTex *, x, y, color, type) */
#define VA_RM_ADD_RECT   0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const Rect *, 0xAABBGGRR, type) */
#define VA_TILE_HORIZ    0x0051F1D0u /* IFaceRenderManager::tile_horiz(IFaceTex *, x0, x1, y, h, color, type) */
#define VA_IFACE_RM      0x00EBD1C0u
#define VA_SET_TEXT_COLOR 0x0050DAF0u /* Buffer::set_text_color(const Color &, const Color &, int, int) */
#define VA_TEXT_WIDTH    0x0050EF70u /* Buffer::text_width(const String &, len) */
#define VA_STR_STR       0x00A1F0A0u /* String::str() */
#define VA_STR_WCTOR     0x00A1EDD0u /* String::String(const wchar_t *) */
#define VA_STRING_CTOR   0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_HAS_FOCUS     0x00A54710u /* Window::has_focus() */
#define VA_COLOR_NONE    0x00EBEA04u
#define VA_WHITE         0x00C8D26Cu
#define VA_SETUPWIN      0x00CAB3A0u /* SetupWin::current: the game's setup screen over the lobby */
#define VA_TRANSLATED    0x00C8CD00u /* the shown strings; + 0x7440 "Host: " */
#define TR_HOST_PREFIX   0x7440
#define VA_CHAT_REDRAW   0x00967590u /* GameSpyChatBase::on_button_redraw(IFaceButton *, const String &) */
#define VA_REDRAW_CALL   0x0096EBCCu /* GameSpyGroup::on_button_redraw: call GameSpyChatBase::on_button_redraw */
#define VA_FONT_PUSH     0x0096FC9Fu /* GameSpyGroup::pre_modal: push -1 / push 12 (FontFamily::get of the list) */
#define VA_ROW_HEIGHT    0x0096FCFFu /* ...: lea ecx, [eax*2 + 10] (the list's row height from the font height) */
#define VA_FILL_CALL     0x0096D6A2u /* the row function: call GraphicWin::fill (the selected row) */
#define VA_PLAYER_CALL   0x0096D575u /* the row function: call ..._custom_redraw_player_list ("players") */
#define VA_PLAYER_ROW    0x0096BE70u /* GameSpyGroup::on_listbox_entry_custom_redraw_player_list */
#define VA_DETAILS_CALL  0x0096D551u /* the row function: call ..._custom_redraw_player_details */
#define VA_DETAILS_ROW   0x0096BCE0u /* GameSpyGroup::on_listbox_entry_custom_redraw_player_details */
#define VA_WRITE_RIGHT_L 0x0050FA40u /* Buffer::write_right_l(const String &, const Rect *, len): the text's left x */
#define VA_LINE_HEIGHT   0x0050F370u /* Buffer::text_line_height() */
#define VA_FONTS         0x00ED8160u /* FontFamily */
#define VA_FONT_GET      0x00522810u /* FontFamily::get(size, style) */
#define VA_LIGHT_BLUE    0x00C8D200u
#define VA_GREEN         0x00C8D1D0u
#define VA_ICON_CALL     0x0096D77Cu /* ...: call IFaceRenderManager::add (the password padlock) */

#define LB_SELECTED   0x278 /* ListBox2: the selected entry */
#define LB_FONT       0x1D0 /* ListBox2: its Buffer's font (Buffer +0x7C), 12 bytes */
#define LB_ROW_H      0x298 /* ListBox2: row height */
#define LB_FLAGS      0x274 /* ListBox2: bit 0 = lay out again (ListBox2::recalc) */
#define VT_RESET_CLIP 0xC0
#define WIN_ID        0x0C
#define WIN_BUFFER    0x154
#define WIN_SURFACE   0x184 /* its vtable +0x4c sets the clip, +0xcc the window's own rect */
#define BUF_CLIP_OBJ  0x30
#define BUF_COLORS    0x90
#define BUF_COLORS_N  0x80
#define BTN_SORTED    0x2B0 /* set by GameSpyGroup::on_button_redraw for the sorted column */
#define BTN_TITLE     0x2C4 /* String; its length at +8 */
#define BTN_STATE     0x318 /* bit 0 pressed, bit 2 mouse over */
#define VT_FIND_TEX   0xA4
#define VT_SET_CLIP   0x4C
#define VT_GET_RECT   0xCC
#define RENDER_SOLID  5 /* IFaceRenderType: untextured rectangle */
#define RENDER_PLATE  4 /* the lobby plates' type (draw_expander in on_button_redraw): colour x2 */
#define VA_GRAPHSYS   0x00E335A0u
#define GS_DEPTH      0x164 /* GraphSys update depth: +2 per window, the z of what is added */

#define SPLIT_PCT 60 /* the room name's share of the Description column; the host has the rest */
#define NAME_GAP  10
#define ROW_PAD   16 /* row height = font height + this */
#define SEGMENTS  24 /* the line under a row, in pieces fading toward the ends */
#define SEP_ALPHA 200     /* the line's alpha in its middle */
#define PLAYER_PAD 10     /* player list rows: font height + this */
#define PLAYER_INDENT 4   /* the names a little in from the list's edge */
#define DETAILS_PAD 6     /* player details rows */

typedef struct {
    unsigned char r, g, b, a;
    unsigned short c555, c565;
    unsigned flags;
} color_t;

int g_lobby_list_enabled = 1;
void *g_orig_lobby_row;
static int g_ready;     /* every call site checked and ours */
static int g_font_size; /* the list's font size */

static struct {
    char *lb;
    const short *rect;
    int sel, active, drawn;
} g_row;

static color_t g_name, g_text, g_name_sel, g_text_sel, g_gold, g_gold_hi, g_gold_sorted;
static unsigned g_sep[SEGMENTS]; /* 0xAABBGGRR: grey, fading to nothing at the ends */
static void *g_font;             /* FontFamily::get(g_font_size, regular), once the fonts are loaded */
__attribute__((used)) color_t g_ll_band;
__attribute__((used)) void *g_ll_fill_fn;
__attribute__((used)) float g_ll_minus_one = -1.0f;

static const UINT_PTR k_write_site[8] = { 0x0096D858u, 0x0096D923u, 0x0096D9BFu, 0x0096DAA9u,
                                          0x0096DB7Bu, 0x0096DC7Eu, 0x0096DD5Au, 0x0096DE36u };
/* players, room name, host, team style, map style, map size, game rules, ELO */
static const UINT_PTR k_write_target[8] = { VA_WRITE, VA_WRITE, VA_WRITE, VA_WRITE_CENT_S,
                                            VA_WRITE, VA_WRITE_CENT_S, VA_WRITE_CENT_S, VA_WRITE_CENT_S };
enum { S_PLAYERS, S_NAME, S_HOST };

typedef int(__attribute__((thiscall)) *write2_t)(void *buf, void *str, const short *rect);
typedef int(__attribute__((thiscall)) *writel_t)(void *buf, void *str, const short *rect, int len);
typedef int(__attribute__((thiscall)) *width_t)(void *buf, void *str, int len);
typedef void(__attribute__((thiscall)) *color_fn_t)(void *buf, const void *fg, const void *bg, int a, int b);
typedef const wchar_t *(__attribute__((thiscall)) *str_t)(void *s);
typedef void(__attribute__((thiscall)) *wctor_t)(void *s, const wchar_t *w);
typedef void(__attribute__((thiscall)) *ctor_t)(void *s, const char *c);
typedef void(__attribute__((thiscall)) *close_t)(void *s);
typedef void(__attribute__((thiscall)) *set_clip_t)(void *obj, const short *rect);
typedef void(__attribute__((thiscall)) *get_rect_t)(void *obj, short *rect);
typedef void *(__attribute__((thiscall)) *find_tex_t)(void *win, const void *name, int id, int *iter);
typedef void(__attribute__((thiscall)) *add_xy_t)(void *rm, const void *tex, int x, int y, unsigned color, int type);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *rm, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef void(__attribute__((thiscall)) *tile_t)(void *rm, const void *tex, int x0, int x1, int y, int h,
                                                 unsigned color, int type);
typedef int(__attribute__((thiscall)) *redraw_t)(void *self, void *btn, void *name);
typedef int(__attribute__((thiscall)) *row_fn_t)(void *self, char *lb, const short *rect, void *entry);
typedef int(__attribute__((thiscall)) *fn_i_t)(void *self);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);

void ll_fill_call(char *lb, const short *rect, const color_t *c); /* asm below: xmm3 = -1 */

void lobby_list_load_config(const char *ini_path)
{
    g_lobby_list_enabled = GetPrivateProfileIntA("ui", "lobby_list", 1, ini_path) != 0;
    g_font_size = GetPrivateProfileIntA("ui", "lobby_list_font", 16, ini_path);
    if (g_font_size < 10 || g_font_size > 24)
        g_font_size = 16;
}

static void make_color(color_t *c, int r, int g, int b)
{
    c->r = (unsigned char)r;
    c->g = (unsigned char)g;
    c->b = (unsigned char)b;
    c->a = 0xFF;
    c->c555 = (unsigned short)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    c->c565 = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    c->flags = ((const color_t *)VA(VA_WHITE))->flags;
}

static int split_of(int column_width)
{
    return column_width * SPLIT_PCT / 100;
}

static void set_clip(char *buf, const short *rect)
{
    void *obj = *(void **)(buf + BUF_CLIP_OBJ);
    if (obj)
        ((set_clip_t)(*(void ***)obj)[VT_SET_CLIP / 4])(obj, rect);
}

/* ---- the rows ------------------------------------------------------------------------------ */

/* In place of the row function's eight Buffer::write / write_cent calls (ecx = the list's
   buffer); `site` = which one (asm stubs below). */
__attribute__((used)) int c_ll_write(char *buf, void *str, short *rect, int site)
{
    if (site < 0 || site > 7)
        return 0;
    if (!g_row.active || !g_row.rect || !rect || !str || !buf)
        return ((write2_t)VA(k_write_target[site]))(buf, str, rect);
    int len = *(unsigned short *)((char *)str + 8);
    if (len <= 0)
        return 0;
    const short *e = g_row.rect;
    int l = rect[0], r = rect[1];
    void *s = str;
    int n = len, own = 0;
    unsigned char tmp_str[0x14] __attribute__((aligned(4))) = { 0 };
    static wchar_t tmp[128];
    if (site == S_NAME || site == S_HOST) {
        /* the game's rects: the column 5 in on both sides, then 2.5 times as wide; the host's
           10 further right */
        int origin = site == S_NAME ? rect[0] : rect[0] - 10;
        int inner = (rect[1] - rect[0]) * 2 / 5;
        int split = split_of(inner + 10);
        if (site == S_NAME) {
            l = origin;
            r = origin + split - NAME_GAP;
        } else {
            l = origin + split;
            r = origin + inner;
            /* "Host: " + the name -> the name */
            char *tr = *(char **)VA(VA_TRANSLATED);
            const wchar_t *w = ((str_t)VA(VA_STR_STR))(str);
            if (tr && w) {
                void *prefix = tr + TR_HOST_PREFIX;
                int pn = *(unsigned short *)((char *)prefix + 8);
                const wchar_t *p = pn > 0 ? ((str_t)VA(VA_STR_STR))(prefix) : NULL;
                int skip = p && pn < len && !wcsncmp(w, p, pn) ? pn : 0;
                while (skip < len && w[skip] == L' ')
                    skip++;
                if (skip > 0 && skip < len) {
                    n = len - skip;
                    if (n > 127)
                        n = 127;
                    memcpy(tmp, w + skip, n * sizeof(wchar_t));
                    tmp[n] = 0;
                    ((wctor_t)VA(VA_STR_WCTOR))(tmp_str, tmp);
                    s = tmp_str;
                    own = 1;
                }
            }
        }
    }
    if (r > e[1])
        r = e[1];
    if (r <= l)
        return 0;
    short cell[4] = { (short)l, (short)r, e[2], e[3] };
    set_clip(buf, cell);
    const color_t *c = site == S_NAME ? (g_row.sel ? &g_name_sel : &g_name) : (g_row.sel ? &g_text_sel : &g_text);
    ((color_fn_t)VA(VA_SET_TEXT_COLOR))(buf, c, (void *)VA(VA_COLOR_NONE), 1, 1);
    int ret = ((writel_t)VA(VA_WRITE_L))(buf, s, cell, n);
    if (own)
        ((close_t)VA(VA_STRING_CLOSE))(tmp_str);
    g_row.drawn = 1;
    return ret;
}

/* the password padlock: centred in the row's height */
__attribute__((used, thiscall)) void c_ll_icon(void *rm, void *tex, int x, int y, unsigned color, int type)
{
    if (g_row.active && g_row.rect && tex) {
        const short *t = (const short *)tex; /* +8 top, +0xA bottom of the picture */
        int th = t[5] - t[4];
        if (th > 0 && th < g_row.rect[3] - g_row.rect[2])
            y = g_row.rect[2] + (g_row.rect[3] - g_row.rect[2] - th) / 2;
    }
    ((add_xy_t)VA(VA_RM_ADD_XY))(rm, tex, x, y, color, type);
}


/* The line under a row: 1 px solid rectangles of the render manager in grey, their alpha fading
   to nothing toward both ends, so the line lies on the panel whatever shows through it (a fill
   of the window's surface would be opaque). Nothing textured of the list window lies under it. */
static void draw_separator(char *group, const short *e)
{
    (void)group;
    int l = e[0] + 4, r = e[1] - 4, y = e[3] - 1;
    if (r - l < 2 * SEGMENTS)
        return;
    for (int i = 0; i < SEGMENTS; i++) {
        short seg[4] = { (short)(l + (r - l) * i / SEGMENTS), (short)(l + (r - l) * (i + 1) / SEGMENTS), (short)y,
                         (short)(y + 1) };
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, seg, g_sep[i], RENDER_SOLID);
    }
}

/* the list's font: regular, g_font_size; its rows the font's height + pad (the list lays itself
   out again on its next redraw) */
static void set_list_font(char *lb, int pad)
{
    if (!g_font)
        g_font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), g_font_size, 0);
    if (!g_font || !*(void **)g_font)
        return;
    if (*(void **)(lb + LB_FONT) != g_font) {
        *(void **)(lb + LB_FONT) = g_font;
        *(int *)(lb + LB_FONT + 4) = 0;
        *(int *)(lb + LB_FONT + 8) = 0;
    }
    int h = ((fn_i_t)VA(VA_LINE_HEIGHT))(lb + WIN_BUFFER) + pad;
    if (h > 12 && h < 80 && *(int *)(lb + LB_ROW_H) != h) {
        *(int *)(lb + LB_ROW_H) = h;
        lb[LB_FLAGS] |= 1; /* lay out again: entry positions and the scroll bar */
    }
}

/* In place of GameSpyGroup::on_listbox_entry_custom_redraw_player_list (the lobby's player
   list, right of the chat): the same, with the game list's band, font, rows and lines. The game
   writes the entry's right text (+0x1C) right-aligned, then its name (+8, icons inline) in the
   rest, in light blue (entry state 0), green (1) or white; the selected row on a light band in
   black. */
__attribute__((used, thiscall)) int c_ll_player_row(char *self, char *lb, const short *rect, int *entry)
{
    if (!lb || !rect || !entry)
        return ((row_fn_t)VA(VA_PLAYER_ROW))(self, lb, rect, entry);
    set_list_font(lb, PLAYER_PAD);
    char *buf = lb + WIN_BUFFER;
    int sel = *(int **)(lb + LB_SELECTED) == entry;
    const void *c = sel ? (const void *)&g_name_sel
                  : entry[0] == 0 ? (const void *)VA(VA_LIGHT_BLUE)
                  : entry[0] == 1 ? (const void *)VA(VA_GREEN) : (const void *)&g_name;
    if (sel)
        ll_fill_call(lb, rect, &g_ll_band);
    ((color_fn_t)VA(VA_SET_TEXT_COLOR))(buf, c, (void *)VA(VA_COLOR_NONE), 1, 1);
    short r[4] = { (short)(rect[0] + PLAYER_INDENT), rect[1], rect[2], rect[3] };
    char *e = (char *)entry;
    int rlen = *(unsigned short *)(e + 0x1C + 8);
    if (rlen) {
        int x = ((writel_t)VA(VA_WRITE_RIGHT_L))(buf, e + 0x1C, r, rlen);
        if (x - 10 > r[0])
            r[1] = (short)(x - 10);
    }
    int len = *(unsigned short *)(e + 8 + 8);
    if (len) {
        set_clip(buf, r);
        ((writel_t)VA(VA_WRITE_L))(buf, e + 8, r, len);
        void *obj = *(void **)(buf + BUF_CLIP_OBJ);
        if (obj)
            ((fn_i_t)(*(void ***)obj)[VT_RESET_CLIP / 4])(obj);
    }
    if (!sel)
        draw_separator(self, rect);
    return 1;
}

/* The player details (Played, Wins, Losses...) keep the game's own font and drawing: in the list's
   larger font their rows no longer fit the panel and a scroll bar came up (2026-11-24). */

/* GameSpyGroup::on_listbox_entry_redraw, from its first instruction (g_hooks "lobby_row") */
__attribute__((thiscall)) int hk_lobby_row(void *self, char *lb, const short *rect, void *entry)
{
    row_fn_t orig = (row_fn_t)g_orig_lobby_row;
    if (!g_ready || !lb || !rect)
        return orig(self, lb, rect, entry);
    __typeof__(g_row) saved = g_row;
    g_row.lb = lb;
    g_row.rect = rect;
    g_row.active = 1;
    g_row.drawn = 0;
    g_row.sel = *(void **)(lb + LB_SELECTED) == entry && !*(void **)VA(VA_SETUPWIN);
    int r = orig(self, lb, rect, entry);
    if (g_row.drawn && !g_row.sel)
        draw_separator(self, rect);
    g_row = saved;
    return r;
}

/* ---- the column titles --------------------------------------------------------------------- */

static void *find_tex(char *win, const char *name)
{
    unsigned char s[64] __attribute__((aligned(4))) = { 0 };
    ((ctor_t)VA(VA_STRING_CTOR))(s, name);
    void *t = ((find_tex_t)(*(void ***)win)[VT_FIND_TEX / 4])(win, s, -1, NULL);
    ((close_t)VA(VA_STRING_CLOSE))(s);
    return t;
}

static void header_text(char *buf, void *str, int len, int l, int r, int t, int b, const color_t *c)
{
    if (len <= 0 || r <= l)
        return;
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((color_fn_t)VA(VA_SET_TEXT_COLOR))(buf, c, (void *)VA(VA_COLOR_NONE), 1, 1);
    ((writel_t)VA(VA_WRITE_L))(buf, str, rect, len);
}

static int is_sort(void *name)
{
    const wchar_t *w = ((str_t)VA(VA_STR_STR))(name);
    return w && *(unsigned short *)((char *)name + 8) == 4 && !wcsncmp(w, L"sort", 4);
}

static int draw_header(char *group, char *btn)
{
    void *surf = *(void **)(btn + WIN_SURFACE);
    if (!surf)
        return 0;
    short R[4] = { 0, 0, 0, 0 };
    ((get_rect_t)(*(void ***)surf)[VT_GET_RECT / 4])(surf, R);
    int w = R[1] - R[0], h = R[3] - R[2];
    if (w <= 4 || h <= 4)
        return 0;
    void *rm = (void *)VA(VA_IFACE_RM);
    /* the panel's grain under the title (the window around it draws none under its children):
       a strip of it in the atlas of the plates (menu_parts.tga), drawn as the plates were (type
       4, a 2x modulate: 0x80 leaves the texture as it is), so the text comes out over it */
    void *tex = find_tex(group, "list_grain");
    if (tex) {
        short t[8];
        memcpy(t, tex, sizeof t);
        if (t[5] - t[4] > h)
            t[5] = (short)(t[4] + h);
        ((tile_t)VA(VA_TILE_HORIZ))(rm, t, R[0], R[1], R[2], h, 0xFF808080u, RENDER_PLATE);
    } /* (without it nothing: the panel may be see-through, an opaque fill would show) */
    char *buf = btn + WIN_BUFFER;
    unsigned char saved[BUF_COLORS_N];
    memcpy(saved, buf + BUF_COLORS, BUF_COLORS_N);
    int id = *(int *)(btn + WIN_ID);
    int sorted = *(int *)(btn + BTN_SORTED) != 0;
    int hover = (btn[BTN_STATE] & 5) || ((fn_i_t)VA(VA_HAS_FOCUS))(btn);
    void *title = btn + BTN_TITLE;
    int len = *(unsigned short *)(btn + BTN_TITLE + 8);
    int split = id == 0 && w >= 200 ? split_of(w) : 0; /* Description: room name | host */
    int right = split ? split - NAME_GAP : R[1];
    const color_t *c = sorted ? &g_gold_sorted : hover ? &g_gold_hi : &g_gold;
    header_text(buf, title, len, R[0], right, R[2], R[3], c);
    /* (the sorted column's title had an orange underline; removed 2026-11-24 at the user's
       request: only its lighter gold marks it now) */
    if (split) {
        unsigned char s[64] __attribute__((aligned(4))) = { 0 };
        lang_string(s, lang_text("Host"));
        header_text(buf, s, *(unsigned short *)(s + 8), R[0] + split, R[1], R[2], R[3], &g_gold);
        ((close_t)VA(VA_STRING_CLOSE))(s);
    }
    memcpy(buf + BUF_COLORS, saved, BUF_COLORS_N);
    return 1;
}

/* in place of GameSpyGroup::on_button_redraw's call of GameSpyChatBase::on_button_redraw */
__attribute__((used, thiscall)) int c_ll_button_redraw(char *self, char *btn, void *name)
{
    if (g_ready && self && btn && name && is_sort(name) && draw_header(self, btn))
        return 1;
    return ((redraw_t)VA(VA_CHAT_REDRAW))(self, btn, name);
}

/* ---- the stubs ----------------------------------------------------------------------------- */

void hk_ll_w0(void), hk_ll_w1(void), hk_ll_w2(void), hk_ll_w3(void), hk_ll_w4(void), hk_ll_w5(void),
    hk_ll_w6(void), hk_ll_w7(void), hk_ll_fill(void);
static void (*const k_write_stub[8])(void) = { hk_ll_w0, hk_ll_w1, hk_ll_w2, hk_ll_w3,
                                               hk_ll_w4, hk_ll_w5, hk_ll_w6, hk_ll_w7 };

#define WSTUB(n) ".globl _hk_ll_w" #n "\n_hk_ll_w" #n ":\n    mov eax, " #n "\n    jmp ll_write_common\n"
__asm__(".intel_syntax noprefix\n"
        WSTUB(0) WSTUB(1) WSTUB(2) WSTUB(3) WSTUB(4) WSTUB(5) WSTUB(6) WSTUB(7)
        /* thiscall write(str, rect), ecx = the buffer -> c_ll_write(buf, str, rect, site) */
        "ll_write_common:\n"
        "    push eax\n"
        "    push dword ptr [esp+12]\n" /* rect */
        "    push dword ptr [esp+12]\n" /* str */
        "    push ecx\n"
        "    call _c_ll_write\n"
        "    add esp, 16\n"
        "    ret 8\n"

        /* GraphicWin::fill(rect, color) of the selected row: the band's colour (xmm3 kept) */
        ".globl _hk_ll_fill\n_hk_ll_fill:\n"
        "    mov dword ptr [esp+8], offset _g_ll_band\n"
        "    jmp dword ptr [_g_ll_fill_fn]\n"

        /* ll_fill_call(lb, rect, color): GraphicWin::fill with xmm3 = -1.0 as the game passes */
        ".globl _ll_fill_call\n_ll_fill_call:\n"
        "    mov ecx, [esp+4]\n"
        "    movss xmm3, dword ptr [_g_ll_minus_one]\n"
        "    push dword ptr [esp+12]\n" /* color */
        "    push dword ptr [esp+12]\n" /* rect */
        "    call dword ptr [_g_ll_fill_fn]\n"
        "    ret\n"
        ".att_syntax prefix\n");

/* ---- install ------------------------------------------------------------------------------- */

static int call_to(UINT_PTR at, UINT_PTR target)
{
    unsigned char *p = (unsigned char *)VA(at);
    return p[0] == 0xE8 && (UINT_PTR)(p + 5 + *(INT32 *)(p + 1)) == VA(target);
}

static void write_call(UINT_PTR at, const void *to)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE8;
    *(INT32 *)(p + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(p + 5));
    VirtualProtect(p, 5, old, &old);
}

static void write_bytes(UINT_PTR at, const unsigned char *b, int n)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p, b, n);
    VirtualProtect(p, n, old, &old);
}

void lobby_list_install(void)
{
    if (!g_lobby_list_enabled || !g_orig_lobby_row)
        return; /* off, or the row function's signature did not match (logged by the hook table) */
    static const unsigned char font_sig[4] = { 0x6A, 0xFF, 0x6A, 0x0C };
    static const unsigned char height_sig[7] = { 0x8D, 0x0C, 0x45, 0x0A, 0x00, 0x00, 0x00 };
    int ok = !memcmp((void *)VA(VA_FONT_PUSH), font_sig, 4) && !memcmp((void *)VA(VA_ROW_HEIGHT), height_sig, 7) &&
             call_to(VA_FILL_CALL, VA_FILL) && call_to(VA_ICON_CALL, VA_RM_ADD_XY) &&
             call_to(VA_REDRAW_CALL, VA_CHAT_REDRAW) && call_to(VA_PLAYER_CALL, VA_PLAYER_ROW);
    for (int i = 0; i < 8; i++)
        ok = ok && call_to(k_write_site[i], k_write_target[i]);
    if (!ok) {
        logf_("lobby list: signature mismatch, the Game Browser's list left as it is");
        return;
    }
    make_color(&g_name, 236, 238, 242);
    make_color(&g_text, 172, 180, 192);
    make_color(&g_name_sel, 255, 255, 255);
    make_color(&g_text_sel, 226, 232, 242);
    make_color(&g_gold, 200, 172, 116);
    make_color(&g_gold_hi, 228, 202, 148);
    make_color(&g_gold_sorted, 244, 216, 160);
    make_color(&g_ll_band, 40, 70, 116);
    /* the line under a row: grey in the middle, the panel's colour (27, 31, 38) at the ends */
    for (int i = 0; i < SEGMENTS; i++) {
        float t = (i + 0.5f) / SEGMENTS, d = t < 0.5f ? 1 - 2 * t : 2 * t - 1;
        float k = 1 - d * d * d;
        g_sep[i] = ((unsigned)(SEP_ALPHA * k) << 24) | (112u << 16) | (100u << 8) | 92u; /* (92, 100, 112) */
    }
    g_ll_fill_fn = (void *)VA(VA_FILL);
    for (int i = 0; i < 8; i++)
        write_call(k_write_site[i], k_write_stub[i]);
    write_call(VA_FILL_CALL, hk_ll_fill);
    write_call(VA_ICON_CALL, c_ll_icon);
    write_call(VA_REDRAW_CALL, c_ll_button_redraw);
    write_call(VA_PLAYER_CALL, c_ll_player_row);
    /* the list's font: FontFamily::get(size, regular); its rows one line tall */
    const unsigned char font[4] = { 0x6A, 0x00, 0x6A, (unsigned char)g_font_size };
    const unsigned char height[7] = { 0x8D, 0x48, ROW_PAD, 0x90, 0x90, 0x90, 0x90 }; /* lea ecx, [eax + ROW_PAD] */
    write_bytes(VA_FONT_PUSH, font, 4);
    write_bytes(VA_ROW_HEIGHT, height, 7);
    g_ready = 1;
    logf_("lobby list: one line per game, a Host column, font %d, title labels", g_font_size);
}
