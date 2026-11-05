/*
 * Play button: when a game has finished loading, the loading screen stays up and its
 * progress bar becomes a PLAY button (the full green bar, a light sweeping across it, a
 * slow glow). The game starts when it is clicked, or on Enter or Space.
 *
 * Game::run loads everything with the LoadingWin showing, then calls
 * CommandManager::start, which closes the LoadingWin and lets the game clock run
 * (Game+0x558 = 1 right after). That one call is pointed at a stub that first waits: it
 * keeps the screen alive the way the loading itself does (SplashScreen::refresh calls
 * Window::modal_idle_func, which draws every window while Game+0x558 is 0) until the
 * button is clicked. Nothing of the game runs while it waits.
 *
 * LoadingWin::on_redraw draws the bar with LoadingWin::draw_progress; that call is pointed
 * at play_draw_progress, which, while waiting, runs the original with the bar forced full
 * and the "Loading..." text hidden, then draws the button over the bar.
 *
 * Single player only: in multiplayer every player would wait for everyone else's click.
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_GAME_PTR       0x00C061ECu /* Game * */
#define VA_LOADING_PTR    0x00E39A3Cu /* LoadingWin * (MiscAccess::loading) */
#define VA_TIPWIN         0x00E8A898u /* IFaceTipWin tipwin (start-of-game tips, a profile option) */
#define VA_QUIT           0x00E335C4u /* int quit */
#define VA_MODAL_IDLE_FN  0x00E334E4u /* void (*Window::modal_idle_func)(void) */
#define VA_MODAL_IDLE     0x005994A0u /* System::modal_idle, the usual value of the above */
#define VA_FLUSH_INPUT    0x005388D0u /* flush_input() */
#define VA_TIMEGETTIME    0x00AC5460u /* the exe's import of timeGetTime */
#define VA_CMD_START      0x00942E10u /* CommandManager::start() */
#define AT_CMD_START      0x005851C0u /* Game::run: call CommandManager::start */
#define VA_DRAW_PROGRESS  0x00836070u /* LoadingWin::draw_progress() */
#define AT_DRAW_PROGRESS  0x00836E34u /* LoadingWin::on_redraw: call draw_progress */
#define VA_WINDOW_HIDE    0x00A54780u /* Window::hide() */
#define VA_GET_MOUSE_POS  0x00A54170u /* Window::get_mouse_pos(int *x, int *y): client coordinates */
#define VA_STRING_CTOR    0x00A1D660u
#define VA_STRING_CLOSE   0x00A1CF40u
#define VA_FONTS          0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET       0x00522810u /* FontFamily::get(size, style) const */
#define VA_SET_TEXT_COLOR 0x0050DAF0u /* Buffer::set_text_color(const Color &, const Color &, int, int) */
#define VA_WRITE_CENT     0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len): centred both ways */
#define VA_TEXT_WIDTH     0x0050EF40u /* Buffer::text_width(const String &) */
#define VA_WHITE          0x00C8D26Cu /* Color WHITE (12 bytes: R, G, B, A, ...) */
#define VA_BLACK          0x00C8D260u
#define VA_IFACE_RM       0x00EBD1C0u
#define VA_RM_ADD_RECT    0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_GRAPHSYS       0x00E335A0u
#define GS_DEPTH          0x164 /* GraphSys update depth: the z of what is added (see flags.c) */

/* SplashScreen statics read by draw_progress: fill = min(total_ticks, tick_limit[cur_block])
   / total_ticks_assumed, and the text under the bar is work_string when it is not empty */
#define VA_CUR_BLOCK      0x00CBE520u
#define VA_TICKS_ASSUMED  0x00CBE524u
#define VA_TOTAL_TICKS    0x00CBE528u
#define VA_TICK_LIMIT     0x00CBE530u /* int[] */
#define VA_WORK_STRING    0x00E802BCu /* String: length (word) at +8 */

#define GAME_FLAGS0   0x820 /* bit 2: multiplayer */
#define GAME_FLAGS2   0x822 /* bit 3: scenario, which has its own splash screen instead */
#define GAME_FPS_TIME 0x840 /* frame counter start, set by Game::clock_init */
#define WIN_FLAGS     0x08  /* bit 0: visible */
#define WIN_BUFFER    0x154 /* Buffer */
#define BUF_FONT      0x7C  /* Font *, then two more words draw_progress clears with it */
#define BUF_COLORS    0x90  /* set_text_color: colour +0x90, second colour +0xCC, flags +0x108, +0x10D */
#define BUF_COLORS_N  0x80  /* LoadingWin::on_redraw sets them once for all its texts: kept */
#define VT_SHOW       0x18  /* Window::show(int) */
#define VT_FIND_RECT  0x488 /* GraphicWinXML::find_rect(name, id, int *) */
#define RENDER_SOLID  5

/* the bar texture (load_bar_light) is 483 x 70; its glass tube inside the copper frame */
#define BAR_W   483.0f
#define BAR_H   70.0f
#define TUBE_L  15.0f
#define TUBE_R  468.0f
#define TUBE_T  15.0f
#define TUBE_B  50.0f
#define FRAME_T 2.0f /* the frame, for clicks */
#define FRAME_B 60.0f

/* animation, in seconds */
#define FLASH_TIME   0.35f /* white flash as the bar turns into the button */
#define SHINE_PERIOD 2.4f
#define SHINE_SWEEP  0.9f
#define GLOW_HZ      0.8f

typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_i)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_cstr)(void *self, const char *s);
typedef short *(__attribute__((thiscall)) *find_rect_t)(void *self, const void *name, int id, int *out);
typedef void(__attribute__((thiscall)) *mouse_t)(void *self, int *x, int *y);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *self, int size, int style);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c, const void *shadow, int a, int b);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef int(__attribute__((thiscall)) *text_width_t)(void *self, const void *str);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);

int g_play_enabled;
void *g_play_start_fn; /* CommandManager::start, jumped to by the stub */

static int g_waiting;
static double g_wait_since;
static short g_btn[4] = { 0, -1, 0, -1 }; /* clickable area (window coordinates), from the last draw */
static int g_hover, g_pressed;

void play_load_config(const char *ini_path)
{
    g_play_enabled = GetPrivateProfileIntA("qol", "play_button", 1, ini_path);
}

static double now_s(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

/* ---- drawing ------------------------------------------------------------------------- */

static void fill(float l, float t, float r, float b, unsigned color)
{
    short rect[4] = { (short)lrintf(l), (short)lrintf(r), (short)lrintf(t), (short)lrintf(b) };
    if (rect[1] > rect[0] && rect[3] > rect[2])
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), NULL, rect, color, RENDER_SOLID);
}

static unsigned alpha(unsigned rgb, float a)
{
    if (a <= 0)
        return 0;
    if (a > 1)
        a = 1;
    return (unsigned)(a * 255.0f + 0.5f) << 24 | (rgb & 0xFFFFFF);
}

/* the glass tube: a capsule with round ends */
typedef struct {
    float l, t, r, b, cy, rad;
} tube_t;

/* top and bottom of the capsule at x; 0 outside it */
static int tube_span(const tube_t *k, float x, float *top, float *bot)
{
    float dx = 0;
    if (x < k->l + k->rad)
        dx = k->l + k->rad - x;
    else if (x > k->r - k->rad)
        dx = x - (k->r - k->rad);
    if (dx >= k->rad)
        return 0;
    float hh = sqrtf(k->rad * k->rad - dx * dx);
    *top = k->cy - hh;
    *bot = k->cy + hh;
    return 1;
}

/* the whole tube in one colour, in 2-pixel columns */
static void tube_fill(const tube_t *k, unsigned color)
{
    if (!(color >> 24))
        return;
    for (float x = k->l; x < k->r; x += 2) {
        float t, b;
        if (tube_span(k, x + 1, &t, &b))
            fill(x, t, x + 2, b, color);
    }
}

/* a slanted band of light at x = pos (at the tube's middle), soft on both sides */
static void tube_shine(const tube_t *k, float pos, float sigma, float peak)
{
    const float slant = 0.55f, cell = 2;
    float reach = sigma * 2.5f + slant * k->rad;
    for (float x = fmaxf(k->l, pos - reach); x < fminf(k->r, pos + reach); x += cell) {
        float t, b;
        if (!tube_span(k, x + cell * 0.5f, &t, &b))
            continue;
        for (float y = t; y < b; y += cell) {
            float d = (x + cell * 0.5f - pos - (k->cy - y) * slant) / sigma;
            float a = peak * expf(-d * d);
            if (a > 0.02f)
                fill(x, y, x + cell, fminf(y + cell, b), alpha(0xFFFFFF, a));
        }
    }
}

/* a play triangle pointing right, its left side `h` tall */
static void triangle(float l, float cy, float h, unsigned color)
{
    float w = h * 0.87f;
    for (float x = 0; x < w; x += 1) {
        float half = h * 0.5f * (1 - (x + 0.5f) / w);
        fill(l + x, cy - half, l + x + 1, cy + half, color);
    }
}

static void draw_button(char *win)
{
    unsigned char name[64] = { 0 };
    ((fn_v_cstr)VA(VA_STRING_CTOR))(name, "progress_rect");
    short *r = ((find_rect_t)(*(void ***)win)[VT_FIND_RECT / 4])(win, name, -1, NULL);
    ((fn_v)VA(VA_STRING_CLOSE))(name);
    if (!r)
        return;
    float x0 = r[0], y0 = r[2], sx = (r[1] - r[0]) / BAR_W, sy = (r[3] - r[2]) / BAR_H;
    g_btn[0] = r[0];
    g_btn[1] = r[1];
    g_btn[2] = (short)(y0 + FRAME_T * sy);
    g_btn[3] = (short)(y0 + FRAME_B * sy);

    tube_t k;
    k.l = x0 + TUBE_L * sx;
    k.r = x0 + TUBE_R * sx;
    k.t = y0 + TUBE_T * sy;
    k.b = y0 + TUBE_B * sy;
    k.cy = (k.t + k.b) * 0.5f;
    k.rad = (k.b - k.t) * 0.5f;

    float t = (float)(now_s() - g_wait_since);
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
    (*depth)++; /* over the bar */

    /* glow: the tube breathes, brighter under the mouse; pressed, it darkens */
    if (g_pressed)
        tube_fill(&k, 0x50000000);
    else {
        float glow = 0.10f + 0.16f * (0.5f - 0.5f * cosf(t * GLOW_HZ * 6.2831853f));
        if (g_hover)
            glow += 0.18f;
        tube_fill(&k, alpha(0x80FFB0, glow));
    }
    if (t < FLASH_TIME)
        tube_fill(&k, alpha(0xFFFFFF, 0.8f * (1 - t / FLASH_TIME)));

    /* the light sweeping across, then a pause */
    float phase = fmodf(t, SHINE_PERIOD);
    if (phase < SHINE_SWEEP) {
        float u = phase / SHINE_SWEEP;
        u = u * u * (3 - 2 * u); /* eases in and out */
        float span = (k.r - k.l) + 8 * k.rad;
        tube_shine(&k, k.l - 4 * k.rad + u * span, 7.0f * sx, 0.75f);
        tube_shine(&k, k.l - 4 * k.rad + u * span - 5.0f * k.rad, 3.0f * sx, 0.35f);
    }

    /* PLAY with a triangle in front, white (gold under the mouse) with a dark shadow */
    (*depth)++;
    char *buf = win + WIN_BUFFER;
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), 22, 1);
    if (font) {
        unsigned char saved[12], saved_colors[BUF_COLORS_N];
        memcpy(saved, buf + BUF_FONT, 12);
        memcpy(saved_colors, buf + BUF_COLORS, BUF_COLORS_N);
        *(void **)(buf + BUF_FONT) = font;
        *(int *)(buf + BUF_FONT + 4) = 0;
        *(int *)(buf + BUF_FONT + 8) = 0;
        _Alignas(4) unsigned char color[12];
        memcpy(color, (void *)VA(VA_WHITE), 12);
        if (g_hover && !g_pressed) {
            color[0] = 0xFF;
            color[1] = 0xE6;
            color[2] = 0x7A;
        }
        unsigned char str[64] = { 0 };
        lang_string(str, lang_text("PLAY"));
        int len = *(unsigned short *)(str + 8);
        int tw = ((text_width_t)VA(VA_TEXT_WIDTH))(buf, str);
        float press = g_pressed ? 1.0f : 0.0f;
        float tri = 18.0f * sy, gap = 10.0f * sx;
        float left = (k.l + k.r) * 0.5f - (tri * 0.87f + gap + tw) * 0.5f + press;
        float cy = k.cy + press;
        unsigned tc = 0xFF000000u | color[2] << 16 | color[1] << 8 | color[0];
        triangle(left + 2, cy + 2, tri, 0xB0000000);
        triangle(left, cy, tri, tc);
        float tl = left + tri * 0.87f + gap;
        short rect[4] = { (short)lrintf(tl), (short)lrintf(tl + tw), (short)lrintf(k.t + press),
                          (short)lrintf(k.b + press) };
        ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, (void *)VA(VA_BLACK), -1, 0);
        if (len)
            ((write_t)VA(VA_WRITE_CENT))(buf, str, rect, len);
        ((fn_v)VA(VA_STRING_CLOSE))(str);
        memcpy(buf + BUF_FONT, saved, 12);
        memcpy(buf + BUF_COLORS, saved_colors, BUF_COLORS_N);
    }
    *depth -= 2;
}

/* Replaces `call LoadingWin::draw_progress` in LoadingWin::on_redraw. */
__attribute__((used, thiscall)) int c_play_draw_progress(void *win)
{
    if (!g_waiting)
        return ((fn_i)VA(VA_DRAW_PROGRESS))(win);
    int *assumed = (int *)VA(VA_TICKS_ASSUMED), *total = (int *)VA(VA_TOTAL_TICKS);
    int *limit = (int *)VA(VA_TICK_LIMIT) + *(int *)VA(VA_CUR_BLOCK);
    unsigned short *work_len = (unsigned short *)(VA(VA_WORK_STRING) + 8);
    int s_assumed = *assumed, s_total = *total, s_limit = *limit;
    unsigned short s_len = *work_len;
    *assumed = *total = *limit = 1; /* the bar full */
    *work_len = 0;                  /* no "Loading..." */
    int r = ((fn_i)VA(VA_DRAW_PROGRESS))(win);
    *assumed = s_assumed;
    *total = s_total;
    *limit = s_limit;
    *work_len = s_len;
    draw_button((char *)win);
    return r;
}

/* ---- waiting ------------------------------------------------------------------------- */

static int game_in_front(void)
{
    HWND h = GetForegroundWindow();
    DWORD pid = 0;
    if (h)
        GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

static int down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static void idle(void)
{
    void (*fn)(void) = *(void (**)(void))VA(VA_MODAL_IDLE_FN);
    (fn ? fn : (void (*)(void))VA(VA_MODAL_IDLE))();
    Sleep(8);
}

static int over_button(char *win)
{
    int x = -1, y = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(win, &x, &y);
    return x >= g_btn[0] && x < g_btn[1] && y >= g_btn[2] && y < g_btn[3];
}

/* Called by the stub that replaces Game::run's call to CommandManager::start. */
__attribute__((used)) void c_play_wait(void)
{
    stdio_raise(); /* before the tracks are opened, if the display hook is off */
    crash_install();
    char *game = *(char **)VA(VA_GAME_PTR);
    char *win = *(char **)VA(VA_LOADING_PTR);
    if (!g_play_enabled || !game || !win || (game[GAME_FLAGS0] & 4) || (game[GAME_FLAGS2] & 8) ||
        !(*(unsigned *)(win + WIN_FLAGS) & 1))
        return;

    char *tip = (char *)VA(VA_TIPWIN);
    int tip_shown = *(unsigned *)(tip + WIN_FLAGS) & 1;
    if (tip_shown)
        ((fn_v)VA(VA_WINDOW_HIDE))(tip);

    logf_("play button: waiting");
    g_waiting = 1;
    g_wait_since = now_s();
    g_hover = g_pressed = 0;
    /* keys already held (Enter from the setup screen) count only once released */
    int was_mouse = down(VK_LBUTTON), was_enter = down(VK_RETURN), was_space = down(VK_SPACE);
    int key = 0;
    while (!*(int *)VA(VA_QUIT)) {
        idle();
        int front = game_in_front();
        int mouse = front && down(VK_LBUTTON), enter = front && down(VK_RETURN), space = front && down(VK_SPACE);
        g_hover = front && over_button(win);
        if (mouse && !was_mouse && g_hover)
            g_pressed = 1;
        else if (!mouse && g_pressed) {
            g_pressed = 0;
            if (g_hover)
                break; /* pressed and released on the button */
        }
        if ((enter && !was_enter) || (space && !was_space)) {
            key = enter ? VK_RETURN : VK_SPACE;
            break;
        }
        was_mouse = mouse;
        was_enter = enter;
        was_space = space;
    }
    /* the key's release would reach the game (Enter opens the chat) */
    while (key && down(key) && !*(int *)VA(VA_QUIT))
        idle();
    g_waiting = 0;
    ((void (*)(void))VA(VA_FLUSH_INPUT))();
    if (tip_shown)
        ((fn_v_i)(*(void ***)tip)[VT_SHOW / 4])(tip, 0);
    /* only the frame counter: the game's own clock starts in CommandManager::start */
    *(DWORD *)(game + GAME_FPS_TIME) = (*(DWORD(WINAPI **)(void))VA(VA_TIMEGETTIME))();
    logf_("play button: starting after %.1f s", now_s() - g_wait_since);
    crash_test_now();
    mem_log("at the start");
}

void hk_play_start(void);

__asm__(".intel_syntax noprefix\n"
        /* replaces `call CommandManager::start` (thiscall, no arguments): wait, then go on
           to start with every register as it was */
        ".globl _hk_play_start\n"
        "_hk_play_start:\n" HOOK_ENTER
        "    call _c_play_wait\n" HOOK_LEAVE
        "    jmp dword ptr [_g_play_start_fn]\n"
        ".att_syntax prefix\n");

static const struct {
    UINT_PTR at, target;
    void *repl;
} k_sites[2] = {
    { AT_CMD_START, VA_CMD_START, (void *)hk_play_start },
    { AT_DRAW_PROGRESS, VA_DRAW_PROGRESS, (void *)c_play_draw_progress },
};

/* Called from install_hooks once the exe is known to be the supported version. */
void play_install(void)
{
    if (!g_play_enabled)
        return;
    for (int i = 0; i < 2; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(k_sites[i].target)) {
            logf_("play button: call site %d mismatch, turned off", i);
            g_play_enabled = 0;
            return;
        }
    }
    g_play_start_fn = (void *)VA(VA_CMD_START);
    for (int i = 0; i < 2; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        DWORD old;
        VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(at + 1) = (INT32)((UINT_PTR)k_sites[i].repl - (UINT_PTR)(at + 5));
        VirtualProtect(at, 5, old, &old);
    }
    logf_("play button installed");
}
