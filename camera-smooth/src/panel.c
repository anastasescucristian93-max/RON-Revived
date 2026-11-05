/*
 * Command panel (the grid of build / train / research buttons):
 *
 *  - Mixed building selections. SelectGroup::make_options lets every selected building
 *    add its options, then Options::purge(n) keeps only those offered by all n of them,
 *    so barracks + stable + siege factory together showed no unit at all. Here, for a
 *    selection of the local player's buildings of 2+ different types, every "Create"
 *    option is kept on its building's row: 0 infantry, 1 cavalry/tanks, 2 artillery,
 *    3 air (the rally point moves to the bottom-right corner); other options keep the
 *    original rule. Clicks need nothing: the game's
 *    queue command (GroupOut::issue_queue_up, a normal network command) already queues the
 *    unit in each selected building that can make it.
 *
 *  - Button looks. Locked options (wrong age, missing technology) get a padlock on top
 *    of the game's disabled look; available ones the player cannot afford right now are
 *    drawn darker. The game itself never disables a button for its cost.
 *
 *  - Hotkeys on the buttons: the key of every command button with a shortcut is written in
 *    its top-left corner (as in Company of Heroes 3), so there is no need to hover for it.
 *    The text comes from KeyMap::get_hotkey_string(String &, option code, type), the same
 *    call the game's tooltip makes. Unit-line upgrades (OPT_RESEARCH) are left without it.
 *
 * Only the interface is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <math.h>
#include <string.h>

#define VA_PURGE_CALL    0x007168BAu /* SelectGroup::make_options: call Options::purge(n) */
#define VA_PURGE         0x0072BFD0u /* Options::purge(int n), ignores `this` */
#define VA_BTN_REDRAW    0x007FC730u /* IFaceButton::on_redraw() */
#define VA_OPTIONS_PTR   0x00C06204u /* Options*: +0x10 Option[], +0x20 count */
#define VA_CONSOLE_PTR   0x00C06210u /* Console*: +0x298 local player */
#define VA_OBJECTS       0x00C0AB84u /* Object** per player, stride 28 */
#define VA_LEADERS       0x00E3A390u /* LeaderData[8], 0x6EEC each */
#define VA_TYPE_TABLE    0x00E85DDCu /* TypeData** (Types + 0x14) */
#define VA_QUEUE_HERE    0x00639D10u /* BuildTypeData::queue_here(type, 0) */
#define VA_TYPE_AVAIL    0x006E33A0u /* LeaderData::type_avail(type, 1): 4 = available */
#define VA_IS_ENABLED    0x0042A390u /* Window::is_enabled() */
#define VA_IFACE_RM      0x00EBD1C0u /* IFaceRenderManager */
#define VA_RM_ADD_RECT   0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */

#define OPT_SIZE  0x14 /* Option: +0 code, +4 type, +8 count, +0xC disabled, +0x10 gx, +0x11 gy */
#define OPT_CREATE   0x2F
#define OPT_RESEARCH 0x30 /* unit-line upgrade */
#define OPT_RALLY    0x33 /* rally point, at (3,3) */
#define SG_COUNT   0x0C
#define SG_PLAYER  0x4A
#define SG_MEMBERS 0x8CC
#define TC_CAN_PAY 0x84 /* TypeData vtable: can_pay_cost(player, -1, -1, 1) */
#define BTN_OPTION 0x330 /* option index, -1 when the button shows no option */
#define BTN_KIND   0x32C /* 3 unit, 4 building, 6 technology */
#define BTN_TYPE   0x370
#define BTN_FLAGS  0x3BC /* word; bit 3: the button is not drawn */
#define BTN_COLOR  0x384 /* Color, 12 bytes: R, G, B, A bytes first */
#define WIN_RECT   0x126 /* short left, right, top, bottom */
#define RENDER_SOLID 5   /* IFaceRenderType: untextured rectangle */
#define VA_HOTKEY_STRING  0x007D4FD0u /* KeyMap::get_hotkey_string(String &, code, type), uses the global key map */
#define VA_STRING_CTOR    0x00A1D660u
#define VA_STRING_CLOSE   0x00A1CF40u
#define VA_STRING_AS_CHAR 0x0042D710u
#define VA_KEY_MAP        0x00C06228u /* KeyMap key_map */
#define VA_WRITE_L        0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len) */
#define VA_SET_TEXT_COLOR 0x0050DAF0u /* Buffer::set_text_color(const Color &, const Color &, int, int) */
#define VA_WHITE          0x00C8D26Cu
#define VA_BLACK          0x00C8D260u
#define WIN_BUFFER        0x154 /* GraphicWin: Buffer, font pointer at +0x7c */
#define BUF_FONT          0x7C
#define VA_FONTS          0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET       0x00522810u /* FontFamily::get(size, style) const: the loaded font closest to it */
#define FONT_BOLD         1           /* FontFamily::string_to_style: B 1, I 2, U 4, S 8, O 0x10 */

typedef void(__attribute__((thiscall)) *purge_t)(void *self, int n);
typedef int(__attribute__((thiscall)) *queue_here_t)(void *bt, int type, int a);
typedef int(__attribute__((thiscall)) *type_avail_t)(void *leader, int type, int a);
typedef int(__attribute__((thiscall)) *fn_int_t)(void *self);
typedef int(__attribute__((thiscall)) *can_pay_t)(void *type, int player, int a, int b, int c);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef void(__attribute__((thiscall)) *fn_v_t)(void *self);

void *g_orig_btn_redraw;
static int g_show_hotkeys;
static int g_hotkey_size, g_hotkey_bold;

static int local_player(void)
{
    char *c = *(char **)VA(VA_CONSOLE_PTR);
    return c ? *(int *)(c + 0x298) : -1;
}

/* ---- mixed building selections --------------------------------------------------------- */

#define GRID_COLS 5 /* IFACEOPTIONS num_x_slots / num_y_slots in interface.xml */
#define GRID_ROWS 4

__attribute__((used)) void c_purge(char *sg, int n)
{
    int p = (unsigned char)sg[SG_PLAYER];
    char **objs = p < 8 ? *(char ***)(VA(VA_OBJECTS) + 28 * p) : NULL;
    void *bts[8];
    int rows[8], nbt = 0;
    if (objs && p == local_player()) {
        int count = *(int *)(sg + SG_COUNT);
        for (int i = 0; i < count; i++) {
            char *obj = objs[*(short *)(sg + SG_MEMBERS + 2 * i)];
            if (!obj || !(obj[8] & 1))
                continue;
            void *bt = *(void **)(obj + 0x18);
            int k = 0;
            while (k < nbt && bts[k] != bt)
                k++;
            if (k == nbt && nbt < 8)
                bts[nbt++] = bt;
        }
    }
    if (nbt < 2) { /* one building type (or not ours): the game's own rule */
        ((purge_t)VA(VA_PURGE))(NULL, n);
        return;
    }
    /* Fixed rows for the military buildings (infantry, cavalry/tanks, artillery, air);
       any other building type gets a row none of those uses, if one is left. */
    int used = 0;
    for (int k = 0; k < nbt; k++) {
        rows[k] = qol_military_row(bts[k]);
        if (rows[k] >= 0)
            used |= 1 << rows[k];
    }
    for (int k = 0; k < nbt; k++)
        if (rows[k] < 0)
            for (int r = 0; r < GRID_ROWS; r++)
                if (!(used & (1 << r))) {
                    rows[k] = r;
                    used |= 1 << r;
                    break;
                }

    char *opts = *(char **)VA(VA_OPTIONS_PTR);
    char *arr = *(char **)(opts + 0x10);
    int cnt = *(int *)(opts + 0x20), out = 0;
    for (int i = 0; i < cnt; i++) {
        char *e = arr + i * OPT_SIZE;
        int keep;
        if (*(int *)e == OPT_CREATE) {
            int row = -1;
            for (int k = 0; k < nbt && row < 0; k++)
                if (((queue_here_t)VA(VA_QUEUE_HERE))(bts[k], *(int *)(e + 4), 0))
                    row = rows[k];
            keep = row >= 0;
            if (keep)
                e[0x11] = (char)row;
        } else {
            keep = *(int *)(e + 8) >= n;
            /* The rally point (bottom row, column 3) moves to the corner, out of the way
               of the air units on that row. */
            if (keep && (*(int *)e == OPT_RALLY || (e[0x10] == 3 && e[0x11] == GRID_ROWS - 1))) {
                e[0x10] = GRID_COLS - 1;
                e[0x11] = GRID_ROWS - 1;
            }
        }
        if (keep) {
            if (out != i)
                memmove(arr + out * OPT_SIZE, e, OPT_SIZE);
            out++;
        }
    }
    *(int *)(opts + 0x20) = out;
    static int logged;
    if (!logged) {
        logged = 1;
        logf_("panel: mixed selection of %d building types, %d of %d options kept", nbt, out, cnt);
    }
}

/* ---- padlock / can't afford ------------------------------------------------------------- */

enum { LOOK_NORMAL, LOOK_LOCKED, LOOK_POOR };

/* An ability (Option code 4, type = the craft) that some selected unit has but none can cast
   now (GroupData::get_valid_spellcaster runs SpellTypeData::is_castable: craft points, ...). */
#define OPT_SPELL       4
#define VA_GET_CASTER   0x0070CA10u /* GroupData::get_spellcaster(type) const, -1 = none, ret 4 */
#define VA_GET_VCASTER  0x0070C770u /* GroupData::get_valid_spellcaster(type) const, ret 4 */
#define VA_SELECTIONS   0x00E8D444u /* SelectGroup[], 0x9F0 each; Console +0x2A0 = the local one */
typedef int(__attribute__((thiscall)) *caster_t)(void *group, int type);

static int spell_poor(int type)
{
    char *console = *(char **)VA(VA_CONSOLE_PTR), *sel = *(char **)VA(VA_SELECTIONS);
    if (!console || !sel || airdrop_is_option(OPT_SPELL, type)) /* the airdrop shows its own clock */
        return 0;
    char *group = sel + *(int *)(console + 0x2A0) * 0x9F0;
    return ((caster_t)VA(VA_GET_CASTER))(group, type) >= 0 && ((caster_t)VA(VA_GET_VCASTER))(group, type) < 0;
}

static int button_look(char *btn)
{
    int idx = *(int *)(btn + BTN_OPTION), kind = *(int *)(btn + BTN_KIND), type = *(int *)(btn + BTN_TYPE);
    if (idx >= 0 && !(*(unsigned short *)(btn + BTN_FLAGS) & 8)) {
        char *opts = *(char **)VA(VA_OPTIONS_PTR);
        if (opts && idx < *(int *)(opts + 0x20)) {
            char *e = *(char **)(opts + 0x10) + idx * OPT_SIZE;
            if (*(int *)e == OPT_SPELL)
                return spell_poor(*(int *)(e + 4)) ? LOOK_POOR : LOOK_NORMAL;
        }
    }
    if (idx < 0 || type < 0 || (kind != 3 && kind != 4 && kind != 6) || (*(unsigned short *)(btn + BTN_FLAGS) & 8))
        return LOOK_NORMAL;
    char *opts = *(char **)VA(VA_OPTIONS_PTR);
    if (!opts || idx >= *(int *)(opts + 0x20))
        return LOOK_NORMAL;
    char *e = *(char **)(opts + 0x10) + idx * OPT_SIZE;
    if (*(int *)(e + 4) != type)
        return LOOK_NORMAL;
    int p = local_player();
    if (p < 0 || p > 7)
        return LOOK_NORMAL;
    void *leader = (void *)(VA(VA_LEADERS) + p * 0x6EEC);
    int avail = ((type_avail_t)VA(VA_TYPE_AVAIL))(leader, type, 1) == 4;
    int enabled = ((fn_int_t)VA(VA_IS_ENABLED))(btn);
    if (!enabled)
        return avail ? LOOK_NORMAL : LOOK_LOCKED;
    if (!avail || *(int *)e == OPT_RESEARCH)
        return LOOK_NORMAL;
    char *td = (*(char ***)VA(VA_TYPE_TABLE))[type];
    if (!td)
        return LOOK_NORMAL;
    int can = ((can_pay_t)(*(void ***)td)[TC_CAN_PAY / 4])(td, p, -1, -1, 1);
    return can ? LOOK_NORMAL : LOOK_POOR;
}

/* A small padlock drawn from solid rectangles in the top-right corner of the button
   (the research icon and the count use the bottom-right). Button-relative coordinates:
   on_redraw runs with the render manager's origin at the button. */
static void draw_padlock(char *btn)
{
    short *r = (short *)(btn + WIN_RECT);
    int w = r[1] - r[0];
    int x = w - 13, y = 3;
    static const struct {
        signed char l, r, t, b;
        unsigned color;
    } parts[] = {
        { -1, 11, 4, 13, 0xFF101010 },  /* body outline */
        { 1, 9, -1, 5, 0xFF101010 },    /* shackle outline */
        { 0, 10, 5, 12, 0xFF3CBEE6 },   /* body, gold */
        { 2, 4, 0, 5, 0xFF3CBEE6 },     /* shackle left */
        { 6, 8, 0, 5, 0xFF3CBEE6 },     /* shackle right */
        { 2, 8, 0, 2, 0xFF3CBEE6 },     /* shackle top */
        { 4, 6, 7, 10, 0xFF101010 },    /* keyhole */
    };
    void *rm = (void *)VA(VA_IFACE_RM);
    for (unsigned i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        short rect[4] = { (short)(x + parts[i].l), (short)(x + parts[i].r), (short)(y + parts[i].t),
                          (short)(y + parts[i].b) };
        ((add_rect_t)VA(VA_RM_ADD_RECT))(rm, NULL, rect, parts[i].color, RENDER_SOLID);
    }
}

typedef int(__attribute__((thiscall)) *hotkey_t)(void *keymap, void *out, int code, int type);
typedef const char *(__attribute__((thiscall)) *as_char_t)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_cstr_t)(void *self, const char *s);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);

/* The hotkey of the button's option ("V", "Shift+V", ...) in its top-left corner, in the
   font of [qol] hotkey_font_size / hotkey_bold (looked up every time: the game rebuilds its
   fonts when the resolution changes). The interface loads 12 and 14 bold, 18 plain. */
static void draw_hotkey(char *btn)
{
    int idx = *(int *)(btn + BTN_OPTION);
    char *opts = *(char **)VA(VA_OPTIONS_PTR);
    char *buf = btn + WIN_BUFFER;
    if (idx < 0 || !opts || idx >= *(int *)(opts + 0x20) || !*(void **)(buf + 0x7C) ||
        (*(unsigned short *)(btn + BTN_FLAGS) & 8))
        return;
    char *e = *(char **)(opts + 0x10) + idx * OPT_SIZE;
    if (*(int *)e == OPT_RESEARCH) /* "CTRL + <key>" does not fit and covers the picture (the user, 2026-10-27) */
        return;
    unsigned char str[64] = { 0 };
    const char *own = heal_hotkey(*(int *)e, *(int *)(e + 4)); /* the game's key table has none for it */
    ((fn_v_cstr_t)VA(VA_STRING_CTOR))(str, own ? own : "");
    if (!own)
        ((hotkey_t)VA(VA_HOTKEY_STRING))((void *)VA(VA_KEY_MAP), str, *(int *)e, *(int *)(e + 4));
    int len = *(unsigned short *)(str + 8);
    static int logged;
    if (logged < 12) {
        logged++;
        logf_("hotkeys: option code 0x%x type %d -> \"%s\"", *(int *)e, *(int *)(e + 4),
              len ? ((as_char_t)VA(VA_STRING_AS_CHAR))(str) : "");
    }
    if (len > 0 && len <= 12) {
        void **font = (void **)(buf + BUF_FONT), *own = *font;
        void *big = g_hotkey_size > 0 ? ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), g_hotkey_size,
                                                                      g_hotkey_bold ? FONT_BOLD : 0)
                                      : NULL;
        if (big)
            *font = big;
        ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, (void *)VA(VA_WHITE), (void *)VA(VA_BLACK), -1, 0);
        short *r = (short *)(btn + WIN_RECT);
        short rect[4] = { 3, (short)(r[1] - r[0] - 2), 1, (short)(big ? g_hotkey_size + 6 : 16) };
        ((write_t)VA(VA_WRITE_L))(buf, str, rect, len);
        *font = own;
    }
    ((fn_v_t)VA(VA_STRING_CLOSE))(str);
}

/* Airdrop's cooldown as a clock wipe: a dark sector from 12 o'clock that shrinks clockwise
   until the ability is ready. Drawn in one-pixel columns (solid rectangles are the only shape). */
static void draw_cooldown(char *btn)
{
    int idx = *(int *)(btn + BTN_OPTION);
    char *opts = *(char **)VA(VA_OPTIONS_PTR);
    if (idx < 0 || !opts || idx >= *(int *)(opts + 0x20) || (*(unsigned short *)(btn + BTN_FLAGS) & 8))
        return;
    char *e = *(char **)(opts + 0x10) + idx * OPT_SIZE;
    if (!airdrop_is_option(*(int *)e, *(int *)(e + 4)))
        return;
    int left = airdrop_cooldown_permille();
    if (left <= 0)
        return;
    short *r = (short *)(btn + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2];
    float cx = w * 0.5f, cy = h * 0.5f;
    float start = (1000 - left) * (6.2831853f / 1000); /* already revealed, clockwise from the top */
    void *rm = (void *)VA(VA_IFACE_RM);
    for (int x = 0; x < w; x++) {
        int run = -1;
        for (int y = 0; y <= h; y++) {
            int dark = 0;
            if (y < h) {
                float a = atan2f(x + 0.5f - cx, cy - (y + 0.5f)); /* 0 at the top, clockwise */
                if (a < 0)
                    a += 6.2831853f;
                dark = a >= start;
            }
            if (dark && run < 0)
                run = y;
            else if (!dark && run >= 0) {
                short rect[4] = { (short)x, (short)(x + 1), (short)run, (short)y };
                ((add_rect_t)VA(VA_RM_ADD_RECT))(rm, NULL, rect, 0xA0000000, RENDER_SOLID);
                run = -1;
            }
        }
    }
}

/* Airdrop with several Airbases selected: how many drops can be called now, in the bottom-right
   corner, in the hotkey's font. */
static void draw_charges(char *btn)
{
    int idx = *(int *)(btn + BTN_OPTION);
    char *opts = *(char **)VA(VA_OPTIONS_PTR);
    char *buf = btn + WIN_BUFFER;
    if (idx < 0 || !opts || idx >= *(int *)(opts + 0x20) || !*(void **)(buf + BUF_FONT) ||
        (*(unsigned short *)(btn + BTN_FLAGS) & 8))
        return;
    char *e = *(char **)(opts + 0x10) + idx * OPT_SIZE;
    int n;
    if (nukefx_is_option(*(int *)e, *(int *)(e + 4))) {
        n = nukefx_charges(); /* the Interceptors the selected silos hold */
    } else {
        if (!airdrop_is_option(*(int *)e, *(int *)(e + 4)))
            return;
        int bases;
        n = airdrop_charges(&bases);
        if (bases < 2)
            return;
    }
    char text[8];
    wsprintfA(text, "%d", n > 99 ? 99 : n);
    unsigned char str[64] = { 0 };
    ((fn_v_cstr_t)VA(VA_STRING_CTOR))(str, text);
    void **font = (void **)(buf + BUF_FONT), *own = *font;
    void *big = g_hotkey_size > 0 ? ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), g_hotkey_size,
                                                                  g_hotkey_bold ? FONT_BOLD : 0)
                                  : NULL;
    if (big)
        *font = big;
    int size = big ? g_hotkey_size : 12;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, (void *)VA(VA_WHITE), (void *)VA(VA_BLACK), -1, 0);
    short *r = (short *)(btn + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2], tw = (size * 2 / 3 + 1) * (n > 9 ? 2 : 1);
    short rect[4] = { (short)(w - 3 - tw), (short)(w - 1), (short)(h - size - 5), (short)(h - 1) };
    ((write_t)VA(VA_WRITE_L))(buf, str, rect, *(unsigned short *)(str + 8));
    *font = own;
    ((fn_v_t)VA(VA_STRING_CLOSE))(str);
}

/* The capital's star on the selection panel's picture. IFaceSelected::draw_name draws it on the
   panel itself, so the picture (a child window, drawn after) covered it; ui-theme now puts the
   panel's own copy behind the picture and this one is drawn on top, at the top edge, in the
   middle, the same texture in the player's colour. Same test as draw_name: a building whose
   city (+0x72) has the capital flag (City +4 bit 0x10). */
#define VA_SELECTED_VT  0x00B2A124u /* IFaceSelected::`vftable' */
#define VA_CITIES_REF   0x00C061D4u /* Cities *: +0x10 + 28 * p = City ** */
#define VA_TEAM_COLORS  0x00E39CE0u /* TeamColor[], 0x64 each, by LeaderData +0x6928 */
#define VA_TEAM_UNIT    0x008C2DB0u /* TeamColor::get_unit() const, returns the Color through a pointer */
#define VA_GRAPHSYS     0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define GS_DEPTH        0x164
#define WIN_PARENT      0x74
#define WIN_FIND_TEX    0xA4  /* GraphicWinXML vtable: find_tex(const String &, id, int *) */
#define SEL_MULTI       0x470 /* IFaceSelected: several objects selected */
#define SEL_OBJ_ID      0x47C
#define SEL_OBJ_P       0x480
#define BTN_OBJ_ID      0x35C
#define BTN_OBJ_P       0x360

typedef void *(__attribute__((thiscall)) *find_tex_t)(void *self, const void *name, int id, void *out);
typedef void *(__attribute__((thiscall)) *get_color_t)(void *self, void *out);
typedef int(__attribute__((thiscall)) *is_t)(void *self, int what, int a);

static int is_capital(int p, int id)
{
    char **objs = p >= 0 && p < 8 && id >= 0 ? *(char ***)(VA(VA_OBJECTS) + 28 * p) : NULL;
    char *obj = objs ? objs[id] : NULL;
    if (!obj || !(obj[8] & 1))
        return 0;
    char *data = ((void *(__attribute__((thiscall)) *)(void *))(*(void ***)obj)[0x3C / 4])(obj);
    int city = data ? *(short *)(data + 0x72) : -1;
    if (city < 0 || !((is_t)(*(void ***)data)[0xB8 / 4])(data, 0x19E, 0))
        return 0;
    char *cities = *(char **)VA(VA_CITIES_REF);
    char **arr = cities ? *(char ***)(cities + 0x10 + 28 * p) : NULL;
    return arr && arr[city] && (arr[city][4] & 0x10);
}

static void draw_capital_star(char *btn)
{
    char *panel = *(char **)(btn + WIN_PARENT);
    if (!panel || *(unsigned *)panel != VA(VA_SELECTED_VT) || *(int *)(panel + SEL_MULTI) ||
        (*(unsigned short *)(btn + BTN_FLAGS) & 8))
        return;
    int p = *(int *)(btn + BTN_OBJ_P), id = *(int *)(btn + BTN_OBJ_ID);
    if (p != *(int *)(panel + SEL_OBJ_P) || id != *(int *)(panel + SEL_OBJ_ID) || !is_capital(p, id))
        return;
    unsigned char str[64] = { 0 };
    ((fn_v_cstr_t)VA(VA_STRING_CTOR))(str, "capital_star");
    void *tex = ((find_tex_t)(*(void ***)panel)[WIN_FIND_TEX / 4])(panel, str, -1, NULL);
    ((fn_v_t)VA(VA_STRING_CLOSE))(str);
    if (!tex)
        return;
    unsigned color[4];
    int team = *(unsigned char *)(VA(VA_LEADERS) + p * 0x6EEC + 0x6928);
    ((get_color_t)VA(VA_TEAM_UNIT))((void *)(VA(VA_TEAM_COLORS) + team * 0x64), color);
    short *r = (short *)(btn + WIN_RECT);
    int w = r[1] - r[0], h = r[3] - r[2];
    int sh = h * 9 / 20 < 14 ? 14 : h * 9 / 20, sw = sh * 22 / 21; /* as tall as the rank insignia */
    short rect[4] = { (short)((w - sw) / 2), (short)((w + sw) / 2), 2, (short)(2 + sh) };
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
    (*depth)++;
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color[0], 1);
    (*depth)--;
}

__attribute__((used)) void c_btn_redraw(char *btn)
{
    if (assist_panel_redraw(btn)) /* the assistant panel draws itself */
        return;
    if (qol_button_redraw(btn)) /* the round buttons of the qol row */
        return;
    int look = g_panel_enabled ? button_look(btn) : LOOK_NORMAL; /* padlock, darker: command_panel */
    if (cityrepair_button_off(btn)) /* a city's repair switch while it is off */
        look = LOOK_POOR;
    unsigned char saved[12];
    if (look == LOOK_POOR) { /* darker, the way the game dims disabled buttons (colour scale) */
        memcpy(saved, btn + BTN_COLOR, 12);
        unsigned char *rgb = (unsigned char *)btn + BTN_COLOR;
        for (int i = 0; i < 3; i++)
            rgb[i] = (unsigned char)(rgb[i] * 60 / 100);
    }
    ((fn_v_t)g_orig_btn_redraw)(btn);
    if (look == LOOK_POOR)
        memcpy(btn + BTN_COLOR, saved, 12);
    else if (look == LOOK_LOCKED)
        draw_padlock(btn);
    draw_cooldown(btn);
    draw_charges(btn);
    ranks_draw_button(btn);
    draw_capital_star(btn);
    if (g_show_hotkeys)
        draw_hotkey(btn);
}

/* ---- hooks ---------------------------------------------------------------------------- */

void hk_purge(void);
void hk_btn_redraw(void);

__asm__(".intel_syntax noprefix\n"
        /* Replaces `call Options::purge` in SelectGroup::make_options: [esp+4] = n,
           esi = the SelectGroup. purge is `ret 4`. */
        ".globl _hk_purge\n"
        "_hk_purge:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n" /* n */
        "    push dword ptr [ebx+4]\n"  /* esi from pushad */
        "    call _c_purge\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    ret 4\n"

        /* IFaceButton::on_redraw(), ecx = the button, no arguments */
        ".globl _hk_btn_redraw\n"
        "_hk_btn_redraw:\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push ecx\n"
        "    call _c_btn_redraw\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    ret\n"
        ".att_syntax prefix\n");

int g_panel_enabled;

void panel_load_config(const char *ini_path)
{
    g_panel_enabled = GetPrivateProfileIntA("qol", "command_panel", 1, ini_path);
    g_show_hotkeys = GetPrivateProfileIntA("qol", "show_hotkeys", 1, ini_path);
    g_hotkey_size = GetPrivateProfileIntA("qol", "hotkey_font_size", 14, ini_path);
    g_hotkey_bold = GetPrivateProfileIntA("qol", "hotkey_bold", 1, ini_path);
}

/* The redraw hook is installed whatever command_panel says: the assistant panel, the round qol
   buttons, the Airdrop cooldown and charges, the rank insignia, the capital star and the hotkeys
   are drawn from it too. command_panel only switches the mixed selections and the padlock /
   darker looks. */
void panel_install(void)
{
    /* cmp byte [esi+0x49],0 / pop ebx / je +6 / push ecx / call purge */
    static const unsigned char purge_sig[9] = { 0x80, 0x7E, 0x49, 0x00, 0x5B, 0x74, 0x06, 0x51, 0xE8 };
    unsigned char *pc = (unsigned char *)(VA(VA_PURGE_CALL) - 8);
    if (!g_panel_enabled) {
        logf_("panel: command_panel=0, mixed selections and padlocks off");
    } else if (memcmp(pc, purge_sig, sizeof purge_sig) == 0 &&
               pc + 13 + *(INT32 *)(pc + 9) == (unsigned char *)VA(VA_PURGE)) {
        unsigned char *call = pc + 8;
        DWORD old;
        VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(call + 1) = (INT32)((UINT_PTR)hk_purge - ((UINT_PTR)call + 5));
        VirtualProtect(call, 5, old, &old);
        logf_("hook purge (mixed building selections) installed");
    } else
        logf_("panel: signature mismatch at make_options/purge, mixed selections unchanged");

    /* on_redraw: push ebp / mov ebp,esp / sub esp,0xb0 (stolen) | push ebx / mov ebx,ecx */
    static const unsigned char redraw_sig[12] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x53, 0x8B, 0xD9 };
    unsigned char *rd = (unsigned char *)VA(VA_BTN_REDRAW);
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (mem && memcmp(rd, redraw_sig, sizeof redraw_sig) == 0) {
        g_orig_btn_redraw = hook_trampoline(rd, 9, mem);
        hook_jmp5(rd, hk_btn_redraw);
        logf_("hook IFaceButton::on_redraw (padlock, can't afford) installed");
    } else
        logf_("panel: signature mismatch at IFaceButton::on_redraw, button looks unchanged");
}
