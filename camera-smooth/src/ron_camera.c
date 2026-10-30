/*
 * Smooth Camera pentru Rise of Nations: Extended Edition (Steam, build-ul din 20.06.2024).
 *
 * Se incarca drept winmm.dll din folderul jocului (proxy: toate exporturile merg la
 * winmm.dll-ul din sistem) si pune hook-uri in memorie pe functiile camerei. Exe-ul
 * de pe disc nu e modificat. Daca exe-ul nu e versiunea cunoscuta, nu se pune niciun hook.
 *
 * Ce schimba:
 *  - scroll: diagonala normalizata (nu mai e de 1.41x mai rapida), tastatura + marginea
 *    ecranului nu se mai aduna, acceleratie/franare lina, timp masurat cu QPC.
 *  - zoom: distanta se animeaza lin intre niveluri (inclinarea camerei urmeaza distanta),
 *    zoom in cu rotita pastreaza punctul de sub cursor in loc sa sara pe el.
 *  - pan_to (salturi de camera): miscare lina in loc de salturi.
 *
 * Offset-urile si adresele vin din sbl/rise_z.map livrat cu jocul.
 */
#include <windows.h>
#include <bcrypt.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#include "ron.h"
#include "winmm_proxy.inc"

/* ---- versiunea de joc suportata -------------------------------------------------- */

#define EXE_TIMESTAMP 0x6674863Fu
#define EXE_PREF_BASE 0x00400000u

#define VA_SCROLL_DIR   0x00842F60u /* Camera::scroll_dir(int dir, float amount in xmm2) */
#define VA_CHECK_SCROLL 0x008440F0u /* Camera::check_scroll() */
#define VA_CHECK_ZOOM   0x00843C30u /* Camera::check_zoom() */
#define VA_ZOOM_IN_XY   0x00843F10u /* Camera::zoom_in(int x, int y, int) */
#define VA_ZOOM_IN_KEY  0x00843ED0u /* Camera::zoom_in(int), no stack argument left (ret) */
#define VA_ZOOM_OUT     0x00843FE0u /* Camera::zoom_out(int lock_mask), ret 4 */
#define VA_OUTDATE      0x00844B80u /* Camera::outdate() */
#define VA_LOC_BOUNDS   0x00842FF0u /* Camera::loc_bounds() */
#define VA_CAMERA_PTR   0x00C06200u /* MiscAccess::camera */
#define VA_SCENE_PTR    0x00C0620Cu /* MiscAccess::scene */
#define VA_GAME_PTR     0x00C061E8u /* GameAccessConst::gamec */
#define GAME_KEYS       0xAF0       /* BYTE[256] din GetKeyboardState, citit in Game::do_frame */
#define VA_CORNER_X     0x00ADC3E0u /* int corner_x[8] */
#define VA_CORNER_Y     0x00ADC3C0u /* int corner_y[8] */

/* ---- campuri din Camera ---------------------------------------------------------- */

#define CAM_I(c, off) (*(volatile int *)((char *)(c) + (off)))
#define CAM_F(c, off) (*(volatile float *)((char *)(c) + (off)))

#define OFF_OUTDATED     0x28C /* Camera::outdate; TerrainOut::render rebuilds the visible ground while set */
#define OFF_ZOOMED       0x290 /* setat la schimbarea zoom-ului */
#define OFF_LEVEL        0x294 /* nivelul curent de zoom (0 = departe, 6 = aproape) */
#define OFF_LEVEL_TARGET 0x298
#define OFF_ZOOM_DIR     0x29C
#define OFF_PAN_STEPS    0x2A0
#define OFF_PAN_X        0x2A4
#define OFF_PAN_Y        0x2A8
#define OFF_FOLLOW       0x2B4
#define OFF_SCROLL_SPEED 0x2C4 /* float[7], viteza pe nivel */
#define OFF_SCROLL_TIME  0x2EC /* ultimul timeGetTime() din check_scroll */
#define OFF_DIST         0x2F0 /* distanta de zoom */
#define OFF_TILT         0x2F4
#define OFF_DIR_COS      0x2FC
#define OFF_DIR_SIN      0x300
#define OFF_LOC_X        0x304
#define OFF_LOC_Y        0x308
#define OFF_LEVEL_DIST   0x320 /* float[7] */
#define OFF_LEVEL_TILT   0x33C /* float[7] */
#define OFF_AUTO_TILT    0x358
#define OFF_AUTO_TILT_ON 0x364
#define OFF_LOCK_FLAGS   0x35C /* bit 0: camera blocata, bit 3: fara pan */
#define OFF_ZOOM_LOCK    0x360 /* bit 0: zoom locked (zoom_in / zoom_out do nothing) */

#define NUM_LEVELS 7

/* Directiile din check_scroll: 1 = sus, 2 = dreapta, 3 = jos, 4 = stanga. */
enum { DIR_UP = 1, DIR_RIGHT = 2, DIR_DOWN = 3, DIR_LEFT = 4 };

typedef void(__attribute__((thiscall)) *thiscall0_t)(void *self);
typedef void(__attribute__((thiscall)) *zoom_in_t)(void *self, int x, int y, int z);

UINT_PTR g_delta;

static thiscall0_t g_orig_check_scroll;
static thiscall0_t g_orig_check_zoom;
static thiscall0_t g_orig_add_windows;
static void *g_orig_button_clicked;
static zoom_in_t g_orig_zoom_in;
static thiscall0_t g_orig_zoom_in_key;
static void(__attribute__((thiscall)) *g_orig_zoom_out)(void *self, int mask);
static thiscall0_t g_outdate;
static thiscall0_t g_loc_bounds;

static void *main_camera(void) { return *(void **)VA(VA_CAMERA_PTR); }

/* ---- configurare ------------------------------------------------------------------- */

static struct {
    int smooth_scroll;
    float scroll_accel_ms;
    float scroll_decel_ms;
    float scroll_speed;
    float vertical_speed;
    int smooth_zoom;
    float zoom_smooth_ms;
    int zoom_to_cursor;
    int zoom_tilt; /* 1 = tilt follows the distance, 0 = tilt jumps to the target level */
    int zoom_filter; /* 1 = linear texture filter on sprites while zooming (no shimmer) */
    int extra_zoom;  /* 1 = one zoom step farther and one closer than the game allows */
    float pan_smooth_ms;
    int fix_stuck_keys;
    int log;
} cfg;

static int g_hook_check_scroll; /* smooth_scroll || fix_stuck_keys */
int g_hook_draw_build;          /* nation art || subpixel sprites */
int g_hook_init_display;        /* ui scale || subpixel sprites || postfx */
static int g_extra_in_enabled;  /* smooth zoom && extra_zoom: the step closer than level 6 */
static int g_extra_in;          /* that step is the zoom target now */

char g_dir[MAX_PATH];

float ini_float(const char *path, const char *section, const char *key, float def)
{
    char buf[64], defbuf[64];
    snprintf(defbuf, sizeof defbuf, "%g", def);
    GetPrivateProfileStringA(section, key, defbuf, buf, sizeof buf, path);
    return (float)atof(buf);
}

static int g_fish_retry;
static int g_range_circles;
static int g_coop_ai_teams;

static void load_config(void)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%sron_camera.ini", g_dir);
    cfg.smooth_scroll = GetPrivateProfileIntA("camera", "smooth_scroll", 1, path);
    cfg.scroll_accel_ms = ini_float(path, "camera", "scroll_accel_ms", 0.0f);
    cfg.scroll_decel_ms = ini_float(path, "camera", "scroll_decel_ms", 0.0f);
    cfg.scroll_speed = ini_float(path, "camera", "scroll_speed", 1.0f);
    cfg.vertical_speed = ini_float(path, "camera", "vertical_speed", 1.0f);
    cfg.smooth_zoom = GetPrivateProfileIntA("camera", "smooth_zoom", 1, path);
    cfg.zoom_smooth_ms = ini_float(path, "camera", "zoom_smooth_ms", 250.0f);
    cfg.zoom_to_cursor = GetPrivateProfileIntA("camera", "zoom_to_cursor", 1, path);
    cfg.zoom_tilt = GetPrivateProfileIntA("camera", "zoom_tilt", 1, path);
    cfg.zoom_filter = GetPrivateProfileIntA("camera", "zoom_linear_filter", 1, path);
    cfg.extra_zoom = GetPrivateProfileIntA("camera", "extra_zoom", 1, path);
    cfg.pan_smooth_ms = ini_float(path, "camera", "pan_smooth_ms", 70.0f);
    cfg.fix_stuck_keys = GetPrivateProfileIntA("camera", "fix_stuck_keys", 1, path);
    cfg.log = GetPrivateProfileIntA("camera", "log", 1, path);
    g_zoom_trace = GetPrivateProfileIntA("debug", "zoom_trace", 0, path);
    g_debug_viewports = GetPrivateProfileIntA("debug", "viewport_log", 0, path);
    fog_load_config(path);
    qol_load_config(path);
    grid_load_config(path);
    nation_art_load_config(g_dir);
    ui_scale_load_config(path);
    subpixel_load_config(path);
    kills_load_config(path);
    ranks_load_config(path);
    rankfx_load_config(path);
    heal_load_config(path);
    make_way_load_config(path);
    formation_load_config(path);
    hud_load_config(path);
    flags_load_config(path);
    ngrid_load_config(path);
    override_load_config(path);
    ai_names_load_config(path);
    armageddon_load_config(path);
    corpses_load_config(path);
    nukefx_load_config(path);
    nonukes_load_config(path);
    attrition_load_config(path);
    notices_load_config(path);
    survival_load_config(path);
    assistant_load_config(path);
    assist_orders_load_config(path);
    hover_load_config(path);
    squad_hp_load_config(path);
    outline_load_config(path);
    bribe_load_config(path);
    follow_load_config(path);
    rally_load_config(path);
    costs_load_config(path);
    prodbar_load_config(path);
    facing_load_config(path);
    play_load_config(path);
    airstrike_load_config(path);
    lang_load_config(path);
    postfx_load_config(path);
    g_fish_retry = GetPrivateProfileIntA("gameplay", "fishing_retry", 1, path);
    g_range_circles = GetPrivateProfileIntA("qol", "range_circles", 1, path);
    g_coop_ai_teams = GetPrivateProfileIntA("qol", "coop_ai_any_teams", 1, path);
    dc_load_config(g_dir);
    airdrop_load_config(g_dir);
    entrench_load_config(g_dir);
    romania_load_config(g_dir);
    g_hook_check_scroll = cfg.smooth_scroll || cfg.fix_stuck_keys || g_qol_enabled || g_airdrop_enabled;
    g_hook_draw_build = g_nation_art_enabled || g_subpixel_enabled;
    g_hook_init_display = g_ui_scale_enabled || g_subpixel_enabled || g_postfx_enabled;
    g_extra_in_enabled = cfg.smooth_zoom && cfg.extra_zoom;
}

int g_zoom_trace, g_debug_viewports; /* [debug]: diagnostics, off for players */

void logf_(const char *fmt, ...)
{
    if (!cfg.log)
        return;
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%sron_camera.log", g_dir);
    FILE *f = fopen(path, "a");
    if (!f)
        return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* ---- timp -------------------------------------------------------------------------- */

static double g_qpc_freq;

static double now_s(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / g_qpc_freq;
}

/* 1 - e^(-dt/tau): fractiunea parcursa spre tinta intr-un pas, independenta de FPS. */
static float approach(float dt, float tau_ms)
{
    if (tau_ms <= 0.0f)
        return 1.0f;
    return 1.0f - expf(-dt * 1000.0f / tau_ms);
}

/* Pozitia distantei d intre niveluri: indicele i si t in [0,1] intre nivelul i si i+1. */
static void level_bracket(void *cam, float d, int *i_out, float *t_out)
{
    int i;
    for (i = 0; i < NUM_LEVELS - 2; i++)
        if (d >= CAM_F(cam, OFF_LEVEL_DIST + 4 * (i + 1)))
            break;
    float a = CAM_F(cam, OFF_LEVEL_DIST + 4 * i), b = CAM_F(cam, OFF_LEVEL_DIST + 4 * (i + 1));
    float t = (a != b) ? (d - a) / (b - a) : 0.0f;
    *i_out = i;
    *t_out = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

static int g_zooming, g_cam_moved; /* this frame: zoom in progress / camera position changed */

/* `moved` = the camera position changed by a whole unit: the game then flags the scene
   (+0x22d) so the terrain's visible set is rebuilt. A zoom never sets that flag in the
   original game; setting it every frame rebuilt the terrain each frame. */
static void mark_moved_ex(void *cam, int moved)
{
    void *scene = *(void **)VA(VA_SCENE_PTR);
    if (scene && moved)
        *((volatile unsigned char *)scene + 0x22D) = 1;
    g_cam_moved = 1;
    g_outdate(cam);
    /* Rebuild the camera matrices now, like Camera::set_loc does (vtable slot 2,
       Camera::update_transforms). check_zoom runs inside Scene::render: with only
       outdate() the matrices would update next frame, while draw_build reads the new
       zoom distance right away, so buildings and terrain were a frame apart and shook. */
    void **vt = *(void ***)cam;
    ((thiscall0_t)vt[2])(cam);
}

static void mark_moved(void *cam) { mark_moved_ex(cam, 1); }

/* The screen centre on the ground, in world units (postfx.c's test pictures move it). */
int camera_get_loc(float *x, float *y)
{
    void *cam = main_camera();
    if (!cam || !g_outdate)
        return 0;
    *x = CAM_F(cam, OFF_LOC_X);
    *y = CAM_F(cam, OFF_LOC_Y);
    return 1;
}

void camera_set_loc(float x, float y)
{
    void *cam = main_camera();
    if (!cam || !g_outdate)
        return;
    CAM_F(cam, OFF_LOC_X) = x;
    CAM_F(cam, OFF_LOC_Y) = y;
    g_loc_bounds(cam);
    mark_moved(cam);
}

/* Camera::check_scroll runs once per frame before Scene::render: reset the per-frame
   "camera position changed" flag here; mark_moved_ex sets it. */
static void frame_begin(void) { g_cam_moved = 0; }

/* ---- scroll ------------------------------------------------------------------------ */

static struct {
    unsigned mask;    /* directiile cerute in cadrul curent */
    float amount;     /* ultimul amount primit de scroll_dir (<0 = niciunul) */
    float mult;       /* multiplicatorul jocului (slider-ul de viteza etc.) */
    float vx, vy;     /* viteza curenta, unitati de harta / secunda */
    double last;
} sc = { 0, -1.0f, 1.0f, 0, 0, 0 };

/* Inlocuieste Camera::scroll_dir: doar inregistreaza directia, miscarea o face check_scroll. */
__attribute__((used)) void c_scroll_dir(void *cam, int dir, float amount)
{
    if (dir >= 1 && dir <= 4)
        sc.mask |= 1u << dir;
    sc.amount = amount;
    CAM_I(cam, OFF_FOLLOW) = 0;
}

/* Distance of the extra step closer than the closest level: as far below level 6 as
   level 6 is below level 5 (in ratio), 0 if the levels are not set up. */
static float extra_in_dist(void *cam)
{
    float d6 = CAM_F(cam, OFF_LEVEL_DIST + 4 * (NUM_LEVELS - 1)), d5 = CAM_F(cam, OFF_LEVEL_DIST + 4 * (NUM_LEVELS - 2));
    return (d6 > 0.0f && d5 > d6) ? d6 * d6 / d5 : 0.0f;
}

static float scroll_speed_for_zoom(void *cam)
{
    int i;
    float t, d = CAM_F(cam, OFF_DIST);
    level_bracket(cam, d, &i, &t);
    float a = CAM_F(cam, OFF_SCROLL_SPEED + 4 * i), b = CAM_F(cam, OFF_SCROLL_SPEED + 4 * (i + 1));
    float speed = a + (b - a) * t;
    /* Closer than the closest level: slower in proportion, so the screen moves as fast. */
    float d6 = CAM_F(cam, OFF_LEVEL_DIST + 4 * (NUM_LEVELS - 1));
    if (d > 0.0f && d < d6)
        speed *= d / d6;
    return speed;
}

/* GetKeyboardState da starea tastelor din coada de mesaje a ferestrei. Daca o tasta e
   ridicata cand fereastra pierde focusul (alt-tab, overlay), jocul nu afla si camera
   merge singura pana se apasa alta tasta. Stergem tastele de scroll care nu sunt tinute
   fizic apasate. */
static void fix_stuck_keys(void)
{
    static const unsigned char vks[] = {
        VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN, VK_NUMPAD2, VK_NUMPAD4, VK_NUMPAD6, VK_NUMPAD8, VK_CONTROL, VK_MENU,
    };
    char *game = *(char **)VA(VA_GAME_PTR);
    if (!game)
        return;
    volatile unsigned char *keys = (volatile unsigned char *)(game + GAME_KEYS);
    for (unsigned i = 0; i < sizeof vks; i++)
        if ((keys[vks[i]] & 0x80) && !(GetAsyncKeyState(vks[i]) & 0x8000))
            keys[vks[i]] &= 0x7F;
}

__attribute__((used)) void c_check_scroll(void *self)
{
    frame_begin();
    if (g_qol_enabled)
        qol_tick(now_s());
    if (g_airdrop_enabled)
        airdrop_ui_tick();
    if (cfg.fix_stuck_keys)
        fix_stuck_keys();
    if (!cfg.smooth_scroll) {
        g_orig_check_scroll(self);
        return;
    }

    void *cam = main_camera();
    unsigned old_time = cam ? (unsigned)CAM_I(cam, OFF_SCROLL_TIME) : 0;

    sc.mask = 0;
    sc.amount = -1.0f;
    g_orig_check_scroll(self);
    if (!cam)
        return;

    double t = now_s();
    float dt = (float)(t - sc.last);
    sc.last = t;
    if (dt > 0.25f) { /* pauza lunga (incarcare, alt-tab): pornim de la zero */
        sc.vx = sc.vy = 0.0f;
        return;
    }
    if (dt <= 0.0f)
        return;
    if (dt > 0.1f)
        dt = 0.1f;

    /* Recuperam multiplicatorul jocului din amount = min(1, ms/134) * 63.9936 * mult. */
    unsigned diff_ms = (unsigned)CAM_I(cam, OFF_SCROLL_TIME) - old_time;
    if (sc.amount > 0.0f && diff_ms > 0) {
        float base = (float)diff_ms / 134.0f;
        if (base > 1.0f)
            base = 1.0f;
        sc.mult = sc.amount / (base * 63.9936f);
    }

    if ((CAM_I(cam, OFF_LOCK_FLAGS) & 1) || CAM_I(cam, OFF_PAN_STEPS) || (!sc.mask && CAM_I(cam, OFF_FOLLOW))) {
        sc.vx = sc.vy = 0.0f;
        return;
    }

    float sx = (float)(!!(sc.mask & (1u << DIR_RIGHT)) - !!(sc.mask & (1u << DIR_LEFT)));
    float sy = (float)(!!(sc.mask & (1u << DIR_UP)) - !!(sc.mask & (1u << DIR_DOWN)));
    float len = sqrtf(sx * sx + sy * sy);
    float tx = 0.0f, ty = 0.0f;
    if (len > 0.0f) {
        sx /= len;
        sy /= len;
        const int *cx = (const int *)VA(VA_CORNER_X), *cy = (const int *)VA(VA_CORNER_Y);
        float s = CAM_F(cam, OFF_DIR_SIN), c = CAM_F(cam, OFF_DIR_COS);
        float rx = cx[DIR_RIGHT] * s, ry = cy[DIR_RIGHT] * c;
        float ux = cx[DIR_UP] * s, uy = cy[DIR_UP] * c;
        /* Aceeasi viteza ca jocul original pe o singura directie (unitati/ms * 1000). */
        float speed = sc.mult * (63.9936f / 134.0f) * 1000.0f * scroll_speed_for_zoom(cam) * cfg.scroll_speed;
        sy *= cfg.vertical_speed;
        tx = (sx * rx + sy * ux) * speed;
        ty = (sx * ry + sy * uy) * speed;
    }

    float k = approach(dt, (tx != 0.0f || ty != 0.0f) ? cfg.scroll_accel_ms : cfg.scroll_decel_ms);
    sc.vx += (tx - sc.vx) * k;
    sc.vy += (ty - sc.vy) * k;
    if (tx == 0.0f && ty == 0.0f && sc.vx * sc.vx + sc.vy * sc.vy < 25.0f) {
        sc.vx = sc.vy = 0.0f;
        return;
    }

    float x0 = CAM_F(cam, OFF_LOC_X), y0 = CAM_F(cam, OFF_LOC_Y);
    float x1 = x0 + sc.vx * dt, y1 = y0 + sc.vy * dt;
    CAM_F(cam, OFF_LOC_X) = x1;
    CAM_F(cam, OFF_LOC_Y) = y1;
    g_loc_bounds(cam);
    if (CAM_F(cam, OFF_LOC_X) != x1)
        sc.vx = 0.0f;
    if (CAM_F(cam, OFF_LOC_Y) != y1)
        sc.vy = 0.0f;
    /* Refresh the camera transforms on every move, not only when the integer position
       changes like the original: small sub-unit moves while braking otherwise leave
       buildings drawn with last frame's camera, so they shake against the terrain. */
    if (CAM_F(cam, OFF_LOC_X) != x0 || CAM_F(cam, OFF_LOC_Y) != y0)
        mark_moved(cam);
}

/* ---- zoom si pan ------------------------------------------------------------------- */

static struct {
    int init;
    float dist;      /* distanta afisata */
    float vel;       /* viteza zoom-ului, in log(distanta) pe secunda */
    int anchor;      /* zoom spre cursor activ */
    float ax, ay;    /* punctul de pe harta de sub cursor */
    float built;     /* distance at the last rebuild of the terrain's visible set */
    double last;
} zm;

/* A zoom in at the closest level (6), where the game does nothing: take the extra step
   closer (check_zoom animates to it). 1 = taken. */
static int extra_in_step(void *cam)
{
    if (!g_extra_in_enabled || g_extra_in || (CAM_I(cam, OFF_ZOOM_LOCK) & 1) || extra_in_dist(cam) <= 0.0f)
        return 0;
    if (CAM_I(cam, OFF_LEVEL) != NUM_LEVELS - 1 || CAM_I(cam, OFF_LEVEL_TARGET) != NUM_LEVELS - 1)
        return 0;
    g_extra_in = 1;
    return 1;
}

/* Hook pe Camera::zoom_in(x, y, z) apelat de rotita: retinem punctul de sub cursor. */
__attribute__((used)) void c_zoom_in(void *self, int x, int y, int z)
{
    void *cam = main_camera();
    if (cam && extra_in_step(cam)) {
        /* Same placement as an ordinary step: toward the cursor, centred on it, or none. */
        if (!(CAM_I(cam, OFF_LOCK_FLAGS) & 8)) {
            if (cfg.zoom_to_cursor == 1) {
                zm.anchor = 1;
                zm.ax = (float)x;
                zm.ay = (float)y;
            } else if (cfg.zoom_to_cursor == 0) {
                CAM_I(cam, OFF_PAN_STEPS) = 1;
                CAM_I(cam, OFF_PAN_X) = x;
                CAM_I(cam, OFF_PAN_Y) = y;
            }
        }
        return;
    }
    int had_pan = cam ? CAM_I(cam, OFF_PAN_STEPS) : 0;
    g_orig_zoom_in(self, x, y, z);
    if (cam && cfg.zoom_to_cursor == 2 && !had_pan) {
        /* zoom_to_cursor=2: zoom on the screen centre, the camera does not move at all. */
        CAM_I(cam, OFF_PAN_STEPS) = 0;
        return;
    }
    if (!cam || !cfg.zoom_to_cursor)
        return;
    /* Originalul porneste un pan spre (x, y) ca sa centreze camera pe cursor. Il anulam
       si tinem punctul fix pe ecran in timp ce distanta scade. */
    if (!had_pan && CAM_I(cam, OFF_PAN_STEPS) && !(CAM_I(cam, OFF_LOCK_FLAGS) & 8)) {
        CAM_I(cam, OFF_PAN_STEPS) = 0;
        zm.anchor = 1;
        zm.ax = (float)x;
        zm.ay = (float)y;
    }
}

/* Camera::zoom_in() without a cursor (the zoom key, the wheel over the interface). */
__attribute__((used)) void c_zoom_in_key(void *self)
{
    void *cam = main_camera();
    if (cam && extra_in_step(cam))
        return;
    g_orig_zoom_in_key(self);
}

/* Camera::zoom_out(mask): from the extra step, the first zoom out goes back to level 6. */
__attribute__((used)) void c_zoom_out(void *self, int mask)
{
    void *cam = main_camera();
    if (cam && g_extra_in) {
        if (!(CAM_I(cam, OFF_ZOOM_LOCK) & mask)) {
            g_extra_in = 0;
            zm.anchor = 0;
        }
        return;
    }
    g_orig_zoom_out(self, mask);
}

static void update_tilt(void *cam, float d)
{
    if (CAM_F(cam, OFF_AUTO_TILT) == 0.0f || CAM_I(cam, OFF_AUTO_TILT_ON) == 0)
        return;
    int i;
    float t;
    level_bracket(cam, d, &i, &t);
    float a = CAM_F(cam, OFF_LEVEL_TILT + 4 * i), b = CAM_F(cam, OFF_LEVEL_TILT + 4 * (i + 1));
    CAM_F(cam, OFF_TILT) = a + (b - a) * t;
}

static void smooth_pan(void *cam, float dt)
{
    if (!CAM_I(cam, OFF_PAN_STEPS))
        return;
    if (CAM_I(cam, OFF_LOCK_FLAGS) & 8) {
        CAM_I(cam, OFF_PAN_STEPS) = 0;
        return;
    }
    float tx = (float)CAM_I(cam, OFF_PAN_X), ty = (float)CAM_I(cam, OFF_PAN_Y);
    float x = CAM_F(cam, OFF_LOC_X), y = CAM_F(cam, OFF_LOC_Y);
    float k = approach(dt, cfg.pan_smooth_ms);
    x += (tx - x) * k;
    y += (ty - y) * k;
    if (fabsf(tx - x) < 4.0f && fabsf(ty - y) < 4.0f) {
        x = tx;
        y = ty;
        CAM_I(cam, OFF_PAN_STEPS) = 0;
    }
    CAM_F(cam, OFF_LOC_X) = x;
    CAM_F(cam, OFF_LOC_Y) = y;
    g_loc_bounds(cam);
    mark_moved(cam);
}

/* Scene::render derives the sprites' world-units-per-pixel factors (scene +0x2f8/+0x2fc)
   from the INTEGER pixel distance between two projected points, so with a changing camera
   the result carries +-0.5 % of rounding noise per frame: every building changes size by
   a pixel each frame of a smooth zoom (the "vibration"). At the original discrete levels
   the computation ran once per level, so the error was constant and invisible. The hook
   below runs right after that computation and, while a zoom is in progress, keeps the
   factors from the last frame at rest. */
static float g_hold_sx, g_hold_sy;
static int g_hold_valid;
void *g_orig_scene_scale;

__attribute__((used)) void c_scene_scale(void)
{
    char *scene = *(char **)VA(VA_SCENE_PTR);
    if (!scene)
        return;
    float *sx = (float *)(scene + 0x2F8), *sy = (float *)(scene + 0x2FC);
    if ((g_zooming || g_cam_moved) && g_hold_valid) {
        *sx = g_hold_sx;
        *sy = g_hold_sy;
    } else if (*sx > 0.0f && *sy > 0.0f) {
        g_hold_sx = *sx;
        g_hold_sy = *sy;
        g_hold_valid = 1;
    }
}

/* Building and unit sprites are pre-rendered for the 7 zoom levels and sampled with
   point filtering (RenderState flag 0x400000 -> D3D11 MIN_MAG_MIP_POINT in
   D3D11GLGraphicCard::set_sampler_states). At the in-between scales of a smooth zoom,
   point sampling makes the texels shimmer. While zooming, the flag is cleared on the
   sprite render states so they are filtered linearly; at rest the original flag returns. */
#define VA_RENDER_STATES 0x00C11D2Cu /* Array<RenderState*>: +4 count, +0x10 data */
#define RS_FLAGS         0x50
#define RS_POINT_FILTER  0x400000
#define RS_BUILDINGS     26 /* render_states[0x68 / 4], used by GraphicPieces::draw_build */

static void set_zoom_filter(int zooming)
{
    static int applied, saved_count;
    static struct { char *rs; unsigned flags; } saved[64];
    if (!cfg.zoom_filter || zooming == applied)
        return;
    int count = *(int *)(VA(VA_RENDER_STATES) + 4);
    char **states = *(char ***)(VA(VA_RENDER_STATES) + 0x10);
    if (!states || count <= RS_BUILDINGS)
        return;
    if (zooming) {
        /* Every point-filtered state is switched (units, buildings, shadows share the look). */
        saved_count = 0;
        for (int i = 0; i < count && saved_count < 64; i++) {
            char *rs = states[i];
            if (rs && (*(unsigned *)(rs + RS_FLAGS) & RS_POINT_FILTER)) {
                saved[saved_count].rs = rs;
                saved[saved_count].flags = *(unsigned *)(rs + RS_FLAGS);
                saved_count++;
                *(unsigned *)(rs + RS_FLAGS) &= ~RS_POINT_FILTER;
            }
        }
        static int logged;
        if (!logged) {
            logged = 1;
            logf_("zoom filter: %d of %d render states are point-filtered (buildings = #%d, flags %08X)", saved_count,
                  count, RS_BUILDINGS, states[RS_BUILDINGS] ? *(unsigned *)(states[RS_BUILDINGS] + RS_FLAGS) : 0);
        }
    } else {
        for (int i = 0; i < saved_count; i++)
            *(unsigned *)(saved[i].rs + RS_FLAGS) = saved[i].flags;
        saved_count = 0;
    }
    applied = zooming;
}

/* TerrainOut::render (after check_zoom in Scene::render) rebuilds the visible ground only while
   camera +0x28C is set, and Scene::post_update_screen clears it at the end of every frame. A
   camera change whose flag was cleared before the ground saw it (a minimap click lands between
   frames), or a rebuild that did not finish the new area, left part of the screen black with
   only the objects drawn, until the camera moved again. So: any change of position, distance,
   tilt or direction since the last frame keeps the flag set for SETTLE_S more. */
#define SETTLE_S 0.3
static void settle_ground(void *cam, double t)
{
    static float last[6];
    static double changed = -1.0;
    static int logged;
    float now[6] = { CAM_F(cam, OFF_LOC_X), CAM_F(cam, OFF_LOC_Y), CAM_F(cam, OFF_DIST),
                     CAM_F(cam, OFF_TILT),  CAM_F(cam, OFF_DIR_COS), CAM_F(cam, OFF_DIR_SIN) };
    if (memcmp(now, last, sizeof now)) {
        if (cfg.log && logged < 5 && changed >= 0.0 && !CAM_I(cam, OFF_OUTDATED)) {
            logged++;
            logf_("ground: camera moved (%.0f, %.0f -> %.0f, %.0f) without an outdate, rebuilding", last[0],
                  last[1], now[0], now[1]);
        }
        memcpy(last, now, sizeof now);
        changed = t;
    }
    if (changed >= 0.0 && t - changed < SETTLE_S)
        CAM_I(cam, OFF_OUTDATED) = 1;
}

/* Inlocuieste Camera::check_zoom (apelat la fiecare cadru din Scene::render). */
__attribute__((used)) void c_check_zoom(void *self)
{
    void *cam = main_camera();
    if (self != cam) { /* harta din Conquest are camera ei: o lasam in pace */
        g_orig_check_zoom(self);
        return;
    }

    double t = now_s();
    float dt = (float)(t - zm.last);
    zm.last = t;
    int snap = !zm.init || dt > 0.5f || dt < 0.0f;
    if (dt > 0.1f || dt < 0.0f)
        dt = 0.1f;

    /* Nivelul logic se schimba imediat (ca in original, unde trecea cate un nivel pe
       cadru); doar distanta afisata se animeaza. */
    if (CAM_I(cam, OFF_ZOOM_DIR)) {
        int target = CAM_I(cam, OFF_LEVEL_TARGET);
        if (target < 0)
            target = 0;
        if (target > NUM_LEVELS - 1)
            target = NUM_LEVELS - 1;
        CAM_I(cam, OFF_LEVEL) = target;
        CAM_I(cam, OFF_ZOOM_DIR) = 0;
        CAM_I(cam, OFF_ZOOMED) = 1;
    }

    int level = CAM_I(cam, OFF_LEVEL);
    if (level < 0 || level >= NUM_LEVELS) {
        g_orig_check_zoom(self);
        return;
    }
    /* The game moved the camera to another level (or a new game put it exactly on one):
       the extra step closer is over. */
    if (level != NUM_LEVELS - 1 || (snap && CAM_F(cam, OFF_DIST) == CAM_F(cam, OFF_LEVEL_DIST + 4 * level)))
        g_extra_in = 0;
    float target = CAM_F(cam, OFF_LEVEL_DIST + 4 * level);
    float lo = CAM_F(cam, OFF_LEVEL_DIST + 4 * (NUM_LEVELS - 1)), hi = CAM_F(cam, OFF_LEVEL_DIST);
    float extra = g_extra_in_enabled ? extra_in_dist(cam) : 0.0f;
    if (extra > 0.0f) {
        lo = extra;
        if (g_extra_in)
            target = extra;
    }

    if (snap || !(zm.dist > 0.0f) || target <= 0.0f || lo <= 0.0f) {
        zm.dist = CAM_F(cam, OFF_DIST);
        zm.built = zm.dist;
        zm.vel = 0.0f;
        zm.init = 1;
        zm.anchor = 0;
    }

    float prev = zm.dist;
    float d = prev;
    if (d > 0.0f && target > 0.0f && d != target) {
        /* Critically damped spring (SmoothDamp) on log(distance): eases in and out, and a
           new wheel notch mid-zoom continues from the current speed instead of jerking. */
        float ld = logf(d), lt = logf(target);
        float st = cfg.zoom_smooth_ms > 1.0f ? cfg.zoom_smooth_ms / 1000.0f : 0.001f;
        float omega = 2.0f / st, x = omega * dt;
        float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        float change = ld - lt;
        float temp = (zm.vel + omega * change) * dt;
        zm.vel = (zm.vel - omega * temp) * e;
        ld = lt + (change + temp) * e;
        if ((change > 0.0f) == (ld - lt < 0.0f)) { /* never overshoot */
            ld = lt;
            zm.vel = 0.0f;
        }
        d = expf(ld);
        /* Snap once within 0.5 %: the asymptotic tail would otherwise keep sprites at a
           slightly non-integer scale for a while, which shimmers with point sampling. */
        if (fabsf(ld - lt) < 0.005f) {
            d = target;
            zm.vel = 0.0f;
        }
        if (d < lo)
            d = lo;
        if (d > hi)
            d = hi;
    }
    zm.dist = d;
    g_zooming = d != target;
    /* With sub-pixel sprites the linear filter is also needed while scrolling: with point
       sampling a fractional offset still rounds to whole texels. At the extra close step
       the sprites are enlarged 1.5 times, which point sampling draws as uneven blocks. */
    set_zoom_filter(d != target || g_extra_in || (g_subpixel_enabled && g_cam_moved));

    /* Diagnostic trace of the first zooms of a session ([debug] zoom_trace): one line per
       frame while the distance moves, plus a few frames after it settles. */
    if (g_zoom_trace) {
        static int lines, settled;
        int moving = d != target;
        if (moving)
            settled = 0;
        else
            settled++;
        if (cfg.log && lines < 600 && (moving || settled <= 10)) {
            char path[MAX_PATH];
            snprintf(path, sizeof path, "%sron_zoom_trace.csv", g_dir);
            FILE *f = fopen(path, lines ? "a" : "w");
            if (f) {
                if (!lines)
                    fprintf(f, "t_ms,dt_ms,level,dist,target,tilt,loc_x,loc_y,scale_x,scale_y,cam_dirty\n");
                void *scene = *(void **)VA(VA_SCENE_PTR);
                float sx = scene ? *(float *)((char *)scene + 0x2F8) : 0, sy = scene ? *(float *)((char *)scene + 0x2FC) : 0;
                fprintf(f, "%.1f,%.2f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.5f,%.5f,%d\n", t * 1000.0, dt * 1000.0f, level, d,
                        target, CAM_F(cam, OFF_TILT), CAM_F(cam, OFF_LOC_X), CAM_F(cam, OFF_LOC_Y), sx, sy,
                        CAM_I(cam, 0x28C));
                fclose(f);
                lines++;
            }
        }
    }

    if (d != CAM_F(cam, OFF_DIST)) {
        int x0 = (int)CAM_F(cam, OFF_LOC_X), y0 = (int)CAM_F(cam, OFF_LOC_Y);
        CAM_F(cam, OFF_DIST) = d;
        update_tilt(cam, cfg.zoom_tilt ? d : target);
        if (zm.anchor && prev > 0.0f && d != prev) {
            /* Punctul de sub cursor ramane pe loc: centrul se apropie de el proportional. */
            float r = d / prev;
            CAM_F(cam, OFF_LOC_X) = zm.ax + (CAM_F(cam, OFF_LOC_X) - zm.ax) * r;
            CAM_F(cam, OFF_LOC_Y) = zm.ay + (CAM_F(cam, OFF_LOC_Y) - zm.ay) * r;
            g_loc_bounds(cam);
        }
        CAM_I(cam, OFF_ZOOMED) = 1;
        /* The terrain's visible set is rebuilt only on a move. A zoom shows ground outside
           it, which stayed black (sometimes half the screen) until the camera moved: rebuild
           when the zoom settles and every 10 % of distance on the way. */
        int rebuild = (int)CAM_F(cam, OFF_LOC_X) != x0 || (int)CAM_F(cam, OFF_LOC_Y) != y0 || d == target ||
                      fabsf(d - zm.built) > 0.1f * zm.built;
        if (rebuild)
            zm.built = d;
        mark_moved_ex(cam, rebuild);
    }
    if (d == target)
        zm.anchor = 0;

    smooth_pan(cam, dt);
    settle_ground(cam, t);
}

/* ---- stub-uri asm: salveaza toate registrele (codul jocului e compilat cu LTCG) ---- */

/* The game calls us with a stack aligned to 4 bytes only, while our C code is compiled
   assuming 16 (it uses movaps on stack locals). ENTER saves every register, keeps the
   original stack in ebx and aligns esp to 16 with the XMM registers saved on top:
     [ebx+0]  = registers from pushad, [ebx+32] = return address, [ebx+36] = 1st stack arg
     [esp+32] = saved xmm2 (scroll_dir passes its float argument in xmm2)
   Each stub then pads so esp is 16-aligned at the call (C expects that at the call site). */
#define ENTER                          \
    "    pushad\n"                     \
    "    mov ebx, esp\n"               \
    "    sub esp, 128\n"               \
    "    and esp, -16\n"               \
    "    movdqa [esp+0], xmm0\n"       \
    "    movdqa [esp+16], xmm1\n"      \
    "    movdqa [esp+32], xmm2\n"      \
    "    movdqa [esp+48], xmm3\n"      \
    "    movdqa [esp+64], xmm4\n"      \
    "    movdqa [esp+80], xmm5\n"      \
    "    movdqa [esp+96], xmm6\n"      \
    "    movdqa [esp+112], xmm7\n"
#define LEAVE                          \
    "    movdqa xmm0, [esp+0]\n"       \
    "    movdqa xmm1, [esp+16]\n"      \
    "    movdqa xmm2, [esp+32]\n"      \
    "    movdqa xmm3, [esp+48]\n"      \
    "    movdqa xmm4, [esp+64]\n"      \
    "    movdqa xmm5, [esp+80]\n"      \
    "    movdqa xmm6, [esp+96]\n"      \
    "    movdqa xmm7, [esp+112]\n"     \
    "    mov esp, ebx\n"               \
    "    popad\n"

void hk_check_scroll(void);
void hk_check_zoom(void);
void hk_scroll_dir(void);
void hk_zoom_in(void);
void hk_zoom_in_key(void);
void hk_zoom_out(void);
void hk_add_windows(void);
void hk_button_clicked(void);
void hk_scene_scale(void);

/* IFaceMain::add_windows: the in-game interface was built, add our buttons to it. */
__attribute__((used)) void c_add_windows(void *self)
{
    g_orig_add_windows(self);
    qol_create_buttons(self);
}

/* IFaceMainBase::on_button_clicked(Button *) */
__attribute__((used)) void c_button_clicked(void *self, void *button)
{
    if (!qol_button_clicked(button))
        ((void(__attribute__((thiscall)) *)(void *, void *))g_orig_button_clicked)(self, button);
}

__asm__(".intel_syntax noprefix\n"
        ".globl _hk_check_scroll\n"
        "_hk_check_scroll:\n" ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_check_scroll\n"
        "    add esp, 16\n" LEAVE
        "    ret\n"

        ".globl _hk_check_zoom\n"
        "_hk_check_zoom:\n" ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_check_zoom\n"
        "    add esp, 16\n" LEAVE
        "    ret\n"

        /* scroll_dir(dir) cu amount in xmm2, ret 4 */
        ".globl _hk_scroll_dir\n"
        "_hk_scroll_dir:\n" ENTER
        "    sub esp, 4\n"
        "    push dword ptr [esp+36]\n" /* amount: saved xmm2, now at esp+4+32 */
        "    push dword ptr [ebx+36]\n" /* dir */
        "    push ecx\n"
        "    call _c_scroll_dir\n"
        "    add esp, 16\n" LEAVE
        "    ret 4\n"

        /* zoom_in(x, y, z), ret 12 */
        ".globl _hk_zoom_in\n"
        "_hk_zoom_in:\n" ENTER
        "    push dword ptr [ebx+44]\n" /* z */
        "    push dword ptr [ebx+40]\n" /* y */
        "    push dword ptr [ebx+36]\n" /* x */
        "    push ecx\n"
        "    call _c_zoom_in\n"
        "    add esp, 16\n" LEAVE
        "    ret 12\n"

        /* zoom_in(), ret */
        ".globl _hk_zoom_in_key\n"
        "_hk_zoom_in_key:\n" ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_zoom_in_key\n"
        "    add esp, 16\n" LEAVE
        "    ret\n"

        /* zoom_out(mask), ret 4 */
        ".globl _hk_zoom_out\n"
        "_hk_zoom_out:\n" ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n" /* mask */
        "    push ecx\n"
        "    call _c_zoom_out\n"
        "    add esp, 16\n" LEAVE
        "    ret 4\n"

        /* TerrainVis::fill_fog_texcoords(ft, code), ret 8 */
        ".globl _hk_fill_fog_texcoords\n"
        "_hk_fill_fog_texcoords:\n" ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+40]\n" /* code */
        "    push dword ptr [ebx+36]\n" /* ft */
        "    push ecx\n"
        "    call _c_fill_fog_texcoords\n"
        "    add esp, 16\n" LEAVE
        "    ret 8\n"

        /* TerrainOut::invalidate_fog_cache(&x, &y), ret 8 */
        ".globl _hk_invalidate_fog_cache\n"
        "_hk_invalidate_fog_cache:\n" ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+40]\n" /* &y */
        "    push dword ptr [ebx+36]\n" /* &x */
        "    push ecx\n"
        "    call _c_invalidate_fog_cache\n"
        "    add esp, 16\n" LEAVE
        "    ret 8\n"

        /* IFaceMain::add_windows() */
        ".globl _hk_add_windows\n"
        "_hk_add_windows:\n" ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_add_windows\n"
        "    add esp, 16\n" LEAVE
        "    ret\n"

        /* GraphicPieces::draw_build(chad, rdata), ret 8, returns Liberr in eax */
        ".globl _hk_draw_build\n"
        "_hk_draw_build:\n" ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+40]\n" /* rdata */
        "    push dword ptr [ebx+36]\n" /* chad */
        "    push ecx\n"
        "    call _c_draw_build\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" /* into pushad's eax slot, so popad returns it */
        LEAVE
        "    ret 8\n"

        /* Mid-function hook in Scene::render, right after the sprite scale factors are
           written: run our fix, then continue through the trampoline (stolen `cmp`). */
        ".globl _hk_scene_scale\n"
        "_hk_scene_scale:\n" ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_scene_scale\n"
        "    add esp, 16\n" LEAVE
        "    jmp dword ptr [_g_orig_scene_scale]\n"

        /* BuildTypeOut::draw_template(tx, ty, a, b, player, ...): draw the build grid, then
           continue into the original through the trampoline (stolen prologue). */
        ".globl _hk_draw_template\n"
        "_hk_draw_template:\n" ENTER
        "    push dword ptr [ebx+52]\n" /* player */
        "    push dword ptr [ebx+40]\n" /* ty */
        "    push dword ptr [ebx+36]\n" /* tx */
        "    push ecx\n"
        "    call _c_draw_grid\n"
        "    add esp, 16\n" LEAVE
        "    jmp dword ptr [_g_orig_draw_template]\n"

        /* IFaceMainBase::on_button_clicked(Button *), ret 4 */
        ".globl _hk_button_clicked\n"
        "_hk_button_clicked:\n" ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n" /* button */
        "    push ecx\n"
        "    call _c_button_clicked\n"
        "    add esp, 16\n" LEAVE
        "    ret 4\n"

        /* GraphSys::init_display_quick(...): set the interface scale, then continue into
           the original through the trampoline (stolen prologue). */
        ".globl _hk_init_display_quick\n"
        "_hk_init_display_quick:\n" ENTER
        "    call _c_init_display_quick\n" LEAVE
        "    jmp dword ptr [_g_orig_init_display_quick]\n"

        /* d3dgl D3D11Context::change_resolution, after the scale reset: re-apply our scale,
           then run the two replaced instructions (mov ecx,edi / call close_for_restore). */
        ".globl _hk_ui_resolution_changed\n"
        "_hk_ui_resolution_changed:\n" ENTER
        "    sub esp, 12\n"
        "    push edi\n" /* the D3D11Context */
        "    call _c_ui_resolution_changed\n"
        "    add esp, 16\n" LEAVE
        "    mov ecx, edi\n"
        "    call dword ptr [_g_ui_close_for_restore]\n"
        "    jmp dword ptr [_g_ui_resume]\n"

        /* d3dgl D3D11Context::set_viewport(rect) with z range in xmm2/xmm3 (saved by ENTER):
           pick the scale for the bound target, then run the original. */
        ".globl _hk_ui_set_viewport\n"
        "_hk_ui_set_viewport:\n" ENTER
        "    sub esp, 32\n"             /* 5 arguments, esp kept 16-aligned */
        "    movss [esp+16], xmm3\n"    /* zmax */
        "    movss [esp+12], xmm2\n"    /* zmin */
        "    mov eax, [ebx+32]\n"
        "    mov [esp+8], eax\n"        /* caller (return address) */
        "    mov eax, [ebx+36]\n"
        "    mov [esp+4], eax\n"        /* rect */
        "    mov [esp], ecx\n"          /* the D3D11Context */
        "    call _c_ui_set_viewport\n"
        "    add esp, 32\n" LEAVE
        "    jmp dword ptr [_g_ui_orig_set_viewport]\n"

        /* d3dgl D3D11GLSprite::clip_verts_ordered: inset the sprite's texture coordinates,
           then continue into the original through the trampoline. */
        ".globl _hk_ui_clip_verts\n"
        "_hk_ui_clip_verts:\n" ENTER
        "    call _c_ui_inset_uvs\n" LEAVE
        "    jmp dword ptr [_g_ui_orig_clip_verts]\n"

        /* Mid-function hook in IFaceRenderManager::add_smooth: run the two replaced
           instructions here, inset the UVs in the game's frame, reload the two UVs the next
           call takes in xmm1/xmm2 and resume after the stolen bytes. */
        ".globl _hk_ui_iface_uvs\n"
        "_hk_ui_iface_uvs:\n"
        "    movaps xmm2, xmm0\n"
        "    movss dword ptr [ebp-0x30], xmm0\n" ENTER
        "    sub esp, 12\n"
        "    push ebp\n"
        "    call _c_ui_iface_uvs\n"
        "    add esp, 16\n" LEAVE
        "    movss xmm1, dword ptr [ebp-0x34]\n"
        "    movss xmm2, dword ptr [ebp-0x30]\n"
        "    jmp dword ptr [_g_ui_iface_resume]\n"
        ".att_syntax prefix\n");

/* ---- instalarea hook-urilor ------------------------------------------------------------ */

typedef struct {
    const char *name;
    UINT_PTR va;
    unsigned char sig[12];
    int sig_len;
    int steal; /* octeti copiati in trampolina (0 = functia e inlocuita complet) */
    void *hook;
    void **orig;
    int *enabled;
} hook_t;

static int g_on = 1;

static hook_t g_hooks[] = {
    { "scroll_dir", VA_SCROLL_DIR, { 0x55, 0x8B, 0xEC, 0x8B, 0x55, 0x08, 0x8B, 0x81, 0xE0, 0x02, 0x00, 0x00 }, 12, 0,
      (void *)hk_scroll_dir, NULL, &cfg.smooth_scroll },
    { "check_scroll", VA_CHECK_SCROLL, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x24, 0x53, 0x33, 0xDB }, 12, 9,
      (void *)hk_check_scroll, (void **)&g_orig_check_scroll, &g_hook_check_scroll },
    { "check_zoom", VA_CHECK_ZOOM, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x56, 0x8B, 0xF1 }, 9, 7,
      (void *)hk_check_zoom, (void **)&g_orig_check_zoom, &cfg.smooth_zoom },
    { "zoom_in", VA_ZOOM_IN_XY, { 0x55, 0x8B, 0xEC, 0x51, 0x56, 0x8B, 0x35 }, 7, 5,
      (void *)hk_zoom_in, (void **)&g_orig_zoom_in, &cfg.smooth_zoom },
    /* zoom_in(): mov eax,[ecx+0x294] / cmp eax,6 */
    { "zoom_in_key", VA_ZOOM_IN_KEY, { 0x8B, 0x81, 0x94, 0x02, 0x00, 0x00, 0x83, 0xF8, 0x06 }, 9, 6,
      (void *)hk_zoom_in_key, (void **)&g_orig_zoom_in_key, &g_extra_in_enabled },
    /* zoom_out: push ebp / mov ebp,esp / push ecx / mov eax,[gamec] (address relocated, copied whole) */
    { "zoom_out", VA_ZOOM_OUT, { 0x55, 0x8B, 0xEC, 0x51, 0xA1 }, 5, 9,
      (void *)hk_zoom_out, (void **)&g_orig_zoom_out, &g_extra_in_enabled },
    { "fill_fog_texcoords", 0x008B19F0u, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C, 0x8B, 0x45, 0x08, 0x8D, 0x55 }, 11, 6,
      (void *)hk_fill_fog_texcoords, &g_orig_fill_fog_texcoords, &g_fog_enabled },
    { "invalidate_fog_cache", 0x00866980u, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x83, 0xEC, 0x08, 0x56, 0x57 }, 11, 6,
      (void *)hk_invalidate_fog_cache, &g_orig_invalidate_fog_cache, &g_fog_enabled },
    /* Scene::render+0x3e1: cmp byte ptr [edi+0x228],0 (7 bytes, stolen) / jne (not stolen) */
    { "scene_scale", 0x008B3651u, { 0x80, 0xBF, 0x28, 0x02, 0x00, 0x00, 0x00, 0x75 }, 8, 7,
      (void *)hk_scene_scale, &g_orig_scene_scale, &cfg.smooth_zoom },
    /* draw_build: push ebx / mov ebx,esp / sub esp,8 | and esp,-8 */
    { "draw_build", 0x008E7610u, { 0x53, 0x8B, 0xDC, 0x83, 0xEC, 0x08, 0x83, 0xE4, 0xF8 }, 9, 6,
      (void *)hk_draw_build, &g_orig_draw_build, &g_hook_draw_build },
    /* draw_template: push ebp / mov ebp,esp / push -1 | push offset ehhandler (address relocated, not compared) */
    { "draw_template", 0x00634250u, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 6, 5,
      (void *)hk_draw_template, &g_orig_draw_template, &g_grid_enabled },
    /* add_windows: push ebp / mov ebp,esp / and esp,-8 / sub esp,0x2c */
    { "add_windows", 0x00809610u, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x2C }, 9, 9,
      (void *)hk_add_windows, (void **)&g_orig_add_windows, &g_qol_enabled },
    /* on_button_clicked: push ebp / mov ebp,esp / push ecx / test byte [abs],1 (address relocated, not compared) */
    { "on_button_clicked", 0x008113A0u, { 0x55, 0x8B, 0xEC, 0x51, 0xF6, 0x05 }, 6, 11,
      (void *)hk_button_clicked, &g_orig_button_clicked, &g_qol_enabled },
    /* GraphSys::init_display_quick: push ebp / mov ebp,esp / mov eax,fs:[0] */
    { "init_display_quick", 0x00459B70u, { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 }, 9, 9,
      (void *)hk_init_display_quick, &g_orig_init_display_quick, &g_hook_init_display },
    /* IFaceRenderManager::add_smooth+0x588: movaps xmm2,xmm0 / movss [ebp-0x30],xmm0 (stolen) | call */
    { "iface_uvs", 0x0051BC88u, { 0x0F, 0x28, 0xD0, 0xF3, 0x0F, 0x11, 0x45, 0xD0, 0xE8 }, 9, 8,
      (void *)hk_ui_iface_uvs, &g_orig_iface_uvs, &g_ui_scale_enabled },
};

/* Small in-place byte patches, applied only if the original bytes match. */
typedef struct {
    const char *name;
    UINT_PTR va;
    unsigned char orig[8], repl[8];
    int len;
    int *enabled;
} patch_t;

static patch_t g_patches[] = {
    /* Camera::zoom_out stops at level 4 (`lea edx,[edx*2+2]`, edx = 1 outside a replay) and
       at level 2 in a replay. `lea edx,[edx+2]` lets a game zoom out to level 3, the next
       one the game itself has (distance, tilt, scroll speed); replays still stop at 2. */
    { "extra zoom out", 0x00844001u, { 0x8D, 0x14, 0x55, 0x02, 0x00, 0x00, 0x00 },
      { 0x8D, 0x52, 0x02, 0x0F, 0x1F, 0x40, 0x00 }, 7, &cfg.extra_zoom },
    /* BuildOut::draw_underlays draws two circles around a selected building that attacks
       (Tower, Fort, City, ...), each only while the mouse is over a stat of the selection
       panel. The minimum range (the first number of RANGE, [type+0x200]; 0 for nearly all
       buildings) needs MouseOver +0x54 == 0xC0 and +0x58 == 0xD (1/2, 2/2); the attack range
       (the building's max_range, as a rounded square: draw_funky_rectangle_circle) needs
       MouseOver +4 == 1, +0x54 == 0xC0 and +0x58 == 3 (3/5..5/5). Dropping the `jne`s shows
       both whenever the building is selected. Display only. */
    { "range circles (1/5)", 0x0062C576u, { 0x0F, 0x85, 0xB4, 0x00, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_range_circles },
    { "range circles (2/5)", 0x0062C580u, { 0x0F, 0x85, 0xAA, 0x00, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_range_circles },
    { "range circles (3/5)", 0x0062C6D0u, { 0x0F, 0x85, 0x0E, 0x02, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_range_circles },
    { "range circles (4/5)", 0x0062C6DDu, { 0x0F, 0x85, 0x01, 0x02, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_range_circles },
    { "range circles (5/5)", 0x0062C6E7u, { 0x0F, 0x85, 0xF7, 0x01, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_range_circles },
    /* Unit::think_fish: an idle fishing boat looks for a free fish only when
       (id + game time) % 1024 == 0, so after losing its fish to another boat (or finding
       its way blocked) it sat idle for a long time, with the idle alert going off.
       `and ecx,0x800003ff` -> `and ecx,0x8000003f`: it looks again every 64 ticks. The
       search itself skips fish another boat is using. Game rule: same setting for all
       players in multiplayer. */
    { "fishing boats retry", 0x005F4C8Fu, { 0x81, 0xE1, 0xFF, 0x03, 0x00, 0x80 },
      { 0x81, 0xE1, 0x3F, 0x00, 0x00, 0x80 }, 6, &g_fish_retry },
    /* SetupWin::validate_start: with the SetupWin+0x1f28 byte set (seen with Cooperative AI),
       "Teams of 2 / 3 / 4" need exactly 4 / 6 / 8 players, otherwise "Not enough players
       for a 'Teams of N' game". Without it the game only asks for at least 3 / 5 / 7 and
       no team over N. The three `je` become `jmp`, so the second rule always applies. */
    { "coop ai teams (1/3)", 0x005BAEE8u, { 0x0F, 0x84, 0x69, 0x01, 0x00, 0x00 },
      { 0x90, 0xE9, 0x69, 0x01, 0x00, 0x00 }, 6, &g_coop_ai_teams },
    { "coop ai teams (2/3)", 0x005BAF9Fu, { 0x0F, 0x84, 0xB2, 0x00, 0x00, 0x00 },
      { 0x90, 0xE9, 0xB2, 0x00, 0x00, 0x00 }, 6, &g_coop_ai_teams },
    { "coop ai teams (3/3)", 0x005BB050u, { 0x74, 0x05 }, { 0xEB, 0x05 }, 2, &g_coop_ai_teams },
    /* GraphicPieces::draw_build passes the camera zoom distance to the renderer truncated
       to an int: `cvttss2si ecx,[ecx+0x2f0]` ... `movd xmm0,ecx; cvtdq2ps xmm0,xmm0`.
       With smooth zoom the distance is fractional, so buildings were scaled in steps and
       shook against the terrain. Keep the float: `mov ecx,[ecx+0x2f0]` and drop cvtdq2ps. */
    { "draw_build zoom (1/2)", 0x008E7A1Eu, { 0xF3, 0x0F, 0x2C, 0x89, 0xF0, 0x02, 0x00, 0x00 },
      { 0x8B, 0x89, 0xF0, 0x02, 0x00, 0x00, 0x66, 0x90 }, 8, &cfg.smooth_zoom },
    { "draw_build zoom (2/2)", 0x008E7A4Fu, { 0x0F, 0x5B, 0xC0 }, { 0x0F, 0x1F, 0x00 }, 3, &cfg.smooth_zoom },
    /* IFaceSelected::draw_hitpoints writes "current/max" hit points under the health bar of
       the selected object only with "Show Advanced Options" (profile flag 2), which also
       adds the advanced command buttons. Drop the `je` so the numbers always show. */
    { "show hit points", 0x00821174u, { 0x0F, 0x84, 0x31, 0x03, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_show_hp },
    /* IFaceSelected::update_selected shows a multiple selection one portrait per unit, and one
       portrait per type with its count ("type_buckets2") only when the units do not fit the
       panel's cells AND profile flag 0x20000 is set, and never for a single type. Always group:
       drop the "they fit" jump, the profile-flag jump and the cmove that turns grouping off when
       there is only one type. */
    { "group selection (1/3)", 0x00823B38u, { 0x0F, 0x8E, 0x55, 0x01, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_group_selection },
    { "group selection (2/3)", 0x00823B71u, { 0x0F, 0x84, 0x1C, 0x01, 0x00, 0x00 },
      { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 }, 6, &g_group_selection },
    { "group selection (3/3)", 0x00823C7Au, { 0x0F, 0x44, 0xC8 }, { 0x0F, 0x1F, 0x00 }, 3, &g_group_selection },
    /* Window::recurse_clips multiplies the screen rect, already in logical pixels, by
       1/scale again. With scale < 1 (the only case the game used) the intersection with the
       screen hid it; with scale > 1 top-level windows would be clipped to a fraction of the
       screen. Drop the four `mulss xmm0,[reg+0x358/0x35c]`. */
    { "recurse_clips (1/4)", 0x00A50253u, { 0xF3, 0x0F, 0x59, 0x81, 0x58, 0x03, 0x00, 0x00 },
      { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
    { "recurse_clips (2/4)", 0x00A50271u, { 0xF3, 0x0F, 0x59, 0x80, 0x58, 0x03, 0x00, 0x00 },
      { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
    { "recurse_clips (3/4)", 0x00A50290u, { 0xF3, 0x0F, 0x59, 0x82, 0x5C, 0x03, 0x00, 0x00 },
      { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
    { "recurse_clips (4/4)", 0x00A502AFu, { 0xF3, 0x0F, 0x59, 0x81, 0x5C, 0x03, 0x00, 0x00 },
      { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
    /* The main menu positions its logo and centred boxes from the physical screen size
       (graphsys +0x2f8/+0x2fc); everything else uses the logical one (+0x360/+0x364). */
    { "main menu size (1/6)", 0x0059C135u, { 0x81, 0xB8, 0xFC, 0x02, 0x00, 0x00 },
      { 0x81, 0xB8, 0x64, 0x03, 0x00, 0x00 }, 6, &g_ui_scale_enabled },
    { "main menu size (2/6)", 0x0059CEAAu, { 0x8B, 0x80, 0xF8, 0x02, 0x00, 0x00 },
      { 0x8B, 0x80, 0x60, 0x03, 0x00, 0x00 }, 6, &g_ui_scale_enabled },
    { "main menu size (3/6)", 0x0059DE9Cu, { 0x66, 0x0F, 0x6E, 0x90, 0xF8, 0x02, 0x00, 0x00 },
      { 0x66, 0x0F, 0x6E, 0x90, 0x60, 0x03, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
    { "main menu size (4/6)", 0x0059F1CEu, { 0x66, 0x0F, 0x6E, 0x90, 0xF8, 0x02, 0x00, 0x00 },
      { 0x66, 0x0F, 0x6E, 0x90, 0x60, 0x03, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
    { "main menu size (5/6)", 0x005A0E35u, { 0x8B, 0x8A, 0xFC, 0x02, 0x00, 0x00 },
      { 0x8B, 0x8A, 0x64, 0x03, 0x00, 0x00 }, 6, &g_ui_scale_enabled },
    { "main menu size (6/6)", 0x005A0E5Du, { 0x66, 0x0F, 0x6E, 0x82, 0xF8, 0x02, 0x00, 0x00 },
      { 0x66, 0x0F, 0x6E, 0x82, 0x60, 0x03, 0x00, 0x00 }, 8, &g_ui_scale_enabled },
};

static void apply_patches(void)
{
    int n = (int)(sizeof g_patches / sizeof g_patches[0]);
    for (int i = 0; i < n; i++) {
        patch_t *p = &g_patches[i];
        if (!*p->enabled)
            continue;
        unsigned char *at = (unsigned char *)VA(p->va);
        if (memcmp(at, p->orig, p->len) != 0) {
            logf_("patch %s: bytes differ, skipped", p->name);
            continue;
        }
        DWORD old;
        VirtualProtect(at, p->len, PAGE_EXECUTE_READWRITE, &old);
        memcpy(at, p->repl, p->len);
        VirtualProtect(at, p->len, old, &old);
        logf_("patch %s applied", p->name);
        if (!strcmp(p->name, "draw_build zoom (1/2)"))
            g_dist_patch_applied = 1;
    }
}

static void write_jmp(unsigned char *at, const void *to)
{
    at[0] = 0xE9;
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)to - ((UINT_PTR)at + 5));
}

void hook_jmp5(unsigned char *at, const void *to)
{
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    write_jmp(at, to);
    VirtualProtect(at, 5, old, &old);
}

void *hook_trampoline(unsigned char *at, int n, unsigned char *mem)
{
    memcpy(mem, at, n);
    write_jmp(mem + n, at + n);
    return mem;
}

/* Address space in use, once a minute of running simulation (from airdrop.c's
   Objects::process_all). Without the large-address-aware flag the game has 2 GB, and a long
   8-player game ran out at 46 minutes: "Could not create texture: The GPU device instance has
   been suspended", then exception C00001AD (2026-10-30). */
void mem_frame(void)
{
    static DWORD last;
    DWORD now = GetTickCount();
    if (last && now - last < 60000)
        return;
    last = now;
    MEMORYSTATUSEX m = { sizeof m };
    if (GlobalMemoryStatusEx(&m))
        logf_("memory: %u of %u MB address space in use", (unsigned)((m.ullTotalVirtual - m.ullAvailVirtual) >> 20),
              (unsigned)(m.ullTotalVirtual >> 20));
}

static void install_hooks(void)
{
    unsigned char *exe = (unsigned char *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(exe + ((IMAGE_DOS_HEADER *)exe)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp == LAUNCHER_TIMESTAMP) {
        launcher_install();
        return;
    }
    if (nt->FileHeader.TimeDateStamp != EXE_TIMESTAMP) {
        logf_("unknown exe (timestamp %08lX), hooks NOT installed", nt->FileHeader.TimeDateStamp);
        return;
    }
    g_delta = (UINT_PTR)exe - EXE_PREF_BASE;
    if (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE)
        logf_("riseofnations.exe: 4 GB flag on");
    else
        logf_("riseofnations.exe: NO 4 GB flag, the game has 2 GB (install.bat sets it)");

    /* Hooks that share an `enabled` flag form one feature: a mismatch turns that feature
       off and leaves the others working. */
    int n = (int)(sizeof g_hooks / sizeof g_hooks[0]);
    for (int i = 0; i < n; i++) {
        if (*g_hooks[i].enabled && memcmp((void *)VA(g_hooks[i].va), g_hooks[i].sig, g_hooks[i].sig_len) != 0) {
            logf_("signature mismatch at %s, that feature is turned off", g_hooks[i].name);
            *g_hooks[i].enabled = 0;
        }
    }
    /* Smooth scrolling needs both scroll hooks; buttons need the per-frame tick in check_scroll. */
    if (!g_hook_check_scroll)
        cfg.smooth_scroll = g_qol_enabled = 0;
    if (!cfg.smooth_scroll && !cfg.fix_stuck_keys && !g_qol_enabled && !g_airdrop_enabled)
        g_hook_check_scroll = 0;
    /* The extra close step is animated by the check_zoom replacement. */
    if (!cfg.smooth_zoom)
        g_extra_in_enabled = 0;

    g_outdate = (thiscall0_t)VA(VA_OUTDATE);
    g_loc_bounds = (thiscall0_t)VA(VA_LOC_BOUNDS);

    unsigned char *tramp = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) {
        logf_("VirtualAlloc failed");
        return;
    }
    unsigned char *tp = tramp;

    for (int i = 0; i < n; i++) {
        hook_t *h = &g_hooks[i];
        if (!*h->enabled)
            continue;
        unsigned char *target = (unsigned char *)VA(h->va);
        if (h->steal) {
            memcpy(tp, target, h->steal);
            write_jmp(tp + h->steal, target + h->steal);
            *h->orig = tp;
            tp += h->steal + 5;
        }
        DWORD old;
        VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old);
        write_jmp(target, h->hook);
        VirtualProtect(target, 5, old, &old);
        logf_("hook %s installed", h->name);
    }
    apply_patches();
    lang_install(); /* before the game reads its prefs and opens any XML file */
    romania_install(); /* before the exe's static constructors build the Tribes array */
    kills_install();
    hud_install();
    flags_install();
    ngrid_install();
    qol_install();
    override_install();
    ai_names_install();
    armageddon_install();
    corpses_install();
    animfix_install();
    nukefx_install();
    nonukes_install();
    attrition_install();
    notices_install();
    survival_install();
    assistant_install();
    assist_orders_install();
    dc_install();
    entrench_install();
    airdrop_install();
    airstrike_install();
    ranks_install();
    rankfx_install();
    heal_install();
    make_way_install();
    formation_install();
    hover_install();
    squad_hp_install();
    outline_install();
    bribe_install();
    follow_install();
    rally_install();
    costs_install();
    prodbar_install();
    facing_install();
    play_install();
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}

/* ---- DllMain ------------------------------------------------------------------------- */

static HMODULE g_real_winmm;

static int load_real_winmm(void)
{
    char path[MAX_PATH];
    UINT len = GetSystemDirectoryA(path, MAX_PATH); /* proces 32-bit: redirectat la SysWOW64 */
    if (len == 0 || len > MAX_PATH - 12)
        return 0;
    strcat(path, "\\winmm.dll");
    g_real_winmm = LoadLibraryA(path);
    if (!g_real_winmm)
        return 0;
    for (int i = 0; i < WINMM_PROC_COUNT; i++) {
        const char *name = g_winmm_exports[i].name;
        g_winmm_procs[i] = (void *)GetProcAddress(g_real_winmm, name ? name : (LPCSTR)(UINT_PTR)g_winmm_exports[i].ordinal);
    }
    return 1;
}

/*
 * Startup hang (since the Windows updates of 2026-10-13 and Steam 2026-10-21): about every
 * second start, riseofnations.exe stays in memory with no window. SteamAPI_Init loads
 * steamclient.dll, whose DllMain asks bcrypt for random bytes while holding the loader lock;
 * a PartyWin.dll thread is at the same time inside bcrypt's first-use setup, which waits for
 * the loader. Neither goes on. bcrypt's setup runs only once per process, so doing it here,
 * while the process starts and no other thread may run yet, leaves nothing for them to wait on.
 */
static void warm_up_bcrypt(void)
{
    unsigned char b[16];
    BCryptGenRandom(NULL, b, sizeof b, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;
    DisableThreadLibraryCalls(inst);
    warm_up_bcrypt();

    DWORD len = GetModuleFileNameA(inst, g_dir, MAX_PATH);
    while (len > 0 && g_dir[len - 1] != '\\')
        len--;
    g_dir[len] = 0;

    if (!load_real_winmm())
        return FALSE;

    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpc_freq = (double)f.QuadPart;

    /* One session per log: the last one is kept as ron_camera.old.log (it grew without end) */
    char log_path[MAX_PATH], old_path[MAX_PATH];
    snprintf(log_path, sizeof log_path, "%sron_camera.log", g_dir);
    snprintf(old_path, sizeof old_path, "%sron_camera.old.log", g_dir);
    MoveFileExA(log_path, old_path, MOVEFILE_REPLACE_EXISTING);

    load_config();
    if (g_fog_enabled)
        fog_init();
    postfx_init();
    if (g_on)
        install_hooks();
    return TRUE;
}
