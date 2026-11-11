/*
 * The launcher's Extras tray ([extras] in ron_camera.ini): optional downloads, such as the
 * nation music, that are not in the pack's zip, so a player takes only what they want.
 *
 * On the launcher's left edge, beside the button panel, sits the front of a tray, like a CD
 * drive's: "EXTRAS" up its face, an eject mark, a light, how many extras are installed of how
 * many there are ("3/7") and a red badge with the news (extras not seen yet and installed ones
 * with a newer version out). A click ejects the tray from behind the launcher (it unlatches,
 * slides out to the left and settles); another click takes it back in. The tray is drawn where
 * the launcher's background is clear only, so it comes out from behind its edge. When there is
 * no room on the screen at the left, the launcher moves right first.
 *
 * Each extra is a row: its name, what it is, and a button that tells its state: Download (dim,
 * not installed), Installed (lit; Remove when the mouse is on it), Update (amber, a newer
 * version is out), the download's progress (Cancel when the mouse is on it), Installing...,
 * Retry (it failed).
 *
 * What there is comes from the same RON_Revived_update.txt the updater reads (package\
 * build_package.py writes it): "extras=<id>,<id>", per extra "name.<id>=" and "desc.<id>=" (in
 * English, for extras newer than this launcher's translations) and "<id>=<content hash>" then,
 * per zip, "<sha256> <bytes> <zip name>" (an extra bigger than GitHub's 2 GB per file comes in
 * several). RON_Revived_parts.txt [parts] says which are installed (their content hash), the
 * same file the updater keeps; update.ps1 updates installed extras with the pack and leaves the
 * others out. RON_Revived_extras.ini [seen] remembers which extras the player has seen.
 *
 * Download fetches the zips into %TEMP%\RON_Revived_extras\ here (WinHTTP, progress on the
 * button, SHA-256 checked), then runs RON_Revived_extras.ps1 hidden, which unpacks them and
 * installs them with RON_Revived_install.ps1 (backups and RON_Revived_files.txt as for the pack).
 * Remove runs it too. The texts are launcher_res\locale\<code>.ini ron_extras_* and
 * ron_extra_<id>, ron_extra_<id>_desc (translations\launcher.json).
 */
#include "ron.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define MAXX       12
#define MAXCH      8
#define WM_INDEX   (WM_APP + 21)
#define WM_OPDONE  (WM_APP + 22)
#define WM_OPSTEP  (WM_APP + 23)
#define ANIM_TIMER 1
#define OPEN_MS    760.0f
#define CLOSE_MS   540.0f
#define LATCH      0.12f /* of the opening: the tray first gives a little inwards, unlatching */
#define PI_F       3.14159265f
/* sizes at the theme's 100 %, in pixels */
#define TAB_W  26.0f  /* the tray's front */
#define TRAVEL 340.0f /* how far it comes out */
#define INSET  22.0f  /* how much of it stays behind the launcher when it is out */
#define MARGIN 12.0f  /* round it in the window: the badge, the shadow */
#define HEAD_H 66.0f
#define ROW_H  66.0f
#define FOOT_H 14.0f
#define FRAME  7.0f
#define BTN_W  104.0f
#define BTN_H  28.0f

typedef struct {
    unsigned char *cov; /* text coverage */
    int w, h;
} label_t;

typedef struct {
    char sha[72];
    double bytes;
    char zip[160];
} chunk_t;

typedef struct {
    char id[32];
    WCHAR name[96], desc[200]; /* the index's English */
    char content[80];
    chunk_t ch[MAXCH];
    int nch;
    double bytes;
    char have[80]; /* the installed content hash, "" when not installed */
    int seen, fresh, failed;
} extra_t;

enum { ST_OFF, ST_ON, ST_UPDATE, ST_GET, ST_INSTALL, ST_REMOVE, ST_FAILED };
enum { OP_NONE, OP_GET, OP_REMOVE };

static char g_index[512];
static char g_res[MAX_PATH];
static float g_k = 1.0f;
static WCHAR g_face[64];

/* the catalogue: written once by the index thread, then g_state */
static extra_t g_x[MAXX];
static int g_nx;
static char g_base[512];
static volatile LONG g_state; /* 0 looking, 1 found, 2 failed */

/* the download or removal running, one at a time */
static struct {
    volatile LONG kind, installing, cancel;
    int which, ok;
    volatile LONG done_kb;
    HANDLE thread;
} g_op;

/* the background window's shape and where the tray goes */
static unsigned char *g_alpha;
static int g_bgw, g_bgh, g_edge, g_top, g_bottom;

static HWND g_dlg;
static HWND volatile g_win;
static WNDPROC g_dlg_proc;
static int g_dlg_unicode;
static int g_w, g_h, g_wx, g_wy, g_tray_h; /* the window, its place in background pixels */
static DWORD *g_px;
static unsigned char *g_clip;
static HDC g_dc;
static HBITMAP g_dib;

static struct {
    int open, moving;
    float from;
    DWORD start;
    int hover, tracking, running;
    float hv[MAXX + 1]; /* hover light, eased; [MAXX] the tray's front */
    DWORD last;
} g_m;

static int K(float v)
{
    return (int)(v * g_k + 0.5f);
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static float smooth(float v)
{
    v = clampf(v, 0.0f, 1.0f);
    return v * v * (3.0f - 2.0f * v);
}

static float ease_out_back(float u, float c1)
{
    float c3 = c1 + 1.0f;
    u -= 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* `fmt` with its %s replaced by `arg` */
static void fill(WCHAR *out, int size, const WCHAR *fmt, const WCHAR *arg)
{
    const WCHAR *at = wcsstr(fmt, L"%s");
    if (!at)
        at = fmt + wcslen(fmt), arg = L"";
    _snwprintf(out, size, L"%.*ls%ls%ls", (int)(at - fmt), fmt, arg, *at ? at + 2 : at);
    out[size - 1] = 0;
}

static void megabytes(double bytes, WCHAR *out, int size)
{
    double m = bytes / 1048576.0;
    if (m >= 1024.0)
        _snwprintf(out, size, L"%.1f GB", m / 1024.0);
    else if (m >= 1.0)
        _snwprintf(out, size, L"%d MB", (int)(m + 0.5));
    else
        _snwprintf(out, size, L"< 1 MB");
    out[size - 1] = 0;
}

/* --- what is installed, what was seen ----------------------------------------------------- */

static void read_have(void)
{
    char parts[MAX_PATH], seen[MAX_PATH], v[8];
    snprintf(parts, sizeof parts, "%sRON_Revived_parts.txt", g_dir);
    snprintf(seen, sizeof seen, "%sRON_Revived_extras.ini", g_dir);
    for (int i = 0; i < g_nx; i++) {
        GetPrivateProfileStringA("parts", g_x[i].id, "", g_x[i].have, sizeof g_x[i].have, parts);
        GetPrivateProfileStringA("seen", g_x[i].id, "", v, sizeof v, seen);
        g_x[i].seen = v[0] == '1';
    }
}

static void mark_seen(void)
{
    char seen[MAX_PATH];
    snprintf(seen, sizeof seen, "%sRON_Revived_extras.ini", g_dir);
    for (int i = 0; i < g_nx; i++)
        if (!g_x[i].seen) {
            g_x[i].seen = 1;
            g_x[i].fresh = 1; /* "NEW" on it until the tray goes in */
            WritePrivateProfileStringA("seen", g_x[i].id, "1", seen);
        }
}

static int state_of(int i)
{
    if (g_op.kind != OP_NONE && g_op.which == i)
        return g_op.kind == OP_REMOVE ? ST_REMOVE : g_op.installing ? ST_INSTALL : ST_GET;
    if (g_x[i].failed)
        return ST_FAILED;
    if (!g_x[i].have[0])
        return ST_OFF;
    return _stricmp(g_x[i].have, g_x[i].content) ? ST_UPDATE : ST_ON;
}

static int count_installed(void)
{
    int n = 0;
    for (int i = 0; i < g_nx; i++)
        n += g_x[i].have[0] != 0;
    return n;
}

/* the badge: extras not seen yet and installed ones with a newer version out */
static int count_news(void)
{
    int n = 0;
    for (int i = 0; i < g_nx; i++)
        n += !g_x[i].seen || (g_x[i].have[0] && _stricmp(g_x[i].have, g_x[i].content));
    return n;
}

/* --- the catalogue ------------------------------------------------------------------------ */

static int find_extra(const char *id, int len)
{
    for (int i = 0; i < g_nx; i++)
        if ((int)strlen(g_x[i].id) == len && !_strnicmp(g_x[i].id, id, len))
            return i;
    return -1;
}

static void parse_index(char *buf)
{
    /* the list of extras first, then their lines */
    for (char *line = buf; line && *line;) {
        char *next = strchr(line, '\n');
        if (!strncmp(line, "extras=", 7)) {
            for (char *p = line + 7; *p && *p != '\n' && *p != '\r' && g_nx < MAXX;) {
                size_t n = strcspn(p, ",\r\n");
                if (n && n < sizeof g_x[0].id) {
                    memcpy(g_x[g_nx].id, p, n);
                    g_x[g_nx].id[n] = 0;
                    g_nx++;
                }
                p += n;
                if (*p == ',')
                    p++;
            }
        } else if (!strncmp(line, "base=", 5)) {
            size_t n = strcspn(line + 5, "\r\n");
            if (n < sizeof g_base) {
                memcpy(g_base, line + 5, n);
                g_base[n] = 0;
            }
        }
        line = next ? next + 1 : NULL;
    }
    for (char *line = buf; line && *line;) {
        char *next = strchr(line, '\n');
        size_t len = strcspn(line, "\r\n");
        char text[1024];
        if (len >= sizeof text)
            len = sizeof text - 1;
        memcpy(text, line, len);
        text[len] = 0;
        char *eq = strchr(text, '=');
        line = next ? next + 1 : NULL;
        if (!eq)
            continue;
        *eq = 0;
        const char *val = eq + 1;
        int i;
        if (!strncmp(text, "name.", 5) && (i = find_extra(text + 5, (int)strlen(text + 5))) >= 0)
            MultiByteToWideChar(CP_UTF8, 0, val, -1, g_x[i].name, 96);
        else if (!strncmp(text, "desc.", 5) && (i = find_extra(text + 5, (int)strlen(text + 5))) >= 0)
            MultiByteToWideChar(CP_UTF8, 0, val, -1, g_x[i].desc, 200);
        else if ((i = find_extra(text, (int)strlen(text))) >= 0) {
            /* <content> then <sha256> <bytes> <zip> per zip */
            int at = 0, n = 0;
            if (sscanf(val, "%79s%n", g_x[i].content, &n) != 1)
                continue;
            at += n;
            g_x[i].nch = 0, g_x[i].bytes = 0;
            chunk_t *c;
            while (g_x[i].nch < MAXCH && (c = &g_x[i].ch[g_x[i].nch]) &&
                   sscanf(val + at, "%71s %lf %159s%n", c->sha, &c->bytes, c->zip, &n) == 3) {
                at += n;
                g_x[i].bytes += c->bytes;
                g_x[i].nch++;
            }
        }
    }
    /* an extra without zips is not offered */
    int k = 0;
    for (int i = 0; i < g_nx; i++)
        if (g_x[i].nch && g_x[i].content[0])
            g_x[k++] = g_x[i];
    g_nx = k;
}

static DWORD WINAPI index_thread(void *arg)
{
    (void)arg;
    static char buf[32768];
    int got = launcher_http_get(g_index, buf, sizeof buf);
    if (got > 0 && strstr(buf, "version="))
        parse_index(buf);
    logf_("extras: %s, %d extras", got > 0 ? "list read" : "list not read", g_nx);
    InterlockedExchange(&g_state, got > 0 ? 1 : 2);
    HWND w = g_win;
    if (w)
        PostMessageW(w, WM_INDEX, 0, 0);
    return 0;
}

/* --- the download ------------------------------------------------------------------------- */

/* `url` into `path`, its SHA-256 checked against `sha`; 1 when it arrived whole. `done_kb` grows
   by what arrives (it starts where the caller left it); a non-zero `*cancel` stops it. Also the
   updater's (launcher_update.c). */
int launcher_download(const char *url, const WCHAR *path, const char *sha, volatile LONG *done_kb,
                      volatile LONG *cancel)
{
    WCHAR wurl[1024], host[256], upath[1024];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1024);
    URL_COMPONENTS uc = {sizeof uc};
    uc.lpszHostName = host, uc.dwHostNameLength = 256;
    uc.lpszUrlPath = upath, uc.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc))
        return 0;
    HINTERNET s = WinHttpOpen(L"RON-Revived-launcher", 4 /* automatic proxy */, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s)
        s = WinHttpOpen(L"RON-Revived-launcher", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s)
        return 0;
    WinHttpSetTimeouts(s, 10000, 10000, 15000, 30000);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", upath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : NULL;
    DWORD status = 0, len = sizeof status;
    int ok = 0;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    HANDLE f = INVALID_HANDLE_VALUE;
    static unsigned char buf[1 << 18];
    if (r && WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(r, NULL) &&
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &len, WINHTTP_NO_HEADER_INDEX) &&
        status == 200 && BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0 &&
        BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0 &&
        (f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) !=
            INVALID_HANDLE_VALUE) {
        DWORD n = 0, wrote;
        double done = 0, base = *done_kb * 1024.0;
        ok = 1;
        while (WinHttpReadData(r, buf, sizeof buf, &n) && n) {
            if (*cancel || !WriteFile(f, buf, n, &wrote, NULL) || wrote != n) {
                ok = 0;
                break;
            }
            BCryptHashData(hash, buf, n, 0);
            done += n;
            InterlockedExchange(done_kb, (LONG)((base + done) / 1024.0));
        }
        unsigned char digest[32];
        char hex[72];
        if (ok && BCryptFinishHash(hash, digest, 32, 0) == 0) {
            for (int i = 0; i < 32; i++)
                snprintf(hex + i * 2, 3, "%02x", digest[i]);
            ok = _stricmp(hex, sha) == 0;
            if (!ok)
                logf_("download: %s arrived with SHA-256 %s, not %s", url, hex, sha);
        } else
            ok = 0;
    } else
        logf_("download: %s answered %lu (error %lu)", url, status, GetLastError());
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (hash)
        BCryptDestroyHash(hash);
    if (alg)
        BCryptCloseAlgorithmProvider(alg, 0);
    if (r)
        WinHttpCloseHandle(r);
    if (c)
        WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    if (!ok)
        DeleteFileW(path);
    return ok;
}

/* RON_Revived_extras.ps1 hidden; its exit code, or -1 */
static int run_script(const char *action, const char *id, const WCHAR *zips, const char *content)
{
    char script[MAX_PATH];
    snprintf(script, sizeof script, "%sRON_Revived_extras.ps1", g_dir);
    if (GetFileAttributesA(script) == INVALID_FILE_ATTRIBUTES) {
        logf_("extras: no %s", script);
        return -1;
    }
    static WCHAR cmd[8192];
    WCHAR wscript[MAX_PATH], waction[16], wid[32], wcontent[80];
    MultiByteToWideChar(CP_ACP, 0, script, -1, wscript, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, action, -1, waction, 16);
    MultiByteToWideChar(CP_ACP, 0, id, -1, wid, 32);
    MultiByteToWideChar(CP_ACP, 0, content[0] ? content : "none", -1, wcontent, 80); /* "" may get lost */
    if (!zips[0])
        zips = L"none";
    _snwprintf(cmd, 8192,
               L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%ls\" -Action %ls "
               L"-Part %ls -Zips \"%ls\" -Content \"%ls\"",
               wscript, waction, wid, zips, wcontent);
    cmd[8191] = 0;
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        logf_("extras: could not start %s (error %lu)", script, GetLastError());
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    logf_("extras: %s %s ended with %lu", action, id, code);
    return (int)code;
}

static DWORD WINAPI op_thread(void *arg)
{
    (void)arg;
    extra_t *x = &g_x[g_op.which];
    int ok = 0;
    if (g_op.kind == OP_GET) {
        WCHAR dir[MAX_PATH], path[MAX_PATH], wzip[160];
        static WCHAR zips[MAXCH * MAX_PATH];
        char url[1024];
        DWORD n = GetTempPathW(MAX_PATH, dir);
        if (n && n < MAX_PATH - 24) {
            wcscat(dir, L"RON_Revived_extras\\");
            CreateDirectoryW(dir, NULL);
            zips[0] = 0;
            ok = 1;
            for (int i = 0; ok && i < x->nch; i++) {
                snprintf(url, sizeof url, "%s%s", g_base, x->ch[i].zip);
                MultiByteToWideChar(CP_UTF8, 0, x->ch[i].zip, -1, wzip, 160);
                _snwprintf(path, MAX_PATH, L"%ls%ls", dir, wzip);
                path[MAX_PATH - 1] = 0;
                ok = launcher_download(url, path, x->ch[i].sha, &g_op.done_kb, &g_op.cancel);
                if (i)
                    wcscat(zips, L";");
                wcscat(zips, path);
            }
            if (ok && !g_op.cancel) {
                InterlockedExchange(&g_op.installing, 1);
                HWND w = g_win;
                if (w)
                    PostMessageW(w, WM_OPSTEP, 0, 0);
                ok = run_script("install", x->id, zips, x->content) == 0;
            }
            /* the script removes the zips it installed; these are what is left of a failure */
            for (WCHAR *p = zips, *e; *p; p = *e ? e + 1 : e) {
                e = wcschr(p, L';');
                if (!e)
                    e = p + wcslen(p);
                WCHAR one[MAX_PATH];
                _snwprintf(one, MAX_PATH, L"%.*ls", (int)(e - p), p);
                one[MAX_PATH - 1] = 0;
                DeleteFileW(one);
            }
        }
    } else
        ok = run_script("remove", x->id, L"", "") == 0;
    g_op.ok = ok && !g_op.cancel;
    HWND w = g_win;
    if (w)
        PostMessageW(w, WM_OPDONE, 0, 0);
    return 0;
}

static void start_op(int i, int kind)
{
    if (g_op.kind != OP_NONE)
        return;
    g_op.which = i;
    g_op.cancel = 0, g_op.installing = 0, g_op.done_kb = 0, g_op.ok = 0;
    g_x[i].failed = 0;
    InterlockedExchange(&g_op.kind, kind);
    g_op.thread = CreateThread(NULL, 0, op_thread, NULL, 0, NULL);
    if (!g_op.thread) {
        g_op.kind = OP_NONE;
        g_x[i].failed = 1;
    }
    logf_("extras: %s %s", kind == OP_GET ? "download" : "remove", g_x[i].id);
}

static void op_done(void)
{
    if (g_op.thread) {
        WaitForSingleObject(g_op.thread, 2000);
        CloseHandle(g_op.thread);
        g_op.thread = NULL;
    }
    int i = g_op.which, cancelled = g_op.cancel;
    read_have();
    g_x[i].failed = !g_op.ok && !cancelled;
    g_op.kind = OP_NONE;
    logf_("extras: %s %s", g_x[i].id, g_op.ok ? "done" : cancelled ? "cancelled" : "FAILED");
}

/* --- drawing ------------------------------------------------------------------------------ */

static void over(DWORD *d, float b, float g, float r, float a)
{
    float k = 1.0f - a / 255.0f;
    DWORD p = *d;
    float db = (float)(p & 255) * k + b, dg = (float)((p >> 8) & 255) * k + g;
    float dr = (float)((p >> 16) & 255) * k + r, da = (float)(p >> 24) * k + a;
    *d = (DWORD)clampf(db + 0.5f, 0, 255) | (DWORD)clampf(dg + 0.5f, 0, 255) << 8 |
         (DWORD)clampf(dr + 0.5f, 0, 255) << 16 | (DWORD)clampf(da + 0.5f, 0, 255) << 24;
}

/* colour r, g, b (0..255) at coverage a (0..1) over the pixel */
static void put(int x, int y, float r, float g, float b, float a)
{
    if (a <= 0.002f || x < 0 || y < 0 || x >= g_w || y >= g_h)
        return;
    a = fminf(a, 1.0f);
    over(g_px + (size_t)y * g_w + x, b * a, g * a, r * a, a * 255.0f);
}

static float hash2(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

/* distance from the rounded rectangle's edge, negative inside */
static float rr_dist(float px, float py, float x0, float y0, float x1, float y1, float r)
{
    float hw = (x1 - x0) / 2.0f, hh = (y1 - y0) / 2.0f;
    float qx = fabsf(px - (x0 + hw)) - (hw - r), qy = fabsf(py - (y0 + hh)) - (hh - r);
    return hypotf(fmaxf(qx, 0.0f), fmaxf(qy, 0.0f)) + fminf(fmaxf(qx, qy), 0.0f) - r;
}

typedef struct {
    float r0, g0, b0, r1, g1, b1; /* from the top (or the left) to the bottom (or the right) */
    int across;                   /* the gradient runs left to right */
    float grain, streak;          /* noise; brushed metal */
} paint_t;

static void fill_rr(float x0, float y0, float x1, float y1, float rad, const paint_t *p, float alpha)
{
    int ix0 = (int)floorf(x0) - 1, iy0 = (int)floorf(y0) - 1, ix1 = (int)ceilf(x1) + 1, iy1 = (int)ceilf(y1) + 1;
    ix0 = ix0 < 0 ? 0 : ix0, iy0 = iy0 < 0 ? 0 : iy0, ix1 = ix1 > g_w ? g_w : ix1, iy1 = iy1 > g_h ? g_h : iy1;
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float d = rr_dist((float)x + 0.5f, (float)y + 0.5f, x0, y0, x1, y1, rad);
            float cov = clampf(0.5f - d, 0.0f, 1.0f);
            if (cov <= 0.0f)
                continue;
            float t = p->across ? ((float)x - x0) / fmaxf(x1 - x0, 1.0f) : ((float)y - y0) / fmaxf(y1 - y0, 1.0f);
            t = clampf(t, 0.0f, 1.0f);
            /* the grain and the streaks stay with the tray as it moves */
            int lx = (int)((float)x - x0);
            float n = p->grain * (hash2(lx, y) - 0.5f) * 2.0f +
                      p->streak * (hash2(y * 7 + 3, 11) - 0.5f + 0.5f * (hash2(lx / 9, y) - 0.5f));
            put(x, y, p->r0 + (p->r1 - p->r0) * t + n, p->g0 + (p->g1 - p->g0) * t + n,
                p->b0 + (p->b1 - p->b0) * t + n, cov * alpha);
        }
}

/* a line `width` wide just inside the rounded rectangle's edge */
static void stroke_rr(float x0, float y0, float x1, float y1, float rad, float width, float r, float g, float b,
                      float alpha)
{
    int ix0 = (int)floorf(x0) - 1, iy0 = (int)floorf(y0) - 1, ix1 = (int)ceilf(x1) + 1, iy1 = (int)ceilf(y1) + 1;
    ix0 = ix0 < 0 ? 0 : ix0, iy0 = iy0 < 0 ? 0 : iy0, ix1 = ix1 > g_w ? g_w : ix1, iy1 = iy1 > g_h ? g_h : iy1;
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float d = rr_dist((float)x + 0.5f, (float)y + 0.5f, x0, y0, x1, y1, rad);
            float cov = clampf(width / 2.0f + 0.5f - fabsf(d + width / 2.0f), 0.0f, 1.0f);
            put(x, y, r, g, b, cov * alpha);
        }
}

/* a soft shadow round the rounded rectangle, `spread` wide */
static void shadow_rr(float x0, float y0, float x1, float y1, float rad, float spread, float alpha)
{
    int ix0 = (int)(x0 - spread) - 1, iy0 = (int)(y0 - spread) - 1, ix1 = (int)(x1 + spread) + 2,
        iy1 = (int)(y1 + spread) + 2;
    ix0 = ix0 < 0 ? 0 : ix0, iy0 = iy0 < 0 ? 0 : iy0, ix1 = ix1 > g_w ? g_w : ix1, iy1 = iy1 > g_h ? g_h : iy1;
    for (int y = iy0; y < iy1; y++)
        for (int x = ix0; x < ix1; x++) {
            float d = rr_dist((float)x + 0.5f, (float)y + 0.5f, x0, y0, x1, y1, rad);
            if (d < -1.0f)
                continue;
            put(x, y, 0, 0, 0, alpha * (1.0f - smooth(d / spread)));
        }
}

static void disc(float cx, float cy, float rad, float r, float g, float b, float alpha)
{
    for (int y = (int)(cy - rad) - 1; y <= (int)(cy + rad) + 1; y++)
        for (int x = (int)(cx - rad) - 1; x <= (int)(cx + rad) + 1; x++) {
            float d = hypotf((float)x + 0.5f - cx, (float)y + 0.5f - cy) - rad;
            put(x, y, r, g, b, clampf(0.5f - d, 0.0f, 1.0f) * alpha);
        }
}

static void ring(float cx, float cy, float rad, float width, float r, float g, float b, float alpha)
{
    for (int y = (int)(cy - rad - width) - 1; y <= (int)(cy + rad + width) + 1; y++)
        for (int x = (int)(cx - rad - width) - 1; x <= (int)(cx + rad + width) + 1; x++) {
            float d = fabsf(hypotf((float)x + 0.5f - cx, (float)y + 0.5f - cy) - rad);
            put(x, y, r, g, b, clampf(width / 2.0f + 0.5f - d, 0.0f, 1.0f) * alpha);
        }
}

/* a glowing light: a soft halo and a bright core */
static void lamp(float cx, float cy, float rad, float r, float g, float b, float glow)
{
    for (int y = (int)(cy - rad * 3) - 1; y <= (int)(cy + rad * 3) + 1; y++)
        for (int x = (int)(cx - rad * 3) - 1; x <= (int)(cx + rad * 3) + 1; x++) {
            float d = hypotf((float)x + 0.5f - cx, (float)y + 0.5f - cy) / (rad * 3.0f);
            if (d < 1.0f)
                put(x, y, r, g, b, glow * 0.45f * (1.0f - smooth(d)));
        }
    disc(cx, cy, rad, r * 0.55f, g * 0.55f, b * 0.55f, 1.0f);
    disc(cx, cy, rad * 0.8f, r, g, b, 0.35f + 0.65f * glow);
    disc(cx - rad * 0.25f, cy - rad * 0.3f, rad * 0.3f, 255, 255, 255, 0.25f + 0.45f * glow);
}

static void rivet(float cx, float cy, float rad)
{
    disc(cx + rad * 0.2f, cy + rad * 0.35f, rad * 1.1f, 0, 0, 0, 0.45f);
    disc(cx, cy, rad, 46, 47, 50, 1.0f);
    disc(cx - rad * 0.12f, cy - rad * 0.12f, rad * 0.72f, 128, 128, 132, 1.0f);
    disc(cx - rad * 0.3f, cy - rad * 0.32f, rad * 0.3f, 200, 200, 205, 0.7f);
}

/* the theme's hazard stripes, amber and black */
static void stripes(float x0, float y0, float x1, float y1, float phase, float alpha)
{
    float period = 10.0f * g_k;
    for (int y = (int)y0; y < (int)ceilf(y1); y++)
        for (int x = (int)x0; x < (int)ceilf(x1); x++) {
            float u = fmodf((float)x + (float)y + phase + 1000.0f * period, period) / period;
            float a = clampf((u < 0.5f ? u : 1.0f - u) * period - period / 4.0f + 0.5f, 0.0f, 1.0f);
            put(x, y, 20 + 190 * a, 20 + 135 * a, 20 + 20 * a, alpha);
        }
}

/* the eject mark: a triangle over a bar */
static void eject(float cx, float cy, float size, float r, float g, float b, float alpha)
{
    float hw = size * 0.5f, top = cy - size * 0.5f, base = cy + size * 0.12f;
    for (int y = (int)(top - 1); y <= (int)(base + 1); y++)
        for (int x = (int)(cx - hw - 1); x <= (int)(cx + hw + 1); x++) {
            float cov = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) {
                    float px = (float)x + (sx + 0.5f) / 4.0f, py = (float)y + (sy + 0.5f) / 4.0f;
                    float half = hw * (py - top) / (base - top);
                    cov += py >= top && py <= base && fabsf(px - cx) <= half;
                }
            put(x, y, r, g, b, cov / 16.0f * alpha);
        }
    paint_t p = {r, g, b, r, g, b, 0, 0, 0};
    fill_rr(cx - hw, cy + size * 0.24f, cx + hw, cy + size * 0.42f, 0.5f, &p, alpha);
}

/* --- text --------------------------------------------------------------------------------- */

/* `s` in the launcher's font, `px` high, no wider than `max_w`: made smaller to fit, or with
   "..." at its end (`ellipsis` 1), or on up to two lines (`ellipsis` 2); turned to read upwards
   (`turn`) */
static void make_label(label_t *lb, const WCHAR *s, float px, int weight, int max_w, int ellipsis, int turn)
{
    lb->cov = NULL, lb->w = lb->h = 0;
    HDC dc = s[0] ? CreateCompatibleDC(NULL) : NULL;
    if (!dc)
        return;
    HFONT font = NULL;
    HGDIOBJ first = NULL;
    RECT calc;
    for (int tries = 0; tries < (ellipsis ? 1 : 8); tries++, px *= 0.92f) {
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
    int lines = 1;
    if (ellipsis == 2 && calc.right > max_w) {
        int line_h = calc.bottom;
        SetRect(&calc, 0, 0, max_w, 0);
        DrawTextW(dc, s, -1, &calc, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        lines = 2;
        if (calc.bottom > 2 * line_h)
            calc.bottom = 2 * line_h;
    }
    int w = (calc.right < max_w ? calc.right : max_w) + 2, h = calc.bottom + 2;
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB}};
    void *bits = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (dib) {
        HGDIOBJ old = SelectObject(dc, dib);
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        RECT r = {1, 1, w - 1, h - 1};
        DrawTextW(dc, s, -1, &r,
                  DT_NOPREFIX | (lines > 1 ? DT_WORDBREAK | DT_EDITCONTROL | DT_END_ELLIPSIS
                                           : DT_SINGLELINE | (ellipsis ? DT_END_ELLIPSIS : 0)));
        GdiFlush();
        lb->cov = malloc((size_t)w * h);
        if (lb->cov) {
            const DWORD *p = bits;
            if (turn) /* a quarter turn to the left: the text's start at the bottom */
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                        lb->cov[(size_t)(w - 1 - x) * h + y] = (unsigned char)((p[(size_t)y * w + x] >> 8) & 255);
            else
                for (int i = 0; i < w * h; i++)
                    lb->cov[i] = (unsigned char)((p[i] >> 8) & 255);
            lb->w = turn ? h : w, lb->h = turn ? w : h;
        }
        SelectObject(dc, old);
        DeleteObject(dib);
    }
    SelectObject(dc, first);
    DeleteObject(font);
    DeleteDC(dc);
}

/* the labels drawn lately, so a frame does not make them again */
#define CACHE 64
static struct {
    WCHAR s[128];
    int px10, weight, max_w, mode;
    label_t lb;
} g_cache[CACHE];
static int g_cache_next;

static void clear_cache(void)
{
    for (int i = 0; i < CACHE; i++) {
        free(g_cache[i].lb.cov);
        memset(&g_cache[i], 0, sizeof g_cache[i]);
    }
}

static const label_t *text(const WCHAR *s, float px, int weight, int max_w,
                           int mode /* 1 ellipsis, 2 turned, 3 two lines */)
{
    int px10 = (int)(px * 10.0f);
    for (int i = 0; i < CACHE; i++)
        if (g_cache[i].px10 == px10 && g_cache[i].weight == weight && g_cache[i].max_w == max_w &&
            g_cache[i].mode == mode && !wcscmp(g_cache[i].s, s))
            return &g_cache[i].lb;
    int i = g_cache_next;
    g_cache_next = (g_cache_next + 1) % CACHE;
    free(g_cache[i].lb.cov);
    wcsncpy(g_cache[i].s, s, 127);
    g_cache[i].s[127] = 0;
    g_cache[i].px10 = px10, g_cache[i].weight = weight, g_cache[i].max_w = max_w, g_cache[i].mode = mode;
    make_label(&g_cache[i].lb, s, px, weight, max_w, mode == 1 ? 1 : mode == 3 ? 2 : 0, mode == 2);
    return &g_cache[i].lb;
}

static void draw_label(const label_t *lb, float fx, float fy, float r, float g, float b, float alpha)
{
    int x0 = (int)floorf(fx + 0.5f), y0 = (int)floorf(fy + 0.5f);
    if (!lb->cov)
        return;
    for (int y = 0; y < lb->h; y++)
        for (int x = 0; x < lb->w; x++) {
            unsigned char c = lb->cov[(size_t)y * lb->w + x];
            if (c)
                put(x0 + x, y0 + y, r, g, b, c / 255.0f * alpha);
        }
}

static void say(const char *key, const WCHAR *def, WCHAR *out, int size)
{
    launcher_text(key, def, out, size);
}

static void extra_name(int i, WCHAR *out, int size)
{
    char key[64];
    snprintf(key, sizeof key, "ron_extra_%s", g_x[i].id);
    say(key, g_x[i].name[0] ? g_x[i].name : L"", out, size);
    if (!out[0])
        MultiByteToWideChar(CP_ACP, 0, g_x[i].id, -1, out, size);
}

static void extra_desc(int i, WCHAR *out, int size)
{
    char key[64];
    snprintf(key, sizeof key, "ron_extra_%s_desc", g_x[i].id);
    say(key, g_x[i].desc, out, size);
}

/* --- the tray ----------------------------------------------------------------------------- */

static const paint_t STEEL = {104, 108, 114, 64, 67, 72, 0, 5.0f, 5.0f};
static const paint_t FRONT = {118, 122, 128, 70, 73, 79, 1, 5.0f, 3.0f};
static const paint_t INNER = {36, 43, 56, 21, 24, 29, 0, 4.0f, 0.0f}; /* the button panel's grain */

/* 0 in .. 1 out (a little over 1 as it settles, under 0 as it unlatches) */
static float position(void)
{
    if (!g_m.moving)
        return g_m.open ? 1.0f : 0.0f;
    float dur = g_m.open ? OPEN_MS : CLOSE_MS * fmaxf(g_m.from, 0.3f);
    float u = (float)(GetTickCount() - g_m.start) / dur;
    if (u >= 1.0f) {
        g_m.moving = 0;
        return g_m.open ? 1.0f : 0.0f;
    }
    if (g_m.open) {
        if (g_m.from > 0.01f)
            return g_m.from + (1.0f - g_m.from) * ease_out_back(u, 0.6f);
        if (u < LATCH)
            return -0.012f * sinf(u / LATCH * PI_F);
        return ease_out_back((u - LATCH) / (1.0f - LATCH), 0.6f);
    }
    float v = u < 0.5f ? 4.0f * u * u * u : 1.0f - powf(-2.0f * u + 2.0f, 3.0f) / 2.0f;
    return g_m.from * (1.0f - v);
}

static float pulse(float period_ms)
{
    return 0.5f - 0.5f * cosf((float)(GetTickCount() % 100000) / period_ms * 2.0f * PI_F);
}

/* the row's button in the window: its rectangle when the tray's body starts at bx */
static void button_rect(int i, float bx, float top, float *x0, float *y0, float *x1, float *y1)
{
    float right = bx + (TRAVEL - 14.0f) * g_k, ry = top + (HEAD_H + i * ROW_H) * g_k;
    *x1 = right, *x0 = right - BTN_W * g_k;
    *y0 = ry + 9.0f * g_k, *y1 = *y0 + BTN_H * g_k;
}

static void draw_button(int i, float bx, float top)
{
    float x0, y0, x1, y1;
    button_rect(i, bx, top, &x0, &y0, &x1, &y1);
    int st = state_of(i), busy = g_op.kind != OP_NONE && g_op.which != i;
    float hv = g_m.hv[i], a = busy ? 0.45f : 1.0f, rad = 4.0f * g_k;
    WCHAR cap[64], under[64] = L"", fmt[64], mb[32], mb2[32];
    /* the plate: dim when the extra is not there, lit when it is */
    paint_t p = STEEL;
    if (st == ST_OFF || st == ST_FAILED)
        p.r0 = 70, p.g0 = 72, p.b0 = 76, p.r1 = 44, p.g1 = 46, p.b1 = 50;
    else if (st == ST_ON)
        p.r0 = 92, p.g0 = 100, p.b0 = 98, p.r1 = 54, p.g1 = 60, p.b1 = 60;
    else if (st == ST_UPDATE)
        p.r0 = 120, p.g0 = 96, p.b0 = 60, p.r1 = 70, p.g1 = 52, p.b1 = 32;
    float lift = 14.0f * hv;
    p.r0 += lift, p.g0 += lift, p.b0 += lift, p.r1 += lift, p.g1 += lift, p.b1 += lift;
    p.grain = 3.0f, p.streak = 0;
    shadow_rr(x0, y0 + 1.5f * g_k, x1, y1 + 1.5f * g_k, rad, 4.0f * g_k, 0.5f * a);
    fill_rr(x0, y0, x1, y1, rad, &p, a);
    float led_x = x0 + 11.0f * g_k, led_y = (y0 + y1) / 2.0f, led_r = 3.4f * g_k;
    if (st == ST_GET) {
        double total = g_x[i].bytes > 0 ? g_x[i].bytes : 1;
        float f = clampf((float)(g_op.done_kb * 1024.0 / total), 0.0f, 1.0f);
        paint_t bar = {226, 160, 58, 150, 92, 30, 0, 2.0f, 0};
        fill_rr(x0 + 2, y0 + 2, x0 + 2 + (x1 - x0 - 4) * f, y1 - 2, rad - 1, &bar, a * 0.85f);
        _snwprintf(cap, 64, L"%d%%", (int)(f * 100.0f));
        megabytes(g_op.done_kb * 1024.0, mb, 32);
        megabytes(g_x[i].bytes, mb2, 32);
        _snwprintf(under, 64, L"%ls / %ls", mb, mb2);
        if (hv > 0.5f)
            say("ron_extras_cancel", L"Cancel", cap, 64);
    } else if (st == ST_INSTALL || st == ST_REMOVE) {
        stripes(x0 + 2, y0 + 2, x1 - 2, y1 - 2, (float)(GetTickCount() % 100000) * 0.03f * g_k, 0.35f * a);
        if (st == ST_INSTALL)
            say("ron_extras_installing", L"Installing...", cap, 64);
        else
            say("ron_extras_removing", L"Removing...", cap, 64);
    } else {
        if (st == ST_OFF)
            say("ron_extras_download", L"Download", cap, 64);
        else if (st == ST_ON)
            say(hv > 0.5f ? "ron_extras_remove" : "ron_extras_installed", hv > 0.5f ? L"Remove" : L"Installed", cap,
                64);
        else if (st == ST_UPDATE)
            say("ron_extras_update", L"Update", cap, 64);
        else
            say("ron_extras_retry", L"Retry", cap, 64);
        if (st != ST_ON) {
            megabytes(g_x[i].bytes, under, 64);
            if (st == ST_FAILED) {
                say("ron_extras_failed", L"It did not work", fmt, 64);
                wcscpy(under, fmt);
            }
        }
        /* its light */
        if (st == ST_ON && hv > 0.5f)
            lamp(led_x, led_y, led_r, 230, 70, 50, 0.8f);
        else if (st == ST_ON)
            lamp(led_x, led_y, led_r, 90, 230, 120, 0.9f);
        else if (st == ST_UPDATE)
            lamp(led_x, led_y, led_r, 255, 176, 60, 0.35f + 0.65f * pulse(1400.0f));
        else if (st == ST_FAILED)
            lamp(led_x, led_y, led_r, 230, 60, 45, 0.7f);
        else
            lamp(led_x, led_y, led_r, 80, 82, 86, 0.0f);
    }
    float er = st == ST_UPDATE ? 255 : 150, eg = st == ST_UPDATE ? 176 : 156, eb = st == ST_UPDATE ? 60 : 164;
    stroke_rr(x0, y0, x1, y1, rad, 1.0f, er, eg, eb, (st == ST_UPDATE ? 0.45f + 0.4f * pulse(1400.0f) : 0.28f + 0.3f * hv) * a);
    stroke_rr(x0 + 1, y0 + 1, x1 - 1, y1 - 1, rad - 1, 1.0f, 255, 255, 255, 0.06f * a);
    int has_led = st != ST_GET && st != ST_INSTALL && st != ST_REMOVE;
    float cx0 = has_led ? x0 + 20.0f * g_k : x0 + 6.0f * g_k;
    const label_t *lb = text(cap, 14.5f * g_k, FW_SEMIBOLD, (int)(x1 - cx0 - 6.0f * g_k), 0);
    float bright = st == ST_OFF || st == ST_FAILED ? 205.0f : 240.0f;
    draw_label(lb, cx0 + (x1 - 6.0f * g_k - cx0 - lb->w) / 2.0f, (y0 + y1 - lb->h) / 2.0f, bright, bright * 0.95f,
               bright * 0.84f, a);
    if (under[0]) {
        lb = text(under, 11.5f * g_k, FW_NORMAL, (int)(BTN_W * g_k + 20.0f * g_k), 0);
        float ur = st == ST_FAILED ? 235 : 150, ug = st == ST_FAILED ? 110 : 155, ub = st == ST_FAILED ? 90 : 162;
        draw_label(lb, (x0 + x1 - lb->w) / 2.0f, y1 + 2.0f * g_k, ur, ug, ub, a);
    }
}

static void draw_body(float bx, float top)
{
    float x1 = bx + (TRAVEL + INSET) * g_k, y1 = top + g_tray_h, f = FRAME * g_k, rad = 6.0f * g_k;
    shadow_rr(bx - 2.0f * g_k, top + 3.0f * g_k, x1, y1 + 3.0f * g_k, rad, 10.0f * g_k, 0.55f);
    fill_rr(bx, top, x1, y1, rad, &STEEL, 1.0f);
    stroke_rr(bx, top, x1, y1, rad, 1.0f, 18, 18, 20, 0.9f);
    stroke_rr(bx + 1, top + 1, x1 - 1, y1 - 1, rad - 1, 1.0f, 190, 192, 198, 0.22f);
    /* rust at the frame's corners, as on the launcher's frames */
    for (int c = 0; c < 2; c++)
        for (int n = 0; n < 40; n++) {
            float rx = bx + (2.0f + hash2(n, c * 7) * 26.0f) * g_k;
            float ry = c ? y1 - (2.0f + hash2(n, 5) * 6.0f) * g_k : top + (2.0f + hash2(n, 9) * 6.0f) * g_k;
            disc(rx, ry, (0.6f + hash2(n, 3 + c)) * g_k, 128, 72, 40, 0.35f * hash2(n, 13));
        }
    /* the inner panel, sunk into the frame */
    float ix0 = bx + f, iy0 = top + f, ix1 = x1 - f, iy1 = y1 - f;
    fill_rr(ix0, iy0, ix1, iy1, 3.0f * g_k, &INNER, 1.0f);
    stroke_rr(ix0, iy0, ix1, iy1, 3.0f * g_k, 2.0f * g_k, 0, 0, 0, 0.45f);
    stroke_rr(ix0 - 1, iy0 - 1, ix1 + 1, iy1 + 1, 3.0f * g_k + 1, 1.0f, 200, 204, 210, 0.12f);
    /* where the disc lies on a CD tray: a shallow round recess */
    float dcx = bx + TRAVEL * g_k * 0.62f, dcy = (iy0 + iy1) / 2.0f + 8.0f * g_k;
    float dr = fminf((iy1 - iy0) * 0.62f, 150.0f * g_k);
    ring(dcx, dcy, dr, 2.0f * g_k, 0, 0, 0, 0.22f);
    ring(dcx, dcy + 1.5f * g_k, dr, 1.0f, 150, 170, 200, 0.06f);
    ring(dcx, dcy, dr * 0.18f, 1.5f * g_k, 0, 0, 0, 0.2f);
    rivet(bx + 4.0f * g_k, top + 4.0f * g_k, 2.2f * g_k);
    rivet(bx + 4.0f * g_k, y1 - 4.0f * g_k, 2.2f * g_k);
    rivet(bx + (TRAVEL - 6.0f) * g_k, top + 4.0f * g_k, 2.2f * g_k);
    rivet(bx + (TRAVEL - 6.0f) * g_k, y1 - 4.0f * g_k, 2.2f * g_k);

    /* the heading */
    WCHAR s[160], fmt[96], num[32];
    float left = bx + (FRAME + 12.0f) * g_k, right = bx + (TRAVEL - 14.0f) * g_k;
    say("ron_extras_title", L"Extras", s, 160);
    const label_t *lb = text(s, 20.0f * g_k, FW_SEMIBOLD, (int)(170.0f * g_k), 0);
    draw_label(lb, left, top + 12.0f * g_k, 240, 228, 200, 1.0f);
    if (g_state == 1 && g_nx) {
        _snwprintf(num, 32, L"%d/%d", count_installed(), g_nx);
        say("ron_extras_count", L"%s installed", fmt, 96);
        fill(s, 160, fmt, num);
        lb = text(s, 13.0f * g_k, FW_NORMAL, (int)(130.0f * g_k), 0);
        draw_label(lb, right - lb->w, top + 18.0f * g_k, 214, 170, 92, 1.0f);
    }
    say("ron_extras_sub", L"Optional downloads: take only what you want", s, 160);
    lb = text(s, 12.5f * g_k, FW_NORMAL, (int)(right - left), 1);
    draw_label(lb, left, top + 38.0f * g_k, 150, 155, 162, 1.0f);
    stripes(left, top + (HEAD_H - 7.0f) * g_k, right, top + (HEAD_H - 4.0f) * g_k, 0, 0.55f);

    if (g_state != 1 || !g_nx) {
        if (g_state == 0)
            say("ron_extras_looking", L"Looking for extras...", s, 160);
        else if (g_state == 2)
            say("ron_extras_offline", L"Could not reach GitHub. Try again later.", s, 160);
        else
            say("ron_extras_none", L"No extras yet.", s, 160);
        lb = text(s, 13.5f * g_k, FW_NORMAL, (int)(right - left), 1);
        draw_label(lb, left, top + (HEAD_H + ROW_H / 2.0f) * g_k - lb->h / 2.0f, 190, 190, 185, 1.0f);
        return;
    }
    for (int i = 0; i < g_nx; i++) {
        float ry = top + (HEAD_H + i * ROW_H) * g_k, bx0, by0, bx1, by1;
        button_rect(i, bx, top, &bx0, &by0, &bx1, &by1);
        int st = state_of(i), on = st != ST_OFF && st != ST_FAILED;
        extra_name(i, s, 160);
        lb = text(s, 16.0f * g_k, FW_SEMIBOLD, (int)(bx0 - left - 40.0f * g_k), 1);
        float nr = on ? 240 : 196, ng = on ? 228 : 192, nb = on ? 200 : 184;
        draw_label(lb, left, ry + 8.0f * g_k, nr, ng, nb, 1.0f);
        if (g_x[i].fresh) {
            say("ron_extras_new", L"NEW", fmt, 96);
            const label_t *tag = text(fmt, 10.5f * g_k, FW_BOLD, (int)(60.0f * g_k), 0);
            float tx = left + lb->w + 6.0f * g_k, ty = ry + 11.0f * g_k;
            paint_t amber = {236, 168, 56, 196, 120, 34, 0, 0, 0};
            fill_rr(tx, ty, tx + tag->w + 8.0f * g_k, ty + tag->h + 1.0f * g_k, 3.0f * g_k, &amber, 1.0f);
            draw_label(tag, tx + 4.0f * g_k, ty + 0.5f * g_k, 30, 22, 14, 1.0f);
        }
        extra_desc(i, s, 160);
        lb = text(s, 12.5f * g_k, FW_NORMAL, (int)(bx0 - left - 10.0f * g_k), 3);
        draw_label(lb, left, ry + 29.0f * g_k, 150, 155, 162, on ? 1.0f : 0.85f);
        draw_button(i, bx, top);
        if (i < g_nx - 1) {
            paint_t line = {255, 255, 255, 255, 255, 255, 0, 0, 0};
            fill_rr(left, ry + ROW_H * g_k - 1, right, ry + ROW_H * g_k, 0, &line, 0.06f);
        }
    }
}

static void draw_front(float tx, float top)
{
    float x1 = tx + TAB_W * g_k, y1 = top + g_tray_h, rad = 4.0f * g_k, hv = g_m.hv[MAXX];
    float cx = (tx + x1) / 2.0f;
    shadow_rr(tx - 1.0f * g_k, top + 3.0f * g_k, x1, y1 + 3.0f * g_k, rad, 8.0f * g_k, 0.55f);
    paint_t p = FRONT;
    float lift = 16.0f * hv;
    p.r0 += lift, p.g0 += lift, p.b0 += lift, p.r1 += lift, p.g1 += lift, p.b1 += lift;
    fill_rr(tx, top, x1, y1, rad, &p, 1.0f);
    stroke_rr(tx, top, x1, y1, rad, 1.0f, 16, 16, 18, 0.95f);
    stroke_rr(tx + 1, top + 1, x1 - 1, y1 - 1, rad - 1, 1.0f, 220, 222, 228, 0.25f);
    if (hv > 0.01f)
        stroke_rr(tx - 1, top - 1, x1 + 1, y1 + 1, rad + 1, 1.5f * g_k, 255, 170, 70, 0.55f * hv);
    /* a groove down its face, as a drive's bezel has */
    paint_t groove = {30, 32, 36, 40, 42, 46, 0, 0, 0};
    fill_rr(x1 - 6.0f * g_k, top + 14.0f * g_k, x1 - 4.5f * g_k, y1 - 14.0f * g_k, 0.7f * g_k, &groove, 0.8f);
    rivet(cx - 2.0f * g_k, top + 7.0f * g_k, 2.0f * g_k);
    rivet(cx - 2.0f * g_k, y1 - 7.0f * g_k, 2.0f * g_k);

    int news = g_state == 1 ? count_news() : 0, updates = 0;
    for (int i = 0; g_state == 1 && i < g_nx; i++)
        updates += state_of(i) == ST_UPDATE;
    /* the eject mark, lit when the mouse is on it */
    float ey = top + 22.0f * g_k;
    eject(cx - 1.0f * g_k, ey, 10.0f * g_k, 30 + 225 * hv, 30 + 146 * hv, 32 + 38 * hv, 0.9f);
    eject(cx - 1.0f * g_k, ey + 1.0f, 10.0f * g_k, 255, 255, 255, 0.12f * (1.0f - hv));
    /* its light: amber with news or a download going on, green when all is there */
    float ly = ey + 12.0f * g_k;
    paint_t slot = {12, 12, 14, 26, 26, 28, 0, 0, 0};
    fill_rr(cx - 7.0f * g_k, ly, cx + 5.0f * g_k, ly + 3.5f * g_k, 1.5f * g_k, &slot, 1.0f);
    if (g_op.kind == OP_GET && g_x[g_op.which].bytes > 0) {
        float f = clampf((float)(g_op.done_kb * 1024.0 / g_x[g_op.which].bytes), 0.0f, 1.0f);
        paint_t on = {255, 190, 80, 230, 140, 40, 0, 0, 0};
        fill_rr(cx - 6.5f * g_k, ly + 0.5f, cx - 6.5f * g_k + 11.0f * g_k * f, ly + 3.5f * g_k - 0.5f, 1.0f * g_k, &on, 1.0f);
    } else {
        float lr = 70, lg = 72, lb_ = 76, la = 0.0f;
        if (g_op.kind != OP_NONE || news)
            lr = 255, lg = 176, lb_ = 60, la = 0.4f + 0.6f * pulse(1600.0f);
        else if (g_state == 1 && g_nx && count_installed() == g_nx)
            lr = 90, lg = 230, lb_ = 120, la = 0.85f;
        else if (g_state == 1 && count_installed())
            lr = 90, lg = 230, lb_ = 120, la = 0.5f;
        paint_t on = {lr, lg, lb_, lr * 0.7f, lg * 0.7f, lb_ * 0.7f, 0, 0, 0};
        fill_rr(cx - 6.5f * g_k, ly + 0.5f, cx + 4.5f * g_k, ly + 3.5f * g_k - 0.5f, 1.0f * g_k, &on, la > 0 ? 0.35f + 0.65f * la : 0);
    }
    (void)updates;
    /* EXTRAS up its face */
    WCHAR s[64], num[32];
    float mid0 = ly + 10.0f * g_k, mid1 = y1 - 30.0f * g_k;
    say("ron_extras_tab", L"EXTRAS", s, 64);
    const label_t *lb = text(s, 13.5f * g_k, FW_SEMIBOLD, (int)fmaxf(mid1 - mid0, 10.0f), 2);
    draw_label(lb, cx - 1.0f * g_k - lb->w / 2.0f, (mid0 + mid1 - lb->h) / 2.0f + 1.0f, 0, 0, 0, 0.5f);
    draw_label(lb, cx - 1.0f * g_k - lb->w / 2.0f, (mid0 + mid1 - lb->h) / 2.0f, 232 + 23 * hv, 222 + 10 * hv,
               196 - 40 * hv, 1.0f);
    /* installed of how many */
    if (g_state == 1 && g_nx) {
        _snwprintf(num, 32, L"%d/%d", count_installed(), g_nx);
        lb = text(num, 11.5f * g_k, FW_SEMIBOLD, (int)(TAB_W * g_k - 4.0f * g_k), 0);
        draw_label(lb, cx - 1.0f * g_k - lb->w / 2.0f, y1 - 24.0f * g_k, 214, 170, 92, 1.0f);
    }
    /* the badge with the news */
    if (news) {
        float bxc = tx + 1.0f * g_k, byc = top + 1.0f * g_k, br = 9.0f * g_k * (1.0f + 0.06f * pulse(1600.0f));
        disc(bxc + 0.5f * g_k, byc + 1.5f * g_k, br + 1.5f * g_k, 0, 0, 0, 0.4f);
        disc(bxc, byc, br + 1.3f * g_k, 236, 226, 204, 1.0f);
        disc(bxc, byc, br, 196, 34, 26, 1.0f);
        disc(bxc - br * 0.25f, byc - br * 0.35f, br * 0.45f, 255, 120, 100, 0.25f);
        _snwprintf(num, 32, news > 9 ? L"9+" : L"%d", news);
        lb = text(num, 12.5f * g_k, FW_BOLD, (int)(br * 2.0f), 0);
        draw_label(lb, bxc - lb->w / 2.0f, byc - lb->h / 2.0f, 255, 255, 255, 1.0f);
    }
}

static void render(void)
{
    if (!g_win || !g_px)
        return;
    memset(g_px, 0, (size_t)g_w * g_h * 4);
    float p = position(), off = p * TRAVEL * g_k, top = MARGIN * g_k;
    float tx = (MARGIN + TRAVEL) * g_k - off;
    if (p > 0.002f)
        draw_body(tx + TAB_W * g_k, top);
    draw_front(tx, top);
    /* only where the launcher is not: the tray is behind it */
    for (int i = 0; i < g_w * g_h; i++) {
        unsigned c = g_clip[i];
        if (c == 255)
            continue;
        DWORD v = g_px[i];
        g_px[i] = ((v & 255) * c / 255) | (((v >> 8) & 255) * c / 255) << 8 | (((v >> 16) & 255) * c / 255) << 16 |
                  ((v >> 24) * c / 255) << 24;
    }
    POINT zero = {0, 0};
    SIZE sz = {g_w, g_h};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(g_win, NULL, NULL, &sz, g_dc, &zero, 0, &blend, ULW_ALPHA);
}

/* --- the window --------------------------------------------------------------------------- */

static int needs_frames(void)
{
    if (g_m.moving || g_op.kind != OP_NONE)
        return 1;
    for (int i = 0; i <= MAXX; i++)
        if (g_m.hv[i] != ((g_m.hover == i || (i == MAXX && g_m.hover == -2)) ? 1.0f : 0.0f))
            return 1;
    if (g_state == 1 && count_news())
        return 1; /* the light and the badge breathe */
    for (int i = 0; g_m.open && g_state == 1 && i < g_nx; i++)
        if (state_of(i) == ST_UPDATE)
            return 1;
    return 0;
}

static void animate(void)
{
    if (!g_m.running && g_win) {
        g_m.running = 1;
        g_m.last = GetTickCount();
        SetTimer(g_win, ANIM_TIMER, 16, NULL);
    }
}

static void tick(void)
{
    DWORD now = GetTickCount();
    float k = clampf((float)(now - g_m.last) / 90.0f, 0.0f, 1.0f);
    g_m.last = now;
    for (int i = 0; i <= MAXX; i++) {
        float want = (g_m.hover == i || (i == MAXX && g_m.hover == -2)) ? 1.0f : 0.0f;
        g_m.hv[i] += (want - g_m.hv[i]) * k;
        if (fabsf(want - g_m.hv[i]) < 0.01f)
            g_m.hv[i] = want;
    }
    render();
    if (!needs_frames()) {
        KillTimer(g_win, ANIM_TIMER);
        g_m.running = 0;
    } else if (!g_m.moving && g_op.kind == OP_NONE)
        SetTimer(g_win, ANIM_TIMER, 33, NULL); /* only breathing: fewer frames */
    else
        SetTimer(g_win, ANIM_TIMER, 16, NULL);
}

/* what is under (x, y): a row's button, -2 the tray's front, -1 nothing */
static int hit(int x, int y)
{
    float p = position(), off = p * TRAVEL * g_k, top = MARGIN * g_k;
    float tx = (MARGIN + TRAVEL) * g_k - off, fx = (float)x + 0.5f, fy = (float)y + 0.5f;
    if (fx >= tx && fx < tx + TAB_W * g_k && fy >= top && fy < top + g_tray_h)
        return -2;
    if (!g_m.open || g_m.moving || g_state != 1)
        return -1;
    for (int i = 0; i < g_nx; i++) {
        float x0, y0, x1, y1;
        button_rect(i, tx + TAB_W * g_k, top, &x0, &y0, &x1, &y1);
        if (fx >= x0 && fx < x1 && fy >= y0 && fy < y1)
            return i;
    }
    return -1;
}

/* the background window's place on the screen (the controls' dialog is centred on it) */
static void background_at(int *x, int *y)
{
    RECT d;
    GetWindowRect(g_dlg, &d);
    *x = (d.left + d.right) / 2 - g_bgw / 2;
    *y = (d.top + d.bottom) / 2 - g_bgh / 2;
}

static void place(void)
{
    int bx, by;
    if (!g_win)
        return;
    background_at(&bx, &by);
    SetWindowPos(g_win, NULL, bx + g_wx, by + g_wy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* the tray comes out on the left: when the screen ends there, the launcher moves right */
static void make_room(void)
{
    int bx, by;
    background_at(&bx, &by);
    MONITORINFO mi = {sizeof mi};
    if (!GetMonitorInfoW(MonitorFromWindow(g_dlg, MONITOR_DEFAULTTONEAREST), &mi))
        return;
    int left = bx + g_wx + K(MARGIN) - K(4.0f), dx = mi.rcWork.left - left;
    if (dx <= 0)
        return;
    if (bx + g_bgw + dx > mi.rcWork.right)
        dx = mi.rcWork.right - bx - g_bgw;
    if (dx <= 0)
        return;
    HWND bg = GetWindow(g_dlg, GW_OWNER);
    RECT d, b;
    GetWindowRect(g_dlg, &d);
    if (bg && GetWindowRect(bg, &b))
        SetWindowPos(bg, NULL, b.left + dx, b.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_dlg, NULL, d.left + dx, d.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    place();
    logf_("extras: launcher moved %d px right to make room", dx);
}

static void toggle(void)
{
    float p = position();
    g_m.from = clampf(p, 0.0f, 1.0f);
    g_m.open = !g_m.open;
    g_m.moving = 1;
    g_m.start = GetTickCount();
    if (g_m.open) {
        make_room();
        if (g_state == 1)
            mark_seen();
    } else
        for (int i = 0; i < g_nx; i++)
            g_x[i].fresh = 0;
    animate();
}

static void ask_and_do(int i)
{
    int st = state_of(i);
    WCHAR name[96], fmt[256], msg[384];
    extra_name(i, name, 96);
    if (g_op.kind != OP_NONE && g_op.which != i)
        return;
    if (st == ST_OFF || st == ST_UPDATE || st == ST_FAILED) {
        if (g_base[0])
            start_op(i, OP_GET);
    } else if (st == ST_ON) {
        say("ron_extras_ask_remove", L"Remove %s? You can download it again at any time.", fmt, 256);
        fill(msg, 384, fmt, name);
        if (MessageBoxW(g_dlg, msg, L"RON Revived", MB_YESNO | MB_ICONQUESTION) == IDYES)
            start_op(i, OP_REMOVE);
    } else if (st == ST_GET) {
        say("ron_extras_ask_cancel", L"Stop downloading %s?", fmt, 256);
        fill(msg, 384, fmt, name);
        if (MessageBoxW(g_dlg, msg, L"RON Revived", MB_YESNO | MB_ICONQUESTION) == IDYES)
            InterlockedExchange(&g_op.cancel, 1);
    }
    animate();
}

/* the window and its pictures for the rows there are now */
static int layout(void)
{
    int rows = g_state == 1 && g_nx ? g_nx : 1;
    g_tray_h = K(HEAD_H + rows * ROW_H + FOOT_H);
    g_w = K(MARGIN + TRAVEL + TAB_W + INSET);
    g_h = g_tray_h + 2 * K(MARGIN);
    g_wx = g_edge - K(MARGIN + TRAVEL + TAB_W);
    g_wy = (g_top + g_bottom) / 2 - g_h / 2;
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), g_w, -g_h, 1, 32, BI_RGB}};
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!g_dc)
        g_dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    unsigned char *clip = malloc((size_t)g_w * g_h);
    if (!dib || !g_dc || !clip) {
        if (dib)
            DeleteObject(dib);
        free(clip);
        return 0;
    }
    SelectObject(g_dc, dib);
    if (g_dib)
        DeleteObject(g_dib);
    g_dib = dib;
    g_px = bits;
    /* seen where the launcher's background is clear, left of its edge; never right of it */
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            int bx = g_wx + x, by = g_wy + y;
            int a = bx >= 0 && by >= 0 && bx < g_bgw && by < g_bgh ? g_alpha[(size_t)by * g_bgw + bx] : 0;
            clip[(size_t)y * g_w + x] = (unsigned char)(bx < g_edge + 3 ? 255 - a : 0);
        }
    free(g_clip);
    g_clip = clip;
    if (g_win) {
        int bx, by;
        background_at(&bx, &by);
        SetWindowPos(g_win, NULL, bx + g_wx, by + g_wy, g_w, g_h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return 1;
}

static LRESULT CALLBACK win_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
    switch (msg) {
    case WM_INDEX:
        read_have();
        layout();
        if (g_m.open)
            mark_seen();
        render();
        animate();
        return 0;
    case WM_OPSTEP:
        animate();
        return 0;
    case WM_OPDONE:
        op_done();
        animate();
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, g_m.hover != -1 ? IDC_HAND : IDC_ARROW));
        return TRUE;
    case WM_MOUSEMOVE: {
        int was = g_m.hover;
        g_m.hover = hit(x, y);
        if (g_m.hover >= 0 && g_op.kind != OP_NONE && g_op.which != g_m.hover)
            g_m.hover = -1; /* another one is busy */
        if (!g_m.tracking) {
            TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, h, 0};
            g_m.tracking = TrackMouseEvent(&tme);
        }
        if (was != g_m.hover) {
            if (g_m.hover != -1)
                launcher_sound(0);
            animate();
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_m.tracking = 0;
        g_m.hover = -1;
        animate();
        return 0;
    case WM_LBUTTONDOWN: {
        int at = hit(x, y);
        if (at == -2) {
            launcher_sound(1);
            toggle();
        } else if (at >= 0 && (g_op.kind == OP_NONE || g_op.which == at)) {
            launcher_sound(1);
            ask_and_do(at);
        }
        return 0;
    }
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
        if (g_op.kind == OP_GET)
            InterlockedExchange(&g_op.cancel, 1);
        DestroyWindow(g_win);
        g_win = NULL;
    }
    return r;
}

void launcher_extras_attach(HWND dlg)
{
    if (!g_index[0] || g_dlg || !dlg || !GetDlgItem(dlg, 1017 /* Workshop: the controls' dialog */))
        return;
    g_dlg = dlg; /* once */
    DWORD *bg = launcher_load_bgra("background.png", 1, &g_bgw, &g_bgh);
    if (!bg) {
        logf_("extras: no background.png, no tray");
        return;
    }
    g_alpha = malloc((size_t)g_bgw * g_bgh);
    if (!g_alpha) {
        free(bg);
        return;
    }
    for (int i = 0; i < g_bgw * g_bgh; i++)
        g_alpha[i] = (unsigned char)(bg[i] >> 24);
    free(bg);
    /* the launcher's left edge beside the button panel (layout.ini [extras] top, bottom) */
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%slayout.ini", g_res);
    g_top = GetPrivateProfileIntA("extras", "top", g_bgh * 3 / 10, ini);
    g_bottom = GetPrivateProfileIntA("extras", "bottom", g_bgh * 9 / 10, ini);
    /* the edge most rows have (the corner brackets stand out a little further) */
    int hist[512] = {0}, rows = 0;
    for (int y = g_top < 0 ? 0 : g_top; y < g_bottom && y < g_bgh; y++)
        for (int x = 0; x < g_bgw && x < 512; x++)
            if (g_alpha[(size_t)y * g_bgw + x] >= 128) {
                hist[x]++, rows++;
                break;
            }
    g_edge = 0;
    for (int x = 0, seen = 0; x < 512 && rows; x++)
        if ((seen += hist[x]) * 2 >= rows) {
            g_edge = x;
            break;
        }

    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = win_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"RonRevivedExtras";
    RegisterClassW(&wc);
    if (!layout()) {
        logf_("extras: tray not made");
        return;
    }
    g_win = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, L"", WS_POPUP, 0, 0,
                            g_w, g_h, dlg, NULL, inst, NULL);
    if (!g_win) {
        logf_("extras: tray not made");
        return;
    }
    g_m.hover = -1;
    g_dlg_unicode = IsWindowUnicode(dlg);
    g_dlg_proc = (WNDPROC)(g_dlg_unicode ? SetWindowLongPtrW(dlg, GWLP_WNDPROC, (LONG_PTR)dlg_proc)
                                         : SetWindowLongPtrA(dlg, GWLP_WNDPROC, (LONG_PTR)dlg_proc));
    if (g_state) /* the list came before the window */
        PostMessageW(g_win, WM_INDEX, 0, 0);
    place();
    render();
    if (IsWindowVisible(dlg))
        ShowWindow(g_win, SW_SHOWNOACTIVATE);
    logf_("extras: tray %dx%d at the launcher's edge x %d, rows %d..%d", g_w, g_h, g_edge, g_top, g_bottom);
}

/* the language changed in the launcher: the tray's texts in the new one */
void launcher_extras_relabel(void)
{
    clear_cache();
    if (g_win)
        render();
}

void launcher_extras_install(const char *res_dir, int scale_percent, const WCHAR *font)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%sron_camera.ini", g_dir);
    if (!GetPrivateProfileIntA("extras", "tray", 1, ini)) {
        logf_("extras: tray off");
        return;
    }
    GetPrivateProfileStringA("update", "url", RON_REVIVED_INDEX, g_index, sizeof g_index, ini);
    if (!g_index[0])
        snprintf(g_index, sizeof g_index, "%s", RON_REVIVED_INDEX);
    snprintf(g_res, sizeof g_res, "%s", res_dir);
    g_k = scale_percent / 100.0f;
    wcsncpy(g_face, font, 63);
    HANDLE t = CreateThread(NULL, 0, index_thread, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
}
