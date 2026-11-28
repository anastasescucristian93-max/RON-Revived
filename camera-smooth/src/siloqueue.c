/*
 * Missile Silo queue (the user, 2026-11-26: queue as many missiles as wanted in a silo, and order a
 * missile in all of 8 selected silos at once; what a silo holds stays the game's: one nuclear
 * missile, or one other missile, or two Interceptors): a human player's silo takes any number of
 * missiles in its queue. The first one is built, waits finished while the silo is not empty and
 * goes in as soon as the last one is launched, then the next one is built.
 *
 *  - Queueing. Build::queue_up 0x00620F40 refuses an air unit when num_aircraft_here(0 / 1)
 *    reaches num_aircraft_limit (1 for a silo): those two calls (0x00621848, 0x006218B4) get no
 *    limit for a human player's silo. ObjectData::can_carry(TypeIndex) 0x00645E00 says no for a
 *    silo while one missile is queued or inside (has_nuke): the two calls that ask it whether a
 *    missile may be queued, BuildOut::make_options (0x0062A6C8, the button's greying) and
 *    Group::action_queue_up (0x006FE025, which silos of a selection get one), say yes for a human
 *    player's silo and a missile type (the type test can_carry makes: [0x00C0A274][type] vtable
 *    +0x114). nukefx.c's Interceptor button stays enabled too (siloqueue_open).
 *  - Waiting. Build::finished 0x00628490 (g_hooks "silo_finished") returns -1 for a missile at a
 *    silo that holds one (num_aircraft_here(0) >= num_aircraft_limit), or for an Interceptor while
 *    nukefx.c counts two at the silo: the game's own "wait" (an Aircraft Carrier's full deck),
 *    so the queue's first item stays finished and is tried again every frame. Without the wait,
 *    Build::train would put the missile out of the silo (Unit::come_out).
 *  - Several silos: Group::action_queue_up queues one missile in every selected building that
 *    takes it, per click (Shift: five), so 8 selected silos get one each per click.
 * Computer players keep the game's rule (their planners would tie their resources up in queues).
 * A rule of the simulation (from the players' flags and the game's state, the same everywhere):
 * every player of a multiplayer game needs this version and the same [gameplay] silo_queue.
 */
#include "ron.h"
#include <string.h>

#define VA_LEADERS     0x00E3A390u /* LeaderData[8], 0x6EEC each: +0 flags, & 4 human */
#define VA_TYPES       0x00C0A274u /* Type ** by type index */
#define VA_AIR_HERE    0x00645330u /* ObjectData::num_aircraft_here(int queued) const, ret 4 */
#define VA_AIR_LIMIT   0x006454A0u /* ObjectData::num_aircraft_limit() const */
#define VA_CAN_CARRY   0x00645E00u /* ObjectData::can_carry(TypeIndex) const, ret 4 */
#define TYPE_SILO      0x208
#define INTERCEPTOR    0x183       /* nuke-fx's Interceptor (unit 337) */
#define OBJ_TYPE       0x18        /* ObjectTypeData *: +4 its type index */
#define OBJ_OWNER      9
#define OBJ_ID         0xA
#define VT_IS_MISSILE  0x114       /* Type: what can_carry asks of a silo's unit */
#define LEADER_SIZE    0x6EEC

int g_silo_enabled; /* [gameplay] silo_queue; g_hooks "silo_finished" */
void *g_orig_silo_finished;
UINT_PTR g_silo_air_limit, g_silo_can_carry;
int g_silo_tmp;

typedef int(__attribute__((thiscall)) *fn_i_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_i)(void *self);

int nukefx_silo_stock(int p, int id); /* nukefx.c: Interceptors counted at a silo */

void siloqueue_load_config(const char *ini_path)
{
    g_silo_enabled = GetPrivateProfileIntA("gameplay", "silo_queue", 1, ini_path);
}

static int is_silo(const char *obj)
{
    const char *t = obj ? *(const char *const *)(obj + OBJ_TYPE) : NULL;
    return t && *(const int *)(t + 4) == TYPE_SILO;
}

/* A human player's silo: its queue has no limit. */
int siloqueue_open(const char *obj)
{
    if (!g_silo_enabled || !is_silo(obj))
        return 0;
    int p = (unsigned char)obj[OBJ_OWNER];
    return p < 8 && (*(int *)((char *)VA(VA_LEADERS) + p * LEADER_SIZE) & 4);
}

static int is_missile(int type)
{
    void *t = type > 0 && type < 0x1000 ? (*(void ***)VA(VA_TYPES))[type] : NULL;
    return t && ((fn_i)(*(void ***)t)[VT_IS_MISSILE / 4])(t);
}

/* queue_up's limit: none for an open silo */
__attribute__((used)) int c_silo_queue_limit(char *obj)
{
    return siloqueue_open(obj) ? 0x7FFFFFFF : ((fn_i)g_silo_air_limit)(obj);
}

/* "may a missile be queued here" (make_options, action_queue_up) */
__attribute__((used)) int c_silo_can_queue(char *obj, int type)
{
    if (siloqueue_open(obj) && is_missile(type))
        return 1;
    return ((fn_i_i)g_silo_can_carry)(obj, type);
}

/* Build::finished(type): 1 = wait (the hook returns -1), 0 = the game's finish */
__attribute__((used)) int c_silo_finished(char *b, int type)
{
    if (!g_silo_enabled || !is_silo(b) || !is_missile(type))
        return 0;
    if (((fn_i_i)VA(VA_AIR_HERE))(b, 0) >= ((fn_i)VA(VA_AIR_LIMIT))(b))
        return 1;
    return type == INTERCEPTOR && nukefx_silo_stock((unsigned char)b[OBJ_OWNER], *(short *)(b + OBJ_ID)) >= 2;
}

void hk_silo_queue_limit(void);
void hk_silo_can_queue(void);

__asm__(".intel_syntax noprefix\n"
        /* Build::finished(TypeIndex), entry (ecx = the building, ret 4) */
        ".globl _hk_silo_finished\n"
        "_hk_silo_finished:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_silo_finished\n"
        "    add esp, 16\n"
        "    mov [_g_silo_tmp], eax\n" HOOK_LEAVE
        "    cmp dword ptr [_g_silo_tmp], 0\n"
        "    je 1f\n"
        "    or eax, -1\n"
        "    ret 4\n"
        "1:  jmp dword ptr [_g_orig_silo_finished]\n"
        /* queue_up's calls of num_aircraft_limit() (ecx = the building) */
        ".globl _hk_silo_queue_limit\n"
        "_hk_silo_queue_limit:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_silo_queue_limit\n"
        "    add esp, 16\n"
        "    mov [_g_silo_tmp], eax\n" HOOK_LEAVE
        "    mov eax, [_g_silo_tmp]\n"
        "    ret\n"
        /* the calls of can_carry(TypeIndex) (ecx = the building, ret 4) */
        ".globl _hk_silo_can_queue\n"
        "_hk_silo_can_queue:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_silo_can_queue\n"
        "    add esp, 16\n"
        "    mov [_g_silo_tmp], eax\n" HOOK_LEAVE
        "    mov eax, [_g_silo_tmp]\n"
        "    ret 4\n"
        ".att_syntax prefix\n");

static int redirect(UINT32 va, UINT_PTR want, const void *to)
{
    unsigned char *at = (unsigned char *)VA(va);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(want))
        return 0;
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    return 1;
}

/* After install_hooks; only with the wait in place (the "silo_finished" hook). */
void siloqueue_install(void)
{
    if (!g_silo_enabled || !g_orig_silo_finished)
        return;
    static const struct {
        UINT32 va, target;
        int carry;
    } sites[4] = {
        { 0x00621848u, VA_AIR_LIMIT, 0 }, { 0x006218B4u, VA_AIR_LIMIT, 0 },
        { 0x0062A6C8u, VA_CAN_CARRY, 1 }, { 0x006FE025u, VA_CAN_CARRY, 1 },
    };
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(sites[i].va);
        if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(sites[i].target)) {
            logf_("silo queue: call site mismatch at %08lX, a silo queues one missile as in the game",
                  (unsigned long)sites[i].va);
            g_silo_enabled = 0; /* the wait gives the game's answer again */
            return;
        }
    }
    g_silo_air_limit = VA(VA_AIR_LIMIT);
    g_silo_can_carry = VA(VA_CAN_CARRY);
    for (int i = 0; i < 4; i++)
        redirect(sites[i].va, sites[i].target, sites[i].carry ? (const void *)hk_silo_can_queue : (const void *)hk_silo_queue_limit);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("silo queue: a human player's Missile Silo queues any number of missiles, built one at a time");
}
