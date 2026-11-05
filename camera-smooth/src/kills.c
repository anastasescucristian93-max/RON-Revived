/*
 * Kill counter: a skull stat in the selected-object panel showing how many enemy units
 * and buildings the selected military unit or building has destroyed. Kills made by
 * aircraft and missiles also count for their home (Airbase, Carrier, Missile Silo:
 * UnitData::home_base), which shows the total made by everything it launched.
 *
 * The game keeps kills per player only. Every hit goes through Object::take_damage
 * (melee, projectiles and nukes all call it from Object::do_damage), whose 7th and 8th
 * arguments are the attacker's object id and player. A wrapper notes when the target
 * dies and credits the attacker. It only reads game state, so it is multiplayer-safe.
 * Counts live in memory: they start at zero for a loaded saved game.
 *
 * The stat is drawn like the game's own (IFaceSelected::draw_stat): right after the
 * stats loop of IFaceSelected::on_redraw, in the first free "stat_short" slot. A single unit's
 * goes to the right half of the panel instead, beside its rank (ranks_draw_panel).
 *
 * Decoys (unit +0x68 bit 0, set by SpellType::cast_create_decoy) do not count as kills. The
 * General's Create Decoys ability is switched off in the data (general-heal mod, since
 * 2026-10-27), so they only come from scenarios now.
 *
 * Veteran ranks (ranks.c) share it too: less damage taken before the hit, experience after it.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_TAKE_DAMAGE  0x00652020u /* Object::take_damage, 9 stack args, ret 0x24 */
#define VA_STATS_DONE   0x008268FAu /* IFaceSelected::on_redraw, after the stats loop */
#define VA_INT_STRS     0x00C06378u /* int_str_array: the instruction at VA_STATS_DONE reads it */
#define VA_OBJECTS      0x00C0AB84u /* Object** per player, stride 28 */
#define VA_STRING_CTOR  0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE 0x00A1CF40u
#define VA_STRING_SETI  0x00A1CC30u /* String::operator=(int) */
#define VA_MSG          0x00ED6730u /* String msg: the panel's scratch string */
#define VA_WHITE        0x00C8D26Cu /* Color WHITE */
#define VA_IFACE_RM     0x00EBD1C0u /* IFaceRenderManager iface_render_manager */
#define VA_RM_ADD       0x0051F440u /* IFaceRenderManager::add(IFaceTex *, x, y, color, type) */
#define VA_RM_ADD_RECT  0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_WRITE_CENT   0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_HOME_BASE    0x00609DC0u /* UnitData::home_base(int *): the unit's Airbase / Carrier id, or -1 */

#define OBJ_FLAGS  0x08 /* bit 0: alive */
#define OBJ_PLAYER 0x09 /* byte */
#define OBJ_ID     0x0A /* short */
#define OBJ_TYPE   0x18 /* TypeData* */
#define OBJ_DAMAGE 0x24 /* hit points lost */
#define UNIT_FLAGS 0x68 /* units: bit 0 = decoy */
#define TYPE_ATTACK      0x1E8 /* draw_stat shows the attack stat when these are set */
#define TYPE_ATTACK_BLDG 0x1FC
#define SEL_PLAYER 0x480 /* IFaceSelected: selected object */
#define SEL_ID     0x47C
#define SEL_BUFFER 0x154 /* Buffer the panel writes text with */
#define SEL_CELL   0x48C /* short l r t b: the group grid's first cell (update_selected) */
#define VT_IS_UNIT     0x18  /* Object vtable */
#define VT_MAX_HITS    0x11C /* ObjectData::max hits(0) */
#define VT_FIND_TEX    0xA4  /* GraphicWinXML::find_tex(name, id, int *) */
#define VT_FIND_RECT   0x488 /* IFaceSelected: find a named rect (name, id, int *index) */

/* on_redraw's frame at VA_STATS_DONE (esp-relative) */
#define FR_SHORT_BASE 0x2C /* index of the first "stat_short" rect */
#define FR_USED       0x40 /* int[2][4]: slot used, [column][row] */

#define MAX_ID 8192
#define SKULL_L 150 /* skull in TEX_COMMON (art\Iface_ALLCIVS_assets.tga), 14x14 */
#define SKULL_T 99
#define SKULL_W 14

int g_kills_enabled;
void *g_orig_take_damage;
void *g_orig_stats_done;

static struct {
    void *obj; /* the object the count belongs to: ids are reused */
    int kills;
} g_kills[8][MAX_ID];

typedef int(__attribute__((thiscall)) *take_damage_t)(void *self, int a1, int a2, int a3, int a4, int a5, int a6,
                                                      int a7, int a8, int a9);
typedef int(__attribute__((thiscall)) *fn_int_t)(void *self);
typedef int(__attribute__((thiscall)) *fn_int1_t)(void *self, int a);
typedef void(__attribute__((thiscall)) *fn_str_t)(void *self, const char *s);
typedef void(__attribute__((thiscall)) *fn_close_t)(void *self);
typedef void(__attribute__((thiscall)) *fn_seti_t)(void *self, int v);
typedef void *(__attribute__((thiscall)) *find_t)(void *self, const void *name, int id, void *out);
typedef void(__attribute__((thiscall)) *rm_add_t)(void *self, const void *tex, int x, int y, unsigned color, int type);
typedef void(__attribute__((thiscall)) *rm_add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                        int type);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);

void kills_load_config(const char *ini_path)
{
    g_kills_enabled = GetPrivateProfileIntA("qol", "show_kills", 1, ini_path);
}

static int is_decoy(char *obj)
{
    void **vt = *(void ***)obj;
    return ((fn_int_t)vt[VT_IS_UNIT / 4])(obj) && (*(int *)(obj + UNIT_FLAGS) & 1);
}

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char **objs = *(char ***)(VA(VA_OBJECTS) + 28 * p);
    return objs ? objs[id] : NULL;
}

static int kills_of(int p, int id, void *obj)
{
    if (p < 0 || p > 7 || id < 0 || id >= MAX_ID || g_kills[p][id].obj != obj)
        return 0;
    return g_kills[p][id].kills;
}

typedef int(__attribute__((thiscall)) *home_base_t)(void *self, int *out);

static void credit(int p, int id, char *obj)
{
    if (g_kills[p][id].obj != obj) {
        g_kills[p][id].obj = obj;
        g_kills[p][id].kills = 0;
    }
    g_kills[p][id].kills++;
}

static void on_death(char *victim, int killer_id, int killer_p)
{
    int vp = *(unsigned char *)(victim + OBJ_PLAYER), vid = *(short *)(victim + OBJ_ID);
    if (vp < 8 && vid >= 0 && vid < MAX_ID)
        g_kills[vp][vid].obj = NULL, g_kills[vp][vid].kills = 0;
    if (killer_p < 0 || killer_p > 7 || killer_id < 0 || killer_id >= MAX_ID || killer_p == vp)
        return;
    char *killer = object(killer_p, killer_id);
    if (!killer)
        return;
    credit(killer_p, killer_id, killer);
    int n = g_kills[killer_p][killer_id].kills;
    /* an aircraft or missile: its home gets the kill too */
    void **vt = *(void ***)killer;
    if (((fn_int_t)vt[VT_IS_UNIT / 4])(killer)) {
        int out = -1;
        int home = ((home_base_t)VA(VA_HOME_BASE))(killer, &out);
        char *base = home >= 0 && home < MAX_ID ? object(killer_p, home) : NULL;
        if (base && base != killer && (base[OBJ_FLAGS] & 1)) {
            credit(killer_p, home, base);
            static int logged_home;
            if (logged_home < 5) {
                logged_home++;
                logf_("kills: player %d object %d is home of the killer %d (%d kills)", killer_p, home, killer_id,
                      g_kills[killer_p][home].kills);
            }
        }
    }
    static int logged;
    if (logged < 5) {
        logged++;
        logf_("kills: player %d object %d killed player %d object %d (%d kills)", killer_p, killer_id, vp, vid, n);
    }
}

/* Wrapper around Object::take_damage: a1 = hit points, a7 / a8 = the attacker's id and player
   (-1 for attrition and sabotage). Passed through unchanged, except what ranks.c changes. */
__attribute__((used)) int c_take_damage(char *self, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
                                         int a9)
{
    int alive = self[OBJ_FLAGS] & 1;
    int decoy = alive && is_decoy(self);
    int before = *(int *)(self + OBJ_DAMAGE);
    int vmax = alive ? ((fn_int1_t)(*(void ***)self)[VT_MAX_HITS / 4])(self, 0) : 0;
    if (alive && !decoy)
        ranks_before_hit(self, &a1, &a2, a7);
    int r = ((take_damage_t)g_orig_take_damage)(self, a1, a2, a3, a4, a5, a6, a7, a8, a9);
    if (alive)
        ranks_after_hit(self, before, vmax, decoy, a7, a8);
    if (g_kills_enabled && alive && !(self[OBJ_FLAGS] & 1))
        on_death(self, decoy ? -1 : a7, a8);
    return r;
}

/* The skull, size x size at x, y: ui-theme's (qol_buttons.tga, the user's Gemini picture,
   2026-11-01: "kills_skull" 96 px for large spots, "kills_skull_small" 24 px for the 14-16 unit
   ones, so the GPU does not shrink it fourfold), else the game's 14 px one on the attack icon's
   texture (TEX_COMMON). */
static void draw_skull(const unsigned char *attack, int x, int y, int size, int large)
{
    const void *tex = qol_iface_tex(large ? "kills_skull" : "kills_skull_small");
    static _Alignas(16) unsigned char game[16];
    if (!tex) {
        memcpy(game, attack, 16);
        *(short *)(game + 4) = SKULL_L;
        *(short *)(game + 6) = SKULL_L + SKULL_W;
        *(short *)(game + 8) = SKULL_T;
        *(short *)(game + 10) = SKULL_T + SKULL_W;
        tex = game;
    }
    short r[4] = { (short)x, (short)(x + size), (short)y, (short)(y + size) };
    ((rm_add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, r, *(unsigned *)VA(VA_WHITE), 1);
}

static int can_attack(char *obj)
{
    char *type = *(char **)(obj + OBJ_TYPE);
    if (!type || !*(int *)(type + TYPE_ATTACK))
        return 0;
    void **vt = *(void ***)obj;
    return ((fn_int_t)vt[VT_IS_UNIT / 4])(obj) || *(int *)(type + TYPE_ATTACK_BLDG);
}

static void *find_named(char *self, int vt_off, const char *name, int id, void *out)
{
    unsigned char str[64] = { 0 };
    ((fn_str_t)VA(VA_STRING_CTOR))(str, name);
    void *r = ((find_t)(*(void ***)self)[vt_off / 4])(self, str, id, out);
    ((fn_close_t)VA(VA_STRING_CLOSE))(str);
    return r;
}

/* The skull h x h and the kills after it, the number ending at `right` (panel units). */
void kills_draw_count(char *panel, int kills, int right, int y, int h)
{
    unsigned char *attack = find_named(panel, VT_FIND_TEX, "stat_attack", -1, NULL);
    int nh = h * 8 / 10, nw = hud_number(kills, 0, 0, nh, 0, 0);
    if (nw > 0) {
        if (attack)
            draw_skull(attack, right - nw - 3 - h, y, h, 1);
        hud_number(kills, right - nw, y + (h - nh) / 2, nh, 0xFFFFFFFF, 1);
        return;
    }
    if (attack)
        draw_skull(attack, right - 32 - h, y, h, 1);
    void *msg = (void *)VA(VA_MSG);
    ((fn_seti_t)VA(VA_STRING_SETI))(msg, kills);
    int len = *(unsigned short *)((char *)msg + 8);
    short r[4] = { (short)(right - 30), (short)right, (short)y, (short)(y + h) };
    if (len)
        ((write_t)VA(VA_WRITE_CENT))(panel + SEL_BUFFER, msg, r, len);
}

/* Called right after on_redraw drew the stats: `self` is the IFaceSelected panel. */
__attribute__((used)) void c_kills_stat(char *self, char *frame)
{
    int p = *(int *)(self + SEL_PLAYER), id = *(int *)(self + SEL_ID);
    char *obj = object(p, id);
    if (!obj || !(obj[OBJ_FLAGS] & 1))
        return;
    /* attackers, and launchers whose aircraft or missiles have kills (Airbase, Carrier, Silo) */
    int show = g_kills_enabled && (can_attack(obj) || kills_of(p, id, obj));
    /* a unit: the kills go to the right half with its rank (ranks.c, the user, 2026-11-02) */
    if (((fn_int_t)(*(void ***)obj)[VT_IS_UNIT / 4])(obj) && ranks_draw_panel(self, obj, kills_of(p, id, obj), show))
        return;
    if (!show)
        return;

    int *used = (int *)(frame + FR_USED), col = -1, row = -1;
    for (int r = 0; r < 4 && col < 0; r++)
        for (int c = 0; c < 2; c++)
            if (!used[c * 4 + r]) {
                col = c;
                row = r;
                break;
            }
    if (col < 0)
        return;
    int idx = *(int *)(frame + FR_SHORT_BASE) + col * 4 + row;
    short *slot = find_named(self, VT_FIND_RECT, "stat_short", -1, &idx);
    if (!slot)
        return;
    used[col * 4 + row] = 1;
    short rect[4] = { slot[0], slot[1], slot[2], slot[3] }; /* left, right, top, bottom */

    /* Same layout as draw_stat: the right column has its background on the left and the
       icon at the right edge, the left column the other way round. */
    unsigned char *bg = find_named(self, VT_FIND_TEX, col ? "stat_right" : "stat_left", -1, NULL);
    unsigned char *attack = find_named(self, VT_FIND_TEX, "stat_attack", -1, NULL);
    unsigned white = *(unsigned *)VA(VA_WHITE);
    void *rm = (void *)VA(VA_IFACE_RM);
    if (bg) {
        int bw = *(short *)(bg + 6) - *(short *)(bg + 4);
        ((rm_add_t)VA(VA_RM_ADD))(rm, bg, col ? rect[0] : rect[1] - bw, rect[2], white, 1);
        if (col)
            rect[1] = rect[0] + bw;
        else
            rect[0] = rect[1] - bw;
    }
    if (attack) {
        int cell_x = col ? slot[1] - 20 : slot[0];
        draw_skull(attack, cell_x + 3, slot[2] + 1, SKULL_W, 0);
    }

    void *msg = (void *)VA(VA_MSG);
    ((fn_seti_t)VA(VA_STRING_SETI))(msg, kills_of(p, id, obj));
    int len = *(unsigned short *)((char *)msg + 8);
    if (len)
        ((write_t)VA(VA_WRITE_CENT))(self + SEL_BUFFER, msg, rect, len);
}

/* Several units selected: on_redraw draws only the group's name (draw_group_name, in the panel's
   "name" bar) and no stats. After it, the skull and the kills of the whole selection added up,
   at the left end of that bar, when the selection holds something that attacks or has kills. */
#define VA_GROUP_NAME_CALL 0x008266BAu /* IFaceSelected::on_redraw: call draw_group_name(rect) */
#define VA_GROUP_NAME      0x00820B30u /* IFaceSelected::draw_group_name(const Rect *), ret 4 */
#define VA_CONSOLE         0x00C06210u /* Console *: +0x298 local player, +0x2A0 its selection */
#define VA_SELECTIONS      0x00E8D444u /* SelectGroup[], 0x9F0 each: +0xC count, +0x8CC ids (short) */
typedef void(__attribute__((thiscall)) *group_name_t)(void *self, const short *rect);

__attribute__((used, thiscall)) void c_kills_group_name(char *self, const short *rect)
{
    ((group_name_t)VA(VA_GROUP_NAME))(self, rect);
    char *console = *(char **)VA(VA_CONSOLE), *sel = *(char **)VA(VA_SELECTIONS);
    if (!g_kills_enabled || !console || !sel || !rect)
        return;
    int p = *(int *)(console + 0x298);
    char *group = sel + *(int *)(console + 0x2A0) * 0x9F0;
    int n = *(int *)(group + 0xC), total = 0, fighters = 0;
    for (int i = 0; i < n && i < 1000; i++) {
        int id = *(short *)(group + 0x8CC + 2 * i);
        char *obj = object(p, id);
        if (!obj || !(obj[OBJ_FLAGS] & 1))
            continue;
        int k = kills_of(p, id, obj);
        total += k;
        fighters |= k > 0 || can_attack(obj);
    }
    unsigned char *attack = find_named(self, VT_FIND_TEX, "stat_attack", -1, NULL);
    /* A group without a name (selpanel.c keeps the top row for the kills of every multiple
       selection): the skull and the number as large as the row's first cell takes them (the user,
       2026-11-01). A named group shows them only when it fights. */
    int unnamed = *(int *)(group + 0x9D8) == -1 && g_sel_kills_row;
    if (!fighters && !unnamed)
        return;
    if (unnamed && attack) {
        short *cell = (short *)(self + SEL_CELL);
        int ch = rect[3] - rect[2], cw = cell[1] - cell[0];
        if (cw <= 0 || cw > rect[1] - rect[0])
            cw = rect[1] - rect[0];
        int s = ch - 4 < cw / 2 ? ch - 4 : cw / 2, nh = ch * 6 / 10;
        int x = rect[0] + 2, room = cw - s - 7;
        int nw = hud_number(total, 0, 0, nh, 0, 0);
        while (nw > room && nh > 8)
            nw = hud_number(total, 0, 0, --nh, 0, 0);
        if (nw > 0) {
            draw_skull(attack, x, rect[2] + (ch - s) / 2, s, 1);
            hud_number(total, x + s + 3 + (room - nw) / 2, rect[2] + (ch - nh) / 2, nh, 0xFFFFFFFF, 1);
            return;
        }
    }
    if (attack)
        draw_skull(attack, rect[0] + 4, rect[2], SKULL_W, 0);
    void *msg = (void *)VA(VA_MSG);
    ((fn_seti_t)VA(VA_STRING_SETI))(msg, total);
    int len = *(unsigned short *)((char *)msg + 8);
    short r[4] = { (short)(rect[0] + 4 + SKULL_W + 2), (short)(rect[0] + 4 + SKULL_W + 42), rect[2], rect[3] };
    if (len)
        ((write_t)VA(VA_WRITE_CENT))(self + SEL_BUFFER, msg, r, len);
}

/* ---- hooks ---------------------------------------------------------------------------- */

void hk_take_damage(void);
void hk_kills_stat(void);

__asm__(".intel_syntax noprefix\n"
        /* Object::take_damage(9 args), ret 0x24, returns int */
        ".globl _hk_take_damage\n"
        "_hk_take_damage:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+68]\n"
        "    push dword ptr [ebx+64]\n"
        "    push dword ptr [ebx+60]\n"
        "    push dword ptr [ebx+56]\n"
        "    push dword ptr [ebx+52]\n"
        "    push dword ptr [ebx+48]\n"
        "    push dword ptr [ebx+44]\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_take_damage\n"
        "    add esp, 48\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret 0x24\n"

        /* IFaceSelected::on_redraw after the stats loop: ebx = the panel, the game's esp
           is ebx+32 inside the stub. Then the replaced `mov eax,[int_str_array]`. */
        ".globl _hk_kills_stat\n"
        "_hk_kills_stat:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    lea eax, [ebx+32]\n"
        "    push eax\n"
        "    push dword ptr [ebx+16]\n"
        "    call _c_kills_stat\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_stats_done]\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version. */
void kills_install(void)
{
    if (!g_kills_enabled && !g_ranks_enabled)
        return;
    /* take_damage: push ebp / mov ebp,esp / sub esp,0x9ec */
    static const unsigned char td_sig[9] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xEC, 0x09, 0x00, 0x00 };
    unsigned char *td = (unsigned char *)VA(VA_TAKE_DAMAGE), *sd = (unsigned char *)VA(VA_STATS_DONE);
    /* mov eax, [int_str_array]: the absolute address is relocated in memory */
    int sd_ok = sd[0] == 0xA1 && *(UINT32 *)(sd + 1) == (UINT32)VA(VA_INT_STRS);
    if (memcmp(td, td_sig, sizeof td_sig) != 0 || (g_kills_enabled && !sd_ok)) {
        logf_("kills: signature mismatch, kill counter and ranks turned off");
        g_kills_enabled = g_ranks_enabled = 0;
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    g_orig_take_damage = hook_trampoline(td, 9, mem);
    hook_jmp5(td, hk_take_damage);
    if (sd_ok) { /* the kills stat, and a unit's rank panel (ranks.c) */
        g_orig_stats_done = hook_trampoline(sd, 5, mem + 32);
        hook_jmp5(sd, hk_kills_stat);
    }
    if (g_kills_enabled) {
        unsigned char *gn = (unsigned char *)VA(VA_GROUP_NAME_CALL);
        if (gn[0] == 0xE8 && (UINT_PTR)(gn + 5 + *(INT32 *)(gn + 1)) == VA(VA_GROUP_NAME)) {
            DWORD old;
            VirtualProtect(gn, 5, PAGE_EXECUTE_READWRITE, &old);
            *(INT32 *)(gn + 1) = (INT32)((UINT_PTR)c_kills_group_name - (UINT_PTR)(gn + 5));
            VirtualProtect(gn, 5, old, &old);
        } else {
            logf_("kills: call site mismatch at draw_group_name, no kills for a group");
        }
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("hook take_damage installed (kill counter %d, ranks %d)", g_kills_enabled, g_ranks_enabled);
}
