/*
 * The minimap while the bottom interface is folded (the game's "display" button): in the top
 * right corner of the screen instead of the bottom right, in one of three sizes (the round
 * button in the screen's corner, the empty triangle between the map's diamond and the screen's
 * edges, cycles small, medium +20 %, large +44 %; the choice
 * is kept in ron_camera.ini). Unfolded, the minimap is back in its frame at its own size.
 *
 *  - The minimap is IFaceMiniMap (the WorldMap window is its child), inside the interface's
 *    mid_window frame (IFaceMainBase + 0x8F8), which IFaceMainBase::update_visibility moves to
 *    the bottom right when folded. Folded, IFaceMiniMap is made a child of the Console (the
 *    game view, where the qol buttons live too), sized to the map, at the top right; the empty
 *    frame is hidden so it takes no clicks. Unfolded, all of it goes back.
 *  - The WorldMap keeps its rect in its parent (+0x10BC, short l r t b, from IFACEMINIMAP's
 *    "map" rect) and most of its drawing reads the size from there. A new rect is followed by
 *    what WorldMap::reinit_all does, without the fog part: WorldMap::init(), vtable +0x420,
 *    show(0).
 *  - The game's clock lines (IFaceMainBase::draw_timers: the Armageddon count, wonder and time
 *    limit clocks, right-aligned at screen width - 10, from y 45) start a little under the minimap
 *    whatever its size (the user, 2026-11-01: about 50 px): `mov esi, 0x2D` at 0x0080B09C becomes
 *    a call that loads g_minimap_timer_top. The wonders being built (setup_progress_buttons)
 *    follow by themselves, they start where draw_timers ended; survival.c's clocks follow too.
 *  - IFaceMainBase::draw_cap_area frames the help and menu buttons in the top right corner
 *    (draw_border around both, a divider between them) also while folded, when the buttons
 *    are hidden: two empty boxes. Folded, that part is skipped.
 *  - Every match starts with the interface folded (the user, 2026-11-08; [ui] start_folded): the
 *    game keeps the last choice in the profile (byte 0x00ECD104, IFaceMainBase::close copies it to
 *    +0x470) and LoadGame::do_load unfolds it for a saved game. On the first frame of a match (after
 *    Game::run's CommandManager::start, which play_button.c hooks; so not without the play
 *    button) +0x470 of IFaceMain
 *    (MiscAccess::iface_main) goes to 0 and its update_visibility (vtable +0x4E8) runs, as
 *    LoadGame::do_load does after its change. The arrow button still unfolds it.
 * Drawing and interface only. Engine notes: ENGINE_NOTES.md, "Minimap".
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_WORLDMAP_PTR   0x00E399ACu /* WorldMap* */
#define VA_IFACE_PTR      0x00E39964u /* IFaceMainBase* while a game is shown */
#define VA_IFACE_MAIN     0x00E37B9Cu /* IFaceMain* (MiscAccess::iface_main) */
#define VT_UPDATE_VIS     0x4E8
#define VA_CONSOLE_PTR    0x00C06210u
#define VA_LOADING_PTR    0x00E39A3Cu /* LoadingWin* */
#define VA_GRAPHSYS_PTR   0x00E335A0u /* +0x360 screen width */
#define VA_INT_STRS_PTR   0x00C06378u
#define VA_MINIMAP_VTABLE 0x00B3B058u /* IFaceMiniMap */
#define VA_WORLDMAP_INIT  0x008D2CD0u /* WorldMap::init(void), reads the static itself */
#define VA_SET_PARENT     0x00A53E70u /* Window::set_parent(Window *) */
#define VA_WINDOW_MOVE    0x00A54080u /* Window::move(x, y), inside the parent */
#define VA_WINDOW_SHOW    0x00A55B30u /* Window::show(int) */
#define VA_WINDOW_HIDE    0x00A54780u
#define VA_IS_VISIBLE     0x00A54740u
#define VA_STRING_CTOR    0x00A1D660u
#define VA_STRING_CLOSE   0x00A1CF40u
#define VA_TIMERS_TOP     0x0080B09Cu /* draw_timers: mov esi, 0x2D (5 bytes), the first line's y */
#define TIMERS_TOP        45
#define TIMERS_GAP_PX     50          /* screen pixels between the minimap and the clocks */
#define VA_CAP_FRAME      0x00810400u /* draw_cap_area: mov eax, [int_str_array] after the cap border */
#define VA_CAP_FRAME_END  0x008105A7u /* its epilogue (pop edi ...) */

#define IFB_EXPANDED 0x470 /* byte: 0 while the interface is folded */
#define IFB_MAP_HOST 0x8F8 /* the minimap's frame window (mid_window) */
#define WM_RECT      0x10BC
#define WM_PARENT    0x10C4
#define WIN_PARENT   0x74
#define WIN_RECT     0x126 /* short left, right, top, bottom, relative to the parent (interface units) */
#define VT_RESIZE    0x20  /* GraphicWin::resize(w, h, 0, 0) */
#define VT_WM_SHOW   0x18
#define VT_WM_UPDATE 0x420
#define VT_FIND_RECT 0x488 /* GraphicWinXML::find_rect(String &, id, int *) */
#define MARGIN       4     /* from the screen's edges */

typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int a);
typedef void(__attribute__((thiscall)) *fn_v_p)(void *self, void *p);
typedef int(__attribute__((thiscall)) *fn_i)(void *self);
typedef int(__attribute__((thiscall)) *fn_move)(void *self, int x, int y);
typedef int(__attribute__((thiscall)) *fn_resize)(void *self, int w, int h, int a, int b);
typedef void(__attribute__((thiscall)) *fn_v_cstr)(void *self, const char *s);
typedef short *(__attribute__((thiscall)) *find_rect_t)(void *self, const void *name, int id, int *out);

#define CALL(type, va) ((type)VA(va))

/* The steel frame (2026-11-08): ui-theme\tools\build_minimap_frame.py's tiles, sizes in their
   texture's pixels. A tile holds the bar sheared to a slope of 1/2; its centre line at x = 0 is
   FT_C_DOWN / FT_C_UP from the tile's top. */
#define WM_DIAMOND 0xB90  /* float *: the diamond's 4 corners in the WorldMap's own coordinates,
                             x, y at +0, +4, 0x10 apart (WorldMap::check_mouse_accept) */
#define FT_W       140.0f
#define FT_H       110.0f
#define FT_C_DOWN  18.5f
#define FT_C_UP    88.5f
#define FT_PLATE   40.0f
#define FT_BOLT    17.0f  /* the bolt's centre in the plate texture (its shadow is below right) */
#define FT_K       0.6f   /* tile pixels to interface units: the bar about 15 units thick */
#define FT_OUT     4.0f   /* the bar's centre line this far outside the diamond's edge */
#define FRAME_ROOM 12     /* the minimap this much lower, so the top bolt is not cut off */

static const float g_scales[3] = { 1.0f, 1.2f, 1.44f };
static int g_on, g_size, g_start_folded = 1, g_frame = 1;
static char g_ini[MAX_PATH];
int g_minimap_timer_top = TIMERS_TOP; /* the clocks' first line, in draw_timers' units */
UINT_PTR g_cap_strs, g_cap_back, g_cap_skip;
static char *g_mm;         /* the IFaceMiniMap while it sits in the corner */
static int g_home[4] = { 0, 0, 332, 196 }; /* its x, y, w, h inside the frame (interface.xml) */
static char *g_hidden_host;
static int g_btn[2] = { -1, -1 };

void minimap_load_config(const char *ini_path)
{
    lstrcpynA(g_ini, ini_path, MAX_PATH);
    g_on = GetPrivateProfileIntA("ui", "minimap_top_right", 1, ini_path);
    g_size = GetPrivateProfileIntA("ui", "minimap_size", 0, ini_path);
    g_start_folded = GetPrivateProfileIntA("ui", "start_folded", 1, ini_path);
    g_frame = GetPrivateProfileIntA("ui", "minimap_frame", 1, ini_path);
    if (g_size < 0 || g_size > 2)
        g_size = 0;
}

static short *rect(char *w) { return (short *)(w + WIN_RECT); }

/* IFACEMINIMAP's "map" rect: the WorldMap's place and size inside the frame. */
static short *map_rect(char *mm)
{
    unsigned char str[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(str, "map");
    short *r = ((find_rect_t)(*(void ***)mm)[VT_FIND_RECT / 4])(mm, str, -1, NULL);
    CALL(fn_v, VA_STRING_CLOSE)(str);
    return r;
}

/* A new rect for the WorldMap (inside its parent), as WorldMap::reinit_all applies one. */
static void set_map_rect(char *wm, const short *r, int force)
{
    short *cur = (short *)(wm + WM_RECT);
    if (!force && !memcmp(cur, r, 8))
        return;
    memcpy(cur, r, 8);
    CALL(fn_v, VA_WORLDMAP_INIT)(wm);
    void **vt = *(void ***)wm;
    ((fn_v)vt[VT_WM_UPDATE / 4])(wm);
    ((fn_v_i)vt[VT_WM_SHOW / 4])(wm, 0);
}

/* Window `w` at x, y, size cw x ch inside `parent`; 1 when anything changed. */
static int place(char *w, char *parent, int x, int y, int cw, int ch)
{
    int moved = 0;
    if (*(char **)(w + WIN_PARENT) != parent) {
        CALL(fn_v_p, VA_SET_PARENT)(w, parent);
        moved = 1;
    }
    short *r = rect(w); /* relative to the parent, as Window::move sets it */
    if (r[1] - r[0] != cw || r[3] - r[2] != ch) {
        ((fn_resize)(*(void ***)w)[VT_RESIZE / 4])(w, cw, ch, 0, 0);
        moved = 1;
    }
    if (r[0] != x || r[2] != y) {
        CALL(fn_move, VA_WINDOW_MOVE)(w, x, y);
        moved = 1;
    }
    return moved;
}

static void put_back(char *wm)
{
    char *mm = g_mm;
    g_mm = NULL;
    g_btn[0] = -1;
    g_minimap_timer_top = TIMERS_TOP;
    char *iface = *(char **)VA(VA_IFACE_PTR);
    char *host = g_hidden_host;
    g_hidden_host = NULL;
    if (!iface || !mm || *(UINT_PTR *)mm != VA(VA_MINIMAP_VTABLE))
        return;
    if (host != iface + IFB_MAP_HOST) /* the interface was built again since */
        host = iface + IFB_MAP_HOST;
    else
        CALL(fn_v_i, VA_WINDOW_SHOW)(host, 1);
    int moved = place(mm, host, g_home[0], g_home[1], g_home[2], g_home[3]);
    short *home = map_rect(mm);
    if (home)
        set_map_rect(wm, home, moved);
    logf_("minimap: back in the interface");
}

extern int g_match_starting; /* play_button.c: Game::run's CommandManager::start, every match */

/* The first frame of a match: the interface folded once. */
static void fold_at_start(char *iface, int in_game)
{
    if (!in_game || !g_match_starting)
        return;
    g_match_starting = 0;
    if (!g_start_folded)
        return;
    char *main = *(char **)VA(VA_IFACE_MAIN);
    if (!main || main != iface || !main[IFB_EXPANDED])
        return;
    main[IFB_EXPANDED] = 0;
    ((fn_v)(*(void ***)main)[VT_UPDATE_VIS / 4])(main);
    logf_("minimap: the match starts with the interface folded");
}

/* Once per frame. */
void minimap_tick(void)
{
    char *iface = *(char **)VA(VA_IFACE_PTR);
    char *wm = *(char **)VA(VA_WORLDMAP_PTR);
    char *con = *(char **)VA(VA_CONSOLE_PTR);
    char *loading = *(char **)VA(VA_LOADING_PTR);
    int in_game = iface && wm && con && !(loading && CALL(fn_i, VA_IS_VISIBLE)(loading));
    fold_at_start(iface, in_game);
    if (!g_on || !in_game)
        return;
    char *mm = *(char **)(wm + WM_PARENT);
    if (!mm || *(UINT_PTR *)mm != VA(VA_MINIMAP_VTABLE)) {
        static int logged;
        if (!logged++)
            logf_("minimap: the WorldMap's parent is not the IFaceMiniMap, left where it is");
        return;
    }
    if (iface[IFB_EXPANDED]) {
        if (g_mm)
            put_back(wm);
        return;
    }
    char *host = iface + IFB_MAP_HOST;
    short *home = map_rect(mm);
    if (!home)
        return;
    if (*(char **)(mm + WIN_PARENT) == host) {
        /* its place in the frame: Window +0x126 is relative to the parent already (the old
           r - frame's r put it 316 units left of the frame after the first unfold, the user
           2026-11-08: the minimap gone from the unfolded interface after one fold) */
        short *r = rect(mm);
        g_home[0] = r[0], g_home[1] = r[2];
        g_home[2] = r[1] - r[0], g_home[3] = r[3] - r[2];
    }
    float s = g_scales[g_size];
    int w = (int)lroundf((home[1] - home[0]) * s), h = (int)lroundf((home[3] - home[2]) * s);
    short *cr = rect(con);
    int cw = cr[1] - cr[0];
    int x = cw - w - MARGIN, y = MARGIN + (g_frame ? FRAME_ROOM : 0);
    int first = !g_mm;
    int moved = place(mm, con, x, y, w, h);
    /* something of the game's moving it back every frame would make a WorldMap::init a frame */
    static int churn;
    churn = moved ? churn + 1 : 0;
    if (churn > 20) {
        logf_("minimap: the game keeps moving it back, it stays in the interface");
        g_mm = mm;
        put_back(wm);
        g_on = 0;
        return;
    }
    const short r[4] = { 0, (short)w, 0, (short)h };
    set_map_rect(wm, r, moved);
    g_mm = mm;
    if (CALL(fn_i, VA_IS_VISIBLE)(host)) {
        CALL(fn_v, VA_WINDOW_HIDE)(host);
        g_hidden_host = host;
    }
    char *gs = *(char **)VA(VA_GRAPHSYS_PTR);
    int gw = gs ? *(int *)(gs + 0x360) : cw;
    float ui = ui_scale_now() > 0 ? ui_scale_now() : 1.0f;
    g_minimap_timer_top = cw > 0 ? (int)((y + h + TIMERS_GAP_PX / ui) * gw / cw + 0.5f) : TIMERS_TOP;
    g_btn[0] = cr[1] - MINIMAP_BUTTON - MARGIN; /* fixed in the corner, whatever the size */
    g_btn[1] = cr[2] + MARGIN;
    if (first || moved)
        logf_("minimap: top right, %dx%d (size %d of 3), at %d,%d of %dx%d", w, h, g_size + 1, x, y, cw,
              cr[3] - cr[2]);
}

int minimap_frame_wanted(void) { return g_on && g_frame; }

/* The steel frame along the diamond's edges, drawn in the Console's redraw (rankfx.c), so the
   minimap (a Console child) lies over it: the bar's inner side goes under the map's edge. The
   interface draws upright rectangles only, so each edge is a row of diagonal tiles, each
   stretched to the edge's own slope (the diamond is not quite symmetric: the minimap has a 3D
   camera); bolts cover the four corners. Picture only. */
void minimap_draw_frame(void)
{
    if (!g_frame || !g_mm)
        return;
    char *wm = *(char **)VA(VA_WORLDMAP_PTR);
    if (!wm || *(char **)(wm + WM_PARENT) != g_mm || !CALL(fn_i, VA_IS_VISIBLE)(g_mm))
        return;
    const float *d = *(const float **)(wm + WM_DIAMOND);
    const void *t_down = qol_iface_tex("mm_frame_down"), *t_up = qol_iface_tex("mm_frame_up");
    const void *t_bolt = qol_iface_tex("mm_frame_plate");
    static int logged;
    if (!d || !t_down || !t_up || !t_bolt) {
        if (!logged++)
            logf_("minimap frame: %s, no frame", d ? "textures missing (interface.xml)" : "no diamond");
        return;
    }
    short *r = rect(g_mm);
    float w = (float)(r[1] - r[0]), h = (float)(r[3] - r[2]);
    float vx[4], vy[4], ang[4], cx = 0.0f, cy = 0.0f;
    for (int i = 0; i < 4; i++) {
        vx[i] = d[i * 4], vy[i] = d[i * 4 + 1];
        if (!(vx[i] > -0.5f * w && vx[i] < 1.5f * w && vy[i] > -0.5f * h && vy[i] < 1.5f * h)) {
            if (!logged++)
                logf_("minimap frame: corner %d at %.1f,%.1f outside the %gx%g minimap, no frame", i, vx[i], vy[i], w, h);
            return;
        }
        cx += vx[i] * 0.25f, cy += vy[i] * 0.25f;
    }
    if (!logged++)
        logf_("minimap frame: corners %.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f in %gx%g", vx[0], vy[0], vx[1], vy[1],
              vx[2], vy[2], vx[3], vy[3], w, h);
    /* round the centre in angle order, in the render manager's coordinates */
    int o[4] = { 0, 1, 2, 3 };
    for (int i = 0; i < 4; i++)
        ang[i] = atan2f(vy[i] - cy, vx[i] - cx);
    for (int i = 1; i < 4; i++)
        for (int j = i; j > 0 && ang[o[j]] < ang[o[j - 1]]; j--) {
            int t = o[j];
            o[j] = o[j - 1], o[j - 1] = t;
        }
    float px[4], py[4];
    for (int i = 0; i < 4; i++) {
        px[i] = (float)r[0] + vx[o[i]], py[i] = (float)r[2] + vy[o[i]];
        rankfx_console_to_rm(&px[i], &py[i]);
    }
    float mx = (px[0] + px[1] + px[2] + px[3]) * 0.25f, my = (py[0] + py[1] + py[2] + py[3]) * 0.25f;
    for (int e = 0; e < 4; e++) {
        float ax = px[e], ay = py[e], bx = px[(e + 1) & 3], by = py[(e + 1) & 3];
        float ex = bx - ax, ey = by - ay, len = sqrtf(ex * ex + ey * ey);
        if (len < 4.0f)
            continue;
        float nx = ey / len, ny = -ex / len; /* outwards: away from the centre */
        if (nx * ((ax + bx) * 0.5f - mx) + ny * ((ay + by) * 0.5f - my) < 0.0f)
            nx = -nx, ny = -ny;
        ax += nx * FT_OUT, ay += ny * FT_OUT, bx += nx * FT_OUT, by += ny * FT_OUT;
        if (ax > bx) {
            float t = ax;
            ax = bx, bx = t, t = ay, ay = by, by = t;
        }
        float dx = bx - ax, s = (by - ay) / dx;
        if (dx < 4.0f || fabsf(s) < 0.05f)
            continue;
        int n = (int)lroundf(dx / (FT_W * FT_K));
        if (n < 1)
            n = 1;
        float tw = dx / (float)n, ky = 2.0f * fabsf(s) * tw / FT_W;
        const void *tex = s > 0.0f ? t_down : t_up;
        float c0 = s > 0.0f ? FT_C_DOWN : FT_C_UP;
        for (int i = 0; i < n; i++)
            rf_add(tex, ax + (float)i * tw, ay + (float)i * s * tw - c0 * ky, tw + 0.5f, FT_H * ky, 0xFFFFFFFFu);
    }
    float ps = FT_PLATE * FT_K, bc = FT_BOLT * FT_K;
    for (int i = 0; i < 4; i++) {
        float ux = px[i] - mx, uy = py[i] - my, ul = sqrtf(ux * ux + uy * uy);
        if (ul < 1.0f)
            continue;
        float bx = px[i] + ux / ul * FT_OUT, by = py[i] + uy / ul * FT_OUT;
        rf_add(t_bolt, bx - bc, by - bc, ps, ps, 0xFFFFFFFFu);
    }
}

/* The size button's top left corner (screen, interface units) in the screen's top right corner,
   which the minimap's diamond leaves empty (the user, 2026-11-01); 0 while there is none. */
int minimap_button_pos(int *x, int *y)
{
    if (g_btn[0] < 0)
        return 0;
    *x = g_btn[0], *y = g_btn[1];
    return 1;
}

void minimap_cycle_size(void)
{
    g_size = (g_size + 1) % 3;
    char v[4];
    wsprintfA(v, "%d", g_size);
    WritePrivateProfileStringA("ui", "minimap_size", v, g_ini);
    logf_("minimap: size %d of 3", g_size + 1);
}

void hk_minimap_timers_top(void);
void hk_minimap_cap_frame(void);

__asm__(".intel_syntax noprefix\n"
        /* draw_timers: esi = the first line's y, under a minimap in the corner */
        ".globl _hk_minimap_timers_top\n"
        "_hk_minimap_timers_top:\n"
        "    mov esi, [_g_minimap_timer_top]\n"
        "    ret\n"
        /* draw_cap_area (ebx = IFaceMainBase): no frame around the hidden help / menu buttons */
        ".globl _hk_minimap_cap_frame\n"
        "_hk_minimap_cap_frame:\n"
        "    cmp byte ptr [ebx+0x470], 0\n"
        "    je 1f\n"
        "    mov eax, [_g_cap_strs]\n"
        "    mov eax, [eax]\n"
        "    jmp dword ptr [_g_cap_back]\n"
        "1:  jmp dword ptr [_g_cap_skip]\n"
        ".att_syntax prefix\n");

static void patch(unsigned char *at, const unsigned char *bytes, int n)
{
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
}

static void patch_rel(unsigned char *at, unsigned char op, void *to, int n)
{
    unsigned char b[8] = { op, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    int rel = (int)((UINT_PTR)to - (UINT_PTR)(at + 5));
    memcpy(b + 1, &rel, 4);
    patch(at, b, n);
}

void minimap_install(void)
{
    /* the empty frame goes whatever minimap_top_right says */
    unsigned char *cap = (unsigned char *)VA(VA_CAP_FRAME);
    static const unsigned char end_sig[7] = { 0x5F, 0x5E, 0x5B, 0x8B, 0xE5, 0x5D, 0xC3 };
    if (cap[0] == 0xA1 && *(UINT32 *)(cap + 1) == (UINT32)VA(VA_INT_STRS_PTR) &&
        !memcmp((void *)VA(VA_CAP_FRAME_END), end_sig, sizeof end_sig)) {
        g_cap_strs = VA(VA_INT_STRS_PTR);
        g_cap_back = VA(VA_CAP_FRAME) + 5;
        g_cap_skip = VA(VA_CAP_FRAME_END);
        patch_rel(cap, 0xE9, hk_minimap_cap_frame, 5);
        logf_("minimap: no empty help / menu frame while the interface is folded");
    } else {
        logf_("minimap: signature mismatch at draw_cap_area, the help / menu frame stays");
    }
    if (!g_on)
        return;
    unsigned char *tt = (unsigned char *)VA(VA_TIMERS_TOP);
    static const unsigned char tt_sig[5] = { 0xBE, 0x2D, 0x00, 0x00, 0x00 };
    if (memcmp(tt, tt_sig, sizeof tt_sig)) {
        logf_("minimap: signature mismatch at draw_timers, the clocks may sit under the minimap");
    } else {
        patch_rel(tt, 0xE8, hk_minimap_timers_top, 5);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("minimap: top right while the interface is folded, size %d of 3", g_size + 1);
}
