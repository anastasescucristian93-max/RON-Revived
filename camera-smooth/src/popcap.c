/*
 * Population limits above 200 (the user, 2026-11-25): the game setup's Population box offers 300,
 * 400 and 500 after the game's 50 ... 200, and the Military research at the Library scales with the
 * chosen limit, so every level gives an eighth of it as at 200 (25, 50, ... 200).
 *
 *  - The choices are data: rules.xml <CATEGORIES id="poplimits"> (romania/tools/build_data.py adds
 *    300, 400, 500 after 200, so the game's indices 0..5 keep their values in saved setups).
 *    Two fixed counts become 9: Game::init_rules_data loads only that many entries
 *    (Categories::init(6, ...), push 6 at 0x0058F7B9) and SetupWin::setup_combos fills the box
 *    with that many (push 6 at 0x005C0264).
 *  - Leader::calc_pop_cap 0x006DC490 takes the base from POP_CAP (Constants +0x3C4, 8 ints, one per
 *    Military level: 25 ... 200) and clamps to the chosen limit. Above POP_CAP[7] the game has an
 *    odd rule of its own (+25 per level from level 4 and the limit itself at level 7). Before the
 *    game's function runs, datacenter.c's calc_pop_cap hook calls popcap_prepare: for a limit L
 *    above 200, POP_CAP[i] = L * (i + 1) / 8 (so POP_CAP[7] = L and the odd rule never applies);
 *    for 200 or less the game's own values. Only calc_pop_cap reads POP_CAP.
 *  - The limit is the same for every player of a game, so the result is the same on every machine,
 *    but players on older versions have no entries 6..8: every player needs this version.
 * Engine notes: data-center/ENGINE_NOTES.md, "Population cap".
 */
#include "ron.h"
#include <string.h>

#define VA_POP_COMBO     0x005C0264u /* SetupWin::setup_combos: push 6 (the pop_limits count) */
#define VA_POP_LOAD      0x0058F7B9u /* Game::init_rules_data: push 6, Categories::init(6, xml, "poplimits") */
#define VA_GAME_PTR      0x00C061ECu /* Game *: +0x31 byte, the chosen pop_limits index */
#define VA_CONSTANTS_PTR 0x00C061F0u /* Constants *: +0x3C4 POP_CAP[8] */
#define VA_POP_LIMITS    0x00E80030u /* pop_limits (Categories) data: 0x58 per entry, +0x3C its value */
#define C_POP_CAP        0x3C4
#define POP_CHOICES      9           /* 50 75 100 125 150 200 300 400 500 */

int g_popcap_enabled;
static int g_orig_caps[8], g_have_orig, g_last_limit = -1;

void popcap_load_config(const char *ini_path)
{
    g_popcap_enabled = GetPrivateProfileIntA("gameplay", "pop_limits_500", 1, ini_path);
}

/* The chosen population limit, or -1. */
static int chosen_limit(void)
{
    char *game = *(char **)VA(VA_GAME_PTR), *cats = *(char **)VA(VA_POP_LIMITS);
    if (!game || !cats)
        return -1;
    int i = *(unsigned char *)(game + 0x31);
    if (i >= POP_CHOICES)
        return -1;
    return *(int *)(cats + i * 0x58 + 0x3C);
}

/* From datacenter.c's calc_pop_cap hook, before the game's function. */
void popcap_prepare(void)
{
    if (!g_popcap_enabled)
        return;
    char *c = *(char **)VA(VA_CONSTANTS_PTR);
    if (!c)
        return;
    int *caps = (int *)(c + C_POP_CAP);
    if (!g_have_orig) {
        memcpy(g_orig_caps, caps, sizeof g_orig_caps);
        g_have_orig = 1;
    }
    int limit = chosen_limit();
    if (limit > g_orig_caps[7]) {
        for (int i = 0; i < 8; i++)
            caps[i] = (limit * (i + 1) + 4) / 8;
    } else
        memcpy(caps, g_orig_caps, sizeof g_orig_caps);
    if (limit != g_last_limit) {
        g_last_limit = limit;
        logf_("pop limits: limit %d, Military levels %d .. %d", limit, caps[0], caps[7]);
    }
}

void popcap_install(void)
{
    if (!g_popcap_enabled)
        return;
    /* setup_combos: push offset pop_limits (relocated, so compared as an address) / push 6;
       init_rules_data: mov ecx, offset pop_limits (0x0058F7A7) ... push 6 / call Categories::init */
    unsigned char *at = (unsigned char *)VA(VA_POP_COMBO) - 5, *load = (unsigned char *)VA(VA_POP_LOAD);
    if (at[0] != 0x68 || *(UINT_PTR *)(at + 1) != VA(0x00E80020u) || at[5] != 0x6A || at[6] != 0x06 ||
        *(unsigned char *)VA(0x0058F7A7u) != 0xB9 || *(UINT_PTR *)VA(0x0058F7A8u) != VA(0x00E80020u) ||
        load[0] != 0x6A || load[1] != 0x06 || load[2] != 0xE8) {
        logf_("pop limits: signature mismatch at setup_combos / init_rules_data, the game's limits stay");
        g_popcap_enabled = 0;
        return;
    }
    DWORD old;
    VirtualProtect(at + 6, 1, PAGE_EXECUTE_READWRITE, &old);
    at[6] = POP_CHOICES;
    VirtualProtect(at + 6, 1, old, &old);
    VirtualProtect(load + 1, 1, PAGE_EXECUTE_READWRITE, &old);
    load[1] = POP_CHOICES;
    VirtualProtect(load + 1, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("pop limits: on (up to 500, Military research scaled)");
}
