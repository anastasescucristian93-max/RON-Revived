/*
 * The big announcements (age reached, city under attack, wonders, captures, nukes, defeats:
 * the game's "Notifications") at the top centre of the screen instead of its middle.
 *
 * IFaceMainBase::do_notice(IFaceTex *, Rect *, String, Color) builds the notice's start rect
 * centred on the screen, y = (screen height - icon height) / 2 (`mov eax, [esi+0x364];
 * sub eax, ecx` at 0x0080D91D, then halved). That becomes `call stub` returning 2 * top, so the
 * icon's top is at `top`, lower by 55 while the mini library is open ([0x00ECD114]), like the
 * game's own one-line scrolls above it. The text follows 5 px under the icon.
 * At the end of its time a notice flies to its end rect: the String do_notice puts it off
 * screen at the bottom right (`mov ecx, [eax+0x364]` at 0x0081087F, the y); with `xor ecx, ecx`
 * it leaves to the right along the top instead of across the map and the minimap.
 * Timers and scenario notices use the same start rect, so they move too. Drawing only.
 * Engine notes: ENGINE_NOTES.md, "Centre-screen announcements".
 */
#include "ron.h"
#include <string.h>

#define VA_NOTICE_Y     0x0080D91Du /* do_notice: mov eax, [esi+0x364] / sub eax, ecx (8 bytes) */
#define VA_NOTICE_END_Y 0x0081087Fu /* do_notice(String...): mov ecx, [eax+0x364] (6 bytes) */
#define VA_MINI_LIBRARY 0x00ECD114u /* int: the mini library is open */
#define LIBRARY_H       55

int g_notice_top;
UINT_PTR g_notice_lib;
static int g_notice_fly_top;

void notices_load_config(const char *ini_path)
{
    g_notice_top = GetPrivateProfileIntA("ui", "notices_top", 75, ini_path);
    g_notice_fly_top = GetPrivateProfileIntA("ui", "notices_leave_top", 1, ini_path);
}

void hk_notice_y(void);

__asm__(".intel_syntax noprefix\n"
        /* returns eax = 2 * the icon's top; ecx and edx are free, everything else is kept */
        ".globl _hk_notice_y\n"
        "_hk_notice_y:\n"
        "    mov eax, [_g_notice_top]\n"
        "    mov edx, [_g_notice_lib]\n"
        "    cmp dword ptr [edx], 0\n"
        "    je 1f\n"
        "    add eax, 55\n"
        "1:  add eax, eax\n"
        "    ret\n"
        ".att_syntax prefix\n");

static void patch(unsigned char *at, const unsigned char *bytes, int n)
{
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
}

void notices_install(void)
{
    if (g_notice_top <= 0)
        return;
    static const unsigned char y_sig[8] = { 0x8B, 0x86, 0x64, 0x03, 0x00, 0x00, 0x2B, 0xC1 };
    static const unsigned char end_sig[6] = { 0x8B, 0x88, 0x64, 0x03, 0x00, 0x00 };
    unsigned char *y = (unsigned char *)VA(VA_NOTICE_Y), *end = (unsigned char *)VA(VA_NOTICE_END_Y);
    if (memcmp(y, y_sig, sizeof y_sig) != 0 || memcmp(end, end_sig, sizeof end_sig) != 0) {
        logf_("notices: signature mismatch, the announcements stay in the middle");
        return;
    }
    g_notice_lib = VA(VA_MINI_LIBRARY);
    unsigned char call[8] = { 0xE8, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
    int rel = (int)((UINT_PTR)hk_notice_y - (UINT_PTR)(y + 5));
    memcpy(call + 1, &rel, 4);
    patch(y, call, sizeof call);
    if (g_notice_fly_top) {
        static const unsigned char xor_ecx[6] = { 0x31, 0xC9, 0x90, 0x90, 0x90, 0x90 };
        patch(end, xor_ecx, sizeof xor_ecx);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("notices: announcements at the top centre (y %d, +%d with the mini library)%s", g_notice_top, LIBRARY_H,
          g_notice_fly_top ? ", leaving along the top" : "");
}
