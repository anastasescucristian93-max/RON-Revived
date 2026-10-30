/*
 * Rally line: a selected building of yours shows a dotted line on the ground from the
 * building to each of its rally flags, in your colour.
 *
 * BuildOut::draw_underlays already draws the rally flags (a GraphicPieces sprite at each
 * gather point) for a building of the local player that is in the local selection. Its first
 * call, WallOut::draw_underlays, is pointed at c_rally_underlays, which calls it and then
 * draws the dots with the game's own ground circle (draw_circle, the one of the range
 * circles): small rings every RALLY_STEP world units, at the terrain's height, so they follow
 * hills and are hidden by buildings and units like the range circles.
 *
 * Gather points: BuildData +0xC8 count, +0xCC first node; node +0 next, +8 GatherPoint;
 * GatherPoint +4 x, +8 y (world units), +0xC kind (3 = inside, x or y < 0 = inside too).
 * World units: 192 per tile; objects keep x, y, z at +0x10, +0x14, +0xC xor 0x63637.
 *
 * Only the drawing is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_CALL_SITE    0x0062BE16u /* BuildOut::draw_underlays: call WallOut::draw_underlays */
#define VA_WALL_UNDER   0x006410A0u /* WallOut::draw_underlays() const */
#define VA_MEMBER       0x0070F8F0u /* GroupData::member(id, player, alive) const, ret 0xC */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each, indexed by Console +0x2A0 */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player, +0x2A0 selection index */
#define VA_TERRAIN      0x00C06218u /* Terrain * */
#define VA_FIND_Z       0x00866560u /* TerrainOut::find_data_z(Coord *out, x, y, 1), ret 0x10 */
#define VA_DRAW_CIRCLE  0x008C6040u /* draw_circle(x, y, z, radius, Color, CircleType, ...) */
#define VA_GET_UNIT     0x008C2DB0u /* TeamColor::get_unit(Color *out) const, ret 4 */
#define VA_LEADERS_PTR  0x00C061E0u /* LeaderData[8], 0x6EEC each; +0x6928 colour index */
#define VA_TEAM_COLORS  0x00E39CE0u /* TeamColor[], 0x64 each */

#define KEY        0x63637
#define TILE       192
#define RALLY_STEP 80 /* world units between dots */
#define RALLY_DOT  14 /* dot radius, world units */
#define MAX_DOTS   400

typedef int(__attribute__((thiscall)) *under_t)(void *self);
typedef int(__attribute__((thiscall)) *member_t)(void *group, int id, int player, int alive);
typedef int *(__attribute__((thiscall)) *find_z_t)(void *terrain, int *out, int x, int y, int flag);
typedef void *(__attribute__((thiscall)) *color_t)(void *team, void *out);

int g_rally_enabled;

void rally_load_config(const char *ini_path)
{
    g_rally_enabled = GetPrivateProfileIntA("qol", "rally_line", 1, ini_path);
}

/* draw_circle takes the radius in ecx and the circle type in edx; the rest on the stack
   (x, y, z, Color by value, -1, 1, 0, 0.0f), cleaned by the caller, as BuildOut does for
   the attack range. */
void rally_circle(int x, int y, int z, int radius, const void *color);
__asm__(".intel_syntax noprefix\n"
        ".globl _rally_circle\n" /* airstrike.c draws its circle with it too */
        "_rally_circle:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    push esi\n"
        "    push edi\n"
        "    sub esp, 8\n"
        "    mov dword ptr [esp+4], 0\n" /* float 0.0 */
        "    mov dword ptr [esp], 0\n"
        "    push 1\n"
        "    push -1\n"
        "    sub esp, 12\n" /* Color, 12 bytes by value */
        "    mov esi, [ebp+24]\n"
        "    mov edi, esp\n"
        "    mov eax, [esi]\n"
        "    mov [edi], eax\n"
        "    mov eax, [esi+4]\n"
        "    mov [edi+4], eax\n"
        "    mov eax, [esi+8]\n"
        "    mov [edi+8], eax\n"
        "    push dword ptr [ebp+16]\n" /* z */
        "    push dword ptr [ebp+12]\n" /* y */
        "    push dword ptr [ebp+8]\n"  /* x */
        "    mov ecx, [ebp+20]\n"       /* radius */
        "    xor edx, edx\n"            /* CircleType 0 */
        "    call [_g_draw_circle]\n"
        "    add esp, 0x28\n"
        "    pop edi\n"
        "    pop esi\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

UINT_PTR g_draw_circle;

static int ground_z(int x, int y)
{
    void *terrain = *(void **)VA(VA_TERRAIN);
    int z = 0;
    if (terrain)
        ((find_z_t)VA(VA_FIND_Z))(terrain, &z, x, y, 1);
    return z;
}

static void draw_line(int x0, int y0, int x1, int y1, int bx0, int by0, int bx1, int by1, const void *color)
{
    float dx = (float)(x1 - x0), dy = (float)(y1 - y0);
    float len = sqrtf(dx * dx + dy * dy);
    int n = (int)(len / RALLY_STEP);
    if (n > MAX_DOTS)
        n = MAX_DOTS;
    for (int i = 1; i <= n; i++) {
        int x = x0 + (int)(dx * i * RALLY_STEP / len), y = y0 + (int)(dy * i * RALLY_STEP / len);
        if (x >= bx0 && x <= bx1 && y >= by0 && y <= by1)
            continue; /* under the building itself */
        rally_circle(x, y, ground_z(x, y), RALLY_DOT, color);
    }
}

__attribute__((used, thiscall)) int c_rally_underlays(char *b)
{
    int r = ((under_t)VA(VA_WALL_UNDER))(b);
    char *console = *(char **)VA(VA_CONSOLE);
    char *leaders = *(char **)VA(VA_LEADERS_PTR);
    if (!console || !leaders)
        return r;
    int player = (unsigned char)b[9], id = *(short *)(b + 0xA);
    if (player != *(int *)(console + 0x298) || player >= 8)
        return r;
    char *sel = *(char **)VA(VA_SELECTIONS);
    if (!sel || !((member_t)VA(VA_MEMBER))(sel + *(int *)(console + 0x2A0) * 0x9F0, id, player, 1))
        return r;
    /* a selected Missile Silo: the ring its Interceptors defend, on the ground of the map (nukefx.c;
       the user, 2026-10-29: the interception range on the map, the minimap keeps the attack range) */
    int ring = nukefx_reach_ring(*(int *)(*(char **)(b + 0x18) + 4));
    if (ring) {
        char *t = *(char **)(b + 0x18);
        int sx = (*(int *)(b + 0x10) ^ KEY) + *(int *)(t + 0x234) * TILE / 2;
        int sy = (*(int *)(b + 0x14) ^ KEY) + *(int *)(t + 0x238) * TILE / 2;
        unsigned char rc[12];
        int ci = (unsigned char)leaders[player * 0x6EEC + 0x6928];
        ((color_t)VA(VA_GET_UNIT))((char *)VA(VA_TEAM_COLORS) + ci * 0x64, rc);
        *(unsigned *)rc = (*(unsigned *)rc & 0xFFFFFF) | 0xC0000000u;
        rally_circle(sx, sy, ground_z(sx, sy), ring, rc);
    }
    if (!g_rally_enabled)
        return r;
    int count = *(int *)(b + 0xC8);
    char *node = *(char **)(b + 0xCC);
    if (!node || count <= 0)
        return r;

    char *type = *(char **)(b + 0x18);
    int bx = *(int *)(b + 0x10) ^ KEY, by = *(int *)(b + 0x14) ^ KEY;
    int w = *(int *)(type + 0x234) * TILE, h = *(int *)(type + 0x238) * TILE;
    int cx = bx + w / 2, cy = by + h / 2;

    unsigned char color[12];
    int ci = (unsigned char)leaders[player * 0x6EEC + 0x6928];
    ((color_t)VA(VA_GET_UNIT))((char *)VA(VA_TEAM_COLORS) + ci * 0x64, color);
    *(unsigned *)color = (*(unsigned *)color & 0xFFFFFF) | 0xC0000000u;

    for (int i = 0; i < count && node; i++, node = *(char **)node) {
        char *gp = *(char **)(node + 8);
        if (!gp)
            continue;
        int gx = *(int *)(gp + 4), gy = *(int *)(gp + 8);
        if (gp[0xC] == 3 || gx < 0 || gy < 0)
            continue; /* units stay inside */
        draw_line(cx, cy, gx, gy, bx, by, bx + w, by + h, color);
    }
    return r;
}

/* Called from install_hooks once the exe is known to be the supported version. */
void rally_install(void)
{
    if (!g_rally_enabled && !nukefx_reach_ring(0x208)) /* the hook also draws the silos' interception ring */
        return;
    unsigned char *at = (unsigned char *)VA(VA_CALL_SITE);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(VA_WALL_UNDER)) {
        logf_("rally: call site mismatch, rally line turned off");
        g_rally_enabled = 0;
        return;
    }
    g_draw_circle = VA(VA_DRAW_CIRCLE);
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)c_rally_underlays - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    logf_("rally line installed");
}
