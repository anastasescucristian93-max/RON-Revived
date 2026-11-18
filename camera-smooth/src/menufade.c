/*
 * Main menu background: one scene melts into the next instead of the game's dip to black.
 *
 * TopMenu::on_redraw (0x0059C100) plays the topmenu.xml ANIM blocks in turn. Its own fade gives
 * every layer one alpha: down to 0 over a scene's last second, 0.5 s of pure black, up again
 * over 1 s (and a one-frame flash of the old scene between), so every change of scene went
 * through black, the layers of each scene showing through each other on the way.
 *
 * Here the part from its `call [timeGetTime]` (0x0059C203) to the end of the scene loop
 * (0x0059C563) is ours: the same timer and the same layer placement (rect + W * (move * t +
 * start_offset)), but
 *  - while a new scene is younger than the fade, the previous one is drawn first, fully opaque
 *    and still drifting (its time runs on past its duration), and the new one in front of it
 *    (one depth step nearer) with its alpha rising from 0; the vignette (the layer both scenes
 *    share, the first one) is drawn once, opaque, in front;
 *  - when the menu opens (no previous scene) the first scene comes up from black over half
 *    a second, as in the game, without the black hold;
 *  - a frame's time step is at most a quarter second, so the first frame after the menu was
 *    away does not skip a scene.
 * ron_camera.ini [ui] menu_crossfade = the fade's length in seconds; 0 = the game's own fade.
 */
#include "ron.h"
#include <string.h>

#define AT_TIMER      0x0059C203u /* on_redraw: call [timeGetTime], 6 bytes; ebx = this */
#define AT_AFTER      0x0059C563u /* after the scene loop and the switch: mov ecx, gamespy */
#define VA_ADD_SMOOTH 0x0051B700u /* IFaceRenderManager::add_smooth(tex, x xmm2, y xmm3, w, h, color, type), ret 0x14 */
#define VA_IFACE_RM   0x00EBD1C0u
#define VA_WHITE      0x00C8D26Cu
#define VA_GRAPHSYS   0x00E335A0u
#define GS_DEPTH      0x164

/* on_redraw's this (TopMenu + 0x9C) */
#define TM_WINDOW     (0x154 + 0x30) /* the window: vtable +0xD0 its rect (short left, right, top, bottom) */
#define TM_SCENES_N   0x11E4
#define TM_SCENES     0x11F0 /* 0x1C each: +4 layer count, +0x10 layers, +0x18 duration (float) */
#define TM_CUR        0x11F8
#define TM_T          0x11FC /* float, seconds into the current scene */
#define TM_LAST_MS    0x1200 /* timeGetTime of the last frame */
#define VT_FIND_TEX   0x47C  /* (const String &name, int) -> IFaceTex * */
#define VT_RECT       0xD0

#define SCN_SIZE       0x1C
#define SCN_COUNT      0x04
#define SCN_LAYERS     0x10
#define SCN_DURATION   0x18
#define LY_SIZE       0x2C /* move_x, move_y, rect (unused), start_offset_x, start_offset_y, String texture */
#define LY_NAME       0x18
#define RENDER_TYPE   7    /* what on_redraw draws its layers with */

#define OPEN_FADE     0.5f /* the first scene, coming up from black */
#define MAX_STEP      0.25f

typedef void *(__attribute__((thiscall)) *find_tex_t)(void *self, const void *name, int a);
typedef short *(__attribute__((thiscall)) *rect_t)(void *self);

static float g_fade = 1.5f;
static int g_prev = -1;      /* the scene fading out, -1 none */
static int g_seen = -1;      /* the scene and time we left last frame: anything else is a reset */
static float g_seen_t;
static int g_opening;        /* the menu just opened: its first scene comes up from black */
UINT_PTR g_mf_add_smooth, g_mf_rm, g_mf_after;

void menufade_load_config(const char *ini_path)
{
    g_fade = ini_float(ini_path, "ui", "menu_crossfade", 1.5f);
    if (g_fade > 10.0f)
        g_fade = 10.0f;
}

void mf_add(const void *tex, float x, float y, float w, float h, unsigned color);
__asm__(".intel_syntax noprefix\n"
        ".globl _mf_add\n"
        "_mf_add:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    movss xmm2, dword ptr [ebp+12]\n"
        "    movss xmm3, dword ptr [ebp+16]\n"
        "    push 7\n"                     /* RENDER_TYPE */
        "    push dword ptr [ebp+28]\n"    /* color */
        "    push dword ptr [ebp+24]\n"    /* h */
        "    push dword ptr [ebp+20]\n"    /* w */
        "    push dword ptr [ebp+8]\n"     /* tex */
        "    mov ecx, [_g_mf_rm]\n"
        "    call [_g_mf_add_smooth]\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

/* Milliseconds, as the game's timeGetTime, but from the performance counter: GetTickCount
   steps by 15.6 ms, which shows in a slow drift. */
static DWORD now_ms(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (DWORD)(now.QuadPart * 1000 / freq.QuadPart);
}

static void *layer_tex(char *self, char *layer)
{
    void **vt = *(void ***)self;
    return ((find_tex_t)vt[VT_FIND_TEX / 4])(self, layer + LY_NAME, 0);
}

/* Scene `s` at time t, its layers at alpha a (0..255); `skip` (the shared vignette) not drawn,
   `vignette_a` >= 0: its first layer at that alpha instead. */
static void draw_scene(char *self, const short *r, int s, float t, int a, const void *skip, int vignette_a)
{
    char *sc = *(char **)(self + TM_SCENES) + s * SCN_SIZE;
    int n = *(int *)(sc + SCN_COUNT);
    char *layers = *(char **)(sc + SCN_LAYERS);
    float w = (float)(r[1] - r[0]), h = (float)(r[3] - r[2]);
    unsigned white = *(unsigned *)VA(VA_WHITE) & 0x00FFFFFFu;
    for (int i = 0; i < n; i++) {
        char *ly = layers + i * LY_SIZE;
        void *tex = layer_tex(self, ly);
        if (!tex || tex == skip)
            continue;
        float *f = (float *)ly;
        float dx = w * (f[0] * t + f[4]), dy = h * (f[1] * t + f[5]);
        int la = i == 0 && vignette_a >= 0 ? vignette_a : a;
        mf_add(tex, r[0] + dx, r[2] + dy, w, h, white | (unsigned)la << 24);
    }
}

__attribute__((used)) void c_menu_scenes(char *self)
{
    int n = *(int *)(self + TM_SCENES_N);
    char *scenes = *(char **)(self + TM_SCENES);
    int *cur = (int *)(self + TM_CUR);
    float *t = (float *)(self + TM_T);
    DWORD now = now_ms();
    float step = (float)(DWORD)(now - *(DWORD *)(self + TM_LAST_MS)) / 1000.0f;
    *(DWORD *)(self + TM_LAST_MS) = now;
    if (n <= 0 || !scenes || *cur < 0 || *cur >= n)
        return;
    if (*cur != g_seen || *t < g_seen_t) { /* TopMenu::clear: back to scene 0 at 0, nothing fading out */
        g_prev = -1;
        g_opening = 1;
    }
    *t += step > MAX_STEP ? MAX_STEP : step;
    float dur = *(float *)(scenes + *cur * SCN_SIZE + SCN_DURATION);
    if (*t > dur) {
        g_prev = *cur;
        *t -= dur;
        if (*t > dur)
            *t = 0.0f;
        *cur = *cur + 1 >= n ? 0 : *cur + 1;
    }
    g_seen = *cur;
    g_seen_t = *t;

    void *win = *(void **)(self + TM_WINDOW);
    if (!win)
        return;
    const short *r = ((rect_t)(*(void ***)win)[VT_RECT / 4])(win);
    if (!r)
        return;
    char *s0 = *(char **)(scenes + *cur * SCN_SIZE + SCN_LAYERS);
    void *vignette = *(int *)(scenes + *cur * SCN_SIZE + SCN_COUNT) > 0 ? layer_tex(self, s0) : NULL;

    if (g_prev >= 0 && *t < g_fade) {
        float prev_t = *(float *)(scenes + g_prev * SCN_SIZE + SCN_DURATION) + *t;
        int a = (int)(255.0f * *t / g_fade);
        draw_scene(self, r, g_prev, prev_t, 255, vignette, -1);
        (*(int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH))++; /* the new scene in front */
        draw_scene(self, r, *cur, *t, a < 0 ? 0 : a > 255 ? 255 : a, NULL, 255);
        return;
    }
    g_prev = -1; /* faded in */
    int a = 255;
    if (g_opening) {
        if (*t < OPEN_FADE)
            a = (int)(255.0f * *t / OPEN_FADE);
        else
            g_opening = 0;
    }
    draw_scene(self, r, *cur, *t, a < 0 ? 0 : a, NULL, -1);
}

void hk_menu_scenes(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_menu_scenes\n"
        "_hk_menu_scenes:\n"
        "    push ebx\n"                 /* on_redraw's this */
        "    call _c_menu_scenes\n"      /* keeps ebx, esi, edi, ebp */
        "    add esp, 4\n"
        "    lea edi, [ebx+0x154]\n"     /* as the game's loop leaves it */
        "    jmp dword ptr [_g_mf_after]\n"
        ".att_syntax prefix\n");

void menufade_install(void)
{
    if (g_fade <= 0.0f)
        return;
    unsigned char *at = (unsigned char *)VA(AT_TIMER);
    unsigned char *after = (unsigned char *)VA(AT_AFTER);
    /* call dword ptr [timeGetTime] | mov ecx, offset gamespy */
    if (at[0] != 0xFF || at[1] != 0x15 || after[0] != 0xB9) {
        logf_("menufade: signature mismatch, the menu keeps the game's fade to black");
        return;
    }
    g_mf_add_smooth = VA(VA_ADD_SMOOTH);
    g_mf_rm = VA(VA_IFACE_RM);
    g_mf_after = (UINT_PTR)after;
    hook_jmp5(at, hk_menu_scenes);
    DWORD old;
    VirtualProtect(at + 5, 1, PAGE_EXECUTE_READWRITE, &old);
    at[5] = 0x90;
    VirtualProtect(at + 5, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("menufade: menu scenes cross-fade over %.2f s", (double)g_fade);
}
