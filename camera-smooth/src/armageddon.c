/*
 * No Armageddon defeat. In the game, when enough nuclear weapons have gone off (the
 * Armageddon clock reaches zero) everyone loses. Here the game goes on instead, and from
 * then on nobody can build or launch nuclear weapons.
 *
 *  - GameDaemon::process_victory calls Game::defeat_all when game+0x6E0 (nukes so far)
 *    reaches Game::get_armageddon(); its `jl` over that call becomes a `jmp`.
 *  - LeaderData::can_nuke (the launch command's check) says no once the clock is out, and
 *    with a count chosen in the setup (nonukes.c) once the nukes in the air would use it up.
 *  - Nuclear Missile / Nuclear ICBM are offered disabled (datacenter.c's type_eligible hook
 *    asks arm_type_blocked).
 * The rule runs in the simulation on every machine: every player needs the same setting.
 */
#include "ron.h"
#include <string.h>

#define VA_VICTORY_JL   0x00730F1Fu /* GameDaemon::process_victory: jl over defeat_all */
#define VA_CAN_NUKE     0x006D53C0u /* LeaderData::can_nuke(), returns int */
#define VA_ARMAGEDDON   0x00594020u /* Game::get_armageddon(): nukes allowed */
#define VA_GAME_PTR     0x00C061ECu
#define GAME_NUKES      0x6E0
#define NUCLEAR_MISSILE 0x13B /* unit types (unitrules.xml 265, 266) */
#define NUCLEAR_ICBM    0x13C

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int (*get_armageddon_t)(void);

int g_arm_enabled;
void *g_orig_can_nuke;

void armageddon_load_config(const char *ini_path)
{
    g_arm_enabled = GetPrivateProfileIntA("gameplay", "no_armageddon", 1, ini_path);
}

static int clock_out(void)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    return game && *(int *)(game + GAME_NUKES) >= ((get_armageddon_t)VA(VA_ARMAGEDDON))();
}

int arm_type_blocked(int type)
{
    return g_arm_enabled && (type == NUCLEAR_MISSILE || type == NUCLEAR_ICBM) && clock_out();
}

__attribute__((used)) int c_can_nuke(void *self)
{
    /* nonukes.c: a count chosen in the setup counts the nukes in the air too */
    return clock_out() || nonukes_launch_full() ? 0 : ((fn0_t)g_orig_can_nuke)(self);
}

void hk_can_nuke(void);

__asm__(".intel_syntax noprefix\n"
        ".globl _hk_can_nuke\n"
        "_hk_can_nuke:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_can_nuke\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret\n"
        ".att_syntax prefix\n");

void armageddon_install(void)
{
    if (!g_arm_enabled)
        return;
    unsigned char *jl = (unsigned char *)VA(VA_VICTORY_JL), *cn = (unsigned char *)VA(VA_CAN_NUKE);
    /* jl +0x0C | push 0 / push 0x13B */
    static const unsigned char cn_sig[7] = { 0x6A, 0x00, 0x68, 0x3B, 0x01, 0x00, 0x00 };
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem || jl[0] != 0x7C || jl[1] != 0x0C || memcmp(cn, cn_sig, sizeof cn_sig) != 0) {
        logf_("armageddon: signature mismatch, the game's Armageddon rule stays");
        g_arm_enabled = 0;
        return;
    }
    DWORD old;
    VirtualProtect(jl, 1, PAGE_EXECUTE_READWRITE, &old);
    jl[0] = 0xEB;
    VirtualProtect(jl, 1, old, &old);
    g_orig_can_nuke = hook_trampoline(cn, 7, mem);
    hook_jmp5(cn, hk_can_nuke);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("armageddon: no defeat; nuclear weapons stop when the clock runs out");
}
