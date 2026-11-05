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
 * The same hook centres the text of the Name, Nation and Difficulty fields (setup_centre;
 * the flag then sits left of the centred name), and flags_install makes the setup screen's
 * three fonts larger (setup_font_percent): SetupWin::exec loads them as FontFamily::get(12 / 14
 * / 16, bold) for every combo, title and check box of the screen; ui-theme's fonts.xml adds the
 * sizes asked for (Arial 15 bold, Georgia 20 bold; Georgia 18 bold is the game's).
 *
 * Only the interface is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_SETUPWIN      0x00CAB3A0u /* SetupWin::current, set while the setup screen runs */
#define VA_WRITE_L       0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len) */
#define VA_WRITE_CENT    0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_TEXT_WIDTH    0x0050EF70u /* Buffer::text_width(const String &, len) */
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
#define WIN_ID         0xC    /* Window id: the combo id for the setup combos */
#define LEFT_INDENT    4      /* on_combo_redraw moves left-aligned text 4 px right */

/* SetupWin::exec: push 1 / push <size> / FontFamily::get, the size bytes of its three fonts
   (SetupWin+0x2268 12, +0x226C 14, +0x2270 16, all bold) */
static const struct {
    UINT_PTR at;
    unsigned char size;
} k_fonts[3] = { { 0x005C72F7u, 12 }, { 0x005C7305u, 14 }, { 0x005C7319u, 16 } };

int g_flags_enabled;
static int g_centre;       /* centre the Name, Nation and Difficulty fields */
static int g_font_percent; /* the setup screen's fonts, percent of the game's */

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
typedef int(__attribute__((thiscall)) *width_t)(void *self, const void *str, int len);
typedef void(__attribute__((thiscall)) *fn_str_t)(void *self, const char *s);
typedef void(__attribute__((thiscall)) *fn_close_t)(void *self);
typedef void *(__attribute__((thiscall)) *find_t)(void *self, const void *name, int id, void *out);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);

void flags_load_config(const char *ini_path)
{
    g_flags_enabled = GetPrivateProfileIntA("qol", "nation_flags", 1, ini_path);
    g_centre = GetPrivateProfileIntA("qol", "setup_centre", 1, ini_path);
    g_font_percent = GetPrivateProfileIntA("qol", "setup_font_percent", 125, ini_path);
    if (g_font_percent < 50 || g_font_percent > 200)
        g_font_percent = 100;
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

/* combo ids (SetupWin::setup_combos): player 0..7, nation 8..15, handicap 0x18..0x1F,
   diff 0x20..0x27 (handicap and diff share the Difficulty column) */
static int is_centred_combo(char *combo)
{
    int id = *(int *)(combo + WIN_ID);
    return (id >= 0 && id < 0x10) || (id >= 0x18 && id < 0x28);
}

__attribute__((used, thiscall)) int c_flag_write(void *buf, const void *str, const short *rect, int len)
{
    write_t write = (write_t)VA(k_sites[g_flag_site].target);
    char *sw = *(char **)VA(VA_SETUPWIN);
    if (!sw || !rect)
        return write(buf, str, rect, len);
    /* only a field's own text (site 0, left aligned) is centred, not the open list's rows */
    int centre = g_centre && g_flag_site == 0 && is_centred_combo(g_flag_combo);
    int flag = g_flags_enabled && is_nation_combo(sw, g_flag_combo) && find_flag(sw, 0);
    if (!flag && !centre)
        return write(buf, str, rect, len);

    short r[4] = { rect[0], rect[1], rect[2], rect[3] }; /* left, right, top, bottom */
    if (centre)
        r[0] = (short)(r[0] - LEFT_INDENT); /* the field's rect is then symmetric */
    int h = r[3] - r[2], size = h < 10 ? 10 : h > 32 ? 32 : h;
    void *tex = NULL;
    if (flag) {
        int id = g_flag_item < 0 ? *(int *)(g_flag_combo + COMBO_SEL) : g_flag_item;
        tex = id >= 0 && id < g_num_tribes ? find_flag(sw, id) : NULL;
    }
    /* in a list every row moves, flag or not (Random), so the names stay in one column;
       a centred field centres the flag and the name together */
    int lead = flag && (tex || !centre) ? size + FLAG_GAP : 0;
    if (centre) {
        int tw = ((width_t)VA(VA_TEXT_WIDTH))(buf, str, len);
        int x = r[0] + (r[1] - r[0] - lead - tw) / 2;
        r[0] = (short)(x > r[0] ? x : r[0]);
        write = (write_t)VA(VA_WRITE_L);
    }
    if (tex) {
        short fr[4] = { r[0], (short)(r[0] + size), (short)(r[2] + (h - size) / 2),
                        (short)(r[2] + (h - size) / 2 + size) };
        flags_draw(tex, fr, *(unsigned *)VA(VA_WHITE));
    }
    r[0] = (short)(r[0] + lead);
    return write(buf, str, r, len);
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

/* The setup screen's three fonts at g_font_percent of their size. */
static void install_fonts(void)
{
    if (g_font_percent == 100)
        return;
    for (int i = 0; i < 3; i++) {
        unsigned char *at = (unsigned char *)VA(k_fonts[i].at);
        if (at[-3] != 0x6A || at[-2] != 0x01 || at[-1] != 0x6A || at[0] != k_fonts[i].size) {
            logf_("setup fonts: push %d not found, sizes left as they are", k_fonts[i].size);
            return;
        }
    }
    for (int i = 0; i < 3; i++) {
        unsigned char *at = (unsigned char *)VA(k_fonts[i].at);
        int size = (k_fonts[i].size * g_font_percent + 50) / 100;
        DWORD old;
        VirtualProtect(at, 1, PAGE_EXECUTE_READWRITE, &old);
        *at = (unsigned char)(size > 60 ? 60 : size);
        VirtualProtect(at, 1, old, &old);
    }
    logf_("setup fonts at %d%%: %d, %d, %d", g_font_percent, *(unsigned char *)VA(k_fonts[0].at),
          *(unsigned char *)VA(k_fonts[1].at), *(unsigned char *)VA(k_fonts[2].at));
}

/* Called from install_hooks once the exe is known to be the supported version. */
void flags_install(void)
{
    install_fonts();
    if (!g_flags_enabled && !g_centre)
        return;
    void *stubs[4] = { hk_flag_combo0, hk_flag_combo1, hk_flag_item2, hk_flag_item3 };
    for (int i = 0; i < 4; i++) {
        unsigned char *at = (unsigned char *)VA(k_sites[i].at);
        if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(k_sites[i].target)) {
            logf_("flags: call site %d mismatch, nation flags and centred fields turned off", i);
            g_flags_enabled = 0;
            g_centre = 0;
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
    logf_("setup combos hooked (nation flags %d, centred fields %d)", g_flags_enabled, g_centre);
}
