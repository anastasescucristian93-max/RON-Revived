/*
 * Tower and Fort price increase counts the whole line.
 *
 * Every Tower and Fort a player owns should make the next one dearer (buildingrules.xml
 * SUPPORT0/1 + SUPPORTVALUE0/1: +10 Wealth and +25 Metal per Tower, more per Fort). In
 * TypeData::get_cost 0x00664090 the number owned is read for the type being priced only:
 * at 0x00665966 count = word [leader + 0x5A22 + 2 type] (placed, not started) + word
 * [leader + 0x5222 + 2 type] (started or finished), the arrays Wall::increment_stats and
 * Wall::activate keep by the object's own type. Seen in game 2026-11-05 (Chinese, Information
 * Age start): with a finished Bunker and a second one being built the Bunker button still asked
 * the base 30 Wealth / 50 Metal, and the site was named "Constructing Tower", so the towers were
 * counted under one type of the line and the price asked for another. Computer players then
 * covered the map in Towers at the base price.
 *
 * The hook makes the count of any type of the Tower line (Tower 0x1B7, Keep, Stockade, Bunker
 * 0x1BA) the sum over the four, and the same for the Fort line (Fort 0x1BB, Castle, Fortress,
 * Redoubt 0x1BE). The rest of get_cost (the Forts' steeper curve, the Indians' half ramp, the
 * Korean and Roman discounts) is the game's own. It runs in the simulation (the price is
 * charged with it): in multiplayer every player needs the same setting.
 *
 * Computer players laying old levels (seen 2026-11-08, Koreans, Information Age start with All
 * Technologies). Leader::init gives an "All Technologies" player the levels of a line by setting
 * their bits at leader +0x6C18 (0x006E446A), but never marks the lower levels obsolete in the
 * BitMask at +0x6CF4 (bits at +0x6D00), which Leader::gain_tech does when a level is gained in a
 * match (0x006DDEE2 / 0x006DDF56). type_eligible 0x006DBD10 only rejects a building through
 * that mask (0x006DC169), so the plain Tower stayed buildable: the build menu showed the Bunker
 * (UnitOut::make_options asks current_upgrade), the computer players and the Cooperative AI laid
 * Towers (Leader::produce_building 0x006E1400 takes the type as is).
 * The hook at the entry of produce_building: for a type of the Tower or Fort line it asks
 * LeaderData::current_upgrade (the level the menu shows), lays that one instead and marks the
 * levels below it obsolete, as gain_tech would have, so the planner asks for the right level
 * afterwards. Also part of the simulation: every player in multiplayer needs the same setting.
 */
#include "ron.h"
#include <string.h>

#define VA_COUNT_SITE 0x00665966u /* get_cost: movzx edx/eax counts, add, mov [ebp+0x20] (21 bytes) */
#define LD_SIZE       0x6EEC
#define LD_BUILT      0x5222 /* word per type: started or finished */
#define LD_QUEUED     0x5A22 /* word per type: placed or queued, not started */
#define TOWER_FIRST   0x1B7
#define FORT_FIRST    0x1BB
#define LINE_LEN      4

#define VA_PRODUCE_BUILDING 0x006E1400u /* Leader::produce_building(type, unit, x): 9-byte prologue */
#define VA_CURRENT_UPGRADE  0x006E3140u /* LeaderData::current_upgrade(type) */
#define VA_BITMASK_SET      0x00450360u /* BitMask::set(index, on) */
#define VA_LEADERS          0x00E3A390u /* Leaders leaders: LeaderData[8], 0x6EEC each */
#define LD_OBSOLETE         0x6CF4      /* BitMask: building types replaced by a higher level */

int g_ramp_line = 1;
int g_build_current_level = 1;
UINT_PTR g_ramp_back;
UINT_PTR g_upbuild_back;
static int g_upbuild_logs;
static int g_ramp_logs;
static unsigned short g_ramp_last[8][LINE_LEN * 2];

void defenseramp_load_config(const char *ini_path)
{
    g_ramp_line = GetPrivateProfileIntA("gameplay", "defense_ramp_whole_line", 1, ini_path);
    g_build_current_level = GetPrivateProfileIntA("gameplay", "defense_build_current_level", 1, ini_path);
}

static int line_first(int type)
{
    if (type >= TOWER_FIRST && type < TOWER_FIRST + LINE_LEN)
        return TOWER_FIRST;
    if (type >= FORT_FIRST && type < FORT_FIRST + LINE_LEN)
        return FORT_FIRST;
    return -1;
}

/* The count the game's code would read for (p, type), or the line's total. */
__attribute__((used)) int c_ramp_count(int p, int type, char *leaders)
{
    if (p < 0 || p >= 8 || type < 0 || !leaders)
        return 0;
    const unsigned short *built = (const unsigned short *)(leaders + p * LD_SIZE + LD_BUILT);
    const unsigned short *queued = (const unsigned short *)(leaders + p * LD_SIZE + LD_QUEUED);
    int n = built[type] + queued[type];
    int first = line_first(type);
    if (first < 0)
        return n;
    int total = 0;
    for (int t = first; t < first + LINE_LEN; t++)
        total += built[t] + queued[t];

    /* the counts of the Tower line, logged when they change (to see where the game keeps them) */
    if (first == TOWER_FIRST && g_ramp_logs < 300) {
        unsigned short now[LINE_LEN * 2];
        for (int i = 0; i < LINE_LEN; i++) {
            now[2 * i] = built[first + i];
            now[2 * i + 1] = queued[first + i];
        }
        if (memcmp(now, g_ramp_last[p], sizeof now) != 0) {
            memcpy(g_ramp_last[p], now, sizeof now);
            g_ramp_logs++;
            logf_("ramp: player %d, price of type 0x%X; Tower %d+%d, Keep %d+%d, Stockade %d+%d, "
                  "Bunker %d+%d (started+placed); the game counts %d, the line %d",
                  p, type, now[0], now[1], now[2], now[3], now[4], now[5], now[6], now[7], n, total);
        }
    }
    return total;
}

void hk_ramp_count(void);

__asm__(".intel_syntax noprefix\n"
        /* eax = player * 0x3776 + type, edi = LeaderData[0], esi = type, [ebp+0xC] = player.
           The game's code leaves the count in edx and in [ebp+0x20]. */
        ".globl _hk_ramp_count\n"
        "_hk_ramp_count:\n"
        "    push ecx\n"
        "    push edi\n"
        "    push esi\n"
        "    push dword ptr [ebp+0xC]\n"
        "    call _c_ramp_count\n"
        "    add esp, 12\n"
        "    pop ecx\n"
        "    mov edx, eax\n"
        "    mov [ebp+0x20], edx\n"
        "    jmp dword ptr [_g_ramp_back]\n"
        ".att_syntax prefix\n");

typedef int(__attribute__((thiscall)) *current_upgrade_t)(void *leader, int type);
typedef void(__attribute__((thiscall)) *bitmask_set_t)(void *mask, int index, int on);

/* The type produce_building lays for (leader, type): the current level of a Tower or Fort line. */
__attribute__((used)) int c_upbuild_type(char *leader, int type)
{
    int first = line_first(type);
    if (first < 0 || !leader)
        return type;
    int p = *(int *)(leader + 8);
    if (p < 0 || p >= 8)
        return type;
    char *ld = (char *)VA(VA_LEADERS) + p * LD_SIZE;
    int up = ((current_upgrade_t)VA(VA_CURRENT_UPGRADE))(ld, type);
    if (up <= type || up >= first + LINE_LEN)
        return type;
    /* the levels below the current one are obsolete, as Leader::gain_tech marks them */
    for (int t = first; t < up; t++)
        ((bitmask_set_t)VA(VA_BITMASK_SET))(ld + LD_OBSOLETE, t, 1);
    if (g_upbuild_logs < 40) {
        g_upbuild_logs++;
        logf_("defense level: player %d asked for type 0x%X, lays 0x%X (lower levels now obsolete)", p, type,
              up);
    }
    return up;
}

void hk_upbuild(void);

__asm__(".intel_syntax noprefix\n"
        /* entry of Leader::produce_building: ecx = Leader, [esp+4] = type */
        ".globl _hk_upbuild\n"
        "_hk_upbuild:\n"
        "    push ecx\n"
        "    push edx\n"
        "    push dword ptr [esp+12]\n"
        "    push ecx\n"
        "    call _c_upbuild_type\n"
        "    add esp, 8\n"
        "    pop edx\n"
        "    pop ecx\n"
        "    mov [esp+4], eax\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    sub esp, 0xa98\n"
        "    jmp dword ptr [_g_upbuild_back]\n"
        ".att_syntax prefix\n");

static void upbuild_install(void)
{
    if (!g_build_current_level)
        return;
    static const unsigned char sig[9] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x98, 0x0A, 0x00, 0x00 };
    unsigned char *at = (unsigned char *)VA(VA_PRODUCE_BUILDING);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("defense level: signature mismatch, computer players lay the levels the game gives");
        return;
    }
    g_upbuild_back = (UINT_PTR)(at + sizeof sig);
    hook_jmp5(at, hk_upbuild);
    DWORD old;
    VirtualProtect(at + 5, sizeof sig - 5, PAGE_EXECUTE_READWRITE, &old);
    memset(at + 5, 0x90, sizeof sig - 5);
    VirtualProtect(at + 5, sizeof sig - 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("defense level: computer players lay the current level of Towers and Forts");
}

void defenseramp_install(void)
{
    upbuild_install();
    if (!g_ramp_line)
        return;
    static const unsigned char sig[21] = { 0x0F, 0xB7, 0x94, 0x47, 0x22, 0x5A, 0x00, 0x00,
                                           0x0F, 0xB7, 0x84, 0x47, 0x22, 0x52, 0x00, 0x00,
                                           0x03, 0xD0, 0x89, 0x55, 0x20 };
    unsigned char *at = (unsigned char *)VA(VA_COUNT_SITE);
    if (memcmp(at, sig, sizeof sig) != 0) {
        logf_("ramp: signature mismatch, the game's tower and fort prices stay");
        return;
    }
    g_ramp_back = (UINT_PTR)(at + sizeof sig);
    hook_jmp5(at, hk_ramp_count);
    DWORD old;
    VirtualProtect(at + 5, sizeof sig - 5, PAGE_EXECUTE_READWRITE, &old);
    memset(at + 5, 0x90, sizeof sig - 5);
    VirtualProtect(at + 5, sizeof sig - 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("ramp: Tower and Fort prices rise with every building of the whole line");
}
