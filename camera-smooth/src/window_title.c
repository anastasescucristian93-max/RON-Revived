/*
 * The game window's title (taskbar, Alt+Tab, Task Manager, window capture in OBS) reads
 * "Rise of Nations: Revived" instead of the game's own.
 *
 * The window is made by d3dgl.dll, which finds CreateWindowExW at run time (neither it nor the
 * exe imports it by name), so the user32 exports are hooked (hook_export: hot-patch prologue)
 * instead of an import. A top-level window whose name starts with "Rise of Nations" (followed by
 * nothing or a ':') gets the new name; "RISE OF NATIONS -- Exception Handler" does not match.
 * Later SetWindowTextA/W calls on that window are renamed too. Every top-level window made is
 * logged once with its class and name, so the log shows the title if it ever stops matching.
 * Not part of the simulation: no effect on multiplayer.
 */
#include "ron.h"
#include <wchar.h>

typedef HWND(WINAPI *create_w_t)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE,
                                 LPVOID);
typedef HWND(WINAPI *create_a_t)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL(WINAPI *settext_w_t)(HWND, LPCWSTR);
typedef BOOL(WINAPI *settext_a_t)(HWND, LPCSTR);

static create_w_t r_create_w;
static create_a_t r_create_a;
static settext_w_t r_settext_w;
static settext_a_t r_settext_a;

static WCHAR g_title[128];
static HWND g_main;

void wintitle_load_config(const char *ini_path)
{
    char buf[128];
    GetPrivateProfileStringA("ui", "window_title", "Rise of Nations: Revived", buf, sizeof buf, ini_path);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, -1, g_title, 128))
        MultiByteToWideChar(CP_ACP, 0, buf, -1, g_title, 128);
}

static int is_game_title(const WCHAR *name)
{
    static const WCHAR prefix[] = L"Rise of Nations";
    const int n = (int)(sizeof prefix / sizeof prefix[0]) - 1;
    if ((UINT_PTR)name <= 0xFFFF || _wcsnicmp(name, prefix, n) != 0)
        return 0;
    return name[n] == 0 || name[n] == L':';
}

static int is_top_level(DWORD style, HWND parent)
{
    return !parent && !(style & WS_CHILD);
}

static void log_window(const char *api, const WCHAR *cls, const WCHAR *name, int renamed)
{
    static int logged;
    if (logged >= 8)
        return;
    logged++;
    char c[64] = "(atom)", t[128] = "";
    if ((UINT_PTR)cls > 0xFFFF)
        WideCharToMultiByte(CP_UTF8, 0, cls, -1, c, sizeof c - 1, NULL, NULL);
    if ((UINT_PTR)name > 0xFFFF)
        WideCharToMultiByte(CP_UTF8, 0, name, -1, t, sizeof t - 1, NULL, NULL);
    logf_("window title: %s top-level window, class \"%s\", name \"%s\"%s", api, c, t,
          renamed ? ", renamed" : "");
}

static HWND WINAPI h_create_w(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x, int y, int w, int h,
                              HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    if (!is_top_level(style, parent))
        return r_create_w(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    int ours = is_game_title(name);
    log_window("CreateWindowExW", cls, name, ours);
    HWND wnd = r_create_w(ex, cls, ours ? g_title : name, style, x, y, w, h, parent, menu, inst, param);
    if (ours && wnd)
        g_main = wnd;
    return wnd;
}

static HWND WINAPI h_create_a(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h,
                              HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    if (!is_top_level(style, parent) || (UINT_PTR)name <= 0xFFFF)
        return r_create_a(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    WCHAR wname[128] = L"", wcls[64] = L"";
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, 127);
    if ((UINT_PTR)cls > 0xFFFF)
        MultiByteToWideChar(CP_ACP, 0, cls, -1, wcls, 63);
    int ours = is_game_title(wname);
    log_window("CreateWindowExA", (UINT_PTR)cls > 0xFFFF ? wcls : NULL, wname, ours);
    if (!ours)
        return r_create_a(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    /* The A window keeps its A procedure; the title alone goes through SetWindowTextW. */
    HWND wnd = r_create_a(ex, cls, "", style, x, y, w, h, parent, menu, inst, param);
    if (wnd) {
        g_main = wnd;
        r_settext_w ? r_settext_w(wnd, g_title) : SetWindowTextW(wnd, g_title);
    }
    return wnd;
}

static BOOL WINAPI h_settext_w(HWND wnd, LPCWSTR text)
{
    return r_settext_w(wnd, wnd && wnd == g_main ? g_title : text);
}

static BOOL WINAPI h_settext_a(HWND wnd, LPCSTR text)
{
    if (wnd && wnd == g_main)
        return r_settext_w ? r_settext_w(wnd, g_title) : SetWindowTextW(wnd, g_title);
    return r_settext_a(wnd, text);
}

void wintitle_install(void)
{
    HMODULE u = GetModuleHandleA("user32.dll");
    if (!g_title[0] || !u) {
        logf_("window title: off, the game's own");
        return;
    }
    r_settext_w = hook_export(u, "SetWindowTextW", (void *)h_settext_w);
    r_settext_a = hook_export(u, "SetWindowTextA", (void *)h_settext_a);
    r_create_w = hook_export(u, "CreateWindowExW", (void *)h_create_w);
    r_create_a = hook_export(u, "CreateWindowExA", (void *)h_create_a);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("window title: CreateWindowExW %d, CreateWindowExA %d, SetWindowTextW %d, SetWindowTextA %d",
          r_create_w != NULL, r_create_a != NULL, r_settext_w != NULL, r_settext_a != NULL);
}
