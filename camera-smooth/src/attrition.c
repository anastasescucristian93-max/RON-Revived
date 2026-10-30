/*
 * Stronger attrition: units in enemy territory without supply lose hit points more often.
 *
 * Unit::process_attrition sets the frames until the unit's next attrition hit ([unit+0x9E])
 * to ATTRITION (rules.xml, 48 frames, Constants+0xD3C) x UnitData::get_attrition / 256, at
 * least 1. Unit::suffer_attrition then takes 1 hit point (a squad: 16 / soldiers sixteenths).
 * The hook divides the ATTRITION baseline by the strength: 150 % = a hit every 32 frames
 * instead of 48. Supply Wagons, Forage / Supply / Logistics, Mongols, Russians, the Kremlin
 * and the Statue of Liberty keep their effects, which are factors on the same interval.
 * Peace-time border attrition and the assassin attrition (their own constants) stay as they are.
 * The rule runs in the simulation on every machine: every player needs the same setting.
 */
#include "ron.h"
#include <string.h>

#define VA_LOAD_ATTRITION 0x005E1942u /* process_attrition: mov eax, [eax+0xD3C] (6 bytes) */

int g_attr_percent;
UINT_PTR g_attr_back;

void attrition_load_config(const char *ini_path)
{
    g_attr_percent = GetPrivateProfileIntA("gameplay", "attrition_percent", 150, ini_path);
}

void hk_attrition(void);

__asm__(".intel_syntax noprefix\n"
        /* eax = Constants; ecx (the unit's attrition factor) must be kept */
        ".globl _hk_attrition\n"
        "_hk_attrition:\n"
        "    mov eax, [eax+0xD3C]\n"
        "    imul eax, eax, 100\n"
        "    cdq\n"
        "    idiv dword ptr [_g_attr_percent]\n"
        "    cmp eax, 1\n"
        "    jge 1f\n"
        "    mov eax, 1\n"
        "1:  jmp dword ptr [_g_attr_back]\n"
        ".att_syntax prefix\n");

void attrition_install(void)
{
    if (g_attr_percent <= 0 || g_attr_percent == 100)
        return;
    static const unsigned char sig[6] = { 0x8B, 0x80, 0x3C, 0x0D, 0x00, 0x00 };
    unsigned char *at = (unsigned char *)VA(VA_LOAD_ATTRITION);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("attrition: signature mismatch, the game's attrition stays");
        return;
    }
    g_attr_back = (UINT_PTR)(at + 6);
    hook_jmp5(at, hk_attrition);
    DWORD old;
    VirtualProtect(at + 5, 1, PAGE_EXECUTE_READWRITE, &old);
    at[5] = 0x90;
    VirtualProtect(at + 5, 1, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("attrition: %d %% of the game's strength", g_attr_percent);
}
