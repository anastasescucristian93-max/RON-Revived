/*
 * File overrides: a file the game opens is taken from <game>\mod_override\ when a file
 * with the same relative path exists there (mod_override\Data\topmenu.xml replaces
 * Data\topmenu.xml, mod_override\art\x.tga replaces art\x.tga). The game's own files are
 * never changed; deleting the folder restores everything.
 *
 * Done with inline hooks on KernelBase's CreateFileW / GetFileAttributes(Ex)W, which every
 * file open in the process ends up in: the CRT, d3dgl's texture loader and MSXML (which
 * reads the interface .xml files from its own DLL, out of reach of import table patches).
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

static char g_root[MAX_PATH]; /* <game>\mod_override\ */
static int g_active;
static DWORD g_tls_busy = TLS_OUT_OF_INDEXES; /* set while inside a hook: logf_ opens files too */

/* Path relative to the game folder, or NULL for files outside it. */
static const char *relative(const char *path, char *buf, int size)
{
    if (strncmp(path, "\\\\?\\", 4) == 0)
        path += 4;
    if (strncmp(path, "\\\\", 2) == 0 || strchr(path, '*') || strchr(path, '?'))
        return NULL; /* devices, pipes, network paths, wildcards */
    DWORD n = GetFullPathNameA(path, size, buf, NULL);
    if (n == 0 || n >= (DWORD)size)
        return NULL;
    size_t d = strlen(g_dir);
    if (_strnicmp(buf, g_dir, d) != 0)
        return NULL;
    return buf + d;
}

/* 1 the first time a path (case-insensitive) is seen, so the log names every overridden file
 * once instead of the first 50 opens, which the interface files used up. */
static int first_time(const char *rel)
{
    static volatile LONG seen[4096];
    DWORD u = 2166136261u; /* FNV-1a, unsigned: signed overflow let the compiler drop the loop */
    for (const char *p = rel; *p; p++)
        u = (u ^ (unsigned char)(*p | 0x20)) * 16777619u;
    LONG h = u ? (LONG)u : 1;
    for (DWORD i = 0; i < 4096; i++) {
        volatile LONG *slot = &seen[(u + i) & 4095];
        LONG cur = InterlockedCompareExchange(slot, h, 0);
        if (cur == 0)
            return 1;
        if (cur == h)
            return 0;
    }
    return 0;
}

/* Files the DLL makes itself (fog.c's ramps), served before mod_override\: game-relative
   path -> full path. Registered before override_install. */
static struct {
    char rel[64];
    char path[MAX_PATH];
} g_made[8];
static int g_nmade;

void override_redirect(const char *rel, const char *path)
{
    if (g_nmade >= (int)(sizeof g_made / sizeof g_made[0]))
        return;
    snprintf(g_made[g_nmade].rel, sizeof g_made[0].rel, "%s", rel);
    snprintf(g_made[g_nmade].path, sizeof g_made[0].path, "%s", path);
    g_nmade++;
}

/* The override for `path`, or NULL. */
static const char *override_a(const char *path, char *out, int size)
{
    char buf[MAX_PATH];
    const char *rel = relative(path, buf, sizeof buf);
    if (!rel || !*rel || _strnicmp(rel, "mod_override\\", 13) == 0)
        return NULL;
    for (int i = 0; i < g_nmade; i++) {
        if (_stricmp(rel, g_made[i].rel) != 0 || GetFileAttributesA(g_made[i].path) == INVALID_FILE_ATTRIBUTES)
            continue; /* a temp folder cleaned meanwhile falls back to the game's file */
        if (first_time(rel))
            logf_("override: %s -> %s", rel, g_made[i].path);
        snprintf(out, size, "%s", g_made[i].path);
        return out;
    }
    snprintf(out, size, "%s%s", g_root, rel);
    DWORD a = GetFileAttributesA(out);
    if (a == INVALID_FILE_ATTRIBUTES || (a & FILE_ATTRIBUTE_DIRECTORY))
        return NULL;
    if (first_time(rel))
        logf_("override: %s", rel);
    return out;
}

static const wchar_t *override_w(const wchar_t *path, wchar_t *out, int size)
{
    if (!g_active || !path || TlsGetValue(g_tls_busy))
        return NULL;
    TlsSetValue(g_tls_busy, (void *)1);
    const wchar_t *r = NULL;
    char a[MAX_PATH], o[MAX_PATH];
    if (WideCharToMultiByte(CP_ACP, 0, path, -1, a, sizeof a, NULL, NULL) && override_a(a, o, sizeof o) &&
        MultiByteToWideChar(CP_ACP, 0, o, -1, out, size))
        r = out;
    TlsSetValue(g_tls_busy, NULL);
    return r;
}

/* ---- hooks --------------------------------------------------------------------------------- */

typedef HANDLE(WINAPI *create_w_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef DWORD(WINAPI *attr_w_t)(LPCWSTR);
typedef BOOL(WINAPI *attr_ex_w_t)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef HANDLE(WINAPI *find_ex_w_t)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);

static create_w_t r_create_w;
static attr_w_t r_attr_w;
static attr_ex_w_t r_attr_ex_w;
static find_ex_w_t r_find_ex_w;

/* Only reads are redirected: anything opened for writing (logs, saves, prefs) stays put. */
#define WRITE_ACCESS (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE)

static HANDLE WINAPI h_create_w(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp,
                                DWORD flags, HANDLE tmpl)
{
    wchar_t o[MAX_PATH];
    const wchar_t *p = !(access & WRITE_ACCESS) && disp == OPEN_EXISTING ? override_w(name, o, MAX_PATH) : NULL;
    return r_create_w(p ? p : name, access, share, sa, disp, flags, tmpl);
}

static DWORD WINAPI h_attr_w(LPCWSTR name)
{
    wchar_t o[MAX_PATH];
    const wchar_t *p = override_w(name, o, MAX_PATH);
    return r_attr_w(p ? p : name);
}

static BOOL WINAPI h_attr_ex_w(LPCWSTR name, GET_FILEEX_INFO_LEVELS level, LPVOID info)
{
    wchar_t o[MAX_PATH];
    const wchar_t *p = override_w(name, o, MAX_PATH);
    return r_attr_ex_w(p ? p : name, level, info);
}

/* The game checks that a model exists with _wfindfirst (FindFirstFileExW) before it loads it, so
   a model only in mod_override\ was "missing" ("NO DEFAULT UNIT GPIECE"). A search for one exact
   file is sent to the override; wildcard searches (folder listings) are only logged for now. */
static HANDLE WINAPI h_find_ex_w(LPCWSTR name, FINDEX_INFO_LEVELS level, LPVOID data, FINDEX_SEARCH_OPS op,
                                 LPVOID filter, DWORD flags)
{
    wchar_t o[MAX_PATH];
    const wchar_t *p = override_w(name, o, MAX_PATH);
    if (!p && g_active && name && (wcschr(name, L'*') || wcschr(name, L'?')) && !TlsGetValue(g_tls_busy)) {
        TlsSetValue(g_tls_busy, (void *)1);
        char a[MAX_PATH];
        if (WideCharToMultiByte(CP_ACP, 0, name, -1, a, sizeof a, NULL, NULL) && first_time(a))
            logf_("override: folder search %s (not merged)", a);
        TlsSetValue(g_tls_busy, NULL);
    }
    return r_find_ex_w(p ? p : name, level, data, op, filter, flags);
}

/* Hooks a system DLL export that starts with the hot-patch prologue (mov edi,edi; push ebp;
   mov ebp,esp: 5 bytes, nothing position-dependent); returns the trampoline or NULL. */
void *hook_export(HMODULE mod, const char *name, void *repl)
{
    static const unsigned char prologue[5] = {0x8B, 0xFF, 0x55, 0x8B, 0xEC};
    unsigned char *at = (unsigned char *)GetProcAddress(mod, name);
    if (!at || memcmp(at, prologue, 5) != 0) {
        logf_("override: %s has an unknown prologue, not hooked", name);
        return NULL;
    }
    unsigned char *mem = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return NULL;
    void *tramp = hook_trampoline(at, 5, mem);
    hook_jmp5(at, repl);
    return tramp;
}

/* The game, d3dgl and xasl.dll share ucrtbase.dll, whose stdio allows 512 open FILE streams by
   default. Every <SOUND> of every player's tribe in sound.xml is opened at the start of a game
   (the game's own: 192 for 8 players; with the nation music 750-930), so the streams ran out and
   the next model open failed: "art\logchop5.bh3 does not exist for Transform Batch Transform"
   (2026-11-05). ucrtbase's limit is raised to its maximum. Called from the startup display hook,
   when ucrtbase is surely initialised (not from DllMain: winmm.dll is the exe's first import). */
void stdio_raise(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    HMODULE u = GetModuleHandleA("ucrtbase.dll");
    int (*get)(void) = u ? (int (*)(void))GetProcAddress(u, "_getmaxstdio") : NULL;
    int (*set)(int) = u ? (int (*)(int))GetProcAddress(u, "_setmaxstdio") : NULL;
    if (!get || !set) {
        logf_("open files: ucrtbase.dll not found, limit unchanged");
        return;
    }
    int before = get();
    int r = set(8192);
    logf_("open files: limit %d -> %d", before, r == -1 ? before : get());
}

int g_override_enabled;

void override_load_config(const char *ini_path)
{
    g_override_enabled = GetPrivateProfileIntA("camera", "file_overrides", 1, ini_path);
}

/* At DLL load. */
void override_install(void)
{
    snprintf(g_root, sizeof g_root, "%smod_override\\", g_dir);
    DWORD a = GetFileAttributesA(g_root);
    int have_root = a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
    if (!g_override_enabled || (!have_root && !g_nmade))
        return;
    g_tls_busy = TlsAlloc();
    HMODULE kb = GetModuleHandleA("kernelbase.dll");
    if (g_tls_busy == TLS_OUT_OF_INDEXES || !kb)
        return;
    r_create_w = hook_export(kb, "CreateFileW", (void *)h_create_w);
    r_attr_w = hook_export(kb, "GetFileAttributesW", (void *)h_attr_w);
    r_attr_ex_w = hook_export(kb, "GetFileAttributesExW", (void *)h_attr_ex_w);
    r_find_ex_w = hook_export(kb, "FindFirstFileExW", (void *)h_find_ex_w);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    g_active = r_create_w != NULL;
    logf_("file overrides from %s (CreateFileW %d, GetFileAttributesW %d, GetFileAttributesExW %d, "
          "FindFirstFileExW %d)", g_root, !!r_create_w, !!r_attr_w, !!r_attr_ex_w, !!r_find_ex_w);
}
