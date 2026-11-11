/*
 * The General's Heal: a fifth General ability. It takes the game's unfinished "Rally!" craft
 * (type 0x279): the General already has its button slot (4, 0), its animation, its sound and its
 * hotkey, and the general-heal mod's craftrules.xml switches it on (FROM General, 1000 mana).
 * SpellType::cast has no case for it (its jump table sends 0x279 to the exit, and cast_rally is an
 * empty `ret 8`), so the game pays the mana and does nothing else.
 *
 * ranks.c's hook of SpellType::cast calls heal_start for 0x279 once the cast is valid
 * (SpellTypeData::is_castable == 3), in the simulation, on every machine. For [heal] seconds the
 * General's own units within his radius (HeroData::get_radius, the circle of his other abilities,
 * 192 world units per tile, measured from him each time, so the circle moves with him) get back
 * [heal] percent of their max hit points in total, a little every 3 frames (heal_frame, from
 * airdrop.c's Objects::process_all). A unit that is in the circle the whole time gets exactly that
 * share. Decoys and buildings are not healed; soldiers of a squad who already died do not return.
 *
 * Heals in progress are not in saved games (they last 5 seconds).
 *
 * Hotkey: the game's key table (KeyMap::keymap_options) has no entry for this craft, so
 * Console::on_key_down is hooked: [heal] hotkey (a letter, no Shift / Ctrl / Alt) with a General
 * in the local selection presses the button (Options::do_spell(0x279, 0), the same as a click);
 * without a General the key does what it did before. panel.c shows the letter on the button.
 *
 * The same hook gives the Senator the Spy's Bribe key ([qol] senator_bribe_hotkey): the key table binds Bribe (0x275) in the Spy's key group only, so with a Senator the key
 * did nothing. With a Bribe caster in the selection, the key KeyMap::get_hotkey_string gives for
 * (4, 0x275) presses the Bribe button, as a click on it does.
 *
 * quicksave.c's key goes through the same hook, before the others; bigsel.c looks after the
 * game's handling of every key (a control group called back).
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_OBJECTS     0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object **, +0x15C + 4 * p = unit count */
#define VA_GAME        0x00C061ECu /* Game *: +0x550 = the frame (15 a second) */
#define VA_CONSOLE     0x00C06210u /* Console *: +0x298 the local player */
#define VA_HEROES      0x00C0A130u /* + 0x1C * p: HeroData ** by the General's hero index */
#define VA_HERO_RADIUS 0x00739E50u /* HeroData::get_radius() const: tiles */
#define VA_SOUND_PLAY  0x0097F770u /* SoundGlobal::play(SoundGlobalCat) const, ret 4 (ecx unused) */
#define VA_KEY_DOWN    0x007CC210u /* Console::on_key_down(int key), virtual, ret 4 */
#define VA_GET_CASTER  0x0070CA10u /* GroupData::get_spellcaster(type) const: -1 = none, ret 4 */
#define VA_DO_SPELL    0x0071D7A0u /* Options::do_spell(type, 0): the button, ret 8 */
#define VA_OPTIONS     0x00C06204u
#define VA_SELECTIONS  0x00E8D444u /* SelectGroup[], 0x9F0 each; Console +0x2A0 = the local one */
#define VA_HOTKEY_STRING  0x007D4FD0u /* KeyMap::get_hotkey_string(String &, code, type) */
#define VA_KEY_MAP        0x00C06228u /* KeyMap key_map */
#define VA_STRING_CTOR    0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE   0x00A1CF40u
#define VA_STRING_AS_CHAR 0x0042D710u

#define SPELL_HEAL   0x279
#define SPELL_BRIBE  0x275
#define KEY_MODS     0x70000   /* on_key_down: Shift / Ctrl / Alt bits on the key */

#define SND_GENERAL_RALLY 0x31 /* sound_global_cat_names 0x00ED3568: index of GENERAL_RALLY */
#define COORD_KEY    0x63637   /* x, y at +0x10, +0x14 xor this */
#define TILE         192
#define OBJ_FLAGS    0x08      /* bit 0: alive */
#define OBJ_OWNER    0x09
#define OBJ_ID       0x0A
#define OBJ_TYPE     0x18
#define OBJ_MAXHITS  0x20
#define OBJ_DAMAGE   0x24      /* hit points lost */
#define U_FLAGS      0x68      /* bit 0 = decoy */
#define U_HERO       0x86      /* word: the General's hero index */
#define OBJS_UNITS_N 0x15C
#define FIRST_BUILD  2000
#define STEP         3         /* frames between two heals: 5 a second */
#define MAX_HEALS    32

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);

int g_heal_enabled;
static int g_bribe_key = 1;
static int g_pct = 25, g_secs = 5, g_key = 'E';
static char g_key_name[2];
static void *g_orig_key_down;

static struct {
    char *general; /* NULL = free */
    int p, id, start;
    int units, hits; /* for the log: units healed, hit points given back */
} g_h[MAX_HEALS];
static int g_last_frame = -1;

void heal_load_config(const char *ini_path)
{
    g_heal_enabled = GetPrivateProfileIntA("gameplay", "general_heal", 1, ini_path);
    g_pct = GetPrivateProfileIntA("heal", "percent", g_pct, ini_path);
    g_secs = GetPrivateProfileIntA("heal", "seconds", g_secs, ini_path);
    if (g_secs < 1)
        g_secs = 1;
    char key[8];
    GetPrivateProfileStringA("heal", "hotkey", "E", key, sizeof key, ini_path);
    g_key = key[0] >= 'a' && key[0] <= 'z' ? key[0] - 32 : key[0];
    if (g_key < 'A' || g_key > 'Z' || key[1])
        g_key = 0; /* empty or not a single letter: no hotkey */
    g_key_name[0] = (char)g_key;
    g_bribe_key = GetPrivateProfileIntA("qol", "senator_bribe_hotkey", 1, ini_path);
}

/* For panel.c: the letter to show on the Heal button, NULL for any other button. */
const char *heal_hotkey(int code, int type)
{
    return g_heal_enabled && g_key && code == 4 && type == SPELL_HEAL ? g_key_name : NULL;
}

static int now(void) { return *(int *)(*(char **)VA(VA_GAME) + 0x550); }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

/* The General's radius in world units, 0 when he has no hero data. */
static long long radius(char *general, int p)
{
    char **heroes = *(char ***)VA(VA_HEROES + 0x1C * p);
    short h = *(short *)(general + U_HERO);
    char *hero = heroes && h >= 0 ? heroes[h] : NULL;
    return hero ? (long long)((fn0_t)VA(VA_HERO_RADIUS))(hero) * TILE : 0;
}

/* From ranks.c's SpellType::cast hook: a General cast Heal. */
void heal_start(char *general)
{
    if (!g_heal_enabled || !general)
        return;
    int p = (unsigned char)general[OBJ_OWNER], id = *(short *)(general + OBJ_ID);
    if (p > 7 || id < 0 || id >= FIRST_BUILD)
        return;
    for (int i = 0; i < MAX_HEALS; i++)
        if (!g_h[i].general) {
            g_h[i].general = general;
            g_h[i].p = p;
            g_h[i].id = id;
            g_h[i].start = now();
            g_h[i].units = g_h[i].hits = 0;
            char *console = *(char **)VA(VA_CONSOLE);
            if (console && *(int *)(console + 0x298) == p)
                ((fn1_t)VA(VA_SOUND_PLAY))(NULL, SND_GENERAL_RALLY);
            static int logged;
            if (logged < 20) {
                logged++;
                logf_("heal: player %d General %d heals %d%% over %d s within %d tiles", p, id, g_pct, g_secs,
                      (int)(radius(general, p) / TILE));
            }
            return;
        }
}

/* One heal step k of n for every unit of the General's player in his circle: the part of the
   total that falls in this step, so the steps add up to exactly g_pct. */
static void step(int i, int k, int n)
{
    char *g = g_h[i].general;
    int p = g_h[i].p;
    long long r = radius(g, p);
    if (r <= 0)
        return;
    int gx = *(int *)(g + 0x10) ^ COORD_KEY, gy = *(int *)(g + 0x14) ^ COORD_KEY;
    int count = *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_UNITS_N + 4 * p);
    if (count > FIRST_BUILD)
        count = FIRST_BUILD;
    for (int id = 0; id < count; id++) {
        char *u = object(p, id);
        if (!u || !*(char **)(u + OBJ_TYPE) || (*(int *)(u + U_FLAGS) & 1))
            continue;
        int lost = *(int *)(u + OBJ_DAMAGE);
        if (lost <= 0)
            continue;
        long long dx = (*(int *)(u + 0x10) ^ COORD_KEY) - gx, dy = (*(int *)(u + 0x14) ^ COORD_KEY) - gy;
        if (dx * dx + dy * dy > r * r)
            continue;
        long long total = (long long)*(int *)(u + OBJ_MAXHITS) * g_pct;
        int heal = (int)(total * k / (100LL * n) - total * (k - 1) / (100LL * n));
        if (heal > 0) {
            *(int *)(u + OBJ_DAMAGE) = lost > heal ? lost - heal : 0;
            g_h[i].units++;
            g_h[i].hits += lost > heal ? heal : lost;
        }
    }
}

/* Once per simulation frame (airdrop.c's Objects::process_all). */
void heal_frame(void)
{
    if (!g_heal_enabled)
        return;
    int t = now();
    if (t < g_last_frame || t - g_last_frame > 15) /* another game, or a saved game loaded */
        memset(g_h, 0, sizeof g_h);
    g_last_frame = t;
    int n = g_secs * 15 / STEP;
    for (int i = 0; i < MAX_HEALS; i++) {
        if (!g_h[i].general)
            continue;
        int age = t - g_h[i].start;
        if (object(g_h[i].p, g_h[i].id) != g_h[i].general || age > n * STEP) {
            static int logged;
            if (logged < 20) {
                logged++;
                logf_("heal: General %d of player %d done, %d heal steps on units, %d hit points back", g_h[i].id,
                      g_h[i].p, g_h[i].units, g_h[i].hits);
            }
            g_h[i].general = NULL; /* the General died, or the heal is over */
            continue;
        }
        if (age > 0 && age % STEP == 0)
            step(i, age / STEP, n);
    }
}

/* ---- the glow --------------------------------------------------------------------------- */

/* While a Heal runs, every unit of that player in the General's circle stands in bright green
   rings on the ground: three close rings and one that grows and fades every second.
   UnitOut::draw_underlays (virtual, per visible unit, where the game draws the selection circle)
   draws them after its own, with airdrop.c's draw_circle wrapper. Drawing only. */
#define VA_UNIT_UNDER 0x00614100u /* UnitOut::draw_underlays() const, thiscall */
#define OBJ_Z         0x0C

void ad_draw_circle(int x, int y, int z, int radius, const void *color);
extern UINT_PTR g_va_draw_circle; /* set by airdrop_install, which runs first */
static void *g_orig_underlays;

static void green(unsigned char *c, int a)
{
    int r = 40, g = 255, b = 70; /* red and blue nearly alike: either byte order is green */
    c[0] = (unsigned char)b, c[1] = (unsigned char)g, c[2] = (unsigned char)r, c[3] = (unsigned char)a;
    *(unsigned short *)(c + 4) = (unsigned short)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    *(unsigned short *)(c + 6) = (unsigned short)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    *(int *)(c + 8) = 2;
}

static int healed_now(char *u)
{
    int p = (unsigned char)u[OBJ_OWNER];
    if (p > 7 || (*(int *)(u + U_FLAGS) & 1))
        return 0;
    int x = *(int *)(u + 0x10) ^ COORD_KEY, y = *(int *)(u + 0x14) ^ COORD_KEY;
    for (int i = 0; i < MAX_HEALS; i++) {
        char *g = g_h[i].general;
        if (!g || g_h[i].p != p || object(p, g_h[i].id) != g)
            continue;
        long long r = radius(g, p), dx = x - (*(int *)(g + 0x10) ^ COORD_KEY), dy = y - (*(int *)(g + 0x14) ^ COORD_KEY);
        if (r > 0 && dx * dx + dy * dy <= r * r)
            return 1;
    }
    return 0;
}

__attribute__((thiscall)) static int heal_underlays(char *u)
{
    int ret = ((fn0_t)g_orig_underlays)(u);
    if (!g_heal_enabled || !g_va_draw_circle || !healed_now(u))
        return ret;
    int x = *(int *)(u + 0x10) ^ COORD_KEY, y = *(int *)(u + 0x14) ^ COORD_KEY, z = *(int *)(u + OBJ_Z) ^ COORD_KEY;
    unsigned char c[12];
    green(c, 0xF0);
    for (int k = 0; k < 3; k++)
        ad_draw_circle(x, y, z, TILE * 3 / 10 + k * 10, c);
    int ms = (int)(GetTickCount() % 1000);
    green(c, 0xF0 - ms * 0xD0 / 1000);
    ad_draw_circle(x, y, z, TILE * 3 / 10 + 30 + ms * (TILE / 3) / 1000, c);
    return ret;
}

/* ---- over the heads ---------------------------------------------------------------------- */

/* While a Heal runs, every unit it heals that the local player can see gets a green halo where
   its health bar is and green crosses ("heal_plus", ui-theme build_rank_icons.py) that rise from
   it and fade, three at a time, each unit out of step with its neighbours. From rankfx.c's map
   window redraw (Console::on_redraw), with its anchor and zoom. Drawing only. */
#define VA_GAMEC    0x00C061E8u /* +0x820 flags: 0x42 = everything is shown */
#define VT_VISIBLE  0x48        /* units: (player, 0) seen by that player */
#define PLUS_RISE   1.2f        /* seconds a cross takes to rise and fade */
#define PLUS_N      3

void *qol_iface_tex(const char *name);

static int visible(char *u, int local)
{
    if ((unsigned char)u[OBJ_OWNER] == local)
        return 1;
    const char *game = *(const char **)VA(VA_GAMEC);
    if (game && (*(const unsigned char *)(game + 0x820) & 0x42))
        return 1;
    return ((fn2_t)(*(void ***)u)[VT_VISIBLE / 4])(u, local, 0) != 0;
}

static void tinted(const void *tex, float cx, float cy, float w, float h, unsigned rgb, float alpha)
{
    if (alpha <= 0.0f || w < 0.5f)
        return;
    int a = (int)(alpha * 255.0f + 0.5f);
    rf_add(tex, cx - w * 0.5f, cy - h * 0.5f, w, h, (unsigned)(a > 255 ? 255 : a) << 24 | rgb);
}

void heal_draw_overhead(void)
{
    if (!g_heal_enabled)
        return;
    int any = 0;
    for (int i = 0; i < MAX_HEALS; i++)
        any |= g_h[i].general != NULL;
    const char *console = *(const char **)VA(VA_CONSOLE);
    const void *plus = any ? qol_iface_tex("heal_plus") : NULL;
    if (!plus || !console)
        return;
    const void *glow = qol_iface_tex("rank_glow");
    int local = *(const int *)(console + 0x298);
    float z = rankfx_zoom(), s = 22.0f * z;
    s = s < 12.0f ? 12.0f : s > 32.0f ? 32.0f : s;
    float t = (float)(GetTickCount() % 600000) / 1000.0f;
    for (int i = 0; i < MAX_HEALS; i++) {
        char *g = g_h[i].general;
        int p = g_h[i].p;
        if (!g || object(p, g_h[i].id) != g)
            continue;
        long long r = radius(g, p);
        if (r <= 0)
            continue;
        int gx = *(int *)(g + 0x10) ^ COORD_KEY, gy = *(int *)(g + 0x14) ^ COORD_KEY;
        int count = *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_UNITS_N + 4 * p);
        if (count > FIRST_BUILD)
            count = FIRST_BUILD;
        /* the first and last half second fade in and out */
        float age = (float)(now() - g_h[i].start) / 15.0f, total = (float)g_secs;
        float life = age < 0.5f ? age / 0.5f : age > total - 0.5f ? (total - age) / 0.5f : 1.0f;
        if (life <= 0.0f)
            continue;
        for (int id = 0; id < count; id++) {
            char *u = object(p, id);
            if (!u || !*(char **)(u + OBJ_TYPE) || (*(int *)(u + U_FLAGS) & 1))
                continue;
            long long dx = (*(int *)(u + 0x10) ^ COORD_KEY) - gx, dy = (*(int *)(u + 0x14) ^ COORD_KEY) - gy;
            float x, y;
            if (dx * dx + dy * dy > r * r || !visible(u, local) || !rankfx_anchor(u, &x, &y))
                continue;
            float phase = (float)((unsigned)id * 2654435761u >> 22) / 1024.0f; /* 0..1, per unit */
            if (glow) {
                float halo = s * (2.0f + 0.25f * sinf((t + phase) * 6.2831853f * 1.5f));
                tinted(glow, x, y - s * 0.2f, halo, halo, 0x50FF3C, 0.55f * life);
            }
            for (int k = 0; k < PLUS_N; k++) {
                float f = (t / PLUS_RISE) + phase + (float)k / PLUS_N;
                f -= floorf(f); /* 0..1 through this cross's rise */
                float drift = sinf((phase * 7.0f + (float)k * 2.1f) * 3.0f) * s * 0.45f;
                float size = s * (0.55f + 0.35f * (f < 0.2f ? f / 0.2f : 1.0f));
                float alpha = (f < 0.15f ? f / 0.15f : f > 0.6f ? (1.0f - f) / 0.4f : 1.0f) * life;
                tinted(plus, x + drift, y - s * 0.4f - f * s * 2.4f, size, size, 0x5AFF46, alpha);
            }
        }
    }
}

/* ---- the hotkey ------------------------------------------------------------------------- */

typedef int(__attribute__((thiscall)) *key_down_t)(void *self, int key);
typedef void(__attribute__((thiscall)) *str_ctor_t)(void *self, const char *s);
typedef const char *(__attribute__((thiscall)) *as_char_t)(void *self);
typedef int(__attribute__((thiscall)) *hotkey_t)(void *keymap, void *out, int code, int type);

/* The Spy's Bribe key as a capital letter, 0 if it is none or has Shift / Ctrl / Alt. */
static int bribe_letter(void)
{
    unsigned char str[64] = { 0 };
    ((str_ctor_t)VA(VA_STRING_CTOR))(str, "");
    ((hotkey_t)VA(VA_HOTKEY_STRING))((void *)VA(VA_KEY_MAP), str, 4, SPELL_BRIBE);
    int k = 0;
    if (*(unsigned short *)(str + 8) == 1) {
        k = (unsigned char)((as_char_t)VA(VA_STRING_AS_CHAR))(str)[0];
        k = k >= 'a' && k <= 'z' ? k - 32 : k;
    }
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    return k >= 'A' && k <= 'Z' ? k : 0;
}

__attribute__((used)) int c_heal_key_down(void *self, int *a)
{
    int key = a[0];
    if (quicksave_key(key))
        return 1;
    char *console = *(char **)VA(VA_CONSOLE), *sel = *(char **)VA(VA_SELECTIONS);
    void *opts = *(void **)VA(VA_OPTIONS);
    if (g_bribe_key && !(key & KEY_MODS) && console && sel && opts) {
        char *group = sel + *(int *)(console + 0x2A0) * 0x9F0;
        if (((fn1_t)VA(VA_GET_CASTER))(group, SPELL_BRIBE) >= 0 && (key & 0xFFFF) == bribe_letter()) {
            ((fn2_t)VA(VA_DO_SPELL))(opts, SPELL_BRIBE, 0);
            return 1;
        }
    }
    if (g_heal_enabled && g_key && !(key & KEY_MODS) && (key & 0xFFFF) == g_key && console && sel && opts) {
        char *group = sel + *(int *)(console + 0x2A0) * 0x9F0;
        if (((fn1_t)VA(VA_GET_CASTER))(group, SPELL_HEAL) >= 0) {
            ((fn2_t)VA(VA_DO_SPELL))(opts, SPELL_HEAL, 0);
            return 1;
        }
    }
    unsigned gen = bigsel_gen();
    int r = ((key_down_t)g_orig_key_down)(self, key);
    bigsel_after_key(gen); /* bigsel.c: a control group's overflow comes back with it */
    return r;
}

void hk_heal_key_down(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_heal_key_down\n"
        "_hk_heal_key_down:\n" HOOK_ENTER "    sub esp, 8\n"
        "    lea eax, [ebx+36]\n"
        "    push eax\n"
        "    push ecx\n"
        "    call _c_heal_key_down\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE "    ret 4\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version. */
void heal_install(void)
{
    if (g_heal_enabled) {
        /* push ebp / mov ebp,esp / push -1 (then the exception handler) */
        static const unsigned char under_sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
        unsigned char *under = (unsigned char *)VA(VA_UNIT_UNDER);
        unsigned char *umem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (umem && memcmp(under, under_sig, sizeof under_sig) == 0) {
            g_orig_underlays = hook_trampoline(under, 5, umem);
            hook_jmp5(under, heal_underlays);
            logf_("heal: green rings installed");
        } else {
            logf_("heal: signature mismatch at UnitOut::draw_underlays, no green rings");
        }
    }
    if (!(g_heal_enabled && g_key) && !g_bribe_key && !g_quicksave_key && !g_bigsel_enabled) {
        FlushInstructionCache(GetCurrentProcess(), NULL, 0);
        return;
    }
    /* push ebp / mov ebp,esp / mov eax, fs:[0] */
    static const unsigned char sig[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    unsigned char *at = (unsigned char *)VA(VA_KEY_DOWN);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("heal: signature mismatch at Console::on_key_down, no hotkey");
        g_key = 0;
        g_bribe_key = 0;
        g_quicksave_key = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    g_orig_key_down = hook_trampoline(at, 9, mem);
    hook_jmp5(at, hk_heal_key_down);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("heal: hotkey %c%s%s", g_heal_enabled && g_key ? g_key : '-',
          g_bribe_key ? "; the Senator's Bribe on the Spy's key" : "",
          g_quicksave_key ? "; quicksave key" : "");
}
