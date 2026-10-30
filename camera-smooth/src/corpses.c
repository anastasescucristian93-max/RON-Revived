/*
 * Longer-lasting dead: fallen soldiers and animals, tank and vehicle wrecks and the scorch or
 * blood marks under them stay on the ground longer before they fade (corpse_time_percent).
 *
 * Every dead unit is a DeathObj. DeathObj::inc_time 0x008D5240 (from Objects::inc_time, once
 * per game frame) counts its age in +0x40 and removes it when the age reaches
 *   anim + body_fade_end   (int 0x00B2176C = 135)  without corpse art (+0x44 == -1)
 *   anim + corpse_fade_end (int 0x00B21774 = 627)  with corpse art (wrecks, remains)
 *   anim                                           types with +0x218 == 1
 * where anim = AnimationPacket::get_game_frames of the death animation (ebx there).
 * DeathObjOut::draw / draw_underlay fade with spans written into the code: the body fades
 * out from anim + 105 (`lea eax, [esi + 0x69]`) to body_fade_end over 30 frames, the corpse
 * art fades in over the first 105 frames, holds to corpse_hold_end (int 0x00B21770 = 582)
 * and fades out over 45. Raising the ends alone would push the alpha above 255.
 *
 * So the age itself runs slower once the death animation is over: the `inc dword [edi+0x40]`
 * at 0x008D5384 goes to a stub that counts a frame only when game time [game+0x550] crosses
 * a step of percent / 100 frames. The animation plays as before, every later stage (fades,
 * bleeding, removal) takes percent / 100 times as long.
 *
 * Tank wrecks (FLAGS `x`, type +0x2B4 & 0x800000, UnitTypeData::blocks_while_dead) block
 * the ground until they are removed (DeathObj::clear_blocking), so they block longer too.
 * DeathObjs are part of the simulation (CheckSums::check_deaths): every player of a
 * multiplayer game needs the same setting.
 */
#include "ron.h"
#include <string.h>

#define VA_INC_AGE   0x008D5384u /* DeathObj::inc_time: inc dword [edi+0x40] / cmp dword [edi+0x44], -1 */
#define VA_GAME_PTR  0x00C061ECu /* Game *: +0x550 game time */
#define GAME_TIME    0x550

static int g_percent = 200;
UINT_PTR g_corpse_back;

void corpses_load_config(const char *ini_path)
{
    g_percent = GetPrivateProfileIntA("gameplay", "corpse_time_percent", 200, ini_path);
    if (g_percent < 100 || g_percent > 1000)
        g_percent = g_percent < 100 ? 100 : 1000;
}

/* 1 when the dead object ages this frame: always during the death animation, after it once
   per percent / 100 frames of game time. */
__attribute__((used)) int c_corpse_ages(int age, int anim)
{
    if (age < anim)
        return 1;
    char *game = *(char **)VA(VA_GAME_PTR);
    if (!game)
        return 1;
    unsigned t = (unsigned)*(int *)(game + GAME_TIME);
    return t * 100u / (unsigned)g_percent != (t - 1u) * 100u / (unsigned)g_percent;
}

/* in place of `inc dword [edi+0x40]; cmp dword [edi+0x44], -1`: edi = the DeathObj, ebx = the
   death animation's frames; eax, ecx and edx are free (reloaded after). The flags of the cmp go
   back to the `je` at 0x008D538B. */
__asm__(".intel_syntax noprefix\n"
        "_hk_corpse_age:\n"
        "    push ebx\n"
        "    push dword ptr [edi+0x40]\n"
        "    call _c_corpse_ages\n"
        "    add esp, 8\n"
        "    test eax, eax\n"
        "    jz 1f\n"
        "    inc dword ptr [edi+0x40]\n"
        "1:\n"
        "    cmp dword ptr [edi+0x44], -1\n"
        "    jmp dword ptr [_g_corpse_back]\n"
        ".att_syntax prefix\n");
void hk_corpse_age(void);

/* Called from install_hooks once the exe is known to be the supported version. */
void corpses_install(void)
{
    if (g_percent == 100)
        return;
    static const unsigned char sig[9] = { 0xFF, 0x47, 0x40, 0x83, 0x7F, 0x44, 0xFF, 0x74, 0x0C };
    unsigned char *at = (unsigned char *)VA(VA_INC_AGE);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("corpses: signature mismatch in DeathObj::inc_time, dead units fade as in the game");
        return;
    }
    g_corpse_back = (UINT_PTR)(at + 7);
    DWORD old;
    VirtualProtect(at, 7, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE9;
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_corpse_age - (UINT_PTR)(at + 5));
    at[5] = 0x90;
    at[6] = 0x90;
    VirtualProtect(at, 7, old, &old);
    logf_("corpses: dead units, wrecks and their marks stay %d%% as long", g_percent);
}
