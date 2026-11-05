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
 *
 * How many dead the map holds (corpse_limit): Objects::init 0x0065EA80 (from Game::init_data,
 * every new game) allocates the DeathObj array with 100 entries of 0xA4 bytes and never grows
 * it; Objects::add_death 0x00653B60 reuses the oldest non-blocking one when all are taken, so
 * in a big battle the oldest dead vanish early. The 100 is written six times in Objects::init:
 *   0x0065F1DB  cmp dword [edi+0x144], 0x64   capacity already big enough? (imm8: a stub)
 *   0x0065F22B  mov dword [edi+0x144], 0x64   the new capacity
 *   0x0065F235  mov eax, 0x64                 times 0xA4 = the bytes to allocate
 *   0x0065F26F  push 0x64 / lea esi,[eax+4] / mov dword [eax], 0x64 / push 0xa4 / push esi
 *               (17 bytes: the count for the constructor loop and the delete[] cookie; the
 *               push is imm8, so the block is rewritten with `push dword [eax]`)
 *   0x0065F295  mov dword [edi+0x140], 0x64   the count add_death, inc_time and the checksums use
 * The limit is simulation state too (which dead a new one replaces).
 */
#include "ron.h"
#include <string.h>

#define VA_INC_AGE   0x008D5384u /* DeathObj::inc_time: inc dword [edi+0x40] / cmp dword [edi+0x44], -1 */
#define VA_GAME_PTR  0x00C061ECu /* Game *: +0x550 game time */
#define GAME_TIME    0x550

static int g_percent = 667;
static int g_limit = 400;
UINT_PTR g_corpse_back;
UINT_PTR g_limit_back;
int g_corpse_limit = 400;

void corpses_load_config(const char *ini_path)
{
    g_percent = GetPrivateProfileIntA("gameplay", "corpse_time_percent", 667, ini_path);
    if (g_percent < 100 || g_percent > 1000)
        g_percent = g_percent < 100 ? 100 : 1000;
    g_limit = GetPrivateProfileIntA("gameplay", "corpse_limit", 400, ini_path);
    if (g_limit < 100 || g_limit > 2000)
        g_limit = g_limit < 100 ? 100 : 2000;
    g_corpse_limit = g_limit;
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

/* in place of `cmp dword [edi+0x144], 0x64` (imm8, too small for the limit); the two movs
   after it leave the flags to the `jge` at 0x0065F1F0. */
__asm__(".intel_syntax noprefix\n"
        "_hk_corpse_cap:\n"
        "    push eax\n"
        "    mov eax, dword ptr [_g_corpse_limit]\n"
        "    cmp dword ptr [edi+0x144], eax\n"
        "    pop eax\n"
        "    jmp dword ptr [_g_limit_back]\n"
        ".att_syntax prefix\n");
void hk_corpse_cap(void);

static void patch(unsigned char *at, const void *bytes, size_t n)
{
    DWORD old;
    VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, n);
    VirtualProtect(at, n, old, &old);
}

static void corpses_install_limit(void)
{
    if (g_limit == 100)
        return;
    static const unsigned char s_cmp[7]  = { 0x83, 0xBF, 0x44, 0x01, 0x00, 0x00, 0x64 };
    static const unsigned char s_cap[10] = { 0xC7, 0x87, 0x44, 0x01, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00 };
    static const unsigned char s_mul[5]  = { 0xB8, 0x64, 0x00, 0x00, 0x00 };
    static const unsigned char s_ctor[17] = { 0x6A, 0x64, 0x8D, 0x70, 0x04, 0xC7, 0x00, 0x64, 0x00,
                                              0x00, 0x00, 0x68, 0xA4, 0x00, 0x00, 0x00, 0x56 };
    static const unsigned char s_cnt[10] = { 0xC7, 0x87, 0x40, 0x01, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00 };
    unsigned char *cmp = (unsigned char *)VA(0x0065F1DB), *cap = (unsigned char *)VA(0x0065F22B),
                  *mul = (unsigned char *)VA(0x0065F235), *ctor = (unsigned char *)VA(0x0065F26F),
                  *cnt = (unsigned char *)VA(0x0065F295);
    if (memcmp(cmp, s_cmp, 7) || memcmp(cap, s_cap, 10) || memcmp(mul, s_mul, 5) ||
        memcmp(ctor, s_ctor, 17) || memcmp(cnt, s_cnt, 10)) {
        logf_("corpses: signature mismatch in Objects::init, the map keeps the game's 100 dead");
        return;
    }
    INT32 n = g_limit;
    unsigned char b[17];

    g_limit_back = (UINT_PTR)(cmp + 7);
    b[0] = 0xE9;
    *(INT32 *)(b + 1) = (INT32)((UINT_PTR)hk_corpse_cap - (UINT_PTR)(cmp + 5));
    b[5] = 0x90;
    b[6] = 0x90;
    patch(cmp, b, 7);

    patch(cap + 6, &n, 4);
    patch(mul + 1, &n, 4);
    patch(cnt + 6, &n, 4);

    /* mov dword [eax], n / push dword [eax] / lea esi, [eax+4] / push 0xa4 / push esi */
    b[0] = 0xC7; b[1] = 0x00; memcpy(b + 2, &n, 4);
    b[6] = 0xFF; b[7] = 0x30;
    b[8] = 0x8D; b[9] = 0x70; b[10] = 0x04;
    b[11] = 0x68; b[12] = 0xA4; b[13] = 0x00; b[14] = 0x00; b[15] = 0x00;
    b[16] = 0x56;
    patch(ctor, b, 17);
    logf_("corpses: the map keeps the last %d dead (the game: 100)", g_limit);
}

/* Called from install_hooks once the exe is known to be the supported version. */
void corpses_install(void)
{
    corpses_install_limit();
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
