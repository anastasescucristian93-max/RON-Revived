/*
 * The resource panel (top left) in the worker-bars layout ([ui] resource_bars):
 *  - no workers column: under each resource row a bar with one segment per place of work of
 *    that resource (farm, woodcutter, mine, oil well, merchant, scholar places), lit for the
 *    ones in use, and "used/places" at the row's right end;
 *  - the workers column's toggle is a handle under the rows that shows / hides the bars (the
 *    rows are shorter without them);
 *  - the market column opens right of the resources, its rows level with theirs, instead of
 *    left of them, pushing the panel aside;
 *  - the population and city counters (IFaceMainBase's cap_area) sit side by side on the cap
 *    row right of the resources, or one under the other under the market's prices while the
 *    market is open; the closed market's tab starts under the cap row, out of their way; one
 *    dark background under their whole frame.
 * The theme (ui-theme\tools\build_menu_theme.py, PANEL_RECTS) makes the resource column wider
 * and gives the handle its size (infra_button_rect1).
 *
 * IFaceResources keeps its rects at +0x4A4 (market_rect0 ... infra_item_rect, interface.xml's
 * order, short left, right, top, bottom); +0x470 is the state (1 market, 2 resources, 4 workers
 * open). adjust_rects places them (market at x 0, the resources right of it, the workers right
 * of those) and sizes the window to their union; the rows' height comes from market_sell_rect.
 * The hooks let it run, then move everything to this layout and size the window again.
 * The workers are counted only in draw_infra_area (into +0x47C..0x490, while the column is
 * open), so that function still runs, with its drawing jumped over: "bars shown" is the workers
 * column's own open bit, toggled by its own button (the handle).
 */
#include "ron.h"
#include <string.h>

#define VA_ADJUST_RECTS    0x00819A90u /* IFaceResources::adjust_rects() */
#define VA_ADJUST_BUTTONS  0x00819660u /* IFaceResources::adjust_buttons() */
#define VA_RES_AREA        0x00818CC0u /* IFaceResources::draw_resource_area(const Rect *) */
#define VA_DRAW_RESOURCE   0x00818840u /* IFaceResources::draw_resource(int, const Rect *) */
#define VA_BTN_REDRAW      0x0081AF70u /* IFaceResources::on_button_redraw(Button *, int) */
#define VA_CAP_AREA        0x00810370u /* IFaceMainBase::draw_cap_area(): the counters' frame */
#define VA_DRAW_BACK       0x0081A920u /* IFaceResources::draw_back(const Rect *, const Color &) */
#define AT_ROW_BACK        0x008189BEu /* draw_resource: call draw_back, the row's plate */
#define AT_INFRA_FRAME     0x00818289u /* draw_infra_area: mov ecx, [edi+0x184] (frame, then the counting) */
#define AT_INFRA_COUNT     0x008183AAu /* call get_who: the counting loop */
#define AT_INFRA_ROWS      0x00818561u /* xor ebx, ebx / mov esi, 0x19AC: the column's rows */
#define AT_INFRA_END       0x008187DCu
#define AT_MARKET_TITLE    0x00819261u /* draw_market_area: the "Buy" / "Sell" titles' rect, taken
                                          as window coordinates (the market was always at x 0) */
#define AT_MARKET_TITLE_GO 0x0081926Du
#define VA_GET_WHO         0x0081A8B0u /* IFaceResources::get_who(): the player shown */
#define VA_CARAVAN_LIMIT   0x006DCA50u /* LeaderData::get_caravan_limit(int) */
#define VA_WINDOW_MOVE     0x00A54080u /* Window::move(x, y) */
#define VA_RESIZE          0x0051A550u /* GraphicWin::resize(w, h, int, PixFormat) */
#define VA_LEADERS_REF     0x00C061E0u /* LeaderData * (GameAccessConst::leadersc), 0x6EEC each */
#define VA_GAME_REF        0x00C061E8u /* Game * (GameAccessConst::gamec) */
#define VA_IFACE_CURRENT   0x00E39964u /* IFaceMainBase * (MiscAccess::iface_current) */
#define VA_IFACE_MAIN      0x00E37B9Cu /* IFaceMain * (MiscAccess::iface_main) */
#define IM_RESOURCES       0x4C9C      /* IFaceMain: its IFaceResources */
#define VA_STR_TABLE       0x00C06378u /* StringTable *: +0x10 = the interface's String names */
#define VA_IFACE_RM        0x00EBD1C0u /* IFaceRenderManager */
#define VA_RM_ADD_RECT     0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], 0xAABBGGRR, type) */
#define VA_WRITE           0x0051F660u /* IFaceRenderManager::write(String &, x, y, Color, style), returns x after */
#define VA_WRITE_WIDTH     0x0051F5C0u /* IFaceRenderManager::write_width(String &, style) */
#define VA_STRING_CTOR     0x00A1D660u
#define VA_STRING_CLOSE    0x00A1CF40u
#define VA_STRING_AS_CHAR  0x0042D710u /* String::str_as_char() const */
#define AT_RES_SLASH       0x00818B90u /* draw_resource: write the "/" between amount and rate */
#define AT_POP_TEXT        0x0081032Cu /* draw_pop_cap: write "population/limit" */
#define AT_CITY_TEXT       0x0080FF69u /* draw_city_cap: write "cities/limit" */
#define VA_MARKET_BUTTON   0x00817BA0u /* IFaceResources::draw_market_button(Button *, int) */
#define AT_MB_LEFT         0x00817D50u /* its add_smooth of the left end, at the picture's height */
#define AT_MB_RIGHT        0x00817DF4u /* the right end */
#define AT_MB_MIDDLE       0x00817E37u /* tile_random of the middle over the button's rect */
#define VA_ADD_SMOOTH      0x0051B700u
#define VA_GRAPHSYS        0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define AT_CAP_BORDER      0x008103FBu /* draw_cap_area: call draw_border, the counters' frame */
#define AT_POP_DIVIDER     0x00810345u /* draw_pop_cap: call draw_vert_divider */
#define VA_DRAW_BORDER     0x0051D260u /* IFaceRenderManager::draw_border(Rect *, corners, edges, extras, Color) */
#define VA_HORIZ_DIVIDER   0x0051CF80u /* IFaceRenderManager::draw_horiz_divider(x, y, length, thick, -) */
#define VA_VERT_DIVIDER    0x0051D0D0u
#define VA_FONT_TOP        0x00EBDEACu /* short top, bottom of the numbers' glyphs (draw_resource centres with them) */
#define VA_WHITE           0x00C8D26Cu
#define VA_GREY            0x00C8D218u
#define VA_BLACK           0x00C8D260u

#define RS_STATE   0x470
#define RS_TRADES  0x474 /* market rows (resources shown that can be traded) */
#define RS_ROWS    0x478 /* resources shown */
#define RS_UNION   0x494 /* the window's rect */
#define RS_RECTS   0x4A4
#define RS_BUTTONS 0x804 /* Button[15], 0x3C4 each: market, resources, workers, sell x6, buy x6 */
#define BTN_SIZE   0x3C4
#define BTN_FLAGS  0x318 /* bit 0: pressed */
#define WIN_RECT   0x126 /* short left, right, top, bottom */
#define WIN_RESIZE 0x20  /* Window vtable: resize(w, h, int, int) */
#define TEX_PLATE  0x534 /* resource_button_tex (IFaceTex: +4 short left, right, top, bottom) */
#define TEX_PLATE_MID 0x554 /* resource_item_tex1, the middle of the rows' plate */
#define TEX_CORNERS 0x574   /* corner_tex0..3, the panel's frame */
#define TEX_SIDES   0x5B4   /* side_tex0..3 */
#define TEX_UP     0x6A4
#define TEX_DOWN   0x6B4
#define TEX_ICONS  0x7F4 /* first_good_tex, the resource icons */
#define LD_RES     0x6EB8 /* LeaderData: resource block; +0x4C + 4 r, ^ 0x8932: at the commerce cap */
#define MB_FIND_RECT 0x488 /* GraphicWinXML vtable: find_rect(const String &, id, int *) */
#define RENDER_SOLID 5

enum { MARKET0, MARKET1, MBTN0, MBTN1, TITLE, BUY, SELL, RES0, RES1, RBTN0, RBTN1, CAP, ITEM,
       INFRA0, INFRA1, IBTN0, IBTN1, IITEM, NRECTS };

/* draw_infra's places of work per resource (food, timber, wealth, knowledge, metal, oil): the
   count IFaceResources keeps, and the LeaderData field with the places (-1: merchants, through
   get_caravan_limit) */
static const struct { short used, places; } WORK[6] = {
    { 0x47C, 0x8A4 }, { 0x480, 0x8A8 }, { 0x490, -1 }, { 0x488, 0x8B0 }, { 0x484, 0x8B4 }, { 0x48C, 0x8B8 },
};

typedef void(__attribute__((thiscall)) *fn_v_t)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_p_t)(void *self, const void *a);
typedef void(__attribute__((thiscall)) *draw_res_t)(void *self, int r, const short *rect);
typedef int(__attribute__((thiscall)) *btn_redraw_t)(void *self, void *btn, int id);
typedef int(__attribute__((thiscall)) *fn_int_t)(void *self);
typedef int(__attribute__((thiscall)) *fn_int_int_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *move_t)(void *win, int x, int y);
typedef int(__attribute__((thiscall)) *resize_t)(void *win, int w, int h, int a, int b);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *rm, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef int(__attribute__((thiscall)) *write_t)(void *rm, const void *s, int x, int y, unsigned c0, unsigned c1,
                                                unsigned c2, int style);
typedef int(__attribute__((thiscall)) *write_width_t)(void *rm, const void *s, int style);
typedef void(__attribute__((thiscall)) *fn_v_cstr_t)(void *self, const char *s);
typedef const char *(__attribute__((thiscall)) *as_char_t)(const void *self);
typedef short *(__attribute__((thiscall)) *find_rect_t)(void *self, const void *name, int id, void *out);

static int g_on = 1;
static void *g_orig_adjust_rects, *g_orig_adjust_buttons, *g_orig_res_area, *g_orig_draw_resource,
    *g_orig_btn_redraw, *g_orig_cap_area, *g_orig_market_button;

/* the rects as interface.xml gives them (adjust_rects moves them, and the row heights are set
   here), taken from each new panel before its first layout */
static char *g_snap_for;
static short g_snap[NRECTS][4];
/* where the population and city counters go: 0 not known yet, 1 side by side on the cap row from
   x g_counters_at[0], 2 one under the other in the box g_counters_at = left, right, top */
static int g_counters;
static short g_counters_at[3];
static short g_counter_rects[3][4]; /* where counters_follow last put the frame, population, cities */

/* read by the asm stubs */
int g_rp_back_full;  /* draw_resource's plate covers the whole row, bar included */
short g_rp_back_rect[4];
void *g_rp_draw_back, *g_rp_infra_count, *g_rp_infra_end, *g_rp_market_title_go, *g_rp_add_smooth;
int g_rp_mb_h;     /* in draw_market_button: the button's height, its picture stretched to it */
float g_rp_mb_hf;
extern int g_hud_zone; /* hud.c: numbers drawn in ui-theme's font inside it */

static short (*rects(char *self))[4] { return (short (*)[4])(self + RS_RECTS); }
static int bars_shown(const char *self) { return (self[RS_STATE] & 6) == 6; }
static int text_height(void) { return g_snap[ITEM][3] - g_snap[ITEM][2]; }
static int bar_strip(void) { return (text_height() * 3 + 4) / 8; } /* 8 under a 21-high row */
/* population over cities, each as tall as a resource row's text, 3 apart from each other and from
   the frame, as the resource rows */
static int counters_box_height(void) { return 2 * text_height() + 9; }

void respanel_load_config(const char *ini_path)
{
    g_on = GetPrivateProfileIntA("ui", "resource_bars", 1, ini_path);
}

static void __attribute__((thiscall)) rp_adjust_rects(char *self)
{
    short (*r)[4] = rects(self);
    if (g_snap_for != self) {
        memcpy(g_snap, r, sizeof g_snap);
        g_snap_for = self;
    }
    for (int k = MARKET0; k <= SELL; k++) { /* what adjust_rects reads, as interface.xml has it */
        r[k][2] = g_snap[k][2];
        r[k][3] = g_snap[k][3];
    }
    int row = text_height() + (bars_shown(self) ? bar_strip() : 0);
    r[ITEM][3] = r[ITEM][2] + row;
    r[BUY][3] = r[BUY][2] + row; /* market rows level with the resource rows */
    r[SELL][3] = r[SELL][2] + row; /* and this one sets the resource column's height */
    ((fn_v_t)g_orig_adjust_rects)(self);
    if (!*(int *)(self + RS_ROWS))
        return;

    int open = (self[RS_STATE] & 2) != 0, market = (self[RS_STATE] & 3) == 3;
    for (int k = RES0; k <= RES1; k++) {
        int w = r[k][1] - r[k][0];
        r[k][0] = 0;
        r[k][1] = (short)w;
    }
    r[RES1][3] += (short)(g_snap[IBTN1][3] - g_snap[IBTN1][2] + 6); /* the handle under the rows */
    short *res = r[open ? RES1 : RES0];
    for (int k = MARKET0; k <= MARKET1; k++) {
        r[k][0] = (short)(res[1] - 1);
        r[k][1] = (short)(r[k][0] + (open ? g_snap[k][1] - g_snap[k][0] : 0));
    }
    int mw = g_snap[MARKET1][1] - g_snap[MARKET1][0], hh = g_snap[IBTN1][3] - g_snap[IBTN1][2];
    if (open && !market) { /* closed: the counters' box at the top, a flat bar under it opens the market */
        short *m = r[MARKET0], *b = r[MBTN0];
        m[1] = (short)(m[0] + mw);
        m[2] = (short)(counters_box_height() - 1);
        m[3] = (short)(m[2] + hh + 8);
        b[0] = 4; /* relative to the market */
        b[1] = (short)(mw - 4);
        b[2] = 4;
        b[3] = (short)(4 + hh);
    }
    for (int k = INFRA0; k <= INFRA1; k++) /* no column: no width, at the resources' edge (their height
                                              keeps the workers' button rects, sized from it, positive) */
        r[k][0] = r[k][1] = (short)(res[1] - 1);
    short u[4] = { res[0], res[1], res[2], res[3] };
    if (open) {
        short *m = r[market ? MARKET1 : MARKET0];
        if (m[1] > u[1])
            u[1] = m[1];
        if (m[3] > u[3])
            u[3] = m[3];
    }
    memcpy(self + RS_UNION, u, sizeof u);
    ((resize_t)VA(VA_RESIZE))(self, u[1] - u[0], u[3] - u[2], 0, 0);
    if (open) { /* the counters one under the other in the market's column: under its prices, or at
                   the top over the bar that opens it */
        short *m = r[market ? MARKET1 : MARKET0];
        g_counters = 2;
        g_counters_at[0] = m[0];
        g_counters_at[1] = (short)(m[0] + mw);
        g_counters_at[2] = (short)(market ? m[3] - 1 : 0); /* frames overlapping, as market and resources */
    } else { /* panel folded: side by side on the cap row */
        g_counters = 1;
        g_counters_at[0] = (short)(res[1] + 2);
    }
}

static void __attribute__((thiscall)) rp_adjust_buttons(char *self)
{
    ((fn_v_t)g_orig_adjust_buttons)(self);
    if (!*(int *)(self + RS_ROWS) || !(self[RS_STATE] & 2) || g_snap_for != self)
        return;
    short (*r)[4] = rects(self);
    int market = (self[RS_STATE] & 3) == 3;
    short *m = r[market ? MARKET1 : MARKET0], *b = r[market ? MBTN1 : MBTN0];
    ((move_t)VA(VA_WINDOW_MOVE))(self + RS_BUTTONS, m[0] + b[0], m[2] + b[2]); /* b is relative to m */
    if (market) { /* Sell (buttons 3..) and Buy (9..) as tall as their row (rp_market_button stretches
                     their picture to it) */
        int step = r[SELL][3] - r[SELL][2] + 3, n = *(int *)(self + RS_TRADES);
        for (int i = 0; i < n && i < 6; i++)
            for (int k = SELL, idx = 3; k >= BUY; k--, idx += 6) {
                char *btn = self + RS_BUTTONS + (idx + i) * BTN_SIZE;
                ((move_t)VA(VA_WINDOW_MOVE))(btn, m[0] + r[k][0], m[2] + r[k][2] + i * step);
                ((resize_t)(*(void ***)btn)[WIN_RESIZE / 4])(btn, r[k][1] - r[k][0], r[k][3] - r[k][2], 0, 0);
            }
    }
    char *handle = self + RS_BUTTONS + 2 * BTN_SIZE; /* as wide as the rows */
    int hw = g_snap[ITEM][1] - g_snap[ITEM][0], hh = g_snap[IBTN1][3] - g_snap[IBTN1][2];
    short *res = r[RES1];
    ((move_t)VA(VA_WINDOW_MOVE))(handle, res[0] + g_snap[ITEM][0], res[3] - hh - 5);
    ((resize_t)(*(void ***)handle)[WIN_RESIZE / 4])(handle, hw, hh, 0, 0);
}

/* The counters' three rects (IFaceMainBase looks them up by name on every draw), placed as
   g_counters says. Looked up every frame: a new game's interface may come at the same address
   with interface.xml's rects again. */
static int counter_rects(char *mb, short *r[3])
{
    char *tbl = *(char **)VA(VA_STR_TABLE);
    static const int names[3] = { 0x15040, 0x15054, 0x15068 }; /* cap_area, pop_cap, city_cap */
    for (int i = 0; i < 3; i++) {
        r[i] = mb && tbl ? ((find_rect_t)(*(void ***)mb)[MB_FIND_RECT / 4])(mb, *(char **)(tbl + 0x10) + names[i],
                                                                              -1, NULL)
                         : NULL;
        if (!r[i])
            return 0;
    }
    return 1;
}

static void counters_follow(void)
{
    static short xml[3][4]; /* as interface.xml has them: the first ones seen, before any move */
    static int have_xml;
    short *r[3], want[3][4];
    if (!g_counters || !counter_rects(*(char **)VA(VA_IFACE_CURRENT), r))
        return;
    if (!have_xml) {
        for (int i = 0; i < 3; i++)
            memcpy(xml[i], r[i], sizeof xml[i]);
        have_xml = 1;
    }
    if (g_counters == 1) {
        int dx = g_counters_at[0] - xml[0][0];
        for (int i = 0; i < 3; i++) {
            want[i][0] = (short)(xml[i][0] + dx);
            want[i][1] = (short)(xml[i][1] + dx);
            want[i][2] = xml[i][2];
            want[i][3] = xml[i][3];
        }
    } else { /* counters_box_height: the cells as tall as the resources' text */
        int px = xml[1][0] - xml[0][0], ch = text_height();
        int x0 = g_counters_at[0], x1 = g_counters_at[1], y = g_counters_at[2];
        short frame[4] = { (short)x0, (short)x1, (short)y, (short)(y + counters_box_height()) };
        short pop[4] = { (short)(x0 + px), (short)(x1 - px), (short)(y + 3), (short)(y + 3 + ch) };
        short city[4] = { pop[0], pop[1], (short)(pop[3] + 3), (short)(pop[3] + 3 + ch) };
        memcpy(want[0], frame, sizeof frame);
        memcpy(want[1], pop, sizeof pop);
        memcpy(want[2], city, sizeof city);
    }
    for (int i = 0; i < 3; i++)
        memcpy(r[i], want[i], sizeof want[i]);
    memcpy(g_counter_rects, want, sizeof g_counter_rects);
}

static void add_solid(int x0, int x1, int y0, int y1, unsigned abgr);
static int __attribute__((thiscall)) rp_write_sep(void *rm, const void *str, int x, int y, unsigned c0, unsigned c1,
                                                  unsigned c2, int style);

/* the IFaceResources the counters take their plates from (IFaceMain's own) */
static char *counters_panel(void)
{
    char *im = *(char **)VA(VA_IFACE_MAIN);
    return im ? im + IM_RESOURCES : NULL;
}

/* One dark background under the whole counters' frame: the game only puts one under each
   number, and the terrain showed through between and around them. Textured (the resource rows'
   plate middle, in their grey), so it is drawn before the digits: over a solid rectangle alone
   their soft bottom edge, which reaches past the number's own plate, showed a line of terrain. */
static void __attribute__((thiscall)) rp_cap_area(char *self)
{
    short *r[3];
    if (counter_rects(self, r)) {
        short f[4] = { (short)(r[0][0] + 2), (short)(r[0][1] - 2), (short)(r[0][2] + 2), (short)(r[0][3] - 2) };
        /* ui-theme's opaque block, near black as the resource panel's fill, a depth step under the
           cells: their plates are see-through, and over anything drawn later (a solid rectangle,
           or this block at the same depth) they came out darker than the resource rows */
        const void *block = qol_iface_tex("hud_block");
        int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + 0x164);
        if (block) {
            (*depth)--;
            ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), block, f, 0xFF101010, 1);
            (*depth)++;
        } else
            add_solid(f[0], f[1], f[2], f[3], 0xFF101010);
    }
    ((fn_v_t)g_orig_cap_area)(self);
}

typedef void(__attribute__((thiscall)) *border_t)(void *rm, const short *rect, const void *corners, const void *edges,
                                                   const void *extras, unsigned c0, unsigned c1, unsigned c2);
typedef void(__attribute__((thiscall)) *divider_t)(void *rm, int x, int y, int len, int thick, int unused);

/* draw_cap_area's frame in the resource panel's pieces (corner_tex*, side_tex*), as the market's */
static void __attribute__((thiscall)) rp_cap_border(void *rm, const short *rect, const void *corners,
                                                    const void *edges, const void *extras, unsigned c0, unsigned c1,
                                                    unsigned c2)
{
    char *res = counters_panel();
    if (res) {
        corners = res + TEX_CORNERS;
        edges = res + TEX_SIDES;
        extras = NULL;
    }
    ((border_t)VA(VA_DRAW_BORDER))(rm, rect, corners, edges, extras, c0, c1, c2);
}

/* draw_pop_cap's last call, the upright divider right of the population (side by side with the
   cities): one under the other, the resource rows' divider between them instead */
static void __attribute__((thiscall)) rp_pop_divider(void *rm, int x, int y, int len, int thick, int unused)
{
    if (g_counters != 2) {
        ((divider_t)VA(VA_VERT_DIVIDER))(rm, x, y, len, thick, unused);
        return;
    }
    short *frame = g_counter_rects[0], *pop = g_counter_rects[1];
    ((divider_t)VA(VA_HORIZ_DIVIDER))(rm, frame[0], pop[3], frame[1] - frame[0], 0, 0);
}

/* the counters' "n/limit", a unit higher than the game puts it: centred for its pixel font,
   ui-theme's digits reached the bottom edge of their plate */
static int __attribute__((thiscall)) rp_write_counter(void *rm, const void *str, int x, int y, unsigned c0, unsigned c1,
                                                      unsigned c2, int style)
{
    return rp_write_sep(rm, str, x, y - 1, c0, c1, c2, style);
}

static void __attribute__((thiscall)) rp_res_area(char *self, const short *rect)
{
    counters_follow(); /* drawn every frame, folded or not, while iface_current is the live one */
    ((fn_v_p_t)g_orig_res_area)(self, rect);
}

static int text_width(const char *text)
{
    unsigned char str[64] = { 0 };
    ((fn_v_cstr_t)VA(VA_STRING_CTOR))(str, text);
    int w = ((write_width_t)VA(VA_WRITE_WIDTH))((void *)VA(VA_IFACE_RM), str, 0);
    ((fn_v_t)VA(VA_STRING_CLOSE))(str);
    return w;
}

static int write_plain(const char *text, int x, int y, const unsigned *c, int style)
{
    unsigned char str[64] = { 0 };
    ((fn_v_cstr_t)VA(VA_STRING_CTOR))(str, text);
    x = ((write_t)VA(VA_WRITE))((void *)VA(VA_IFACE_RM), str, x, y, c[0], c[1], c[2], style);
    ((fn_v_t)VA(VA_STRING_CLOSE))(str);
    return x;
}

static void add_solid(int x0, int x1, int y0, int y1, unsigned abgr)
{
    short rect[4] = { (short)x0, (short)x1, (short)y0, (short)y1 };
    if (x1 > x0 && y1 > y0)
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, rect, abgr, RENDER_SOLID);
}

/* A thin upright line in the place of a "/" (the number font's slash, enlarged, looked
   pixelated): between amount and rate, population and its limit, workers and places. */
static int separator(int x, int y)
{
    int w = text_width("/");
    int h = (int)((*(short *)VA(VA_FONT_TOP + 2) - *(short *)VA(VA_FONT_TOP)) * hud_text_scale() + 0.5f);
    add_solid(x + w / 2, x + w / 2 + 1, y + 1, y + h - 1, 0xFF8A8A8A);
    return x + w;
}

/* IFaceRenderManager::write, from the places that write a "/" in the panel and the counters:
   the text with each "/" drawn as separator(). */
static int __attribute__((thiscall)) rp_write_sep(void *rm, const void *str, int x, int y, unsigned c0, unsigned c1,
                                                  unsigned c2, int style)
{
    char text[64];
    const char *s = ((as_char_t)VA(VA_STRING_AS_CHAR))(str);
    lstrcpynA(text, s ? s : "", sizeof text);
    if (!strchr(text, '/'))
        return ((write_t)VA(VA_WRITE))(rm, str, x, y, c0, c1, c2, style);
    const unsigned c[3] = { c0, c1, c2 };
    for (char *p = text;;) {
        char *e = strchr(p, '/');
        if (e)
            *e = 0;
        if (*p)
            x = write_plain(p, x, y, c, style);
        if (!e)
            return x;
        x = separator(x, y);
        p = e + 1;
    }
}

/* The bar in the row's bottom strip and "used/places" at the right end of its top part. */
static void draw_workers(char *self, int res, const short *row, const short *top)
{
    int who = ((fn_int_t)VA(VA_GET_WHO))(self);
    if (who < 0 || res < 0 || res > 5)
        return;
    char *leader = *(char **)VA(VA_LEADERS_REF) + who * 0x6EEC;
    int used = *(int *)(self + WORK[res].used);
    int places = WORK[res].places < 0 ? ((fn_int_int_t)VA(VA_CARAVAN_LIMIT))(leader, 1)
                                      : *(int *)(leader + WORK[res].places);
    if (places < 0)
        places = 0;

    const unsigned short *icon = (const unsigned short *)(self + TEX_ICONS);
    int x0 = row[0] + 1 + (icon[3] - icon[2]) + 4, x1 = row[1] - 5;
    int y1 = row[3] - 3, y0 = top[3] + 1;
    if (y1 - y0 > 4)
        y0 = y1 - 4;
    add_solid(x0, x1, y0, y1, 0xFF101010); /* the track */
    int lit = used < places ? used : places;
    if (places > 0 && places <= 16) { /* a segment per place, a pixel apart */
        for (int i = 0; i < places; i++) {
            int s0 = x0 + i * (x1 - x0 + 1) / places, s1 = x0 + (i + 1) * (x1 - x0 + 1) / places - 1;
            add_solid(s0, s1, y0, y1, i < lit ? 0xFF5FC45F : 0xFF3A3A3A);
        }
    } else if (places > 16) {
        add_solid(x0, x1, y0, y1, 0xFF3A3A3A);
        add_solid(x0, x0 + (x1 - x0) * lit / places, y0, y1, 0xFF5FC45F);
    }

    if (!places && !used)
        return;
    char *rb = *(char **)(leader + LD_RES);
    char *game = *(char **)VA(VA_GAME_REF);
    int capped = rb && (*(unsigned *)(rb + 0x4C + 4 * res) ^ 0x8932) && game && !(game[0x844] & 8);
    char a[16], b[16];
    wsprintfA(a, "%d", used);
    wsprintfA(b, "%d", places);
    int fh = *(short *)VA(VA_FONT_TOP + 2) - *(short *)VA(VA_FONT_TOP);
    int x = top[1] - 6 - text_width(a) - text_width("/") - text_width(b), y = top[2] + (top[3] - top[2] - fh) / 2;
    x = write_plain(a, x, y, (const unsigned *)VA(capped ? VA_BLACK : VA_WHITE), 0);
    x = separator(x, y);
    write_plain(b, x, y, (const unsigned *)VA(capped ? VA_BLACK : VA_GREY), 0);
}

static void __attribute__((thiscall)) rp_draw_resource(char *self, int res, const short *rect)
{
    int th = text_height();
    if (!bars_shown(self) || g_snap_for != self || rect[3] - rect[2] <= th) {
        ((draw_res_t)g_orig_draw_resource)(self, res, rect);
        return;
    }
    short top[4] = { rect[0], rect[1], rect[2], (short)(rect[2] + th) };
    memcpy(g_rp_back_rect, rect, sizeof g_rp_back_rect);
    g_rp_back_full = 1; /* the plate under the whole row; icon and numbers in its top part */
    ((draw_res_t)g_orig_draw_resource)(self, res, top);
    g_rp_back_full = 0;
    draw_workers(self, res, rect, top);
}

/* The "^^^" button (id 1), the workers' button, which is the bars' handle (id 2), and the closed
   market's flat bar (id 0): the "^^^" plate stretched to the whole button (the game draws it at
   its picture's size, leaving black beside it), three arrows: up (fold / hide), down (open /
   show, the market). The open market keeps the game's tall tab. */
static int __attribute__((thiscall)) rp_btn_redraw(char *self, char *btn, int id)
{
    if (id > 2 || (id == 0 && (self[RS_STATE] & 3) == 3))
        return ((btn_redraw_t)g_orig_btn_redraw)(self, btn, id);
    int tex = id == 0 ? TEX_DOWN /* the market opens and the counters go down under it */
                      : (id == 1 ? (self[RS_STATE] & 2) != 0 : bars_shown(self)) ? TEX_UP : TEX_DOWN;
    short *wr = (short *)(btn + WIN_RECT);
    int w = wr[1] - wr[0], h = wr[3] - wr[2];
    unsigned short plate[8];
    memcpy(plate, self + TEX_PLATE, sizeof plate);
    if (*(int *)(btn + BTN_FLAGS) & 1) { /* pressed: the next picture right of it, as the "^^^" button */
        int step = plate[3] - plate[2] + 3;
        plate[2] += step;
        plate[3] += step;
    }
    short full[4] = { 0, (short)w, 0, (short)h };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), plate, full, 0xFFFFFFFF, 1);
    const unsigned short *arrow = (const unsigned short *)(self + tex);
    int aw = arrow[3] - arrow[2], ah = arrow[5] - arrow[4];
    int x = (w - 3 * aw - 4) / 2, y = (h - ah) / 2;
    for (int i = 0; i < 3; i++, x += aw + 2) {
        short a[4] = { (short)x, (short)(x + aw), (short)y, (short)(y + ah) };
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), arrow, a, 0xFFFFFFFF, 1);
    }
    return 1;
}

/* The Buy / Sell buttons: their picture (ends at its own height, the middle tiled) stretched to
   the whole button, which is as tall as the row (a strip of the middle showed under it, without
   its ends), and their price in ui-theme's digits (hud.c's zone). */
static int __attribute__((thiscall)) rp_market_button(char *self, char *btn, int id)
{
    short *wr = (short *)(btn + WIN_RECT);
    g_rp_mb_h = wr[3] - wr[2];
    g_rp_mb_hf = (float)g_rp_mb_h;
    g_hud_zone++;
    int done = ((btn_redraw_t)g_orig_market_button)(self, btn, id);
    g_hud_zone--;
    g_rp_mb_h = 0;
    return done;
}

typedef void(__attribute__((thiscall)) *tile_t)(void *rm, const void *tex, const short *rect, int seed, unsigned color,
                                                 int type);

/* instead of its tile_random: the middle stretched over the button's whole height */
static void __attribute__((thiscall)) rp_mb_middle(void *rm, const void *tex, const short *rect, int seed,
                                                   unsigned color, int type)
{
    (void)seed;
    short r[4] = { rect[0], rect[1], 0, (short)g_rp_mb_h };
    ((add_rect_t)VA(VA_RM_ADD_RECT))(rm, tex, r, color, type);
}

void rp_mb_end(void);
void rp_row_back(void);
void rp_infra_frame(void);
void rp_infra_rows(void);
void rp_market_title(void);

__asm__(".intel_syntax noprefix\n"
        /* draw_market_button's add_smooth of an end: from the top (y in xmm3), as tall as the button
           ([esp+12] = h after the return address, the texture and w) */
        ".globl _rp_mb_end\n_rp_mb_end:\n"
        "    cmp dword ptr [_g_rp_mb_h], 0\n"
        "    je 1f\n"
        "    xorps xmm3, xmm3\n"
        "    push eax\n"
        "    mov eax, dword ptr [_g_rp_mb_hf]\n"
        "    mov dword ptr [esp+16], eax\n"
        "    pop eax\n"
        "1:  jmp dword ptr [_g_rp_add_smooth]\n"
        /* replaces draw_resource's `call draw_back`: [esp+4] = the rect, swapped for the whole row */
        ".globl _rp_row_back\n_rp_row_back:\n"
        "    cmp dword ptr [_g_rp_back_full], 0\n"
        "    je 1f\n"
        "    mov dword ptr [esp+4], offset _g_rp_back_rect\n"
        "1:  jmp dword ptr [_g_rp_draw_back]\n"
        /* draw_infra_area, after the "resources open" test: straight to the counting */
        ".globl _rp_infra_frame\n_rp_infra_frame:\n"
        "    jmp dword ptr [_g_rp_infra_count]\n"
        /* after the counting: the end instead of the column's rows */
        ".globl _rp_infra_rows\n_rp_infra_rows:\n"
        "    jmp dword ptr [_g_rp_infra_end]\n"
        /* draw_market_area: the titles' rect moved with the market (ebx+8 = the market's rect) */
        ".globl _rp_market_title\n_rp_market_title:\n"
        "    mov eax, dword ptr [ebx+8]\n"
        "    movsx edx, word ptr [eax]\n"
        "    movzx ecx, word ptr [edi+0x4C6]\n"
        "    add ecx, edx\n"
        "    shl ecx, 16\n"
        "    movzx eax, word ptr [edi+0x4C4]\n"
        "    add eax, edx\n"
        "    movzx eax, ax\n"
        "    or ecx, eax\n"
        "    mov eax, dword ptr [edi+0x4C8]\n"
        "    jmp dword ptr [_g_rp_market_title_go]\n"
        ".att_syntax prefix\n");

/* a jmp at `at` over `len` bytes (the rest nops) */
static void patch_jmp(UINT_PTR at, const void *to, int len)
{
    DWORD old;
    unsigned char *p = (unsigned char *)at;
    VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    *(int *)(p + 1) = (int)((UINT_PTR)to - (at + 5));
    memset(p + 5, 0x90, len - 5);
    VirtualProtect(p, len, old, &old);
}

/* an E8 call at `at` sent to `to` */
static void patch_call(UINT_PTR at, const void *to)
{
    DWORD old;
    VirtualProtect((void *)at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(int *)(at + 1) = (int)((UINT_PTR)to - (at + 5));
    VirtualProtect((void *)at, 5, old, &old);
}

void respanel_install(void)
{
    if (!g_on)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[12];
        int len;
    } sites[] = {
        { "adjust_rects", VA_ADJUST_RECTS, { 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0xB7, 0x78, 0x04, 0x00, 0x00 }, 10 },
        { "adjust_buttons", VA_ADJUST_BUTTONS, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, 6 },
        { "draw_resource_area", VA_RES_AREA, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20 }, 6 },
        { "draw_resource", VA_DRAW_RESOURCE, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5 },
        { "on_button_redraw", VA_BTN_REDRAW, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 }, 6 },
        { "draw_cap_area", VA_CAP_AREA, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, 6 },
        { "row plate", AT_ROW_BACK, { 0xE8, 0x5D, 0x1F, 0x00, 0x00 }, 5 },
        { "resource slash", AT_RES_SLASH, { 0xE8, 0xCB, 0x6A, 0xD0, 0xFF }, 5 },
        { "population text", AT_POP_TEXT, { 0xE8, 0x2F, 0xF3, 0xD0, 0xFF }, 5 },
        { "cities text", AT_CITY_TEXT, { 0xE8, 0xF2, 0xF6, 0xD0, 0xFF }, 5 },
        { "counters frame", AT_CAP_BORDER, { 0xE8, 0x60, 0xCE, 0xD0, 0xFF }, 5 },
        { "population divider", AT_POP_DIVIDER, { 0xE8, 0x86, 0xCD, 0xD0, 0xFF }, 5 },
        { "draw_market_button", VA_MARKET_BUTTON, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5 },
        { "market button left", AT_MB_LEFT, { 0xE8, 0xAB, 0x39, 0xD0, 0xFF }, 5 },
        { "market button right", AT_MB_RIGHT, { 0xE8, 0x07, 0x39, 0xD0, 0xFF }, 5 },
        { "market button middle", AT_MB_MIDDLE, { 0xE8, 0x24, 0x72, 0xD0, 0xFF }, 5 },
        { "workers frame", AT_INFRA_FRAME, { 0x8B, 0x8F, 0x84, 0x01, 0x00, 0x00 }, 6 },
        { "workers rows", AT_INFRA_ROWS, { 0x33, 0xDB, 0xBE, 0xAC, 0x19, 0x00, 0x00 }, 7 },
        { "market titles", AT_MARKET_TITLE,
          { 0x8B, 0x8F, 0xC4, 0x04, 0x00, 0x00, 0x8B, 0x87, 0xC8, 0x04, 0x00, 0x00 }, 12 },
    };
    for (int i = 0; i < (int)(sizeof sites / sizeof sites[0]); i++)
        if (memcmp((void *)VA(sites[i].va), sites[i].sig, sites[i].len) != 0) {
            logf_("respanel: signature mismatch at %s, the resource panel keeps its layout", sites[i].name);
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    g_rp_draw_back = (void *)VA(VA_DRAW_BACK);
    g_rp_infra_count = (void *)VA(AT_INFRA_COUNT);
    g_rp_infra_end = (void *)VA(AT_INFRA_END);
    g_rp_market_title_go = (void *)VA(AT_MARKET_TITLE_GO);

    static const struct {
        UINT_PTR va;
        int steal;
        void *hook;
        void **orig;
    } hooks[] = {
        { VA_ADJUST_RECTS, 10, (void *)rp_adjust_rects, &g_orig_adjust_rects },
        { VA_ADJUST_BUTTONS, 6, (void *)rp_adjust_buttons, &g_orig_adjust_buttons },
        { VA_RES_AREA, 6, (void *)rp_res_area, &g_orig_res_area },
        { VA_DRAW_RESOURCE, 5, (void *)rp_draw_resource, &g_orig_draw_resource },
        { VA_BTN_REDRAW, 6, (void *)rp_btn_redraw, &g_orig_btn_redraw },
        { VA_CAP_AREA, 6, (void *)rp_cap_area, &g_orig_cap_area },
        { VA_MARKET_BUTTON, 5, (void *)rp_market_button, &g_orig_market_button },
    };
    for (int i = 0; i < (int)(sizeof hooks / sizeof hooks[0]); i++) {
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    patch_call(VA(AT_ROW_BACK), rp_row_back);
    patch_call(VA(AT_RES_SLASH), rp_write_sep);
    patch_call(VA(AT_POP_TEXT), rp_write_counter);
    patch_call(VA(AT_CITY_TEXT), rp_write_counter);
    patch_call(VA(AT_CAP_BORDER), rp_cap_border);
    patch_call(VA(AT_POP_DIVIDER), rp_pop_divider);
    g_rp_add_smooth = (void *)VA(VA_ADD_SMOOTH);
    patch_call(VA(AT_MB_LEFT), rp_mb_end);
    patch_call(VA(AT_MB_RIGHT), rp_mb_end);
    patch_call(VA(AT_MB_MIDDLE), rp_mb_middle);
    patch_jmp(VA(AT_INFRA_FRAME), rp_infra_frame, 6);
    patch_jmp(VA(AT_INFRA_ROWS), rp_infra_rows, 7);
    patch_jmp(VA(AT_MARKET_TITLE), rp_market_title, 12);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("respanel: worker bars under the resources, market on the right");
}
