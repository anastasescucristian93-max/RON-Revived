/*
 * The launcher's language button ([ui] language_button). Under Workshop, a round button
 * with the chosen language's flag in a frame; a click opens the other languages around it like
 * a flower (the petals swirl out from behind it), and a flag picked there becomes the game's
 * language and flies into the button. The game reads its language when it starts, from
 * Language= in %APPDATA%\Microsoft Games\Rise of Nations\rise2.ini, [RISE OF NATIONS] (where the
 * options' combo saves it), so the launcher writes it there.
 *
 * The launcher's own captions follow: patriots.exe reads them from launcher_res\locale.ini
 * ([launcher] 170023 Play, 170024 Workshop, 170025 RoN Website, 170027 Achievements, 170032..37
 * the Workshop agreement) through GetPrivateProfileStringA/W. Those reads get
 * mod_override\launcher_res\locale\<code>.ini of the chosen language (built by translations\),
 * and the buttons' GdipDrawString gets the chosen language's caption, so a pick shows at once.
 *
 * The button is a popup owned by the controls' dialog, drawn through UpdateLayeredWindow
 * (per-pixel alpha: smooth edges over the panel, and the mouse passes through where it is
 * clear). Pictures: launcher_res\lang\<code>.png (the round flag) and lang\frame.png (the ring
 * laid over every flag), square, any size; ui-theme\tools\build_launcher.py makes them.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAXL        10
#define ID_WORKSHOP 1017
#define ANIM_TIMER  1
/* sizes at the theme's 100 %, in pixels */
#define BTN      64.0f /* the button (ui-theme\tools\lang_flags.py has the same three) */
#define PETAL    50.0f /* a flag of the flower */
#define RING     90.0f /* the petals' distance from the button's centre */
#define GAP      14.0f /* between Workshop's plate and the button */
#define OPEN_MS  460.0f
#define CLOSE_MS 300.0f
#define DELAY    0.05f /* each petal starts this much (of OPEN_MS) after the one before */
#define CAPS     5
#define PI_F     3.14159265f

typedef struct {
    DWORD *px; /* premultiplied BGRA */
    int w, h;
} img_t;

typedef struct {
    unsigned char *cov; /* text coverage */
    int w, h;
} label_t;

typedef DWORD(WINAPI *gpps_w_t)(LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, DWORD, LPCWSTR);
typedef DWORD(WINAPI *gpps_a_t)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
typedef int(WINAPI *draw_string_t)(void *, const WCHAR *, int, const void *, const void *, const void *,
                                   const void *);

static gpps_w_t r_gpps_w;
static gpps_a_t r_gpps_a;
static draw_string_t r_draw_string;

static char g_res[MAX_PATH]; /* <game>\mod_override\launcher_res\ */
static char g_prefs[MAX_PATH];
static float g_k = 1.0f;     /* the theme's scale */
static WCHAR g_face[64];
static int g_n;              /* languages offered */
static int g_cur;            /* the chosen one (the game's order) */
static img_t g_flag[MAXL], g_frame;
static label_t g_label[MAXL];

/* the launcher's captions, per language, and as the launcher read them first */
static const int k_cap_keys[CAPS] = {170023, 170024, 170025, 170026, 170027};
static const WCHAR *k_cap_default[CAPS] = {L"Play", L"Workshop", L"RoN Website", L"Leaderboards", L"Achievement"};
static WCHAR g_cap[MAXL][CAPS][64];
static WCHAR g_cap_game[CAPS][64]; /* the game's launcher_res\locale.ini */

static HWND g_dlg, g_win;
static WNDPROC g_dlg_proc;
static int g_dlg_unicode;
static int g_size; /* the popup is g_size x g_size, the button in its middle */
static DWORD *g_px;
static HDC g_dc;

static struct {
    float t;              /* 0 closed .. 1 open */
    int target;           /* 1 opening or open, 0 closing or closed */
    DWORD last;           /* the last tick */
    int running;
    int m, slot[MAXL];    /* the petals, in order from the top clockwise (all but the chosen one) */
    int shown;            /* the flag in the button */
    int picked;           /* the petal flying into the button, or -1 */
    int hover;            /* a language under the mouse, -2 the button, -1 nothing */
    float hv[MAXL], bhv;  /* hover light, eased */
    int tracking;
} g_a;

/* --- the language: rise2.ini, else Steam's ------------------------------------------------ */

static int index_of_code(const char *code)
{
    for (int i = 0; i < g_n; i++) {
        char c[8];
        WideCharToMultiByte(CP_ACP, 0, lang_short(i), -1, c, sizeof c, NULL, NULL);
        if (_stricmp(c, code) == 0)
            return i;
    }
    return -1;
}

static int read_language(void)
{
    char code[16] = "";
    if (g_prefs[0])
        GetPrivateProfileStringA("RISE OF NATIONS", "Language", "", code, sizeof code, g_prefs);
    int i = code[0] ? index_of_code(code) : -1;
    if (i >= 0)
        return i;
    /* none saved yet: the game takes Steam's language, comparing four letters */
    HKEY key;
    char steam[64] = "";
    DWORD size = sizeof steam - 1;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegQueryValueExA(key, "Language", NULL, NULL, (BYTE *)steam, &size);
        RegCloseKey(key);
    }
    for (i = 0; steam[0] && i < g_n; i++)
        if (_strnicmp(steam, lang_steam(i), 4) == 0)
            return i;
    return 1; /* English */
}

static void write_language(int i)
{
    char code[8], dir[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, lang_short(i), -1, code, sizeof code, NULL, NULL);
    /* the folders, for a game never started */
    snprintf(dir, sizeof dir, "%s", g_prefs);
    for (char *p = strchr(dir + 3, '\\'); p; p = strchr(p + 1, '\\')) {
        *p = 0;
        CreateDirectoryA(dir, NULL);
        *p = '\\';
    }
    BOOL ok = WritePrivateProfileStringA("RISE OF NATIONS", "Language", code, g_prefs);
    logf_("launcher: language %s %s in %s", code, ok ? "saved" : "NOT saved", g_prefs);
}

/* --- the launcher's captions -------------------------------------------------------------- */

static void locale_path(int lang, char *out, size_t size)
{
    char code[8];
    WideCharToMultiByte(CP_ACP, 0, lang_short(lang), -1, code, sizeof code, NULL, NULL);
    snprintf(out, size, "%slocale\\%s.ini", g_res, code);
}

/* [launcher] `key` in the chosen language's locale\<code>.ini, else English's, else `def`; a
   \n in it becomes a new line */
void launcher_text(const char *key, const WCHAR *def, WCHAR *out, int size)
{
    char a[MAX_PATH];
    WCHAR w[MAX_PATH], k[64];
    MultiByteToWideChar(CP_ACP, 0, key, -1, k, 64);
    out[0] = 0;
    for (int pass = 0; pass < 2 && !out[0] && g_res[0]; pass++) {
        if (pass == 0)
            locale_path(g_cur, a, sizeof a);
        else
            snprintf(a, sizeof a, "%slocale\\EN.ini", g_res);
        MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
        GetPrivateProfileStringW(L"launcher", k, L"", out, (DWORD)size, w);
    }
    if (!out[0]) {
        wcsncpy(out, def, (size_t)size - 1);
        out[size - 1] = 0;
    }
    for (WCHAR *p = out, *q = out;; p++, q++) {
        if (p[0] == L'\\' && p[1] == L'n')
            *q = L'\n', p++;
        else
            *q = *p;
        if (!*p)
            break;
    }
}

static int is_locale_w(const WCHAR *file)
{
    size_t n = file ? wcslen(file) : 0;
    return n >= 23 && _wcsicmp(file + n - 23, L"launcher_res\\locale.ini") == 0;
}

static int is_locale_a(const char *file)
{
    size_t n = file ? strlen(file) : 0;
    return n >= 23 && _stricmp(file + n - 23, "launcher_res\\locale.ini") == 0;
}

static DWORD WINAPI h_gpps_w(LPCWSTR app, LPCWSTR key, LPCWSTR def, LPWSTR ret, DWORD size, LPCWSTR file)
{
    if (is_locale_w(file)) {
        char a[MAX_PATH];
        WCHAR w[MAX_PATH];
        locale_path(g_cur, a, sizeof a);
        MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
        if (GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES)
            return r_gpps_w(app, key, def, ret, size, w);
    }
    return r_gpps_w(app, key, def, ret, size, file);
}

static DWORD WINAPI h_gpps_a(LPCSTR app, LPCSTR key, LPCSTR def, LPSTR ret, DWORD size, LPCSTR file)
{
    if (is_locale_a(file)) {
        char a[MAX_PATH];
        locale_path(g_cur, a, sizeof a);
        if (GetFileAttributesA(a) != INVALID_FILE_ATTRIBUTES)
            return r_gpps_a(app, key, def, ret, size, a);
    }
    return r_gpps_a(app, key, def, ret, size, file);
}

static int same(const WCHAR *s, int len, const WCHAR *c)
{
    size_t n = wcslen(c);
    if (!n)
        return 0;
    return len < 0 ? wcscmp(s, c) == 0 : (size_t)len == n && wcsncmp(s, c, n) == 0;
}

/* Which caption `s` is, in whatever language the launcher read it, or -1. */
static int caption_of(const WCHAR *s, int len)
{
    if (!s)
        return -1;
    for (int k = 0; k < CAPS; k++) {
        if (same(s, len, k_cap_default[k]) || same(s, len, g_cap_game[k]))
            return k;
        for (int i = 0; i < g_n; i++)
            if (same(s, len, g_cap[i][k]))
                return k;
    }
    return -1;
}

static int WINAPI h_draw_string(void *g, const WCHAR *s, int len, const void *font, const void *rect,
                                const void *fmt, const void *brush)
{
    int k = caption_of(s, len);
    if (k >= 0 && g_cap[g_cur][k][0])
        return r_draw_string(g, g_cap[g_cur][k], -1, font, rect, fmt, brush);
    return r_draw_string(g, s, len, font, rect, fmt, brush);
}

static void read_captions(void)
{
    char game[MAX_PATH], a[MAX_PATH];
    WCHAR w[MAX_PATH], key[16];
    snprintf(game, sizeof game, "%slauncher_res\\locale.ini", g_dir);
    for (int k = 0; k < CAPS; k++) {
        swprintf(key, 16, L"%d", k_cap_keys[k]);
        MultiByteToWideChar(CP_ACP, 0, game, -1, w, MAX_PATH);
        GetPrivateProfileStringW(L"launcher", key, L"", g_cap_game[k], 64, w);
        for (int i = 0; i < g_n; i++) {
            locale_path(i, a, sizeof a);
            MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
            GetPrivateProfileStringW(L"launcher", key, L"", g_cap[i][k], 64, w);
        }
    }
}

/* --- drawing ------------------------------------------------------------------------------ */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static float smooth(float v)
{
    v = clampf(v, 0.0f, 1.0f);
    return v * v * (3.0f - 2.0f * v);
}

static float ease_out_back(float u)
{
    const float c1 = 1.25f, c3 = c1 + 1.0f;
    u -= 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* premultiplied `src` (b, g, r, a as 0..255 floats) over the pixel */
static void over(DWORD *d, float b, float g, float r, float a)
{
    float k = 1.0f - a / 255.0f;
    DWORD p = *d;
    float db = (float)(p & 255) * k + b, dg = (float)((p >> 8) & 255) * k + g;
    float dr = (float)((p >> 16) & 255) * k + r, da = (float)(p >> 24) * k + a;
    *d = (DWORD)clampf(db + 0.5f, 0, 255) | (DWORD)clampf(dg + 0.5f, 0, 255) << 8 |
         (DWORD)clampf(dr + 0.5f, 0, 255) << 16 | (DWORD)clampf(da + 0.5f, 0, 255) << 24;
}

static void sample(const img_t *im, float u, float v, float *acc)
{
    u -= 0.5f, v -= 0.5f;
    int x = (int)floorf(u), y = (int)floorf(v);
    float fx = u - (float)x, fy = v - (float)y;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            int sx = x + i, sy = y + j;
            if (sx < 0 || sy < 0 || sx >= im->w || sy >= im->h)
                continue;
            float wgt = (i ? fx : 1.0f - fx) * (j ? fy : 1.0f - fy);
            DWORD p = im->px[(size_t)sy * im->w + sx];
            for (int c = 0; c < 4; c++)
                acc[c] += (float)((p >> (8 * c)) & 255) * wgt;
        }
}

/* `im` centred on (cx, cy), `size` pixels across, turned by `angle`, faded by `alpha`, lit by
   `light` (1 as it is); several samples per pixel when it is drawn smaller than it is. */
static void draw_img(const img_t *im, float cx, float cy, float size, float angle, float alpha, float light)
{
    if (!im->px || size < 1.0f || alpha <= 0.0f)
        return;
    float k = (float)im->w / size;
    int n = k > 1.0f ? (int)ceilf(k) : 1;
    if (n > 4)
        n = 4;
    float c = cosf(angle), s = sinf(angle), half = size * 0.7072f + 1.0f, wgt = alpha / (float)(n * n);
    int x0 = (int)floorf(cx - half), x1 = (int)ceilf(cx + half), y0 = (int)floorf(cy - half),
        y1 = (int)ceilf(cy + half);
    x0 = x0 < 0 ? 0 : x0, y0 = y0 < 0 ? 0 : y0, x1 = x1 > g_size ? g_size : x1, y1 = y1 > g_size ? g_size : y1;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float acc[4] = {0, 0, 0, 0};
            for (int sy = 0; sy < n; sy++)
                for (int sx = 0; sx < n; sx++) {
                    float px = (float)x + ((float)sx + 0.5f) / (float)n - cx;
                    float py = (float)y + ((float)sy + 0.5f) / (float)n - cy;
                    sample(im, (c * px + s * py) * k + (float)im->w * 0.5f, (-s * px + c * py) * k + (float)im->h * 0.5f,
                           acc);
                }
            float a = acc[3] * wgt;
            if (a < 0.5f)
                continue;
            float l = wgt * light;
            over(g_px + (size_t)y * g_size + x, fminf(acc[0] * l, a), fminf(acc[1] * l, a), fminf(acc[2] * l, a), a);
        }
}

/* a dark disc of radius r, fading out from `inner` (0..1 of r) to its edge */
static void draw_shade(float cx, float cy, float r, float inner, float alpha)
{
    for (int y = 0; y < g_size; y++)
        for (int x = 0; x < g_size; x++) {
            float d = hypotf((float)x + 0.5f - cx, (float)y + 0.5f - cy) / r;
            if (d >= 1.0f)
                continue;
            float a = alpha * 255.0f * (1.0f - smooth((d - inner) / (1.0f - inner)));
            if (a >= 0.5f)
                over(g_px + (size_t)y * g_size + x, 0, 0, 0, a);
        }
}

static void draw_label(const label_t *lb, float cx, float cy, float alpha)
{
    if (!lb->cov)
        return;
    int x0 = (int)(cx - lb->w / 2.0f), y0 = (int)(cy - lb->h / 2.0f);
    for (int y = 0; y < lb->h; y++)
        for (int x = 0; x < lb->w; x++) {
            int gx = x0 + x, gy = y0 + y;
            float a = lb->cov[(size_t)y * lb->w + x] * alpha;
            if (a < 0.5f || gx < 0 || gy < 0 || gx >= g_size || gy >= g_size)
                continue;
            over(g_px + (size_t)gy * g_size + gx, a * 200 / 255, a * 228 / 255, a * 240 / 255, a); /* warm white */
        }
}

/* the language's name, white on black, its coverage kept */
static void make_label(int i)
{
    int w = (int)(BTN * g_k * 0.92f), h = (int)(BTN * g_k * 0.8f);
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
    void *bits = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dc || !dib) {
        if (dc)
            DeleteDC(dc);
        return;
    }
    HGDIOBJ old_bmp = SelectObject(dc, dib);
    HFONT font = CreateFontW(-(int)(13.0f * g_k + 0.5f), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                             ANTIALIASED_QUALITY, 0, g_face[0] ? g_face : L"Segoe UI");
    HGDIOBJ old_font = SelectObject(dc, font);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    RECT calc = {0, 0, w, h}, r;
    DrawTextW(dc, lang_name(i), -1, &calc, DT_CENTER | DT_WORDBREAK | DT_CALCRECT);
    r.left = 0, r.right = w, r.top = (h - (calc.bottom - calc.top)) / 2, r.bottom = h;
    DrawTextW(dc, lang_name(i), -1, &r, DT_CENTER | DT_WORDBREAK);
    GdiFlush();
    g_label[i].cov = malloc((size_t)w * h);
    if (g_label[i].cov) {
        for (int p = 0; p < w * h; p++)
            g_label[i].cov[p] = (unsigned char)((((DWORD *)bits)[p] >> 8) & 255);
        g_label[i].w = w, g_label[i].h = h;
    }
    SelectObject(dc, old_font);
    SelectObject(dc, old_bmp);
    DeleteObject(font);
    DeleteObject(dib);
    DeleteDC(dc);
}

static void draw_flag(int lang, float x, float y, float size, float spin, float alpha, float light)
{
    draw_img(&g_flag[lang], x, y, size, 0.0f, alpha, light);
    draw_img(&g_frame, x, y, size, spin, alpha, light);
}

/* petal j at t (0..1 of the opening): its place, size, fade and the frame's turn */
static void petal_at(int j, float t, float *x, float *y, float *size, float *alpha, float *spin)
{
    float span = 1.0f - DELAY * (float)(g_a.m - 1);
    float u = clampf((t - DELAY * (float)j) / span, 0.0f, 1.0f), e = ease_out_back(u);
    float a = -PI_F / 2.0f + (float)j * 2.0f * PI_F / (float)g_a.m - (1.0f - e) * 1.1f;
    float c = g_size / 2.0f, r = RING * g_k * e;
    *x = c + r * cosf(a);
    *y = c + r * sinf(a);
    *size = PETAL * g_k * (0.35f + 0.65f * e) * (1.0f + 0.12f * g_a.hv[g_a.slot[j]]);
    *alpha = clampf(u * 3.0f, 0.0f, 1.0f);
    *spin = -(1.0f - e) * 1.6f;
}

static void render(void)
{
    if (!g_win)
        return;
    float t = g_a.t, c = g_size / 2.0f;
    DWORD fill = g_a.target || t > 0.0f ? 0x01000000u : 0; /* open: catch every click on it */
    for (int i = 0; i < g_size * g_size; i++)
        g_px[i] = fill;
    if (t > 0.0f)
        draw_shade(c, c, (RING + PETAL * 0.8f) * g_k, 0.55f, 0.55f * smooth(t * 1.5f));
    float x, y, size, alpha, spin;
    for (int j = 0; j < g_a.m; j++)
        if (t > 0.0f && g_a.slot[j] != g_a.picked) {
            petal_at(j, t, &x, &y, &size, &alpha, &spin);
            draw_flag(g_a.slot[j], x, y, size, spin, alpha, 1.0f + 0.25f * g_a.hv[g_a.slot[j]]);
        }
    draw_flag(g_a.shown, c, c, BTN * g_k * (1.0f + 0.04f * g_a.bhv), t * 1.05f, 1.0f, 1.0f + 0.15f * g_a.bhv);
    /* the hovered language's name over the button */
    if (g_a.hover >= 0 && g_a.target) {
        float a = smooth((t - 0.6f) / 0.4f);
        draw_shade(c, c, BTN * g_k * 0.47f, 0.8f, 0.82f * a);
        draw_label(&g_label[g_a.hover], c, c, a);
    }
    /* the picked flag flies into the button while the others fold */
    for (int j = 0; j < g_a.m; j++)
        if (g_a.slot[j] == g_a.picked) {
            float px, py, ps, pa, sp, e = smooth(1.0f - t);
            petal_at(j, 1.0f, &px, &py, &ps, &pa, &sp);
            draw_flag(g_a.picked, px + (c - px) * e, py + (c - py) * e, ps + (BTN * g_k - ps) * e, e * 2.0f, 1.0f,
                      1.25f - 0.25f * e);
        }
    POINT zero = {0, 0};
    SIZE sz = {g_size, g_size};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g_win, NULL, NULL, &sz, g_dc, &zero, 0, &blend, ULW_ALPHA);
}

/* --- the window --------------------------------------------------------------------------- */

static void animate(void)
{
    if (!g_a.running) {
        g_a.running = 1;
        g_a.last = GetTickCount();
        SetTimer(g_win, ANIM_TIMER, 15, NULL);
    }
}

static void tick(void)
{
    DWORD now = GetTickCount();
    float dt = (float)(now - g_a.last);
    g_a.last = now;
    g_a.t = clampf(g_a.t + (g_a.target ? dt / OPEN_MS : -dt / CLOSE_MS), 0.0f, 1.0f);
    if (g_a.t == 0.0f && g_a.picked >= 0) {
        g_a.shown = g_a.picked;
        g_a.picked = -1;
    }
    int busy = g_a.t != (g_a.target ? 1.0f : 0.0f);
    float k = clampf(dt / 90.0f, 0.0f, 1.0f);
    for (int i = 0; i < g_n; i++) {
        float want = g_a.hover == i ? 1.0f : 0.0f;
        g_a.hv[i] += (want - g_a.hv[i]) * k;
        if (fabsf(want - g_a.hv[i]) < 0.01f)
            g_a.hv[i] = want;
        else
            busy = 1;
    }
    float want = g_a.hover == -2 && !g_a.target ? 1.0f : 0.0f;
    g_a.bhv += (want - g_a.bhv) * k;
    if (fabsf(want - g_a.bhv) < 0.01f)
        g_a.bhv = want;
    else
        busy = 1;
    if (!busy) {
        KillTimer(g_win, ANIM_TIMER);
        g_a.running = 0;
    }
    render();
}

/* what is under (x, y): a petal's language, -2 the button, -1 nothing */
static int hit(int px, int py)
{
    float c = g_size / 2.0f, x, y, size, alpha, spin;
    if (g_a.target && g_a.t > 0.6f)
        for (int j = 0; j < g_a.m; j++) {
            petal_at(j, 1.0f, &x, &y, &size, &alpha, &spin);
            if (hypotf((float)px + 0.5f - x, (float)py + 0.5f - y) < size * 0.5f)
                return g_a.slot[j];
        }
    return hypotf((float)px + 0.5f - c, (float)py + 0.5f - c) < BTN * g_k * 0.5f ? -2 : -1;
}

static void open_flower(void)
{
    static const int order[MAXL] = {1, 0, 2, 3, 4, 5, 6, 7, 8, 9}; /* English first */
    g_a.m = 0;
    for (int i = 0; i < MAXL; i++)
        if (order[i] < g_n && order[i] != g_cur && g_flag[order[i]].px)
            g_a.slot[g_a.m++] = order[i];
    if (!g_a.m)
        return;
    g_a.target = 1;
    SetCapture(g_win);
    animate();
}

static void close_flower(void)
{
    g_a.target = 0;
    g_a.hover = -1;
    if (GetCapture() == g_win)
        ReleaseCapture();
    animate();
}

static void pick(int lang)
{
    g_a.picked = lang;
    g_cur = lang;
    write_language(lang);
    close_flower();
    if (g_dlg) /* the launcher's captions in the new language */
        RedrawWindow(g_dlg, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    launcher_update_relabel();
}

static LRESULT CALLBACK win_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, g_a.hover != -1 ? IDC_HAND : IDC_ARROW));
        return TRUE;
    case WM_MOUSEMOVE: {
        int was = g_a.hover;
        g_a.hover = hit(x, y);
        if (!g_a.tracking && GetCapture() != h) {
            TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, h, 0};
            g_a.tracking = TrackMouseEvent(&tme);
        }
        if (GetCapture() == h)
            SetCursor(LoadCursor(NULL, g_a.hover != -1 ? IDC_HAND : IDC_ARROW));
        if (was != g_a.hover) {
            if (g_a.hover != -1)
                launcher_sound(0);
            animate();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_a.tracking = 0;
        if (GetCapture() != h) {
            g_a.hover = -1;
            animate();
        }
        return 0;
    case WM_LBUTTONDOWN: {
        int at = hit(x, y);
        if (at != -1)
            launcher_sound(1);
        if (!g_a.target) {
            if (at == -2)
                open_flower();
        } else if (at >= 0)
            pick(at);
        else
            close_flower();
        return 0;
    }
    case WM_CAPTURECHANGED:
        if (g_a.target && (HWND)lp != h)
            close_flower();
        return 0;
    case WM_TIMER:
        if (wp == ANIM_TIMER)
            tick();
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

/* the button's place: under Workshop, on its middle */
static void place(void)
{
    HWND ws = GetDlgItem(g_dlg, ID_WORKSHOP);
    RECT r;
    if (!g_win || !ws || !GetWindowRect(ws, &r))
        return;
    int cx = (r.left + r.right) / 2, cy = r.bottom + (int)((GAP + BTN / 2.0f) * g_k + 0.5f);
    SetWindowPos(g_win, NULL, cx - g_size / 2, cy - g_size / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static LRESULT CALLBACK dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT r = g_dlg_unicode ? CallWindowProcW(g_dlg_proc, h, msg, wp, lp) : CallWindowProcA(g_dlg_proc, h, msg, wp, lp);
    if (!g_win)
        return r;
    if (msg == WM_WINDOWPOSCHANGED) {
        const WINDOWPOS *pos = (const WINDOWPOS *)lp;
        place();
        if (pos->flags & SWP_SHOWWINDOW)
            ShowWindow(g_win, SW_SHOWNOACTIVATE);
        if (pos->flags & SWP_HIDEWINDOW)
            ShowWindow(g_win, SW_HIDE);
    } else if (msg == WM_SHOWWINDOW)
        ShowWindow(g_win, wp ? SW_SHOWNOACTIVATE : SW_HIDE);
    else if (msg == WM_DESTROY) {
        DestroyWindow(g_win);
        g_win = NULL;
    }
    return r;
}

/* The flags and the frame; GDI+ is started here (it counts its users) in case the launcher
   has not started it yet. Returns how many flags. */
static int load_pictures(void)
{
    typedef struct {
        UINT32 version;
        void *callback;
        BOOL no_thread, no_codecs;
    } gp_input_t;
    typedef int(WINAPI *gp_startup_t)(ULONG_PTR *, const gp_input_t *, void *);
    HMODULE gp = LoadLibraryA("gdiplus.dll");
    gp_startup_t startup = gp ? (gp_startup_t)GetProcAddress(gp, "GdiplusStartup") : NULL;
    ULONG_PTR token;
    gp_input_t in = {1, NULL, FALSE, FALSE};
    if (!startup || startup(&token, &in, NULL) != 0)
        return 0;
    char name[32], code[8];
    int flags = 0;
    for (int i = 0; i < g_n; i++) {
        WideCharToMultiByte(CP_ACP, 0, lang_short(i), -1, code, sizeof code, NULL, NULL);
        snprintf(name, sizeof name, "lang\\%s.png", code);
        g_flag[i].px = launcher_load_bgra(name, 0, &g_flag[i].w, &g_flag[i].h);
        flags += !!g_flag[i].px;
    }
    g_frame.px = launcher_load_bgra("lang\\frame.png", 0, &g_frame.w, &g_frame.h);
    return flags;
}

void launcher_lang_attach(HWND dlg)
{
    if (!g_n || g_dlg || !dlg || !GetDlgItem(dlg, ID_WORKSHOP))
        return;
    g_dlg = dlg; /* once */
    int flags = load_pictures();
    if (!flags) {
        logf_("launcher: no flags in %slang\\, no language button", g_res);
        return;
    }
    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = win_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"RonRevivedLanguage";
    RegisterClassW(&wc);
    g_size = (int)ceilf(2.0f * (RING * 1.08f + PETAL * 1.12f * 0.55f + 4.0f) * g_k);
    g_win = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0, 0,
                            g_size, g_size, dlg, NULL, inst, NULL);
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), g_size, -g_size, 1, 32, BI_RGB}};
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    g_dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!g_win || !dib || !g_dc) {
        logf_("launcher: language button not made");
        if (g_win)
            DestroyWindow(g_win);
        g_win = NULL;
        return;
    }
    SelectObject(g_dc, dib);
    g_px = bits;
    for (int i = 0; i < g_n; i++)
        make_label(i);
    g_dlg_unicode = IsWindowUnicode(dlg);
    g_dlg_proc = (WNDPROC)(g_dlg_unicode ? SetWindowLongPtrW(dlg, GWLP_WNDPROC, (LONG_PTR)dlg_proc)
                                         : SetWindowLongPtrA(dlg, GWLP_WNDPROC, (LONG_PTR)dlg_proc));
    place();
    render();
    if (IsWindowVisible(dlg))
        ShowWindow(g_win, SW_SHOWNOACTIVATE);
    char code[8];
    WideCharToMultiByte(CP_ACP, 0, lang_short(g_cur), -1, code, sizeof code, NULL, NULL);
    logf_("launcher: language button %dx%d under Workshop, %d flags%s, now %s", g_size, g_size, flags,
          g_frame.px ? " and a frame" : "", code);
}

void launcher_lang_install(const char *res_dir, int scale_percent, const WCHAR *font)
{
    char ini[MAX_PATH], code[8];
    snprintf(ini, sizeof ini, "%sron_camera.ini", g_dir);
    if (!GetPrivateProfileIntA("ui", "language_button", 1, ini)) {
        logf_("launcher: language button off");
        return;
    }
    snprintf(g_res, sizeof g_res, "%s", res_dir);
    g_k = scale_percent / 100.0f;
    wcsncpy(g_face, font, 63);
    g_n = lang_count();
    char app[MAX_PATH];
    DWORD len = GetEnvironmentVariableA("APPDATA", app, sizeof app);
    if (len && len < sizeof app)
        snprintf(g_prefs, sizeof g_prefs, "%s\\Microsoft Games\\Rise of Nations\\rise2.ini", app);
    g_cur = read_language();
    g_a.shown = g_cur;
    g_a.picked = g_a.hover = -1;
    read_captions();
    HMODULE exe = GetModuleHandleA(NULL);
    r_gpps_w = launcher_patch_import(exe, "GetPrivateProfileStringW", (void *)h_gpps_w);
    r_gpps_a = launcher_patch_import(exe, "GetPrivateProfileStringA", (void *)h_gpps_a);
    r_draw_string = launcher_patch_import(exe, "GdipDrawString", (void *)h_draw_string);
    WideCharToMultiByte(CP_ACP, 0, lang_short(g_cur), -1, code, sizeof code, NULL, NULL);
    logf_("launcher: language %s, captions from locale\\ (%d %d %d)", code, !!r_gpps_w, !!r_gpps_a, !!r_draw_string);
}
