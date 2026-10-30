/*
 * Zero-length animations no longer freeze the game (camera-smooth/ENGINE_NOTES.md, "Zero-length
 * animations"). Always on: it only acts where the game would freeze.
 *
 * Guy::inc_time repeats "while time >= length: next animation", and a walking guy away from its
 * spot only restarts the walk (time = 0), so a length of 0 loops for ever (the game froze
 * 2026-10-31: an AI Artillery, CHAR_WALK, length 0). The loop's test at 0x005DA081 gives such a
 * guy the game's own length for a missing animation, 3.
 */
#include "ron.h"
#include <string.h>

#define VA_ANIM_LOOP     0x005DA081u /* Guy::inc_time: mov eax,[esi+0x74] / cmp eax,[esi+0x78] / jae (esi = the guy) */
#define VA_ANIM_AGAIN    0x005D9F90u
#define VA_ANIM_DONE     0x005DA08Du
#define VA_UNITS         0x00C0AEC0u /* per player, 0x1C apart: units by id */

UINT_PTR g_af_anim_again, g_af_anim_done;

/* a guy's animation lasts 0: give it 3 (the game's length for a missing one); the first few are
   logged so the model behind it can be found (guy +0x9C animation, +0xA1 owner, +0x8C unit id,
   +0x88 model) */
__attribute__((used)) void c_af_zero_anim(char *guy)
{
    static int logged;
    *(int *)(guy + 0x78) = 3;
    if (logged >= 5)
        return;
    logged++;
    int p = guy[0xA1], id = *(short *)(guy + 0x8C), type = -1;
    char **units = p >= 0 && p < 9 && id >= 0 ? *(char ***)VA(VA_UNITS + 0x1C * p) : NULL;
    char *unit = units ? units[id] : NULL;
    if (unit && *(char **)(unit + 0x18))
        type = *(int *)(*(char **)(unit + 0x18) + 4);
    logf_("anim fix: animation %d of player %d unit %d (type %d, model %d) lasted 0, set to 3 so the game does not freeze",
          guy[0x9C], p, id, type, *(int *)(guy + 0x88));
}

__asm__(".intel_syntax noprefix\n"
        /* in place of the loop's test; esi = the guy */
        "_hk_af_anim_loop:\n"
        "    cmp dword ptr [esi+0x78], 0\n"
        "    jne 1f\n"
        "    pushad\n"
        "    push esi\n"
        "    call _c_af_zero_anim\n"
        "    add esp, 4\n"
        "    popad\n"
        "1:  mov eax, dword ptr [esi+0x74]\n"
        "    cmp eax, dword ptr [esi+0x78]\n"
        "    jb 2f\n"
        "    jmp dword ptr [_g_af_anim_again]\n"
        "2:  jmp dword ptr [_g_af_anim_done]\n"
        ".att_syntax prefix\n");
void hk_af_anim_loop(void);

/* Called from install_hooks once the exe is known to be the supported version. */
void animfix_install(void)
{
    static const unsigned char sig[12] = { 0x8B, 0x46, 0x74, 0x3B, 0x46, 0x78, 0x0F, 0x83, 0x03, 0xFF, 0xFF, 0xFF };
    unsigned char *at = (unsigned char *)VA(VA_ANIM_LOOP);
    if (memcmp(at, sig, sizeof sig)) {
        logf_("anim fix: Guy::inc_time's animation loop does not match, skipped");
        return;
    }
    g_af_anim_again = VA(VA_ANIM_AGAIN);
    g_af_anim_done = VA(VA_ANIM_DONE);
    unsigned char code[sizeof sig];
    memset(code, 0x90, sizeof code);
    code[0] = 0xE9;
    *(int *)(code + 1) = (int)((UINT_PTR)hk_af_anim_loop - (UINT_PTR)(at + 5));
    DWORD old;
    VirtualProtect(at, sizeof code, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, code, sizeof code);
    VirtualProtect(at, sizeof code, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("anim fix: zero-length animations cannot freeze the game");
}
