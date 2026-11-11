/*
 * Big selections ([qol] big_selection): more than the game's 128 objects in the selection and in
 * the control groups, in single player and campaign games. Engine notes: camera-smooth/ENGINE_NOTES.md,
 * "Big selections".
 *
 * A Group holds at most 0x80 objects (six parallel arrays inside every Group, which is embedded in
 * hundreds of places), and a squad's soldiers count one by one, so "select all" stops at about 80
 * units. The Group stays as it is; what does not fit is kept here, the "overflow":
 *  - SelectGroup::add (airdrop.c's hook calls bigsel_after_add): an object of the local player
 *    refused because the local selection is full goes into the overflow (by its squad's captain,
 *    with its uid, so a dead or reused id is never taken for it). SelectGroup::clear of the local
 *    selection empties it.
 *  - Orders: every group command is CommandPackage::add_group(group) then add_command(cmd, size).
 *    When the group is the local selection, the same command is queued for the overflow in chunks
 *    (Groups made the way Unit::do_build makes its own), fed into the package as long as it has
 *    room: a package holds 0x200 bytes a turn and a group takes 3 + 4 per captain. They are
 *    ordinary commands of the local player, run in the simulation like any other. Not copied:
 *    crafts (0x17, a second caster would cast too), Build (0x19, a second site would be laid and
 *    paid), the unit / build masks (0x20, 0x21) and the control group command (0x22).
 *  - Control groups: the 0x22 command for the local selection (Ctrl+number) keeps a copy of the
 *    overflow for that group index; when a key press clears the selection and the selection then
 *    equals that control group (GroupData::equals_group_const), the copy becomes the overflow again.
 *  - The selection circle: hover.c's GroupData::member answer in UnitOut / WallOut::draw_underlays
 *    also says yes for overflow objects.
 * Off in multiplayer and replays: a turn's package there carries everyone's commands to every
 * machine, and the chunks would arrive turns later.
 */
#include "ron.h"
#include <stdlib.h>
#include <string.h>

#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x14 + 0x1c * p = object pointers by id */
#define VA_GAMEC        0x00C061E8u /* const Game *: +0x820 flags */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player, +0x2A0 its selection index */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each */
#define VA_HOTKEYS      0x00C0AFE0u /* HotKeyGroup[0xA2], 0x9FC each, the Group at +0 */
#define VA_HOTKEYS_N    0x00C0AFD4u
#define VA_PACKAGE      0x00E8FF88u /* CommandPackage: +0x10 short bytes used, max 0x200 */
#define VA_CMD_MANAGER  0x00E8FF60u
#define VA_CHECK_ACCEPT 0x00940A70u /* CommandManager::check_accept_issue() */
#define VA_ADD_GROUP    0x0094BB60u /* CommandPackage::add_group(const GroupOut *), ret 4 */
#define VA_ADD_COMMAND  0x0094BAE0u /* CommandPackage::add_command(Command *, short), ret 8 */
#define VA_SEL_CLEAR    0x00717270u /* SelectGroup::clear(int), ret 4 */
#define VA_GROUP_CLEAR  0x00713E80u /* Group::clear(int) */
#define VA_GROUP_ADD    0x00714350u /* Group::add(id, player, 0, 0), virtual, ret 0x10 */
#define VA_MEMBER       0x0070F8F0u /* GroupData::member(id, player, alive) const */
#define VA_EQUALS       0x0070CDF0u /* GroupData::equals_group_const(const GroupData *) const */
#define VA_GROUP_VFT    0x00B47C34u /* const Group::`vftable' */
#define VA_GROUP_VBT    0x00B47C4Cu /* const Group::`vbtable' */

#define GROUP_SIZE   0x9F0
#define HOTKEY_SIZE  0x9FC
#define G_N          0x0C  /* int: objects */
#define G_KIND       0x49  /* byte: units / buildings */
#define G_PLAYER     0x4A
#define G_MAX        0x80
#define PKG_USED     0x10
#define PKG_MAX      0x200
#define OBJ_FLAGS    0x08  /* bit 0: alive */
#define OBJ_OWNER    0x09
#define OBJ_UID      0x30  /* word, what the command package checks */
#define U_SQUAD      0x8E  /* word: bit 15 = captain (or no squad), else the captain's id */
#define U_NEXT       0x90  /* short: next soldier of the squad, -1 = none */
#define FIRST_BUILD  2000
#define MAX_OVER     2048
#define MAX_HOTKEYS  0xA2
#define CHUNK_HEADS  100   /* 3 + 4 * 100 + the command fit an empty package */

/* command types (first byte of the Command) left to the selection alone */
static int not_copied(int t) { return t == 0x17 || t == 0x19 || t == 0x20 || t == 0x21 || t == 0x22; }

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef int(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn2_t)(void *self, int a, int b);
typedef int(__attribute__((thiscall)) *fn3_t)(void *self, int a, int b, int c);
typedef int(__attribute__((thiscall)) *fn4_t)(void *self, int a, int b, int c, int d);

typedef struct {
    short id, uid;
} head_t;

int g_bigsel_enabled;
static void *g_orig_add_group, *g_orig_add_command, *g_orig_sel_clear;

static head_t g_over[MAX_OVER];
static int g_over_n, g_over_p = -1;
static unsigned g_gen; /* bumped when the local selection is cleared */
static head_t *g_hot[MAX_HOTKEYS];
static int g_hot_n[MAX_HOTKEYS], g_hot_p[MAX_HOTKEYS];

static char *g_last_group; /* the group of the add_group just before an add_command */
static int g_dup;          /* we are adding our own copies */

static struct {
    int active, size, p;
    unsigned char cmd[PKG_MAX];
    head_t heads[MAX_OVER];
    int n, pos;
} g_pend;

static __attribute__((aligned(16))) char g_chunk[0xA00];

void bigsel_load_config(const char *ini_path)
{
    g_bigsel_enabled = GetPrivateProfileIntA("qol", "big_selection", 1, ini_path);
}

static int local_player(void) { return *(int *)(*(char **)VA(VA_CONSOLE) + 0x298); }

static char *local_selection(void)
{
    char *console = *(char **)VA(VA_CONSOLE), *sels = *(char **)VA(VA_SELECTIONS);
    return console && sels ? sels + *(int *)(console + 0x2A0) * GROUP_SIZE : NULL;
}

/* single player or campaign, not a replay */
static int active(void)
{
    unsigned char *g = *(unsigned char **)VA(VA_GAMEC);
    return g_bigsel_enabled && g && !(g[0x820] & (0x04 | 0x10));
}

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1c * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) && (unsigned char)o[OBJ_OWNER] == p ? o : NULL;
}

static void *vfn(void *o, int off) { return (*(void ***)o)[off / 4]; }

/* the id that stands for o's squad: its captain, or o itself */
static int head_of(int p, int id)
{
    char *o = object(p, id);
    if (!o)
        return -1;
    if (id >= FIRST_BUILD || ((fn0_t)vfn(o, 0xE8))(o)) /* UnitData::is_captain */
        return id;
    short c = *(short *)(o + U_SQUAD);
    return object(p, c) ? c : id;
}

static char *valid(int p, const head_t *h)
{
    char *o = object(p, h->id);
    return o && *(short *)(o + OBJ_UID) == h->uid ? o : NULL;
}

static int squad_size(int p, int id)
{
    int n = 1;
    char *o = object(p, id);
    if (!o || id >= FIRST_BUILD)
        return 1;
    for (short s = *(short *)(o + U_NEXT); s >= 0 && n < 32; n++) {
        char *m = object(p, s);
        if (!m)
            break;
        s = *(short *)(m + U_NEXT);
    }
    return n;
}

static int in_over(int id)
{
    for (int i = 0; i < g_over_n; i++)
        if (g_over[i].id == id)
            return 1;
    return 0;
}

static int member(char *group, int id, int p) { return ((fn3_t)VA(VA_MEMBER))(group, id, p, 1); }

/* ---- the overflow ---------------------------------------------------------------------- */

/* airdrop.c's SelectGroup::add hook, after the game's add. */
void bigsel_after_add(char *self, int id, int p)
{
    if (!active() || self != local_selection() || *(int *)(self + G_N) < G_MAX || p != local_player())
        return;
    int h = head_of(p, id);
    char *o = object(p, h);
    if (!o || member(self, id, p) || member(self, h, p) || in_over(h) || g_over_n >= MAX_OVER)
        return;
    if (((fn0_t)vfn(o, 0x0C))(o) != (unsigned char)self[G_KIND]) /* units and buildings do not mix */
        return;
    if (g_over_n == 0 || g_over_p != p)
        g_over_n = 0;
    g_over_p = p;
    g_over[g_over_n].id = (short)h;
    g_over[g_over_n].uid = *(short *)(o + OBJ_UID);
    g_over_n++;
}

/* hover.c: the selection circle for an overflow object of the local selection */
int bigsel_has(char *group, int id, int p)
{
    if (!g_over_n || p != g_over_p || group != local_selection() || !active())
        return 0;
    int h = head_of(p, id);
    for (int i = 0; i < g_over_n; i++)
        if (g_over[i].id == h)
            return valid(p, &g_over[i]) != NULL;
    return 0;
}

__attribute__((used)) int c_bs_sel_clear(char *self, int *a)
{
    if (self == local_selection()) {
        g_over_n = 0;
        g_gen++;
    }
    return ((fn1_t)g_orig_sel_clear)(self, a[0]);
}

/* ---- the orders ------------------------------------------------------------------------ */

static void make_chunk(void)
{
    memset(g_chunk, 0, sizeof g_chunk);
    *(UINT_PTR *)g_chunk = VA(VA_GROUP_VFT);
    *(UINT_PTR *)(g_chunk + 0x9CC) = VA(VA_GROUP_VBT);
    ((fn1_t)VA(VA_GROUP_CLEAR))(g_chunk, -1);
    *(int *)(g_chunk + 0x14) = 0;
    *(int *)(g_chunk + 4) = -1;
    ((fn1_t)VA(VA_GROUP_CLEAR))(g_chunk, -1);
}

/* Feeds the pending copies into the package while it has room; the rest waits for the next frame. */
static void feed(void)
{
    if (!g_pend.active)
        return;
    if (!active() || g_pend.p != local_player()) {
        g_pend.active = 0;
        return;
    }
    if (!((fn0_t)VA(VA_CHECK_ACCEPT))((void *)VA(VA_CMD_MANAGER)))
        return;
    char *pkg = (char *)VA(VA_PACKAGE);
    int p = g_pend.p;
    while (g_pend.pos < g_pend.n) {
        make_chunk();
        int heads = 0, i = g_pend.pos;
        for (; i < g_pend.n && heads < CHUNK_HEADS; i++) {
            if (!valid(p, &g_pend.heads[i]))
                continue;
            int id = g_pend.heads[i].id;
            int before = *(int *)(g_chunk + G_N);
            if (before + squad_size(p, id) > G_MAX)
                break;
            ((fn4_t)VA(VA_GROUP_ADD))(g_chunk, id, p, 0, 0);
            if (*(int *)(g_chunk + G_N) > before) /* Group::add has its own kind check (+0x1C) */
                heads++;
        }
        if (!heads) {
            g_pend.pos = i;
            continue;
        }
        if (*(short *)(pkg + PKG_USED) + 3 + 4 * heads + g_pend.size + 4 > PKG_MAX)
            break; /* the next turn */
        g_dup = 1;
        if (((fn1_t)g_orig_add_group)(pkg, (int)(UINT_PTR)g_chunk))
            ((fn2_t)g_orig_add_command)(pkg, (int)(UINT_PTR)g_pend.cmd, g_pend.size);
        g_dup = 0;
        g_pend.pos = i;
    }
    if (g_pend.pos >= g_pend.n) {
        g_pend.active = 0;
        static int logged;
        if (logged++ < 5)
            logf_("bigsel: command %#x also given to %d more (in chunks)", g_pend.cmd[0], g_pend.n);
    }
}

__attribute__((used)) int c_bs_add_group(char *self, int *a)
{
    int r = ((fn1_t)g_orig_add_group)(self, a[0]);
    if (!g_dup)
        g_last_group = r ? (char *)(UINT_PTR)a[0] : NULL;
    return r;
}

static void keep_hotkey(int idx, int clear)
{
    if (idx < 0 || idx >= MAX_HOTKEYS)
        return;
    g_hot_n[idx] = 0;
    if (clear || !g_over_n)
        return;
    if (!g_hot[idx] && !(g_hot[idx] = malloc(sizeof(head_t) * MAX_OVER)))
        return;
    memcpy(g_hot[idx], g_over, sizeof(head_t) * g_over_n);
    g_hot_n[idx] = g_over_n;
    g_hot_p[idx] = g_over_p;
}

__attribute__((used)) int c_bs_add_command(char *self, int *a)
{
    char *group = g_dup ? NULL : g_last_group;
    if (!g_dup)
        g_last_group = NULL;
    int r = ((fn2_t)g_orig_add_command)(self, a[0], a[1]);
    if (g_dup || !r || !active())
        return r;
    unsigned char *cmd = (unsigned char *)(UINT_PTR)a[0];
    int size = (short)a[1];
    char *sel = local_selection();
    if (cmd[0] == 0x22) { /* a control group: Ctrl+number, or emptied */
        int idx = *(int *)(cmd + 1);
        if (group == sel)
            keep_hotkey(idx, 0);
        else if (!group)
            keep_hotkey(idx, 1);
        return r;
    }
    if (!group || group != sel || !g_over_n || not_copied(cmd[0]) || size <= 0 || size > PKG_MAX)
        return r;
    memcpy(g_pend.cmd, cmd, size);
    g_pend.size = size;
    g_pend.p = g_over_p;
    memcpy(g_pend.heads, g_over, sizeof(head_t) * g_over_n);
    g_pend.n = g_over_n;
    g_pend.pos = 0;
    g_pend.active = 1; /* a newer order replaces copies not yet sent */
    feed();
    return r;
}

/* ---- control groups -------------------------------------------------------------------- */

unsigned bigsel_gen(void) { return g_gen; }

/* heal.c's Console::on_key_down hook, after the game's handling: a key that cleared the selection
   and left it equal to a control group brings that group's overflow back. */
void bigsel_after_key(unsigned gen)
{
    if (gen == g_gen || !active())
        return;
    char *sel = local_selection(), *hk = *(char **)VA(VA_HOTKEYS);
    int n = *(int *)VA(VA_HOTKEYS_N), p = local_player();
    if (!sel || !hk || *(int *)(sel + G_N) < G_MAX)
        return;
    if (n > MAX_HOTKEYS)
        n = MAX_HOTKEYS;
    for (int i = 0; i < n; i++) {
        if (!g_hot_n[i] || g_hot_p[i] != p)
            continue;
        char *g = hk + i * HOTKEY_SIZE;
        if (*(int *)(g + G_N) < G_MAX || !((fn1_t)VA(VA_EQUALS))(sel, (int)(UINT_PTR)g))
            continue;
        g_over_n = 0;
        g_over_p = p;
        for (int k = 0; k < g_hot_n[i]; k++)
            if (valid(p, &g_hot[i][k]) && !member(sel, g_hot[i][k].id, p))
                g_over[g_over_n++] = g_hot[i][k];
        return;
    }
}

/* ---- every frame ----------------------------------------------------------------------- */

void bigsel_tick(void)
{
    if (!g_bigsel_enabled)
        return;
    if (g_over_n) { /* the dead, the captured and the ones now in the selection leave it */
        char *sel = local_selection();
        int k = 0;
        for (int i = 0; i < g_over_n; i++)
            if (valid(g_over_p, &g_over[i]) && !(sel && member(sel, g_over[i].id, g_over_p)))
                g_over[k++] = g_over[i];
        g_over_n = k;
    }
    feed();
}

/* ---- install --------------------------------------------------------------------------- */

#define BS_STUB(name, cfn, ret)                                                              \
    ".globl _" name "\n_" name ":\n" HOOK_ENTER "    sub esp, 8\n"                          \
    "    lea eax, [ebx+36]\n"                                                               \
    "    push eax\n"                                                                        \
    "    push ecx\n"                                                                        \
    "    call _" cfn "\n"                                                                   \
    "    add esp, 16\n"                                                                     \
    "    mov [ebx+28], eax\n" HOOK_LEAVE "    " ret "\n"

void hk_bs_add_group(void);
void hk_bs_add_command(void);
void hk_bs_sel_clear(void);
__asm__(".intel_syntax noprefix\n"
        BS_STUB("hk_bs_add_group", "c_bs_add_group", "ret 4")
        BS_STUB("hk_bs_add_command", "c_bs_add_command", "ret 8")
        BS_STUB("hk_bs_sel_clear", "c_bs_sel_clear", "ret 4")
        ".att_syntax prefix\n");

void bigsel_install(void)
{
    if (!g_bigsel_enabled)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[6];
        int steal;
        void *hook;
        void **orig;
    } hooks[] = {
        /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
        { "CommandPackage::add_group", VA_ADD_GROUP, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 }, 5, hk_bs_add_group,
          &g_orig_add_group },
        /* push ebp / mov ebp,esp / and esp,-8 */
        { "CommandPackage::add_command", VA_ADD_COMMAND, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, 6,
          hk_bs_add_command, &g_orig_add_command },
        { "SelectGroup::clear", VA_SEL_CLEAR, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, 6, hk_bs_sel_clear,
          &g_orig_sel_clear },
    };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if (memcmp((void *)VA(hooks[i].va), hooks[i].sig, sizeof hooks[i].sig) != 0) {
            logf_("bigsel: signature mismatch at %s, big selections turned off", hooks[i].name);
            g_bigsel_enabled = 0;
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        g_bigsel_enabled = 0;
        return;
    }
    for (int i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("bigsel: big selections on (single player)");
}
