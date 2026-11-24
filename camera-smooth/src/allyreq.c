/*
 * Allied requests (issue #54, the user's design 2026-11-24): a computer ally asks you for help
 * through a panel with its nation's medallion, the request, a countdown and Accept / Decline.
 * Unanswered after [gameplay] ally_request_answer seconds of game time it closes by itself
 * (declined). Three kinds of request:
 *
 * Resources:
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
 * Joint attack:
 *  - when a computer ally's army has mustered and is about to march (Army::do_mustering's only
 *    call of Army::release_mustering, retargeted here), it asks a human ally to join and the
 *    army keeps mustering while the panel is open. The minimap flashes where it gathers;
 *  - Accept: the army waits up to [gameplay] ally_attack_wait seconds, or until 8 of your
 *    fighting units are within 18 tiles of it, then marches; when it has picked its target the
 *    ally names it in the chat and the minimap flashes there. Decline or no answer: it marches
 *    at once. An army re-checks only every 256 frames (17 s), so the times are that coarse.
 *
 * Defence:
 *  - a computer ally with 12 or more enemy fighting units within 12 tiles of one of its cities
 *    asks you to help defend it (the minimap flashes there). The answer changes nothing in the
 *    computer's play: it defends its cities by itself.
 *
 * One open request per ally; after one, no request of the same group (resources / attack and
 * defence) from that ally for ally_request_minutes, none in the first ally_request_first minutes.
 *
 * Simulation: the requests are worked out from game state and the game's frame clock only
 * (airdrop.c's Objects::process_all hook, every machine alike, replays too). Resources and
 * defence change nothing by themselves. A held army does, so the answer to an attack or defence
 * request is a command: Accept sends the taunt "78" (On my way.) or "80" (Gotcha covered.) to
 * allies, Decline "2" (No.), with the game's own chat (LeaderOut::issue_chat), and the hook of
 * CommandPackage::process_chat (g_hooks "ally_chat", every machine) reads it. Typing the number
 * in the chat answers too. The panel shows only on the machine of the human asked; nothing shows
 * in a replay. Every player of a game with computer allies needs the same DLL and settings.
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
#define VA_IS_ENEMY         0x006EBAA0u /* LeaderData::is_enemy(int player) const, ret 4 */
#define VA_GET_DIFF         0x006EC000u /* LeaderData::get_diff() const */
#define VA_ISSUE_CHAT       0x006D2C50u /* LeaderOut::issue_chat(const String &, int mode) const, ret 8; 2 = allies */
#define VA_RELEASE_MUSTER   0x006F87C0u /* Army::release_mustering(): 1 = the army may march */
#define CALL_RELEASE_MUSTER 0x006F4263u /* its only call, in Army::do_mustering */
#define VA_ARMY_COG         0x006F8A20u /* Army::center_of_gravity(WCoord *, WCoord *): 4-tile cells, ret 8 */
#define VA_OBJECTS          0x00C0618Cu /* Objects *: + 0x14 + 0x1C * p = object pointers by id */
#define VA_SCENE_PTR        0x00C0620Cu /* Scene * */
#define VA_ADD_PING         0x008C0E50u /* Scene::add_ping(Coord x, Coord y, int player, 1), ret 0x10: this machine */
#define VA_TERRAIN          0x00C06218u /* Terrain * */
#define VA_FIND_Z           0x00866560u /* TerrainOut::find_data_z(Coord *out, x, y, 1), ret 0x10 */
#define VA_DRAW_CIRCLE      0x008C6040u /* the ground circle (rally.c's rally_circle) */

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
#define LD_ARMY_KIND 0xA68  /* word per army kind (army + 8): & 4 defends (below difficulty 3), & 8 by sea */

/* Army (Army::process, do_mustering, release_mustering, find_target) */
#define AR_STATE     0x04   /* 1 mustering, 2 marching, 0x10 forming, 0x20 defending, 0x40 by sea */
#define AR_KIND      0x08
#define AR_SIZE      0x18   /* its units, as release_mustering counts them */
#define AR_CLOSING   0x24
#define AR_TARGET    0x30   /* object id of the target, -1 none (set while marching, by find_target) */
#define AR_TARGET_P  0x34   /* the target's player */
#define AR_MUSTER_X  0x48   /* muster spot, 4-tile cells, -1 none */
#define AR_MUSTER_Y  0x4C
#define AR_PLAYER    0x94   /* short */
#define CELL         768    /* Coord per 4-tile cell */

/* objects (airdrop.c has the same) */
#define OBJ_FLAGS    0x08
#define OBJ_X        0x10
#define OBJ_Y        0x14
#define OBJ_TYPE     0x18
#define COORD_KEY    0x63637
#define OBJS_UNITS_N 0x15C
#define OBJS_BUILD_N 0x184
#define FIRST_BUILD  2000
#define TILE         192
#define CITY_FIRST   0x19E
#define CITY_LAST    0x1A0

/* ChatCommand (CommandPackage::process_chat): +1 recipients (bit per player, -1 all), +5 the
   taunt's request for the computer, +9 the taunt's number; the package's +4 is the sender's slot */
#define CHAT_TO      1
#define CHAT_TAUNT   9
#define PL_INDEX     0x77   /* byte: game + 0x77 + slot * 0x8C = the slot's player */
#define TAUNT_NO     2      /* "No." */
#define TAUNT_COMING 78     /* "On my way." */
#define TAUNT_HOLD   80     /* "Gotcha covered." */

#define ATTACK_MIN_ARMY 6   /* an army this big asks for a joint attack (the easiest computer marches at 7) */
#define NEAR_TILES      18  /* your units this close to the waiting army count as arrived */
#define NEAR_UNITS      8
#define SIEGE_TILES     12  /* enemy units this close to a city count as an attack on it */
#define SIEGE_UNITS     12

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
typedef void(__attribute__((thiscall)) *issue_chat_t)(void *leader, const void *str, int mode);
typedef int(__attribute__((thiscall)) *release_t)(void *army);
typedef void(__attribute__((thiscall)) *cog_t)(void *army, int *x, int *y);
typedef void(__attribute__((thiscall)) *ping_t)(void *scene, int x, int y, int player, int one);
typedef int(__attribute__((thiscall)) *chat_proc_t)(void *pkg, char *cmd);
typedef int *(__attribute__((thiscall)) *find_z_t)(void *terrain, int *out, int x, int y, int flag);
void rally_circle(int x, int y, int z, int radius, const void *color); /* rally.c */
extern UINT_PTR g_draw_circle;

/* ---- settings ---------------------------------------------------------------------------- */

static int g_on = 1;
static int g_answer_s = 60;   /* seconds of game time to answer */
static int g_cooldown_s = 180; /* between two requests of one ally */
static int g_first_s = 300;   /* none before this */
static int g_wish_s = 20;     /* a want must last this long */
static int g_attack_on = 1, g_defend_on = 1;
static int g_wait_s = 90;     /* after an Accept, the army waits at most this long */
static int g_marks = 1, g_mark_s = 60; /* the marks on the map, and how long they stay */
int g_allyreq_chat = 1;       /* the process_chat hook (g_hooks "ally_chat") */
void *g_orig_ally_chat;

void allyreq_load_config(const char *ini_path)
{
    g_on = GetPrivateProfileIntA("gameplay", "ally_requests", 1, ini_path);
    g_attack_on = GetPrivateProfileIntA("gameplay", "ally_attack_requests", 1, ini_path);
    g_defend_on = GetPrivateProfileIntA("gameplay", "ally_defence_requests", 1, ini_path);
    g_wait_s = GetPrivateProfileIntA("gameplay", "ally_attack_wait", 90, ini_path);
    if (g_wait_s < 15 || g_wait_s > 600)
        g_wait_s = 90;
    g_marks = GetPrivateProfileIntA("gameplay", "ally_map_marks", 1, ini_path);
    g_mark_s = GetPrivateProfileIntA("gameplay", "ally_map_mark_seconds", 60, ini_path);
    if (g_mark_s < 10 || g_mark_s > 600)
        g_mark_s = 60;
    g_allyreq_chat = g_on && (g_attack_on || g_defend_on);
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
static int enemies(int a, int b) { return ((fn_i_i)VA(VA_IS_ENEMY))(leader(a), b) != 0; }

static char *object(int p, int id)
{
    if (p < 0 || p > 7 || id < 0)
        return NULL;
    char *objs = *(char **)VA(VA_OBJECTS);
    char **arr = objs ? *(char ***)(objs + 0x14 + 0x1C * p) : NULL;
    char *o = arr ? arr[id] : NULL;
    return o && (o[OBJ_FLAGS] & 1) ? o : NULL;
}
static int units_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_UNITS_N + 4 * p); }
static int builds_n(int p) { return *(int *)(*(char **)VA(VA_OBJECTS) + OBJS_BUILD_N + 4 * p); }
static int type_of(char *o) { return *(int *)(*(char **)(o + OBJ_TYPE) + 4); }
static int ox(char *o) { return *(int *)(o + OBJ_X) ^ COORD_KEY; }
static int oy(char *o) { return *(int *)(o + OBJ_Y) ^ COORD_KEY; }
static int within(char *o, int x, int y, int tiles)
{
    long long dx = ox(o) - x, dy = oy(o) - y, r = (long long)tiles * TILE;
    return dx * dx + dy * dy <= r * r;
}

/* ---- the requests -------------------------------------------------------------------------- */

enum { W_AGE, W_WONDER, W_TECH, W_BUILD, W_ARMY, W_ECON };
enum { K_RES, K_ATTACK, K_DEFEND };

typedef struct {
    int on, kind, to, r, amount, wish, t0, t_end;
    int x, y;    /* attack / defence: where the minimap flashes (Coord) */
    int ui_done; /* this machine only: answered on the panel, the command is on its way */
} req_t;

/* an army kept mustering by a joint attack request (simulation state, every machine alike) */
typedef struct {
    char *army;
    int to, accepted, until, x, y;
} hold_t;

static req_t g_req[8];                        /* by the asking computer player */
static int g_next_ok[8];                      /* frame of its next possible resource request */
static int g_next_help[8];                    /* ... and attack or defence request */
static int g_asked_at[8];                     /* by human: the frame it was last asked for help */
static hold_t g_hold[8];                      /* by computer player */
static char *g_last_army[8];                  /* the last army that asked, not asked again soon */
static int g_last_army_t[8];
static struct {
    char *army;
    int until;
} g_watch[8];                                 /* a released army whose target is to be named (this machine) */
/* this machine only: a mark on the map where an ally wants you, for g_mark_s seconds of game time */
enum { M_NONE, M_GATHER, M_ATTACK, M_DEFEND };
static struct {
    int kind, x, y, t0;
} g_mark[8];
static int g_wish_type[8], g_wish_r[8], g_wish_since[8];
static char g_why[8][120];                    /* the log's status line: why an ally has not asked */
static int g_releases[8], g_asked_attack[8];  /* the log: armies that went, and that asked */
static int g_last_frame = -1;
static int g_shown = -1;                      /* the asking player on the panel, -1 none */
static int g_check_a = -1, g_check_r, g_check_before, g_check_at; /* the log's check after an Accept */

static void ally_says(int a, const char *s);
static void name_of(int p, char *out, int n);
static void fill(char *out, size_t n, const char *tmpl, const char *name, const char *age, int num, const char *res);

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

/* a fighting unit: no Citizen, caravan or fishing boat */
static int fighter(int type) { return wish_kind(type) == W_ARMY; }

static void forget_all(void)
{
    memset(g_req, 0, sizeof g_req);
    memset(g_next_ok, 0, sizeof g_next_ok);
    memset(g_next_help, 0, sizeof g_next_help);
    memset(g_asked_at, 0, sizeof g_asked_at);
    memset(g_hold, 0, sizeof g_hold);
    memset(g_last_army, 0, sizeof g_last_army);
    memset(g_watch, 0, sizeof g_watch);
    memset(g_mark, 0, sizeof g_mark);
    memset(g_why, 0, sizeof g_why);
    memset(g_releases, 0, sizeof g_releases);
    memset(g_asked_attack, 0, sizeof g_asked_attack);
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
        snprintf(g_why[a], sizeof g_why[a], "saves for nothing it could ask for (type 0x%X)", type);
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
        snprintf(g_why[a], sizeof g_why[a], "lacks nothing for type 0x%X", type);
        return;
    }
    /* the same resource missing for a while; what it saves for may change in between (the
       computer re-sorts its list often, and a want that had to stay the same never lasted) */
    if (g_wish_type[a] < 0 || best_r != g_wish_r[a]) {
        g_wish_type[a] = type;
        g_wish_r[a] = best_r;
        g_wish_since[a] = t;
        snprintf(g_why[a], sizeof g_why[a], "began to lack %d of resource %d for type 0x%X", best_miss, best_r, type);
        return;
    }
    g_wish_type[a] = type;
    if (t - g_wish_since[a] < g_wish_s * 15) {
        snprintf(g_why[a], sizeof g_why[a], "lacks %d of resource %d for type 0x%X since %d s", best_miss, best_r, type,
                 (t - g_wish_since[a]) / 15);
        return;
    }
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
    if (to < 0) {
        snprintf(g_why[a], sizeof g_why[a], "lacks %d of resource %d, but no human ally has %d of it", best_miss, best_r,
                 amount + 200);
        return;
    }
    req_t *q = &g_req[a];
    memset(q, 0, sizeof *q);
    q->on = 1;
    q->kind = K_RES;
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

/* The human ally asked for help the longest time ago (the lowest number on a tie), -1 none. */
static int pick_human(int a)
{
    int to = -1;
    for (int p = 0; p < 8; p++)
        if (p != a && alive(p) && human(p) && allied(a, p) && (to < 0 || g_asked_at[p] < g_asked_at[to]))
            to = p;
    return to;
}

static void open_help(int a, int to, int kind, int x, int y, int t)
{
    req_t *q = &g_req[a];
    memset(q, 0, sizeof *q);
    q->on = 1;
    q->kind = kind;
    q->to = to;
    q->x = x;
    q->y = y;
    q->t0 = t;
    q->t_end = t + g_answer_s * 15;
    g_next_help[a] = t + g_cooldown_s * 15;
    if (g_next_ok[a] < q->t_end + 15 * 15)
        g_next_ok[a] = q->t_end + 15 * 15;
    g_asked_at[to] = t;
}

/* this machine only: a flash on the minimap */
static void ping(int x, int y, int player)
{
    char *scene = *(char **)VA(VA_SCENE_PTR);
    if (scene && !replay())
        ((ping_t)VA(VA_ADD_PING))(scene, x, y, player, 1);
}

/* this machine only: the minimap flashes and the spot gets its mark (ring and text) */
static void mark(int a, int kind, int x, int y)
{
    if (replay())
        return;
    ping(x, y, a);
    if (!g_marks)
        return;
    g_mark[a].kind = kind;
    g_mark[a].x = x;
    g_mark[a].y = y;
    g_mark[a].t0 = now();
}

static int fighters_near(int p, int x, int y, int tiles, int enough)
{
    int n = 0;
    for (int id = 0; id < units_n(p) && n < enough; id++) {
        char *u = object(p, id);
        if (u && fighter(type_of(u)) && within(u, x, y, tiles))
            n++;
    }
    return n;
}

/* Army::do_mustering's call of release_mustering (every machine): 0 keeps the army mustering.
   A computer army about to march asks its human ally to join, and waits while it is held. */
__attribute__((thiscall)) static int hk_release(char *army)
{
    int r = ((release_t)VA(VA_RELEASE_MUSTER))(army);
    if (!r || !g_on || !g_attack_on || !army)
        return r;
    int a = *(short *)(army + AR_PLAYER), t = now();
    if (a < 0 || a > 7 || !computer(a))
        return r;
    hold_t *h = &g_hold[a];
    if (h->army)
        return h->army == army ? 0 : r;
    g_releases[a]++;
    /* will it march? (do_mustering's own choice after a release: defend, by sea or march) */
    unsigned kind = *(unsigned short *)(leader(a) + LD_ARMY_KIND + *(int *)(army + AR_KIND) * 2);
    if (*(int *)(army + AR_CLOSING) || *(int *)(army + AR_SIZE) < ATTACK_MIN_ARMY)
        return r;
    if ((kind & 4) && ((fn_i_v)VA(VA_GET_DIFF))(leader(a)) < 3)
        return r;
    if ((kind & 8) && (*(unsigned *)leader(a) & 0x300))
        return r;
    if (t < g_first_s * 15 || g_req[a].on || t < g_next_help[a])
        return r;
    if (g_last_army[a] == army && t - g_last_army_t[a] < 600 * 15)
        return r;
    int to = pick_human(a);
    if (to < 0)
        return r;
    int cx = *(int *)(army + AR_MUSTER_X), cy = *(int *)(army + AR_MUSTER_Y);
    if (cx < 0 || cy < 0) {
        cx = cy = -1;
        ((cog_t)VA(VA_ARMY_COG))(army, &cx, &cy);
        if (cx < 0 || cy < 0)
            return r;
    }
    int x = cx * CELL + CELL / 2, y = cy * CELL + CELL / 2;
    open_help(a, to, K_ATTACK, x, y, t);
    h->army = army;
    h->to = to;
    h->accepted = 0;
    h->until = g_req[a].t_end;
    h->x = x;
    h->y = y;
    g_last_army[a] = army;
    g_last_army_t[a] = t;
    g_asked_attack[a]++;
    logf_("ally request: player %d asks player %d to join an attack (army of %d at cell %d,%d)", a, to,
          *(int *)(army + AR_SIZE), cx, cy);
    return 0;
}

/* the held army goes: it marches at its next look (do_mustering) */
static void let_go(int a, int t, const char *why, int arrived)
{
    hold_t *h = &g_hold[a];
    logf_("ally request: player %d's army goes (%s)", a, why);
    if (h->accepted && h->to == local_player() && !replay()) {
        ally_says(a, lang_text(arrived ? "Your troops are here. We march together!"
                                       : "I cannot wait any longer. My army marches now."));
        g_watch[a].army = h->army;
        g_watch[a].until = t + 120 * 15;
    }
    h->army = NULL;
}

static void check_holds(int t)
{
    for (int a = 0; a < 8; a++) {
        hold_t *h = &g_hold[a];
        if (!h->army)
            continue;
        if (!computer(a) || !alive(h->to) || !(*(int *)(h->army + AR_STATE) & 1))
            let_go(a, t, "no longer mustering", 0);
        else if (!h->accepted) {
            if (!g_req[a].on || g_req[a].kind != K_ATTACK)
                let_go(a, t, "not accepted", 0);
        } else if (t >= h->until)
            let_go(a, t, "waited long enough", 0);
        else if (t % 15 == 0 && fighters_near(h->to, h->x, h->y, NEAR_TILES, NEAR_UNITS) >= NEAR_UNITS)
            let_go(a, t, "your troops arrived", 1);
    }
}

/* this machine only: name the target of an army that went with you, once it has one */
static void check_watch(int t)
{
    int me = local_player();
    for (int a = 0; a < 8; a++) {
        char *army = g_watch[a].army;
        if (!army)
            continue;
        int tp = *(int *)(army + AR_TARGET_P), id = *(int *)(army + AR_TARGET);
        char *o = tp >= 0 && tp < 8 && enemies(a, tp) ? object(tp, id) : NULL;
        if (o && me >= 0) {
            char name[96], line[192];
            name_of(tp, name, sizeof name);
            fill(line, sizeof line, lang_text("We attack $NAME!"), name[0] ? name : lang_text("the enemy"), "", 0, "");
            ally_says(a, line);
            mark(a, M_ATTACK, ox(o), oy(o));
            logf_("ally request: player %d's army attacks player %d", a, tp);
        }
        if (o || t >= g_watch[a].until || !computer(a))
            g_watch[a].army = NULL;
    }
}

/* a computer ally's city with a large enemy army close: ask for help */
static void consider_defence(int a, int t)
{
    int best = 0, bx = 0, by = 0;
    for (int id = FIRST_BUILD; id < builds_n(a); id++) {
        char *c = object(a, id);
        if (!c || type_of(c) < CITY_FIRST || type_of(c) > CITY_LAST)
            continue;
        int n = 0;
        for (int e = 0; e < 8 && n < 99; e++)
            if (e != a && alive(e) && enemies(a, e))
                n += fighters_near(e, ox(c), oy(c), SIEGE_TILES, 99 - n);
        if (n > best) {
            best = n;
            bx = ox(c);
            by = oy(c);
        }
    }
    if (best < SIEGE_UNITS)
        return;
    int to = pick_human(a);
    if (to < 0)
        return;
    open_help(a, to, K_DEFEND, bx, by, t);
    logf_("ally request: player %d asks player %d to defend a city (%d enemy units near)", a, to, best);
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
    if (!g_on)
        return;
    if (g_check_a >= 0 && t >= g_check_at) { /* did the tribute arrive? (this machine's log only) */
        logf_("ally request: 5 s after the Accept player %d has %d of resource %d (had %d)", g_check_a,
              stock(g_check_a, g_check_r), g_check_r, g_check_before);
        g_check_a = -1;
    }
    for (int a = 0; a < 8; a++) {
        req_t *q = &g_req[a];
        if (q->on && (t >= q->t_end || !computer(a) || !alive(q->to) || !allied(a, q->to))) {
            if (t >= q->t_end && q->to == local_player() && !q->ui_done && !replay())
                logf_("ally request: player %d's request ran out unanswered", a);
            q->on = 0;
        }
    }
    check_holds(t);
    check_watch(t);
    if (t % 900 == 450) /* the log, once a minute: each computer ally of a human and why it has not asked */
        for (int a = 0; a < 8; a++) {
            if (!computer(a) || pick_human(a) < 0)
                continue;
            const char *state = g_req[a].on            ? "a request is open"
                                : t < g_first_s * 15  ? "too early in the game"
                                : t < g_next_ok[a]    ? "resources: waiting since its last request"
                                                      : g_why[a];
            logf_("ally status: player %d (minute %d): %s; armies that marched %d, asked to join %d%s", a, t / 900,
                  state[0] ? state : "-", g_releases[a], g_asked_attack[a], g_hold[a].army ? ", one waits" : "");
        }
    if (t == 30) /* who can ask whom */
        for (int a = 0; a < 8; a++)
            if (computer(a))
                for (int p = 0; p < 8; p++)
                    if (p != a && alive(p) && human(p))
                        logf_("ally status: computer player %d and human player %d are %s", a, p,
                              allied(a, p) ? "allies" : "not allies");
    if (t % 30 || t < g_first_s * 15)
        return;
    for (int a = 0; a < 8; a++) {
        if (!computer(a) || g_req[a].on)
            continue;
        if (g_defend_on && t >= g_next_help[a])
            consider_defence(a, t);
        if (!g_req[a].on && t >= g_next_ok[a])
            consider(a, t);
    }
}

/* CommandPackage::process_chat (g_hooks "ally_chat", every machine): the answer to an attack or
   defence request is a taunt from the human asked to its allies */
__attribute__((thiscall)) int hk_ally_chat(void *pkg, char *cmd)
{
    chat_proc_t orig = (chat_proc_t)g_orig_ally_chat;
    char *g = game();
    if (!g_on || !pkg || !cmd || !g)
        return orig(pkg, cmd);
    int taunt = *(int *)(cmd + CHAT_TAUNT);
    if (taunt != TAUNT_NO && taunt != TAUNT_COMING && taunt != TAUNT_HOLD)
        return orig(pkg, cmd);
    int slot = *(int *)((char *)pkg + 4);
    int from = slot >= 0 && slot < 8 ? (unsigned char)g[PL_INDEX + slot * PL_SIZE] : -1;
    unsigned mask = *(unsigned *)(cmd + CHAT_TO);
    int pick = -1;
    for (int a = 0; a < 8; a++) {
        req_t *q = &g_req[a];
        if (!q->on || q->kind == K_RES || q->to != from || (mask != 0xFFFFFFFFu && !(mask & (1u << a))))
            continue;
        if (taunt != TAUNT_NO && taunt != (q->kind == K_ATTACK ? TAUNT_COMING : TAUNT_HOLD))
            continue;
        if (pick < 0 || q->t0 < g_req[pick].t0)
            pick = a;
    }
    if (pick >= 0) {
        req_t *q = &g_req[pick];
        int yes = taunt != TAUNT_NO, t = now();
        q->on = 0;
        hold_t *h = &g_hold[pick];
        if (q->kind == K_ATTACK && h->army && yes) {
            h->accepted = 1;
            h->until = t + g_wait_s * 15;
        }
        logf_("ally request: player %d %s player %d's %s request (taunt %d, computer request %d)", from,
              yes ? "accepts" : "declines", pick, q->kind == K_ATTACK ? "attack" : "defence", taunt,
              *(int *)(cmd + 5));
        if (from == local_player() && !replay()) {
            static const char *k_say[2][2] = {
                { "Then I attack alone.", "Good. My army waits for you at the signal." },
                { "Understood. I will hold as long as I can.", "Thank you! Hurry, I am holding the city." },
            };
            ally_says(pick, lang_text(k_say[q->kind == K_DEFEND][yes]));
            if (yes) /* the mark stays its full time from now */
                mark(pick, q->kind == K_ATTACK ? M_GATHER : M_DEFEND, q->x, q->y);
            else
                g_mark[pick].kind = M_NONE;
        }
    }
    return orig(pkg, cmd);
}

void allyreq_install(void)
{
    if (!g_on || (!g_attack_on && !g_defend_on))
        return;
    if (!g_allyreq_chat || !g_orig_ally_chat) { /* the answers could not reach every machine */
        logf_("ally request: no chat hook, no joint attack or defence requests");
        g_attack_on = g_defend_on = 0;
        return;
    }
    if (!g_attack_on)
        return;
    unsigned char *at = (unsigned char *)VA(CALL_RELEASE_MUSTER);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5) + *(int *)(at + 1) != VA(VA_RELEASE_MUSTER)) {
        logf_("ally request: signature mismatch at Army::do_mustering, no joint attacks");
        g_attack_on = 0;
        return;
    }
    unsigned char code[5] = { 0xE8 };
    *(int *)(code + 1) = (int)((UINT_PTR)hk_release - (UINT_PTR)(at + 5));
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    memcpy(at, code, 5);
    VirtualProtect(at, 5, old, &old);
    logf_("ally request: joint attacks on (an army waits up to %d s)", g_wait_s);
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
    if (g_shown >= 0 && (!g_req[g_shown].on || g_req[g_shown].ui_done || g_req[g_shown].to != me))
        hide_panel();
    if (g_shown >= 0 || me < 0 || !g_on || replay())
        return;
    int pick = -1;
    for (int a = 0; a < 8; a++)
        if (g_req[a].on && !g_req[a].ui_done && g_req[a].to == me && (pick < 0 || g_req[a].t0 < g_req[pick].t0))
            pick = a;
    if (pick < 0)
        return;
    g_shown = pick;
    if (g_req[pick].kind != K_RES)
        mark(pick, g_req[pick].kind == K_ATTACK ? M_GATHER : M_DEFEND, g_req[pick].x, g_req[pick].y);
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

/* with the Buffer of a window (ours, or the map's for the marks); alpha 0..255 */
static void text_in(char *buf, int l, int t, int r, int b, const char *s, int size, int bold, unsigned rgb,
                    int centred, int alpha)
{
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
    color[3] = (unsigned char)alpha;
    _Alignas(4) unsigned char shade[12];
    memcpy(shade, (void *)VA(VA_BLACK), 12);
    shade[3] = (unsigned char)alpha;
    ((text_color_t)VA(VA_SET_TEXT_COLOR))(buf, color, shade, -1, 0);
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

static void text(int l, int t, int r, int b, const char *s, int size, int bold, unsigned rgb, int centred)
{
    text_in((char *)g_panel + WIN_BUFFER, l, t, r, b, s, size, bold, rgb, centred, 255);
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
    if (q->kind == K_ATTACK) {
        snprintf(out, n, "%s", lang_text("I am gathering my army for an attack. Will you join me? I will wait for your troops."));
        return;
    }
    if (q->kind == K_DEFEND) {
        snprintf(out, n, "%s", lang_text("My city is under attack by a large army! Will you help me defend it?"));
        return;
    }
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
    if (g_shown < 0 || !g_req[g_shown].on || g_req[g_shown].ui_done)
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
    int accept = inside(mx, my, ACCEPT_L, BTN_T, ACCEPT_L + BTN_W, BTN_T + BTN_H);
    if (q->kind != K_RES) { /* the answer is a taunt to the allies: every machine reads it (hk_ally_chat) */
        if (me < 0 || (!accept && !inside(mx, my, DECLINE_L, BTN_T, DECLINE_L + BTN_W, BTN_T + BTN_H)))
            return;
        char num[8];
        snprintf(num, sizeof num, "%d", !accept ? TAUNT_NO : q->kind == K_ATTACK ? TAUNT_COMING : TAUNT_HOLD);
        unsigned char str[64] __attribute__((aligned(4))) = { 0 };
        lang_string(str, num);
        ((issue_chat_t)VA(VA_ISSUE_CHAT))(leader(me), str, 2);
        ((fn_v)VA(VA_STRING_CLOSE))(str);
        logf_("ally request: player %d answers player %d's %s request with taunt %s", me, a,
              q->kind == K_ATTACK ? "attack" : "defence", num);
        q->ui_done = 1;
        hide_panel();
        return;
    }
    if (accept && me >= 0) {
        logf_("ally request: accepted, player %d sends %d of resource %d to player %d (has %d; ally has %d)", me,
              q->amount, q->r, a, stock(me, q->r), stock(a, q->r));
        ((tribute_t)VA(VA_ISSUE_TRIBUTE))(leader(me), a, q->r, q->amount);
        g_check_a = a;
        g_check_r = q->r;
        g_check_before = stock(a, q->r);
        g_check_at = now() + 75;
        ally_says(a, lang_text("Thank you, ally!"));
        q->ui_done = 1; /* still open for the others until it ends: the next request's timing stays the same everywhere */
        hide_panel();
    } else if (inside(mx, my, DECLINE_L, BTN_T, DECLINE_L + BTN_W, BTN_T + BTN_H)) {
        logf_("ally request: declined, player %d", a);
        ally_says(a, lang_text("Understood. I will manage."));
        q->ui_done = 1;
        hide_panel();
    }
}

/* ---- the marks on the map (this machine only: drawing, nothing of the game changes) ---------- */

int allyreq_marks_wanted(void) { return g_on && g_marks && (g_attack_on || g_defend_on); }

static int ground_z(int x, int y)
{
    void *terrain = *(void **)VA(VA_TERRAIN);
    int z = 0;
    if (terrain)
        ((find_z_t)VA(VA_FIND_Z))(terrain, &z, x, y, 1);
    return z;
}

/* 0..1: full for most of its time, fading out over the last 5 s; 0 = gone */
static float mark_alpha(int a, int t)
{
    if (!g_mark[a].kind)
        return 0.0f;
    int age = t - g_mark[a].t0, life = g_mark_s * 15;
    if (age < 0 || age >= life || !computer(a)) {
        g_mark[a].kind = M_NONE;
        return 0.0f;
    }
    int left = life - age;
    return left < 75 ? (float)left / 75.0f : 1.0f;
}

static unsigned mark_rgb(int kind) /* 0xRRGGBB */
{
    return kind == M_ATTACK ? 0xE83C32u : kind == M_DEFEND ? 0x46A0F0u : T_GOLD;
}

/* nukefx.c's Nuke::draw_underlays hook, in the ground pass: two rings, the outer one sweeping in */
void allyreq_draw_ground(void)
{
    if (!allyreq_marks_wanted() || !game() || replay())
        return;
    if (!g_draw_circle)
        g_draw_circle = VA(VA_DRAW_CIRCLE);
    int t = now();
    float secs = (float)GetTickCount() / 1000.0f;
    for (int a = 0; a < 8; a++) {
        float al = mark_alpha(a, t);
        if (al <= 0.0f)
            continue;
        unsigned rgb = mark_rgb(g_mark[a].kind);
        int x = g_mark[a].x, y = g_mark[a].y, z = ground_z(x, y);
        _Alignas(4) unsigned char c[12];
        memcpy(c, (void *)VA(VA_WHITE), 12);
        float wave = secs * 0.8f - (float)(int)(secs * 0.8f); /* 0..1, once every 1.25 s */
        int r0 = 2 * TILE + TILE / 2;
        *(unsigned *)c = (unsigned)(al * 220.0f) << 24 | (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb >> 16 & 0xFF);
        rally_circle(x, y, z, r0, c);
        rally_circle(x, y, z, r0 - 10, c);
        *(unsigned *)c = (unsigned)(al * 200.0f * (1.0f - wave)) << 24 | (*(unsigned *)c & 0xFFFFFF);
        rally_circle(x, y, z, (int)((float)r0 * (1.6f - 0.6f * wave)), c);
        *(unsigned *)c = (unsigned)(al * 160.0f) << 24 | (*(unsigned *)c & 0xFFFFFF);
        rally_circle(x, y, z, TILE / 3, c);
    }
}

/* rankfx.c's Console::on_redraw: the text over each mark, with the map window's own Buffer */
void allyreq_draw_labels(void)
{
    if (!allyreq_marks_wanted() || !game() || replay())
        return;
    char *con = *(char **)VA(VA_CONSOLE_PTR);
    if (!con)
        return;
    int t = now();
    for (int a = 0; a < 8; a++) {
        float al = mark_alpha(a, t);
        if (al <= 0.0f)
            continue;
        int x = g_mark[a].x, y = g_mark[a].y;
        float sx, sy;
        if (!rankfx_project((float)x, (float)y, (float)(ground_z(x, y) + 0x240), &sx, &sy))
            continue;
        short *cr = (short *)(con + WIN_RECT);
        if (sx < cr[0] - 200 || sx > cr[1] + 200 || sy < cr[2] - 60 || sy > cr[3] + 60)
            continue;
        static const char *k_title[4] = { "", "GATHER HERE", "ATTACK HERE", "DEFEND HERE" };
        int ix = (int)sx, iy = (int)sy, alpha = (int)(al * 255.0f);
        char name[96];
        name_of(a, name, sizeof name);
        text_in(con + WIN_BUFFER, ix - 220, iy - 30, ix + 220, iy - 4, lang_text(k_title[g_mark[a].kind]), 18, 1,
                mark_rgb(g_mark[a].kind), 1, alpha);
        if (name[0])
            text_in(con + WIN_BUFFER, ix - 220, iy - 4, ix + 220, iy + 14, name, 12, 0, T_WHITE, 1, alpha);
    }
}
