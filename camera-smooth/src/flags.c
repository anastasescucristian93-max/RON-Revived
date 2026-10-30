/*
 * Nation flags in the game setup screen: a round flag left of the nation name, in the
 * nation combo boxes (nation0..7) and in each row of their open list.
 *
 * SetupWin draws both itself: SetupWin::on_combo_redraw writes the selected item of a
 * combo, SetupWin::on_pulldown_item_redraw each row of an open list, both with
 * Buffer::write_l (or write_cent_l for centred combos). Those four calls are pointed at
 * a stub that notes which combo and item is being written; for a nation combo the flag
 * is drawn at the left of the text rect, as tall as the rect, and the text moves right.
 *
 * The flags are IFACETEX "flag_<tribe index>" declared in setupwin.xml, on the atlas
 * art\revived\nation_flags.tga (both made by ui-theme\tools\build_menu_theme.py). Without
 * them nothing changes. Combo ids 8..15 are the nation combos (SetupWin::setup_combos);
 * their item ids are tribe indexes (24 of them, 25 with Romania: g_num_tribes), other
 * items (Random...) get no flag.
 *
 * Only the interface is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_SETUPWIN      0x00CAB3A0u /* SetupWin::current, set while the setup screen runs */
#define VA_WRITE_L       0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len) */
#define VA_WRITE_CENT    0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_STRING_CTOR   0x00A1D660u
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_WHITE         0x00C8D26Cu
#define VA_IFACE_RM      0x00EBD1C0u
#define VA_RM_ADD_RECT   0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const Rect *, color, type) */
#define VA_GRAPHSYS      0x00E335A0u
#define GS_DEPTH         0x164 /* GraphSys update depth: +2 per window, the z of what is added */

#define SW_COMBOS      0x20BC /* SetupWin: ComboBox * by combo id (0x4e of them) */
#define NATION_COMBO   8      /* ids 8..15: nation0..7 */
#define COMBO_SEL      0x568  /* id of the selected item */
#define COMBO_PULLDOWN 0x28C  /* the ComboBox's own PullDown */
#define PD_ITEMS       0x284  /* PullDown items, 64 bytes each, item id at +0x34 */
#define VT_FIND_TEX    0xA4   /* GraphicWinXML::find_tex(name, id, int *) */
#define FLAG_GAP       4

int g_flags_enabled;

/* the call sites: SetupWin::on_combo_redraw (ebx = ComboBox) and
   SetupWin::on_pulldown_item_redraw (edi = PullDown, ebx = item index * 64) */
static const struct {
    UINT_PTR at, target;
} k_sites[4] = {
    { 0x005C54D1u, VA_WRITE_L },    /* combo text, left aligned */
    { 0x005C54B2u, VA_WRITE_CENT }, /* combo text, centred */
    { 0x005C4703u, VA_WRITE_L },    /* list row, left aligned */
    { 0x005C46D7u, VA_WRITE_CENT }, /* list row, centred */
};

/* set by the stubs before they jump to c_flag_write */
char *g_flag_combo;
int g_flag_item; /* -1: the combo's selected item */
int g_flag_site;
void *g_flag_c;

typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *fn_str_t)(void *self, const char *s);
typedef void(__attribute__((thiscall)) *fn_close_t)(void *self);
typedef void *(__attribute__((thiscall)) *find_t)(void *self, const void *name, int id, void *out);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);

void flags_load_config(const char *ini_path)
{
    g_flags_enabled = GetPrivateProfileIntA("qol", "nation_flags", 1, ini_path);
}

static void *find_tex(char *sw, const char *name)
{
    unsigned char str[64] = { 0 };
    ((fn_str_t)VA(VA_STRING_CTOR))(str, name);
    void *r = ((find_t)(*(void ***)sw)[VT_FIND_TEX / 4])(sw, str, -1, NULL);
    ((fn_close_t)VA(VA_STRING_CLOSE))(str);
    return r;
}

static void *find_flag(char *sw, int tribe)
{
    char name[16];
    snprintf(name, sizeof name, "flag_%d", tribe);
    return find_tex(sw, name);
}

static int is_nation_combo(char *sw, char *combo)
{
    /* compared by address only: for a list that is not in a combo, `combo` is not an object */
    for (int i = 0; i < 8; i++)
        if (*(char **)(sw + SW_COMBOS + 4 * (NATION_COMBO + i)) == combo)
            return 1;
    return 0;
}

/* for nation_grid.c: the flag texture of a tribe and whether a ComboBox is a nation combo,
   both NULL / 0 outside the setup screen */
void *flags_tex(int tribe)
{
    char *sw = *(char **)VA(VA_SETUPWIN);
    return sw && tribe >= 0 && tribe < g_num_tribes ? find_flag(sw, tribe) : NULL;
}

/* Draws a flag in front of the window's other pieces. The render manager batches by
   texture and draws the batches in its own order, so within one window a solid fill or
   another atlas can end up over the flag; one step of depth more (windows are 2 apart)
   keeps the flag in front. */
void flags_draw(void *tex, const short *rect, unsigned color)
{
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
    (*depth)++;
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, 1);
    (*depth)--;
}

/* The opaque white "flag_bg" cell of the flag atlas, tinted `color`, at the window's own
   depth. Drawn under a flag in the colour of what is behind it: the flag sits one step in
   front, and its soft edge would otherwise hide that solid fill (drawn later, further back)
   and show the screen behind the window instead, a grey ring around the flag. Both are in
   the same texture batch, so this one is drawn first. */
void flags_draw_bg(const short *rect, unsigned color)
{
    char *sw = *(char **)VA(VA_SETUPWIN);
    void *tex = sw ? find_tex(sw, "flag_bg") : NULL;
    if (tex)
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, 1);
}

int flags_nation_combo(char *combo)
{
    char *sw = *(char **)VA(VA_SETUPWIN);
    return sw && is_nation_combo(sw, combo);
}

__attribute__((used, thiscall)) int c_flag_write(void *buf, const void *str, const short *rect, int len)
{
    write_t write = (write_t)VA(k_sites[g_flag_site].target);
    char *sw = *(char **)VA(VA_SETUPWIN);
    if (!g_flags_enabled || !sw || !rect || !is_nation_combo(sw, g_flag_combo) || !find_flag(sw, 0))
        return write(buf, str, rect, len);

    int id = g_flag_item < 0 ? *(int *)(g_flag_combo + COMBO_SEL) : g_flag_item;
    int h = rect[3] - rect[2], size = h < 10 ? 10 : h > 32 ? 32 : h;
    void *tex = id >= 0 && id < g_num_tribes ? find_flag(sw, id) : NULL;
    if (tex) {
        short r[4] = { rect[0], (short)(rect[0] + size), (short)(rect[2] + (h - size) / 2),
                       (short)(rect[2] + (h - size) / 2 + size) }; /* left, right, top, bottom */
        flags_draw(tex, r, *(unsigned *)VA(VA_WHITE));
    }
    /* every row moves, flag or not (Random), so the names stay in one column */
    short moved[4] = { (short)(rect[0] + size + FLAG_GAP), rect[1], rect[2], rect[3] };
    return write(buf, str, moved, len);
}

/* ---- hooks ---------------------------------------------------------------------------- */

void hk_flag_combo0(void);
void hk_flag_combo1(void);
void hk_flag_item2(void);
void hk_flag_item3(void);

/* Entered by the replaced call, so the stack is the write call's: [return][str][rect][len],
   ecx = the Buffer. c_flag_write is thiscall with the same arguments and returns there. */
__asm__(".intel_syntax noprefix\n"
        ".globl _hk_flag_combo0\n"
        "_hk_flag_combo0:\n"
        "    mov dword ptr [_g_flag_site], 0\n"
        "    jmp 1f\n"
        ".globl _hk_flag_combo1\n"
        "_hk_flag_combo1:\n"
        "    mov dword ptr [_g_flag_site], 1\n"
        "1:\n"
        "    mov [_g_flag_combo], ebx\n"
        "    mov dword ptr [_g_flag_item], -1\n"
        "    jmp dword ptr [_g_flag_c]\n"

        ".globl _hk_flag_item2\n"
        "_hk_flag_item2:\n"
        "    mov dword ptr [_g_flag_site], 2\n"
        "    jmp 2f\n"
        ".globl _hk_flag_item3\n"
        "_hk_flag_item3:\n"
        "    mov dword ptr [_g_flag_site], 3\n"
        "2:\n"
        "    lea eax, [edi-0x28C]\n"
        "    mov [_g_flag_combo], eax\n"
        "    mov eax, [edi+0x284]\n"
        "    mov eax, [eax+ebx+0x34]\n"
        "    mov [_g_flag_item], eax\n"
        "    jmp dword ptr [_g_flag_c]\n"
        ".att_syntax prefix\n");

/* Called from install_hooks once the exe is known to be the supported version. */
void flags_install(void)
{
    if (!g_flags_enabled)
        return;
    void *stubs[4] = { hk_flag_combo0, hk_flag_combo1, hk_flag_item2, hk_flag_item3 };
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(k_sites[i].target)) {
            logf_("flags: call site %d mismatch, nation flags turned off", i);
            g_flags_enabled = 0;
            return;
        }
    }
    g_flag_c = (void *)c_flag_write;
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        DWORD old;
        VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(at + 1) = (INT32)((UINT_PTR)stubs[i] - (UINT_PTR)(at + 5));
        VirtualProtect(at, 5, old, &old);
    }
    logf_("nation flags installed");
}
