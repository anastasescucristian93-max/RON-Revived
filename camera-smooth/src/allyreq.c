/*
 * Allied requests (issue #54, the user's design 2026-11-24): a computer ally asks you for help
 * through a panel with its nation's medallion, the request, a countdown and Accept / Decline.
 * Unanswered after [gameplay] ally_request_answer seconds of game time it closes by itself
 * (declined). This first stage asks for resources:
 *
 *  - every 2 s of game time each computer leader allied with a human looks at the top of its own
 *    shopping list (the MakeList at leader + 0x6EC8, its entries at [leader + 0x6ED8], highest
 *    priority first: what it saves for). When it has lacked one resource for that same wish for
 *    ally_request_wish seconds, it asks for the missing amount (rounded up to 50, at most
 *    100 + 150 x its age) from the human ally with the most of it, if that human would still
 *    keep 200 after giving;
 *  - one request per ally at a time, then none from it for ally_request_minutes, none in the
 *    first ally_request_first minutes of a game;
 *  - Accept sends the game's own tribute command (LeaderOut::issue_tribute, command 0x2B, the
 *    same as the in-game tribute buttons), so every machine sees the same transfer.
 *
 * Simulation: the requests are worked out from game state and the game's frame clock only
 * (airdrop.c's Objects::process_all hook, every machine alike), but they change nothing by
 * themselves: only the Accept's tribute does, and that is a command. The panel shows only on
 * the machine of the human asked. Nothing in a replay.
 *
 * The panel is a native IFaceButton like the assistant panel (assist_panel.c): created with the
 * qol row (qol.c), drawn by panel.c's IFaceButton::on_redraw hook, clicked through qol.c's
 * on_button_clicked hook. Art: ui-theme\tools\build_ally_panel.py (IFACETEX "ally_*", the
 * medallions "ally_flag_<tribe>").
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
#define VA_BRING_TO_TOP     0x00A53F90u
#define VA_GET_MOUSE_POS    0x00A54170u /* Window::get_mouse_pos(int *x, int *y): client coordinates */
#define VA_IFACE_RM         0x00EBD1C0u
#define VA_RM_ADD_RECT      0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], color, type) */
#define VA_STRING_CLOSE     0x00A1CF40u
#define VA_WRITE_L          0x0050F6E0u /* Buffer::write_l(const String &, const Rect *, len) */
#define VA_WRITE_CENT       0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_SET_TEXT_COLOR   0x0050DAF0u
#define VA_FONTS            0x00ED8160u
#define VA_FONT_GET         0x00522810u /* FontFamily::get(size, style) const */
#define VA_WHITE            0x00C8D26Cu
#define VA_BLACK            0x00C8D260u
#define VA_GRAPHSYS         0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define VA_GAME_PTR         0x00C061ECu /* Game *: +0x550 the frame (15 a second), +0x820 flags */
#define VA_CONSOLE_PTR      0x00C06210u /* Console *: +0x298 local player */
#define VA_LEADERS          0x00E3A390u /* Leader[8], 0x6EEC each */
#define VA_TYPE_TABLE       0x00E85DDCu /* TypeData ** by TypeIndex */
#define VA_IS_ALLY          0x006EDB50u /* LeaderData::is_ally(int player) const, ret 4 */
#define VA_GET_PLAYER       0x006EC0F0u /* LeaderData::get_player() const: its Player slot */
#define VA_ISSUE_TRIBUTE    0x006D3F90u /* LeaderOut::issue_tribute(to, r, amount) const, ret 0xC */
#define VA_CHAT_TO_LOCAL    0x006EC520u /* Leader::chat_to_local(const String &, to, 2, 1), ret 0x10 */
#define VA_CURRENT_AGE      0x009EE920u /* ScenarioFuncSet::get_current_age(const int &p) (this unused): 1..8 */
#define VA_STR_AS_CHAR      0x0042D710u /* String::str_as_char() const */

#define GAME_FRAME   0x550
#define GAME_FLAGS   0x820 /* & 0x10: a replay */
#define GAME_PLAYERS 0x44  /* Player[8], 0x8C each, the name String at +0x40 */
#define PL_SIZE      0x8C
#define PL_NAME      0x40
#define LEADER_SIZE  0x6EEC
#define LD_TRIBE     0x0C
#define LD_STOCK     0x6EB8 /* int * -> treasury, each ^ STOCK_KEY */
#define LD_WISHES    0x6ED8 /* the MakeList's entries: 4 x 0x28, +0 TypeIndex, +4 priority */
#define STOCK_KEY    0x8221
#define VT_GET_COST  0x78   /* TypeData::get_cost(r, player, -1, -1, 0, 1, -1) */

#define IFB_KIND     0x32C
#define IFB_FLAGS    0x3BC
#define WIN_RECT     0x126
#define WIN_BUFFER   0x154
#define BUF_FONT     0x7C
#define BUF_COLORS   0x90
#define BUF_COLORS_N 0x80
#define GS_DEPTH     0x164
#define RENDER_SOLID 5

/* layout, in window pixels; ui-theme\tools\build_ally_panel.py has the same numbers */
#define PW      460
#define PH      178
#define TITLE_H 34
#define SEAT_X  18
#define SEAT_Y  48
#define SEAT    72
#define BTN_W   132
#define BTN_H   30
#define BAR_W   346
#define BAR_H   10
#define TEXT_L  102
#define BAR_T   118
#define BTN_T   (PH - 18 - BTN_H)
#define DECLINE_L (PW - 18 - BTN_W)
#define ACCEPT_L  (DECLINE_L - 12 - BTN_W)

#define ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define T_GOLD  0xE6BE3Cu
#define T_WHITE 0xF0F0F0u
#define T_GREY  0x8A9098u

typedef int(__attribute__((thiscall)) *fn_i_i)(void *self, int a);
typedef int(__attribute__((thiscall)) *fn_i_v)(void *self);
typedef int(__attribute__((thiscall)) *button_init_t)(void *self, const void *text, int id, int x, int y, int w, int h,
                                                      void *parent, int flag);
typedef int(__attribute__((thiscall)) *fn_resize)(void *self, int w, int h, int a, int b);
typedef int(__attribute__((thiscall)) *move_t)(void *self, int x, int y);
typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int a);
typedef void(__attribute__((thiscall)) *mouse_t)(void *self, int *x, int *y);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *self, const void *tex, const short *rect, unsigned color,
                                                     int type);
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);
typedef int(__attribute__((thiscall)) *get_cost_t)(void *type, int r, int player, int a, int b, int c, int d, int e);
typedef void(__attribute__((thiscall)) *tribute_t)(void *leader, int to, int r, int amount);
typedef void(__attribute__((thiscall)) *chat_t)(void *leader, const void *str, int to, int a, int b);
typedef int(__attribute__((thiscall)) *age_t)(void *self, const int *p);
typedef const char *(__attribute__((thiscall)) *as_char_t)(const void *str);

/* ---- settings ---------------------------------------------------------------------------- */

static int g_on = 1;
static int g_answer_s = 60;   /* seconds of game time to answer */
static int g_cooldown_s = 180; /* between two requests of one ally */
static int g_first_s = 300;   /* none before this */
static int g_wish_s = 20;     /* a want must last this long */

void allyreq_load_config(const char *ini_path)
{
    g_on = GetPrivateProfileIntA("gameplay", "ally_requests", 1, ini_path);
    g_answer_s = GetPrivateProfileIntA("gameplay", "ally_request_answer", 60, ini_path);
    g_cooldown_s = GetPrivateProfileIntA("gameplay", "ally_request_minutes", 3, ini_path) * 60;
    g_first_s = GetPrivateProfileIntA("gameplay", "ally_request_first", 5, ini_path) * 60;
    g_wish_s = GetPrivateProfileIntA("gameplay", "ally_request_wish", 20, ini_path);
    if (g_answer_s < 15 || g_answer_s > 300)
        g_answer_s = 60;
    /* a new request of an ally only after its last one has closed on every machine */
    if (g_cooldown_s < g_answer_s + 15)
        g_cooldown_s = g_answer_s + 15;
    if (g_first_s < 0)
        g_first_s = 0;
    if (g_wish_s < 0 || g_wish_s > 600)
        g_wish_s = 20;
}

/* ---- game state ---------------------------------------------------------------------------- */

static char *game(void) { return *(char **)VA(VA_GAME_PTR); }
static int now(void)
{
    char *g = game();
    return g ? *(int *)(g + GAME_FRAME) : 0;
}
static int replay(void)
{
    char *g = game();
    return !g || (g[GAME_FLAGS] & 0x10);
}
static int local_player(void)
{
    char *c = *(char **)VA(VA_CONSOLE_PTR);
    int p = c ? *(int *)(c + 0x298) : -1;
    return p >= 0 && p < 8 ? p : -1;
}
static char *leader(int p) { return (char *)VA(VA_LEADERS) + (size_t)p * LEADER_SIZE; }
static int alive(int p)
{
    unsigned f = *(unsigned *)leader(p);
    return (f & 1) && !(f & 0x40);
}
static int human(int p) { return (*(unsigned *)leader(p) & 4) != 0; }
static int computer(int p) { return alive(p) && !(*(unsigned *)leader(p) & 0xC); }
static int allied(int a, int b) { return ((fn_i_i)VA(VA_IS_ALLY))(leader(a), b) != 0; }
static int stock(int p, int r)
{
    int *s = *(int **)(leader(p) + LD_STOCK);
    return s ? s[r] ^ STOCK_KEY : 0;
}
static int age_of(int p) /* 1 Ancient .. 8 Information */
{
    int n = p + 1;
    int a = ((age_t)VA(VA_CURRENT_AGE))(NULL, &n);
    return a < 1 ? 1 : a > 8 ? 8 : a;
}
static int cost_of(int type, int r, int p)
{
    char *td = (*(char ***)VA(VA_TYPE_TABLE))[type];
    if (!td)
        return 0;
    return ((get_cost_t)(*(void ***)td)[VT_GET_COST / 4])(td, r, p, -1, -1, 0, 1, -1);
}

/* ---- the requests -------------------------------------------------------------------------- */

enum { W_AGE, W_WONDER, W_TECH, W_BUILD, W_ARMY, W_ECON };

typedef struct {
    int on, to, r, amount, wish, t0, t_end;
} req_t;

static req_t g_req[8];                        /* by the asking computer player */
static int g_next_ok[8];                      /* frame of its next possible request */
static int g_wish_type[8], g_wish_r[8], g_wish_since[8];
static int g_last_frame = -1;
static int g_shown = -1;                      /* the asking player on the panel, -1 none */
static int g_check_a = -1, g_check_r, g_check_before, g_check_at; /* the log's check after an Accept */

static const int k_res[5] = { 0, 1, 2, 4, 5 }; /* no knowledge */

static int wish_kind(int type)
{
    if (type >= 0x220 && type <= 0x226)
        return W_AGE;
    if (type >= 0x20E && type <= 0x21E)
        return W_WONDER;
    if (type >= 0x227 && type <= 0x274)
        return W_TECH;
    if (type >= 0x19E && type <= 0x21E)
        return W_BUILD;
    if ((type >= 0x32 && type <= 0x35) || (type >= 0x3B && type <= 0x3E) || (type >= 0x13D && type <= 0x13F) ||
        type == 0x190)
        return W_ECON;
    if (type >= 0x32 && type <= 0x19D)
        return W_ARMY;
    return -1;
}

static void forget_all(void)
{
    memset(g_req, 0, sizeof g_req);
    memset(g_next_ok, 0, sizeof g_next_ok);
    g_check_a = -1;
    for (int p = 0; p < 8; p++) {
        g_wish_type[p] = -1;
        g_wish_since[p] = -1;
    }
}

/* Does computer player a want something it lacks a resource for, long enough to ask? */
static void consider(int a, int t)
{
    char *L = leader(a);
    char *wishes = *(char **)(L + LD_WISHES);
    int type = wishes ? *(int *)wishes : -1;
    if (type < 0x32 || type > 0x274 || wish_kind(type) < 0) {
        g_wish_type[a] = -1;
        return;
    }
    int best_r = -1, best_miss = 0;
    for (int i = 0; i < 5; i++) {
        int r = k_res[i];
        int miss = cost_of(type, r, a) - stock(a, r);
        if (miss > best_miss) {
            best_miss = miss;
            best_r = r;
        }
    }
    if (best_r < 0) {
        g_wish_type[a] = -1;
        return;
    }
    if (type != g_wish_type[a] || best_r != g_wish_r[a]) {
        g_wish_type[a] = type;
        g_wish_r[a] = best_r;
        g_wish_since[a] = t;
        return;
    }
    if (t - g_wish_since[a] < g_wish_s * 15)
        return;
    int amount = (best_miss + 49) / 50 * 50;
    int most = 100 + 150 * age_of(a);
    if (amount > most)
        amount = most;
    /* the human ally with the most of it, who keeps 200 after giving */
    int to = -1, to_has = 0;
    for (int p = 0; p < 8; p++) {
        if (p == a || !alive(p) || !human(p) || !allied(a, p))
            continue;
        int has = stock(p, best_r);
        if (has >= amount + 200 && has > to_has) {
            to = p;
            to_has = has;
        }
    }
    if (to < 0)
        return;
    req_t *q = &g_req[a];
    q->on = 1;
    q->to = to;
    q->r = best_r;
    q->amount = amount;
    q->wish = type;
    q->t0 = t;
    q->t_end = t + g_answer_s * 15;
    g_next_ok[a] = t + g_cooldown_s * 15;
    g_wish_type[a] = -1;
    logf_("ally request: player %d asks player %d for %d of resource %d (wants type 0x%X, has %d of %d)", a, to,
          amount, best_r, type, stock(a, best_r), cost_of(type, best_r, a));
}

/* From airdrop.c's Objects::process_all hook: every simulation frame, on every machine. */
void allyreq_frame(void)
{
    int t = now();
    if (t == g_last_frame)
        return;
    if (t < g_last_frame || t - g_last_frame > 15) /* a new game or a loaded one */
        forget_all();
    g_last_frame = t;
    if (!g_on || replay())
        return;
    if (g_check_a >= 0 && t >= g_check_at) { /* did the tribute arrive? (this machine's log only) */
        logf_("ally request: 5 s after the Accept player %d has %d of resource %d (had %d)", g_check_a,
              stock(g_check_a, g_check_r), g_check_r, g_check_before);
        g_check_a = -1;
    }
    for (int a = 0; a < 8; a++) {
        req_t *q = &g_req[a];
        if (q->on && (t >= q->t_end || !computer(a) || !alive(q->to) || !allied(a, q->to))) {
            if (t >= q->t_end && q->to == local_player())
                logf_("ally request: player %d's request ran out unanswered", a);
            q->on = 0;
        }
    }
    if (t % 30 || t < g_first_s * 15)
        return;
    for (int a = 0; a < 8; a++)
        if (computer(a) && !g_req[a].on && t >= g_next_ok[a])
            consider(a, t);
}

/* ---- the panel ----------------------------------------------------------------------------- */

static _Alignas(16) unsigned char g_panel[0x400];
static int g_made, g_ready;
static char *g_parent;
static DWORD g_shown_at;

int allyreq_panel_is(void *btn) { return g_ready && btn == g_panel; }

void allyreq_panel_create(char *iface, char *parent)
{
    if (!parent)
        return;
    if (!g_made) {
        ((fn_i_i)VA(VA_IFACEBUTTON_CTOR))(g_panel, 1);
        g_made = 1;
    }
    int err = ((button_init_t)VA(VA_BUTTON_INIT))(g_panel, (void *)VA(VA_EMPTY_STRING), 0, 0, 0, 1, 1, parent, 0);
    if (err) {
        logf_("ally request: panel init error %d", err);
        g_ready = 0;
        return;
    }
    *(int *)(g_panel + IFB_KIND) = 0;
    *(unsigned short *)(g_panel + IFB_FLAGS) |= 1;
    ((fn_resize)(*(void ***)g_panel)[8])(g_panel, PW, PH, 0, 0);
    void **font = (void **)(g_panel + WIN_BUFFER + BUF_FONT);
    if (!*font && iface)
        *font = *(void **)(iface + WIN_BUFFER + BUF_FONT);
    ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
    g_parent = parent;
    g_ready = 1;
    g_shown = -1;
}

static void hide_panel(void)
{
    g_shown = -1;
    if (g_ready)
        ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
}

/* Once per rendered frame: shows the oldest open request to the local player, hides a closed one. */
void allyreq_tick(void)
{
    if (!g_ready)
        return;
    int me = local_player();
    if (g_shown >= 0 && (!g_req[g_shown].on || g_req[g_shown].to != me))
        hide_panel();
    if (g_shown >= 0 || me < 0 || !g_on || replay())
        return;
    int pick = -1;
    for (int a = 0; a < 8; a++)
        if (g_req[a].on && g_req[a].to == me && (pick < 0 || g_req[a].t0 < g_req[pick].t0))
            pick = a;
    if (pick < 0)
        return;
    g_shown = pick;
    g_shown_at = GetTickCount();
    short *pr = g_parent ? (short *)(g_parent + WIN_RECT) : NULL;
    int w = pr ? pr[1] - pr[0] : 1024;
    ((move_t)VA(VA_WINDOW_MOVE))(g_panel, (w - PW) / 2, 190);
    ((fn_v_i)VA(VA_WINDOW_SHOW))(g_panel, 1);
    ((fn_v)VA(VA_BRING_TO_TOP))(g_panel);
}

/* ---- drawing ---- */

static void add(const void *tex, int l, int t, int r, int b, unsigned color)
{
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), tex, rect, color, tex ? 1 : RENDER_SOLID);
}

static void frame(int l, int t, int r, int b, int w, unsigned color)
{
    add(NULL, l, t, r, t + w, color);
    add(NULL, l, b - w, r, b, color);
    add(NULL, l, t, l + w, b, color);
    add(NULL, r - w, t, r, b, color);
}

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
    unsigned char str[64] __attribute__((aligned(4))) = { 0 };
    lang_string(str, s);
    short rect[4] = { (short)l, (short)r, (short)t, (short)b };
    int len = *(unsigned short *)(str + 8);
    if (len)
        ((write_t)VA(centred ? VA_WRITE_CENT : VA_WRITE_L))(buf, str, rect, len);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    memcpy(buf + BUF_FONT, saved, 12);
    memcpy(buf + BUF_COLORS, saved_colors, BUF_COLORS_N);
}

/* left-aligned, broken into lines of about `width` characters at whole words (UTF-8 aware) */
static void text_wrapped(int l, int t, int r, const char *s, int size, unsigned rgb, int width, int line_h, int max_lines)
{
    char line[192];
    int y = t;
    while (*s && max_lines-- > 0) {
        int n = 0, chars = 0, cut = -1;
        while (s[n] && chars < width) {
            if (s[n] == ' ')
                cut = n;
            if (((unsigned char)s[n] & 0xC0) != 0x80)
                chars++;
            n++;
        }
        while (((unsigned char)s[n] & 0xC0) == 0x80) /* not inside a character */
            n++;
        if (s[n] && cut > 0)
            n = cut;
        if (n > (int)sizeof line - 1)
            n = (int)sizeof line - 1;
        memcpy(line, s, (size_t)n);
        line[n] = 0;
        text(l, y, r, y + line_h, line, size, 0, rgb, 0);
        y += line_h;
        s += n;
        while (*s == ' ')
            s++;
    }
}

static const char *res_name(int r)
{
    static const char *k[6] = { "Food", "Timber", "Wealth", "Knowledge", "Metal", "Oil" };
    return lang_text(k[r < 0 || r > 5 ? 0 : r]);
}

/* `tmpl` with $NAME, $AGE, $NUM and $RES filled in (a translation may move them in the sentence,
   which printf's %s / %d could not survive) */
static void fill(char *out, size_t n, const char *tmpl, const char *name, const char *age, int num, const char *res)
{
    char numtxt[16];
    snprintf(numtxt, sizeof numtxt, "%d", num);
    size_t o = 0;
    while (*tmpl && o + 1 < n) {
        const char *v = NULL;
        int k = 0;
        if (!strncmp(tmpl, "$NAME", 5))
            v = name, k = 5;
        else if (!strncmp(tmpl, "$AGE", 4))
            v = age, k = 4;
        else if (!strncmp(tmpl, "$NUM", 4))
            v = numtxt, k = 4;
        else if (!strncmp(tmpl, "$RES", 4))
            v = res, k = 4;
        if (k) {
            for (v = v ? v : ""; *v && o + 1 < n; v++)
                out[o++] = *v;
            tmpl += k;
        } else
            out[o++] = *tmpl++;
    }
    out[o] = 0;
}

static void request_text(char *out, size_t n, const req_t *q)
{
    static const char *ages[7] = { "Classical Age", "Medieval Age", "Gunpowder Age", "Enlightenment Age",
                                   "Industrial Age", "Modern Age", "Information Age" };
    static const char *k_text[] = {
        [W_AGE] = "I am saving for the $AGE, but I need $NUM more $RES. Can you send it?",
        [W_WONDER] = "I am building a Wonder, but I need $NUM more $RES. Can you send it?",
        [W_TECH] = "I want to research a new technology, but I need $NUM more $RES. Can you send it?",
        [W_BUILD] = "I want to build, but I need $NUM more $RES. Can you send it?",
        [W_ARMY] = "My army needs $NUM more $RES. Can you send it?",
        [W_ECON] = "My economy needs $NUM more $RES. Can you send it?",
    };
    int k = wish_kind(q->wish);
    if (k < 0)
        k = W_ARMY;
    const char *age = k == W_AGE ? lang_text(ages[q->wish - 0x220]) : "";
    fill(out, n, lang_text(k_text[k]), "", age, q->amount, res_name(q->r));
}

/* the computer player's name, as UTF-8 */
static void name_of(int p, char *out, int n)
{
    out[0] = 0;
    char *g = game();
    if (!g)
        return;
    int slot = ((fn_i_v)VA(VA_GET_PLAYER))(leader(p));
    if (slot < 0 || slot > 7)
        return;
    const char *s = ((as_char_t)VA(VA_STR_AS_CHAR))(g + GAME_PLAYERS + slot * PL_SIZE + PL_NAME);
    if (!s)
        return;
    WCHAR w[64];
    if (!MultiByteToWideChar(CP_ACP, 0, s, -1, w, 64))
        return;
    w[63] = 0;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL);
    out[n - 1] = 0;
}

static int inside(int mx, int my, int l, int t, int r, int b) { return mx >= l && mx < r && my >= t && my < b; }

static void draw_button(int l, const char *label, int mx, int my)
{
    int hover = inside(mx, my, l, BTN_T, l + BTN_W, BTN_T + BTN_H);
    const void *tex = qol_iface_tex(hover ? "ally_btn_hover" : "ally_btn");
    if (tex)
        add(tex, l, BTN_T, l + BTN_W, BTN_T + BTN_H, 0xFFFFFFFFu);
    else {
        add(NULL, l, BTN_T, l + BTN_W, BTN_T + BTN_H, hover ? 0xFF404850u : 0xFF282E34u);
        frame(l, BTN_T, l + BTN_W, BTN_T + BTN_H, 2, 0xFF3CBEE6u);
    }
}

int allyreq_panel_redraw(void *btn)
{
    if (!allyreq_panel_is(btn))
        return 0;
    if (g_shown < 0 || !g_req[g_shown].on)
        return 1;
    const req_t *q = &g_req[g_shown];
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(g_panel, &mx, &my);
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH), base = *depth;
    /* one atlas for the plate, the bar and the buttons: drawn in the order added */
    const void *back = qol_iface_tex("ally_back");
    if (back)
        add(back, 0, 0, PW, PH, 0xFFFFFFFFu);
    else {
        add(NULL, 0, 0, PW, PH, ABGR(18, 20, 24, 236));
        frame(0, 0, PW, PH, 2, ABGR(230, 190, 60, 200));
    }
    const void *bar = qol_iface_tex("ally_bar"), *gold = qol_iface_tex("ally_fill");
    int span = q->t_end - q->t0, left = q->t_end - now();
    float f = span > 0 ? (float)left / (float)span : 0.0f;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    if (bar)
        add(bar, TEXT_L, BAR_T, TEXT_L + BAR_W, BAR_T + BAR_H, 0xFFFFFFFFu);
    int fw = (int)((float)(BAR_W - 4) * f);
    if (fw > 0)
        add(gold, TEXT_L + 2, BAR_T + 2, TEXT_L + 2 + fw, BAR_T + BAR_H - 2, gold ? 0xFFFFFFFFu : 0xFF3CBEE6u);
    draw_button(ACCEPT_L, "Accept", mx, my);
    draw_button(DECLINE_L, "Decline", mx, my);
    /* the medallion, one step in front of the plate */
    char key[24];
    snprintf(key, sizeof key, "ally_flag_%d", *(int *)(leader(g_shown) + LD_TRIBE));
    const void *flag = qol_iface_tex(key);
    *depth = base + 1;
    if (flag)
        add(flag, SEAT_X + 4, SEAT_Y + 4, SEAT_X + SEAT - 4, SEAT_Y + SEAT - 4, 0xFFFFFFFFu);
    /* the text, in front of everything */
    *depth = base + 2;
    char name[96], title[160], body[320];
    name_of(g_shown, name, sizeof name);
    fill(title, sizeof title, lang_text("$NAME asks for your help"), name[0] ? name : lang_text("Your ally"), "", 0, "");
    text(16, 6, PW - 16, TITLE_H - 2, title, 14, 1, T_GOLD, 1);
    request_text(body, sizeof body, q);
    text_wrapped(TEXT_L, 46, PW - 16, body, 12, T_WHITE, 52, 19, 3);
    text(ACCEPT_L, BTN_T, ACCEPT_L + BTN_W, BTN_T + BTN_H, lang_text("Accept"), 14, 1,
         inside(mx, my, ACCEPT_L, BTN_T, ACCEPT_L + BTN_W, BTN_T + BTN_H) ? 0xFFE08Cu : T_WHITE, 1);
    text(DECLINE_L, BTN_T, DECLINE_L + BTN_W, BTN_T + BTN_H, lang_text("Decline"), 14, 1,
         inside(mx, my, DECLINE_L, BTN_T, DECLINE_L + BTN_W, BTN_T + BTN_H) ? 0xFFE08Cu : T_WHITE, 1);
    char secs[48];
    snprintf(secs, sizeof secs, lang_text("%d s to answer"), (left + 14) / 15 > 0 ? (left + 14) / 15 : 0);
    text(18, BTN_T, ACCEPT_L - 8, BTN_T + BTN_H, secs, 11, 0, T_GREY, 0);
    *depth = base;
    return 1;
}

/* the computer ally's answer line in the chat (this machine only) */
static void ally_says(int a, const char *s)
{
    int me = local_player();
    if (me < 0)
        return;
    unsigned char str[64] __attribute__((aligned(4))) = { 0 };
    lang_string(str, s);
    ((chat_t)VA(VA_CHAT_TO_LOCAL))(leader(a), str, me, 2, 1);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
}

void allyreq_panel_click(void)
{
    if (g_shown < 0 || !g_req[g_shown].on || GetTickCount() - g_shown_at < 400) /* no click meant for the map */
        return;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(g_panel, &mx, &my);
    int a = g_shown, me = local_player();
    req_t *q = &g_req[a];
    if (inside(mx, my, ACCEPT_L, BTN_T, ACCEPT_L + BTN_W, BTN_T + BTN_H) && me >= 0) {
        logf_("ally request: accepted, player %d sends %d of resource %d to player %d (has %d; ally has %d)", me,
              q->amount, q->r, a, stock(me, q->r), stock(a, q->r));
        ((tribute_t)VA(VA_ISSUE_TRIBUTE))(leader(me), a, q->r, q->amount);
        g_check_a = a;
        g_check_r = q->r;
        g_check_before = stock(a, q->r);
        g_check_at = now() + 75;
        ally_says(a, lang_text("Thank you, ally!"));
        q->on = 0;
        hide_panel();
    } else if (inside(mx, my, DECLINE_L, BTN_T, DECLINE_L + BTN_W, BTN_T + BTN_H)) {
        logf_("ally request: declined, player %d", a);
        ally_says(a, lang_text("Understood. I will manage."));
        q->on = 0;
        hide_panel();
    }
}
