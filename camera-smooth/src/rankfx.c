/*
 * Promotion effect: when a unit or a defensive building reaches a new veteran rank (ranks.c), its
 * new insignia pops up over its head in a flash of the tier's colour (bronze, silver, gold), rises
 * while it spins twice around its vertical axis, comes to rest facing the camera and fades out;
 * sparks fly off it at the start. Drawing only: nothing of the game changes, so it is safe in
 * multiplayer whatever each player's setting ([ranks] promotion_effect).
 *
 * ranks.c's add_xp calls rankfx_promoted for the squad's captain (or the building) when the level
 * goes up; add_xp runs in the simulation, on every machine, so every player sees every promotion
 * they can see. The effects are drawn in Console::on_redraw (the map window's own redraw, an empty
 * `ret` in the game, called from GraphicWin::on_paint each frame with the render manager's origin
 * at the window), so they lie over the map and under every other window of the interface.
 *
 * The anchor is where the game puts the object's health bar: a unit's interpolated position from
 * its graphic (+0xF4 -> +0xC/+0x10/+0x14, previous +0x68/+0x6C/+0x70, blended by Game +0x854)
 * raised by the bar's height (0xF0, 0x150, 0x180 or 0x1E0 by the type, UnitOut::draw_health_bar);
 * a building's position (+0x10/+0x14/+0xC xor 0x63637) raised by 0x180. It is projected with the
 * same matrix and viewport maths as MiscAccess::world_to_console (subpixel.c), in float. The size
 * follows the zoom (closest level distance / current distance, as the sprites do).
 *
 * The textures are interface.xml IFACETEX "rank_1".."rank_10", "rank_glow" and "rank_spark"
 * (ui-theme\tools\build_rank_icons.py). Without them there is no effect.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define VA_CON_REDRAW    0x007D0250u /* Console::on_redraw(): `ret` and int3 padding (vtable +0xB8, the only reference) */
#define VA_ADD_SMOOTH    0x0051B700u /* IFaceRenderManager::add_smooth(tex, x xmm2, y xmm3, w, h, color, type), ret 0x14 */
#define VA_IFACE_RM      0x00EBD1C0u
#define VA_ACTIVE_CAMERA 0x00E335A8u /* BaseCamera::active_camera */
#define VA_GRAPHSYS      0x00E335A0u
#define VA_CONSOLE       0x00C06210u /* +0x298 the local player, +0x12A the y added to projected points */
#define VA_CAMERA        0x00C06200u
#define VA_GAMEC         0x00C061E8u /* GameAccessConst::gamec: +0x820 flags, +0x854 the frame's blend, +0x928 */
#define VA_OBJECTS       0x00C0618Cu

#define RM_ORIGIN_X  0x1194 /* int: the window's origin, added by add_smooth */
#define RM_ORIGIN_Y  0x1198
#define GS_DEPTH     0x164
#define GS_LOG_W     0x360
#define GS_LOG_H     0x364
#define GS_HEIGHT_K  0x16C
#define CAM_VIEWPROJ 0x240
#define CAM_DIST     0x2F0
#define CAM_DIST_CLOSE 0x338
#define CON_PLAYER   0x298
#define CON_Y_OFF    0x12A
#define GAME_FLAGS   0x820 /* 0x42: everything is shown (the health bar's test) */
#define GAME_BLEND   0x854
#define GAME_DIST_K  0x928

#define OBJ_FLAGS    0x08
#define OBJ_OWNER    0x09
#define OBJ_ID       0x0A
#define OBJ_Z        0x0C
#define OBJ_GFX      0xF4
#define COORD_KEY    0x63637
#define FIRST_BUILD  2000
#define VT_VISIBLE   0x48  /* units: (player, 0) seen by that player */
#define VT_IS        0xB8  /* ObjectData::is(int, int) */
#define VT_OBJMASK   0x148 /* ObjectData::has_objmask(int) */
#define VT_B_VISIBLE 0x17C /* buildings: (player) */

#define MAX_FX       24
#define T_IN         0.30f /* pop in */
#define T_SPIN       1.50f /* two turns, slowing down */
#define T_FADE       1.90f /* then fade out */
#define T_END        2.50f
#define T_SPARKS     0.90f
#define N_SPARKS     10

typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);

void *qol_iface_tex(const char *name);

static int g_mode = 1; /* 0 off, 1 every visible promotion, 2 the local player's only */
static int g_size_pct = 100;
UINT_PTR g_rf_add_smooth, g_rf_rm;

static struct {
    char *obj; /* the object the effect belongs to: ids are reused */
    int p, id, level;
    double t0;
} g_fx[MAX_FX];

/* 0xAABBGGRR */
static const unsigned g_tier[3] = { 0x327FCD, 0xDCD2CD, 0x28CDFF }; /* bronze, silver, gold */

void rankfx_load_config(const char *ini_path)
{
    g_mode = GetPrivateProfileIntA("ranks", "promotion_effect", 1, ini_path);
    g_size_pct = GetPrivateProfileIntA("ranks", "promotion_effect_size", 100, ini_path);
    if (g_size_pct < 25 || g_size_pct > 400)
        g_size_pct = 100;
}

static double seconds(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

/* From ranks.c's add_xp, in the simulation: o reached `level` (1..10). */
void rankfx_promoted(char *o, int level)
{
    if (!g_mode || !g_rf_rm || !o || level < 1 || level > 10)
        return;
    int p = (unsigned char)o[OBJ_OWNER], id = *(short *)(o + OBJ_ID);
    if (p > 7)
        return;
    int slot = 0;
    for (int i = 0; i < MAX_FX; i++) {
        if (g_fx[i].obj == o) { /* promoted again while the last one still shows */
            slot = i;
            break;
        }
        if (g_fx[i].t0 < g_fx[slot].t0)
            slot = i; /* the oldest (or a free one, t0 = 0) */
    }
    g_fx[slot].obj = o;
    g_fx[slot].p = p;
    g_fx[slot].id = id;
    g_fx[slot].level = level;
    g_fx[slot].t0 = seconds();
}

/* ---- drawing ------------------------------------------------------------------------------ */

void rf_add(const void *tex, float x, float y, float w, float h, unsigned color);
__asm__(".intel_syntax noprefix\n"
        ".globl _rf_add\n" /* prodbar.c's icons too */
        "_rf_add:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    movss xmm2, dword ptr [ebp+12]\n"
        "    movss xmm3, dword ptr [ebp+16]\n"
        "    push 1\n"                     /* type: textured, as IFaceButton draws icons */
        "    push dword ptr [ebp+28]\n"    /* color */
        "    push dword ptr [ebp+24]\n"    /* h */
        "    push dword ptr [ebp+20]\n"    /* w */
        "    push dword ptr [ebp+8]\n"     /* tex */
        "    mov ecx, [_g_rf_rm]\n"
        "    call [_g_rf_add_smooth]\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

/* A texture centred on (cx, cy), w x h logical pixels, colour 0xBBGGRR with alpha 0..1. */
static void quad(const void *tex, float cx, float cy, float w, float h, unsigned rgb, float alpha)
{
    if (alpha <= 0.0f || w < 0.5f || h < 0.5f)
        return;
    int a = (int)(alpha * 255.0f + 0.5f);
    rf_add(tex, cx - w * 0.5f, cy - h * 0.5f, w, h, (unsigned)(a > 255 ? 255 : a) << 24 | (rgb & 0xFFFFFF));
}

static float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static float ease_out(float t) { t = 1.0f - clamp01(t); return 1.0f - t * t * t; }
static float ease_out_back(float t) /* overshoots to about 1.1 before settling */
{
    t = clamp01(t) - 1.0f;
    return 1.0f + t * t * (2.70158f * t + 1.70158f);
}

static unsigned mix(unsigned a, unsigned b, float k) /* 0xBBGGRR, k of b */
{
    unsigned r = 0;
    for (int s = 0; s < 24; s += 8) {
        float va = (float)((a >> s) & 0xFF), vb = (float)((b >> s) & 0xFF);
        r |= (unsigned)(va + (vb - va) * k + 0.5f) << s;
    }
    return r;
}

/* The point over the object where the game draws its health bar, in the map window's logical
   pixels; 0 when it is behind the camera. */
static int anchor(char *o, float *sx, float *sy)
{
    const char *cam = *(const char **)VA(VA_ACTIVE_CAMERA), *gs = *(const char **)VA(VA_GRAPHSYS);
    const char *con = *(const char **)VA(VA_CONSOLE), *game = *(const char **)VA(VA_GAMEC);
    if (!cam || !gs || !con || !game)
        return 0;
    float x, y, z;
    if (*(short *)(o + OBJ_ID) < FIRST_BUILD) {
        char **gp = *(char ***)(o + OBJ_GFX);
        const int *g = gp && *gp ? (const int *)*gp : NULL;
        if (!g)
            return 0;
        float f = *(const float *)(game + GAME_BLEND);
        x = (float)g[0x68 / 4] * (1.0f - f) + (float)g[0x0C / 4] * f;
        y = (float)g[0x6C / 4] * (1.0f - f) + (float)g[0x10 / 4] * f;
        z = (float)g[0x70 / 4] * (1.0f - f) + (float)g[0x14 / 4] * f;
        int up = 0xF0;
        if (((fn1_t)(*(void ***)o)[VT_OBJMASK / 4])(o, 0x200000))
            up = 0x180;
        else if (((fn1_t)(*(void ***)o)[VT_OBJMASK / 4])(o, 0x1000))
            up = ((fn1_t)(*(void ***)o)[VT_OBJMASK / 4])(o, 0x10)           ? 0x1E0
                 : ((fn2_t)(*(void ***)o)[VT_IS / 4])(o, 0x3B, 0) ? 0x180
                                                                  : 0x150;
        z += (float)up;
    } else {
        x = (float)(*(int *)(o + 0x10) ^ COORD_KEY);
        y = (float)(*(int *)(o + 0x14) ^ COORD_KEY);
        z = (float)((*(int *)(o + OBJ_Z) ^ COORD_KEY) + 0x180);
    }
    /* MiscAccess::world_to_console */
    const float *m = (const float *)(cam + CAM_VIEWPROJ);
    float cx = m[4] * y + m[0] * x + m[8] * z + m[12];
    float cy = m[5] * y + m[1] * x + m[9] * z + m[13];
    float cw = m[7] * y + m[3] * x + m[11] * z + m[15];
    if (cw <= 0.0f)
        return 0;
    cx /= cw;
    cy /= cw;
    int w = *(const int *)(gs + GS_LOG_W);
    float h = (float)*(const int *)(gs + GS_LOG_H) * *(const float *)(gs + GS_HEIGHT_K);
    *sx = (cx + 1.0f) * (float)w * 0.5f;
    *sy = (1.0f - cy) * h * 0.5f + (float)*(const short *)(con + CON_Y_OFF);
    return 1;
}

/* What the zoom does to the size of things on the map: 1 at the closest level. */
static float zoom_ratio(void)
{
    const char *mcam = *(const char **)VA(VA_CAMERA), *game = *(const char **)VA(VA_GAMEC);
    if (!mcam || !game)
        return 0.6f;
    float closest = (float)(int)*(const float *)(mcam + CAM_DIST_CLOSE) * *(const float *)(game + GAME_DIST_K);
    float d = *(const float *)(mcam + CAM_DIST);
    return d > 0.0f && closest > 0.0f ? closest / d : 0.6f;
}

static int seen(char *o, int local)
{
    int p = (unsigned char)o[OBJ_OWNER];
    if (p == local)
        return 1;
    if (g_mode == 2)
        return 0;
    const char *game = *(const char **)VA(VA_GAMEC);
    if (game && (*(const unsigned char *)(game + GAME_FLAGS) & 0x42))
        return 1;
    if (*(short *)(o + OBJ_ID) < FIRST_BUILD)
        return ((fn2_t)(*(void ***)o)[VT_VISIBLE / 4])(o, local, 0) != 0;
    return ((fn1_t)(*(void ***)o)[VT_B_VISIBLE / 4])(o, local) != 0;
}

static void draw_one(int i, double now, int local, float base_h, const void *glow, const void *spark)
{
    char *o = object(g_fx[i].p, g_fx[i].id);
    float t = (float)(now - g_fx[i].t0);
    if (o != g_fx[i].obj || t >= T_END || t < 0.0f) {
        g_fx[i].obj = NULL;
        g_fx[i].t0 = 0;
        return;
    }
    char name[16];
    snprintf(name, sizeof name, "rank_%d", g_fx[i].level);
    const void *icon = qol_iface_tex(name);
    float x, y;
    if (!icon || !seen(o, local) || !anchor(o, &x, &y))
        return;
    char *rm = (char *)g_rf_rm;
    x -= (float)*(int *)(rm + RM_ORIGIN_X);
    y -= (float)*(int *)(rm + RM_ORIGIN_Y);

    unsigned tier = g_tier[g_fx[i].level >= 10 ? 2 : (g_fx[i].level - 1) / 3];
    float h = base_h, w = h * 48.0f / 56.0f;
    float pop = ease_out_back(t / T_IN);
    float fade = t < T_FADE ? 1.0f : 1.0f - (t - T_FADE) / (T_END - T_FADE);
    float alpha = clamp01(t / (T_IN * 0.5f)) * clamp01(fade);
    float cy = y - h * 0.75f - h * 1.1f * ease_out(t / T_END); /* starts just over the health bar */

    if (glow) {
        /* a ring of light that bursts outwards, then a steady halo that breathes */
        float burst = clamp01(t / 0.6f);
        quad(glow, x, cy, h * (0.8f + 2.6f * burst), h * (0.8f + 2.6f * burst), mix(tier, 0xFFFFFF, 0.35f),
             0.85f * (1.0f - burst));
        float halo = h * 1.9f * pop * (1.0f + 0.06f * sinf(t * 9.0f));
        quad(glow, x, cy, halo, halo, tier, 0.75f * alpha);
    }

    /* the turn about the vertical axis: the width follows the cosine; its back is darker */
    float angle = 4.0f * 3.14159265f * ease_out(t / T_SPIN), c = cosf(angle);
    float iw = w * pop * (fabsf(c) < 0.08f ? 0.08f : fabsf(c));
    quad(icon, x, cy, iw, h * pop, c >= 0.0f ? 0xFFFFFF : 0x8C8C8C, alpha);
    if (c >= 0.0f && glow) { /* a glint across the face as it turns towards the camera */
        float off = angle - 6.2831853f * floorf(angle / 6.2831853f + 0.5f); /* from facing the camera */
        float glint = clamp01(1.0f - fabsf(off) * 3.0f) * clamp01((T_SPIN - t) / 0.25f);
        quad(glow, x, cy, iw * 0.9f, h * 0.9f * pop, 0xFFFFFF, 0.45f * glint * alpha);
    }

    if (spark && t < T_SPARKS) {
        unsigned seed = (unsigned)(g_fx[i].id * 2654435761u) ^ (unsigned)g_fx[i].level * 40503u;
        float k = t / T_SPARKS;
        for (int s = 0; s < N_SPARKS; s++) {
            seed = seed * 1103515245u + 12345u;
            float jitter = (float)((seed >> 16) & 0xFF) / 255.0f;
            float dir = (float)s * (6.2831853f / N_SPARKS) + jitter * 0.6f;
            float dist = h * (0.9f + 0.8f * jitter) * ease_out(k);
            float sx = x + cosf(dir) * dist, sy = cy + sinf(dir) * dist * 0.8f + h * 0.9f * k * k;
            float size = h * (0.55f - 0.3f * k) * (s & 1 ? 0.8f : 1.0f);
            quad(spark, sx, sy, size, size, s & 1 ? 0xFFFFFF : mix(tier, 0xFFFFFF, 0.3f), 1.0f - k);
        }
    }
}

/* For prodbar.c's icons, drawn in the same redraw: where the health bar of o is, in the render
   manager's coordinates (its origin taken off), and the zoom's size factor. */
int rankfx_anchor(char *o, float *x, float *y)
{
    if (!g_rf_rm || !anchor(o, x, y))
        return 0;
    *x -= (float)*(int *)((char *)g_rf_rm + RM_ORIGIN_X);
    *y -= (float)*(int *)((char *)g_rf_rm + RM_ORIGIN_Y);
    return 1;
}

float rankfx_zoom(void) { return zoom_ratio(); }

/* A point where a health bar was drawn (the console's coordinates) in the render manager's. */
void rankfx_console_to_rm(float *x, float *y)
{
    if (!g_rf_rm)
        return;
    *x -= (float)*(int *)((char *)g_rf_rm + RM_ORIGIN_X);
    *y -= (float)*(int *)((char *)g_rf_rm + RM_ORIGIN_Y);
}

/* Console::on_redraw, in place of its `ret`: ecx = the console. */
__attribute__((thiscall)) static void rankfx_redraw(void *console)
{
    (void)console;
    static int logged;
    if (!logged) {
        logged = 1;
        logf_("rankfx: map window redraw reached, textures %s",
              qol_iface_tex("rank_1") ? "found" : "missing (interface.xml), no promotion effect");
    }
    int any = 0;
    for (int i = 0; i < MAX_FX; i++)
        any |= g_fx[i].obj != NULL;
    const char *con = *(const char **)VA(VA_CONSOLE), *gs = *(const char **)VA(VA_GRAPHSYS);
    if (!con || !gs)
        return;
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
    (*depth)++; /* in front of the window, still under the next one (flags.c does the same) */
    if (any) {
        double now = seconds();
        int local = *(const int *)(con + CON_PLAYER);
        float r = zoom_ratio(), h = 34.0f * r;
        h = (h < 18.0f ? 18.0f : h > 48.0f ? 48.0f : h) * (float)g_size_pct / 100.0f;
        const void *glow = qol_iface_tex("rank_glow"), *spark = qol_iface_tex("rank_spark");
        for (int i = 0; i < MAX_FX; i++)
            if (g_fx[i].obj)
                draw_one(i, now, local, h, glow, spark);
    }
    minimap_draw_frame(); /* the minimap is a child of the console, so it is drawn over this */
    prodbar_draw_icons();
    heal_draw_overhead();
    cityrepair_draw_overhead();
    ranks_draw_insignia_queue();
    ranks_draw_hero_marks(); /* the Survival heroes' medallions */
    (*depth)--;
}

/* Called from install_hooks once the exe is known to be the supported version. The redraw also
   carries prodbar.c's icons, so it is put in for either. */
void rankfx_install(void)
{
    if (g_mode && !g_ranks_enabled)
        g_mode = 0;
    if (!g_mode && !g_prodbar_enabled && !g_ranks_enabled && !g_heal_enabled &&
        !cityrepair_wanted() && !minimap_frame_wanted())
        /* ranks: the insignia beside health bars; heal: its crosses; city repair: its pliers; the minimap's frame */
        return;
    static const unsigned char sig[5] = { 0xC3, 0xCC, 0xCC, 0xCC, 0xCC }; /* ret, int3 padding */
    unsigned char *at = (unsigned char *)VA(VA_CON_REDRAW);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("rankfx: signature mismatch at Console::on_redraw, no promotion effect, no production icons");
        return;
    }
    g_rf_add_smooth = VA(VA_ADD_SMOOTH);
    g_rf_rm = VA(VA_IFACE_RM);
    hook_jmp5(at, (const void *)rankfx_redraw);
    if (g_mode)
        logf_("rankfx: promotion effect on (%s, size %d%%)", g_mode == 2 ? "own units only" : "every visible unit",
              g_size_pct);
    else
        logf_("rankfx: map window redraw hooked for the production icons only");
}
