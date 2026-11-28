/*
 * Unit cards (the user, 2026-11-26): several objects selected, a "Unit cards" button at the right
 * end of the selection panel's top row (the kills row) switches the grid between one picture per
 * kind with its count (the "group selection" patches of ron_camera.c) and one card per unit, each
 * with its own health bar, so every aircraft of an Airbase shows its hit points. The choice is
 * kept in ron_camera.ini ([ui] unit_cards) for the next game.
 *
 *  - One card per unit is the game's own way: IFaceSelected::update_selected (0x00823790) gives
 *    every object of the selection a button of its own (IFaceButton +0x35C id, +0x360 player;
 *    IFaceButton::on_redraw draws its health from UnitData::total_damage) when they fit the grid's
 *    cells, and with more of them than cells it makes the cells 1/2, 1/3 ... as large until all
 *    fit (the scale loop at 0x00823B3E). The folded panel has 6 x 3 cells (18), the unfolded one a
 *    row of the panel's width, so 10 units always get full-size cards.
 *  - The grouped grid comes from two of the "group selection" patches, which this module turns
 *    back and forth in memory: 0x00823B38 (the "they fit" jle, nopped for grouping; put back for
 *    cards) and 0x00823B71 (the profile flag 0x20000 je, nopped for grouping; for cards a jmp to
 *    the same place, 0x00823C93, the game's ungrouped path, whatever the profile says). The third
 *    patch (0x00823C7A, the single-type cmove) is only on the grouped path and stays.
 *  - The button is drawn from kills.c's draw_group_name wrapper (the top row's rect,
 *    IFaceSelected +0x4E4) with the main menu's steel plate (IFACETEX ally_btn / _hover / _down);
 *    pressed while the cards show. The click is read once a frame (the Console's check_scroll
 *    hook): the left button going down with the mouse on the button, in the panel's own
 *    coordinates (Window::get_mouse_pos). Then IFaceMain::update_visibility (vtable +0x4E8) builds
 *    the grid again, as when the selection changes.
 * Interface only, nothing reaches the simulation.
 */
#include "ron.h"
#include <string.h>

#define VA_IFACE_MAIN     0x00E37B9Cu /* IFaceMain * (MiscAccess::iface_main) */
#define VA_IFACE_PTR      0x00E39964u /* IFaceMainBase * shown (iface_current): the same while a game is on */
#define VA_CONSOLE_PTR    0x00C06210u /* Console *: +0x2A0 the local selection */
#define VA_SELECTIONS     0x00E8D444u /* SelectGroup[], 0x9F0 each: +0xC count, +0x9D8 name (-1 none) */
#define VA_FIT_JUMP       0x00823B38u /* update_selected: jle 0x00823C93 (the units fit the cells) */
#define VA_FLAG_JUMP      0x00823B71u /* update_selected: je 0x00823C93 (profile flag 0x20000 clear) */
#define VA_GET_MOUSE_POS  0x00A54170u /* Window::get_mouse_pos(int *x, int *y): client coordinates */
#define VA_IFACE_RM       0x00EBD1C0u
#define VA_RM_ADD_RECT    0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_STRING_CLOSE   0x00A1CF40u
#define VA_WRITE_CENT     0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_TEXT_WIDTH     0x0050EF70u /* Buffer::text_width(const String &, len) */
#define VA_LINE_HEIGHT    0x0050F370u /* Buffer::text_line_height() */
#define VA_SET_TEXT_COLOR 0x0050DAF0u
#define VA_FONTS          0x00ED8160u
#define VA_FONT_GET       0x00522810u /* FontFamily::get(size, style) const */
#define VA_WHITE          0x00C8D26Cu
#define VA_BLACK          0x00C8D260u
#define VA_SOUND_PLAY     0x0097F770u /* SoundGlobal::play(cat) const, ret 4 (ecx unused) */
#define CLICK_SOUND       0x111
#define VT_UPDATE_VIS     0x4E8
#define IFM_SELECTED      0x8D1C      /* IFaceMain: the IFaceSelected */
#define SEL_SHOWN         0x468       /* IFaceSelected: shown */
#define SEL_MULTI         0x470       /* dword: several objects selected */
#define SEL_CELL          0x48C       /* short l r t b: the grid's first cell */
#define WIN_FLAGS         0x08        /* & 1: visible */
#define WIN_BUFFER        0x154
#define BUF_FONT          0x7C
#define BUF_COLORS        0x90
#define BUF_COLORS_N      0x80
#define RENDER_SOLID      5
#define LABEL             "Unit cards"

typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_i)(void *self);
typedef void(__attribute__((thiscall)) *mouse_t)(void *self, int *x, int *y);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *rm, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef int(__attribute__((thiscall)) *width_t)(void *self, const void *str, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);

static const unsigned char k_fit_grouped[6] = { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
static const unsigned char k_fit_cards[6] = { 0x0F, 0x8E, 0x55, 0x01, 0x00, 0x00 };
static const unsigned char k_flag_grouped[6] = { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
static const unsigned char k_flag_cards[6] = { 0xE9, 0x1D, 0x01, 0x00, 0x00, 0x90 }; /* jmp 0x00823C93 */

static int g_on;        /* [ui] unit_cards_button */
static int g_ready;     /* the patches are where we expect them */
static int g_cards;     /* [ui] unit_cards: one card per unit */
static char g_ini[MAX_PATH];
static char *g_btn_panel; /* where the button was drawn last, in that panel's coordinates */
static int g_btn[4];
static DWORD g_btn_time;
static int g_was_down;

void unitcards_load_config(const char *ini_path)
{
    lstrcpynA(g_ini, ini_path, MAX_PATH);
    g_on = GetPrivateProfileIntA("ui", "unit_cards_button", 1, ini_path);
    g_cards = GetPrivateProfileIntA("ui", "unit_cards", 0, ini_path) != 0;
}

/* kills.c installs the draw_group_name wrapper for the button too */
int unitcards_wanted(void) { return g_on && g_group_selection; }

static void write_code(UINT32 va, const unsigned char *bytes)
{
    unsigned char *at = (unsigned char *)VA(va);
    DWORD old;
    VirtualProtect(at, 6, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, 6);
    VirtualProtect(at, 6, old, &old);
}

static void apply(int cards)
{
    write_code(VA_FIT_JUMP, cards ? k_fit_cards : k_fit_grouped);
    write_code(VA_FLAG_JUMP, cards ? k_flag_cards : k_flag_grouped);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}

/* After apply_patches: only with the "group selection" patches in place (both sites as they
   left them, or as this module leaves them for cards). */
void unitcards_install(void)
{
    if (!g_on)
        return;
    if (!g_group_selection) {
        logf_("unit cards: group_selection is off, the grid already shows one picture per unit");
        return;
    }
    const unsigned char *fit = (const unsigned char *)VA(VA_FIT_JUMP), *flag = (const unsigned char *)VA(VA_FLAG_JUMP);
    if ((memcmp(fit, k_fit_grouped, 6) && memcmp(fit, k_fit_cards, 6)) ||
        (memcmp(flag, k_flag_grouped, 6) && memcmp(flag, k_flag_cards, 6))) {
        logf_("unit cards: signature mismatch at update_selected, no Unit cards button");
        return;
    }
    g_ready = 1;
    apply(g_cards);
    logf_("unit cards: the button is on the selection's top row (%s)", g_cards ? "one card per unit" : "grouped by kind");
}

/* ---- drawing ---------------------------------------------------------------------------- */

static void add(const void *tex, int l, int t, int r, int b, unsigned color)
{
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, tex ? 1 : RENDER_SOLID);
}

static void *set_font(char *buf, int size)
{
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), size, 1);
    if (font) {
        *(void **)(buf + BUF_FONT) = font;
        *(int *)(buf + BUF_FONT + 4) = 0;
        *(int *)(buf + BUF_FONT + 8) = 0;
    }
    return font;
}

static int inside(int x, int y, const int *r) { return x >= r[0] && x < r[2] && y >= r[1] && y < r[3]; }

/* Several objects selected: the button at the right end of the top row `rect` (l r t b, the
   panel's coordinates), from kills.c's draw_group_name wrapper. */
void unitcards_draw(char *panel, const short *rect)
{
    if (!g_ready || !panel || !rect || !*(int *)(panel + SEL_MULTI))
        return;
    char *console = *(char **)VA(VA_CONSOLE_PTR), *sel = *(char **)VA(VA_SELECTIONS);
    if (!console || !sel)
        return;
    char *group = sel + *(int *)(console + 0x2A0) * 0x9F0;
    /* the top row is there for every group with selpanel.c, else only for a named one */
    if (!g_sel_kills_row && *(int *)(group + 0x9D8) == -1)
        return;
    int h = rect[3] - rect[2];
    if (h < 14 || rect[1] - rect[0] < 120)
        return;
    int bh = h - 4 < 24 ? h - 4 : 24, top = rect[2] + (h - bh) / 2;
    /* the kills take the row's first cell (kills.c) */
    short *cell = (short *)(panel + SEL_CELL);
    int cw = cell[1] - cell[0];
    if (cw <= 0 || cw > rect[1] - rect[0])
        cw = 60;
    int room = rect[1] - 2 - (rect[0] + cw + 6);

    char *buf = panel + WIN_BUFFER;
    unsigned char saved[12], saved_colors[BUF_COLORS_N];
    memcpy(saved, buf + BUF_FONT, 12);
    memcpy(saved_colors, buf + BUF_COLORS, BUF_COLORS_N);
    unsigned char str[64] __attribute__((aligned(4))) = { 0 };
    lang_string(str, lang_text(LABEL));
    int len = *(unsigned short *)(str + 8);
    int size = 11, bw = 0, fits = 0;
    for (; len && size >= 8 && set_font(buf, size); size--) {
        int tw = ((width_t)VA(VA_TEXT_WIDTH))(buf, str, len);
        bw = tw + bh + 4; /* half a height of plate each side */
        if (bw < 3 * bh)  /* the plates' ratio, 3:1 at least */
            bw = 3 * bh;
        if (tw > 0 && bw <= room) {
            fits = 1;
            break;
        }
    }
    if (!fits) {
        g_btn_panel = NULL;
        memcpy(buf + BUF_FONT, saved, 12);
        ((fn_v)VA(VA_STRING_CLOSE))(str);
        return;
    }
    int r = rect[1] - 2, l = r - bw;
    g_btn[0] = l, g_btn[1] = top, g_btn[2] = r, g_btn[3] = top + bh;
    g_btn_panel = panel;
    g_btn_time = GetTickCount();

    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(panel, &mx, &my);
    int hover = inside(mx, my, g_btn);
    const void *tex = qol_iface_tex(g_cards ? "ally_btn_down" : hover ? "ally_btn_hover" : "ally_btn");
    if (tex) {
        add(tex, l, top, r, top + bh, 0xFFFFFFFFu);
    } else {
        add(NULL, l, top, r, top + bh, g_cards ? 0xFF1E2226u : hover ? 0xFF404850u : 0xFF282E34u);
    }
    /* the label: gold while the cards show */
    unsigned rgb = g_cards ? 0xF0C860u : hover ? 0xFFFFFFu : 0xD8D8D8u;
    _Alignas(4) unsigned char color[12], shade[12];
    memcpy(color, (void *)VA(VA_WHITE), 12);
    color[0] = (unsigned char)(rgb >> 16), color[1] = (unsigned char)(rgb >> 8), color[2] = (unsigned char)rgb;
    color[3] = 255;
    memcpy(shade, (void *)VA(VA_BLACK), 12);
    shade[3] = 255;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, shade, -1, 0);
    int lh = ((fn_i)VA(VA_LINE_HEIGHT))(buf);
    if (lh <= 0 || lh > bh)
        lh = bh;
    int ty = top + (bh - lh) / 2 + (g_cards ? 1 : 0);
    short tr[4] = { (short)l, (short)r, (short)ty, (short)(ty + lh) };
    ((write_t)VA(VA_WRITE_CENT))(buf, str, tr, len);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    memcpy(buf + BUF_FONT, saved, 12);
    memcpy(buf + BUF_COLORS, saved_colors, BUF_COLORS_N);
}

/* ---- the click ----------------------------------------------------------------------------- */

static int game_in_front(void)
{
    DWORD pid = 0;
    HWND fg = GetForegroundWindow();
    if (!fg)
        return 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

/* Once a frame (ron_camera.c's check_scroll hook). */
void unitcards_tick(void)
{
    if (!g_ready)
        return;
    int vk = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
    int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    int press = down && !g_was_down;
    g_was_down = down;
    if (!press || !g_btn_panel || GetTickCount() - g_btn_time > 250 || !game_in_front())
        return;
    char *main = *(char **)VA(VA_IFACE_MAIN), *iface = *(char **)VA(VA_IFACE_PTR);
    char *panel = main && main == iface ? main + IFM_SELECTED : NULL;
    if (!panel || panel != g_btn_panel || !*(int *)(panel + SEL_SHOWN) || !*(int *)(panel + SEL_MULTI) ||
        !(panel[WIN_FLAGS] & 1))
        return;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(panel, &mx, &my);
    if (!inside(mx, my, g_btn))
        return;
    g_cards = !g_cards;
    apply(g_cards);
    WritePrivateProfileStringA("ui", "unit_cards", g_cards ? "1" : "0", g_ini);
    ((fn_v_i)VA(VA_SOUND_PLAY))(NULL, CLICK_SOUND);
    g_btn_panel = NULL; /* drawn again with the new grid */
    ((fn_v)(*(void ***)main)[VT_UPDATE_VIS / 4])(main);
    logf_("unit cards: %s", g_cards ? "one card per unit" : "grouped by kind");
}
