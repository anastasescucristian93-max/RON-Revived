/*
 * The game setup remembers the last game (the user, 2026-11-25: "eu vreau sa isi aminteasca
 * setarile din ultimul meci ... sau ultimul lobby facut de mine").
 *
 *  - SetupWin::exec loads the setup from the profile (PlayerProfile::load_into_game 0x005D0450,
 *    call at 0x005C6900) and, when the screen closes, copies the setup back into the profile only
 *    with the Save to Profile box ticked (Game+0x20 bit 0, test at 0x005C81F6): otherwise it just
 *    clears the profile's bit 0 and saves. PlayerProfile::update_from_game 0x005D0930 keeps the
 *    rules (Game+0x24..+0x41, one set for single player and one for multiplayer) and the flags
 *    dword (Game+0x20: Cooperative AI, the nukes slider, ...). So with the box unticked the setup
 *    came back every time as it was saved once, long ago (Cooperative AI ticked again, Survival
 *    of the Fittest with old teams).
 *    Here the copy runs every time in single player and for the host of a multiplayer game, as if
 *    the box were ticked; not in the Survival setup (its rules are fixed by survival.c, and it
 *    keeps its own Cooperative AI choice in ron_camera.ini).
 *  - Teams left over: SetupWin::process_combo_change sets every team combo (ids 0x28..0x2F) to
 *    item 8 (no team) when Teams & Allies (id 0x30) becomes Diplomacy 0, Survival of the Fittest 1
 *    or Assassin 2, and the team column then hides. A setup that opens on such a style keeps the
 *    teams it had (shown greyed, 1 1 2 2 3 3 4 4), so nonukes.c's setup_checks hook calls
 *    setup_memory_teams, which does the same.
 * Client-only: the settings travel to the other players as before.
 * Engine notes: camera-smooth/ENGINE_NOTES.md, "Game setup memory".
 */
#include "ron.h"
#include <string.h>

#define VA_SAVE_TEST     0x005C81F1u /* SetupWin::exec: mov eax, [game] / test byte [eax+0x20], 1 / je */
#define VA_SAVE_COPY     0x005C81FCu /* mov ecx, offset player_profile / call update_from_game */
#define VA_SAVE_SKIP     0x005C8208u /* and dword [profile flags], ~1 */
#define VA_GAME_PTR      0x00C061ECu /* Game *: +0x20 the flags dword, bit 0 Save to Profile */
#define VA_NETSYS        0x00E335C8u /* NetSys *: vtable +0x10 is the host */
#define VA_COMBO_CHANGE  0x005BA570u /* SetupWin::make_combo_change(int combo id, int item id) */
#define SW_COMBOS        0x20BC      /* SetupWin: ComboBox * by combo id; +0x568 the item */
#define SW_MODE          0x1F24      /* & 2: single player */
#define ID_TEAM          0x28        /* team combos 0x28..0x2F, one per row */
#define ID_STYLE         0x30        /* Teams & Allies */
#define TEAM_NONE        8

typedef unsigned char(__attribute__((thiscall)) *is_host_t)(void *self);
typedef void(__attribute__((thiscall)) *combo_change_t)(void *sw, int id, int item);

static int g_enabled;
UINT_PTR g_setup_copy, g_setup_skip;

void setup_memory_load_config(const char *ini_path)
{
    g_enabled = GetPrivateProfileIntA("qol", "remember_setup", 1, ini_path);
}

static int may_change(char *sw)
{
    if (sw && (*(unsigned *)(sw + SW_MODE) & 2))
        return 1;
    void *net = *(void **)VA(VA_NETSYS);
    return net && ((is_host_t)(*(void ***)net)[0x10 / 4])(net);
}

/* At the test (esi = the SetupWin): 1 = copy the setup into the profile. */
__attribute__((used)) int c_setup_save(char *sw)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    if (!game)
        return 0;
    if (*(unsigned *)(game + 0x20) & 1) /* Save to Profile ticked: the game's own way */
        return 1;
    if (survival_setup_active())
        return 0;
    int save = may_change(sw);
    if (save)
        logf_("setup memory: the setup kept for the next game");
    return save;
}

void hk_setup_save(void);
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_setup_save\n_hk_setup_save:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push esi\n" /* the SetupWin (exec's this) */
        "    call _c_setup_save\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE /* eax as popad restores it */
        "    test eax, eax\n"
        "    jz 1f\n"
        "    jmp dword ptr [_g_setup_copy]\n"
        "1:  jmp dword ptr [_g_setup_skip]\n"
        ".att_syntax prefix\n");

/* nonukes.c's setup_checks hook, once the screen is built: no teams left over on a style
   without teams. */
void setup_memory_teams(char *sw)
{
    if (!g_enabled || !sw || !may_change(sw))
        return;
    char *style = *(char **)(sw + SW_COMBOS + 4 * ID_STYLE);
    if (!style || *(int *)(style + 0x568) > 2)
        return;
    int cleared = 0;
    for (int i = 0; i < 8; i++) {
        char *team = *(char **)(sw + SW_COMBOS + 4 * (ID_TEAM + i));
        if (team && *(int *)(team + 0x568) != TEAM_NONE) {
            ((combo_change_t)VA(VA_COMBO_CHANGE))(sw, ID_TEAM + i, TEAM_NONE);
            cleared++;
        }
    }
    if (cleared)
        logf_("setup memory: %d team(s) left over from an older setup cleared (no teams in this style)", cleared);
}

void setup_memory_install(void)
{
    if (!g_enabled)
        return;
    static const unsigned char sig[11] = { 0xA1, 0xEC, 0x61, 0xC0, 0x00, 0xF6, 0x40, 0x20, 0x01, 0x74, 0x0C };
    unsigned char *at = (unsigned char *)VA(VA_SAVE_TEST);
    unsigned char want[11];
    memcpy(want, sig, sizeof want);
    *(UINT_PTR *)(want + 1) = VA(VA_GAME_PTR);
    if (memcmp(at, want, sizeof want) != 0) {
        logf_("setup memory: signature mismatch at SetupWin::exec, the game's Save to Profile only");
        g_enabled = 0;
        return;
    }
    g_setup_copy = VA(VA_SAVE_COPY);
    g_setup_skip = VA(VA_SAVE_SKIP);
    hook_jmp5(at, hk_setup_save);
    DWORD old;
    VirtualProtect(at + 5, 6, PAGE_EXECUTE_READWRITE, &old);
    memset(at + 5, 0x90, 6);
    VirtualProtect(at + 5, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("setup memory: on (the setup kept from the last game, teams cleared on styles without teams)");
}
