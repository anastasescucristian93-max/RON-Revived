/*
 * "No Nukes" in the game setup screen: one more check box next to the game's own (Turbo
 * Resources, No Unique Units, ...). Ticked, nobody can build a Nuclear Missile or a Nuclear
 * ICBM, nor nuke-fx's Interceptor (useless without nukes): their buttons are gone from the
 * Missile Silo and computer players do not get them. Cruise Missiles stay.
 * Engine notes: camera-smooth/ENGINE_NOTES.md, "No Nukes check box".
 *
 * The game's check boxes are bits of Game+0x20, bit = the box's window id (setup_checks); the
 * setup screen keeps them in SetupWin+0x21F4 (11 slots, the 8 ready boxes follow at +0x2220),
 * and a multiplayer lobby sends the whole dword as "lobby_flags" (ConnectionData+0x2BE). Bits
 * 12 and up are read nowhere, so No Nukes is bit 14, in its own box ("nonukes_solo" /
 * "nonukes_mp" in ui-theme's setupwin.xml) that the game's 11-slot code never sees:
 *  - setup_checks (call in exec): after it, the box gets its title, id 14 and the bit's state;
 *  - on_check_clicked / on_check_move / on_check_redraw (SetupWin vtable): the click toggles it
 *    (in multiplayer only the host, who sets the lobby bit for the others as the game's own
 *    request_check_change does), the tooltip is help.xml's SETUPCHECK14 (the game builds the
 *    key from the id; the box is lent to slot 14 for the call, for its rect), and in
 *    multiplayer the box shows the lobby bit, greyed for everyone but the host;
 *  - setup_game (both calls): Game+0x20 bit 14 from the box (single player) or the lobby bit
 *    (multiplayer), on every machine, as setup_game does for bits 0..10.
 * The rule itself is datacenter.c's type_eligible hook, which asks nonukes_type_blocked:
 * 0 = not offered at all. Same bit on every machine, so it is multiplayer safe.
 */
#include "ron.h"
#include <string.h>

#define VA_CALL_CHECKS   0x005C73C1u /* SetupWin::exec: call setup_checks */
#define VA_SETUP_CHECKS  0x005C16A0u /* SetupWin::setup_checks() */
#define VA_CALL_GAME_1   0x005B7028u /* SetupWin::update_interface: call setup_game */
#define VA_CALL_GAME_2   0x005C7EFCu /* SetupWin::exec: call setup_game */
#define VA_SETUP_GAME    0x005B3860u /* SetupWin::setup_game(), returns int */
#define VT_CHECK_REDRAW  0x00B3ACC4u /* SetupWin vtable slot: on_check_redraw(CheckBox *, int id) */
#define VT_CHECK_CLICKED 0x00B3AE60u /* on_check_clicked(int id) */
#define VT_CHECK_MOVE    0x00B3AE6Cu /* on_check_move(int id) */
#define VA_ON_REDRAW     0x005C5610u
#define VA_ON_CLICKED    0x005C5920u
#define VA_ON_MOVE       0x005C3BF0u
#define VA_SET_ID        0x00A542E0u /* Window::set_id(int) */
#define VA_SET_ENABLED   0x00A54110u /* Window::set_enabled(int) */
#define VA_STRING_ASSIGN 0x00A1EEB0u /* String::operator=(const String &) */
#define VA_STRING_CTOR   0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_SOUND_PLAY    0x0097F770u /* SoundGlobal::play(cat): the check box click, 0x111 */
#define VA_SET_CHANGED   0x005AE7D0u /* SetupWin::set_local_game_data_changed() */
#define VA_GAME_PTR      0x00C061ECu
#define VA_CONNECTION    0x00C0622Cu /* ConnectionData * */
#define VA_NETSYS        0x00E335C8u /* NetSys *: vtable +0x10 is the host */
#define VA_WHITE         0x00C8D26Cu /* Color, 12 bytes */
#define VA_COLOR_NONE    0x00EBEA04u

#define SW_MODE     0x1F24 /* & 2: single player (5 boxes), else multiplayer (11) */
#define SW_CHECKS   0x21F4 /* CheckBox *[11] */
#define SW_FONT     0x2268
#define SW_RULES    0x2A6C /* the rules panel, where the boxes are: inside SetupWin, not a pointer */
#define VT_FIND     0x438  /* Window: child by name (const String &, 0) */
#define VT_SHOW     0x18
#define CB_FLAGS    8      /* & 8: disabled */
#define CB_BUFFER   0x154
#define CB_FONT     0x1D0
#define CB_TITLE    0x270
#define CB_STATE    0x284 /* bit 0: ticked */
#define GAME_FLAGS  0x20
#define CD_FLAGS    0x2BE
#define BIT         14
#define CLICK_SOUND 0x111

#define NUCLEAR_MISSILE 0x13B
#define NUCLEAR_ICBM    0x13C
#define INTERCEPTOR     0x183 /* nuke-fx's (unit 337, the campaign SAS slot without it) */

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef void(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef int(__attribute__((thiscall)) *redraw_t)(void *self, void *box, int id);
typedef int(__attribute__((thiscall)) *clicked_t)(void *self, int id);
typedef void(__attribute__((thiscall)) *str_ctor_t)(void *self, const char *s);
typedef void *(__attribute__((thiscall)) *assign_t)(void *self, const void *src);
typedef void *(__attribute__((thiscall)) *find_t)(void *self, const void *name, int a);
typedef unsigned char(__attribute__((thiscall)) *is_host_t)(void *self);

int g_nonukes_enabled;
static char *g_box; /* the box of the setup screen now open */

void nonukes_load_config(const char *ini_path)
{
    g_nonukes_enabled = GetPrivateProfileIntA("gameplay", "no_nukes_option", 1, ini_path);
}

static unsigned *game_flags(void)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    return game ? (unsigned *)(game + GAME_FLAGS) : NULL;
}

int nonukes_type_blocked(int type)
{
    if (!g_nonukes_enabled || (type != NUCLEAR_MISSILE && type != NUCLEAR_ICBM && type != INTERCEPTOR))
        return 0;
    unsigned *f = game_flags();
    return f && (*f >> BIT & 1);
}

static int solo(char *sw) { return *(int *)(sw + SW_MODE) & 2; }

static unsigned *lobby_flags(void)
{
    char *cd = *(char **)VA(VA_CONNECTION);
    return cd ? (unsigned *)(cd + CD_FLAGS) : NULL;
}

static int is_host(void)
{
    char *net = *(char **)VA(VA_NETSYS);
    return net && ((is_host_t)(*(void ***)net)[0x10 / 4])(net);
}

static void set_bit(unsigned *f, int on)
{
    if (f)
        *f = on ? *f | 1u << BIT : *f & ~(1u << BIT);
}

static char *find_box(char *sw)
{
    char *rules = sw + SW_RULES; /* setup_checks: lea ecx, [sw+0x2A6C] / call [[ecx]+0x438] */
    unsigned char str[64] = { 0 };
    ((str_ctor_t)VA(VA_STRING_CTOR))(str, solo(sw) ? "nonukes_solo" : "nonukes_mp");
    char *box = ((find_t)(*(void ***)rules)[VT_FIND / 4])(rules, str, 0);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    return box;
}

/* After setup_checks: the box set up as the game sets up its own (font, colours, state). */
__attribute__((used, thiscall)) void c_nn_setup_checks(char *sw)
{
    ((fn0_t)VA(VA_SETUP_CHECKS))(sw);
    g_box = find_box(sw);
    if (!g_box) {
        static int said;
        if (!said++)
            logf_("no nukes: no nonukes_solo / nonukes_mp box in setupwin.xml (ui-theme not installed?)");
        return;
    }
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text("No Nukes"));
    ((assign_t)VA(VA_STRING_ASSIGN))(g_box + CB_TITLE, str);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    ((fn1_t)VA(VA_SET_ID))(g_box, BIT);
    void **font = *(void ***)(sw + SW_FONT);
    if (font) {
        if (*font)
            *(void **)(g_box + CB_FONT) = font;
        *(int *)(g_box + CB_FONT + 4) = 0;
        *(int *)(g_box + CB_FONT + 8) = 0;
    }
    char *buf = g_box + CB_BUFFER;
    memcpy(buf + 0x90, (void *)VA(VA_WHITE), 12);
    memcpy(buf + 0xCC, (void *)VA(VA_COLOR_NONE), 12);
    buf[0x108] = 1;
    buf[0x10D] = 1;
    unsigned *f = game_flags();
    g_box[CB_STATE] = (char)((g_box[CB_STATE] & ~1) | (f && (*f >> BIT & 1)));
    ((fn1_t)(*(void ***)g_box)[VT_SHOW / 4])(g_box, 0);
}

/* Multiplayer: the box shows the lobby's bit (the host's choice), greyed for the others. */
__attribute__((used, thiscall)) int c_nn_check_redraw(char *sw, char *box, int id)
{
    if (box && box == g_box && id == BIT && !solo(sw)) {
        unsigned *lf = lobby_flags();
        if (lf)
            box[CB_STATE] = (char)((box[CB_STATE] & ~1) | (*lf >> BIT & 1));
        int host = is_host(), enabled = (*(int *)(box + CB_FLAGS) & 8) == 0;
        if (enabled != host)
            ((fn1_t)VA(VA_SET_ENABLED))(box, host);
    }
    return ((redraw_t)VA(VA_ON_REDRAW))(sw, box, id);
}

__attribute__((used, thiscall)) int c_nn_check_clicked(char *sw, int id)
{
    if (id != BIT || !g_box)
        return ((clicked_t)VA(VA_ON_CLICKED))(sw, id);
    ((fn1_t)VA(VA_SOUND_PLAY))(sw, CLICK_SOUND);
    int on = !(g_box[CB_STATE] & 1);
    if (!solo(sw)) {
        if (!is_host())
            return 0;
        set_bit(lobby_flags(), on);
        ((fn0_t)VA(VA_SET_CHANGED))(sw); /* the host's Apply sends it */
    }
    g_box[CB_STATE] = (char)((g_box[CB_STATE] & ~1) | on);
    return 0;
}

/* The tooltip: the game's own code with the box in slot 14 for the call (it takes the box's
   rect from SetupWin+0x21F4 + 4 * id, which is a ready box's slot for 14). */
__attribute__((used, thiscall)) void c_nn_check_move(char *sw, int id)
{
    if (id != BIT || !g_box) {
        ((fn1_t)VA(VA_ON_MOVE))(sw, id);
        return;
    }
    char **slot = (char **)(sw + SW_CHECKS + 4 * BIT);
    char *lent = *slot;
    *slot = g_box;
    ((fn1_t)VA(VA_ON_MOVE))(sw, id);
    *slot = lent;
}

/* Every machine, as the game starts: the bit into Game+0x20, which the rule reads. */
__attribute__((used, thiscall)) int c_nn_setup_game(char *sw)
{
    char *box = solo(sw) ? find_box(sw) : NULL;
    unsigned *lf = lobby_flags();
    int on = solo(sw) ? box && (box[CB_STATE] & 1) : lf && (*lf >> BIT & 1);
    set_bit(game_flags(), on);
    logf_("no nukes: the game starts %s", on ? "WITHOUT nuclear weapons" : "with nuclear weapons");
    return ((fn0_t)VA(VA_SETUP_GAME))(sw);
}

static int call_to(UINT_PTR at, UINT_PTR target)
{
    unsigned char *p = (unsigned char *)VA(at);
    return p[0] == 0xE8 && (UINT_PTR)(p + 5 + *(INT32 *)(p + 1)) == VA(target);
}

static void patch_call(UINT_PTR at, void *to)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(p + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(p + 5));
    VirtualProtect(p, 5, old, &old);
}

static void patch_slot(UINT_PTR slot, void *to)
{
    void **p = (void **)VA(slot);
    DWORD old;
    VirtualProtect(p, 4, PAGE_READWRITE, &old);
    *p = to;
    VirtualProtect(p, 4, old, &old);
}

void nonukes_install(void)
{
    if (!g_nonukes_enabled)
        return;
    if (!call_to(VA_CALL_CHECKS, VA_SETUP_CHECKS) || !call_to(VA_CALL_GAME_1, VA_SETUP_GAME) ||
        !call_to(VA_CALL_GAME_2, VA_SETUP_GAME) || *(UINT_PTR *)VA(VT_CHECK_REDRAW) != VA(VA_ON_REDRAW) ||
        *(UINT_PTR *)VA(VT_CHECK_CLICKED) != VA(VA_ON_CLICKED) || *(UINT_PTR *)VA(VT_CHECK_MOVE) != VA(VA_ON_MOVE)) {
        logf_("no nukes: setup screen code differs, the option is off");
        g_nonukes_enabled = 0;
        return;
    }
    patch_call(VA_CALL_CHECKS, c_nn_setup_checks);
    patch_call(VA_CALL_GAME_1, c_nn_setup_game);
    patch_call(VA_CALL_GAME_2, c_nn_setup_game);
    patch_slot(VT_CHECK_REDRAW, c_nn_check_redraw);
    patch_slot(VT_CHECK_CLICKED, c_nn_check_clicked);
    patch_slot(VT_CHECK_MOVE, c_nn_check_move);
    logf_("no nukes: check box in the game setup (Game+0x20 bit %d)", BIT);
}
