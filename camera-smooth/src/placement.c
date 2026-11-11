/*
 * Placement helpers: Farms in a block and lines of Tank Traps. Engine notes:
 * camera-smooth/ENGINE_NOTES.md, "Farm snap" and "Tank Trap lines".
 *
 * Farm snap ([qol] farm_snap): placing Farms with Shift, the placement ghost jumps after each
 * Farm to the next free plot touching the ones placed, so a click without moving the mouse lays
 * the next one. The plots fill rings around the first Farm of the row (a square block, not a
 * long strip): ring 1 is the 8 plots around it, then ring 2, nearest first and, among equals,
 * the one nearest the Farm just placed. Moving the mouse gives the ghost back to the mouse.
 *
 * Tank Trap lines ([qol] tank_trap_lines): with Shift held, press, drag and let go: a line of
 * Tank Traps from where the drag started to where it ended, in any direction, diagonals too.
 * While dragging, every trap of the line shows as a ghost (red where it cannot stand). With
 * Shift still held, a drag that starts within 2 tiles of the last line's end goes on from that
 * end, so the line can turn. A line is "solid" ([qol] tank_trap_solid_lines = 1): every trap
 * touches the next by a side, so on a diagonal nothing slips between two traps.
 *
 * Options + 0xB0 is the type being placed (-1 none; it stays after a placement with Shift).
 * Options::mouse_move(x, y) puts the ghost there (BuildTypeData::snap_center into +0xB4 / +0xB8);
 * Options::picked_spot(x0, y0, x1, y1) is the click (on the button's release; x1 = y1 = -1 for
 * one spot) and lays the building through GroupOut::issue_build. A plot is free when
 * BuildTypeData::blocked_site says so and none of ours was placed there yet (the site appears a
 * turn later). The first trap of a line goes through picked_spot (its sound and messages); the
 * others are GroupOut::issue_build of the selection with QueuePos 1 (after the orders before
 * them, as Shift does), fed into the command package as long as it has room (0x200 bytes a
 * turn), the rest in the next frames. Only the interface changes: the buildings are laid by the
 * game's own build command, so multiplayer is fine.
 */
#include "ron.h"
#include <stdlib.h>
#include <string.h>

#define VA_OPTIONS      0x00C06204u /* Options * */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 the local player, +0x2A0 its selection */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each */
#define VA_BUILDTYPES   0x00C061F4u /* PtrArray<BuildType> *: +0x10 = BuildType * by TypeIndex */
#define VA_MOUSE_MOVE   0x007238C0u /* Options::mouse_move(Coord x, Coord y), ret 8 */
#define VA_PICKED_SPOT  0x00721C40u /* Options::picked_spot(x0, y0, x1, y1), ret 0x10 */
#define VA_SNAP_CENTER  0x00636190u /* BuildTypeData::snap_center(x, y, &x, &y, player) */
#define VA_BLOCKED_SITE 0x00636A50u /* BuildTypeData::blocked_site(x, y, player, -1, 0): 0 = fine */
#define VA_ISSUE_BUILD  0x00708C60u /* GroupOut::issue_build(x0, y0, x1, y1, type, QueuePos), ret 0x18 */
#define VA_PACKAGE      0x00E8FF88u /* CommandPackage: +0x10 short bytes used, max 0x200 */
#define VA_CMD_MANAGER  0x00E8FF60u
#define VA_CHECK_ACCEPT 0x00940A70u /* CommandManager::check_accept_issue() */

#define OPT_PLACING 0xB0
#define OPT_X       0xB4
#define OPT_Y       0xB8
#define OPT_Z       0xBC
#define FARM        0x1A1
#define TILE        192
#define STEP        (4 * TILE) /* a Farm is 4 x 4 tiles */
#define RINGS       6
#define MAX_ROW     64
#define MOVE_PX     6          /* this far, the mouse takes the ghost back */
#define MAX_LINE    96         /* traps in one line */
#define CHAIN_TILES 2
#define PKG_MAX     0x200
#define QUEUE_AFTER 1          /* QueuePos with Shift: after the orders before */

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);
typedef int(__attribute__((thiscall)) *fn5_t)(void *self, int a, int b, int c, int d, int e);
typedef int(__attribute__((thiscall)) *fn6_t)(void *self, int a, int b, int c, int d, int e, int f);

int g_farmsnap_enabled, g_ttline_enabled, g_place_extra;
static int g_ttline_solid = 1;
static void *g_orig_mouse_move, *g_orig_picked;

void farmsnap_load_config(const char *ini_path)
{
    g_farmsnap_enabled = GetPrivateProfileIntA("qol", "farm_snap", 1, ini_path);
    g_ttline_enabled = GetPrivateProfileIntA("qol", "tank_trap_lines", 1, ini_path);
    g_ttline_solid = GetPrivateProfileIntA("qol", "tank_trap_solid_lines", 1, ini_path);
}

static char *options(void) { return *(char **)VA(VA_OPTIONS); }
static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }
static void *buildtype(int type) { return (*(void ***)(*(char **)VA(VA_BUILDTYPES) + 0x10))[type]; }
static int placing(void) { return options() ? *(int *)(options() + OPT_PLACING) : -1; }
static int key_down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
static int left_down(void) { return key_down(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON); }

static char *local_selection(void)
{
    char *console = *(char **)VA(VA_CONSOLE), *sels = *(char **)VA(VA_SELECTIONS);
    return console && sels ? sels + *(int *)(console + 0x2A0) * 0x9F0 : NULL;
}

/* the snapped centre of a building of `type` standing on tile (tx, ty) */
static void tile_center(void *bt, int p, int tx, int ty, int *x, int *y)
{
    *x = tx * TILE + TILE / 2;
    *y = ty * TILE + TILE / 2;
    ((fn5_t)VA(VA_SNAP_CENTER))(bt, *x, *y, (int)x, (int)y, p);
}

static int site_ok(void *bt, int p, int x, int y) { return ((fn5_t)VA(VA_BLOCKED_SITE))(bt, x, y, p, -1, 0) == 0; }

/* ==== Farm snap ========================================================================= */

static struct {
    int x, y;
} g_row[MAX_ROW]; /* the Farms of this row: centres; [0] is the anchor */
static int g_row_n;
static int g_snap_on, g_snap_x, g_snap_y;
static POINT g_snap_mouse;

static int mouse_still(void)
{
    POINT pt;
    if (!GetCursorPos(&pt))
        return 0;
    return abs(pt.x - g_snap_mouse.x) <= MOVE_PX && abs(pt.y - g_snap_mouse.y) <= MOVE_PX;
}

static int in_row(int x, int y)
{
    for (int i = 0; i < g_row_n; i++)
        if (abs(g_row[i].x - x) < STEP && abs(g_row[i].y - y) < STEP)
            return 1;
    return 0;
}

/* touches a Farm of the row (shares a side or a corner) */
static int next_to_row(int x, int y)
{
    for (int i = 0; i < g_row_n; i++)
        if (abs(g_row[i].x - x) <= STEP + TILE / 2 && abs(g_row[i].y - y) <= STEP + TILE / 2)
            return 1;
    return 0;
}

static int free_plot(void *bt, int p, int x, int y)
{
    if (x < 0 || y < 0 || in_row(x, y))
        return 0;
    int sx = x, sy = y;
    ((fn5_t)VA(VA_SNAP_CENTER))(bt, x, y, (int)&sx, (int)&sy, p);
    return sx == x && sy == y && site_ok(bt, p, x, y);
}

/* The next plot of the row, or 0 when none is free within RINGS of the anchor. */
static int next_plot(void *bt, int p, int *ox, int *oy)
{
    int ax = g_row[0].x, ay = g_row[0].y, lx = g_row[g_row_n - 1].x, ly = g_row[g_row_n - 1].y;
    for (int r = 1; r <= RINGS; r++) {
        int best = 0;
        long long bd = 0, bl = 0;
        for (int i = -r; i <= r; i++)
            for (int j = -r; j <= r; j++) {
                if (abs(i) != r && abs(j) != r)
                    continue;
                int x = ax + i * STEP, y = ay + j * STEP;
                if (!next_to_row(x, y) || !free_plot(bt, p, x, y))
                    continue;
                long long d = (long long)i * i + (long long)j * j;
                long long dl = (long long)(x - lx) * (x - lx) + (long long)(y - ly) * (y - ly);
                if (!best || d < bd || (d == bd && dl < bl)) {
                    best = 1;
                    bd = d;
                    bl = dl;
                    *ox = x;
                    *oy = y;
                }
            }
        if (best)
            return 1;
    }
    return 0;
}

static int farm_picked(char *self, int *a, int p)
{
    if (g_snap_on && mouse_still()) {
        a[0] = g_snap_x;
        a[1] = g_snap_y;
    }
    g_snap_on = 0;
    void *bt = buildtype(FARM);
    int x = a[0], y = a[1];
    ((fn5_t)VA(VA_SNAP_CENTER))(bt, x, y, (int)&x, (int)&y, p);
    int ok = site_ok(bt, p, x, y) && !in_row(x, y);
    int r = ((fn4_t)g_orig_picked)(self, a[0], a[1], a[2], a[3]);
    if (!ok)
        return r;
    if (!g_row_n || !next_to_row(x, y))
        g_row_n = 0; /* a new row: this Farm is its anchor */
    if (g_row_n < MAX_ROW) {
        g_row[g_row_n].x = x;
        g_row[g_row_n].y = y;
        g_row_n++;
    }
    int nx, ny;
    if (placing() == FARM && next_plot(bt, p, &nx, &ny) && GetCursorPos(&g_snap_mouse)) {
        g_snap_on = 1;
        g_snap_x = nx;
        g_snap_y = ny;
        ((fn2_t)g_orig_mouse_move)(self, nx, ny); /* the ghost goes there now */
    }
    return r;
}

/* ==== Tank Trap lines =================================================================== */

typedef struct {
    int tx, ty;
} tile_t;

static int g_drag, g_skip_first;           /* a drag is on; its first tile is the last line's end */
static int g_up_frames;                    /* frames since the button went up during a drag */
static tile_t g_from, g_to;                /* the drag's tiles */
static int g_have_end;                     /* Shift still held since the last placement */
static tile_t g_end;
static struct {
    int x, y;
} g_pend[MAX_LINE];                        /* traps still to send */
static int g_pend_n, g_pend_pos, g_pend_p = -1;

static int tile_of(int c) { return c < 0 ? -1 : c / TILE; }

/* The tiles from a to b: solid = every step goes along one axis (the line has no diagonal gap). */
static int line_tiles(tile_t a, tile_t b, tile_t *out, int max)
{
    int n = 0, x = a.tx, y = a.ty;
    int dx = abs(b.tx - a.tx), dy = abs(b.ty - a.ty), sx = b.tx > a.tx ? 1 : -1, sy = b.ty > a.ty ? 1 : -1;
    out[n].tx = x, out[n].ty = y, n++;
    if (g_ttline_solid) {
        for (int ix = 0, iy = 0; (ix < dx || iy < dy) && n < max;) {
            /* the next crossing: of a vertical tile edge at (ix + 0.5) / dx, a horizontal one at (iy + 0.5) / dy */
            if (iy >= dy || (ix < dx && (2 * ix + 1) * dy < (2 * iy + 1) * dx))
                x += sx, ix++;
            else
                y += sy, iy++;
            out[n].tx = x, out[n].ty = y, n++;
        }
    } else {
        int err = dx - dy;
        while ((x != b.tx || y != b.ty) && n < max) {
            int e2 = 2 * err;
            if (e2 > -dy)
                err -= dy, x += sx;
            if (e2 < dx)
                err += dx, y += sy;
            out[n].tx = x, out[n].ty = y, n++;
        }
    }
    return n;
}

static int pending_has(int x, int y)
{
    for (int i = g_pend_pos; i < g_pend_n; i++)
        if (g_pend[i].x == x && g_pend[i].y == y)
            return 1;
    return 0;
}

/* mouse_move: a Shift drag starts or goes on */
static void ttline_move(int *a)
{
    if (placing() != TANK_TRAPS) {
        g_drag = g_have_end = 0;
        return;
    }
    if (!key_down(VK_SHIFT) && !g_drag)
        g_have_end = 0;
    if (!left_down())
        return; /* a drag ends with the release (picked_spot or ttline_tick) */
    tile_t t = { tile_of(a[0]), tile_of(a[1]) };
    if (t.tx < 0 || t.ty < 0)
        return;
    if (!g_drag) {
        if (!key_down(VK_SHIFT))
            return;
        g_drag = 1;
        g_skip_first = g_have_end && abs(t.tx - g_end.tx) <= CHAIN_TILES && abs(t.ty - g_end.ty) <= CHAIN_TILES;
        g_from = g_skip_first ? g_end : t;
    }
    g_to = t;
}

/* Lays the dragged line from g_from to t; the first trap through the game's click. */
static int lay_line(char *self, tile_t t, int p)
{
    static tile_t tiles[MAX_LINE];
    int n = line_tiles(g_from, t, tiles, MAX_LINE), first = 1;
    void *bt = buildtype(TANK_TRAPS);
    if (g_pend_p != p || g_pend_pos >= g_pend_n)
        g_pend_n = g_pend_pos = 0;
    g_pend_p = p;
    int ret = 0;
    for (int i = g_skip_first ? 1 : 0; i < n; i++) {
        int x, y;
        tile_center(bt, p, tiles[i].tx, tiles[i].ty, &x, &y);
        if (!site_ok(bt, p, x, y) || pending_has(x, y))
            continue;
        if (first) { /* the game's own click: its sound, its messages, the queue of Shift */
            ret = ((fn4_t)g_orig_picked)(self, x, y, -1, -1);
            first = 0;
        } else if (g_pend_n < MAX_LINE) {
            g_pend[g_pend_n].x = x;
            g_pend[g_pend_n].y = y;
            g_pend_n++;
        }
    }
    if (key_down(VK_SHIFT) && placing() == TANK_TRAPS) {
        g_have_end = 1;
        g_end = t;
    }
    static int logged;
    if (logged++ < 5)
        logf_("tank trap line: %d tiles from (%d, %d) to (%d, %d), %d to send after the first", n, g_from.tx,
              g_from.ty, t.tx, t.ty, g_pend_n - g_pend_pos);
    return ret;
}

/* picked_spot of a Tank Trap: the release of a drag lays the line (with one point the release
   point, with two the drag's own end); returns 1 when handled. */
static int ttline_picked(char *self, int *a, int p, int *ret)
{
    tile_t t = a[2] == -1 ? (tile_t){ tile_of(a[0]), tile_of(a[1]) } : g_to;
    int dragged = g_drag;
    g_drag = 0;
    g_up_frames = 0;
    if (!dragged || t.tx < 0 || t.ty < 0 || (!g_skip_first && t.tx == g_from.tx && t.ty == g_from.ty)) {
        if (a[2] != -1)
            return 0;
        *ret = ((fn4_t)g_orig_picked)(self, a[0], a[1], a[2], a[3]);
        if (t.tx >= 0 && key_down(VK_SHIFT) && placing() == TANK_TRAPS) {
            g_have_end = 1;
            g_end = t;
        }
        return 1;
    }
    *ret = lay_line(self, t, p);
    return 1;
}

/* every frame: a drag let go without a click (the game took it for something else) is laid
   a few frames later */
static void ttline_tick(void)
{
    if (!g_drag)
        return;
    if (placing() != TANK_TRAPS) {
        g_drag = 0;
        return;
    }
    if (left_down()) {
        g_up_frames = 0;
        return;
    }
    if (++g_up_frames < 8)
        return;
    g_drag = 0;
    g_up_frames = 0;
    int p = local_player();
    if (p >= 0 && p <= 7 && (g_skip_first || g_to.tx != g_from.tx || g_to.ty != g_from.ty))
        lay_line(options(), g_to, p);
}

/* every frame: the rest of the line, as the package has room */
static void ttline_feed(void)
{
    if (g_pend_pos >= g_pend_n)
        return;
    char *sel = local_selection();
    if (!sel || g_pend_p != local_player() || *(int *)(sel + 0x0C) <= 0) {
        g_pend_n = g_pend_pos = 0;
        return;
    }
    if (!((fn0_t)VA(VA_CHECK_ACCEPT))((void *)VA(VA_CMD_MANAGER)))
        return;
    char *pkg = (char *)VA(VA_PACKAGE);
    int need = 3 + 4 * *(int *)(sel + 0x0C) + 0x19 + 8;
    while (g_pend_pos < g_pend_n && *(short *)(pkg + 0x10) + need <= PKG_MAX) {
        int x = g_pend[g_pend_pos].x, y = g_pend[g_pend_pos].y;
        g_pend_pos++;
        if (!site_ok(buildtype(TANK_TRAPS), g_pend_p, x, y))
            continue;
        ((fn6_t)VA(VA_ISSUE_BUILD))(sel, x, y, -1, -1, TANK_TRAPS, QUEUE_AFTER);
    }
}

/* airdrop.c's Options::draw_cursor_follows hook, after the game's ghost: one more ghost per tile
   of the line being dragged (build_grid.c leaves its grid out for them). */
void placement_draw_ghosts(char *opts, void *orig_follows)
{
    if (!g_ttline_enabled || !g_drag || !opts || placing() != TANK_TRAPS)
        return;
    static tile_t tiles[MAX_LINE];
    int n = line_tiles(g_from, g_to, tiles, MAX_LINE), p = local_player();
    void *bt = buildtype(TANK_TRAPS);
    int sx = *(int *)(opts + OPT_X), sy = *(int *)(opts + OPT_Y), sz = *(int *)(opts + OPT_Z);
    g_place_extra = 1;
    for (int i = g_skip_first ? 1 : 0; i < n - 1; i++) { /* the last is the cursor's: drawn already */
        int x, y;
        tile_center(bt, p, tiles[i].tx, tiles[i].ty, &x, &y);
        *(int *)(opts + OPT_X) = x;
        *(int *)(opts + OPT_Y) = y;
        ((fn0_t)orig_follows)(opts);
    }
    g_place_extra = 0;
    *(int *)(opts + OPT_X) = sx;
    *(int *)(opts + OPT_Y) = sy;
    *(int *)(opts + OPT_Z) = sz;
}

/* ==== hooks ============================================================================= */

__attribute__((used)) int c_fs_mouse_move(char *self, int *a)
{
    if (g_ttline_enabled)
        ttline_move(a);
    if (g_snap_on) {
        if (placing() != FARM || !mouse_still())
            g_snap_on = 0;
        else {
            a[0] = g_snap_x;
            a[1] = g_snap_y;
        }
    }
    return ((fn2_t)g_orig_mouse_move)(self, a[0], a[1]);
}

__attribute__((used)) int c_fs_picked(char *self, int *a)
{
    int type = placing(), p = local_player(), r;
    if (type != FARM)
        g_row_n = 0, g_snap_on = 0;
    if (p >= 0 && p <= 7) {
        if (type == FARM && g_farmsnap_enabled && a[2] == -1)
            return farm_picked(self, a, p);
        if (type == TANK_TRAPS && g_ttline_enabled && ttline_picked(self, a, p, &r))
            return r;
    }
    return ((fn4_t)g_orig_picked)(self, a[0], a[1], a[2], a[3]);
}

void placement_tick(void)
{
    if (g_ttline_enabled) {
        ttline_tick();
        ttline_feed();
    }
}

#define FS_STUB(name, cfn, ret)                                                              \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    lea eax, [ebx+36]\n"                                                               \
    "    push eax\n"                                                                        \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

void hk_fs_mouse_move(void);
void hk_fs_picked(void);
__asm__(".intel_syntax noprefix\n"
        FS_STUB("hk_fs_mouse_move", "c_fs_mouse_move", "ret 8")
        FS_STUB("hk_fs_picked", "c_fs_picked", "ret 0x10")
        ".att_syntax prefix\n");

void farmsnap_install(void)
{
    if (!g_farmsnap_enabled && !g_ttline_enabled)
        return;
    /* push ebp / mov ebp,esp / and esp,-8 */
    static const unsigned char mm_sig[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
    /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
    static const unsigned char ps_sig[6] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
    unsigned char *mm = (unsigned char *)VA(VA_MOUSE_MOVE), *ps = (unsigned char *)VA(VA_PICKED_SPOT);
    if (memcmp(mm, mm_sig, sizeof mm_sig) != 0 || memcmp(ps, ps_sig, sizeof ps_sig) != 0) {
        logf_("placement: signature mismatch, farm snap and tank trap lines turned off");
        g_farmsnap_enabled = g_ttline_enabled = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        g_farmsnap_enabled = g_ttline_enabled = 0;
        return;
    }
    g_orig_mouse_move = hook_trampoline(mm, 6, mem);
    g_orig_picked = hook_trampoline(ps, 5, mem + 32);
    hook_jmp5(mm, hk_fs_mouse_move);
    hook_jmp5(ps, hk_fs_picked);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("placement: farm snap %s, tank trap lines %s%s", g_farmsnap_enabled ? "on" : "off",
          g_ttline_enabled ? "on" : "off", g_ttline_enabled && g_ttline_solid ? " (solid)" : "");
}
