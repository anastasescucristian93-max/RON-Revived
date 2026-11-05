/*
 * The selection panel while the bottom interface is folded (the user, 2026-11-01): the full panel
 * of the unfolded interface (name, hit points, attack, armor, range..., the production queue)
 * instead of the game's one-line strip; for several objects the unfolded grid (one cell per kind
 * with its count: the "group selection" patches of ron_camera.c), six cells wide, on the command
 * panel's empty cells, in the selection panel's frame with the command panel's stone frieze and
 * end cap, its last row on the screen's edge, its top row (0.7 of a cell) kept for the selection's
 * kills (kills.c draws the skull and the number there).
 *
 *  - The selection panel's code asks the interface whether it is unfolded (byte
 *    IFaceMainBase + 0x470) in eleven places: IFaceSelected's on_redraw, check_mouse_accept,
 *    update_selected, update_queue, update_carrier_queue, update_progress's
 *    setup_progress_bar_and_button, draw_progress, draw_build_text; IFaceGarrison's
 *    update_visibility; twice IFaceMain::position_windows (the sizes and places of the panels in
 *    right_window). Each read becomes a read of g_sel_unfolded, always 1. The interface itself
 *    still folds: IFaceMainBase::update_visibility puts right_window next to the command
 *    buttons, and position_windows then lays the panels out in it as unfolded.
 *  - Folded, update_visibility puts right_window right at the command panel's edge and makes it
 *    as wide as the old minimap frame leaves (`mov eax, [ebp-4]; movsx eax, word ptr [eax+2]`, the
 *    command panel's right edge, at 0x008121A8 for the width and 0x008121C2 for the x). Both add
 *    folded_panel_gap; for several objects selected the width becomes six grid cells.
 *  - position_windows puts a group's IFaceSelected 4 units into right_window (0x008092ED: push 4,
 *    push 4, Window::move); folded, the y puts the grid's last row on the screen's bottom edge, as
 *    the command buttons beside it, so the panel's top sits lower than theirs.
 *  - The kills row: update_selected keeps the grid's top row for the group's name only when the
 *    group has one (SelectGroup + 0x9D8 != -1, cmp at 0x00823AAE, je at 0x00823AB6); the je goes,
 *    so every multiple selection keeps it, for the kills. The grid then moves down a cell and a
 *    spacing (0x00823B05, 15 bytes): a call moves it 0.7 of a cell instead and ends the row there.
 * Interface only, nothing reaches the simulation. Engine notes: ENGINE_NOTES.md, "Selection
 * panel while folded".
 */
#include "ron.h"
#include <string.h>

#define VA_IFACE_PTR      0x00E39964u
#define VA_CONSOLE_PTR    0x00C06210u /* +0x2A0 the local selection */
#define VA_SELECTIONS     0x00E8D444u /* SelectGroup[], 0x9F0 each: +0xC count */
#define VA_GRAPHSYS_PTR   0x00E335A0u /* +0x164 the interface's depth step */
#define VA_RM_ADD_RECT    0x0051F3D0u
#define VA_DRAW_BORDER    0x0051D260u /* IFaceRenderManager::draw_border(Rect *, corners, edges, extras, Color) */
#define VA_IFACE_RM       0x00EBD1C0u
#define VA_WHITE          0x00C8D26Cu
#define VA_INT_STRS       0x00C06378u
#define STR_MISC_BORDER   0x45C4      /* "misc_border", IFACESELECTED's frame */
#define VA_SEL_REDRAW     0x00826420u /* IFaceSelected::on_redraw */
#define VA_SEL_RESERVE    0x00823AAEu /* update_selected: cmp dword [eax+esi+0x9D8], -1 / je */
#define VA_GAP_WIDTH      0x008121A8u /* IFaceMainBase::update_visibility, folded: right_window's width */
#define VA_GAP_X          0x008121C2u /* ... and its x */
#define VA_GROUP_MOVE     0x008092EDu /* position_windows, several selected: push 4 / push 4 / lea ecx / call move */
#define VA_WINDOW_MOVE    0x00A54080u /* Window::move(x, y), ret 8 */
#define VA_SINGLE_RESIZE  0x0080935Cu /* position_windows, one object: IFaceSelected's resize to "main" */
#define IFB_RIGHT_WINDOW  0xD64       /* IFaceMainBase: right_window, the panels' frame */
#define IFACE_BOTTOM_GAP  3           /* right_window ends 3 units over the interface's (the screen's) bottom */
#define VA_KILLS_ROW_H    0x00823B05u /* update_selected: mov ax, [edx+6] / sub ax, [edx+4] / cwde / add eax, [edi+0x4D8] */
#define KILLS_ROW(ch)     (((ch) * 179) >> 8) /* the kills row: 0.7 of a cell (the user, 2026-11-01), as hk_sel_kills_h */
#define IFB_UNFOLDED      0x470
#define IFM_SELECTED      0x8D1C      /* IFaceMain: the IFaceSelected */
#define SEL_MULTI         0x470       /* IFaceSelected: dword, several objects selected */
#define SEL_COLS          0x484       /* the group grid: columns, rows */
#define SEL_ROWS          0x488
#define SEL_CELL          0x48C       /* short l r t b: its first cell */
#define SEL_SPACE_X       0x4D4
#define SEL_SPACE_Y       0x4D8
#define WIN_RECT          0x126
#define VT_FIND_BORDER    0x494       /* GraphicWinXML: IFACEBORDER by name (name, id, int *) */
#define GRID_COLS         6           /* the user, 2026-11-01 */
#define FRAME_OUT         4           /* IFaceSelected sits 4 units inside right_window */

typedef void(__attribute__((thiscall)) *add_rect_t)(void *rm, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef void(__attribute__((thiscall)) *border_t)(void *rm, const short *rect, const void *corners, const void *edges,
                                                   const void *extras, unsigned c0, unsigned c1, unsigned c2);
typedef void *(__attribute__((thiscall)) *find_border_t)(void *self, const void *name, int id, int *out);

static int g_on;
int g_sel_gap;      /* interface units between the command buttons and the panel, folded */
int g_sel_kills_row; /* the top row is kept for the kills (kills.c) */
unsigned char g_sel_unfolded = 1; /* what the patched reads see */
int g_sel_tmp, g_sel_left, g_sel_right, g_sel_frame;
UINT_PTR g_sel_window_move;
void *g_orig_sel_redraw;

void selpanel_load_config(const char *ini_path)
{
    g_on = GetPrivateProfileIntA("ui", "folded_selection_panel", 1, ini_path);
    g_sel_gap = GetPrivateProfileIntA("ui", "folded_panel_gap", 12, ini_path);
}

static void write_code(unsigned char *at, const unsigned char *bytes, int n)
{
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
}

/* ---- the unfolded reads ------------------------------------------------------------------ */

static const struct {
    UINT32 va;
    int cmp; /* 1: cmp byte [reg+0x470], 0 (7 bytes); 0: mov al, byte [eax+0x470] (6 bytes) */
    unsigned char reg; /* ModRM byte of the original */
} g_reads[] = {
    { 0x0081F125u, 0, 0x80 }, /* draw_build_text */
    { 0x008230A2u, 0, 0x80 }, /* draw_progress */
    { 0x008237A2u, 1, 0xB8 }, /* update_selected */
    { 0x00824250u, 0, 0x80 }, /* update_queue */
    { 0x00824AA4u, 0, 0x80 }, /* update_carrier_queue */
    { 0x00825C88u, 0, 0x80 }, /* setup_progress_bar_and_button */
    { 0x00826447u, 0, 0x80 }, /* on_redraw */
    { 0x00826D42u, 1, 0xB8 }, /* check_mouse_accept */
    { 0x00806182u, 0, 0x80 }, /* IFaceGarrison::update_visibility */
    { 0x00809303u, 1, 0xBF }, /* IFaceMain::position_windows (edi) */
    { 0x00809450u, 1, 0xBF },
};

static int patch_reads(void)
{
    const int n = sizeof g_reads / sizeof g_reads[0];
    for (int i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)VA(g_reads[i].va);
        unsigned char want[7] = { g_reads[i].cmp ? 0x80 : 0x8A, g_reads[i].reg, 0x70, 0x04, 0, 0, 0 };
        if (memcmp(at, want, g_reads[i].cmp ? 7 : 6)) {
            logf_("selpanel: signature mismatch at %08lX, the folded panel stays the game's", (unsigned long)g_reads[i].va);
            return 0;
        }
    }
    UINT32 g = (UINT32)(UINT_PTR)&g_sel_unfolded;
    for (int i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)VA(g_reads[i].va);
        unsigned char b[7];
        if (g_reads[i].cmp) { /* cmp byte ptr [g], 0 */
            b[0] = 0x80, b[1] = 0x3D;
            memcpy(b + 2, &g, 4);
            b[6] = 0;
            write_code(at, b, 7);
        } else { /* mov al, byte ptr [g]; nop */
            b[0] = 0xA0;
            memcpy(b + 1, &g, 4);
            b[5] = 0x90;
            write_code(at, b, 6);
        }
    }
    return 1;
}

/* ---- the kills row ---------------------------------------------------------------------- */

static int patch_kills_row(void)
{
    static const unsigned char sig[10] = { 0x83, 0xBC, 0x30, 0xD8, 0x09, 0x00, 0x00, 0xFF, 0x74, 0x6A };
    unsigned char *at = (unsigned char *)VA(VA_SEL_RESERVE);
    if (memcmp(at, sig, sizeof sig)) {
        logf_("selpanel: signature mismatch at update_selected, no kills row for unnamed groups");
        return 0;
    }
    static const unsigned char nop2[2] = { 0x90, 0x90 };
    write_code(at + 8, nop2, 2);
    g_sel_kills_row = 1;
    return 1;
}

/* ---- the folded right_window: a gap, six cells for a group -------------------------------- */

static char *selected_panel(void)
{
    char *iface = *(char **)VA(VA_IFACE_PTR);
    return iface ? iface + IFM_SELECTED : NULL;
}

static int several_selected(void)
{
    char *console = *(char **)VA(VA_CONSOLE_PTR), *sel = *(char **)VA(VA_SELECTIONS);
    return console && sel && *(int *)(sel + *(int *)(console + 0x2A0) * 0x9F0 + 0xC) > 1;
}

/* The grid's cell size and spacing, as update_selected last set them (the "object" button,
   60 x 35, 2 apart, before the first group). */
static void cell_size(char *sel, int *cw, int *ch, int *sx, int *sy)
{
    short *c = (short *)(sel + SEL_CELL);
    *cw = c[1] - c[0], *ch = c[3] - c[2];
    *sx = *(int *)(sel + SEL_SPACE_X), *sy = *(int *)(sel + SEL_SPACE_Y);
    if (*cw < 16 || *cw > 160 || *ch < 10 || *ch > 120)
        *cw = 60, *ch = 35;
    if (*sx < 0 || *sx > 16)
        *sx = 2;
    if (*sy < 0 || *sy > 16)
        *sy = 2;
}

/* update_visibility's width site: the command panel's right edge (+ the gap) that leaves
   right_window GRID_COLS cells wide when several objects are selected; right_window's width is
   then g_sel_right - this - g_sel_frame (the minimap frame's width). */
__attribute__((used)) int c_sel_width_left(void)
{
    int left = g_sel_left;
    char *sel = selected_panel();
    if (!sel || !several_selected())
        return left;
    int cw, ch, sx, sy;
    cell_size(sel, &cw, &ch, &sx, &sy);
    int want = GRID_COLS * cw + (GRID_COLS - 1) * sx + 2 * FRAME_OUT;
    int alt = g_sel_right - g_sel_frame - want;
    return alt > left ? alt : left;
}

/* position_windows, several objects selected: IFaceSelected's y in right_window. Unfolded the
   game's 4; folded so many rows of cells as fit end on the screen's bottom edge (the user,
   2026-11-01: the grid starts at the bottom, like the command buttons, and the panel is lower than
   them). */
__attribute__((used)) int c_sel_group_y(void)
{
    char *iface = *(char **)VA(VA_IFACE_PTR);
    char *sel = selected_panel();
    if (!iface || !sel || iface[IFB_UNFOLDED])
        return FRAME_OUT;
    short *rw = (short *)(iface + IFB_RIGHT_WINDOW + WIN_RECT);
    int ph = rw[3] - rw[2], h = ph - 2 * FRAME_OUT;
    int cw, ch, sx, sy;
    cell_size(sel, &cw, &ch, &sx, &sy);
    int rows = (h + sy) / (ch + sy);
    int used = KILLS_ROW(ch) + (rows - 1) * (ch + sy); /* the kills row, then rows - 1 of cells */
    int y = ph + IFACE_BOTTOM_GAP - used;
    return rows > 0 && y > FRAME_OUT && y < ph ? y : FRAME_OUT;
}

/* position_windows, one object selected: it only resized IFaceSelected (to its "main" rect) and
   left it where the last group put it, so after a group the panel stayed that much lower and its
   bottom rows went under the screen's edge (the user, 2026-11-02: a unit, worse for a Redoubt).
   Now it is moved too: unfolded the game's 4; folded so the stats' last row (y 145..162,
   interface.xml stat_short) ends on the screen's bottom edge when right_window is lower than
   that. */
#define SINGLE_BOTTOM 162

__attribute__((used)) int c_sel_single_y(void)
{
    char *iface = *(char **)VA(VA_IFACE_PTR);
    if (!iface || iface[IFB_UNFOLDED])
        return FRAME_OUT;
    short *rw = (short *)(iface + IFB_RIGHT_WINDOW + WIN_RECT);
    int y = rw[3] - rw[2] + IFACE_BOTTOM_GAP - SINGLE_BOTTOM;
    return y < FRAME_OUT ? y : FRAME_OUT;
}

void hk_sel_gap(void);
void hk_sel_width(void);
void hk_sel_move(void);
void hk_sel_single(void);
void hk_sel_kills_h(void);

/* position_windows' move of a group's IFaceSelected, and update_selected's shift of the grid
   under the kills row, each become a call */
static int patch_group_layout(void)
{
    static const unsigned char move_sig[11] = { 0x6A, 0x04, 0x6A, 0x04, 0x8D, 0x8F, 0x1C, 0x8D, 0x00, 0x00, 0xE8 };
    static const unsigned char row_sig[15] = { 0x66, 0x8B, 0x42, 0x06, 0x66, 0x2B, 0x42, 0x04, 0x98,
                                               0x03, 0x87, 0xD8, 0x04, 0x00, 0x00 };
    unsigned char *mv = (unsigned char *)VA(VA_GROUP_MOVE), *row = (unsigned char *)VA(VA_KILLS_ROW_H);
    if (memcmp(mv, move_sig, sizeof move_sig) || (UINT_PTR)(mv + 15 + *(INT32 *)(mv + 11)) != VA(VA_WINDOW_MOVE) ||
        memcmp(row, row_sig, sizeof row_sig)) {
        logf_("selpanel: signature mismatch at the group layout, the grid keeps the game's place");
        return 0;
    }
    g_sel_window_move = VA(VA_WINDOW_MOVE);
    unsigned char b[15];
    memset(b, 0x90, sizeof b);
    b[0] = 0xE8;
    INT32 rel = (INT32)((UINT_PTR)hk_sel_move - (UINT_PTR)(mv + 5));
    memcpy(b + 1, &rel, 4);
    write_code(mv, b, 15);
    memset(b, 0x90, sizeof b);
    b[0] = 0xE8;
    rel = (INT32)((UINT_PTR)hk_sel_kills_h - (UINT_PTR)(row + 5));
    memcpy(b + 1, &rel, 4);
    write_code(row, b, 15);
    return 1;
}

/* position_windows, one object: push 0 / push 0 / movsx edx, dx / push edx / cwde / push eax /
   call [esi+0x20] (13 bytes) becomes a call of hk_sel_single */
static int patch_single_move(void)
{
    static const unsigned char sig[13] = { 0x6A, 0x00, 0x6A, 0x00, 0x0F, 0xBF, 0xD2, 0x52, 0x98, 0x50, 0xFF, 0x56, 0x20 };
    unsigned char *at = (unsigned char *)VA(VA_SINGLE_RESIZE);
    if (memcmp(at, sig, sizeof sig)) {
        logf_("selpanel: signature mismatch at the single panel's resize, it keeps the game's place");
        return 0;
    }
    g_sel_window_move = VA(VA_WINDOW_MOVE);
    unsigned char b[13];
    memset(b, 0x90, sizeof b);
    b[0] = 0xE8;
    INT32 rel = (INT32)((UINT_PTR)hk_sel_single - (UINT_PTR)(at + 5));
    memcpy(b + 1, &rel, 4);
    write_code(at, b, 13);
    return 1;
}
void hk_sel_redraw(void);

__asm__(".intel_syntax noprefix\n"
        /* IFaceMainBase::update_visibility, folded, x site: eax = the command panel's right edge +
           the gap (the original `mov eax, [ebp-4]; movsx eax, word ptr [eax+2]`) */
        ".globl _hk_sel_gap\n"
        "_hk_sel_gap:\n"
        "    mov eax, [ebp-4]\n"
        "    movsx eax, word ptr [eax+2]\n"
        "    add eax, [_g_sel_gap]\n"
        "    ret\n"
        /* the width site (ecx = right_window's right edge, [ebp-8] the minimap frame's width) */
        ".globl _hk_sel_width\n"
        "_hk_sel_width:\n"
        "    mov eax, [ebp-4]\n"
        "    movsx eax, word ptr [eax+2]\n"
        "    add eax, [_g_sel_gap]\n"
        "    mov [_g_sel_left], eax\n"
        "    mov [_g_sel_right], ecx\n"
        "    mov eax, [ebp-8]\n"
        "    mov [_g_sel_frame], eax\n" HOOK_ENTER
        "    call _c_sel_width_left\n"
        "    mov [_g_sel_tmp], eax\n" HOOK_LEAVE
        "    mov eax, [_g_sel_tmp]\n"
        "    ret\n"
        /* position_windows: move(4, y) of a group's IFaceSelected (edi = IFaceMain) */
        ".globl _hk_sel_move\n"
        "_hk_sel_move:\n" HOOK_ENTER
        "    call _c_sel_group_y\n"
        "    mov [_g_sel_tmp], eax\n" HOOK_LEAVE
        "    push dword ptr [_g_sel_tmp]\n"
        "    push 4\n"
        "    lea ecx, [edi+0x8D1C]\n"
        "    call dword ptr [_g_sel_window_move]\n"
        "    ret\n"
        /* position_windows, one object: the replaced resize(w, h, 0, 0) (ecx = IFaceSelected,
           esi its vtable, ax the width, dx the height), then move(4, y) */
        ".globl _hk_sel_single\n"
        "_hk_sel_single:\n"
        "    movsx edx, dx\n"
        "    cwde\n"
        "    push 0\n"
        "    push 0\n"
        "    push edx\n"
        "    push eax\n"
        "    call dword ptr [esi+0x20]\n" HOOK_ENTER
        "    call _c_sel_single_y\n"
        "    mov [_g_sel_tmp], eax\n" HOOK_LEAVE
        "    push dword ptr [_g_sel_tmp]\n"
        "    push 4\n"
        "    lea ecx, [edi+0x8D1C]\n"
        "    call dword ptr [_g_sel_window_move]\n"
        "    ret\n"
        /* update_selected, the kills row kept: eax = how far the grid moves down (the original:
           the cell's height + the spacing). The row is 0.7 of a cell (179 / 256) and the name
           rect +0x4E4 that kills.c draws in ends there (edi = IFaceSelected, edx = the cell). */
        ".globl _hk_sel_kills_h\n"
        "_hk_sel_kills_h:\n"
        "    mov ax, word ptr [edx+6]\n"
        "    sub ax, word ptr [edx+4]\n"
        "    cwde\n"
        "    imul eax, eax, 179\n"
        "    sar eax, 8\n"
        "    push ecx\n"
        "    mov cx, word ptr [edi+0x4E8]\n"
        "    add cx, ax\n"
        "    mov word ptr [edi+0x4EA], cx\n"
        "    pop ecx\n"
        "    add eax, dword ptr [edi+0x4D8]\n"
        "    ret\n"
        /* IFaceSelected::on_redraw: the folded grid's background first */
        ".globl _hk_sel_redraw\n"
        "_hk_sel_redraw:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_sel_redraw\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_sel_redraw]\n"
        ".att_syntax prefix\n");

static int patch_gap(void)
{
    static const UINT32 at[2] = { VA_GAP_WIDTH, VA_GAP_X };
    void *const to[2] = { (void *)hk_sel_width, (void *)hk_sel_gap };
    static const unsigned char sig[7] = { 0x8B, 0x45, 0xFC, 0x0F, 0xBF, 0x40, 0x02 };
    for (int i = 0; i < 2; i++)
        if (memcmp((void *)VA(at[i]), sig, sizeof sig)) {
            logf_("selpanel: signature mismatch at %08lX, the folded panel keeps the game's width", (unsigned long)at[i]);
            return 0;
        }
    for (int i = 0; i < 2; i++) {
        unsigned char *p = (unsigned char *)VA(at[i]);
        unsigned char b[7] = { 0xE8, 0, 0, 0, 0, 0x90, 0x90 };
        INT32 rel = (INT32)((UINT_PTR)to[i] - (UINT_PTR)(p + 5));
        memcpy(b + 1, &rel, 4);
        write_code(p, b, 7);
    }
    return 1;
}

/* ---- the background and the frame ----------------------------------------------------------- */

/* `tex` repeated over rect at one texel per interface unit, the last row and column cut short
   (their texture rect too, so nothing is squeezed). */
static void tile(void *rm, const unsigned char *tex, const short *rect)
{
    int tw = *(short *)(tex + 6) - *(short *)(tex + 4), th = *(short *)(tex + 10) - *(short *)(tex + 8);
    if (tw <= 0 || th <= 0)
        return;
    static _Alignas(16) unsigned char part[16];
    for (int y = rect[2]; y < rect[3]; y += th)
        for (int x = rect[0]; x < rect[1]; x += tw) {
            int w = rect[1] - x < tw ? rect[1] - x : tw, h = rect[3] - y < th ? rect[3] - y : th;
            memcpy(part, tex, 16);
            *(short *)(part + 6) = (short)(*(short *)(tex + 4) + w);
            *(short *)(part + 10) = (short)(*(short *)(tex + 8) + h);
            short r[4] = { (short)x, (short)(x + w), (short)y, (short)(y + h) };
            ((add_rect_t)VA(VA_RM_ADD_RECT))(rm, part, r, 0xFFFFFFFF, 1);
        }
}

/* Several objects selected, interface folded: the interface's own background (IFACEMAINBASE
   "back", the unfolded bar's stone) under the whole grid and the kills row, down to the screen's
   edge, the command panel's empty cell (IFACETEX sel_blank) in every place of the game's grid
   where the kinds go, and the selection panel's frame (IFACEBORDER misc_border) round it all. */
__attribute__((used)) void c_sel_redraw(char *self)
{
    char *iface = *(char **)VA(VA_IFACE_PTR);
    char *con = *(char **)VA(VA_CONSOLE_PTR);
    if (!g_on || !iface || !con || iface[IFB_UNFOLDED] || !*(int *)(self + SEL_MULTI))
        return; /* unfolded, or one object: the game's own background */
    const unsigned char *back = qol_iface_tex("back");
    const void *cell = qol_iface_tex("sel_blank");
    if (!back)
        return;
    short *r = (short *)(self + WIN_RECT), *cr = (short *)(con + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2];
    int bottom = cr[3] - r[2]; /* the screen's bottom edge, in the panel's coordinates */
    if (bottom < h)
        bottom = h;
    int cw, ch, sx, sy;
    cell_size(self, &cw, &ch, &sx, &sy);
    short *c0 = (short *)(self + SEL_CELL);
    int cols = *(int *)(self + SEL_COLS), rows = *(int *)(self + SEL_ROWS);
    if (cols < 0 || cols > 32)
        cols = 0;
    if (rows < 0 || rows > 16)
        rows = 0;
    /* round the window and the grid, which may reach past it */
    int x0 = 0, x1 = w;
    if (cols) {
        if (c0[0] < x0)
            x0 = c0[0];
        if (c0[0] + cols * cw + (cols - 1) * sx > x1)
            x1 = c0[0] + cols * cw + (cols - 1) * sx;
    }
    short f[4] = { (short)(x0 - FRAME_OUT), (short)(x1 + FRAME_OUT), (short)-FRAME_OUT, (short)bottom };
    void *rm = (void *)VA(VA_IFACE_RM);
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS_PTR) + 0x164);
    *depth -= 2;
    tile(rm, back, f);
    (*depth)++;
    if (cell)
        for (int j = 0; j < rows; j++)
            for (int i = 0; i < cols; i++) {
                int x = c0[0] + i * (cw + sx), y = c0[2] + j * (ch + sy);
                short c[4] = { (short)x, (short)(x + cw), (short)y, (short)(y + ch) };
                ((add_rect_t)VA(VA_RM_ADD_RECT))(rm, cell, c, 0xFFFFFFFF, 1);
            }
    (*depth)++;
    char *strs = *(char **)(*(char **)VA(VA_INT_STRS) + 0x10);
    char *b = ((find_border_t)(*(void ***)self)[VT_FIND_BORDER / 4])(self, strs + STR_MISC_BORDER, -1, NULL);
    if (b) {
        const unsigned *white = (const unsigned *)VA(VA_WHITE);
        ((border_t)VA(VA_DRAW_BORDER))(rm, f, b, b + 0x40, b + 0x80, white[0], white[1], white[2]);
    }
    /* the stone edges of the command panel beside it (the user, 2026-11-01): on top its frieze
       (IFACEMAINBASE "bar", which the folded interface tiles over the command panel), on the
       right the upright stone it ends with ("streamlined_end_cap"), from the frieze's top down */
    const unsigned char *bar = qol_iface_tex("bar"), *cap = qol_iface_tex("streamlined_end_cap");
    int top = f[2];
    if (bar) {
        top = f[2] - (*(short *)(bar + 10) - *(short *)(bar + 8));
        short t[4] = { f[0], f[1], (short)top, f[2] };
        tile(rm, bar, t);
    }
    if (cap) {
        int cw2 = *(short *)(cap + 6) - *(short *)(cap + 4);
        short t[4] = { f[1], (short)(f[1] + cw2), (short)top, f[3] };
        tile(rm, cap, t);
    }
}

void selpanel_install(void)
{
    if (!g_on)
        return;
    if (!patch_reads())
        return;
    int row = patch_kills_row();
    if (row)
        patch_group_layout();
    patch_single_move();
    int gap = patch_gap();
    unsigned char *at = (unsigned char *)VA(VA_SEL_REDRAW);
    static const unsigned char sig[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (mem && !memcmp(at, sig, sizeof sig)) {
        g_orig_sel_redraw = hook_trampoline(at, 6, mem);
        hook_jmp5(at, hk_sel_redraw);
    } else {
        logf_("selpanel: signature mismatch at IFaceSelected::on_redraw, no background for the folded grid");
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("selpanel: the full selection panel while the interface is folded%s%s", row ? ", a kills row for every group" : "",
          gap ? ", 6 cells wide for a group" : "");
}
