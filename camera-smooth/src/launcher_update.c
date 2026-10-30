/*
 * Updates from the launcher ([update] in ron_camera.ini). When the launcher starts, a thread
 * reads RON_Revived_update.txt from the latest GitHub release (a few hundred bytes; the
 * releases/latest/download/ address has no API rate limit) and compares its version with
 * RON_Revived_version.txt in the game folder. When it is newer, a plate fades in over the
 * bottom of the launcher's picture: "RON Revived <version> is out", how much there is to
 * download, and an Update button in the theme's BUTTONU.bmp.
 *
 * The pack comes in parts (package\build_package.py): core, art and music, each a zip of its
 * own with a hash of its contents in the index. RON_Revived_parts.txt in the game folder holds
 * the hashes of the parts installed, so only the parts that changed are counted (and
 * downloaded).
 *
 * Update asks first, then starts RON_Revived_update.ps1 from the game folder in its own console
 * window and closes the launcher: the script waits for it to exit (winmm.dll cannot be replaced
 * while it is loaded), downloads the changed parts, checks their SHA-256, installs them with
 * RON_Revived_install.ps1 and starts the game again through Steam.
 *
 * The plate's texts are in launcher_res\locale\<code>.ini (launcher.json in translations\).
 */
#include "ron.h"
#include <winhttp.h>
#include <shellapi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define DEFAULT_INDEX "https://github.com/anastasescucristian93-max/RON-Revived/releases/latest/download/RON_Revived_update.txt"
#define RELEASES_PAGE L"https://github.com/anastasescucristian93-max/RON-Revived/releases/latest"
#define WM_FOUND     (WM_APP + 1)
#define ANIM_TIMER   1
#define FADE_MS      450.0f
#define PULSE_MS     2400.0f
#define PI_F         3.14159265f
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

typedef struct {
    unsigned char *cov; /* text coverage */
    int w, h;
} label_t;

typedef struct {
    DWORD *px; /* opaque BGRA; the corner colour's pixels get alpha 0 */
    int w, h;
} plate_t;

static char g_index[512];
static char g_res[MAX_PATH]; /* <game>\mod_override\launcher_res\ */
static float g_k = 1.0f;
static WCHAR g_face[64];

/* what the check found: set once by the thread, then g_found */
static char g_new[32];
static double g_bytes;
static volatile LONG g_found;

static HWND g_dlg;
static HWND volatile g_win;
static WNDPROC g_dlg_proc;
static int g_dlg_unicode;
static int g_w, g_h; /* the plate's window */
static DWORD *g_px;
static HDC g_dc;
static plate_t g_btn[2]; /* BUTTONU, BUTTOND */
static RECT g_btn_at;    /* the button in the window */
static label_t g_line1, g_line2, g_caption;

static struct {
    DWORD start;     /* when it began to show */
    int shown, running, hover, down, tracking;
    float hv;        /* hover light, eased */
    DWORD last;
} g_a;

/* --- the check ---------------------------------------------------------------------------- */

/* `url` into `out` (NUL-terminated); the bytes read, or -1. */
static int http_get(const char *url, char *out, int size)
{
    WCHAR wurl[512], host[256], path[1024];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 512);
    URL_COMPONENTS uc = {sizeof uc};
    uc.lpszHostName = host, uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path, uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc))
        return -1;
    HINTERNET s = WinHttpOpen(L"RON-Revived-launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) /* before Windows 8.1 */
        s = WinHttpOpen(L"RON-Revived-launcher", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s)
        return -1;
    WinHttpSetTimeouts(s, 5000, 5000, 5000, 10000);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : NULL;
    DWORD status = 0, len = sizeof status;
    int got = -1;
    if (r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(r, NULL) &&
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &len, WINHTTP_NO_HEADER_INDEX) &&
        status == 200) {
        DWORD n = 0;
        got = 0;
        while (got < size - 1 && WinHttpReadData(r, out + got, (DWORD)(size - 1 - got), &n) && n)
            got += (int)n;
        out[got] = 0;
    } else
        logf_("update: %s answered %lu (error %lu)", url, status, GetLastError());
    if (r)
        WinHttpCloseHandle(r);
    if (c)
        WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return got;
}

/* <0, 0, >0 as version a is older, the same, newer than b ("2026.10.28.2" style) */
static int version_cmp(const char *a, const char *b)
{
    while (*a || *b) {
        char *ea, *eb;
        long x = strtol(a, &ea, 10), y = strtol(b, &eb, 10);
        if (x != y)
            return x < y ? -1 : 1;
        if (ea == a && eb == b) /* not a number on either side */
            return strcmp(a, b);
        a = *ea == '.' ? ea + 1 : ea;
        b = *eb == '.' ? eb + 1 : eb;
    }
    return 0;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && (unsigned char)s[n - 1] <= ' ')
        s[--n] = 0;
}

static DWORD WINAPI check_thread(void *arg)
{
    (void)arg;
    char mine[32] = "", path[MAX_PATH];
    snprintf(path, sizeof path, "%sRON_Revived_version.txt", g_dir);
    FILE *f = fopen(path, "r");
    if (f) {
        if (!fgets(mine, sizeof mine, f))
            mine[0] = 0;
        fclose(f);
    }
    trim(mine);
    if (!mine[0]) {
        logf_("update: no RON_Revived_version.txt, not checked");
        return 0;
    }
    static char buf[16384];
    if (http_get(g_index, buf, sizeof buf) <= 0)
        return 0;

    char version[32] = "", parts[MAX_PATH];
    double bytes = 0;
    int changed = 0, listed = 0;
    snprintf(parts, sizeof parts, "%sRON_Revived_parts.txt", g_dir);
    for (char *line = strtok(buf, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        char key[32], content[80], have[80];
        double size;
        if (sscanf(line, "version=%31s", version) == 1)
            continue;
        /* <part>=<content hash> <zip sha256> <bytes> <zip name> */
        if (sscanf(line, "%31[a-z]=%79s %*s %lf", key, content, &size) != 3)
            continue;
        listed++;
        GetPrivateProfileStringA("parts", key, "", have, sizeof have, parts);
        if (_stricmp(have, content) != 0)
            bytes += size, changed++;
    }
    if (!version[0] || !listed) {
        logf_("update: %s is not an update list", g_index);
        return 0;
    }
    if (version_cmp(version, mine) <= 0) {
        logf_("update: %s is the latest", mine);
        return 0;
    }
    snprintf(g_new, sizeof g_new, "%s", version);
    g_bytes = bytes;
    logf_("update: %s is out (this is %s), %d of %d parts changed, %.1f MB", version, mine, changed, listed,
          bytes / 1048576.0);
    InterlockedExchange(&g_found, 1);
    HWND w = g_win;
    if (w)
        PostMessageW(w, WM_FOUND, 0, 0);
    return 0;
}

/* --- the plate ---------------------------------------------------------------------------- */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static float smooth(float v)
{
    v = clampf(v, 0.0f, 1.0f);
    return v * v * (3.0f - 2.0f * v);
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

/* launcher_res\<name> (a 24-bit bmp) as opaque BGRA; the pixels of its corner colour, the
   panel's, that touch its edge become clear, so the rounded plate shows through there */
static void load_plate(const char *name, plate_t *out)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s%s", g_res, name);
    HBITMAP b = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    BITMAP bm;
    if (!b || !GetObjectA(b, sizeof bm, &bm)) {
        if (b)
            DeleteObject(b);
        return;
    }
    int w = bm.bmWidth, h = bm.bmHeight;
    DWORD *px = malloc((size_t)w * h * 4);
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
    HDC dc = GetDC(NULL);
    int ok = px && GetDIBits(dc, b, 0, (UINT)h, px, &bi, DIB_RGB_COLORS) == h;
    ReleaseDC(NULL, dc);
    DeleteObject(b);
    if (!ok) {
        free(px);
        return;
    }
    DWORD key = px[0] & 0xFFFFFF;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            DWORD *p = &px[(size_t)y * w + x];
            int edge = x < 6 || y < 6 || x >= w - 6 || y >= h - 6, same = 1;
            for (int s = 0; s < 24; s += 8)
                same &= abs((int)((*p >> s) & 255) - (int)((key >> s) & 255)) <= 4;
            *p = edge && same ? 0 : (*p & 0xFFFFFF) | 0xFF000000u;
        }
    out->px = px, out->w = w, out->h = h;
}

/* `s` in the launcher's font, `px` pixels high, no wider than `max_w` (made smaller to fit) */
static void make_label(label_t *lb, const WCHAR *s, float px, int weight, int max_w)
{
    free(lb->cov);
    lb->cov = NULL, lb->w = lb->h = 0;
    if (!s[0])
        return;
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc)
        return;
    HFONT font = NULL;
    HGDIOBJ first = NULL;
    RECT calc;
    for (int tries = 0; tries < 8; tries++, px *= 0.92f) {
        HFONT f = CreateFontW(-(int)(px + 0.5f), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY,
                              0, g_face[0] ? g_face : L"Segoe UI");
        HGDIOBJ was = SelectObject(dc, f);
        if (font)
            DeleteObject(font);
        else
            first = was;
        font = f;
        SetRect(&calc, 0, 0, 0, 0);
        DrawTextW(dc, s, -1, &calc, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
        if (calc.right <= max_w)
            break;
    }
    int w = calc.right + 2, h = calc.bottom + 2;
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
    void *bits = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dib) {
        HGDIOBJ old = SelectObject(dc, dib);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        RECT r = {1, 1, w - 1, h - 1};
        DrawTextW(dc, s, -1, &r, DT_SINGLELINE | DT_NOPREFIX);
        GdiFlush();
        lb->cov = malloc((size_t)w * h);
        if (lb->cov) {
            for (int p = 0; p < w * h; p++)
                lb->cov[p] = (unsigned char)((((DWORD *)bits)[p] >> 8) & 255);
            lb->w = w, lb->h = h;
        }
        SelectObject(dc, old);
        DeleteObject(dib);
    }
    SelectObject(dc, first);
    DeleteObject(font);
    DeleteDC(dc);
}

static void draw_label(const label_t *lb, int x0, int y0, float r, float g, float b, float alpha)
{
    for (int y = 0; y < lb->h; y++)
        for (int x = 0; x < lb->w; x++) {
            int gx = x0 + x, gy = y0 + y;
            float a = lb->cov[(size_t)y * lb->w + x] * alpha;
            if (a < 0.5f || gx < 0 || gy < 0 || gx >= g_w || gy >= g_h)
                continue;
            over(g_px + (size_t)gy * g_w + gx, a * b / 255.0f, a * g / 255.0f, a * r / 255.0f, a);
        }
}

/* the dark plate with rounded corners and a thin amber edge that breathes with `pulse` */
static void draw_back(float alpha, float pulse)
{
    float rad = 7.0f * g_k, hw = g_w / 2.0f, hh = g_h / 2.0f;
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            /* distance outside the rounded rectangle (negative inside) */
            float qx = fabsf((float)x + 0.5f - hw) - (hw - rad), qy = fabsf((float)y + 0.5f - hh) - (hh - rad);
            float d = hypotf(fmaxf(qx, 0.0f), fmaxf(qy, 0.0f)) + fminf(fmaxf(qx, qy), 0.0f) - rad;
            float cov = clampf(0.5f - d, 0.0f, 1.0f);
            if (cov <= 0.0f)
                continue;
            float a = 232.0f * cov * alpha;
            DWORD *p = g_px + (size_t)y * g_w + x;
            over(p, 42.0f * a / 255.0f, 32.0f * a / 255.0f, 26.0f * a / 255.0f, a);
            float edge = clampf(1.0f - fabsf(d + 1.0f), 0.0f, 1.0f) * cov;
            if (edge > 0.0f) {
                float e = 255.0f * edge * alpha * (0.45f + 0.35f * pulse);
                over(p, 60.0f * e / 255.0f, 150.0f * e / 255.0f, 220.0f * e / 255.0f, e);
            }
        }
}

static void draw_button(float alpha, float light)
{
    const plate_t *pl = &g_btn[g_a.down && g_a.hover && g_btn[1].px ? 1 : 0];
    int x0 = g_btn_at.left, y0 = g_btn_at.top;
    if (pl->px)
        for (int y = 0; y < pl->h; y++)
            for (int x = 0; x < pl->w; x++) {
                DWORD p = pl->px[(size_t)y * pl->w + x];
                int gx = x0 + x, gy = y0 + y;
                if (!(p >> 24) || gx >= g_w || gy >= g_h)
                    continue;
                float a = 255.0f * alpha;
                over(g_px + (size_t)gy * g_w + gx, fminf((float)(p & 255) * light, 255.0f) * alpha,
                     fminf((float)((p >> 8) & 255) * light, 255.0f) * alpha,
                     fminf((float)((p >> 16) & 255) * light, 255.0f) * alpha, a);
            }
    int bw = g_btn_at.right - g_btn_at.left, bh = g_btn_at.bottom - g_btn_at.top, push = g_a.down && g_a.hover;
    draw_label(&g_caption, x0 + (bw - g_caption.w) / 2 + push, y0 + (bh - g_caption.h) / 2 + push, 240, 228, 200,
               alpha);
}

static void render(void)
{
    if (!g_win)
        return;
    float t = smooth((float)(GetTickCount() - g_a.start) / FADE_MS);
    float pulse = 0.5f - 0.5f * cosf((float)(GetTickCount() - g_a.start) / PULSE_MS * 2.0f * PI_F);
    memset(g_px, 0, (size_t)g_w * g_h * 4);
    draw_back(t, pulse);
    int left = (int)(16.0f * g_k), gap = (int)(3.0f * g_k);
    int top = (g_h - g_line1.h - gap - g_line2.h) / 2;
    if (g_line1.cov)
        draw_label(&g_line1, left, top, 240, 228, 200, t);
    if (g_line2.cov)
        draw_label(&g_line2, left, top + g_line1.h + gap, 170, 175, 182, t);
    draw_button(t, 1.0f + 0.05f * pulse + 0.18f * g_a.hv);

    POINT zero = {0, 0};
    SIZE sz = {g_w, g_h};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g_win, NULL, NULL, &sz, g_dc, &zero, 0, &blend, ULW_ALPHA);
}

/* `fmt` with its %s replaced by `arg` (the translations' %s, whatever printf makes of it) */
static void fill(WCHAR *out, int size, const WCHAR *fmt, const WCHAR *arg)
{
    const WCHAR *at = wcsstr(fmt, L"%s");
    if (!at)
        at = fmt + wcslen(fmt), arg = L"";
    _snwprintf(out, size, L"%.*ls%ls%ls", (int)(at - fmt), fmt, arg, *at ? at + 2 : at);
    out[size - 1] = 0;
}

static void make_labels(void)
{
    WCHAR fmt[256], s[256], ver[32], mb[32];
    int text_w = g_btn_at.left - (int)(28.0f * g_k);
    MultiByteToWideChar(CP_ACP, 0, g_new, -1, ver, 32);
    launcher_text("ron_update_out", L"RON Revived %s is out", fmt, 256);
    fill(s, 256, fmt, ver);
    make_label(&g_line1, s, 17.0f * g_k, FW_SEMIBOLD, text_w);
    double m = g_bytes / 1048576.0;
    if (m < 1.0)
        wcscpy(mb, L"< 1");
    else
        _snwprintf(mb, 32, L"%d", (int)(m + 0.5));
    launcher_text("ron_update_size", L"%s MB to download", fmt, 256);
    fill(s, 256, fmt, mb);
    make_label(&g_line2, s, 13.5f * g_k, FW_NORMAL, text_w);
    launcher_text("ron_update_button", L"Update", s, 256);
    make_label(&g_caption, s, 17.0f * g_k, FW_SEMIBOLD, g_btn_at.right - g_btn_at.left - (int)(12.0f * g_k));
}

/* --- the update ---------------------------------------------------------------------------- */

static void start_update(void)
{
    WCHAR fmt[512], ask[600], ver[32];
    MultiByteToWideChar(CP_ACP, 0, g_new, -1, ver, 32);
    launcher_text("ron_update_ask",
                  L"Download and install RON Revived %s now?\n\nThe launcher closes while it installs and opens again "
                  L"when it is done.",
                  fmt, 512);
    fill(ask, 600, fmt, ver);
    if (MessageBoxW(g_dlg, ask, L"RON Revived", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    char script[MAX_PATH];
    snprintf(script, sizeof script, "%sRON_Revived_update.ps1", g_dir);
    WCHAR cmd[2048], wscript[MAX_PATH], windex[512];
    MultiByteToWideChar(CP_ACP, 0, script, -1, wscript, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, g_index, -1, windex, 512);
    _snwprintf(cmd, 2048,
               L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%ls\" -Index \"%ls\" -WaitPid %lu", wscript,
               windex, GetCurrentProcessId());
    cmd[2047] = 0;
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (GetFileAttributesA(script) == INVALID_FILE_ATTRIBUTES ||
        !CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        logf_("update: could not start %s (error %lu), the releases page instead", script, GetLastError());
        ShellExecuteW(NULL, L"open", RELEASES_PAGE, NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    logf_("update: %s started, the launcher closes", script);
    ExitProcess(0); /* the script waits for this process to go before it replaces winmm.dll */
}

/* --- the window --------------------------------------------------------------------------- */

static void animate(void)
{
    if (!g_a.running) {
        g_a.running = 1;
        g_a.last = GetTickCount();
        SetTimer(g_win, ANIM_TIMER, 30, NULL);
    }
}

static void tick(void)
{
    DWORD now = GetTickCount();
    float k = clampf((float)(now - g_a.last) / 90.0f, 0.0f, 1.0f), want = g_a.hover ? 1.0f : 0.0f;
    g_a.last = now;
    g_a.hv += (want - g_a.hv) * k;
    if (fabsf(want - g_a.hv) < 0.01f)
        g_a.hv = want;
    render(); /* the edge breathes as long as it is there */
}

static int on_button(int x, int y)
{
    POINT p = {x, y};
    return PtInRect(&g_btn_at, p);
}

/* over the bottom of the launcher's picture, in the middle */
static void place(void)
{
    RECT d, b, pic;
    HWND bg = GetWindow(g_dlg, GW_OWNER);
    if (!g_win || !GetWindowRect(g_dlg, &d))
        return;
    int x, y;
    if (bg && GetWindowRect(bg, &b) && launcher_picture_rect(&pic)) {
        /* the controls' dialog is centred on the background window */
        int bx = (d.left + d.right) / 2 - (b.right - b.left) / 2, by = (d.top + d.bottom) / 2 - (b.bottom - b.top) / 2;
        x = bx + (pic.left + pic.right) / 2 - g_w / 2;
        y = by + pic.bottom - g_h - (int)(30.0f * g_k);
    } else {
        x = (d.left + d.right) / 2 - g_w / 2;
        y = d.bottom - g_h - (int)(30.0f * g_k);
    }
    SetWindowPos(g_win, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void show(void)
{
    if (g_a.shown || !g_win)
        return;
    g_a.shown = 1;
    g_a.start = GetTickCount();
    make_labels();
    place();
    render();
    if (IsWindowVisible(g_dlg))
        ShowWindow(g_win, SW_SHOWNOACTIVATE);
    animate();
}

static LRESULT CALLBACK win_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    switch (msg) {
    case WM_FOUND:
        show();
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, g_a.hover ? IDC_HAND : IDC_ARROW));
        return TRUE;
    case WM_MOUSEMOVE: {
        int was = g_a.hover;
        g_a.hover = on_button(x, y);
        if (!g_a.tracking) {
            TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, h, 0};
            g_a.tracking = TrackMouseEvent(&tme);
        }
        if (g_a.hover && !was)
            launcher_sound(0);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_a.tracking = 0;
        if (GetCapture() != h)
            g_a.hover = 0;
        return 0;
    case WM_LBUTTONDOWN:
        if (on_button(x, y)) {
            g_a.down = 1;
            SetCapture(h);
            launcher_sound(1);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_a.down) {
            g_a.down = 0;
            ReleaseCapture();
            render();
            if (on_button(x, y))
                start_update();
        }
        return 0;
    case WM_CAPTURECHANGED:
        g_a.down = 0;
        return 0;
    case WM_TIMER:
        if (wp == ANIM_TIMER)
            tick();
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static LRESULT CALLBACK dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT r = g_dlg_unicode ? CallWindowProcW(g_dlg_proc, h, msg, wp, lp) : CallWindowProcA(g_dlg_proc, h, msg, wp, lp);
    if (!g_win || !g_a.shown)
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

void launcher_update_attach(HWND dlg)
{
    if (!g_index[0] || g_dlg || !dlg || !GetDlgItem(dlg, 1017 /* Workshop: the controls' dialog */))
        return;
    g_dlg = dlg; /* once */
    load_plate("BUTTONU.bmp", &g_btn[0]);
    load_plate("BUTTOND.bmp", &g_btn[1]);
    int bw = g_btn[0].px ? g_btn[0].w : (int)(150.0f * g_k), bh = g_btn[0].px ? g_btn[0].h : (int)(40.0f * g_k);
    RECT pic;
    g_w = launcher_picture_rect(&pic) ? pic.right - pic.left - (int)(44.0f * g_k) : (int)(440.0f * g_k);
    g_h = bh + (int)(18.0f * g_k);
    int pad = (g_h - bh) / 2;
    SetRect(&g_btn_at, g_w - pad - bw, pad, g_w - pad, pad + bh);

    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = win_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"RonRevivedUpdate";
    RegisterClassW(&wc);
    HWND win = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0,
                               0, g_w, g_h, dlg, NULL, inst, NULL);
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), g_w, -g_h, 1, 32, BI_RGB}};
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    g_dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!win || !dib || !g_dc) {
        logf_("update: plate not made");
        if (win)
            DestroyWindow(win);
        return;
    }
    SelectObject(g_dc, dib);
    g_px = bits;
    g_dlg_unicode = IsWindowUnicode(dlg);
    g_dlg_proc = (WNDPROC)(g_dlg_unicode ? SetWindowLongPtrW(dlg, GWLP_WNDPROC, (LONG_PTR)dlg_proc)
                                         : SetWindowLongPtrA(dlg, GWLP_WNDPROC, (LONG_PTR)dlg_proc));
    g_win = win;
    if (InterlockedCompareExchange(&g_found, 0, 0)) /* the check was quicker than the window */
        show();
    logf_("update: plate %dx%d ready, button %ldx%ld%s", g_w, g_h, g_btn_at.right - g_btn_at.left,
          g_btn_at.bottom - g_btn_at.top, g_btn[0].px ? "" : " (no BUTTONU.bmp)");
}

/* the language changed in the launcher: the plate's texts in the new one */
void launcher_update_relabel(void)
{
    if (g_win && g_a.shown) {
        make_labels();
        render();
    }
}

void launcher_update_install(const char *res_dir, int scale_percent, const WCHAR *font)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%sron_camera.ini", g_dir);
    if (!GetPrivateProfileIntA("update", "check", 1, ini)) {
        logf_("update: check off");
        return;
    }
    GetPrivateProfileStringA("update", "url", DEFAULT_INDEX, g_index, sizeof g_index, ini);
    if (!g_index[0])
        snprintf(g_index, sizeof g_index, "%s", DEFAULT_INDEX);
    snprintf(g_res, sizeof g_res, "%s", res_dir);
    g_k = scale_percent / 100.0f;
    wcsncpy(g_face, font, 63);
    HANDLE t = CreateThread(NULL, 0, check_thread, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    logf_("update: checking %s", g_index);
}
