/*
 * Survival mode support (the survival\ game script, scenario\Scripts\Survival):
 *  - the clocks at the top right: time left to survive and time to the next wave;
 *  - the no-build zone: a strip of the map (the side away from the defenders' starts) where
 *    no one may build, so the waves always have room to appear.
 *
 * The script talks to the DLL through the names of its timers. ScenarioFuncSet::set_timer
 * stores (name, game second at which it expires) with ScriptTimers::add_timer, which is
 * hooked here:
 *   surv_end              the end of the 30 minutes
 *   surv_wave, surv_boss  the next wave / boss wave
 *   surv_zone x0 y0 x1 y1 the no-build zone, in tiles (x0 <= x < x1, y0 <= y < y1)
 * The script sets them again every minute, so after loading a saved game (which restores
 * the timers without add_timer) the clocks and the zone come back within a minute.
 *
 * The clocks are drawn after IFaceMainBase::draw_timers (the game's own clock / timers at
 * the top right, every frame), with its font ("timer_size") and Buffer::write_right_l.
 * The zone answers BuildTypeData::blocked_site with code 0x18 ("Can't build in enemy
 * territory."): it is the placement check of the cursor, of the build command on every
 * machine and of the computer players, so all of them keep out and the game stays in sync.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define VA_ADD_TIMER    0x00A049E0u /* ScriptTimers::add_timer(String &name, int end_second), ret 8 */
#define VA_DRAW_TIMERS  0x0080B060u /* IFaceMainBase::draw_timers(int), ret 4 */
#define VA_BLOCKED_SITE 0x00636A50u /* BuildTypeData::blocked_site(x, y, player, int, int *), ret 0x14 */
#define VA_GAME_PTR     0x00C061ECu /* Game *: +0x560 game time in seconds (set_timer adds to it) */
#define VA_STR_STR      0x00A1F0A0u /* String::str(): the characters */
#define VA_STRING_CLOSE 0x00A1CF40u
#define VA_INT_STRS     0x00C06378u /* int_str_array: +0x10 the strings, +0x1511C "timer_size" */
#define VA_FONTS        0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET     0x00522050u /* FontFamily::get(String const &, int, int, int), ret 0x10 */
#define VA_SCREEN       0x00ED8010u /* Buffer Window::screen */
#define VA_WRITE_RIGHT  0x0050F9C0u /* Buffer::write_right_l(String const &, x right, y, len), ret 0x10 */
#define VA_GRAPHSYS     0x00E335A0u /* GraphSys *: +0x360 screen width, +0x164 draw layer */
#define VA_LAYER_UI     0x00C8C33Cu /* the layer draw_timers draws in */
#define VA_WHITE        0x00C8D26Cu
#define VA_BLACK        0x00C8D260u
#define VA_YELLOW       0x00C8D278u
#define VA_RED          0x00C8D248u

#define STR_TIMER_FONT  0x1511C
#define BLOCK_ENEMY     0x18 /* "Can't build in enemy territory." */
#define TILE            192  /* world units per tile */

static int g_enabled = 1;
static int g_hud_y;       /* 0 = under the game's own clock lines */

static int g_set_at = -1; /* game second of the last survival timer, to notice a new game */
static int g_end = -1, g_wave = -1, g_boss = -1;
static int g_zone[4], g_zone_on;
static int g_zone_logged;

void *g_orig_add_timer, *g_orig_draw_timers, *g_orig_blocked;

typedef const wchar_t *(__attribute__((thiscall)) *str_t)(void *s);
typedef void(__attribute__((thiscall)) *close_t)(void *s);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *fonts, void *name, int a, int b, int c);
typedef int(__attribute__((thiscall)) *write_right_t)(void *buf, void *s, int x, int y, int len);
typedef int(__attribute__((thiscall)) *font_h_t)(void *font);

void survival_load_config(const char *ini_path)
{
    g_enabled = GetPrivateProfileIntA("gameplay", "survival_support", 1, ini_path);
    g_hud_y = GetPrivateProfileIntA("ui", "survival_clock_y", 0, ini_path);
}

static int game_second(void)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    return game ? *(int *)(game + 0x560) : 0;
}

static void forget(void)
{
    g_end = g_wave = g_boss = -1;
    g_zone_on = 0;
    g_set_at = -1;
}

/* a new game (or a load further back in time): the old timers mean nothing */
static void check_new_game(int now)
{
    if (g_set_at >= 0 && now < g_set_at)
        forget();
}

__attribute__((used)) void c_surv_add_timer(void *name, int end)
{
    if (!name)
        return;
    const wchar_t *s = ((str_t)VA(VA_STR_STR))(name);
    int len = *(unsigned short *)((char *)name + 8);
    if (!s || len < 5 || len > 60 || wcsncmp(s, L"surv_", 5) != 0)
        return;
    char a[64];
    int i;
    for (i = 0; i < len && i < 63; i++)
        a[i] = s[i] < 128 ? (char)s[i] : '?';
    a[i] = 0;
    int now = game_second();
    check_new_game(now);
    g_set_at = now;
    if (!strcmp(a, "surv_end"))
        g_end = end;
    else if (!strcmp(a, "surv_wave"))
        g_wave = end;
    else if (!strcmp(a, "surv_boss"))
        g_boss = end;
    else if (sscanf(a, "surv_zone %d %d %d %d", &g_zone[0], &g_zone[1], &g_zone[2], &g_zone[3]) == 4) {
        if (!g_zone_on || !g_zone_logged)
            logf_("survival: no-build zone x %d..%d, y %d..%d (tiles)", g_zone[0], g_zone[2] - 1, g_zone[1],
                  g_zone[3] - 1);
        g_zone_on = 1;
        g_zone_logged = 1;
    }
}

/* ---- the clocks ----------------------------------------------------------------------- */

static void write_line(const char *text, int x, int y, unsigned color_va)
{
    unsigned char str[0x14] __attribute__((aligned(4)));
    lang_string(str, text);
    memcpy((void *)VA(0x00ED80A0u), (void *)VA(color_va), 12); /* the screen Buffer's text colour */
    memcpy((void *)VA(0x00ED80DCu), (void *)VA(VA_BLACK), 12); /* and its shadow */
    *(unsigned char *)VA(0x00ED8118u) = 1;
    *(unsigned char *)VA(0x00ED811Du) = 1;
    int len = *(unsigned short *)(str + 8);
    if (len)
        ((write_right_t)VA(VA_WRITE_RIGHT))((void *)VA(VA_SCREEN), str, x, y, len);
    ((close_t)VA(VA_STRING_CLOSE))(str);
}

static void mmss(char *out, size_t n, const char *label, int secs)
{
    if (secs < 0)
        secs = 0;
    snprintf(out, n, "%s  %d:%02d", lang_text(label), secs / 60, secs % 60);
}

__attribute__((used)) void c_surv_draw(void)
{
    if (g_end < 0)
        return;
    int now = game_second();
    check_new_game(now);
    if (g_end < 0 || now > g_end + 20)
        return;
    char *gs = *(char **)VA(VA_GRAPHSYS);
    if (!gs)
        return;

    /* the font and layer draw_timers uses for its own lines */
    char *strs = *(char **)(*(char **)VA(VA_INT_STRS) + 0x10);
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), strs + STR_TIMER_FONT, 0xC, -1, 1);
    if (font && *(void **)font) {
        *(void **)VA(0x00ED808Cu) = font;
        *(int *)VA(0x00ED8090u) = 0;
        *(int *)VA(0x00ED8094u) = 0;
    }
    void *cur = *(void **)VA(0x00ED808Cu);
    void *inner = cur ? *(void **)cur : 0;
    int h = inner ? ((font_h_t)(*(void ***)inner)[0x24 / 4])(inner) : 16;
    if (h <= 0 || h > 64)
        h = 16;

    int saved = *(int *)(gs + 0x164);
    *(int *)(gs + 0x164) = *(int *)VA(VA_LAYER_UI);
    int x = *(int *)(gs + 0x360) - 10;
    int y = g_hud_y > 0 ? g_hud_y : 45 + 2 * h + 6;

    char line[96];
    int left = g_end - now;
    if (left > 0) {
        mmss(line, sizeof line, "Survive", left);
        write_line(line, x, y, VA_WHITE);
        /* the next wave: the nearer of the two timers still ahead and before the end */
        int next = -1, boss = 0;
        if (g_wave > now && g_wave <= g_end)
            next = g_wave;
        if (g_boss > now && g_boss <= g_end && (next < 0 || g_boss < next)) {
            next = g_boss;
            boss = 1;
        }
        if (next > 0) {
            int in = next - now;
            mmss(line, sizeof line, boss ? "Boss wave in" : "Next wave in", in);
            write_line(line, x, y + h + 2, in <= 10 ? VA_RED : boss ? VA_YELLOW : VA_WHITE);
        }
    } else {
        write_line(lang_text("Survived!"), x, y, VA_YELLOW);
    }
    *(int *)(gs + 0x164) = saved;
}

/* ---- the no-build zone ---------------------------------------------------------------- */

__attribute__((used)) int c_surv_blocked(int x, int y, int player)
{
    (void)player;
    if (!g_zone_on)
        return 0;
    int now = game_second();
    check_new_game(now);
    if (!g_zone_on)
        return 0;
    int tx = x / TILE, ty = y / TILE;
    return tx >= g_zone[0] && tx < g_zone[2] && ty >= g_zone[1] && ty < g_zone[3] ? BLOCK_ENEMY : 0;
}

void hk_surv_add_timer(void);
void hk_surv_draw_timers(void);
void hk_surv_blocked(void);

__asm__(".intel_syntax noprefix\n"
        /* add_timer(String &name, int end): note it, then the original */
        ".globl _hk_surv_add_timer\n_hk_surv_add_timer:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    call _c_surv_add_timer\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_add_timer]\n"

        /* draw_timers(int): the original, then the clocks; its result is kept */
        ".globl _hk_surv_draw_timers\n_hk_surv_draw_timers:\n"
        "    push dword ptr [esp+4]\n"
        "    call dword ptr [_g_orig_draw_timers]\n"
        "    push eax\n" HOOK_ENTER
        "    call _c_surv_draw\n" HOOK_LEAVE
        "    pop eax\n"
        "    ret 4\n"

        /* blocked_site(x, y, player, int, int *): the zone's code, or the original */
        ".globl _hk_surv_blocked\n_hk_surv_blocked:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+44]\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    call _c_surv_blocked\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp dword ptr [_g_orig_blocked]\n"
        "1:\n"
        "    ret 0x14\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version. */
void survival_install(void)
{
    if (!g_enabled)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[9];
        int steal;
        void *hook;
        void **orig;
    } hooks[] = {
        /* push ebp / mov ebp,esp / mov eax,fs:[0] */
        { "ScriptTimers::add_timer", VA_ADD_TIMER, { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0, 0, 0, 0 }, 9, hk_surv_add_timer,
          &g_orig_add_timer },
        /* push ebp / mov ebp,esp / push -1 */
        { "draw_timers", VA_DRAW_TIMERS, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5, hk_surv_draw_timers,
          &g_orig_draw_timers },
        /* push ebp / mov ebp,esp / sub esp,0x1C */
        { "blocked_site", VA_BLOCKED_SITE, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x1C }, 6, hk_surv_blocked,
          &g_orig_blocked },
    };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if (memcmp((void *)VA(hooks[i].va), hooks[i].sig, hooks[i].steal) != 0) {
            logf_("survival: signature mismatch at %s, survival clocks and zone turned off", hooks[i].name);
            g_enabled = 0;
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    for (int i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    logf_("survival: clocks and no-build zone for the Survival script installed");
}
