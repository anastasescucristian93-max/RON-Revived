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
 * Drawing and interface only. Engine notes: ENGINE_NOTES.md, "Minimap".
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_WORLDMAP_PTR   0x00E399ACu /* WorldMap* */
#define VA_IFACE_PTR      0x00E39964u /* IFaceMainBase* while a game is shown */
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
#define WIN_RECT     0x126 /* short left, right, top, bottom, screen (interface units) */
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

static const float g_scales[3] = { 1.0f, 1.2f, 1.44f };
static int g_on, g_size;
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
    short *r = rect(w), *pr = rect(parent);
    if (r[1] - r[0] != cw || r[3] - r[2] != ch) {
        ((fn_resize)(*(void ***)w)[VT_RESIZE / 4])(w, cw, ch, 0, 0);
        moved = 1;
    }
    if (r[0] - pr[0] != x || r[2] - pr[2] != y) {
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

/* Once per frame. */
void minimap_tick(void)
{
    if (!g_on)
        return;
    char *iface = *(char **)VA(VA_IFACE_PTR);
    char *wm = *(char **)VA(VA_WORLDMAP_PTR);
    char *con = *(char **)VA(VA_CONSOLE_PTR);
    char *loading = *(char **)VA(VA_LOADING_PTR);
    if (!iface || !wm || !con || (loading && CALL(fn_i, VA_IS_VISIBLE)(loading)))
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
        short *r = rect(mm), *hr = rect(host);
        g_home[0] = r[0] - hr[0], g_home[1] = r[2] - hr[2];
        g_home[2] = r[1] - r[0], g_home[3] = r[3] - r[2];
    }
    float s = g_scales[g_size];
    int w = (int)lroundf((home[1] - home[0]) * s), h = (int)lroundf((home[3] - home[2]) * s);
    short *cr = rect(con);
    int cw = cr[1] - cr[0];
    int x = cw - w - MARGIN, y = MARGIN;
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
    g_minimap_timer_top = cw > 0 ? (int)((MARGIN + h + TIMERS_GAP_PX / ui) * gw / cw + 0.5f) : TIMERS_TOP;
    g_btn[0] = cr[1] - MINIMAP_BUTTON - MARGIN; /* fixed in the corner, whatever the size */
    g_btn[1] = cr[2] + MARGIN;
    if (first || moved)
        logf_("minimap: top right, %dx%d (size %d of 3), at %d,%d of %dx%d", w, h, g_size + 1, x, y, cw,
              cr[3] - cr[2]);
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
