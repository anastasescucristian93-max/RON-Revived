/*
 * The game's desync logs keep every line (issue #51).
 *
 * SyncLogger::writeToFileAndReset writes the "SyncLog <kind> <id>.txt" files with a
 * std::wofstream in the C locale. A character the locale cannot convert (above 0xFF, such as an
 * emoji in a Steam name) puts the stream in its fail state and every later write is lost: with a
 * player named "rj ⚡" all four files stopped at "Name: rj", before the first turn, on both
 * machines (2026-11-25), so the checksums that name the first diverging part were never written.
 *
 * Each log line goes through std::operator<<(wostream &, const wstring &) 0x004F7450 (ecx = the
 * stream, edx = the string), called twice in writeToFileAndReset: once for a line's header and
 * once for its text. Both calls are pointed at a stub that turns every character above 0xFF in
 * that temporary string into '?', then goes on to the game's function. Only the log files change.
 */
#include "ron.h"
#include <string.h>

#define VA_WSTR_OUT   0x004F7450u /* std::operator<<(std::wostream &, const std::wstring &), ecx / edx */
#define CALL_HEADER   0x00A2FB17u /* writeToFileAndReset: << line.cHeaderString() */
#define CALL_TEXT     0x00A2FBDBu /* writeToFileAndReset: << line.cString() */

UINT_PTR g_synclog_out;
static int g_cleaned;

/* MSVC std::wstring: 8 wchar_t inline or a pointer, size at +0x10, capacity at +0x14 */
__attribute__((used)) void c_synclog_clean(char *ws)
{
    if (!ws)
        return;
    unsigned n = *(unsigned *)(ws + 0x10), cap = *(unsigned *)(ws + 0x14);
    unsigned short *p = cap >= 8 ? *(unsigned short **)ws : (unsigned short *)ws;
    if (!p || n > 0x100000)
        return;
    for (unsigned i = 0; i < n; i++)
        if (p[i] > 0xFF) {
            p[i] = '?';
            g_cleaned++;
        }
}

void hk_synclog_out(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_synclog_out\n"
        "_hk_synclog_out:\n" HOOK_ENTER
        "    push dword ptr [ebx+20]\n" /* edx: the string */
        "    call _c_synclog_clean\n"
        "    add esp, 4\n" HOOK_LEAVE
        "    jmp dword ptr [_g_synclog_out]\n"
        ".att_syntax prefix\n");

static int retarget(UINT_PTR at_va, void *to)
{
    unsigned char *at = (unsigned char *)VA(at_va);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5) + *(int *)(at + 1) != VA(VA_WSTR_OUT))
        return 0;
    unsigned char code[5] = { 0xE8 };
    *(int *)(code + 1) = (int)((UINT_PTR)to - (UINT_PTR)(at + 5));
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, code, 5);
    VirtualProtect(at, 5, old, &old);
    return 1;
}

/* Called from install_hooks once the exe is known to be the supported version. */
void synclog_install(void)
{
    g_synclog_out = VA(VA_WSTR_OUT);
    int n = retarget(CALL_HEADER, (void *)hk_synclog_out) + retarget(CALL_TEXT, (void *)hk_synclog_out);
    if (n == 2)
        logf_("sync log: names with emoji or other wide letters no longer cut the desync logs short");
    else
        logf_("sync log: signature mismatch in SyncLogger::writeToFileAndReset (%d of 2 calls), logs as they were", n);
}
