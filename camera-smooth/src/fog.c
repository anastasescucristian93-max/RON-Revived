/*
 * Smooth fog of war.
 *
 * The game keeps fog in a grid of half-tile cells (values 0..3) and gives each terrain
 * vertex (5x5 per tile) the average of the 2x2 cells around it, quantized to 13 levels.
 * That value is a texture coordinate into art/fow_fade.tga, a 256x1 alpha ramp that
 * drops sharply between "explored" and "visible", so fog edges follow the tile grid.
 *
 * Here every vertex gets a Gaussian-weighted average of the cells around it (continuous,
 * no quantization), warped so that the game's own ramp produces a smooth falloff at the
 * edge of vision. Cells now influence tiles up to FOG_R cells away, so fog invalidation is
 * widened to those tiles as well.
 *
 * The warp alone is not enough: texture coordinates are interpolated linearly between
 * vertices and the ramp is read per pixel, so a cliff in the ramp still turns into a sharp
 * line that zigzags along the terrain triangles. The ramps (fow_fade.tga, and
 * fow_fade_dark.tga for the dark tilesets) are therefore rewritten with a straight line from
 * "explored" down to clear, and served to the game through the file overrides; the pixels
 * between vertices then interpolate the alpha itself.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define VA_FOG_WIDTH  0x00E8A79Cu /* WorldMapFog fog + 0x0c: cells per row */
#define VA_FOG_HEIGHT 0x00E8A7A0u /* fog + 0x10 */
#define VA_FOG_CELLS  0x00E8A894u /* fog + 0x104: BYTE*, (width + 2) per row, 1-cell border */
#define VA_SCENE_PTR  0x00C0620Cu

/* FogTransition */
#define FT_CUR   0x064 /* float[25], what is on screen now */
#define FT_DEST  0x0C8 /* float[25], target of the fade */
#define FT_VEL   0x12C /* float[25], fade speed */
#define FT_TILE_X 0x190
#define FT_TILE_Y 0x194

int g_fog_enabled;
static float g_sigma;
static float g_edge;
static int g_soft_edge;

void *g_orig_fill_fog_texcoords;
void *g_orig_invalidate_fog_cache;

typedef void(__attribute__((thiscall)) *fill_fog_t)(void *self, void *ft, int code);
typedef void(__attribute__((thiscall)) *invalidate_t)(void *self, int *x, int *y);

/* 1D weights of the cells around a vertex: the Gaussian integrated over each cell, cut at
   FOG_R cells from the vertex. Vertices on a cell boundary (even half-cell coordinate) and on a
   cell centre (odd) both integrate the same window, so neighbouring vertex rows get the same
   blur; with different kernels they alternated and the steep edge showed it as teeth.
   Taps start at cell (hx >> 1) - FOG_R. */
#define FOG_R 3
#define FOG_TAPS (2 * FOG_R + 1)
static float g_w_edge[FOG_TAPS], g_w_center[FOG_TAPS];

/* t -> t' so that fow_fade(t') follows the smoothed curve. */
#define WARP_N 1024
static float g_warp[WARP_N + 1];

void fog_load_config(const char *ini_path)
{
    g_fog_enabled = GetPrivateProfileIntA("fog", "smooth_fog", 1, ini_path);
    g_sigma = ini_float(ini_path, "fog", "fog_blur", 1.0f);
    g_edge = ini_float(ini_path, "fog", "fog_edge", 1.0f);
    g_soft_edge = GetPrivateProfileIntA("fog", "soft_edge", 1, ini_path);
    if (g_sigma < 0.3f)
        g_sigma = 0.3f;
    if (g_sigma > 1.5f)
        g_sigma = 1.5f;
    if (g_edge < 0.05f)
        g_edge = 0.05f;
    if (g_edge > 1.0f)
        g_edge = 1.0f;
}

/* Below this ramp position ("explored" and darker) the game's ramps are kept. */
#define RAMP_T1 (2.0f / 3.0f)

/* Reads a whole 256x1 32-bit TGA ramp into `file`; returns its pixels, or NULL. */
static unsigned char *read_ramp(const char *path, unsigned char *file, int cap, int *size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    *size = (int)fread(file, 1, cap, f);
    fclose(f);
    const unsigned char *hdr = file;
    if (*size < 18 || hdr[2] != 2 || hdr[16] != 32 || hdr[12] != 0 || hdr[13] != 1 || hdr[14] != 1 ||
        hdr[15] != 0 || *size < 18 + hdr[0] + 256 * 4)
        return NULL;
    return file + 18 + hdr[0];
}

static float ramp_at(const unsigned char a[256], float t);

/* Writes art\<name> with its alpha straightened above RAMP_T1 to `dir` and sends the game
   there. `alpha` gets the ramp the game will use. */
static int straighten_ramp(const char *name, const char *dir, unsigned char alpha[256])
{
    char path[MAX_PATH], rel[64];
    unsigned char file[4096];
    int size;
    snprintf(path, sizeof path, "%sart\\%s", g_dir, name);
    unsigned char *px = read_ramp(path, file, sizeof file, &size);
    if (!px)
        return 0;
    for (int i = 0; i < 256; i++)
        alpha[i] = px[i * 4 + 3];
    float a1 = ramp_at(alpha, RAMP_T1);
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        if (t > RAMP_T1)
            px[i * 4 + 3] = alpha[i] = (unsigned char)(a1 * (1.0f - t) / (1.0f - RAMP_T1) + 0.5f);
    }
    snprintf(path, sizeof path, "%s%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    int ok = (int)fwrite(file, 1, size, f) == size;
    ok &= fclose(f) == 0;
    if (!ok)
        return 0;
    snprintf(rel, sizeof rel, "art\\%s", name);
    override_redirect(rel, path);
    return 1;
}

/* The ramp the warp is built for: the straightened one when it can be served, else the game's. */
static int load_fade_ramp(unsigned char alpha[256])
{
    char dir[MAX_PATH], path[MAX_PATH];
    unsigned char file[4096], dark[256];
    int size;
    DWORD n = GetTempPathA(sizeof dir, dir);
    if (g_override_enabled && n > 0 && n < sizeof dir - 16) {
        strcat(dir, "RON Revived\\");
        CreateDirectoryA(dir, NULL);
        if (straighten_ramp("fow_fade.tga", dir, alpha)) {
            if (!straighten_ramp("fow_fade_dark.tga", dir, dark))
                logf_("fog: could not rewrite art\\fow_fade_dark.tga, dark maps keep the game's edge");
            return 1;
        }
        logf_("fog: could not rewrite art\\fow_fade.tga in %s, the edge will zigzag", dir);
    }
    snprintf(path, sizeof path, "%sart\\fow_fade.tga", g_dir);
    unsigned char *px = read_ramp(path, file, sizeof file, &size);
    for (int i = 0; px && i < 256; i++)
        alpha[i] = px[i * 4 + 3];
    return px != NULL;
}

static float ramp_at(const unsigned char a[256], float t)
{
    float x = t * 255.0f;
    if (x <= 0.0f)
        return a[0];
    if (x >= 255.0f)
        return a[255];
    int i = (int)x;
    float f = x - i;
    return a[i] + (a[i + 1] - a[i]) * f;
}

static void build_warp(void)
{
    unsigned char a[256];
    for (int i = 0; i <= WARP_N; i++)
        g_warp[i] = (float)i / WARP_N;
    if (!g_soft_edge)
        return;
    if (!load_fade_ramp(a)) {
        logf_("fog: could not read art\\fow_fade.tga, soft edge off");
        return;
    }
    /* Below 2/3 ("explored" and darker) keep the game's curve. From 2/3 to 1 ("visible")
       fade the remaining alpha out with a smoothstep over the middle `g_edge` of the band,
       instead of the game's late cliff. The rest of the band stays fully clear or fogged,
       so the inside of the vision area stays crisp. With the straightened ramp this is a
       closed form, but the search also serves the game's own ramp. */
    const float t1 = RAMP_T1;
    float a1 = ramp_at(a, t1);
    for (int i = 0; i <= WARP_N; i++) {
        float t = (float)i / WARP_N;
        if (t <= t1)
            continue;
        float s = ((t - t1) / (1.0f - t1) - (1.0f - g_edge) * 0.5f) / g_edge;
        s = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        float want = a1 * (1.0f - s * s * (3.0f - 2.0f * s));
        /* The ramp only goes down: find the first t' >= t1 where it reaches `want`. */
        float lo = t1, hi = 1.0f;
        for (int k = 0; k < 24; k++) {
            float mid = 0.5f * (lo + hi);
            if (ramp_at(a, mid) > want)
                lo = mid;
            else
                hi = mid;
        }
        g_warp[i] = hi;
    }
}

static float warp(float t)
{
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    float x = t * WARP_N;
    int i = (int)x;
    return g_warp[i] + (g_warp[i + 1] - g_warp[i]) * (x - i);
}

void fog_init(void)
{
    /* Cell i covers [i, i + 1]; the vertex sits at 0 (boundary) or 0.5 (centre) in cell
       units, relative to cell 0 = (hx >> 1). */
    float k = 1.0f / (g_sigma * sqrtf(2.0f));
    for (int t = 0; t < FOG_TAPS; t++) {
        for (int c = 0; c < 2; c++) {
            float p = c ? 0.5f : 0.0f;
            float lo = (float)(t - FOG_R) - p, hi = lo + 1.0f;
            if (lo < -FOG_R)
                lo = -FOG_R;
            if (hi > FOG_R)
                hi = FOG_R;
            float w = hi > lo ? 0.5f * (erff(hi * k) - erff(lo * k)) : 0.0f;
            (c ? g_w_center : g_w_edge)[t] = w;
        }
    }
    build_warp();
    logf_("fog: blur %.2f cells, edge %.2f, soft edge %s", g_sigma, g_edge, g_soft_edge ? "on" : "off");
}

static inline int cell(const unsigned char *cells, int w, int h, int x, int y)
{
    /* The grid has a 1-cell border: valid coordinates are -1..w and -1..h. */
    if (x < -1)
        x = -1;
    if (x > w)
        x = w;
    if (y < -1)
        y = -1;
    if (y > h)
        y = h;
    return cells[(y + 1) * (w + 2) + (x + 1)];
}

/* Fog value (0..1) at a vertex. Vertex coordinates are in half cells: even = cell boundary,
   odd = cell centre. */
static float sample(const unsigned char *cells, int w, int h, int hx, int hy)
{
    const float *wx = (hx & 1) ? g_w_center : g_w_edge;
    const float *wy = (hy & 1) ? g_w_center : g_w_edge;
    int x0 = (hx >> 1) - FOG_R, y0 = (hy >> 1) - FOG_R;

    float sum = 0.0f, wsum = 0.0f;
    for (int j = 0; j < FOG_TAPS; j++) {
        if (wy[j] == 0.0f)
            continue;
        float row = 0.0f, rw = 0.0f;
        for (int i = 0; i < FOG_TAPS; i++) {
            row += wx[i] * cell(cells, w, h, x0 + i, y0 + j);
            rw += wx[i];
        }
        sum += wy[j] * row;
        wsum += wy[j] * rw;
    }
    return sum / (wsum * 3.0f);
}

__attribute__((used)) void c_fill_fog_texcoords(void *self, char *ft, int code)
{
    ((fill_fog_t)g_orig_fill_fog_texcoords)(self, ft, code);

    const unsigned char *cells = *(const unsigned char **)VA(VA_FOG_CELLS);
    int w = *(int *)VA(VA_FOG_WIDTH), h = *(int *)VA(VA_FOG_HEIGHT);
    if (!cells || w <= 0 || h <= 0)
        return;
    int tx = *(int *)(ft + FT_TILE_X), ty = *(int *)(ft + FT_TILE_Y);

    float v[25];
    for (int b = 0; b < 5; b++)
        for (int a = 0; a < 5; a++)
            v[b * 5 + a] = warp(sample(cells, w, h, 4 * tx + a, 4 * ty + b));

    if (code != 0) { /* set immediately */
        memcpy(ft + FT_CUR, v, sizeof v);
        return;
    }

    /* Fade toward the new value: same speed rule as the game (scene+0x8c is the fade time). */
    memcpy(ft + FT_DEST, v, sizeof v);
    void *scene = *(void **)VA(VA_SCENE_PTR);
    unsigned fade = scene ? *(unsigned *)((char *)scene + 0x8C) : 0;
    float k = fade ? (float)fade / 30000.0f : 1.0f / 1000.0f;
    float *cur = (float *)(ft + FT_CUR), *vel = (float *)(ft + FT_VEL);
    for (int i = 0; i < 25; i++) {
        float d = (v[i] - cur[i]) * k;
        vel[i] = d > 0.0f ? (d > 0.001f ? d : 0.001f) : (d < -0.001f ? d : -0.001f);
    }
}

__attribute__((used)) void c_invalidate_fog_cache(void *self, int *px, int *py)
{
    invalidate_t orig = (invalidate_t)g_orig_invalidate_fog_cache;
    int x = *px, y = *py;
    if (x == -1) {
        orig(self, px, py);
        return;
    }
    int w = *(int *)VA(VA_FOG_WIDTH), h = *(int *)VA(VA_FOG_HEIGHT);
    /* Cell c reaches the vertices within FOG_R cells of it, i.e. tiles (c - 5) / 2 < t <
       (c + 4) / 2: tiles k - 2 .. k + 1 for c = 2k, k - 1 .. k + 2 for c = 2k + 1. One call per
       tile: the game invalidates the tile that contains cell (x, y). */
    int fx = (x & 1) ? -2 : -4, fy = (y & 1) ? -2 : -4;
    for (int dy = fy; dy <= fy + 6; dy += 2) {
        for (int dx = fx; dx <= fx + 6; dx += 2) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                continue;
            orig(self, &nx, &ny);
        }
    }
}
