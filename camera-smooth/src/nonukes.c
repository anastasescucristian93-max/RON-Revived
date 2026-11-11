/*
 * "Nukes" in the game setup screen: a slider in place of the old No Nukes check box. It sets how
 * many nuclear weapons may go off in the game (the Armageddon clock):
 *   far left   None       nobody can build a Nuclear Missile, a Nuclear ICBM or nuke-fx's
 *                         Interceptor (the old No Nukes); Cruise Missiles stay;
 *   1 ... 250             the clock starts at that number; the stops get wider to the right
 *                         (1 to 10 one by one, then by 2, 5, 10, 20, 25);
 *   far right  Unlimited  no clock: it is never shown and never runs out.
 * Untouched (or after a right-click on it) it keeps the game's rule: rules.xml's ARMAGEDDON 4
 * + 1 per nation + 2 per team. The tooltip is help.xml's SETUPCHECK14 (nuke-fx's build_data.py).
 * Engine notes: camera-smooth/ENGINE_NOTES.md, "No Nukes check box" and "Nukes slider".
 *
 * Where the choice lives: Game+0x20, the setup's flags dword (bit = check box id, bits 0..10
 * the game's boxes). Bit 14 = None, as the old box; bits 15..21 = the slider's stop (1..38) for
 * a count or Unlimited, 0 = the game's rule. A multiplayer lobby sends the whole dword as
 * "lobby_flags" (ConnectionData+0x2BE), so every machine gets the host's choice; bits 12 and up
 * are read nowhere else.
 *
 * The box ("nonukes" in ui-theme's setupwin.xml, first row of the left column in both single
 * player and multiplayer, the Units button beside it) is the game's CheckBox,
 * set up after setup_checks with id 14. It gets a vtable of its own (a copy of its class's with
 * the mouse slots replaced), so the game's other check boxes are untouched: left button down
 * takes the mouse (as Button::on_left_down does) and moves the knob, moves drag it, the button
 * up lets go and keeps the stop; a right-click goes back to the game's rule. Drawing is
 * SetupWin::on_check_redraw (SetupWin vtable), which draws this box by hand: a track of the
 * game's slider art (setupwin.xml "nukes_track_*", "nukes_caret") with a gold fill, and the
 * label through the box's Buffer. In multiplayer only the host moves it; everyone else sees the
 * lobby's value, greyed.
 *
 * The rule, on every machine:
 *  - None: datacenter.c's type_eligible hook asks nonukes_type_blocked (0 = not offered).
 *  - A count / Unlimited: Constants +0xD14 (ARMAGEDDON) = the count (1000000 for Unlimited),
 *    +0xD18 / +0xD1C (per nation / per team) = 0. The game has the formula inlined in five
 *    places besides Game::get_armageddon (the clock on the screen, check_victory, ...), so the
 *    constants are what changes, not the function. Set at setup_game and again every simulation
 *    frame (nonukes_frame, from airdrop.c's Objects::process_all hook) from Game+0x20, and put
 *    back to rules.xml's values for a game without a choice. Game styles multiply the clock
 *    (Game+0x2D 7 x3; 5, 6, 11, 12 x2; 8 at least 100): with a count chosen, the six places that
 *    read the style for it (get_armageddon, check_victory twice, draw_timers twice, the end of
 *    game achievement) read 0 instead (their `mov reg8, [reg+0x2D]` becomes `xor reg, reg`).
 *  - The count is exact: launches stop once the nukes gone off plus those in the air (nukefx.c
 *    counts Nuclear Missiles and ICBMs) reach it. The launch command (armageddon.c's can_nuke
 *    asks nonukes_launch_full) says no with the game's message; the computer's own launch check
 *    in Object::do_launch, which kept 2 in reserve, is pointed at nonukes_ai_armageddon; and a
 *    nuke that would still go over (several fired by one command, or two commands a moment
 *    apart) does not fire in Object::fire_ammo: it stays in its silo (its Object::die right
 *    after the shot is skipped) and its orders are cleared at the end of the frame.
 *  - Unlimited: the clock line of IFaceMainBase::draw_timers is skipped (its jne at 0x0080C772).
 */
#include "ron.h"
#include <stdio.h>
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
#define VA_GET_MOUSE_POS 0x00A54170u /* Window::get_mouse_pos(int *x, int *y): client coordinates */
#define VA_RELEASE_MOUSE 0x00459DC0u /* Window::release_mouse_capture() */
#define VA_MOUSE_CAPTURE 0x00E334F4u /* Window::mouse_capture */
#define VA_NC_CAPTURE    0x00E334F0u /* Window::nc_mouse_capture */
#define VA_STRING_ASSIGN 0x00A1EEB0u /* String::operator=(const String &) */
#define VA_STRING_CTOR   0x00A1D660u /* String::String(const char *) */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_SOUND_PLAY    0x0097F770u /* SoundGlobal::play(cat): the check box click, 0x111 */
#define VA_SET_CHANGED   0x005AE7D0u /* SetupWin::set_local_game_data_changed() */
#define VA_IFACE_RM      0x00EBD1C0u /* IFaceRenderManager */
#define VA_RM_ADD_RECT   0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_WRITE_RIGHT   0x0050FA40u /* Buffer::write_right_l(const String &, const Rect *, len) */
#define VA_TEXT_WIDTH    0x0050EF40u /* Buffer::text_width(const String &) */
#define VA_SET_TEXT_COLOR 0x0050DAF0u /* Buffer::set_text_color(const Color &, const Color &, int, int) */
#define VA_GRAPHSYS      0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define VA_GAME_PTR      0x00C061ECu
#define VA_CONSTANTS     0x00C061F0u /* Constants * */
#define VA_CONNECTION    0x00C0622Cu /* ConnectionData * */
#define VA_NETSYS        0x00E335C8u /* NetSys *: vtable +0x10 is the host */
#define VA_WHITE         0x00C8D26Cu /* Color, 12 bytes */
#define VA_BLACK         0x00C8D260u
#define VA_COLOR_NONE    0x00EBEA04u
#define VA_ARMAGEDDON    0x00594020u /* Game::get_armageddon() */
#define VA_CALL_AI_ARM   0x0064FE5Eu /* Object::do_launch: call get_armageddon, then sub eax, 2 */
#define VA_CLOCK_JNE     0x0080C772u /* draw_timers: jne (75 0F) to the Armageddon clock line */
#define VA_FIRE_AMMO     0x0064C8B0u /* Object::fire_ammo(int, int), ret 8 */
#define VA_DIE           0x00647080u /* Object::die(char, int, float), ret 0xC */
#define VA_CLEAR_ORDERS  0x005E3860u /* Unit::clear_orders() */
#define VA_OBJECTS       0x00C0618Cu /* Objects *: +0x14 + 0x1C p = the player's objects */

#define SW_MODE     0x1F24 /* & 2: single player (5 boxes), else multiplayer (11) */
#define SW_CHECKS   0x21F4 /* CheckBox *[11] */
#define SW_FONT     0x2268
#define SW_RULES    0x2A6C /* the rules panel, where the boxes are: inside SetupWin, not a pointer */
#define VT_FIND     0x438  /* Window: child by name (const String &, 0) */
#define VT_FIND_TEX 0xA4   /* SetupWin: IFACETEX by name (const String &, -1, 0) */
#define VT_SHOW     0x18
#define VT_MOVE     0xD0   /* Window vtable: on_mouse_move(x, y) */
#define VT_CLICK    0xD8   /* on_left_click(x, y) */
#define VT_LDOWN    0xDC   /* on_left_down(x, y) */
#define VT_LUP      0xE0   /* on_left_up(x, y) */
#define VT_RCLICK   0xF0   /* on_right_click(x, y) */
#define VT_RDOWN    0xF4   /* on_right_down(x, y) */
#define VT_COPY     0x600  /* bytes of the box's vtable copied (Window vtables end before) */
#define CB_FLAGS    8      /* & 8: disabled */
#define CB_BUFFER   0x154
#define CB_FONT     0x1D0
#define CB_TITLE    0x270
#define WIN_RECT    0x126 /* short left, right, top, bottom (screen) */
#define BUF_FONT    0x7C
#define BUF_COLORS  0x90
#define BUF_COLORS_N 0x80
#define GS_DEPTH    0x164
#define RENDER_SOLID 5
#define GAME_FLAGS  0x20
#define GAME_NUKES  0x6E0 /* nuclear detonations so far */
#define CD_FLAGS    0x2BE
#define C_ARM       0xD14 /* Constants: ARMAGEDDON, ARMAGEDDON_PER_NATION, ARMAGEDDON_PER_TEAM */
#define BIT         14    /* None */
#define POS_SHIFT   15    /* the slider's stop, 7 bits */
#define POS_MASK    0x7Fu
#define CLICK_SOUND 0x111
#define UNLIMITED   1000000

#define NUCLEAR_MISSILE 0x13B
#define NUCLEAR_ICBM    0x13C
#define INTERCEPTOR     0x183 /* nuke-fx's (unit 337, the campaign SAS slot without it) */

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef void(__attribute__((thiscall)) *fn1_t)(void *self, int a);
typedef void(__attribute__((thiscall)) *fn_xy_t)(void *self, int x, int y);
typedef int(__attribute__((thiscall)) *redraw_t)(void *self, void *box, int id);
typedef int(__attribute__((thiscall)) *clicked_t)(void *self, int id);
typedef void(__attribute__((thiscall)) *str_ctor_t)(void *self, const char *s);
typedef void *(__attribute__((thiscall)) *assign_t)(void *self, const void *src);
typedef void *(__attribute__((thiscall)) *find_t)(void *self, const void *name, int a);
typedef void *(__attribute__((thiscall)) *find_tex_t)(void *self, const void *name, int a, int b);
typedef unsigned char(__attribute__((thiscall)) *is_host_t)(void *self);
typedef void(__attribute__((thiscall)) *mouse_t)(void *self, int *x, int *y);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef int (*count_t)(void);
typedef int(__attribute__((thiscall)) *text_width_t)(void *self, const void *str);

/* The counts between None and Unlimited: wider steps to the right. */
static const short g_counts[] = { 1,  2,   3,   4,   5,   6,   7,   8,   9,   10,  12,  14, 16,
                                  18, 20,  22,  24,  26,  28,  30,  35,  40,  45,  50,  55, 60,
                                  70, 80,  90,  100, 120, 140, 160, 180, 200, 225, 250 };
#define N_COUNTS ((int)(sizeof g_counts / sizeof g_counts[0]))
#define N_STOPS  (N_COUNTS + 2) /* None, the counts, Unlimited */
#define POS_RULE (-1)           /* the game's rule */
#define POS_NONE 0
#define POS_UNLIMITED (N_COUNTS + 1)

int g_nonukes_enabled;
static char *g_box; /* the box of the setup screen now open */
static char *g_sw;
static int g_pos = POS_RULE; /* single player: the slider; multiplayer: the host while dragging */
static int g_drag;
static const void *g_tex_l, *g_tex_m, *g_tex_r, *g_tex_caret;
static void *g_vt[VT_COPY / 4 + 1]; /* [0] = RTTI, then the box's own vtable */
static void *g_box_move, *g_box_ldown, *g_box_lup;
static int g_track_w; /* the track's width in the box, set as it is drawn (the label takes the rest) */

void nonukes_load_config(const char *ini_path)
{
    g_nonukes_enabled = GetPrivateProfileIntA("gameplay", "no_nukes_option", 1, ini_path);
}

static char *game(void) { return *(char **)VA(VA_GAME_PTR); }

static unsigned *game_flags(void)
{
    char *g = game();
    return g ? (unsigned *)(g + GAME_FLAGS) : NULL;
}

static int pos_of(unsigned f)
{
    if (f >> BIT & 1)
        return POS_NONE;
    unsigned p = f >> POS_SHIFT & POS_MASK;
    return p >= 1 && p <= POS_UNLIMITED ? (int)p : POS_RULE;
}

static void set_pos(unsigned *f, int pos)
{
    if (!f)
        return;
    *f &= ~(1u << BIT | POS_MASK << POS_SHIFT);
    if (pos == POS_NONE)
        *f |= 1u << BIT;
    else if (pos > 0)
        *f |= (unsigned)pos << POS_SHIFT;
}

/* The game's chosen count: 0 = none chosen (the game's rule, or None), else the count. */
static int chosen_limit(void)
{
    unsigned *f = game_flags();
    int pos = f ? pos_of(*f) : POS_RULE;
    if (pos <= 0)
        return 0;
    return pos == POS_UNLIMITED ? UNLIMITED : g_counts[pos - 1];
}

int nonukes_type_blocked(int type)
{
    if (!g_nonukes_enabled || (type != NUCLEAR_MISSILE && type != NUCLEAR_ICBM && type != INTERCEPTOR))
        return 0;
    unsigned *f = game_flags();
    return f && (*f >> BIT & 1);
}

static int in_flight(void) { return nukefx_nukes_in_flight(); }

/* armageddon.c's can_nuke: a chosen count is reached by the nukes gone off and those in the air. */
int nonukes_launch_full(void)
{
    int n = g_nonukes_enabled ? chosen_limit() : 0;
    char *g = game();
    return n > 0 && n < UNLIMITED && g && *(int *)(g + GAME_NUKES) + in_flight() >= n;
}

/* Object::do_launch, the computer's launch: it stops at get_armageddon() - 2. With a count
   chosen it stops when the nukes gone off and those in the air reach the count. */
__attribute__((used)) int c_nn_ai_armageddon(void)
{
    int n = chosen_limit();
    if (n <= 0)
        return ((count_t)VA(VA_ARMAGEDDON))();
    return n >= UNLIMITED ? n : n + 2 - in_flight();
}

/* ---- the rule: rules.xml's Armageddon constants ------------------------------------------- */

static int g_orig[3], g_written[3], g_have_orig, g_clock_hidden;
static void style_reads(int off);

static void patch_byte2(UINT_PTR at, unsigned char a, unsigned char b)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, 2, PAGE_EXECUTE_READWRITE, &old);
    p[0] = a;
    p[1] = b;
    VirtualProtect(p, 2, old, &old);
}

static void apply(void)
{
    char *c = *(char **)VA(VA_CONSTANTS);
    if (!c)
        return;
    int *k = (int *)(c + C_ARM);
    if (!g_have_orig || memcmp(k, g_written, sizeof g_written) != 0) {
        memcpy(g_orig, k, sizeof g_orig); /* rules.xml's (also after the game read it again) */
        g_have_orig = 1;
    }
    int n = chosen_limit(), want[3] = { n, 0, 0 };
    if (n <= 0)
        memcpy(want, g_orig, sizeof want);
    if (memcmp(k, want, sizeof want) != 0) {
        memcpy(k, want, sizeof want);
        if (n > 0)
            logf_("nukes: Armageddon clock %s", n >= UNLIMITED ? "off (unlimited)" : "set to the chosen count");
    }
    memcpy(g_written, want, sizeof g_written);
    style_reads(n > 0);
    int hide = n >= UNLIMITED;
    if (hide != g_clock_hidden) {
        patch_byte2(VA_CLOCK_JNE, hide ? 0x90 : 0x75, hide ? 0x90 : 0x0F);
        g_clock_hidden = hide;
    }
}

/* Every simulation frame, every machine (airdrop.c's Objects::process_all hook). */
static void stop_kept(void);

void nonukes_frame(void)
{
    if (!g_nonukes_enabled)
        return;
    apply();
    stop_kept();
    unitbans_frame();
}

/* ---- the count is exact: a nuke that would go over does not fire ------------------------- */

#define MAX_KEPT 32
static void *g_orig_fire, *g_orig_die;
static char *g_kept_now;                          /* the nuke refused this moment: skip its die */
static struct { int p, id; } g_kept[MAX_KEPT];    /* nukes whose orders are cleared at frame end */
static int g_n_kept;

typedef void(__attribute__((thiscall)) *fire_t)(void *self, int a, int b);
typedef void(__attribute__((thiscall)) *die_t)(void *self, int a, int b, float c);

static int is_nuke(char *o)
{
    char *td = o ? *(char **)(o + 0x18) : NULL;
    int t = td ? *(int *)(td + 4) : 0;
    return t == NUCLEAR_MISSILE || t == NUCLEAR_ICBM;
}

__attribute__((used, thiscall)) static void c_nn_fire_ammo(char *o, int a, int b)
{
    if (is_nuke(o) && nonukes_launch_full()) {
        g_kept_now = o;
        if (g_n_kept < MAX_KEPT)
            g_kept[g_n_kept].p = (unsigned char)o[9], g_kept[g_n_kept++].id = *(short *)(o + 0xA);
        static int said;
        if (said++ < 10)
            logf_("nukes: a nuke of player %d stays in its silo, the chosen count is used up", (unsigned char)o[9]);
        return;
    }
    ((fire_t)g_orig_fire)(o, a, b);
}

__attribute__((used, thiscall)) static void c_nn_die(char *o, int a, int b, float c)
{
    if (o && o == g_kept_now) {
        g_kept_now = NULL; /* the missile that did not fire is not used up */
        return;
    }
    ((die_t)g_orig_die)(o, a, b, c);
}

static void stop_kept(void)
{
    g_kept_now = NULL;
    char *objs = *(char **)VA(VA_OBJECTS);
    for (int i = 0; i < g_n_kept && objs; i++) {
        int p = g_kept[i].p, id = g_kept[i].id;
        char **arr = p >= 0 && p < 8 ? *(char ***)(objs + 0x14 + 0x1C * p) : NULL;
        char *o = arr && id >= 0 ? arr[id] : NULL;
        if (o && (o[8] & 1) && is_nuke(o))
            ((fn0_t)VA(VA_CLEAR_ORDERS))(o);
    }
    g_n_kept = 0;
}

/* The six places that multiply the clock by the game style: `mov reg8, [reg+0x2D]` (3 bytes),
   and the `xor reg, reg; nop` that reads style 0 instead. */
static const struct {
    UINT_PTR at;
    unsigned char orig[3], zero[3];
} g_style_reads[] = {
    { 0x00594049u, { 0x8A, 0x42, 0x2D }, { 0x31, 0xC0, 0x90 } }, /* Game::get_armageddon */
    { 0x005928B3u, { 0x8A, 0x46, 0x2D }, { 0x31, 0xC0, 0x90 } }, /* Game::check_victory */
    { 0x005929D4u, { 0x8A, 0x46, 0x2D }, { 0x31, 0xC0, 0x90 } },
    { 0x0080C807u, { 0x8A, 0x56, 0x2D }, { 0x31, 0xD2, 0x90 } }, /* IFaceMainBase::draw_timers */
    { 0x0080C96Eu, { 0x8A, 0x46, 0x2D }, { 0x31, 0xC0, 0x90 } },
    { 0x00584389u, { 0x8A, 0x4E, 0x2D }, { 0x31, 0xC9, 0x90 } }, /* the end of game achievement */
};
#define N_STYLE_READS ((int)(sizeof g_style_reads / sizeof g_style_reads[0]))
static int g_style_ok, g_style_off;

static void style_reads(int off)
{
    if (!g_style_ok || off == g_style_off)
        return;
    for (int i = 0; i < N_STYLE_READS; i++) {
        unsigned char *p = (unsigned char *)VA(g_style_reads[i].at);
        DWORD old;
        VirtualProtect(p, 3, PAGE_EXECUTE_READWRITE, &old);
        memcpy(p, off ? g_style_reads[i].zero : g_style_reads[i].orig, 3);
        VirtualProtect(p, 3, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    g_style_off = off;
}

/* ---- the setup screen ----------------------------------------------------------------------- */

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

static void *game_string(const char *s, unsigned char str[64])
{
    memset(str, 0, 64);
    ((str_ctor_t)VA(VA_STRING_CTOR))(str, s);
    return str;
}

static char *find_box(char *sw)
{
    char *rules = sw + SW_RULES; /* setup_checks: lea ecx, [sw+0x2A6C] / call [[ecx]+0x438] */
    unsigned char str[64];
    game_string("nonukes", str);
    char *box = ((find_t)(*(void ***)rules)[VT_FIND / 4])(rules, str, 0);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    return box;
}

static const void *find_tex(char *sw, const char *name)
{
    unsigned char str[64];
    game_string(name, str);
    const void *t = ((find_tex_t)(*(void ***)sw)[VT_FIND_TEX / 4])(sw, str, -1, 0);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    return t;
}

/* The value shown: the local one in single player and while the host drags, else the lobby's. */
static int shown_pos(void)
{
    if (!g_sw || solo(g_sw) || g_drag)
        return g_pos;
    unsigned *lf = lobby_flags();
    return lf ? pos_of(*lf) : POS_RULE;
}

static int may_change(void) { return g_sw && (solo(g_sw) || is_host()); }

/* The box's size and the track inside it (client pixels). */
struct layout {
    int w, h, tl, tr, kw;
};

static struct layout layout_of(char *box)
{
    short *r = (short *)(box + WIN_RECT);
    struct layout L;
    L.w = r[1] - r[0];
    L.h = r[3] - r[2];
    L.kw = L.h * 17 / 28; /* the caret's shape, 17 x 28 */
    int tw = g_track_w > 0 ? g_track_w : L.w * 52 / 100;
    L.tl = L.w - tw + L.kw / 2; /* where the knob's centre runs */
    L.tr = L.w - L.kw / 2 - 1;
    return L;
}

static int knob_x(const struct layout *L, int pos)
{
    return L->tl + (L->tr - L->tl) * pos / (N_STOPS - 1);
}

static void commit(void)
{
    if (!g_sw)
        return;
    ((fn1_t)VA(VA_SOUND_PLAY))(g_sw, CLICK_SOUND);
    if (!solo(g_sw)) {
        set_pos(lobby_flags(), g_pos);
        ((fn0_t)VA(VA_SET_CHANGED))(g_sw); /* the host's Apply sends it */
    }
    logf_("nukes: slider at stop %d (%d)", g_pos, g_pos > 0 && g_pos < POS_UNLIMITED ? g_counts[g_pos - 1] : g_pos);
}

static void follow_mouse(char *box)
{
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(box, &mx, &my);
    struct layout L = layout_of(box);
    int span = L.tr - L.tl;
    if (span <= 0)
        return;
    int pos = ((mx - L.tl) * (N_STOPS - 1) * 2 + span) / (2 * span); /* the nearest stop */
    g_pos = pos < 0 ? 0 : pos > N_STOPS - 1 ? N_STOPS - 1 : pos;
}

/* The box's own vtable slots. */
__attribute__((used, thiscall)) static void c_box_ldown(char *box, int x, int y)
{
    (void)x, (void)y;
    if (box != g_box || !may_change())
        return;
    if (!solo(g_sw))
        g_pos = shown_pos();
    g_drag = 1;
    if (!*(void **)VA(VA_MOUSE_CAPTURE)) { /* as Button::on_left_down */
        *(void **)VA(VA_MOUSE_CAPTURE) = box;
        *(void **)VA(VA_NC_CAPTURE) = NULL;
    }
    follow_mouse(box);
}

__attribute__((used, thiscall)) static void c_box_move(char *box, int x, int y)
{
    ((fn_xy_t)g_box_move)(box, x, y); /* the tooltip */
    if (box == g_box && g_drag)
        follow_mouse(box);
}

__attribute__((used, thiscall)) static void c_box_lup(char *box, int x, int y)
{
    (void)x, (void)y;
    if (box != g_box || !g_drag)
        return;
    follow_mouse(box);
    g_drag = 0;
    ((fn0_t)VA(VA_RELEASE_MOUSE))(box);
    commit();
}

__attribute__((used, thiscall)) static void c_box_nothing(char *box, int x, int y) { (void)box, (void)x, (void)y; }

__attribute__((used, thiscall)) static void c_box_rdown(char *box, int x, int y)
{
    (void)x, (void)y;
    if (box != g_box || !may_change() || g_drag)
        return;
    g_pos = POS_RULE;
    commit();
}

/* The box gets a copy of its vtable with the mouse slots replaced: the game's other check
   boxes keep theirs. */
static void own_vtable(char *box)
{
    void **vt = *(void ***)box;
    if (vt == g_vt + 1)
        return; /* already ours */
    memcpy(g_vt, vt - 1, sizeof g_vt); /* RTTI pointer first */
    void **mine = g_vt + 1;
    g_box_move = vt[VT_MOVE / 4];
    g_box_ldown = vt[VT_LDOWN / 4];
    g_box_lup = vt[VT_LUP / 4];
    mine[VT_MOVE / 4] = (void *)c_box_move;
    mine[VT_CLICK / 4] = (void *)c_box_nothing;
    mine[VT_LDOWN / 4] = (void *)c_box_ldown;
    mine[VT_LUP / 4] = (void *)c_box_lup;
    mine[VT_RCLICK / 4] = (void *)c_box_nothing;
    mine[VT_RDOWN / 4] = (void *)c_box_rdown;
    *(void ***)box = mine;
}

/* Multiplayer has no Cooperative AI (setup_checks skips box 4 there). So that the setup screen
   looks the same as in single player, its box is shown anyway, greyed and dead: set up as
   setup_checks sets up a box (title from the category, font, colours), with an id no game code
   uses (so the game's check box logic and its slot 4, empty in multiplayer, stay untouched), a
   vtable copy without mouse slots (no click, no tooltip) and disabled. */
#define VA_CATEGORIES 0x00E80560u /* the gameinfo_flags categories, 0x58 each; the box title */
#define COOP_ID       16
static void *g_coop_vt[VT_COPY / 4 + 1];

__attribute__((used, thiscall)) static void c_coop_nothing(char *box, int x, int y) { (void)box, (void)x, (void)y; }

static void coop_ai_greyed(char *sw)
{
    if (solo(sw))
        return;
    char *rules = sw + SW_RULES;
    unsigned char str[64];
    game_string("check4", str);
    char *box = ((find_t)(*(void ***)rules)[VT_FIND / 4])(rules, str, 0);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    char *cats = *(char **)VA(VA_CATEGORIES);
    if (!box || !cats)
        return;
    ((assign_t)VA(VA_STRING_ASSIGN))(box + CB_TITLE, cats + 4 * 0x58);
    ((fn1_t)VA(VA_SET_ID))(box, COOP_ID);
    void **font = *(void ***)(sw + SW_FONT);
    if (font) {
        if (*font)
            *(void **)(box + CB_FONT) = font;
        *(int *)(box + CB_FONT + 4) = 0;
        *(int *)(box + CB_FONT + 8) = 0;
    }
    char *buf = box + CB_BUFFER;
    memcpy(buf + 0x90, (void *)VA(VA_WHITE), 12);
    memcpy(buf + 0xCC, (void *)VA(VA_COLOR_NONE), 12);
    buf[0x108] = 1;
    buf[0x10D] = 1;
    *(unsigned char *)(box + 0x284) &= ~1u; /* not ticked */
    void **vt = *(void ***)box;
    if (vt != g_coop_vt + 1) {
        memcpy(g_coop_vt, vt - 1, sizeof g_coop_vt);
        void **mine = g_coop_vt + 1;
        mine[VT_MOVE / 4] = mine[VT_CLICK / 4] = mine[VT_LDOWN / 4] = mine[VT_LUP / 4] = (void *)c_coop_nothing;
        mine[VT_RCLICK / 4] = mine[VT_RDOWN / 4] = (void *)c_coop_nothing;
        *(void ***)box = mine;
    }
    ((fn1_t)VA(VA_SET_ENABLED))(box, 0);
    ((fn1_t)(*(void ***)box)[VT_SHOW / 4])(box, 0);
}

/* After setup_checks: the box set up as the game sets up its own (font, colours), its own mouse
   handling, the art. */
__attribute__((used, thiscall)) void c_nn_setup_checks(char *sw)
{
    ((fn0_t)VA(VA_SETUP_CHECKS))(sw);
    survival_setup_checks(sw); /* the Survival setup: one computer player, its rows hidden */
    g_sw = sw;
    g_drag = 0;
    g_track_w = 0; /* measured again at the first drawing, in this screen's font */
    unitbans_setup(sw);  /* the Units button beside the slider */
    coop_ai_greyed(sw); /* multiplayer: the same layout as single player */
    g_box = find_box(sw);
    if (!g_box) {
        static int said;
        if (!said++)
            logf_("nukes: no nonukes box in setupwin.xml (ui-theme not installed?)");
        return;
    }
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text("Nukes"));
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
    own_vtable(g_box);
    g_tex_l = find_tex(sw, "nukes_track_l");
    g_tex_m = find_tex(sw, "nukes_track_m");
    g_tex_r = find_tex(sw, "nukes_track_r");
    g_tex_caret = find_tex(sw, "nukes_caret");
    unsigned *f = game_flags();
    g_pos = f ? pos_of(*f) : POS_RULE;
    ((fn1_t)(*(void ***)g_box)[VT_SHOW / 4])(g_box, 0);
    if (survival_setup_active()) /* the Survival setup: no slider; the Units button chooses the hero */
        survival_hide_box(g_box);
    static int said;
    if (!said++)
        logf_("nukes: slider in the game setup (%d stops, art %s)", N_STOPS,
              g_tex_m && g_tex_caret ? "found" : "missing, flat look");
}

static void add(const void *tex, int l, int t, int r, int b, unsigned color, int type)
{
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, type);
}

static void label(char *box, int r, int h, const char *s, unsigned rgb)
{
    char *buf = box + CB_BUFFER;
    unsigned char saved[BUF_COLORS_N];
    memcpy(saved, buf + BUF_COLORS, BUF_COLORS_N);
    _Alignas(4) unsigned char color[12];
    memcpy(color, (void *)VA(VA_WHITE), 12);
    color[0] = (unsigned char)(rgb >> 16);
    color[1] = (unsigned char)(rgb >> 8);
    color[2] = (unsigned char)rgb;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, (void *)VA(VA_BLACK), -1, 0);
    unsigned char str[64] = { 0 };
    lang_string(str, s);
    short rect[4] = { 0, (short)r, 0, (short)h };
    int len = *(unsigned short *)(str + 8);
    if (len)
        ((write_t)VA(VA_WRITE_RIGHT))(buf, str, rect, len);
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    memcpy(buf + BUF_COLORS, saved, BUF_COLORS_N);
}

static void label_text(int pos, char *text, int size)
{
    char value[32];
    if (pos == POS_RULE)
        snprintf(value, sizeof value, "%s", lang_text("game rule"));
    else if (pos == POS_NONE)
        snprintf(value, sizeof value, "%s", lang_text("none"));
    else if (pos == POS_UNLIMITED)
        snprintf(value, sizeof value, "%s", lang_text("unlimited"));
    else
        snprintf(value, sizeof value, "%d", g_counts[pos - 1]);
    snprintf(text, size, "%s: %s", lang_text("Nukes"), value);
}

/* The track takes about half the box, less when the longest label (in this language and font)
   needs the room. */
static void fit_track(char *box)
{
    short *r = (short *)(box + WIN_RECT);
    int w = r[1] - r[0], widest = 0;
    char text[96];
    for (int pos = POS_RULE; pos <= POS_UNLIMITED; pos = pos == 1 ? POS_UNLIMITED - 1 : pos + 1) {
        label_text(pos, text, sizeof text);
        unsigned char str[64] = { 0 };
        lang_string(str, text);
        int tw = ((text_width_t)VA(VA_TEXT_WIDTH))(box + CB_BUFFER, str);
        ((fn0_t)VA(VA_STRING_CLOSE))(str);
        if (tw > widest)
            widest = tw;
    }
    int track = w - widest - 14, lo = w * 30 / 100, hi = w * 55 / 100;
    g_track_w = track < lo ? lo : track > hi ? hi : track;
}

static void draw_slider(char *box, int enabled)
{
    if (g_track_w <= 0)
        fit_track(box);
    struct layout L = layout_of(box);
    int pos = shown_pos();
    unsigned white = *(unsigned *)VA(VA_WHITE);
    int bl = L.tl - L.kw / 2, br = L.w, th = L.h * 12 / 21, tt = (L.h - th) / 2;
    if (g_tex_l && g_tex_m && g_tex_r) {
        add(g_tex_l, bl, tt, bl + 5, tt + th, white, 1);
        add(g_tex_m, bl + 5, tt, br - 5, tt + th, white, 1);
        add(g_tex_r, br - 5, tt, br, tt + th, white, 1);
    } else {
        add(NULL, bl, tt, br, tt + th, 0xFF404850u, RENDER_SOLID);
    }
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
    (*depth)++; /* the fill, knob and label in front of the track */
    if (pos >= 0) {
        int kx = knob_x(&L, pos), kl = kx - L.kw / 2;
        if (kl > bl + 3)
            add(NULL, bl + 3, tt + 3, kl + 2, tt + th - 3, enabled ? 0xD03CBEE6u : 0xC0808080u, RENDER_SOLID);
        if (g_tex_caret)
            add(g_tex_caret, kl, 0, kl + L.kw, L.h, enabled ? white : 0xFF909090u, 1);
        else
            add(NULL, kl + 2, 1, kl + L.kw - 2, L.h - 1, enabled ? 0xFFE8F0F0u : 0xFF808080u, RENDER_SOLID);
    }
    char text[96];
    label_text(pos, text, sizeof text);
    label(box, bl - 8, L.h, text, enabled ? 0xF0F0F0u : 0x8A9098u);
    (*depth)--;
}

/* The box is drawn by hand; the game's other boxes as before. */
__attribute__((used, thiscall)) int c_nn_check_redraw(char *sw, char *box, int id)
{
    if (survival_hidden_check(id)) { /* shown again by an update: hidden again */
        survival_hide_box(box);
        return 1;
    }
    if (unitbans_check_redraw(sw, box, id))
        return 1;
    if (!box || box != g_box || id != BIT)
        return ((redraw_t)VA(VA_ON_REDRAW))(sw, box, id);
    if (g_drag && !(GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) & 0x8000)) {
        g_drag = 0; /* the button came up where the box never heard of it */
        ((fn0_t)VA(VA_RELEASE_MOUSE))(box);
        commit();
    }
    int enabled = may_change();
    if (!solo(sw) && enabled != ((*(int *)(box + CB_FLAGS) & 8) == 0))
        ((fn1_t)VA(VA_SET_ENABLED))(box, enabled);
    draw_slider(box, enabled);
    return 1;
}

/* Clicks come through the box's own slots; the game's path must not toggle anything. */
__attribute__((used, thiscall)) int c_nn_check_clicked(char *sw, int id)
{
    if (id != BIT || !g_box)
        return ((clicked_t)VA(VA_ON_CLICKED))(sw, id);
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

/* Every machine, as the game starts: the choice into Game+0x20, which the rule reads. */
__attribute__((used, thiscall)) int c_nn_setup_game(char *sw)
{
    unsigned *lf = lobby_flags();
    int pos = solo(sw) ? (find_box(sw) ? g_pos : POS_RULE) : lf ? pos_of(*lf) : POS_RULE;
    set_pos(game_flags(), pos);
    g_drag = 0;
    apply();
    unitbans_setup_game(sw);
    if (pos == POS_NONE)
        logf_("nukes: the game starts WITHOUT nuclear weapons");
    else if (pos == POS_RULE)
        logf_("nukes: the game starts with the game's Armageddon rule");
    else if (pos == POS_UNLIMITED)
        logf_("nukes: the game starts with unlimited nuclear weapons, no Armageddon clock");
    else
        logf_("nukes: the game starts with %d nuclear weapons on the Armageddon clock", g_counts[pos - 1]);
    survival_fix_rules(sw); /* the Survival setup's fixed rules, before the game reads them */
    survival_before_game(sw); /* its battle director: added after a co-op test's allies, white */
    int r = ((fn0_t)VA(VA_SETUP_GAME))(sw);
    survival_setup_game(); /* after the game's: a game from the Survival button runs its script */
    return r;
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
    if (!g_nonukes_enabled) {
        g_unitbans_enabled = 0; /* the Units window rides on the slider's hooks */
        return;
    }
    const unsigned char *jne = (const unsigned char *)VA(VA_CLOCK_JNE);
    const unsigned char *sub = (const unsigned char *)VA(VA_CALL_AI_ARM + 0xB); /* sub eax, 2 */
    if (!call_to(VA_CALL_CHECKS, VA_SETUP_CHECKS) || !call_to(VA_CALL_GAME_1, VA_SETUP_GAME) ||
        !call_to(VA_CALL_GAME_2, VA_SETUP_GAME) || !call_to(VA_CALL_AI_ARM, VA_ARMAGEDDON) ||
        sub[0] != 0x83 || sub[1] != 0xE8 || sub[2] != 0x02 || jne[0] != 0x75 || jne[1] != 0x0F ||
        *(UINT_PTR *)VA(VT_CHECK_REDRAW) != VA(VA_ON_REDRAW) ||
        *(UINT_PTR *)VA(VT_CHECK_CLICKED) != VA(VA_ON_CLICKED) || *(UINT_PTR *)VA(VT_CHECK_MOVE) != VA(VA_ON_MOVE)) {
        logf_("nukes: setup screen code differs, the option is off");
        g_nonukes_enabled = g_unitbans_enabled = 0;
        return;
    }
    patch_call(VA_CALL_CHECKS, c_nn_setup_checks);
    patch_call(VA_CALL_GAME_1, c_nn_setup_game);
    patch_call(VA_CALL_GAME_2, c_nn_setup_game);
    patch_call(VA_CALL_AI_ARM, c_nn_ai_armageddon);
    g_style_ok = 1;
    for (int i = 0; i < N_STYLE_READS; i++)
        if (memcmp((void *)VA(g_style_reads[i].at), g_style_reads[i].orig, 3) != 0)
            g_style_ok = 0;
    if (!g_style_ok)
        logf_("nukes: the game style reads differ, a chosen count is multiplied by the style");
    static const unsigned char fire_sig[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x54 };
    static const unsigned char die_sig[9] = { 0x55, 0x8B, 0xEC, 0x51, 0xF3, 0x0F, 0x10, 0x45, 0x10 };
    unsigned char *fire = (unsigned char *)VA(VA_FIRE_AMMO), *die = (unsigned char *)VA(VA_DIE);
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (mem && memcmp(fire, fire_sig, 6) == 0 && memcmp(die, die_sig, 9) == 0) {
        g_orig_fire = hook_trampoline(fire, 6, mem);
        g_orig_die = hook_trampoline(die, 9, mem + 32);
        hook_jmp5(fire, (void *)c_nn_fire_ammo);
        hook_jmp5(die, (void *)c_nn_die);
    } else
        logf_("nukes: fire_ammo / die differ, a salvo can still go over the chosen count");
    patch_slot(VT_CHECK_REDRAW, c_nn_check_redraw);
    patch_slot(VT_CHECK_CLICKED, c_nn_check_clicked);
    patch_slot(VT_CHECK_MOVE, c_nn_check_move);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("nukes: slider in the game setup (Game+0x20 bit %d = none, bits %d.. = the count)", BIT, POS_SHIFT);
    unitbans_install();
}
