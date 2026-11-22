/*
 * End of game statistics: on the Military tab the star for Units Lost, Buildings Lost and
 * Cities Lost goes to the lowest number instead of the highest.
 *
 * AchieveChartPane::init builds one AchieveChartEntry (0x30 bytes) per column of every tab
 * (tab t = interface.xml's <CATEGORIES id="achieve_chart_t">, the columns in their order,
 * which is also the order AchieveChartPane::set_chart_data fills them). Entry +0x2E holds
 * flags: 1 = the lowest value is the best (the game sets it only for the age times on tab
 * 4), 2 = percent, 4 = a time, 8 = no star. AchieveChartPane::on_redraw keeps the best value
 * of each column in +0x28 and the number of players tied at it in +0x2F, and draws the star
 * beside every player at the best value while fewer than all players are tied.
 *
 * - The call to AchieveChartEntry::calc_min_width at the end of init's column loop (with
 *   the loop's tab in [ebp-0x3C] and column in [ebp-0x1C]) goes through hk_min_width, which
 *   sets flag 1 on tab 2's columns 3, 6 and 9.
 * - on_redraw's "lowest" branch never counted ties (`jge` past the count), so a column where
 *   every player lost 0 would star them all. The `jge` now lands on the "highest" branch's
 *   `jne skip / inc [+0x2F]`, with the flags of the same compare: a tie adds to the count,
 *   and a column where everyone is equal gets no star, as for the other columns. The age
 *   times share that branch, so the same rule now holds for them.
 *
 * Display only, so it is multiplayer-safe.
 */
#include "ron.h"
#include <string.h>

#define VA_CALL_MIN_WIDTH 0x007F6909u /* AchieveChartPane::init: call AchieveChartEntry::calc_min_width */
#define VA_MIN_WIDTH      0x007F69F0u /* AchieveChartEntry::calc_min_width(Font *), thiscall, ret 4 */
#define VA_LOWEST_JGE     0x007F74D5u /* on_redraw: cmp ecx,[eax+0x28] / jge +0x14 (the lowest branch) */

#define ENTRY_FLAGS     0x2E
#define FLAG_LOWEST     1
#define TAB_MILITARY    2

int g_endstats_enabled;
__attribute__((used)) UINT_PTR g_endstats_min_width;

void endstats_load_config(const char *ini_path)
{
    g_endstats_enabled = GetPrivateProfileIntA("ui", "fewest_losses_star", 1, ini_path);
}

__attribute__((used)) void c_endstats_entry(unsigned char *entry, int tab, int col)
{
    /* units_lost, buildings_lost, cities_lost */
    if (tab == TAB_MILITARY && (col == 3 || col == 6 || col == 9))
        entry[ENTRY_FLAGS] |= FLAG_LOWEST;
}

/* thiscall calc_min_width(font), from inside init's frame (ebp = init's) */
__asm__(".intel_syntax noprefix\n"
        "_hk_min_width:\n"
        "    push ecx\n"
        "    push dword ptr [ebp-0x1C]\n"
        "    push dword ptr [ebp-0x3C]\n"
        "    push ecx\n"
        "    call _c_endstats_entry\n"
        "    add esp, 12\n"
        "    pop ecx\n"
        "    jmp dword ptr [_g_endstats_min_width]\n"
        ".att_syntax prefix\n");
void hk_min_width(void);

/* Called from install_hooks once the exe is known to be the supported version. */
void endstats_install(void)
{
    if (!g_endstats_enabled)
        return;
    unsigned char *call = (unsigned char *)VA(VA_CALL_MIN_WIDTH);
    unsigned char *jge = (unsigned char *)VA(VA_LOWEST_JGE);
    static const unsigned char cmp_jge[] = { 0x3B, 0x48, 0x28, 0x7D, 0x14 };
    if (call[0] != 0xE8 || (UINT_PTR)(call + 5 + *(INT32 *)(call + 1)) != VA(VA_MIN_WIDTH) ||
        memcmp(jge - 3, cmp_jge, sizeof cmp_jge) != 0) {
        logf_("endstats: code mismatch, fewest losses star turned off");
        g_endstats_enabled = 0;
        return;
    }
    g_endstats_min_width = VA(VA_MIN_WIDTH);
    DWORD old;
    VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(call + 1) = (INT32)((UINT_PTR)hk_min_width - (UINT_PTR)(call + 5));
    VirtualProtect(call, 5, old, &old);
    VirtualProtect(jge, 2, PAGE_EXECUTE_READWRITE, &old);
    jge[1] = 0x0F; /* jge 0x007F74E6: jne skip / inc byte [eax+0x2F] */
    VirtualProtect(jge, 2, old, &old);
    logf_("endstats: the fewest losses get the star");
}
