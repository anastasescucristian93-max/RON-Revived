/*
 * Missing resources in the cost tooltips: after a price you cannot pay, the tooltip also shows
 * how much you are short, for example "120 (-45)".
 *
 * Options::describe hands each price to Scene::set_good_required, which keeps them in the
 * Scene's RequiredResource array (+0x1CC count, +0x1D8 data, 0x24 bytes each: +4 amount,
 * +8 resource, +0x10 "can pay"); the game already writes a price you cannot pay in red.
 * Scene::draw_mouseover_text turns each entry into marked-up text and ends it with a
 * separator (String::operator+= at 0x008B9BC1, with ebx = the Scene and edi = the entry's
 * offset in the array). That call is pointed at hk_cost_sep, which appends the shortfall
 * first, so it is written in the same colour as the price.
 *
 * Only the tooltip text is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <stdio.h>

#define VA_CALL_SITE    0x008B9BC1u /* Scene::draw_mouseover_text: call String::operator+= (separator) */
#define VA_STR_APPEND   0x00A1D440u /* String::operator+=(const String &), ret 4 */
#define VA_STRING_CTOR  0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE 0x00A1CF40u
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player */
#define VA_LEADERS_PTR  0x00C061E0u /* LeaderData[8], 0x6EEC each */

#define SCENE_REQ_COUNT 0x1CC
#define SCENE_REQ_DATA  0x1D8
#define REQ_SIZE        0x24
#define LEADER_STOCK    0x6EB8 /* int * per resource, xor 0x8221 */
#define STOCK_KEY       0x8221
#define NUM_RESOURCES   6

typedef void *(__attribute__((thiscall)) *append_t)(void *self, const void *other);
typedef void(__attribute__((thiscall)) *ctor_t)(void *self, const char *s);
typedef void(__attribute__((thiscall)) *close_t)(void *self);

int g_costs_enabled;

void costs_load_config(const char *ini_path)
{
    g_costs_enabled = GetPrivateProfileIntA("qol", "missing_resources", 1, ini_path);
}

static int shortfall(const char *scene, int off)
{
    char *console = *(char **)VA(VA_CONSOLE);
    char *leaders = *(char **)VA(VA_LEADERS_PTR);
    int count = *(const int *)(scene + SCENE_REQ_COUNT);
    if (!console || !leaders || off < 0 || off % REQ_SIZE || off / REQ_SIZE >= count)
        return 0;
    const char *req = *(char *const *)(scene + SCENE_REQ_DATA) + off;
    int amount = *(const int *)(req + 4), res = *(const int *)(req + 8);
    int me = *(int *)(console + 0x298);
    if (res < 0 || res >= NUM_RESOURCES || me < 0 || me >= 8)
        return 0;
    int *stock = *(int **)(leaders + me * 0x6EEC + LEADER_STOCK);
    if (!stock)
        return 0;
    int have = stock[res] ^ STOCK_KEY;
    return have < amount ? amount - have : 0;
}

__attribute__((used)) void *c_cost_sep(void *text, const void *sep, const char *scene, int off)
{
    int missing = shortfall(scene, off);
    if (missing > 0) {
        char buf[32];
        snprintf(buf, sizeof buf, " (-%d)", missing);
        unsigned char str[64] = { 0 };
        ((ctor_t)VA(VA_STRING_CTOR))(str, buf);
        ((append_t)VA(VA_STR_APPEND))(text, str);
        ((close_t)VA(VA_STRING_CLOSE))(str);
    }
    return ((append_t)VA(VA_STR_APPEND))(text, sep);
}

/* thiscall String::operator+=(sep) with the caller's ebx (Scene) and edi (entry offset) */
__asm__(".intel_syntax noprefix\n"
        "_hk_cost_sep:\n"
        "    push edi\n"
        "    push ebx\n"
        "    push dword ptr [esp+12]\n"
        "    push ecx\n"
        "    call _c_cost_sep\n"
        "    add esp, 16\n"
        "    ret 4\n"
        ".att_syntax prefix\n");
void hk_cost_sep(void);

/* Called from install_hooks once the exe is known to be the supported version. */
void costs_install(void)
{
    if (!g_costs_enabled)
        return;
    unsigned char *at = (unsigned char *)VA(VA_CALL_SITE);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(VA_STR_APPEND)) {
        logf_("costs: call site mismatch, missing resources turned off");
        g_costs_enabled = 0;
        return;
    }
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_cost_sep - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    logf_("missing resources in tooltips installed");
}
