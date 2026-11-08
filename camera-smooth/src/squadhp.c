/*
 * Squad health bars: a squad (several soldiers, UBER_SIZE > 1) shows one small health bar over
 * every soldier, each with that soldier's own hit points, instead of the game's single bar over
 * the captain. They show when the squad is in the local selection or any of its soldiers is
 * under the cursor.
 *
 * The game's UnitOut::draw_health_bar() (virtual +0x6C, run for every unit) draws only for a
 * captain, at the captain, with UnitData::total_damage, the whole squad's loss against one
 * soldier's maximum. Its entry is hooked: a squad soldier that is not a decoy is handled here and
 * the game's function is skipped; anything else goes on to the game's code.
 *
 * A soldier is its own object (camera-smooth/ENGINE_NOTES.md "Squads"): max hits +0x20 (the
 * virtual hits(0), +0x11C), hits lost +0x24 plus sixteenths in the byte +0x3B, and it dies when
 * the loss reaches the maximum. Position, height above the ground, the visibility test, the bar's
 * size per zoom level and the colours (ObjectData::health_level: 90 / 75 / 50 / 25 %) are the
 * game's own. The captain's bar keeps the rank insignia and the craft bar (ranks.c).
 *
 * squad_hovered() is also used by hover.c, so that every soldier of the hovered squad gets the
 * hover circle.
 *
 * Larger health bars (the user, 2026-11-08: "sunt cam minuscule", +75 %): every health bar takes
 * its size from two small tables per zoom level, width 30 / 20 / 10 and height 3 / 2 / 1 (zoom 6,
 * 5, the rest), in UnitOut::draw_health_bar(int, int) (units, also their mana, decoy and craft
 * bars, stacked from the same size) and WallOut::draw_health_bar(int, int) (buildings, walls).
 * Their immediates are scaled by [qol] health_bar_percent; the squad soldiers' bars here use the
 * same. The rank insignia (ranks.c) and the production bar (prodbar.c) are placed from the bar's
 * rect, so they follow.
 *
 * Only the drawing is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <string.h>

#define VA_HB           0x00613810u /* UnitOut::draw_health_bar() const, virtual +0x6C */
#define VA_GP_BAR       0x00908650u /* GraphicPieces::draw_health_bar(rect *, float (xmm2), Color &, Color &), ret 0xC */
#define VA_W2C          0x00583840u /* MiscAccess::world_to_console: ecx, edx = &out x, &out y; x, y, z, -1, -1 */
#define VA_MEMBER       0x0070F8F0u /* GroupData::member(id, player, alive) const, ret 0xC */
#define VA_IS_OVER      0x0043E220u /* Player::is_over() const */
#define VA_TOURNAMENT   0x0043EC90u /* Game::is_tournament() const */
#define VA_OBJECTS      0x00C0AB84u /* Object ** per player, stride 28 */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player, +0x2A0 selection index, +0x12A y offset */
#define VA_MOUSEOVER    0x00C06230u /* MouseOver *: +4 bit 4 object, +0x4C player, +0x58 id */
#define VA_SELECTIONS   0x00E8D444u
#define VA_GAMEC        0x00C061E8u /* +0x820 view flags, +0x854 tick fraction (float) */
#define VA_CAMERA       0x00C06200u /* +0x294 zoom level */
#define VA_GRAPHIC_PIECES 0x00C06214u
#define VA_GREEN        0x00C8D1D0u
#define VA_YELLOW       0x00C8D278u
#define VA_RED          0x00C8D248u
#define VA_BLACK        0x00C8D260u

#define U_TYPE    0x18
#define U_MAXHITS 0x20
#define U_LOST    0x24
#define U_FRAC    0x3B
#define U_FLAGS   0x68  /* bit 0: a decoy */
#define U_CAPTAIN 0x8E  /* bit 15: the captain; otherwise the captain's id */
#define U_NEXT    0x90  /* the next soldier, -1 at the end */
#define U_GUYS    0xF4
#define T_UBER    0x308 /* UBER_SIZE */

typedef int(__attribute__((thiscall)) *member_t)(void *group, int id, int player, int alive);
typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);

int g_squad_hp_enabled;
static int g_width_pct = 70;
static int g_bar_pct = 175; /* [qol] health_bar_percent: every health bar */
UINT_PTR g_sq_gp_bar, g_sq_w2c;
void *g_sq_orig;

void squad_hp_load_config(const char *ini_path)
{
    g_squad_hp_enabled = GetPrivateProfileIntA("qol", "squad_health_bars", 1, ini_path);
    g_width_pct = GetPrivateProfileIntA("qol", "squad_bar_width_percent", 70, ini_path);
    if (g_width_pct < 20 || g_width_pct > 100)
        g_width_pct = 70;
    g_bar_pct = GetPrivateProfileIntA("qol", "health_bar_percent", 175, ini_path);
    if (g_bar_pct < 50 || g_bar_pct > 400)
        g_bar_pct = 175;
}

static int bar_w(int w) { return (w * g_bar_pct + 50) / 100; }
static int bar_h(int h)
{
    int v = (h * g_bar_pct + 50) / 100;
    return v < 1 ? 1 : v;
}

/* GraphicPieces::draw_health_bar(gp, rect, frac, fill, back) from C */
void sq_draw(void *gp, short *rect, float frac, const void *fill, const void *back);
/* MiscAccess::world_to_console(x, y, z) from C: 0 when on screen */
int sq_w2c(int x, int y, int z, int *ox, int *oy);
__asm__(".intel_syntax noprefix\n"
        "_sq_draw:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    movss xmm2, dword ptr [ebp+16]\n"
        "    push dword ptr [ebp+24]\n"
        "    push dword ptr [ebp+20]\n"
        "    push dword ptr [ebp+12]\n"
        "    mov ecx, [ebp+8]\n"
        "    call [_g_sq_gp_bar]\n"
        "    pop ebp\n"
        "    ret\n"
        "_sq_w2c:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    push -1\n"
        "    push -1\n"
        "    push dword ptr [ebp+16]\n"
        "    push dword ptr [ebp+12]\n"
        "    push dword ptr [ebp+8]\n"
        "    mov ecx, [ebp+20]\n"
        "    mov edx, [ebp+24]\n"
        "    call [_g_sq_w2c]\n"
        "    add esp, 20\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

static char *object(int player, int id)
{
    if (player < 0 || player >= 8 || id < 0)
        return NULL;
    char **objs = *(char ***)(VA(VA_OBJECTS) + 28 * player);
    return objs ? objs[id] : NULL;
}

static int is_unit(char *o) { return ((fn0_t)(*(void ***)o)[0x18 / 4])(o) != 0; }

static int captain_id(char *u)
{
    unsigned short c = *(unsigned short *)(u + U_CAPTAIN);
    return (c & 0x8000) ? *(short *)(u + 0xA) : (short)c;
}

static int squad_size(char *u)
{
    char *t = *(char **)(u + U_TYPE);
    return t ? *(int *)(t + T_UBER) : 1;
}

/* the object under the cursor is (id, player) or a soldier of the same squad */
int squad_hovered(int id, int player)
{
    char *mo = *(char **)VA(VA_MOUSEOVER);
    if (!mo || !(*(int *)(mo + 4) & 4) || *(int *)(mo + 0x4C) != player)
        return 0;
    int mid = *(int *)(mo + 0x58);
    if (mid == id)
        return 1;
    char *a = object(player, id), *b = object(player, mid);
    if (!a || !b || !is_unit(a) || !is_unit(b) || squad_size(a) <= 1)
        return 0;
    return captain_id(a) == captain_id(b);
}

static int selected(int id, int player)
{
    char *console = *(char **)VA(VA_CONSOLE);
    char *sel = *(char **)VA(VA_SELECTIONS);
    return console && sel && ((member_t)VA(VA_MEMBER))(sel + *(int *)(console + 0x2A0) * 0x9F0, id, player, 1);
}

/* UnitOut::draw_health_bar(): own units, everything when revealed, a finished player's view
   outside tournaments, what the local player sees */
static int seen(char *u)
{
    char *console = *(char **)VA(VA_CONSOLE);
    char *gc = *(char **)VA(VA_GAMEC);
    if (!console || !gc)
        return 0;
    int me = *(int *)(console + 0x298);
    if ((unsigned char)u[9] == me)
        return 1;
    int flags = *(int *)(gc + 0x820);
    if (flags & 0x42)
        return 1;
    char *pl = gc + 0x44 + *(int *)(console + 0x2A0) * 0x8C;
    if (((fn0_t)VA(VA_IS_OVER))(pl) && !((fn0_t)VA(VA_TOURNAMENT))(gc))
        return 1;
    if (((fn2_t)(*(void ***)u)[0x48 / 4])(u, me, 0))
        return 1;
    return (flags & 0x50) != 0;
}

static float lerp(int prev, int cur, float f) { return prev == cur ? (float)cur : (1.0f - f) * prev + cur * f; }

/* the height of the bar above the unit's feet, as the game picks it */
static int bar_height(char *u)
{
    fn1_t has = (fn1_t)(*(void ***)u)[0x148 / 4];
    if (has(u, 0x200000))
        return 0x180;
    if (!has(u, 0x1000))
        return 0xF0;
    if (has(u, 0x10))
        return 0x1E0;
    return ((fn2_t)(*(void ***)u)[0xB8 / 4])(u, 0x3B, 0) ? 0x180 : 0x150;
}

void c_rk_unit_bar(char *u, short *rect); /* ranks.c: the insignia and the craft bar */

__attribute__((used)) int c_sq_hb(char *u)
{
    if (squad_size(u) <= 1 || (*(unsigned *)(u + U_FLAGS) & 1))
        return 0;
    int player = (unsigned char)u[9], id = *(short *)(u + 0xA), cap = captain_id(u);
    if (!(u[8] & 1))
        return 1;
    if (!selected(id, player) && !selected(cap, player) && !squad_hovered(cap, player))
        return 1;
    if (!seen(u))
        return 1;

    char **guys = *(char ***)(u + U_GUYS);
    char *g = guys ? guys[0] : NULL;
    if (!g)
        return 1;
    float f = *(float *)(*(char **)VA(VA_GAMEC) + 0x854);
    int x = (int)lerp(*(int *)(g + 0x68), *(int *)(g + 0xC), f);
    int y = (int)lerp(*(int *)(g + 0x6C), *(int *)(g + 0x10), f);
    int z = (int)lerp(*(int *)(g + 0x70), *(int *)(g + 0x14), f);
    int sx, sy;
    if (sq_w2c(x, y, z + bar_height(u), &sx, &sy))
        return 1;
    sy += *(short *)(*(char **)VA(VA_CONSOLE) + 0x12A);

    int level = *(int *)(*(char **)VA(VA_CAMERA) + 0x294);
    int w = level == 6 ? 30 : level == 5 ? 20 : 10, h = level >= 5 ? level - 3 : 1;
    w = bar_w(w) * g_width_pct / 100, h = bar_h(h);
    if (w < 6)
        w = 6;
    short r[4] = { (short)(sx - w / 2), (short)(sx - w / 2 + w), (short)(sy - h / 2), (short)(sy - h / 2 + h) };

    int max = ((fn1_t)(*(void ***)u)[0x11C / 4])(u, 0);
    float lost = *(int *)(u + U_LOST) + *(signed char *)(u + U_FRAC) / 16.0f;
    float frac = max > 0 ? (max - lost) / max : 0.0f;
    frac = frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
    UINT_PTR fill = frac >= 0.9f ? VA_GREEN : frac >= 0.25f ? VA_YELLOW : VA_RED;
    sq_draw(*(void **)VA(VA_GRAPHIC_PIECES), r, frac, (void *)VA(fill), (void *)VA(VA_BLACK));
    if (cap == id && g_ranks_enabled)
        c_rk_unit_bar(u, r);
    return 1;
}

/* entry of UnitOut::draw_health_bar(): ecx = the unit; non-zero from C = handled, return */
__asm__(".intel_syntax noprefix\n"
        "_hk_sq_hb:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_sq_hb\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp [_g_sq_orig]\n"
        "1:  ret\n"
        ".att_syntax prefix\n");
void hk_sq_hb(void);

/* Called from install_hooks once the exe is known to be the supported version. */
/* ---- every health bar larger ------------------------------------------------------------- */

static int same(UINT32 va, const unsigned char *sig, int n) { return !memcmp((void *)VA(va), sig, n); }

static void put(UINT32 va, const unsigned char *b, int n)
{
    DWORD old;
    unsigned char *at = (unsigned char *)VA(va);
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, b, n);
    VirtualProtect(at, n, old, &old);
}

/* mov dword ptr [ebp+d8], imm32 (C7 45 d8 imm32): the imm32 scaled */
static int mov_site(UINT32 va, unsigned char d8, int was, int now, int dry)
{
    unsigned char sig[7] = { 0xC7, 0x45, d8, (unsigned char)was, 0, 0, 0 };
    if (dry)
        return same(va, sig, 7);
    memcpy(sig + 3, &now, 4);
    put(va, sig, 7);
    return 1;
}

static void health_bars_install(void)
{
    if (g_bar_pct == 100)
        return;
    static const unsigned char lea_m3[3] = { 0x8D, 0x70, 0xFD }; /* lea esi, [eax-3] (eax = the zoom level) */
    static const unsigned char mov1[5] = { 0xBE, 0x01, 0x00, 0x00, 0x00 };
    int ok = 1;
    for (int dry = 1; dry >= 0 && ok; dry--) {
        /* UnitOut::draw_health_bar(int, int): width [ebp-4], height esi */
        ok &= mov_site(0x00613BABu, 0xFC, 30, bar_w(30), dry);
        ok &= mov_site(0x00613BBCu, 0xFC, 20, bar_w(20), dry);
        ok &= mov_site(0x00613BC8u, 0xFC, 10, bar_w(10), dry);
        /* WallOut::draw_health_bar(int, int): width [ebp-8], height [ebp+0xC] */
        ok &= mov_site(0x00640F84u, 0xF8, 30, bar_w(30), dry);
        ok &= mov_site(0x00640F99u, 0xF8, 20, bar_w(20), dry);
        ok &= mov_site(0x00640FA9u, 0xF8, 10, bar_w(10), dry);
        ok &= mov_site(0x00640F8Bu, 0x0C, 3, bar_h(3), dry);
        ok &= mov_site(0x00640FA0u, 0x0C, 2, bar_h(2), dry);
        ok &= mov_site(0x00640FB0u, 0x0C, 1, bar_h(1), dry);
        if (dry) {
            ok &= same(0x00613BB2u, lea_m3, 3) && same(0x00613BC3u, lea_m3, 3) && same(0x00613BCFu, mov1, 5);
        } else { /* lea esi, [eax + (h - zoom)] at zoom 6 and 5, mov esi, h below */
            unsigned char l6[3] = { 0x8D, 0x70, (unsigned char)(signed char)(bar_h(3) - 6) };
            unsigned char l5[3] = { 0x8D, 0x70, (unsigned char)(signed char)(bar_h(2) - 5) };
            unsigned char m[5] = { 0xBE };
            int h1 = bar_h(1);
            memcpy(m + 1, &h1, 4);
            put(0x00613BB2u, l6, 3);
            put(0x00613BC3u, l5, 3);
            put(0x00613BCFu, m, 5);
        }
    }
    if (!ok) {
        logf_("health bars: signature mismatch, they keep the game's size");
        return;
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("health bars: %d%%, %dx%d / %dx%d / %dx%d", g_bar_pct, bar_w(30), bar_h(3), bar_w(20), bar_h(2), bar_w(10),
          bar_h(1));
}

void squad_hp_install(void)
{
    health_bars_install();
    if (!g_squad_hp_enabled)
        return;
    /* push ebp / mov ebp,esp / sub esp,0xC */
    static const unsigned char sig[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C };
    unsigned char *at = (unsigned char *)VA(VA_HB);
    for (int i = 0; i < 6; i++)
        if (at[i] != sig[i]) {
            logf_("squad health bars: signature mismatch, turned off");
            g_squad_hp_enabled = 0;
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        g_squad_hp_enabled = 0;
        return;
    }
    g_sq_gp_bar = VA(VA_GP_BAR);
    g_sq_w2c = VA(VA_W2C);
    g_sq_orig = hook_trampoline(at, 6, mem);
    hook_jmp5(at, hk_sq_hb);
    logf_("squad health bars installed");
}
