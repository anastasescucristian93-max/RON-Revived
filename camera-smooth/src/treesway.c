/*
 * Trees sway: the tops of the trees lean a little to one side and back in the wind, the bases stay
 * where they are, with waves of wind rolling across the map. And trees darken and brighten
 * gradually as they leave and enter your sight.
 *
 * How the game draws trees (camera-smooth/ENGINE_NOTES.md, "Tree sway"): ObjectsOut::sort_trees
 * fills the visible trees into seven vertex batches, the VoidArrays at RTreeData+0x3B4 + k x 0x20
 * (RTreeData at GraphicPieces+0x60, +0x64 for the low-detail trees), only when the view changes. A
 * tree is a trunk part and three crown layers, each in its own batch, copied in cell by cell from a
 * per-cell cache, so the vertices of one tree are not together. A vertex is 8 floats: x, y, z
 * (world, z up), then colour and uv. TreeFlushMethod::execute uploads the batches to the card only
 * while RTreeData+0x128 is set (it clears it after), and draws the card's copy every frame.
 *
 * Every time an RTreeData's batches are uploaded and drawn (a pre-hook on TreeFlushMethod::execute,
 * vtable +0x14, so after any refill of that frame; from the draw_underlays hook, which runs
 * earlier, a refill later in the frame showed the trees upright for one frame) ts_flush looks at
 * the batches. When the game has refilled them (a size, place or our last values
 * changed) it keeps a copy of every vertex and its height above the ground there, taken as the
 * lowest vertex of all batches within about a tree's width (the trunk's base). Then it moves every
 * vertex from its copy along the screen's horizontal (world (1, -1, 0) / sqrt 2: the camera is
 * turned 45 degrees) by an angle times its height, a little more near the top, so the trunks bend
 * rather than tip, and sets +0x128 so the game uploads them again. The angle follows a wave that
 * changes smoothly across the map, so all parts of one tree move together. Every vertex is checked
 * for a change: the game also refills parts of a batch, or grows it into a new buffer with our
 * moved vertices copied along; a vertex found where we wrote one, or where one of ours rests, is
 * ours and keeps its place (its lean must not become its rest: that showed as a snap).
 *
 * The same pass eases the trees' colour: the game darkens trees out of sight (and brightens them
 * back) at once by rewriting their vertex colours (RTreeData::adjust_tree_colors, or a refill);
 * the colour shown follows the game's over about [fog] fog_fade_time seconds.
 *
 * Drawing only (the batches are the game's picture of its state, which is not touched), so it is
 * multiplayer-safe.
 */
#include "ron.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define VA_GRAPHIC_PIECES 0x00C06214u /* GraphicPieces *: +0x60, +0x64 RTreeData * */
#define VA_FLUSH_VTBL     0x00B55364u /* TreeFlushMethod vtable: +0x14 execute(RData *) */
#define VA_FLUSH_EXECUTE  0x00922350u

typedef int(__attribute__((thiscall)) *flush_t)(void *self, void *rd);
#define RT_BATCH  0x3B4               /* RTreeData: the first VoidArray drawn every frame */
#define RT_NBATCH 7                   /* ... and the next ones every 0x20 bytes */
#define RT_UPLOAD 0x128               /* RTreeData: 1 = upload the batches to the card this frame */
#define VA_DATA   0x00                /* float *: the vertices */
#define VA_COUNT  0x04
#define VA_STRIDE 0x14                /* floats per vertex (8) */

#define CELL 160.0f /* the grid for "the ground near a vertex", about a tree's width */
#define HASH 16384

/* Per vertex: where the game put it (x, y), its height (or z while being copied), where we last
   wrote it (x, y), the game's colour for it, the colour we last wrote (both ARGB bits), and the
   colour shown now (a, r, g, b as floats, easing towards the game's). */
#define REC 11
#define R_OX 0
#define R_OY 1
#define R_H 2
#define R_WX 3
#define R_WY 4
#define R_GAME 5
#define R_WROTE 6
#define R_SHOWN 7

typedef struct {
    float *data;        /* where the batch was when we copied it */
    int count, stride;
    float *rec;         /* REC floats for every vertex */
    int cap;
} batch_t;

int g_tree_sway_enabled; /* sway or colour fade */
static float g_amount;
static int g_fade;      /* [fog] fog_fade: trees darken and brighten gradually */
static float g_fade_s;  /* [fog] fog_fade_time: seconds for most of the change */
static DWORD g_t0, g_last[2];
static batch_t g_b[2][RT_NBATCH];
static struct {
    int bx, by, used;
    float zmin;
} g_cells[HASH];
static int g_used;

void tree_sway_load_config(const char *ini_path)
{
    g_amount = ini_float(ini_path, "graphics", "tree_sway", 1.15f);
    if (g_amount < 0.0f)
        g_amount = 0.0f;
    if (g_amount > 4.0f)
        g_amount = 4.0f;
    g_fade = GetPrivateProfileIntA("fog", "fog_fade", 1, ini_path);
    g_fade_s = ini_float(ini_path, "fog", "fog_fade_time", 1.5f);
    if (g_fade_s < 0.1f)
        g_fade_s = 0.1f;
    if (g_fade_s > 10.0f)
        g_fade_s = 10.0f;
    g_tree_sway_enabled = g_amount > 0.0f || g_fade;
}

static unsigned bits_of(float f)
{
    unsigned u;
    memcpy(&u, &f, 4);
    return u;
}

static float float_of(unsigned u)
{
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static void colour_to(float *shown, unsigned c)
{
    shown[0] = (float)(c >> 24), shown[1] = (float)((c >> 16) & 255), shown[2] = (float)((c >> 8) & 255),
    shown[3] = (float)(c & 255);
}

static unsigned colour_of(const float *shown)
{
    unsigned c = 0;
    for (int k = 0; k < 4; k++) {
        float f = shown[k] + 0.5f;
        unsigned b = f <= 0.0f ? 0u : f >= 255.0f ? 255u : (unsigned)f;
        c = (c << 8) | b;
    }
    return c;
}

static unsigned cell_slot(int bx, int by)
{
    unsigned h = ((unsigned)bx * 73856093u) ^ ((unsigned)by * 19349663u);
    h &= HASH - 1;
    while (g_cells[h].used && (g_cells[h].bx != bx || g_cells[h].by != by))
        h = (h + 1) & (HASH - 1);
    return h;
}

static int batch_ok(char *va, float **data, int *count, int *stride)
{
    *data = *(float **)(va + VA_DATA);
    *count = *(int *)(va + VA_COUNT);
    *stride = *(int *)(va + VA_STRIDE);
    return *data && *count > 0 && *stride >= 3 && *stride <= 32;
}

/* Has the game changed this batch since we last wrote it? Every vertex is checked: the game also
   refills only parts of a batch, or grows it into a new buffer with our moved vertices copied
   along. */
static int refilled(batch_t *b, char *va)
{
    float *data;
    int count, stride;
    if (!batch_ok(va, &data, &count, &stride))
        return b->count != 0;
    if (data != b->data || count != b->count || stride != b->stride)
        return 1;
    for (int i = 0; i < count; i++) {
        const float *v = data + (size_t)i * stride, *r = b->rec + (size_t)i * REC;
        if (v[0] != r[3] || v[1] != r[4])
            return 1;
    }
    return 0;
}

static unsigned key_of(float x, float y)
{
    unsigned a, c;
    memcpy(&a, &x, 4);
    memcpy(&c, &y, 4);
    return (a * 73856093u) ^ (c * 19349663u);
}

static void table_put(int *table, unsigned size, float x, float y, int value)
{
    unsigned h = key_of(x, y) & (size - 1);
    while (table[h] >= 0)
        h = (h + 1) & (size - 1);
    table[h] = value;
}

/* After a change: where the game put each vertex. One standing exactly where we last wrote one of
   ours (at any index: the game also moves runs of vertices when it drops a cell) is ours, so it
   keeps the place we had for it, else its lean would become its rest. One standing where one of
   ours rests is that vertex put back by the game (a refill, e.g. after the fog changed). Both keep
   the colour shown, so a refill does not cut its fade short. Any other is new. z goes in the
   height's slot until the ground is known. */
static int recover(batch_t *b, const float *data, int count, int stride)
{
    float *rec = malloc((size_t)count * REC * sizeof *rec);
    if (!rec)
        return 0;
    int old = b->rec ? b->count : 0;
    unsigned size = 1;
    while (size < 4u * (unsigned)old)
        size *= 2;
    /* entries: 2 i = an old vertex by where we wrote it, 2 i + 1 = by where it rests */
    int *table = old ? malloc((size_t)size * sizeof *table) : NULL;
    if (table) {
        memset(table, 0xFF, (size_t)size * sizeof *table);
        for (int i = 0; i < old; i++) {
            const float *o = b->rec + (size_t)i * REC;
            table_put(table, size, o[R_WX], o[R_WY], 2 * i);
            table_put(table, size, o[R_OX], o[R_OY], 2 * i + 1);
        }
    }
    for (int i = 0; i < count; i++) {
        const float *v = data + (size_t)i * stride;
        float *r = rec + (size_t)i * REC;
        unsigned c = bits_of(v[3]);
        const float *o = NULL;
        if (table)
            for (unsigned h = key_of(v[0], v[1]) & (size - 1); table[h] >= 0; h = (h + 1) & (size - 1)) {
                const float *q = b->rec + (size_t)(table[h] >> 1) * REC;
                const float *at = q + ((table[h] & 1) ? R_OX : R_WX);
                if (at[0] == v[0] && at[1] == v[1]) {
                    o = q;
                    break;
                }
            }
        r[R_OX] = o ? o[R_OX] : v[0], r[R_OY] = o ? o[R_OY] : v[1];
        r[R_H] = v[2], r[R_WX] = v[0], r[R_WY] = v[1];
        r[R_GAME] = float_of(c), r[R_WROTE] = float_of(c);
        if (o)
            memcpy(r + R_SHOWN, o + R_SHOWN, 4 * sizeof *r);
        else
            colour_to(r + R_SHOWN, c);
    }
    free(table);
    free(b->rec);
    b->rec = rec, b->cap = count;
    b->data = (float *)data, b->count = count, b->stride = stride;
    return 1;
}

/* The lowest vertex of a batch per grid cell; 0 when the grid is full. */
static int add_ground(const batch_t *b)
{
    for (int i = 0; i < b->count; i++) {
        const float *r = b->rec + (size_t)i * REC;
        int bx = (int)floorf(r[0] / CELL), by = (int)floorf(r[1] / CELL);
        unsigned s = cell_slot(bx, by);
        if (!g_cells[s].used) {
            if (++g_used > HASH * 3 / 4)
                return 0;
            g_cells[s].used = 1, g_cells[s].bx = bx, g_cells[s].by = by, g_cells[s].zmin = r[2];
        } else if (r[2] < g_cells[s].zmin)
            g_cells[s].zmin = r[2];
    }
    return 1;
}

/* Every vertex's height above the lowest vertex near it (the trunk's base). */
static void set_heights(batch_t *b)
{
    for (int i = 0; i < b->count; i++) {
        float *r = b->rec + (size_t)i * REC;
        int bx = (int)floorf(r[0] / CELL), by = (int)floorf(r[1] / CELL);
        float ground = r[2];
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                unsigned s = cell_slot(bx + dx, by + dy);
                if (g_cells[s].used && g_cells[s].zmin < ground)
                    ground = g_cells[s].zmin;
            }
        r[2] -= ground;
    }
}

static void sway_batch(batch_t *b, char *va, float t, float ease)
{
    float *data;
    int count, stride;
    if (!batch_ok(va, &data, &count, &stride) || data != b->data || count != b->count)
        return;
    float amp = 0.012f * g_amount; /* 0: no sway, only the colour fade */
    for (int i = 0; i < count; i++) {
        float *r = b->rec + (size_t)i * REC;
        float h = r[2];
        float *v = data + (size_t)i * stride;
        if (h <= 1.0f) {
            v[0] = r[0], v[1] = r[1];
        } else {
            /* a wave that moves across the map, plus a slower swell of gusts */
            float ph = 0.0045f * r[0] + 0.0031f * r[1];
            float gust = 0.6f + 0.4f * sinf(0.35f * t - 0.0004f * (r[0] - r[1]));
            float lean = amp * gust * (0.7f * sinf(1.9f * t + ph) + 0.3f * sinf(3.3f * t + 1.7f * ph));
            float off = lean * h * (0.5f + 0.5f * fminf(h / 250.0f, 2.0f));
            v[0] = r[0] + off * 0.70710678f;
            v[1] = r[1] - off * 0.70710678f;
        }
        r[R_WX] = v[0], r[R_WY] = v[1];
        /* the colour: the game darkens trees out of sight (and brightens them back) at once; this
           eases the shown colour towards the game's */
        unsigned c = bits_of(v[3]);
        if (c != bits_of(r[R_WROTE]))
            r[R_GAME] = v[3]; /* the game set a new one */
        if (g_fade) {
            unsigned want = bits_of(r[R_GAME]);
            float *sh = r + R_SHOWN, tgt[4];
            colour_to(tgt, want);
            for (int k = 0; k < 4; k++)
                sh[k] += (tgt[k] - sh[k]) * ease;
            c = colour_of(sh);
            if (c != want && fabsf(sh[0] - tgt[0]) + fabsf(sh[1] - tgt[1]) + fabsf(sh[2] - tgt[2]) +
                                     fabsf(sh[3] - tgt[3]) < 1.0f)
                c = want, colour_to(sh, want);
        } else
            c = bits_of(r[R_GAME]);
        v[3] = float_of(c);
        r[R_WROTE] = v[3];
    }
}

static void sway_trees(char *rt, batch_t *set, float t, float ease)
{
    int fresh = 0;
    for (int k = 0; k < RT_NBATCH && !fresh; k++)
        fresh = refilled(&set[k], rt + RT_BATCH + 0x20 * k);
    if (fresh) {
        /* where the game put every vertex, the ground under all of them, then the heights */
        int ok = 1;
        for (int k = 0; k < RT_NBATCH; k++) {
            float *data;
            int count, stride;
            if (batch_ok(rt + RT_BATCH + 0x20 * k, &data, &count, &stride)) {
                if (!recover(&set[k], data, count, stride))
                    ok = 0;
            } else
                set[k].data = NULL, set[k].count = 0;
        }
        memset(g_cells, 0, sizeof g_cells);
        g_used = 0;
        for (int k = 0; k < RT_NBATCH && ok; k++)
            ok = add_ground(&set[k]);
        for (int k = 0; k < RT_NBATCH; k++) {
            if (ok)
                set_heights(&set[k]);
            else
                set[k].data = NULL, set[k].count = 0; /* too much to hold: the trees stand still */
        }
    }
    int any = 0;
    for (int k = 0; k < RT_NBATCH; k++) {
        if (!set[k].count)
            continue;
        sway_batch(&set[k], rt + RT_BATCH + 0x20 * k, t, ease);
        any = 1;
    }
    if (any)
        *(int *)(rt + RT_UPLOAD) = 1; /* the game uploads the moved vertices */
}

/* TreeFlushMethod::execute(RData *): uploads (when +0x128 is set) and draws an RTreeData's
   batches. Ours runs first, after any refill of this frame, so no frame shows the trees upright. */
static flush_t g_real_flush;

static int __attribute__((thiscall)) ts_flush(void *self, void *rd)
{
    char *gp = *(char **)VA(VA_GRAPHIC_PIECES);
    if (g_tree_sway_enabled && gp && rd) {
        for (int k = 0; k < 2; k++)
            if (*(void **)(gp + 0x60 + 4 * k) == rd) {
                /* how far the colours go towards the game's since the last time: about 95 % of a
                   change takes fog_fade_time */
                DWORD now = GetTickCount();
                float dt = g_last[k] ? (now - g_last[k]) / 1000.0f : 0.0f;
                g_last[k] = now;
                if (dt > 0.25f)
                    dt = 0.25f;
                float ease = 1.0f - expf(-3.0f * dt / g_fade_s);
                sway_trees(rd, g_b[k], (now - g_t0) / 1000.0f, ease);
                break;
            }
    }
    return g_real_flush(self, rd);
}

static int hook_slot(void **slot, void *to)
{
    DWORD old;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old))
        return 0;
    g_real_flush = (flush_t)*slot;
    *slot = to;
    VirtualProtect(slot, sizeof *slot, old, &old);
    return 1;
}

void tree_sway_install(void)
{
    if (!g_tree_sway_enabled)
        return;
    g_t0 = GetTickCount();
    void **slot = (void **)VA(VA_FLUSH_VTBL) + 5;
    if ((UINT_PTR)*slot != VA(VA_FLUSH_EXECUTE) || !hook_slot(slot, (void *)ts_flush)) {
        logf_("tree sway: signature mismatch, turned off");
        g_tree_sway_enabled = 0;
        return;
    }
    logf_("tree sway: on, strength %.2f", g_amount);
}
