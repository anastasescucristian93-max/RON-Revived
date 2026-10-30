/*
 * Smooth unit facing (render only). Two causes of units turning in steps, see ENGINE_NOTES.md,
 * "Unit facing":
 *
 *  - GuyOut::draw turns the interpolated angle into the model's rotation through
 *    fast_angle_to_degrees (a 256-entry table of whole degrees) and the cosine / sine tables of
 *    whole half-degrees, so a model has only 256 facings. The block from the conversion to the
 *    sine (0x005DC880..0x005DC8E3) is replaced by the exact cosine and sine of the half angle
 *    (x87 fsincos), and the shadow's facing (draw_shadows, call at 0x005DC422) gets the exact
 *    angle in degrees.
 *
 *  - Aircraft: Unit::do_air_physics sets the angle with Guy::set_angle(a, snap 1), which writes
 *    the previous tick's angle too, so GuyData::get_render_angle has nothing to interpolate and
 *    the plane snaps once per simulation tick. get_render_angle is replaced: when the previous
 *    and current angles are equal, the angle it had at the previous tick comes from a small
 *    table kept here (per Guy, by game time), and the drawn angle moves from it to the current
 *    one over the tick, like the position does. Units that turn the normal way keep the game's
 *    own interpolation.
 *
 * get_render_angle is called only by GuyOut::draw and GuyOut::draw_shadows, and nothing here
 * writes game state, so it is multiplayer safe.
 */
#include "ron.h"
#include <string.h>

#define VA_GAME          0x00C061ECu /* Game *: +0x550 game time (one per Game::do_frame), +0x854 frame fraction */
#define VA_RENDER_ANGLE  0x005DE570u /* GuyData::get_render_angle() const, leaf, ecx = Guy, returns eax */
#define VA_DRAW_FACING   0x005DC880u /* GuyOut::draw: mov ecx,eax / call fast_angle_to_degrees ... */
#define VA_DRAW_BACK     0x005DC8E3u /* ... the sine is in xmm0, the cosine at [ebp-0x50] */
#define VA_SHADOW_CALL   0x005DC422u /* GuyOut::draw_shadows: call fast_angle_to_degrees */
#define VA_FAST_DEGREES  0x00A28F70u /* fast_angle_to_degrees(int in ecx): whole degrees, float in xmm0 */
#define GUY_ANGLE        0x18        /* binary angle, 2^32 = full circle */
#define GUY_PREV_ANGLE   0x1C        /* at the previous simulation tick */

int g_facing_enabled;
UINT_PTR g_facing_back; /* VA_DRAW_BACK, relocated, for the asm stub */

/* ---- aircraft: the angle at the previous tick ----------------------------------------- */

#define CACHE 4096
static struct {
    char *guy;
    int tick;
    unsigned at, before; /* the angle at `tick` and at the tick before it */
} g_c[CACHE];

__attribute__((used)) unsigned c_render_angle(char *guy)
{
    unsigned cur = *(unsigned *)(guy + GUY_ANGLE), prev = *(unsigned *)(guy + GUY_PREV_ANGLE);
    char *game = *(char **)VA(VA_GAME);
    if (cur == prev) {
        int t = *(int *)(game + 0x550);
        unsigned h = (unsigned)((UINT_PTR)guy >> 3) * 2654435761u >> 20;
        typeof(&g_c[0]) e = &g_c[h & (CACHE - 1)];
        if (e->guy != guy || e->tick < t - 1 || e->tick > t) { /* new, not drawn last tick, or a loaded game */
            e->guy = guy;
            e->tick = t;
            e->at = e->before = cur;
            return cur;
        }
        if (e->tick == t - 1) {
            e->before = e->at;
            e->at = cur;
            e->tick = t;
        } else if (e->at != cur) /* changed without a tick: take it as it is */
            e->at = e->before = cur;
        prev = e->before;
        if (prev == cur)
            return cur;
    }
    /* the game's own: from the previous angle the shortest way round, by the frame fraction */
    float f = *(float *)(game + 0x854);
    return prev + (unsigned)(int)((float)(int)(cur - prev) * f);
}

/* ---- stubs -------------------------------------------------------------------------------- */

void hk_render_angle(void);
void hk_draw_facing(void);
void hk_shadow_degrees(void);

__asm__(".intel_syntax noprefix\n"
        /* replaces get_render_angle: every register kept but eax (the result) */
        ".globl _hk_render_angle\n"
        "_hk_render_angle:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_render_angle\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret\n"

        /* GuyOut::draw, eax = the render angle. The model's facing is d = degrees + 180 (mod 360)
           and the game takes cos(d / 2), sin(d / 2): here exactly, from the angle's top 24 bits.
           d / 2 = angle / 2 + 90 degrees, or - 90 from 180 degrees on (the mod). */
        ".globl _hk_draw_facing\n"
        "_hk_draw_facing:\n"
        "    sub esp, 8\n"
        "    mov [esp], eax\n"
        "    shr dword ptr [esp], 8\n"
        "    fild dword ptr [esp]\n"
        "    fmul dword ptr [_k_half_rad]\n"
        "    test eax, eax\n"
        "    js Lfacing_neg\n"
        "    fadd dword ptr [_k_quarter]\n"
        "    jmp Lfacing_go\n"
        "Lfacing_neg:\n"
        "    fsub dword ptr [_k_quarter]\n"
        "Lfacing_go:\n"
        "    fsincos\n" /* st0 = cos, st1 = sin */
        "    fstp dword ptr [ebp-0x50]\n"
        "    fstp dword ptr [esp]\n"
        "    movss xmm0, dword ptr [esp]\n"
        "    add esp, 8\n"
        "    jmp dword ptr [_g_facing_back]\n"

        /* replaces fast_angle_to_degrees in draw_shadows: ecx = angle, xmm0 = degrees */
        ".globl _hk_shadow_degrees\n"
        "_hk_shadow_degrees:\n"
        "    push ecx\n"
        "    shr ecx, 8\n"
        "    cvtsi2ss xmm0, ecx\n"
        "    mulss xmm0, dword ptr [_k_degrees]\n"
        "    pop ecx\n"
        "    ret\n"
        ".att_syntax prefix\n");

__attribute__((used)) float k_half_rad = 1.8725351e-7f; /* pi / 2^24: half the angle, in radians */
__attribute__((used)) float k_quarter = 1.5707963f;    /* 90 degrees */
__attribute__((used)) float k_degrees = 2.1457672e-5f; /* 360 / 2^24 */

/* ---- install ------------------------------------------------------------------------------ */

void facing_load_config(const char *ini_path)
{
    g_facing_enabled = GetPrivateProfileIntA("units", "smooth_facing", 1, ini_path);
}

static void patch(unsigned char *at, const unsigned char *bytes, int n)
{
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
}

void facing_install(void)
{
    if (!g_facing_enabled)
        return;
    unsigned char *ra = (unsigned char *)VA(VA_RENDER_ANGLE);
    unsigned char *df = (unsigned char *)VA(VA_DRAW_FACING);
    unsigned char *sc = (unsigned char *)VA(VA_SHADOW_CALL);
    /* mov edx,[ecx+0x18] / mov ecx,[ecx+0x1c] / cmp edx,ecx */
    static const unsigned char ra_sig[8] = { 0x8B, 0x51, 0x18, 0x8B, 0x49, 0x1C, 0x3B, 0xD1 };
    /* mov ecx,eax / call fast_angle_to_degrees / cvttss2si ecx,xmm0 */
    static const unsigned char df_sig[3] = { 0x8B, 0xC8, 0xE8 };
    int ok = memcmp(ra, ra_sig, sizeof ra_sig) == 0 && memcmp(df, df_sig, sizeof df_sig) == 0 &&
             df + 7 + *(INT32 *)(df + 3) == (unsigned char *)VA(VA_FAST_DEGREES) &&
             memcmp(df + 7, "\xF3\x0F\x2C\xC8", 4) == 0 && sc[0] == 0xE8 &&
             sc + 5 + *(INT32 *)(sc + 1) == (unsigned char *)VA(VA_FAST_DEGREES);
    if (!ok) {
        logf_("facing: signature mismatch, units turn as in the game");
        g_facing_enabled = 0;
        return;
    }
    g_facing_back = VA(VA_DRAW_BACK);
    hook_jmp5(ra, hk_render_angle);
    hook_jmp5(df, hk_draw_facing);
    patch(df + 5, (const unsigned char *)"\x90\x90", 2);
    unsigned char call[5] = { 0xE8 };
    *(INT32 *)(call + 1) = (INT32)((UINT_PTR)hk_shadow_degrees - ((UINT_PTR)sc + 5));
    patch(sc, call, 5);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("facing: exact model facing and interpolated aircraft turns on");
}
