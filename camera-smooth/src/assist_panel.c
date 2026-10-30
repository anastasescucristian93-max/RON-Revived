/*
 * The AI assistant panel: a small window opened by the assistant button (qol.c) with the
 * control switch, a checkbox per kind of purchase and a slider for the resource reserve
 * (assistant.c holds the settings and saves them to ron_camera.ini).
 *
 * The switch hands your nation to the computer and takes it back, like the console commands
 * "computer <name>" and "human <name>": one click on FULL CONTROL ticks every kind and lets the
 * computer play, one click on NO CONTROL takes it back. With some kinds unticked while the
 * computer plays, the switch reads CUSTOM. The game's own ScenarioFuncSet::change_to_ai /
 * change_to_human do the switching (the human flag of the leader, bit 2 of [leader + 0], and
 * the AI flag of every unit). Single player only: in a multiplayer game only this machine
 * would know, and the game would go out of sync.
 *
 * The window is a native IFaceButton, a child of the Console window (the in-game screen),
 * so the game positions, clips, layers and clicks it like its own interface. Its contents
 * are drawn by hand in IFaceButton::on_redraw (panel.c's hook asks assist_panel_redraw):
 * pieces of art\revived\assist_panel.tga (IFACETEX "assist_*", made by
 * ui-theme\tools\build_assist_panel.py at the sizes below) through IFaceRenderManager::add,
 * and text through the window's own Buffer (GraphicWin + 0x154), one depth step in front.
 * Without the textures it falls back to flat rectangles. A click arrives as a click on the
 * button (IFaceMainBase::on_button_clicked, hooked in ron_camera.c / qol.c); the mouse
 * position inside it comes from Window::get_mouse_pos.
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_IFACEBUTTON_CTOR 0x007FC190u /* IFaceButton::IFaceButton(int most_derived) */
#define VA_BUTTON_INIT      0x00524E60u /* Button::init(const String &, id, x, y, w, h, Window *, int) */
#define VA_EMPTY_STRING     0x00EB437Cu
#define VA_WINDOW_MOVE      0x00A54080u /* Window::move(x, y) */
#define VA_WINDOW_SHOW      0x00A55B30u /* Window::show(int) */
#define VA_WINDOW_HIDE      0x00A54780u /* Window::hide() */
#define VA_BRING_TO_TOP     0x00A53F90u /* Window::bring_to_top() */
#define VA_GET_MOUSE_POS    0x00A54170u /* Window::get_mouse_pos(int *x, int *y): client coordinates */
#define VA_IFACE_RM         0x00EBD1C0u /* IFaceRenderManager */
#define VA_RM_ADD_RECT      0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_STRING_CTOR      0x00A1D660u
#define VA_STRING_CLOSE     0x00A1CF40u
#define VA_WRITE_L          0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len) */
#define VA_WRITE_CENT       0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len): centred both ways */
#define VA_SET_TEXT_COLOR   0x0050DAF0u /* Buffer::set_text_color(const Color &, const Color &, int, int) */
#define VA_FONTS            0x00ED8160u /* FontFamily fonts */
#define VA_FONT_GET         0x00522810u /* FontFamily::get(size, style) const: the loaded font closest to it */
#define VA_WHITE            0x00C8D26Cu /* Color WHITE (12 bytes: R, G, B, A, ...) */
#define VA_BLACK            0x00C8D260u
#define VA_GRAPHSYS         0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define VA_TO_AI            0x009E5D20u /* ScenarioFuncSet::change_to_ai(const int &player), player 1-8, this unused */
#define VA_TO_HUMAN         0x009E5C80u /* ScenarioFuncSet::change_to_human(const int &player) */
#define VA_GAME_PTR         0x00C061ECu
#define VA_CONSOLE_PTR      0x00C06210u /* Console*: +0x298 local player */
#define VA_LEADERS          0x00E3A390u

#define IFB_KIND     0x32C
#define IFB_FLAGS    0x3BC
#define WIN_FLAGS    0x08  /* bit 0: visible */
#define WIN_RECT     0x126 /* short left, right, top, bottom (screen) */
#define WIN_BUFFER   0x154 /* Buffer */
#define BUF_FONT     0x7C  /* Font *, then two more words */
#define BUF_COLORS   0x90  /* what set_text_color changes */
#define BUF_COLORS_N 0x80
#define GS_DEPTH     0x164
#define RENDER_SOLID 5
#define LEADER_SIZE  0x6EEC
#define LD_HUMAN     4     /* [leader + 0]: bit 2 set = a human plays it */
#define GAME_FLAGS   0x820 /* byte, bit 2: multiplayer game */

/* layout, in window pixels; the art (build_assist_panel.py) has the same sizes */
#define PW        320
#define PH        352
#define TITLE_H   36
#define BOX       24
#define TG_L      16 /* control switch */
#define TG_T      50
#define TG_W      288
#define TG_H      40
#define PLATE_W   140
#define PLATE_H   34
#define STATUS_T  94
#define DIV1_T    120
#define HEAD_T    126
#define ROWS_T    148
#define ROW_H     28
#define DIV2_T    294
#define RES_T     300
#define SL_L      24 /* reserve slider */
#define SL_T      326
#define SL_W      272
#define SL_H      14
#define KNOB      22
#define SL_IN     8 /* the value runs from SL_L + SL_IN to SL_L + SL_W - SL_IN */
#define RESERVE_MAX  5000
#define RESERVE_STEP 100

/* text colours, 0xRRGGBB */
#define T_GOLD  0xE6BE3Cu
#define T_WHITE 0xF0F0F0u
#define T_GREY  0x8A9098u
#define T_DIM   0x5A6068u

/* flat fallback colours: 0xAABBGGRR */
#define C_BACK   0xEC141A20u
#define C_TITLE  0xFF2A3644u
#define C_BORDER 0xFF3CBEE6u /* gold */
#define C_BOX    0xFF20262Cu
#define C_TICK   0xFF40D040u
#define C_TRACK  0xFF404850u
#define C_HANDLE 0xFFE8F0F0u

typedef int(__attribute__((thiscall)) *fn_i_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *button_init_t)(void *self, const void *text, int id, int x, int y, int w, int h,
                                                      void *parent, int flag);
typedef int(__attribute__((thiscall)) *fn_resize)(void *self, int w, int h, int a, int b);
typedef int(__attribute__((thiscall)) *move_t)(void *self, int x, int y);
typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int a);
typedef void(__attribute__((thiscall)) *mouse_t)(void *self, int *x, int *y);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef void(__attribute__((thiscall)) *fn_v_cstr)(void *self, const char *s);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);
typedef int(__attribute__((thiscall)) *change_t)(void *self, const int *player);

void *qol_iface_tex(const char *name);

static _Alignas(16) unsigned char g_panel[0x400];
static int g_made, g_ready;

int assist_panel_is(void *btn) { return g_ready && btn == g_panel; }

static int is_open(void) { return g_ready && (*(unsigned *)(g_panel + WIN_FLAGS) & 1); }

/* Called with the interface (for its font) and the Console window (the parent) whenever
   the game builds its in-game interface. */
void assist_panel_create(char *iface, char *parent)
{
    if (!parent)
        return;
    if (!g_made) {
        ((fn_i_i)VA(VA_IFACEBUTTON_CTOR))(g_panel, 1);
        g_made = 1;
    }
    int err = ((button_init_t)VA(VA_BUTTON_INIT))(g_panel, (void *)VA(VA_EMPTY_STRING), 0, 0, 0, 1, 1, parent, 0);
    if (err) {
        logf_("assistant panel: init error %d", err);
        g_ready = 0;
        return;
    }
    *(int *)(g_panel + IFB_KIND) = 0;
    *(unsigned short *)(g_panel + IFB_FLAGS) |= 1;
    ((fn_resize)(*(void ***)g_panel)[8])(g_panel, PW, PH, 0, 0);
    /* the window's text buffer may have no font of its own: use the interface's */
    void **font = (void **)(g_panel + WIN_BUFFER + BUF_FONT);
    if (!*font && iface)
        *font = *(void **)(iface + WIN_BUFFER + BUF_FONT);
    ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
    g_ready = 1;
    logf_("assistant panel: made (font %p)", *font);
}

void assist_panel_toggle(char *parent)
{
    if (!g_ready)
        return;
    if (is_open()) {
        ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
        return;
    }
    short *pr = parent ? (short *)(parent + WIN_RECT) : NULL;
    int w = pr ? pr[1] - pr[0] : 1024, h = pr ? pr[3] - pr[2] : 768;
    ((move_t)VA(VA_WINDOW_MOVE))(g_panel, (w - PW) / 2, (h - PH) / 2 - h / 8);
    ((fn_v_i)VA(VA_WINDOW_SHOW))(g_panel, 1);
    ((fn_v)VA(VA_BRING_TO_TOP))(g_panel);
}

/* ---- who plays the local nation ------------------------------------------------------ */

static int local_player(void)
{
    char *console = *(char **)VA(VA_CONSOLE_PTR);
    int p = console ? *(int *)(console + 0x298) : -1;
    return p >= 0 && p < 8 ? p : -1;
}

static int multiplayer(void)
{
    char *game = *(char **)VA(VA_GAME_PTR);
    return !game || (game[GAME_FLAGS] & 4);
}

static int computer_plays(void)
{
    int p = local_player();
    return p >= 0 && !(*(unsigned *)(VA(VA_LEADERS) + p * LEADER_SIZE) & LD_HUMAN);
}

static void set_computer(int on)
{
    int p = local_player();
    if (p < 0 || multiplayer() || computer_plays() == on)
        return;
    int n = p + 1; /* the scenario functions count players from 1 */
    ((change_t)VA(on ? VA_TO_AI : VA_TO_HUMAN))(NULL, &n);
    logf_("assistant panel: %s", on ? "the computer plays your nation" : "you play your nation again");
}

static int all_allowed(void)
{
    for (int k = 0; k < assistant_kinds(); k++)
        if (!assistant_allowed(k))
            return 0;
    return 1;
}

/* ---- drawing ------------------------------------------------------------------------- */

static int g_have_art;

static void add(const void *tex, int l, int t, int r, int b, unsigned color, int type)
{
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, type);
}

static void fill(int l, int t, int r, int b, unsigned color) { add(NULL, l, t, r, b, color, RENDER_SOLID); }

static void frame(int l, int t, int r, int b, unsigned color)
{
    fill(l, t, r, t + 1, color);
    fill(l, b - 1, r, b, color);
    fill(l, t, l + 1, b, color);
    fill(r - 1, t, r, b, color);
}

/* Draws IFACETEX "assist_<name>" stretched to the rectangle; 0 when it is missing. */
static int piece(const char *name, int l, int t, int r, int b)
{
    char full[32];
    snprintf(full, sizeof full, "assist_%s", name);
    const void *tex = qol_iface_tex(full);
    if (tex)
        add(tex, l, t, r, b, *(unsigned *)VA(VA_WHITE), 1);
    return tex != NULL;
}

/* Text in the given font size and colour (0xRRGGBB), left-aligned or centred in the box. */
static void text(int l, int t, int r, int b, const char *s, int size, int bold, unsigned rgb, int centred)
{
    char *buf = (char *)g_panel + WIN_BUFFER;
    void *font = ((font_get_t)VA(VA_FONT_GET))((void *)VA(VA_FONTS), size, bold ? 1 : 0);
    if (!font && !*(void **)(buf + BUF_FONT))
        return;
    unsigned char saved[12], saved_colors[BUF_COLORS_N];
    memcpy(saved, buf + BUF_FONT, 12);
    memcpy(saved_colors, buf + BUF_COLORS, BUF_COLORS_N);
    if (font) {
        *(void **)(buf + BUF_FONT) = font;
        *(int *)(buf + BUF_FONT + 4) = 0;
        *(int *)(buf + BUF_FONT + 8) = 0;
    }
    _Alignas(4) unsigned char color[12];
    memcpy(color, (void *)VA(VA_WHITE), 12);
    color[0] = (unsigned char)(rgb >> 16);
    color[1] = (unsigned char)(rgb >> 8);
    color[2] = (unsigned char)rgb;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, (void *)VA(VA_BLACK), -1, 0);
    unsigned char str[64] = { 0 };
    lang_string(str, lang_text(s));
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    int len = *(unsigned short *)(str + 8);
    if (len)
        ((write_t)VA(centred ? VA_WRITE_CENT : VA_WRITE_L))(buf, str, rect, len);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    memcpy(buf + BUF_FONT, saved, 12);
    memcpy(buf + BUF_COLORS, saved_colors, BUF_COLORS_N);
}

static int row_top(int k) { return ROWS_T + k * ROW_H; }

static int knob_x(int v)
{
    if (v > RESERVE_MAX)
        v = RESERVE_MAX;
    return SL_L + SL_IN + (SL_W - 2 * SL_IN) * v / RESERVE_MAX;
}

static int inside(int mx, int my, int l, int t, int r, int b) { return mx >= l && mx < r && my >= t && my < b; }

enum { S_NONE, S_CUSTOM, S_FULL };

static int state(void) { return !computer_plays() ? S_NONE : all_allowed() ? S_FULL : S_CUSTOM; }

/* The art, all from one atlas, in the order it must lie (quads of one texture are drawn in
   the order they were added). Returns 0 when the textures are missing. */
static int draw_art(int mx, int my, int st, int mp)
{
    if (!piece("back", 0, 0, PW, PH))
        return 0;
    int cx = PW - BOX - 8, cy = 6;
    piece(inside(mx, my, cx, cy, cx + BOX, cy + BOX) ? "close_hover" : "close", cx, cy, cx + BOX, cy + BOX);

    piece("track", TG_L, TG_T, TG_L + TG_W, TG_T + TG_H);
    int pl = st == S_NONE ? TG_L + 3 : TG_L + TG_W - 3 - PLATE_W, pt = TG_T + (TG_H - PLATE_H) / 2;
    if (!mp)
        piece(st == S_FULL ? "plate_full" : st == S_CUSTOM ? "plate_custom" : "plate_none", pl, pt, pl + PLATE_W,
              pt + PLATE_H);

    piece("line", 14, DIV1_T, PW - 14, DIV1_T + 2);
    for (int k = 0; k < assistant_kinds(); k++) {
        int t = row_top(k), hover = inside(mx, my, 14, t, PW - 14, t + ROW_H);
        if (hover)
            piece("glow", 14, t, PW - 14, t + ROW_H);
        const char *b = assistant_allowed(k) ? "box_on" : hover ? "box_hover" : "box_off";
        piece(b, 20, t + (ROW_H - BOX) / 2, 20 + BOX, t + (ROW_H - BOX) / 2 + BOX);
    }
    piece("line", 14, DIV2_T, PW - 14, DIV2_T + 2);

    piece("slider", SL_L, SL_T, SL_L + SL_W, SL_T + SL_H);
    int kx = knob_x(assistant_reserve());
    if (kx > SL_L + 4)
        piece("fill", SL_L + 4, SL_T + 4, kx, SL_T + SL_H - 4);
    piece("knob", kx - KNOB / 2, SL_T + SL_H / 2 - KNOB / 2, kx - KNOB / 2 + KNOB, SL_T + SL_H / 2 - KNOB / 2 + KNOB);
    return 1;
}

/* Without the art: flat rectangles. */
static void draw_flat(int st)
{
    fill(0, 0, PW, PH, C_BACK);
    fill(0, 0, PW, TITLE_H, C_TITLE);
    frame(0, 0, PW, PH, C_BORDER);
    frame(PW - BOX - 8, 6, PW - 8, 6 + BOX, C_BORDER);
    fill(TG_L, TG_T, TG_L + TG_W, TG_T + TG_H, C_TRACK);
    int pl = st == S_NONE ? TG_L + 3 : TG_L + TG_W - 3 - PLATE_W;
    fill(pl, TG_T + 3, pl + PLATE_W, TG_T + TG_H - 3, st == S_FULL ? 0xFF2E8A3Au : st == S_CUSTOM ? 0xFF1E7AB0u : 0xFF242A8Au);
    for (int k = 0; k < assistant_kinds(); k++) {
        int t = row_top(k) + (ROW_H - 14) / 2;
        fill(22, t, 36, t + 14, C_BOX);
        frame(22, t, 36, t + 14, C_BORDER);
        if (assistant_allowed(k))
            fill(25, t + 3, 33, t + 11, C_TICK);
    }
    int kx = knob_x(assistant_reserve());
    fill(SL_L, SL_T + 2, SL_L + SL_W, SL_T + SL_H - 2, C_TRACK);
    fill(SL_L, SL_T + 2, kx, SL_T + SL_H - 2, C_BORDER);
    fill(kx - 4, SL_T - 3, kx + 4, SL_T + SL_H + 3, C_HANDLE);
}

/* Draws the panel; returns 1 when `btn` is the panel (the game's drawing is skipped). */
int assist_panel_redraw(void *btn)
{
    if (!assist_panel_is(btn))
        return 0;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(g_panel, &mx, &my);
    int st = state(), mp = multiplayer();
    int art = draw_art(mx, my, st, mp);
    if (!art)
        draw_flat(st);
    if (g_have_art != art + 1) {
        g_have_art = art + 1;
        logf_("assistant panel: art %s", art ? "found" : "missing (interface.xml), flat look");
    }

    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH);
    (*depth)++; /* the text in front of the art */
    text(0, 0, PW, TITLE_H, "AI ASSISTANT", 14, 1, T_GOLD, 1);
    if (!art)
        text(PW - BOX - 8, 6, PW - 8, 6 + BOX, "X", 12, 1, T_GOLD, 1);

    int half = TG_W / 2;
    unsigned on = T_WHITE, off = T_DIM;
    text(TG_L, TG_T, TG_L + half, TG_T + TG_H, "NO CONTROL", 12, 1, mp ? off : st == S_NONE ? on : off, 1);
    text(TG_L + half, TG_T, TG_L + TG_W, TG_T + TG_H, st == S_CUSTOM ? "CUSTOM" : "FULL CONTROL", 12, 1,
         mp ? off : st == S_NONE ? off : on, 1);
    const char *status = mp                ? "Only in single player games"
                         : st == S_FULL    ? "The computer plays your nation"
                         : st == S_CUSTOM  ? "The computer plays, only what is ticked"
                                           : "You play your nation";
    text(0, STATUS_T, PW, STATUS_T + 18, status, 11, 0, T_GREY, 1);

    text(20, HEAD_T, PW - 20, HEAD_T + 18, "THE COMPUTER MAY BUY", 11, 1, T_GOLD, 0);
    for (int k = 0; k < assistant_kinds(); k++) {
        int t = row_top(k);
        text(56, t + 6, PW - 20, t + ROW_H, assistant_kind_label(k), 12, 0,
             assistant_allowed(k) ? T_WHITE : T_GREY, 0);
    }

    char s[256];
    snprintf(s, sizeof s, lang_text("Keep at least %d of each resource"), assistant_reserve());
    text(20, RES_T, PW - 20, RES_T + 18, s, 12, 0, T_WHITE, 0);
    (*depth)--;
    return 1;
}

/* ---- clicks -------------------------------------------------------------------------- */

void assist_panel_click(void)
{
    if (!is_open())
        return;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(g_panel, &mx, &my);
    if (my < TITLE_H) {
        if (mx >= PW - BOX - 12)
            ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
        return;
    }
    if (inside(mx, my, TG_L, TG_T, TG_L + TG_W, TG_T + TG_H)) {
        /* a switch: a click on either side flips it; FULL CONTROL ticks every kind */
        int st = state();
        int want_full = mx >= TG_L + TG_W / 2 ? st != S_FULL : st == S_NONE;
        if (want_full) {
            for (int k = 0; k < assistant_kinds(); k++)
                if (!assistant_allowed(k))
                    assistant_set_allowed(k, 1);
            set_computer(1);
        } else {
            set_computer(0);
        }
        return;
    }
    for (int k = 0; k < assistant_kinds(); k++) {
        int t = row_top(k);
        if (inside(mx, my, 14, t, PW - 14, t + ROW_H)) {
            assistant_set_allowed(k, !assistant_allowed(k));
            return;
        }
    }
    if (my >= SL_T - 8 && my <= SL_T + SL_H + 8) {
        int l = SL_L + SL_IN, r = SL_L + SL_W - SL_IN;
        int x = mx < l ? l : mx > r ? r : mx;
        int v = (x - l) * RESERVE_MAX / (r - l);
        v = (v + RESERVE_STEP / 2) / RESERVE_STEP * RESERVE_STEP;
        assistant_set_reserve(v);
    }
}
