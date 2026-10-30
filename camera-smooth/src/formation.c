/*
 * Formations: a group of different units moves without one, and a formation is a compact block
 * instead of two very long rows. See ENGINE_NOTES.md, "Formations".
 *
 *  - Group::action_move_near picks the formation ([ebp-0x34]: the order's, or the one its units
 *    last had, GroupData::get_form) and stores it in the group (+0x10). Formation 9 ("Mob", the
 *    Citizens' own) is the game's "no formation": every unit gets its own move order
 *    (add_move_facing_order instead of add_group_move_order) and goes at its own speed, to a
 *    place in rings around the target (Form::compute_dests). The pick is hooked (0x007050FA):
 *    when a human player's group holds more than one unit type, Generals and Supply Wagons not
 *    counted, it becomes 9. The units' remembered formation (+0xAA, written at 0x0070577D) is
 *    left as it was, so the same units sent alone keep theirs.
 *
 *  - Form::compute_rows_and_columns sizes each category's block. For Line, Refused, Envelop,
 *    the Echelons and Sparse (kinds 0-5) the front is as wide as half the largest category
 *    (more than 5 units: c * width / 2), so every block has 2 rows: 100 Riflemen stand in 2
 *    rows of 50. After the game's own sizing, the front is narrowed to a block about
 *    [formations] width_ratio times as wide as it is deep (100 Riflemen: 7 rows of 15). Up to
 *    5 units still stand in one row. Square, Wedge, Column and Mob keep their own shapes.
 *
 * Both are part of the simulation: every player of a multiplayer game needs the same settings.
 */
#include "ron.h"
#include <string.h>

#define VA_FORM_PICK    0x007050FAu /* action_move_near: mov [esi+0x10],eax / mov eax,[ebp+0x2c] */
#define VA_FORM_PICKED  0x00705100u
#define VA_FORM_STORE   0x0070577Au /* mov eax,[ebp-0x34] / mov [ecx+0xaa],al (a unit's formation) */
#define VA_FORM_STORED  0x00705783u
#define VA_ROWS_COLS    0x0072D910u /* Form::compute_rows_and_columns(int *rows, int *cols, int spread), ret 0xC */
#define VA_OBJECTS      0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object ** */
#define VA_LEADERS      0x00E3A390u /* Leader[8], 0x6EEC each: +0 bit 2 = human */
#define LEADER_SIZE     0x6EEC

#define MOB          9
#define G_COUNT      0x0C   /* Group: number of units */
#define G_PLAYER     0x4A   /* byte */
#define G_IDS        0x8CC  /* short[]: the units */
#define OBJ_FLAGS    0x08   /* bit 0: alive */
#define OBJ_TYPE     0x18
#define VT_IS_HERO   0xC4   /* UnitData virtuals: Generals and the other heroes */
#define VT_IS_SUPPLY 0xCC   /* Supply Wagons */
#define F_KIND       0x28   /* Form: the formation */
#define F_COUNT      0x3C   /* int[18]: units per category */
#define F_WIDTH      0x84   /* int[18]: side by side spacing per category */
#define F_DEPTH      0xCC   /* int[18]: row spacing per category */
#define CATEGORIES   18
#define FIRST_BUILD  2000

typedef void(__attribute__((thiscall)) *rows_cols_t)(void *form, int *rows, int *cols, int spread);
typedef int(__attribute__((thiscall)) *pred_t)(void *unit);

int g_formation_enabled;
static int g_mixed = 1, g_mixed_ai, g_compact = 1, g_ratio = 2;
static void *g_orig_rows_cols;
UINT_PTR g_form_picked, g_form_stored;
int g_form_forced; /* the current action_move_near made the group a Mob */

void formation_load_config(const char *ini_path)
{
    g_formation_enabled = GetPrivateProfileIntA("gameplay", "formations", 1, ini_path);
    g_mixed = GetPrivateProfileIntA("formations", "mixed_no_formation", g_mixed, ini_path);
    g_mixed_ai = GetPrivateProfileIntA("formations", "computer_players", g_mixed_ai, ini_path);
    g_compact = GetPrivateProfileIntA("formations", "compact", g_compact, ini_path);
    g_ratio = GetPrivateProfileIntA("formations", "width_ratio", g_ratio, ini_path);
    if (g_ratio < 1)
        g_ratio = 1;
}

static char *unit(int p, int id)
{
    if (p < 0 || p > 7 || id < 0 || id >= FIRST_BUILD)
        return NULL;
    char **arr = *(char ***)(*(char **)VA(VA_OBJECTS) + 0x14 + 0x1C * p);
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}

static int virt(char *u, int slot) { return ((pred_t)(*(void ***)u)[slot / 4])(u); }

/* More than one unit type in the group, Generals and Supply Wagons not counted. */
static int mixed(char *group)
{
    int p = (unsigned char)group[G_PLAYER], n = *(int *)(group + G_COUNT);
    char *first = NULL;
    for (int i = 0; i < n; i++) {
        char *u = unit(p, ((short *)(group + G_IDS))[i]);
        if (!u || virt(u, VT_IS_HERO) || virt(u, VT_IS_SUPPLY))
            continue;
        char *t = *(char **)(u + OBJ_TYPE);
        if (!first)
            first = t;
        else if (t != first)
            return 1;
    }
    return 0;
}

__attribute__((used)) int c_form_pick(char *group, int form)
{
    g_form_forced = 0;
    int p = (unsigned char)group[G_PLAYER];
    if (!g_mixed || form == MOB || p > 7)
        return form;
    if (!g_mixed_ai && !(*(int *)(VA(VA_LEADERS) + p * LEADER_SIZE) & 4))
        return form;
    if (!mixed(group))
        return form;
    g_form_forced = 1;
    static int logged;
    if (logged < 10) {
        logged++;
        logf_("formations: player %d, %d mixed units move without a formation (was %d)", p,
              *(int *)(group + G_COUNT), form);
    }
    return MOB;
}

void hk_form_pick(void);
void hk_form_store(void);
__asm__(".intel_syntax noprefix\n"
        /* eax = the formation picked, esi = the Group, ebp = action_move_near's frame */
        ".globl _hk_form_pick\n"
        "_hk_form_pick:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+28]\n"
        "    push dword ptr [ebx+4]\n"
        "    call _c_form_pick\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    mov [ebp-0x34], eax\n"
        "    mov [esi+0x10], eax\n"
        "    mov eax, [ebp+0x2c]\n"
        "    jmp dword ptr [_g_form_picked]\n"

        /* ecx = a unit of the group: it remembers the formation unless the group was made a Mob */
        ".globl _hk_form_store\n"
        "_hk_form_store:\n"
        "    cmp dword ptr [_g_form_forced], 0\n"
        "    jne Lform_keep\n"
        "    mov eax, [ebp-0x34]\n"
        "    mov [ecx+0xaa], al\n"
        "Lform_keep:\n"
        "    jmp dword ptr [_g_form_stored]\n"
        ".att_syntax prefix\n");

/* Columns for c units in a block about g_ratio times as wide as deep. */
static int block_columns(int c, int width, int depth)
{
    if (depth <= 0)
        depth = width;
    int k = 3;
    while (k < c && k * k * width < g_ratio * c * depth)
        k++;
    return k;
}

__attribute__((thiscall)) static void fm_rows_cols(char *form, int *rows, int *cols, int spread)
{
    ((rows_cols_t)g_orig_rows_cols)(form, rows, cols, spread);
    if (!g_compact || *(int *)(form + F_KIND) > 5)
        return; /* Square, Wedge, Column and Mob have their own shapes */
    int *cnt = (int *)(form + F_COUNT), *w = (int *)(form + F_WIDTH), *d = (int *)(form + F_DEPTH);
    int front = 0;
    for (int i = 0; i < CATEGORIES; i++) {
        if (cnt[i] <= 0 || w[i] <= 0)
            continue;
        int k = cnt[i] <= 5 ? cnt[i] : block_columns(cnt[i], w[i], d[i]);
        if (k * w[i] > front)
            front = k * w[i];
    }
    front = front * spread / 50;
    for (int i = 0; i < CATEGORIES; i++) {
        if (cnt[i] <= 0 || w[i] <= 0)
            continue;
        int k = front / w[i];
        if (k < 1)
            k = 1;
        if (k < cols[i]) {
            cols[i] = k;
            rows[i] = (cnt[i] + k - 1) / k;
        }
    }
}

void formation_install(void)
{
    if (!g_formation_enabled)
        return;
    static const unsigned char pick_sig[6] = { 0x89, 0x46, 0x10, 0x8B, 0x45, 0x2C };
    static const unsigned char store_sig[9] = { 0x8B, 0x45, 0xCC, 0x88, 0x81, 0xAA, 0x00, 0x00, 0x00 };
    /* push ebp / mov ebp,esp / push ecx / push ebx */
    static const unsigned char rows_sig[5] = { 0x55, 0x8B, 0xEC, 0x51, 0x53 };
    unsigned char *pick = (unsigned char *)VA(VA_FORM_PICK), *store = (unsigned char *)VA(VA_FORM_STORE),
                  *rc = (unsigned char *)VA(VA_ROWS_COLS);
    if (memcmp(pick, pick_sig, sizeof pick_sig) || memcmp(store, store_sig, sizeof store_sig) ||
        memcmp(rc, rows_sig, sizeof rows_sig)) {
        logf_("formations: signature mismatch, formations unchanged");
        g_formation_enabled = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    g_form_picked = VA(VA_FORM_PICKED);
    g_form_stored = VA(VA_FORM_STORED);
    if (g_mixed) {
        hook_jmp5(pick, hk_form_pick);
        hook_jmp5(store, hk_form_store);
    }
    if (g_compact) {
        g_orig_rows_cols = hook_trampoline(rc, 5, mem);
        hook_jmp5(rc, fm_rows_cols);
    }
    logf_("formations: mixed groups %s (computer players %s), compact blocks %s (width %d x depth)",
          g_mixed ? "move freely" : "keep formations", g_mixed_ai ? "too" : "not", g_compact ? "on" : "off",
          g_ratio);
}
