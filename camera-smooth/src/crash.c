/*
 * Crash reports: when the game dies on an exception nobody handles (an access violation, the
 * breakpoint behind "Retry" on a BHG run time error), the unhandled exception filter writes
 * <game>\crash\crash_<date>_<time>.txt (what failed and where, as module + offset, the registers,
 * the return addresses on the stack, the memory, the end of ron_camera.log) and a small minidump
 * next to it, then starts RON_Revived_crash.ps1, the panel that offers to send both to the
 * RON Revived Discord. The game then ends without Windows' own "stopped working" report.
 *
 * Everything runs in the dying process, so the filter uses only the stack, fixed buffers and
 * plain Win32 file calls (no FILE streams, whose locks the crashed thread may hold).
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

static volatile LONG g_in_crash;
static int g_crash_enabled = 1, g_crash_test;

/* ---- a tiny file writer (WriteFile, no CRT) ------------------------------------------------ */

static HANDLE g_out;

static void put(const char *s)
{
    DWORD n;
    WriteFile(g_out, s, (DWORD)strlen(s), &n, NULL);
}

static void putf(const char *fmt, ...)
{
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf(b, sizeof b - 1, fmt, ap);
    va_end(ap);
    b[sizeof b - 1] = 0;
    if (n < 0)
        n = (int)strlen(b);
    put(b);
}

/* "module.dll+0x1234" for an address in a loaded module, else NULL. */
static const char *where(DWORD addr, char *buf, int size)
{
    HMODULE m;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)(UINT_PTR)addr, &m))
        return NULL;
    char path[MAX_PATH];
    if (!GetModuleFileNameA(m, path, sizeof path))
        return NULL;
    const char *name = strrchr(path, '\\') ? strrchr(path, '\\') + 1 : path;
    _snprintf(buf, size, "%s+0x%lX", name, (unsigned long)(addr - (DWORD)(UINT_PTR)m));
    buf[size - 1] = 0;
    return buf;
}

/* An address inside an executable section of some module: a likely return address. */
static int is_code(DWORD a)
{
    MEMORY_BASIC_INFORMATION mi;
    if (a < 0x10000 || !VirtualQuery((void *)(UINT_PTR)a, &mi, sizeof mi) || mi.State != MEM_COMMIT ||
        mi.Type != MEM_IMAGE)
        return 0;
    return (mi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

static const char *code_name(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_BREAKPOINT: return "breakpoint (a run time error's Retry, or an assert)";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "division by zero";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error (a file or the disk could not be read)";
    case 0xE06D7363: return "C++ exception";
    case 0xC0000409: return "stack buffer overrun / fast fail";
    case 0xC00001AD: return "out of memory (C00001AD)";
    default: return "exception";
    }
}

/* The last `keep` bytes of a text file, from a line start. */
static void put_tail(const char *path, DWORD keep)
{
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        put("(no log)\r\n");
        return;
    }
    DWORD size = GetFileSize(f, NULL), from = size > keep ? size - keep : 0;
    static char buf[65536];
    if (keep > sizeof buf)
        keep = sizeof buf;
    SetFilePointer(f, (LONG)from, NULL, FILE_BEGIN);
    DWORD n = 0;
    ReadFile(f, buf, keep, &n, NULL);
    CloseHandle(f);
    DWORD s = 0;
    if (from)
        while (s < n && buf[s] != '\n')
            s++;
    DWORD w;
    WriteFile(g_out, buf + s, n - s, &w, NULL);
}

typedef BOOL(WINAPI *minidump_t)(HANDLE, DWORD, HANDLE, int, void *, void *, void *);
typedef struct {
    DWORD tid;
    EXCEPTION_POINTERS *ep;
    BOOL client;
} mdei_t; /* MINIDUMP_EXCEPTION_INFORMATION */

static int write_dump(const char *path, EXCEPTION_POINTERS *ep)
{
    char dll[MAX_PATH];
    _snprintf(dll, sizeof dll, "%sdbghelp.dll", g_dir); /* the game ships one */
    dll[sizeof dll - 1] = 0;
    HMODULE h = LoadLibraryA(dll);
    if (!h)
        h = LoadLibraryA("dbghelp.dll");
    minidump_t dump = h ? (minidump_t)GetProcAddress(h, "MiniDumpWriteDump") : NULL;
    if (!dump)
        return 0;
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    mdei_t e = { GetCurrentThreadId(), ep, FALSE };
    /* MiniDumpNormal | WithIndirectlyReferencedMemory | WithDataSegs off (the exe's .data is 60 MB):
       the stacks, the modules and the memory the registers point at, a few hundred KB. */
    BOOL ok = dump(GetCurrentProcess(), GetCurrentProcessId(), f, 0x00000000 | 0x00000040, &e, NULL, NULL);
    CloseHandle(f);
    if (!ok)
        DeleteFileA(path);
    return ok;
}

static void start_panel(const char *report, const char *dmp)
{
    char script[MAX_PATH];
    _snprintf(script, sizeof script, "%sRON_Revived_crash.ps1", g_dir);
    script[sizeof script - 1] = 0;
    if (GetFileAttributesA(script) == INVALID_FILE_ATTRIBUTES)
        return;
    char cmd[2048];
    _snprintf(cmd, sizeof cmd,
              "powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -STA -File \"%s\" -Report \"%s\""
              " -Dump \"%s\" -WaitPid %lu",
              script, report, dmp, GetCurrentProcessId());
    cmd[sizeof cmd - 1] = 0;
    STARTUPINFOA si = { sizeof si };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, g_dir, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static void write_report(EXCEPTION_POINTERS *ep, const char *path)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *c = ep->ContextRecord;
    char w[160];
    SYSTEMTIME t;
    GetLocalTime(&t);

    put("RON Revived crash report\r\n========================\r\n");
    putf("Time: %04d-%02d-%02d %02d:%02d:%02d\r\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    char ver[64] = "?", vp[MAX_PATH];
    _snprintf(vp, sizeof vp, "%sRON_Revived_version.txt", g_dir);
    vp[sizeof vp - 1] = 0;
    HANDLE vf = CreateFileA(vp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (vf != INVALID_HANDLE_VALUE) {
        DWORD n = 0;
        ReadFile(vf, ver, sizeof ver - 1, &n, NULL);
        ver[n] = 0;
        for (char *p = ver; *p; p++)
            if (*p == '\r' || *p == '\n')
                *p = 0;
        CloseHandle(vf);
    }
    putf("RON Revived version: %s\r\n", ver);
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)write_report, &self);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)self + ((IMAGE_DOS_HEADER *)self)->e_lfanew);
    putf("winmm.dll: base 0x%08lX, %lu bytes of image\r\n", (unsigned long)(UINT_PTR)self,
         (unsigned long)nt->OptionalHeader.SizeOfImage);
    /* RtlGetVersion: GetVersionEx answers 6.2 (build 9200) to an exe without a Windows 10 manifest */
    OSVERSIONINFOW os = { sizeof os };
    typedef LONG(WINAPI * rtl_ver_t)(OSVERSIONINFOW *);
    rtl_ver_t rtl_ver = (rtl_ver_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (rtl_ver)
        rtl_ver(&os);
    putf("Windows: %lu.%lu build %lu\r\n\r\n", (unsigned long)os.dwMajorVersion, (unsigned long)os.dwMinorVersion,
         (unsigned long)os.dwBuildNumber);

    DWORD addr = (DWORD)(UINT_PTR)er->ExceptionAddress;
    putf("Exception: %08lX %s\r\n", (unsigned long)er->ExceptionCode, code_name(er->ExceptionCode));
    putf("At: 0x%08lX %s\r\n", (unsigned long)addr, where(addr, w, sizeof w) ? w : "(no module)");
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
        putf("Access: %s 0x%08lX\r\n",
             er->ExceptionInformation[0] == 0 ? "read of" : er->ExceptionInformation[0] == 1 ? "write to" : "execute at",
             (unsigned long)er->ExceptionInformation[1]);
    putf("Thread: %lu\r\n\r\n", (unsigned long)GetCurrentThreadId());

    putf("EAX %08lX  EBX %08lX  ECX %08lX  EDX %08lX\r\n", c->Eax, c->Ebx, c->Ecx, c->Edx);
    putf("ESI %08lX  EDI %08lX  EBP %08lX  ESP %08lX  EIP %08lX\r\n\r\n", c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip);

    put("Return addresses on the stack (most recent first):\r\n");
    DWORD *sp = (DWORD *)(UINT_PTR)c->Esp;
    MEMORY_BASIC_INFORMATION mi;
    if (VirtualQuery(sp, &mi, sizeof mi) && mi.State == MEM_COMMIT) {
        DWORD *end = (DWORD *)((char *)mi.BaseAddress + mi.RegionSize);
        int shown = 0;
        for (DWORD *p = sp; p < end && p < sp + 4096 && shown < 40; p++) {
            if (is_code(*p) && where(*p, w, sizeof w)) {
                putf("  [esp+0x%04lX] %s\r\n", (unsigned long)((char *)p - (char *)sp), w);
                shown++;
            }
        }
    }

    MEMORYSTATUSEX m = { sizeof m };
    if (GlobalMemoryStatusEx(&m))
        putf("\r\nMemory: %lu of %lu MB address space in use, %lu MB of RAM free\r\n",
             (unsigned long)((m.ullTotalVirtual - m.ullAvailVirtual) >> 20), (unsigned long)(m.ullTotalVirtual >> 20),
             (unsigned long)(m.ullAvailPhys >> 20));

    put("\r\nEnd of ron_camera.log:\r\n----------------------\r\n");
    char log[MAX_PATH];
    _snprintf(log, sizeof log, "%sron_camera.log", g_dir);
    log[sizeof log - 1] = 0;
    put_tail(log, 24000);
    (void)path;
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
    if (InterlockedExchange(&g_in_crash, 1))
        return EXCEPTION_CONTINUE_SEARCH; /* a crash inside the report: leave it to Windows */

    char dir[MAX_PATH], base[MAX_PATH], txt[MAX_PATH], dmp[MAX_PATH];
    _snprintf(dir, sizeof dir, "%scrash", g_dir);
    dir[sizeof dir - 1] = 0;
    CreateDirectoryA(dir, NULL);
    SYSTEMTIME t;
    GetLocalTime(&t);
    _snprintf(base, sizeof base, "%s\\crash_%04d%02d%02d_%02d%02d%02d", dir, t.wYear, t.wMonth, t.wDay, t.wHour,
              t.wMinute, t.wSecond);
    base[sizeof base - 1] = 0;
    _snprintf(txt, sizeof txt, "%s.txt", base);
    _snprintf(dmp, sizeof dmp, "%s.dmp", base);
    txt[sizeof txt - 1] = dmp[sizeof dmp - 1] = 0;

    g_out = CreateFileA(txt, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (g_out != INVALID_HANDLE_VALUE) {
        write_report(ep, txt);
        CloseHandle(g_out);
    }
    if (!write_dump(dmp, ep))
        dmp[0] = 0;

    char w[160], log[MAX_PATH];
    DWORD addr = (DWORD)(UINT_PTR)ep->ExceptionRecord->ExceptionAddress;
    _snprintf(log, sizeof log, "%sron_camera.log", g_dir);
    log[sizeof log - 1] = 0;
    g_out = CreateFileA(log, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (g_out != INVALID_HANDLE_VALUE) {
        putf("%04d-%02d-%02d %02d:%02d:%02d  CRASH: %08lX at %s, report %s\n", t.wYear, t.wMonth, t.wDay, t.wHour,
             t.wMinute, t.wSecond, (unsigned long)ep->ExceptionRecord->ExceptionCode,
             where(addr, w, sizeof w) ? w : "?", txt);
        CloseHandle(g_out);
    }
    start_panel(txt, dmp);
    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

void crash_load_config(const char *ini_path)
{
    g_crash_enabled = GetPrivateProfileIntA("debug", "crash_reports", 1, ini_path);
    g_crash_test = GetPrivateProfileIntA("debug", "crash_test", 0, ini_path);
}

/* [debug] crash_test=1: an access violation on purpose when PLAY is pressed, to try the panel. */
void crash_test_now(void)
{
    if (!g_crash_test)
        return;
    logf_("crash test: crashing on purpose (crash_test=1 in ron_camera.ini)");
    *(volatile int *)(UINT_PTR)0x10 = 1;
}

/* At DLL load, and again at the display's start and at each game start: the game or a driver
   may set a filter of its own later, which would take the crash instead. */
void crash_install(void)
{
    if (!g_crash_enabled)
        return;
    LPTOP_LEVEL_EXCEPTION_FILTER prev = SetUnhandledExceptionFilter(crash_filter);
    if (prev != crash_filter)
        logf_("crash reports on%s", prev ? " (it replaced a filter set since)" : "");
}
