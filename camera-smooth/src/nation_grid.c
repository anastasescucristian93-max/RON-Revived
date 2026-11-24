/*
 * Nation picker as a grid of flag cards in the game setup screen. The open list of a
 * nation combo (nation0..7) becomes a panel: the other choices (Random, Offensive,
 * Defensive, Economic, X-Pack) as a column of badge cards on the left, the nations as a grid
 * of round flags on the right, without names. Hovering a card lights it with a pulsing gold
 * glow and shows a tooltip beside the panel in SetupWin's own HelpBox: the nation's name and
 * unique units (help.xml TRIBETIP<n>) above the game's power text (TRIBE<n>).
 *
 * The list is the combo's own PullDown, so selecting, closing and the keyboard stay the
 * game's. Only its geometry changes:
 * - PullDown::show sizes the window from the rows: its GraphicWin::resize call gets the
 *   panel's size instead, and the scroll state is reset (no scrolling in the panel);
 * - the hovered item (+0x298) is computed from x and y: PullDown::mouse_move (called by
 *   on_mouse_move) and the virtual on_left_up, which recomputes it before selecting;
 * - the virtual on_redraw draws the panel;
 * - the tooltip is set from the grid's own mouse_move (show_tip); the two HelpBox::set calls
 *   of SetupWin::on_pulldown_change are left out while the grid is open (the game anchors
 *   its tooltip to the combo, under the panel, and it never came up for the grid).
 *
 * The flags are flags.c's ("flag_<tribe>" in setupwin.xml); without them the list stays
 * a list. Only the interface is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VA_PD_VTABLE     0x00B65678u /* PullDown vtable */
#define VA_PD_REDRAW     0x00529570u /* PullDown::on_redraw, vtable +0xB8 */
#define VA_PD_LEFT_UP    0x00528EE0u /* PullDown::on_left_up(x, y), vtable +0xE0 */
#define VA_PD_ON_MOVE    0x0052A0E0u /* PullDown::on_mouse_move(x, y), vtable +0xD0 */
#define VA_PD_MOUSE_MOVE 0x00529060u /* PullDown::mouse_move(x, y), called at 0x0052A17D */
#define VA_PD_ITEM_H     0x0052A800u /* PullDown::get_item_height() */
#define VA_RESIZE        0x0051A550u /* GraphicWin::resize(w, h, PixFormat, int), called at 0x00528B66 */
#define VA_HELP_SET      0x007B9840u /* HelpBox::set(String &key, Rect *anchor, String &key2, int, int) */
#define VA_HELP_CLEAR    0x007B97C0u /* HelpBox::clear() */
#define VA_SETUPWIN      0x00CAB3A0u /* SetupWin::current */
#define VA_TRIBE_PARSE   0x006F0560u /* Tribe::parse() const: the numbers of the power text */
#define VA_TRIBES_DATA   0x00E7FA34u /* Tribes' entries, TRIBE_SIZE bytes each */
#define VA_STRING_CTOR   0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_TO_SCREEN     0x00A53FE0u /* Window::client_to_screen(Rect *) */
#define VA_DEF_FONT      0x00E335E4u /* Font::def_font */
#define VA_IFACE_RM      0x00EBD1C0u
#define VA_RM_ADD_RECT   0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const Rect *, color, type) */
#define VA_WRITE_CENT    0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_TEXT_WIDTH    0x0050EF40u /* Buffer::text_width(const String &) */
#define VA_SET_TEXT_COLOR 0x0050DAF0u
#define VA_GRAPHSYS      0x00E335A0u /* GraphSys *: vtable +0x38 = get_mouse(int *x, int *y) */
#define VT_GET_MOUSE     0x38
#define VA_WHITE         0x00C8D26Cu
#define VA_BLACK         0x00C8D260u

#define VT_REDRAW  0xB8
#define VT_LEFT_UP 0xE0
#define VT_MOUSE_MOVE 0xD0
#define VT_PULLDOWN_CHANGE 0x334 /* Window::on_pulldown_change(PullDown *) */
#define WIN_PARENT 0x74
#define VT_HIDE    0x1C

#define WIN_FLAGS      0x08  /* bit 0: visible */
#define SW_HELPBOX     0x2274 /* SetupWin's HelpBox */
#define TRIBE_SIZE     0x5F0
#define COMBO_PULLDOWN 0x28C /* the ComboBox's own PullDown */
#define COMBO_SEL      0x568 /* ComboBox: id of the selected item */
#define PD_BUFFER      0x154 /* the window's text Buffer, its Font * at +0x7C */
#define PD_FONT        0x1D0
#define PD_COUNT       0x278
#define PD_ITEMS       0x284 /* 64 bytes each: String at +0xC, id at +0x34, flags at +0x38 */
#define PD_HOVER       0x298 /* hovered item index, -1 = none */
#define PD_FIRST       0x2A0 /* scroll: first and end row shown */
#define PD_END         0x2A4
#define PD_SCROLLS     0x2AE /* byte: the list scrolls */
#define PD_ARROW       0x2AF /* byte: scroll arrow pressed */
#define PD_SKIP_UP     0x2B1 /* byte: ignore the next left_up */
#define ITEM_SHOWN     1
#define ITEM_DISABLED  4
#define RENDER_SOLID   5

/* layout, in window pixels */
#define PAD   8
#define CELL  52 /* a flag card */
#define FLAG  42
#define GAP   6
#define COLS  6
#define SEP   12 /* between the button column and the grid */
#define GLOW  4  /* rings of the hovered card's glow, less than GAP */
#define MAX_ITEMS 64

/* colours: 0xAABBGGRR */
#define C_BACK   0xF0141A20u
#define C_BORDER 0xFF3CBEE6u /* gold */
#define C_CARD   0xFF2A3036u
#define C_EDGE   0xFF4A525Au
#define C_HOVER  0xFF485462u
#define C_SEL    0xFF3CBEE6u
#define C_DIM    0xFF686868u

int g_ngrid_enabled;

typedef struct {
    int n, w, h;
    int cards;                /* the other choices as badge cards (else text buttons) */
    short index[MAX_ITEMS];   /* item index of each cell */
    short rect[MAX_ITEMS][4]; /* left, right, top, bottom */
} layout_t;

typedef int(__attribute__((thiscall)) *fn_i)(void *self);
typedef int(__attribute__((thiscall)) *fn_i_p)(void *self, const void *p);
typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_ii)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *resize_t)(void *self, int w, int h, int a, int b);
typedef int(__attribute__((thiscall)) *help_set_t)(void *self, const void *str, const short *rect, const void *str2,
                                                   int a, int b);
typedef void(__attribute__((thiscall)) *to_screen_t)(void *self, short *rect);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void(__attribute__((thiscall)) *get_mouse_t)(void *self, int *x, int *y);
typedef void(__attribute__((thiscall)) *ctor_t)(void *self, const char *s);

void ngrid_load_config(const char *ini_path)
{
    g_ngrid_enabled = GetPrivateProfileIntA("qol", "nation_grid", 1, ini_path);
}

static char *item(char *pd, int i) { return *(char **)(pd + PD_ITEMS) + 64 * i; }
static int item_id(char *pd, int i) { return *(int *)(item(pd, i) + 0x34); }
static unsigned item_flags(char *pd, int i) { return *(unsigned *)(item(pd, i) + 0x38); }
static int is_tribe(int id) { return id >= 0 && id < g_num_tribes; }

/* a nation combo's list, with the flags loaded */
static int is_grid(char *pd)
{
    return g_ngrid_enabled && pd && flags_nation_combo(pd - COMBO_PULLDOWN) && flags_tex(0);
}

/* the list's text Buffer, with the default font if it has none */
static char *text_buffer(char *pd)
{
    if (!*(void **)(pd + PD_FONT))
        *(void **)(pd + PD_FONT) = *(void **)VA(VA_DEF_FONT);
    return *(void **)(pd + PD_FONT) ? pd + PD_BUFFER : NULL;
}

static void make_layout(char *pd, layout_t *L)
{
    int count = *(int *)(pd + PD_COUNT), ne = 0, nt = 0, text_w = 0;
    char *buf = text_buffer(pd);
    for (int i = 0; i < count && i < MAX_ITEMS; i++) {
        if (!(item_flags(pd, i) & ITEM_SHOWN))
            continue;
        if (is_tribe(item_id(pd, i))) {
            nt++;
            continue;
        }
        ne++;
        int tw = buf ? ((fn_i_p)VA(VA_TEXT_WIDTH))(buf, item(pd, i) + 0xC) : 0;
        if (tw > text_w)
            text_w = tw;
    }
    int cols = nt < COLS ? (nt ? nt : 1) : COLS;
    int rows = (nt + cols - 1) / cols;
    /* With the badges (build_flags.py) the other choices are cards like the flags, one column
       on the left, no text (the tooltip names them); without them, text buttons. */
    L->cards = flags_choice_tex(g_num_tribes) != NULL;
    int row_h = L->cards ? CELL : ((fn_i)VA(VA_PD_ITEM_H))(pd) + 4, row_gap = L->cards ? GAP : 4;
    int col_w = !ne ? 0 : L->cards ? CELL : text_w + 24 < 90 ? 90 : text_w + 24 > 200 ? 200 : text_w + 24;
    int grid_l = PAD + (ne ? col_w + SEP : 0);
    int grid_h = rows * CELL + (rows ? (rows - 1) * GAP : 0), col_h = ne ? ne * row_h + (ne - 1) * row_gap : 0;
    L->w = grid_l + cols * CELL + (cols - 1) * GAP + PAD;
    L->h = PAD + (grid_h > col_h ? grid_h : col_h) + PAD;
    L->n = 0;
    int e = 0, t = 0;
    for (int i = 0; i < count && i < MAX_ITEMS; i++) {
        if (!(item_flags(pd, i) & ITEM_SHOWN))
            continue;
        short *r = L->rect[L->n];
        if (is_tribe(item_id(pd, i))) {
            int x = grid_l + (t % cols) * (CELL + GAP), y = PAD + (t / cols) * (CELL + GAP);
            r[0] = x, r[1] = x + CELL, r[2] = y, r[3] = y + CELL;
            t++;
        } else {
            int y = PAD + e * (row_h + row_gap);
            r[0] = PAD, r[1] = PAD + col_w, r[2] = y, r[3] = y + row_h;
            e++;
        }
        L->index[L->n++] = i;
    }
}

/* the item under (x, y), or -1 (also for a disabled item, as the game does) */
static int hit(char *pd, const layout_t *L, int x, int y)
{
    for (int k = 0; k < L->n; k++) {
        const short *r = L->rect[k];
        if (x >= r[0] && x < r[1] && y >= r[2] && y < r[3])
            return item_flags(pd, L->index[k]) & ITEM_DISABLED ? -1 : L->index[k];
    }
    return -1;
}

/* ---- hooks ----------------------------------------------------------------------------- */

/* the nation list last drawn or hovered (only one list is open at a time), and the mouse
   in its client pixels */
static char *g_hovered_pd;
static int g_mouse_x, g_mouse_y;
static int g_tip_item = -1;                  /* the item whose tooltip is shown, -1 none */
static unsigned char g_no_tip[MAX_ITEMS];    /* tribes without a TRIBETIP<n> in help.xml */
static void show_tip(char *pd, int hover);

__attribute__((thiscall)) static int grid_resize(char *pd, int w, int h, int a, int b)
{
    if (is_grid(pd)) {
        layout_t L;
        make_layout(pd, &L);
        w = L.w, h = L.h;
        *(int *)(pd + PD_FIRST) = 0;
        *(int *)(pd + PD_END) = *(int *)(pd + PD_COUNT);
        pd[PD_SCROLLS] = 0;
        pd[PD_ARROW] = 0;
        /* set when the list would not fit under the combo; the panel is smaller */
        pd[PD_SKIP_UP] = 0;
    }
    return ((resize_t)VA(VA_RESIZE))(pd, w, h, a, b);
}

__attribute__((thiscall)) static void grid_mouse_move(char *pd, int x, int y)
{
    if (!is_grid(pd)) {
        ((fn_v_ii)VA(VA_PD_MOUSE_MOVE))(pd, x, y);
        return;
    }
    layout_t L;
    make_layout(pd, &L);
    *(int *)(pd + PD_HOVER) = hit(pd, &L, x, y);
    g_hovered_pd = pd;
    g_mouse_x = x, g_mouse_y = y;
    show_tip(pd, *(int *)(pd + PD_HOVER));
}

/* PullDown::on_mouse_move calls mouse_move only inside the rect of its +0x184 object (vtable
   +0xD0), which is not the panel: the grid's hover never changed and no tooltip came up
   (2026-11-24). For the grid the whole panel counts; the parent is told as the game does. */
__attribute__((thiscall)) static void grid_on_mouse_move(char *pd, int x, int y)
{
    if (!is_grid(pd)) {
        ((fn_v_ii)VA(VA_PD_ON_MOVE))(pd, x, y);
        return;
    }
    layout_t L;
    make_layout(pd, &L);
    static int logged;
    if (logged < 3) {
        logged++;
        const short *gr = NULL;
        void *o = *(void **)(pd + 0x184);
        if (o)
            gr = ((const short *(__attribute__((thiscall)) *)(void *))(*(void ***)o)[0xD0 / 4])(o);
        logf_("nation grid: mouse at %d,%d on the %dx%d panel (the game's hover rect %d-%d x %d-%d)", x, y, L.w, L.h,
              gr ? gr[0] : 0, gr ? gr[1] : 0, gr ? gr[2] : 0, gr ? gr[3] : 0);
    }
    if (x >= 0 && x < L.w && y >= 0 && y < L.h)
        grid_mouse_move(pd, x, y);
    else if (*(int *)(pd + PD_HOVER) != -1) {
        *(int *)(pd + PD_HOVER) = -1;
        show_tip(pd, -1);
    }
    char *parent = *(char **)(pd + WIN_PARENT);
    if (parent)
        ((void(__attribute__((thiscall)) *)(void *, void *))(*(void ***)parent)[VT_PULLDOWN_CHANGE / 4])(parent, pd);
}

__attribute__((thiscall)) static void grid_left_up(char *pd, int x, int y)
{
    if (!is_grid(pd)) {
        ((fn_v_ii)VA(VA_PD_LEFT_UP))(pd, x, y);
        return;
    }
    layout_t L;
    make_layout(pd, &L);
    if (x < 0 || x >= L.w || y < 0 || y >= L.h)
        return;
    int i = hit(pd, &L, x, y);
    *(int *)(pd + PD_HOVER) = i;
    if (i < 0)
        return;
    if (pd[PD_SKIP_UP]) {
        pd[PD_SKIP_UP] = 0;
        return;
    }
    show_tip(pd, -1);
    ((fn_v)(*(void ***)pd)[VT_HIDE / 4])(pd); /* closes the list; the combo takes the item */
}

static void fill(const short *r, unsigned color)
{
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, r, color, RENDER_SOLID);
}

static void frame(const short *r, int t, unsigned color)
{
    short e[4][4] = { { r[0], r[1], r[2], (short)(r[2] + t) },
                      { r[0], r[1], (short)(r[3] - t), r[3] },
                      { r[0], (short)(r[0] + t), r[2], r[3] },
                      { (short)(r[1] - t), r[1], r[2], r[3] } };
    for (int k = 0; k < 4; k++)
        fill(e[k], color);
}

static void label(char *pd, const char *it, const short *r, int gold, int dim)
{
    char *buf = text_buffer(pd);
    if (!buf)
        return;
    static _Alignas(4) unsigned char color[12];
    memcpy(color, (void *)VA(VA_WHITE), 12);
    if (gold)
        color[0] = 0xE6, color[1] = 0xBE, color[2] = 0x3C;
    else if (dim)
        color[0] = color[1] = color[2] = 0x80;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, (void *)VA(VA_BLACK), -1, 0);
    const void *str = it + 0xC;
    int len = *(const unsigned short *)(it + 0xC + 8);
    if (len)
        ((write_t)VA(VA_WRITE_CENT))(buf, str, r, len);
}

/* The hovered card's glow: gold rings outside the card, fading outwards, their strength
   pulsing (about once a second). Drawn before the card, in the gap between cards. */
static void glow(const short *r)
{
    float t = (float)(GetTickCount() % 1100) / 1100.0f;
    float pulse = 0.55f + 0.45f * (t < 0.5f ? t * 2 : (1 - t) * 2); /* triangle wave 0.55..1 */
    static const float ring[GLOW] = { 0.85f, 0.55f, 0.3f, 0.12f };
    for (int i = 0; i < GLOW; i++) {
        short e[4] = { (short)(r[0] - 1 - i), (short)(r[1] + 1 + i), (short)(r[2] - 1 - i), (short)(r[3] + 1 + i) };
        unsigned a = (unsigned)(255 * ring[i] * pulse);
        frame(e, 1, (a << 24) | (C_SEL & 0xFFFFFF));
    }
}

__attribute__((thiscall)) static void grid_redraw(char *pd)
{
    if (!is_grid(pd)) {
        ((fn_v)VA(VA_PD_REDRAW))(pd);
        return;
    }
    g_hovered_pd = pd;
    layout_t L;
    make_layout(pd, &L);
    short all[4] = { 0, (short)L.w, 0, (short)L.h };
    fill(all, C_BACK);
    frame(all, 1, C_BORDER);
    int hover = *(int *)(pd + PD_HOVER), sel = *(int *)(pd - COMBO_PULLDOWN + COMBO_SEL);
    for (int k = 0; k < L.n; k++) {
        int i = L.index[k], id = item_id(pd, i);
        int on = i == hover, chosen = id == sel, off = (item_flags(pd, i) & ITEM_DISABLED) != 0;
        const short *r = L.rect[k];
        if (on && !off)
            glow(r);
        fill(r, on ? C_HOVER : C_CARD);
        frame(r, on || chosen ? 2 : 1, on || chosen ? C_SEL : C_EDGE);
        if (!is_tribe(id) && !L.cards) {
            short t[4] = { r[0], r[1], (short)(r[2] + 2), r[3] };
            label(pd, item(pd, i), t, chosen, off);
            continue;
        }
        /* a nation's flag, or the badge of Random, Offensive... */
        void *tex = flags_choice_tex(id);
        if (!tex)
            continue;
        int m = (CELL - FLAG) / 2, lift = on && !off ? 1 : 0;
        short f[4] = { (short)(r[0] + m - lift), (short)(r[1] - m + lift), (short)(r[2] + m - lift),
                       (short)(r[3] - m + lift) };
        flags_draw_bg(f, on ? C_HOVER : C_CARD);
        flags_draw(tex, f, off ? C_DIM : *(unsigned *)VA(VA_WHITE));
    }
}

/* The tooltip's anchor for the hovered item, in the screen space HelpBox::on_redraw tests
   the mouse in; 0 when the item is not on the panel. */
static int tip_anchor(char *pd, const layout_t *L, int hover, short r[4])
{
    for (int k = 0; k < L->n; k++) {
        if (L->index[k] != hover)
            continue;
        /* The anchor is also the hover zone: HelpBox::on_redraw clears the box once the
           mouse is outside it. adjust_rect puts the box right of the anchor (left of it
           when it does not fit), 5 pixels under its bottom. The card's row across the
           panel: the box comes out beside the panel, under the row, and stays while the
           mouse is on that row. */
        r[0] = 0, r[1] = (short)L->w, r[2] = L->rect[k][2], r[3] = L->rect[k][3];
        ((to_screen_t)VA(VA_TO_SCREEN))(pd, r);
        /* HelpBox::on_redraw tests the mouse as GraphSys reports it (vtable +0x38). If
           that is not the space client_to_screen gives (the interface scale), the anchor
           is scaled into it, from the mouse's own two positions. */
        short p[4] = { (short)g_mouse_x, (short)g_mouse_x, (short)g_mouse_y, (short)g_mouse_y };
        ((to_screen_t)VA(VA_TO_SCREEN))(pd, p);
        int mx = 0, my = 0;
        void *gs = *(void **)VA(VA_GRAPHSYS);
        if (gs)
            ((get_mouse_t)(*(void ***)gs)[VT_GET_MOUSE / 4])(gs, &mx, &my);
        float kx = 1, ky = 1;
        if (p[0] > 0 && p[2] > 0 && (abs(mx - p[0]) > 2 || abs(my - p[2]) > 2))
            kx = (float)mx / p[0], ky = (float)my / p[2];
        if (kx != 1 || ky != 1)
            for (int j = 0; j < 4; j++)
                r[j] = (short)(r[j] * (j < 2 ? kx : ky) + 0.5f);
        return 1;
    }
    return 0;
}

/* help.xml keys (section/entry, as SetupWin's help key "TRIBES/TRIBE" plus the item) of the
   other choices, by id - g_num_tribes (TribeIndex RANDOM_TRIBE...) */
static const char *const k_random_keys[] = { "TRIBES/TRIBERANDOM", "TRIBES/TRIBERANDOMOFFENSIVE",
                                             "TRIBES/TRIBERANDOMDEFENSIVE", "TRIBES/TRIBERANDOMECONOMIC",
                                             "TRIBES/TRIBERANDOMXPACK" };

static int help_set(void *hb, const char *key, const short *anchor, const char *key2, int parsed)
{
    unsigned char s1[64] __attribute__((aligned(4))) = { 0 }, s2[64] __attribute__((aligned(4))) = { 0 };
    ((ctor_t)VA(VA_STRING_CTOR))(s1, key);
    ((ctor_t)VA(VA_STRING_CTOR))(s2, key2);
    int r = ((help_set_t)VA(VA_HELP_SET))(hb, s1, anchor, s2, parsed, 0);
    ((fn_v)VA(VA_STRING_CLOSE))(s1);
    ((fn_v)VA(VA_STRING_CLOSE))(s2);
    return r;
}

/* The hovered item's tooltip, in SetupWin's own HelpBox, the way SetupWin::on_pulldown_change
   shows it for a list (called on every mouse move; HelpBox::set does nothing when the key and
   the anchor are the ones it shows). A nation: TRIBETIP<n> (its name and unique units, built
   by romania\tools\build_data.py) with TRIBE<n> (the game's power text, its numbers from
   Tribe::parse) under it; the other choices: their own text. */
static void show_tip(char *pd, int hover)
{
    char *sw = *(char **)VA(VA_SETUPWIN);
    if (!sw)
        return;
    void *hb = sw + SW_HELPBOX;
    layout_t L;
    make_layout(pd, &L);
    short r[4];
    if (hover < 0 || !tip_anchor(pd, &L, hover, r)) {
        if (g_tip_item >= 0)
            ((fn_v)VA(VA_HELP_CLEAR))(hb);
        g_tip_item = -1;
        return;
    }
    int id = item_id(pd, hover), res = 1;
    char key[32] = "", key2[32] = "";
    if (is_tribe(id)) {
        ((fn_v)VA(VA_TRIBE_PARSE))(*(char **)VA(VA_TRIBES_DATA) + TRIBE_SIZE * id);
        snprintf(key, sizeof key, "TRIBES/TRIBETIP%d", id);
        snprintf(key2, sizeof key2, "TRIBES/TRIBE%d", id);
        /* a missing key makes HelpBox::set write it to the game's list of missing texts,
           so it is asked for once */
        if (!g_no_tip[id] && (res = help_set(hb, key, r, key2, 1)) != 0)
            g_no_tip[id] = 1;
        if (g_no_tip[id]) /* the power text alone */
            res = help_set(hb, key2, r, "", 1);
    } else if (id - g_num_tribes >= 0 && id - g_num_tribes < 5) {
        snprintf(key, sizeof key, "%s", k_random_keys[id - g_num_tribes]);
        res = help_set(hb, key, r, "", 0);
    }
    if (hover != g_tip_item) {
        g_tip_item = hover;
        logf_("nation grid: tooltip %s for item %d (id %d)%s, anchor %d-%d x %d-%d", key[0] ? key : "-", hover, id,
              res ? ", not in help.xml" : "", r[0], r[1], r[2], r[3]);
    }
}

/* SetupWin::on_pulldown_change's two HelpBox::set calls: while the nation grid is open,
   show_tip owns the tooltip, so the game's own (anchored to the combo, under the panel)
   is left out. */
__attribute__((thiscall)) static int grid_help_set(void *hb, const void *str, const short *anchor, const void *str2,
                                                   int a, int b)
{
    char *pd = g_hovered_pd;
    if (pd && is_grid(pd) && (*(unsigned *)(pd + WIN_FLAGS) & 1))
        return 0;
    return ((help_set_t)VA(VA_HELP_SET))(hb, str, anchor, str2, a, b);
}

/* ---- install --------------------------------------------------------------------------- */

static int check_call(UINT_PTR at, UINT_PTR target)
{
    unsigned char *p = (unsigned char *)VA(at);
    return p[0] == 0xE8 && (UINT_PTR)(p + 5 + *(INT32 *)(p + 1)) == VA(target);
}

static void set_call(UINT_PTR at, void *fn)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(p + 1) = (INT32)((UINT_PTR)fn - (UINT_PTR)(p + 5));
    VirtualProtect(p, 5, old, &old);
}

static void set_slot(int off, void *fn)
{
    void **slot = (void **)VA(VA_PD_VTABLE + off);
    DWORD old;
    VirtualProtect(slot, 4, PAGE_READWRITE, &old);
    *slot = fn;
    VirtualProtect(slot, 4, old, &old);
}

/* Called from install_hooks once the exe is known to be the supported version. */
void ngrid_install(void)
{
    if (!g_ngrid_enabled)
        return;
    void **vt = (void **)VA(VA_PD_VTABLE);
    if (!check_call(0x00528B66u, VA_RESIZE) || !check_call(0x0052A17Du, VA_PD_MOUSE_MOVE) ||
        !check_call(0x005C3504u, VA_HELP_SET) || !check_call(0x005C3528u, VA_HELP_SET) ||
        (UINT_PTR)vt[VT_REDRAW / 4] != VA(VA_PD_REDRAW) || (UINT_PTR)vt[VT_LEFT_UP / 4] != VA(VA_PD_LEFT_UP) ||
        (UINT_PTR)vt[VT_MOUSE_MOVE / 4] != VA(VA_PD_ON_MOVE)) {
        logf_("nation grid: code mismatch, turned off");
        g_ngrid_enabled = 0;
        return;
    }
    set_call(0x00528B66u, (void *)grid_resize);
    set_call(0x0052A17Du, (void *)grid_mouse_move);
    set_call(0x005C3504u, (void *)grid_help_set);
    set_call(0x005C3528u, (void *)grid_help_set);
    set_slot(VT_REDRAW, (void *)grid_redraw);
    set_slot(VT_LEFT_UP, (void *)grid_left_up);
    set_slot(VT_MOUSE_MOVE, (void *)grid_on_mouse_move);
    logf_("nation grid installed");
}
