/*
 * Build grid: while a building is being placed, the tiles of your own territory around
 * the cursor are drawn as a grid, green where that building can stand and red where the
 * tile is blocked (trees, resources, buildings, water, cliffs, ...).
 *
 * BuildTypeOut::draw_template draws the footprint under the cursor every frame of
 * placement mode. The hook runs just before it and draws the grid the same way: the
 * terrain triangles of each tile (TerrainOut::get_mesh: two triangles split from corner
 * (1,0) to (0,1), 192 units per tile) with per-vertex colours, through the graphic card
 * with the footprint's render state, so the game's footprint is drawn on top.
 *
 * Per tile, free/blocked is the game's own rule for that building type:
 * BuildTypeData::blocked_tcoord, the check blocked_site runs on every footprint tile.
 * Territory comes from the world cells (one per 4x4 tiles), as in non_friendly_territory.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define VA_WORLD_PTR       0x00C061D0u /* GameAccessConst::worldc */
#define VA_LEADERS_PTR     0x00C061E0u /* GameAccessConst::leadersc: LeaderData[8], 0x6eec each */
#define VA_TERRAIN_PTR     0x00C06218u /* MiscAccess::terrain */
#define VA_CARD_PTR        0x00C0AEACu /* the_card + 4: D3D11GLGraphicCard* */
#define VA_RENDER_STATES   0x00C11D2Cu /* Array<RenderState*>: +0x10 data */
#define VA_GRAPHSYS_PTR    0x00E335A0u
#define VA_SORT_DEPTHS     0x00C8C310u /* int sort_depths[] */
#define VA_BLOCKED_TCOORD  0x00636DB0u /* BuildTypeData::blocked_tcoord(TCoord x, TCoord y, int player, int) */
#define VA_TRANSFORM_IDENT 0x00420040u /* Transform<float>::ident() */
#define VA_VERT3_VFT       0x00B24614u /* Vert3Array */
#define VA_COLORS_VFT      0x00B2D6ECu /* SimpleArray<unsigned long> */

#define WORLD_W_CELLS 0x000 /* int: width in cells (4 tiles each) */
#define WORLD_TILES_W 0x018
#define WORLD_TILES_H 0x01C
#define WORLD_CELLS   0x134 /* 28 bytes per cell, +0xf = territory owner (signed char, <0 = none) */
#define CELL_SIZE     28
#define CELL_OWNER    0x0F
#define LEADER_SIZE   0x6EEC
#define LEADER_ID     0x08  /* the value territory cells hold for this player */
#define TERRAIN_HEIGHTS 0x4A4 /* float[], (cells*4 + 1) per row: one height per tile corner */
#define GRAPHSYS_SORT 0x164
#define RS_TEMPLATE   19    /* render_states[0x4c / 4], used by draw_template */
#define TILE          192.0f

/* D3D11GLGraphicCard vtable, in bytes */
#define GC_FLUSH        0x18
#define GC_SET_TEXTURE  0x20
#define GC_SET_RSTATE   0x24
#define GC_SET_WORLD    0x28
#define GC_SET_PRIM     0x2C
#define GC_SET_VERTS    0x34
#define GC_SET_COLORS   0x48
#define GC_SET_TEX      0x4C
#define GC_SET_INDEX    0x60
#define GC_ADD_PRIM     0x80
#define PRIM_TRIANGLES  2

typedef int(__attribute__((thiscall)) *fn_blocked)(void *self, int x, int y, int player, int flag);
typedef int(__attribute__((thiscall)) *fn_v)(void *self);
typedef int(__attribute__((thiscall)) *fn_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_ii)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn_p)(void *self, const void *p);
typedef int(__attribute__((thiscall)) *fn_pi)(void *self, const void *p, int a);
#define CALL(type, va) ((type)VA(va))
#define CARD(type, off) ((type)(*(void ***)card)[(off) / 4])

int g_grid_enabled;
void *g_orig_draw_template;

static int g_radius, g_alpha;
static float g_line;

void grid_load_config(const char *ini_path)
{
    char buf[64];
    g_grid_enabled = GetPrivateProfileIntA("grid", "build_grid", 1, ini_path);
    g_radius = GetPrivateProfileIntA("grid", "grid_radius", 16, ini_path);
    g_alpha = GetPrivateProfileIntA("grid", "grid_alpha", 90, ini_path);
    GetPrivateProfileStringA("grid", "grid_line", "0.08", buf, sizeof buf, ini_path);
    g_line = (float)atof(buf);
    if (g_radius < 2)
        g_radius = 2;
    if (g_radius > 40)
        g_radius = 40;
    if (g_alpha < 10)
        g_alpha = 10;
    if (g_alpha > 255)
        g_alpha = 255;
    if (g_line < 0.0f)
        g_line = 0.0f;
    if (g_line > 0.3f)
        g_line = 0.3f;
}

/* Same layout as the game's Array: vftable, count, capacity, grow, data. The graphic card
   only reads count (+4) and data (+0x10). */
typedef struct {
    UINT_PTR vft;
    int count, capacity;
    short grow, pad;
    void *data;
    int flag;
} game_array;

#define BATCH 3000 /* vertices per add_prim; the card's stream holds 16383 */
static float g_verts[BATCH][3];
static unsigned g_colors[BATCH];
static game_array g_vert_arr, g_color_arr;
static _Alignas(16) unsigned char g_ident[0x40];

static void submit(void *card)
{
    if (!g_vert_arr.count)
        return;
    g_color_arr.count = g_vert_arr.count;
    CARD(fn_p, GC_SET_VERTS)(card, &g_vert_arr);
    CARD(fn_pi, GC_SET_TEX)(card, NULL, 0);
    CARD(fn_pi, GC_SET_COLORS)(card, &g_color_arr, 0);
    CARD(fn_i, GC_ADD_PRIM)(card, 0);
    g_vert_arr.count = 0;
}

static void vertex(void *card, float x, float y, float z, unsigned color)
{
    if (g_vert_arr.count >= BATCH)
        submit(card);
    float *v = g_verts[g_vert_arr.count];
    v[0] = x;
    v[1] = y;
    v[2] = z > 0.0f ? z : 0.0f; /* like draw_template: water tiles sit on the surface */
    g_colors[g_vert_arr.count] = color;
    g_vert_arr.count++;
}

/* Colour as the card takes it: A in the top byte, then B, G, R. */
static unsigned abgr(int r, int g, int b, float a)
{
    int ai = (int)(a + 0.5f);
    return (unsigned)ai << 24 | (unsigned)b << 16 | (unsigned)g << 8 | (unsigned)r;
}

/* Opacity at a point `d` tiles from the cursor: full inside, fading out over the last 3 tiles. */
static float fade(float d)
{
    float t = ((float)g_radius + 0.5f - d) / 3.0f;
    if (t <= 0.0f)
        return 0.0f;
    if (t >= 1.0f)
        return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* Called before BuildTypeOut::draw_template(tx, ty, ..., player, ...). */
__attribute__((used)) void c_draw_grid(char *type, int cx, int cy, int player)
{
    if (g_place_extra) /* placement.c's extra ghosts of a line: one grid is enough */
        return;
    char *world = *(char **)VA(VA_WORLD_PTR);
    char *leaders = *(char **)VA(VA_LEADERS_PTR);
    char *terrain = *(char **)VA(VA_TERRAIN_PTR);
    void *card = *(void **)VA(VA_CARD_PTR);
    char *graphsys = *(char **)VA(VA_GRAPHSYS_PTR);
    if (!type || !world || !leaders || !terrain || !card || !graphsys || player < 0 || player > 7)
        return;
    char *cells = *(char **)(world + WORLD_CELLS);
    const float *heights = *(const float **)(terrain + TERRAIN_HEIGHTS);
    char **states = *(char ***)(VA(VA_RENDER_STATES) + 0x10);
    int wc = *(int *)(world + WORLD_W_CELLS);
    int tw = *(int *)(world + WORLD_TILES_W), th = *(int *)(world + WORLD_TILES_H);
    if (!cells || !heights || !states || wc <= 0 || tw <= 0 || th <= 0)
        return;
    int me = *(int *)(leaders + player * LEADER_SIZE + LEADER_ID);
    int stride = wc * 4 + 1;

    static int logged;
    if (!logged) {
        logged = 1;
        logf_("build grid: first draw at tile %d,%d, player %d (id %d), map %dx%d tiles", cx, cy, player, me, tw, th);
    }

    int saved_sort = *(int *)(graphsys + GRAPHSYS_SORT);
    *(int *)(graphsys + GRAPHSYS_SORT) = ((int *)VA(VA_SORT_DEPTHS))[1]; /* as draw_template */

    g_vert_arr.vft = VA(VA_VERT3_VFT);
    g_vert_arr.data = g_verts;
    g_vert_arr.capacity = BATCH;
    g_vert_arr.count = 0;
    g_color_arr.vft = VA(VA_COLORS_VFT);
    g_color_arr.data = g_colors;
    g_color_arr.capacity = BATCH;
    CALL(fn_v, VA_TRANSFORM_IDENT)(g_ident);

    CARD(fn_ii, GC_SET_TEXTURE)(card, 0, 0);
    CARD(fn_ii, GC_SET_TEXTURE)(card, 1, 0);
    CARD(fn_p, GC_SET_RSTATE)(card, states[RS_TEMPLATE]);
    CARD(fn_p, GC_SET_WORLD)(card, g_ident);
    CARD(fn_p, GC_SET_INDEX)(card, NULL);
    CARD(fn_i, GC_SET_PRIM)(card, PRIM_TRIANGLES);

    const float g0 = g_line * 0.5f, g1 = 1.0f - g_line * 0.5f;
    for (int ty = cy - g_radius; ty <= cy + g_radius; ty++) {
        if (ty < 0 || ty >= th)
            continue;
        for (int tx = cx - g_radius; tx <= cx + g_radius; tx++) {
            if (tx < 0 || tx >= tw)
                continue;
            float dx = (float)(tx - cx), dy = (float)(ty - cy);
            if (fade(sqrtf(dx * dx + dy * dy) - 0.75f) <= 0.0f)
                continue;
            int owner = cells[((ty >> 2) * wc + (tx >> 2)) * CELL_SIZE + CELL_OWNER];
            if (owner != me)
                continue;

            int blocked = CALL(fn_blocked, VA_BLOCKED_TCOORD)(type, tx, ty, player, -1);
            int r = blocked ? 230 : 60, g = blocked ? 50 : 210, b = blocked ? 40 : 70;

            /* Heights of the tile corners and the two planes of its triangles. */
            const float *h = heights + ty * stride + tx;
            float h00 = h[0], h10 = h[1], h01 = h[stride], h11 = h[stride + 1];
            float u[4] = { g0, g1, g0, g1 }, v[4] = { g0, g0, g1, g1 };
            float px[4], py[4], pz[4];
            unsigned col[4];
            for (int k = 0; k < 4; k++) {
                float z = u[k] + v[k] <= 1.0f ? h00 + u[k] * (h10 - h00) + v[k] * (h01 - h00)
                                               : h11 + (1.0f - u[k]) * (h01 - h11) + (1.0f - v[k]) * (h10 - h11);
                px[k] = ((float)tx + u[k]) * TILE;
                py[k] = ((float)ty + v[k]) * TILE;
                pz[k] = z;
                float ddx = (float)tx + u[k] - (float)cx - 0.5f, ddy = (float)ty + v[k] - (float)cy - 0.5f;
                col[k] = abgr(r, g, b, (float)g_alpha * fade(sqrtf(ddx * ddx + ddy * ddy)));
            }
            /* Same split as the terrain: (0,0) (1,0) (0,1) and (0,1) (1,0) (1,1). */
            static const int order[6] = { 0, 1, 2, 2, 1, 3 };
            for (int k = 0; k < 6; k++)
                vertex(card, px[order[k]], py[order[k]], pz[order[k]], col[order[k]]);
        }
    }
    submit(card);
    CARD(fn_v, GC_FLUSH)(card);
    *(int *)(graphsys + GRAPHSYS_SORT) = saved_sort;
}
