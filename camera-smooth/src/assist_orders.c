/*
 * The AI assistant keeps its hands off the units you command. While the computer plays your
 * nation (assist_panel.c), it runs your units like its own: it pulls soldiers into its armies
 * and marches them about (back to a muster spot in the middle of a fight), and it sends any
 * Citizen to the buildings it places, builders of your wonder included. The game's own
 * "Cooperative AI" (a check box in the game setup) does the same while you play: your leader
 * stays human but gets bit 3, and the computer takes over the units made from then on (their
 * AI flag), so both cases are covered.
 *
 * A unit (a whole squad) you give an order to is "kept": from then on
 *   - the computer's armies do not take it (Unit::add_to_army, Army::add_unit), and the orders
 *     the armies give their groups (Army::* -> Group::action_move_to / siege_attack_to /
 *     stance / halt) and the Citizens sent to build or to leave orphaned buildings
 *     (Leader::produce_building / produce_city / check_orphaned_buildings ->
 *     Group::action_swarm_around / action_disband) leave it out;
 *   - its own idle thinking (Unit::think) runs as for a human player's unit: it still
 *     defends itself and fights what comes in range, idle Citizens still look for work as
 *     your stance tells them, but it does not join armies or walk off to defend a city.
 * It is handed back once it has stood idle for ordered_units_idle_seconds (-1: never).
 *
 * Your orders reach the game as commands (CommandPackage::process); the unit orders among them
 * act on the package's current group, the units of the command's group. The computer never
 * sends commands. Single player only, as the assistant itself.
 *
 * Settings: ron_camera.ini [assistant] keep_ordered_units, ordered_units_idle_seconds.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_GAME_PTR       0x00C061ECu
#define VA_CONSOLE_PTR    0x00C06210u /* Console*: +0x298 local player */
#define VA_LEADERS        0x00E3A390u
#define VA_OBJECTS        0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object **, +0x15C + 4 * p = unit count */
#define VA_GROUPS_PTR     0x00E85F20u /* Group *: the persistent groups, GROUP_SIZE each */
#define VA_UNIT_THINK     0x005F6E40u /* Unit::think(void) */
#define RET_DO_IDLE       0x0060DD64u /* Unit::do_idle's call of think: the unit is idle */
#define VA_ADD_TO_ARMY    0x005F7740u /* Unit::add_to_army(void): army index or -1 */
#define VA_ARMY_ADD_UNIT  0x006F9F40u /* Army::add_unit(int id), ret 4 */
#define VA_CMD_PROCESS    0x0094A700u /* CommandPackage::process(Command *), ret 4 */
#define CALL_CMD_PROCESS  0x0094C6D4u /* its call in CommandPackage::process_all */

#define LEADER_SIZE  0x6EEC
#define LD_HUMAN     4      /* [leader + 0]: a human plays it */
#define LD_COOP_AI   8      /* [leader + 0]: a human with the Cooperative AI (Game+0x20 bit 4) */
#define GAME_FLAGS   0x820  /* byte, bit 2: multiplayer game */
#define GAME_TIME    0x550  /* frames, 15 a second */
#define OBJS_UNITS_N 0x15C
#define MAX_UNITS    2000

#define OBJ_FLAGS    0x08   /* bit 0: alive */
#define OBJ_PLAYER   0x09
#define OBJ_ID       0x0A
#define U_FLAGS      0x68   /* 0x40000: a computer player's unit */
#define U_AI_UNIT    0x40000
#define U_CAPTAIN    0x8E   /* word: bit 15 = the captain, else the captain's id */
#define U_NEXT       0x90   /* short: the next soldier of the squad, -1 at the end */

#define PKG_GROUP    0x0C   /* CommandPackage: the current group's index, -1 none */
#define GROUP_SIZE   0x9D4
#define GRP_COUNT    0x0C
#define GRP_PLAYER   0x4A   /* byte */
#define GRP_IDS      0x8CC  /* short ids[count] */
#define VT_GRP_KILL  4      /* Group::kill(id, player, 0, 0): takes the unit's squad out */

/* the unit orders among the commands (CommandPackage::process's switch) */
static const unsigned char k_order_cmds[] = {
    0x02 /* stance */,     0x03 /* form */,         0x04 /* attack */,        0x05 /* siege attack */,
    0x06 /* swarm around */, 0x07 /* move to */,    0x08 /* move near */,     0x09 /* attack ground */,
    0x0A /* patrol */,     0x0B /* launch patrol */, 0x0C /* halt */,         0x0D /* transport */,
    0x0F /* board ship */, 0x10 /* repair */,       0x11 /* trade */,         0x13 /* gather */,
    0x14 /* garrison */,   0x17 /* spell */,        0x19 /* build */,         0x1C /* flight */,
    0x1E /* follow */,     0x1F /* guard */,        0x23 /* recall */,        0x24 /* scramble */,
};

/* the computer's calls of Group actions that must leave kept units out */
enum { GA_MOVE_TO, GA_SIEGE_TO, GA_STANCE, GA_HALT, GA_SWARM, GA_DISBAND, GA_COUNT };
static const struct {
    UINT_PTR target;
    const char *name;
} k_actions[GA_COUNT] = {
    { 0x0070FBA0u, "move_to" },  { 0x0070D830u, "siege_attack_to" }, { 0x0070D440u, "stance" },
    { 0x0070D0C0u, "halt" },     { 0x0070FBE0u, "swarm_around" },    { 0x0070E260u, "disband" },
};
static const struct {
    UINT_PTR at;
    int action;
} k_calls[] = {
    { 0x006F45CCu, GA_MOVE_TO },  /* Army::do_forming */
    { 0x006F5064u, GA_MOVE_TO },  /* Army::march_to_target */
    { 0x006F508Du, GA_MOVE_TO },  /* Army::march_to_target */
    { 0x006F84EDu, GA_MOVE_TO },  /* Army::find_target */
    { 0x006F8566u, GA_MOVE_TO },  /* Army::find_target */
    { 0x006F99A1u, GA_MOVE_TO },  /* Army::send_here */
    { 0x006F9B18u, GA_MOVE_TO },  /* Army::charge */
    { 0x006F45E4u, GA_SIEGE_TO }, /* Army::do_forming */
    { 0x006F509Cu, GA_SIEGE_TO }, /* Army::march_to_target */
    { 0x006F8483u, GA_STANCE },   /* Army::find_target */
    { 0x006F879Fu, GA_STANCE },   /* Army::set_stance */
    { 0x006F8EECu, GA_HALT },     /* Army::close */
    { 0x006CA672u, GA_SWARM },    /* Leader::check_orphaned_buildings */
    { 0x006CB41Bu, GA_SWARM },    /* Leader::produce_city */
    { 0x006E2EB2u, GA_SWARM },    /* Leader::produce_building */
    { 0x006CA17Du, GA_DISBAND },  /* Leader::check_orphaned_buildings */
    { 0x006CA4DDu, GA_DISBAND },  /* Leader::check_orphaned_buildings */
};

static int g_enabled = 1, g_idle_frames = 60 * 15;

/* one per unit of each player: kept while obj is the object in that slot */
static struct kept {
    char *obj;
    int since;     /* game time of the last order */
    int idle_from; /* game time it went idle, -1 busy */
    int idle_last; /* game time it last thought while idle */
} g_kept[8][MAX_UNITS];
static int g_last_time = -1;

UINT_PTR g_ao_think, g_ao_add_to_army, g_ao_army_add, g_ao_cmd;
UINT_PTR g_ao_action[GA_COUNT];

typedef void(__attribute__((thiscall)) *grp_kill_t)(void *g, int id, int player, int a, int b);

static int game_time(void)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    return game ? *(int *)(game + GAME_TIME) : 0;
}

/* A new game (or an earlier saved game) starts the clock again: forget every kept unit. */
static int now_checked(void)
{
    int t = game_time();
    if (t < g_last_time)
        memset(g_kept, 0, sizeof g_kept);
    g_last_time = t;
    return t;
}

/* Does the computer play player p for the local human, or with them (single player)? */
static int assisted(int p)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    char *console = *(char **)VA(VA_CONSOLE_PTR);
    if (!g_enabled || !game || !console || (game[GAME_FLAGS] & 4))
        return 0;
    if (p < 0 || p > 7 || p != *(int *)(console + 0x298))
        return 0;
    unsigned flags = *(unsigned *)(VA(VA_LEADERS) + p * LEADER_SIZE);
    return !(flags & LD_HUMAN) || (flags & LD_COOP_AI);
}

static char *unit_at(int p, int id)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    if (!objs || p < 0 || p > 7 || id < 0 || id >= MAX_UNITS || id >= *(int *)(objs + OBJS_UNITS_N + 4 * p))
        return NULL;
    char **arr = *(char ***)(objs + 0x14 + 0x1C * p);
    char *u = arr ? arr[id] : NULL;
    return u && (u[OBJ_FLAGS] & 1) ? u : NULL;
}

static struct kept *kept_of(char *u)
{
    if (!u)
        return NULL;
    int p = (unsigned char)u[OBJ_PLAYER], id = *(short *)(u + OBJ_ID);
    if (p > 7 || id < 0 || id >= MAX_UNITS)
        return NULL;
    struct kept *k = &g_kept[p][id];
    return k->obj == u && (u[OBJ_FLAGS] & 1) ? k : NULL;
}

/* Is u a unit of yours the computer must leave alone? */
static int is_kept(char *u)
{
    if (!u)
        return 0;
    now_checked();
    return kept_of(u) && assisted((unsigned char)u[OBJ_PLAYER]);
}

static void keep(char *u, int t)
{
    struct kept *k = &g_kept[(unsigned char)u[OBJ_PLAYER]][*(short *)(u + OBJ_ID)];
    k->obj = u;
    k->since = t;
    k->idle_from = -1;
    k->idle_last = -1;
}

/* ---- your orders ------------------------------------------------------------------------ */

__attribute__((used)) void c_ao_command(char *pkg, unsigned char *cmd)
{
    if (!g_enabled || !cmd || !memchr(k_order_cmds, cmd[0], sizeof k_order_cmds))
        return;
    int gi = *(int *)(pkg + PKG_GROUP);
    char *groups = *(char **)VA(VA_GROUPS_PTR);
    if (gi < 0 || !groups)
        return;
    char *g = groups + gi * GROUP_SIZE;
    int p = (unsigned char)g[GRP_PLAYER], n = *(int *)(g + GRP_COUNT);
    if (!assisted(p) || n <= 0)
        return;
    int t = now_checked(), marked = 0;
    for (int i = 0; i < n && i < 512; i++) {
        char *u = unit_at(p, ((short *)(g + GRP_IDS))[i]);
        /* the whole squad: from its captain along the chain */
        unsigned short cap = u ? *(unsigned short *)(u + U_CAPTAIN) : 0x8000;
        if (u && !(cap & 0x8000) && unit_at(p, (short)cap))
            u = unit_at(p, (short)cap);
        for (int guard = 0; u && guard < 64; guard++) {
            keep(u, t);
            marked++;
            u = unit_at(p, *(short *)(u + U_NEXT));
        }
    }
    static int logged;
    if (marked && logged < 20) {
        logged++;
        logf_("assistant: your order (command 0x%x) keeps %d units from the computer", cmd[0], marked);
    }
}

/* ---- the computer's armies and builders ------------------------------------------------- */

/* Takes the kept units out of a group the computer is about to order; returns how many are
   left (the order is dropped when none). */
__attribute__((used)) int c_ao_group(char *g, int action)
{
    int p = (unsigned char)g[GRP_PLAYER];
    if (!assisted(p))
        return 1;
    now_checked();
    int removed = 0;
    for (int pass = 0; pass < 512; pass++) {
        int n = *(int *)(g + GRP_COUNT), hit = -1;
        for (int i = 0; i < n && hit < 0; i++)
            if (kept_of(unit_at(p, ((short *)(g + GRP_IDS))[i])))
                hit = ((short *)(g + GRP_IDS))[i];
        if (hit < 0)
            break;
        ((grp_kill_t)(*(void ***)g)[VT_GRP_KILL])(g, hit, p, 0, 0);
        removed++;
        if (*(int *)(g + GRP_COUNT) >= n) /* not taken out: stop rather than loop */
            break;
    }
    int left = *(int *)(g + GRP_COUNT);
    static int logged;
    if (removed && logged < 30) {
        logged++;
        logf_("assistant: kept %d of your units out of the computer's %s (%d left in its group)", removed,
              k_actions[action].name, left);
    }
    return left > 0;
}

__attribute__((used)) int c_ao_add_to_army(char *u) { return is_kept(u); }

__attribute__((used)) int c_ao_army_add(char *army, int id)
{
    int p = *(short *)(army + 0x94);
    return is_kept(unit_at(p, id));
}

/* ---- the kept unit's own thinking ------------------------------------------------------- */

/* Before Unit::think: a kept unit thinks as a human player's unit (the human flag on its
   leader, the computer flag off the unit), and an idle one is handed back after a while.
   Returns what to undo afterwards: 1 the leader's flag, 2 the unit's flag. */
__attribute__((used)) int c_ao_think_pre(char *u, UINT_PTR ret)
{
    int t = now_checked();
    struct kept *k = kept_of(u);
    if (!k)
        return 0;
    int p = (unsigned char)u[OBJ_PLAYER];
    if (!assisted(p))
        return 0;
    if (ret - g_delta == RET_DO_IDLE) {
        /* idle: a gap of more than 3 s since it last thought idle means it was busy since */
        if (k->idle_from < 0 || t - k->idle_last > 45)
            k->idle_from = t;
        k->idle_last = t;
        if (g_idle_frames >= 0 && t - k->idle_from >= g_idle_frames) {
            static int logged;
            if (logged < 20) {
                logged++;
                logf_("assistant: unit %d idle %d s, handed back to the computer", *(short *)(u + OBJ_ID),
                      (t - k->idle_from) / 15);
            }
            k->obj = NULL;
            return 0;
        }
    }
    int undo = 0;
    unsigned *leader = (unsigned *)VA(VA_LEADERS + p * LEADER_SIZE);
    if (!(*leader & LD_HUMAN)) {
        *leader |= LD_HUMAN;
        undo |= 1;
    }
    if (*(unsigned *)(u + U_FLAGS) & U_AI_UNIT) {
        *(unsigned *)(u + U_FLAGS) &= ~U_AI_UNIT;
        undo |= 2;
    }
    return undo;
}

__attribute__((used)) void c_ao_think_post(char *u, int undo)
{
    if (undo & 1)
        *(unsigned *)VA(VA_LEADERS + (unsigned char)u[OBJ_PLAYER] * LEADER_SIZE) &= ~LD_HUMAN;
    if (undo & 2)
        *(unsigned *)(u + U_FLAGS) |= U_AI_UNIT;
}

/* ---- hooks ------------------------------------------------------------------------------ */

void hk_ao_command(void);
void hk_ao_think(void);
void hk_ao_add_to_army(void);
void hk_ao_army_add(void);
void hk_ao_move_to(void);
void hk_ao_siege_to(void);
void hk_ao_stance(void);
void hk_ao_halt(void);
void hk_ao_swarm(void);
void hk_ao_disband(void);

/* a Group action called by the computer: ecx = the group; dropped when no unit is left */
#define AO_GROUP_STUB(name, action, retn)                     \
    ".globl _" name "\n"                                      \
    "_" name ":\n" HOOK_ENTER                                 \
    "    push " #action "\n"                                  \
    "    push ecx\n"                                          \
    "    call _c_ao_group\n"                                  \
    "    add esp, 8\n"                                        \
    "    mov [ebx+28], eax\n" HOOK_LEAVE                      \
    "    test eax, eax\n"                                     \
    "    jz 1f\n"                                             \
    "    jmp dword ptr [_g_ao_action+" #action "*4]\n"        \
    "1:  ret " #retn "\n"

__asm__(".intel_syntax noprefix\n"
        /* CommandPackage::process (from process_all): ecx = the package, [esp+4] = the command */
        ".globl _hk_ao_command\n"
        "_hk_ao_command:\n" HOOK_ENTER
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_ao_command\n"
        "    add esp, 8\n" HOOK_LEAVE
        "    jmp dword ptr [_g_ao_cmd]\n"

        /* Unit::think: ecx = the unit, no arguments */
        ".globl _hk_ao_think\n"
        "_hk_ao_think:\n" HOOK_ENTER
        "    push dword ptr [ebx+32]\n"
        "    push ecx\n"
        "    call _c_ao_think_pre\n"
        "    add esp, 8\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp dword ptr [_g_ao_think]\n"
        "1:  push eax\n"
        "    push ecx\n"
        "    call dword ptr [_g_ao_think]\n"
        "    pop ecx\n"
        "    pop eax\n" HOOK_ENTER
        "    push eax\n"
        "    push ecx\n"
        "    call _c_ao_think_post\n"
        "    add esp, 8\n" HOOK_LEAVE
        "    ret\n"

        /* Unit::add_to_army: ecx = the unit; a kept one joins no army (-1) */
        ".globl _hk_ao_add_to_army\n"
        "_hk_ao_add_to_army:\n" HOOK_ENTER
        "    push ecx\n"
        "    call _c_ao_add_to_army\n"
        "    add esp, 4\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp dword ptr [_g_ao_add_to_army]\n"
        "1:  or eax, -1\n"
        "    ret\n"

        /* Army::add_unit(int id): ecx = the army */
        ".globl _hk_ao_army_add\n"
        "_hk_ao_army_add:\n" HOOK_ENTER
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_ao_army_add\n"
        "    add esp, 8\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    test eax, eax\n"
        "    jnz 1f\n"
        "    jmp dword ptr [_g_ao_army_add]\n"
        "1:  ret 4\n"

        AO_GROUP_STUB("hk_ao_move_to", 0, 0x28)
        AO_GROUP_STUB("hk_ao_siege_to", 1, 0x14)
        AO_GROUP_STUB("hk_ao_stance", 2, 4)
        AO_GROUP_STUB("hk_ao_halt", 3, 4)
        AO_GROUP_STUB("hk_ao_swarm", 4, 0x14)
        AO_GROUP_STUB("hk_ao_disband", 5, 4)
        ".att_syntax prefix\n");

void assist_orders_load_config(const char *ini_path)
{
    g_enabled = GetPrivateProfileIntA("assistant", "keep_ordered_units", 1, ini_path);
    int s = GetPrivateProfileIntA("assistant", "ordered_units_idle_seconds", 60, ini_path);
    g_idle_frames = s < 0 ? -1 : s * 15;
}

static void write_code(unsigned char *at, const void *bytes, int len)
{
    DWORD old;
    VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, bytes, len);
    VirtualProtect(at, len, old, &old);
}

/* the `call target` at `at` calls `to` instead; 0 if it is not that call */
static int retarget(UINT_PTR at_va, UINT_PTR target_va, void *to)
{
    unsigned char *at = (unsigned char *)VA(at_va);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5) + *(int *)(at + 1) != VA(target_va))
        return 0;
    unsigned char code[5] = { 0xE8 };
    *(int *)(code + 1) = (int)((UINT_PTR)to - (UINT_PTR)(at + 5));
    write_code(at, code, 5);
    return 1;
}

void assist_orders_install(void)
{
    if (!g_enabled)
        return;
    static const struct {
        UINT_PTR va;
        unsigned char sig[9];
        int n;
    } k_entries[3] = {
        { VA_UNIT_THINK, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20 }, 6 },                           /* sub esp, 0x20 */
        { VA_ADD_TO_ARMY, { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x56 }, 6 },                          /* push ecx/ebx/esi */
        { VA_ARMY_ADD_UNIT, { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xE0, 0x09, 0x00, 0x00 }, 9 },      /* sub esp, 0x9E0 */
    };
    for (int i = 0; i < 3; i++)
        if (memcmp((void *)VA(k_entries[i].va), k_entries[i].sig, k_entries[i].n)) {
            logf_("assistant: signature mismatch at 0x%08X, units you command are not kept", k_entries[i].va);
            g_enabled = 0;
            return;
        }
    unsigned char *at = (unsigned char *)VA(CALL_CMD_PROCESS);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5) + *(int *)(at + 1) != VA(VA_CMD_PROCESS)) {
        logf_("assistant: CommandPackage::process call not found, units you command are not kept");
        g_enabled = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        g_enabled = 0;
        return;
    }
    for (int a = 0; a < GA_COUNT; a++)
        g_ao_action[a] = VA(k_actions[a].target);
    static void (*const stubs[GA_COUNT])(void) = { hk_ao_move_to, hk_ao_siege_to, hk_ao_stance,
                                                   hk_ao_halt,    hk_ao_swarm,    hk_ao_disband };
    int calls = 0, n_calls = (int)(sizeof k_calls / sizeof k_calls[0]);
    for (int i = 0; i < n_calls; i++)
        calls += retarget(k_calls[i].at, k_actions[k_calls[i].action].target, (void *)stubs[k_calls[i].action]);

    g_ao_cmd = VA(VA_CMD_PROCESS);
    retarget(CALL_CMD_PROCESS, VA_CMD_PROCESS, (void *)hk_ao_command);
    g_ao_think = (UINT_PTR)hook_trampoline((unsigned char *)VA(VA_UNIT_THINK), 6, mem);
    g_ao_add_to_army = (UINT_PTR)hook_trampoline((unsigned char *)VA(VA_ADD_TO_ARMY), 6, mem + 16);
    g_ao_army_add = (UINT_PTR)hook_trampoline((unsigned char *)VA(VA_ARMY_ADD_UNIT), 9, mem + 32);
    hook_jmp5((unsigned char *)VA(VA_UNIT_THINK), hk_ao_think);
    hook_jmp5((unsigned char *)VA(VA_ADD_TO_ARMY), hk_ao_add_to_army);
    hook_jmp5((unsigned char *)VA(VA_ARMY_ADD_UNIT), hk_ao_army_add);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("assistant: units you command stay yours (handed back after %d s idle; %d of %d computer group "
          "orders filtered)",
          g_idle_frames < 0 ? -1 : g_idle_frames / 15, calls, n_calls);
}
