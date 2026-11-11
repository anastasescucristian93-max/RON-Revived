/*
 * The Survival reward cards (the user, 2026-11-11: "carduri cu rewarduri dupa fiecare boss", the
 * bonuses 40 % below the first proposal, "ca sunt op"; ruins from every enemy building destroyed,
 * a small card from each, "1/4 din bonusul de card de boss"):
 *
 *  - after every boss wave the script (survival.bhs) sends "surv_bosscards P n" for every defender:
 *    the local player gets three cards to choose from in the middle of the screen, the game paused
 *    until one is clicked; a computer ally takes one of its three at random;
 *  - every enemy building destroyed leaves ruins ("surv_ruin x y": a Goody Box, the item the map
 *    maker lays for ruins, Objects::init_item 0x21F); the unit that explores them (Unit::
 *    explore_goody, hooked) brings a small card, a quarter of a boss card, shown the same way;
 *  - the bonuses are counted in quarters (a boss card 4, a small one 1) per player and kind, and
 *    ranks.c adds them where it adds the veteran ranks' (damage, damage taken, attack rate, hit
 *    points, range, sight, the hero's healing and squad, the army's lowest level).
 *
 * The script keeps the counts too, so they survive a saved game: every pick goes to the script as
 * a code (player, kind, size: 9 bits) through timers, as the setup's choices do: the script sets
 * "surv_pick", which expires at once while a pick waits, then "surv_pkb 0".."surv_pkb 8", one per
 * bit, and "surv_pkack" to take it. It applies the instant cards (resources, mercenaries, a smaller
 * next wave) and sends the counts back ("surv_cards P q0 q1 ..."), every minute too; the DLL takes
 * them while none of that player's picks is still waiting.
 *
 * Single player only (the Survival button's games): a pick made on one machine would put a
 * multiplayer game out of sync.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
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
#define VA_WRITE_CENT       0x0050F850u /* Buffer::write_cent_l(const String &, const Rect *, len) */
#define VA_SET_TEXT_COLOR   0x0050DAF0u
#define VA_FONTS            0x00ED8160u
#define VA_FONT_GET         0x00522810u /* FontFamily::get(size, style) const */
#define VA_WHITE            0x00C8D26Cu
#define VA_BLACK            0x00C8D260u
#define VA_GRAPHSYS         0x00E335A0u /* GraphSys *: +0x164 the depth of what is added */
#define VA_GAME_PTR         0x00C061ECu /* Game *: +0x820 bit 2 multiplayer */
#define VA_CONSOLE_PTR      0x00C06210u /* Console *: +0x298 local player */
#define VA_TURN_CONTROL     0x00C06180u /* TurnControl *: +0x10 bit 0 paused (ScenarioFuncSet::is_paused) */
#define VA_PAUSE            0x009E5B70u /* ScenarioFuncSet::pause_game() (this unused; not in multiplayer) */
#define VA_UNPAUSE          0x009E5BA0u /* ScenarioFuncSet::unpause_game() */
#define VA_CURRENT_AGE      0x009EE920u /* ScenarioFuncSet::get_current_age(const int &p) (this unused): 1..8 */
#define VA_EXPLORE_GOODY    0x005F9780u /* Unit::explore_goody() (this = the unit) */
#define VA_INIT_ITEM        0x00653E00u /* Objects::init_item(TypeIndex, Coord x, Coord y), ret 0xC: the index */
#define VA_OBJECTS          0x00C0618Cu
#define VA_WORLD_PTR        0x00C06188u /* World *: +0 / +4 width / height in WCoords (4 tiles), +0x134 cells */
#define VA_OBJ_ARRAYS       0x00C0AB84u /* Object ** per player, 28 bytes apart */
#define VA_ITEMS            0x00C0A110u /* Item ** (PtrArray items: +0x104 count) */
#define VA_ITEMS_N          0x00C0A104u
#define VA_CAMERA           0x00E335A8u /* BaseCamera::active_camera */
#define VA_CAMERA_OUTDATE   0x00844B80u /* Camera::outdate() */

#define ITEM_GOODY  0x21F  /* the Goody Box (itemrules.xml), the map's ruins */
#define WC          768    /* world units per WCoord (4 tiles of 192) */
#define CELL_SIZE   28     /* World cells: +0 flags (bit 15 set: no item there, as the editor checks),
                              +8 / +0xA the first object's id / player of the cell's chain */
#define O_NEXT_ID   0x2C   /* an object's next one in its cell: id, then player; id -3 = the cell's
                              item, its index in the player word */
#define O_NEXT_P    0x2E
#define CHAIN_ITEM  -3
#define KEY         0x63637

#define IFB_KIND     0x32C
#define IFB_FLAGS    0x3BC
#define WIN_FLAGS    0x08
#define WIN_RECT     0x126
#define WIN_BUFFER   0x154
#define BUF_FONT     0x7C
#define BUF_COLORS   0x90
#define BUF_COLORS_N 0x80
#define GS_DEPTH     0x164
#define RENDER_SOLID 5

#define ABGR(r, g, b, a) (((unsigned)(a) << 24) | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))

/* ---- the cards ------------------------------------------------------------------------------ */

enum { G_BUILD, G_HERO, G_ARMY, G_NOW };
enum { U_PCT, U_LESS, U_TILES, U_LEVEL, U_SOLDIER, U_REGEN, U_TRIBUTE, U_MERC };

enum {
    K_B_HITS, K_B_RANGE, K_B_DMG, K_B_RATE, K_B_TAKEN,
    K_H_DMG, K_H_TAKEN, K_H_EYE, K_H_LEVEL, K_H_SQUAD, K_H_REGEN,
    K_U_DMG, K_U_TAKEN, K_U_RATE, K_U_HITS, K_U_RANGE, K_U_SCHOOL,
    K_L_INF, K_L_CAV, K_L_ART,
    K_I_TRIBUTE, K_I_MERC, K_I_RESPITE,
    NK
};

/* a boss card's amount (a small card: a quarter): tenths of a percent (U_PCT, U_LESS), tiles,
   levels, soldiers, per mille of the hit points a second (U_REGEN), resources per age (U_TRIBUTE).
   cap: quarters at most (0 = no limit); rare: from the second boss on; ruin: also a small card.
   The script (survival.bhs, card_cap[]) has the same caps. */
static const struct {
    const char *title, *who, *fmt;
    int group, unit, value, cap, rare, ruin;
} k_cards[NK] = {
    { "Thick Walls", "All your buildings", "+%s hit points", G_BUILD, U_PCT, 150, 12, 0, 1 },
    { "High Ramparts", "Your towers, forts and cities", "+%s range", G_BUILD, U_TILES, 1, 8, 0, 1 },
    { "Heavy Shot", "Your buildings that shoot", "+%s damage", G_BUILD, U_PCT, 120, 12, 0, 1 },
    { "Vigilant Garrison", "Your buildings that shoot", "+%s attack speed", G_BUILD, U_PCT, 120, 12, 0, 1 },
    { "Buttresses", "All your buildings", "%s damage taken", G_BUILD, U_LESS, 90, 12, 0, 1 },
    { "Elite Blade", "Your hero", "+%s damage", G_HERO, U_PCT, 150, 12, 0, 1 },
    { "Champion's Armour", "Your hero", "%s damage taken", G_HERO, U_LESS, 120, 12, 0, 1 },
    { "Eagle Eye", "Your hero", "+%s range and sight", G_HERO, U_TILES, 1, 8, 0, 1 },
    { "War Veteran", "Your hero", "+%s level", G_HERO, U_LEVEL, 1, 0, 0, 1 },
    { "Reinforcements", "Your hero", "+%s soldiers in the squad", G_HERO, U_SOLDIER, 1, 12, 1, 1 },
    { "Iron Blood", "Your hero", "heals %s of its hit points a second, in battle too", G_HERO, U_REGEN, 6, 12, 0, 1 },
    { "Discipline", "Your army", "+%s damage", G_ARMY, U_PCT, 60, 12, 0, 1 },
    { "Shields", "Your army", "%s damage taken", G_ARMY, U_LESS, 60, 12, 0, 1 },
    { "Drill", "Your army", "+%s attack speed", G_ARMY, U_PCT, 60, 12, 0, 1 },
    { "Good Rations", "Your army", "+%s hit points", G_ARMY, U_PCT, 90, 12, 0, 1 },
    { "Keen Eyes", "Your ranged units", "+%s range", G_ARMY, U_TILES, 1, 8, 0, 1 },
    { "War School", "Your army", "every soldier at least veteran level %s", G_ARMY, U_LEVEL, 1, 12, 1, 1 },
    { "Infantry Drill", "Your infantry", "+%s damage", G_ARMY, U_PCT, 120, 12, 0, 1 },
    { "Cavalry Barding", "Your cavalry and armour", "+%s hit points", G_ARMY, U_PCT, 120, 12, 0, 1 },
    { "Gunnery School", "Your siege and artillery", "+%s range", G_ARMY, U_TILES, 1, 8, 0, 1 },
    { "Tribute", "At once", "%s food, timber, wealth and metal", G_NOW, U_TRIBUTE, 180, 0, 0, 1 },
    { "Mercenaries", "At once", "two squads of your age's infantry join you", G_NOW, U_MERC, 2, 0, 0, 0 },
    { "Respite", "The next wave", "%s smaller", G_NOW, U_LESS, 180, 0, 0, 1 },
};

int g_cards_on;
static int g_q[8][NK];         /* quarters, by player index and kind */
static int g_pending[64], g_np; /* picks the script has not taken yet: p | kind << 3 | big << 8 */
static int g_director = -1;     /* the battle director's player index (it gets no cards) */
static int g_installed;
static unsigned g_rand = 0x2545F491u;

static unsigned rnd(void)
{
    g_rand ^= g_rand << 13;
    g_rand ^= g_rand >> 17;
    g_rand ^= g_rand << 5;
    return g_rand;
}

static char *game(void) { return *(char **)VA(VA_GAME_PTR); }
static int multiplayer(void)
{
    char *g = game();
    return !g || (g[0x820] & 4);
}
static int local_player(void)
{
    char *c = *(char **)VA(VA_CONSOLE_PTR);
    int p = c ? *(int *)(c + 0x298) : -1;
    return p >= 0 && p < 8 ? p : -1;
}
static int paused(void)
{
    char *t = *(char **)VA(VA_TURN_CONTROL);
    return t && (t[0x10] & 1);
}
static int age_of(int p) /* 0 Ancient .. 7 Information */
{
    int n = p + 1;
    int a = ((int(__attribute__((thiscall)) *)(void *, const int *))VA(VA_CURRENT_AGE))(NULL, &n);
    return a < 1 ? 0 : a > 8 ? 7 : a - 1;
}

static int pct(int p, int k) { return k_cards[k].value * g_q[p][k] / 4; }

int cards_bonus(int p, int cls, int cat, int stat)
{
    if (p < 0 || p > 7)
        return 0;
    int army = cls != CC_BUILD && cat != 5; /* not Citizens and caravans (CAT Civilian) */
    int v = 0;
    switch (stat) {
    case CS_HITS:
        if (cls == CC_BUILD)
            v = pct(p, K_B_HITS);
        else if (army)
            v = pct(p, K_U_HITS) + (cat == 1 || cat == 2 ? pct(p, K_L_CAV) : 0);
        break;
    case CS_DMG:
        if (cls == CC_BUILD)
            v = pct(p, K_B_DMG);
        else if (army)
            v = pct(p, K_U_DMG) + (cls == CC_HERO ? pct(p, K_H_DMG) : 0) + (cat == 0 ? pct(p, K_L_INF) : 0);
        break;
    case CS_RATE:
        v = cls == CC_BUILD ? pct(p, K_B_RATE) : army ? pct(p, K_U_RATE) : 0;
        break;
    case CS_TAKEN:
        if (cls == CC_BUILD)
            v = pct(p, K_B_TAKEN);
        else if (army)
            v = pct(p, K_U_TAKEN) + (cls == CC_HERO ? pct(p, K_H_TAKEN) : 0);
        break;
    case CS_RANGE:
        if (cls == CC_BUILD)
            v = g_q[p][K_B_RANGE] / 4;
        else if (army)
            v = g_q[p][K_U_RANGE] / 4 + (cls == CC_HERO ? g_q[p][K_H_EYE] / 4 : 0) +
                (cat == 3 ? g_q[p][K_L_ART] / 4 : 0);
        break;
    case CS_LOS:
        v = cls == CC_HERO ? g_q[p][K_H_EYE] / 4 : 0;
        break;
    case CS_REGEN:
        v = cls == CC_HERO ? pct(p, K_H_REGEN) : 0;
        break;
    }
    return v;
}

int cards_min_level(int p) { return p >= 0 && p < 8 ? g_q[p][K_U_SCHOOL] / 4 : 0; }

int cards_hero_extra(int p, int rider)
{
    if (p < 0 || p > 7)
        return 0;
    return rider ? g_q[p][K_H_SQUAD] / 8 : g_q[p][K_H_SQUAD] / 4;
}

static void any_on(void)
{
    g_cards_on = 0;
    for (int p = 0; p < 8; p++)
        for (int k = 0; k < NK; k++)
            if (g_q[p][k])
                g_cards_on = 1;
}

/* a kind player p may be offered: not at its cap, the hero's only with a hero standing */
static int offerable(int p, int k, int boss, int nboss)
{
    if (!boss && !k_cards[k].ruin)
        return 0;
    if (boss && k_cards[k].rare && nboss < 2)
        return 0;
    if (k_cards[k].cap && g_q[p][k] + (boss ? 4 : 1) > k_cards[k].cap)
        return 0;
    if (k_cards[k].group == G_HERO && !ranks_has_hero(p))
        return 0;
    return 1;
}

static int pick_kinds(int p, int boss, int nboss, int *out, int want)
{
    int pool[NK], n = 0;
    for (int k = 0; k < NK; k++)
        if (offerable(p, k, boss, nboss))
            pool[n++] = k;
    int got = 0;
    while (got < want && n > 0) {
        int i = (int)(rnd() % (unsigned)n);
        out[got++] = pool[i];
        pool[i] = pool[--n];
    }
    return got;
}

/* the card's effect for this size, as the card shows it */
static void amount(char *out, int n, int p, int k, int q)
{
    int v = k_cards[k].value * q; /* quarters */
    char a[32];
    switch (k_cards[k].unit) {
    case U_PCT:
    case U_LESS: { /* tenths of a percent, x q / 4 */
        int t = v / 4; /* tenths */
        if (t % 10)
            snprintf(a, sizeof a, "%s%d.%d%%", k_cards[k].unit == U_LESS && k != K_I_RESPITE ? "-" : "", t / 10, t % 10);
        else
            snprintf(a, sizeof a, "%s%d%%", k_cards[k].unit == U_LESS && k != K_I_RESPITE ? "-" : "", t / 10);
        break;
    }
    case U_REGEN: { /* per mille a second x q / 4, shown in hundredths of a percent */
        int h = v * 5 / 2;
        snprintf(a, sizeof a, "%d.%02d%%", h / 100, h % 100);
        break;
    }
    case U_TRIBUTE:
        snprintf(a, sizeof a, "%d", k_cards[k].value * (age_of(p) + 1) * q / 4);
        break;
    case U_TILES:
    case U_LEVEL:
    case U_SOLDIER:
        if (q % 4 == 0)
            snprintf(a, sizeof a, "%d", v / 4);
        else
            snprintf(a, sizeof a, "%d/4", v);
        break;
    default:
        a[0] = 0;
    }
    snprintf(out, n, lang_text(k_cards[k].fmt), a);
}

/* the pick: the bonus now, and the code for the script */
static void apply(int p, int k, int big)
{
    int q = big ? 4 : 1;
    if (k == K_H_LEVEL)
        ranks_hero_quarter_levels(p, q);
    else if (k_cards[k].group != G_NOW) {
        g_q[p][k] += q;
        if (k_cards[k].cap && g_q[p][k] > k_cards[k].cap)
            g_q[p][k] = k_cards[k].cap;
    }
    any_on();
    if (k == K_B_HITS || k == K_U_HITS || k == K_L_CAV)
        ranks_refresh_hits(p);
    if (g_np < (int)(sizeof g_pending / sizeof g_pending[0]))
        g_pending[g_np++] = p | k << 3 | (big ? 1 : 0) << 8;
    logf_("cards: player %d takes %s (%s card), %d quarters of it now", p, k_cards[k].title, big ? "boss" : "small",
          g_q[p][k]);
}

/* ---- the panel ------------------------------------------------------------------------------ */

#define CW    200  /* a card */
#define CH    290
#define GAP   22
#define HEAD  62
#define FOOT  46
#define PW    (3 * CW + 4 * GAP)
#define PH    (HEAD + CH + FOOT)
#define ICON  76

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
typedef int(__attribute__((thiscall)) *write_t)(void *self, const void *str, const short *rect, int len);
typedef void(__attribute__((thiscall)) *text_color_t)(void *self, const void *c1, const void *c2, int a, int b);
typedef void *(__attribute__((thiscall)) *font_get_t)(void *family, int size, int style);

static _Alignas(16) unsigned char g_panel[0x400];
static int g_made, g_ready;
static char *g_parent;

/* what is shown: a boss card choice (three) or a small card found in ruins (one) */
static struct {
    int on, boss, p, n, k[3], nboss;
    DWORD t0;
} g_ui;
static int g_we_paused; /* the panel paused the game: it unpauses it when the last card goes */
static struct {
    int boss, p, n, k[3], nboss;
} g_wait[8];
static int g_nwait;

int cards_panel_is(void *btn) { return g_ready && btn == g_panel; }

void cards_panel_create(char *iface, char *parent)
{
    if (!parent)
        return;
    if (!g_made) {
        ((fn_i_i)VA(VA_IFACEBUTTON_CTOR))(g_panel, 1);
        g_made = 1;
    }
    int err = ((button_init_t)VA(VA_BUTTON_INIT))(g_panel, (void *)VA(VA_EMPTY_STRING), 0, 0, 0, 1, 1, parent, 0);
    if (err) {
        logf_("cards: panel init error %d", err);
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
    g_ui.on = 0;
}

static void show_next(void)
{
    if (g_ui.on || !g_nwait || !g_ready)
        return;
    g_ui.on = 1;
    g_ui.boss = g_wait[0].boss;
    g_ui.p = g_wait[0].p;
    g_ui.n = g_wait[0].n;
    g_ui.nboss = g_wait[0].nboss;
    memcpy(g_ui.k, g_wait[0].k, sizeof g_ui.k);
    memmove(g_wait, g_wait + 1, sizeof g_wait[0] * (size_t)(--g_nwait));
    g_ui.t0 = GetTickCount();
    short *pr = g_parent ? (short *)(g_parent + WIN_RECT) : NULL;
    int w = pr ? pr[1] - pr[0] : 1024, h = pr ? pr[3] - pr[2] : 768;
    ((move_t)VA(VA_WINDOW_MOVE))(g_panel, (w - PW) / 2, (h - PH) / 2 - h / 14);
    ((fn_v_i)VA(VA_WINDOW_SHOW))(g_panel, 1);
    ((fn_v)VA(VA_BRING_TO_TOP))(g_panel);
    if (!paused() && !multiplayer()) {
        ((fn_v)VA(VA_PAUSE))(NULL);
        g_we_paused = 1;
    }
}

static void close_ui(void)
{
    g_ui.on = 0;
    if (g_ready)
        ((fn_v)VA(VA_WINDOW_HIDE))(g_panel);
    if (!g_nwait) {
        if (g_we_paused && paused())
            ((fn_v)VA(VA_UNPAUSE))(NULL);
        g_we_paused = 0;
    }
    show_next();
}

static void queue_ui(int boss, int p, const int *k, int n, int nboss)
{
    if (g_nwait >= (int)(sizeof g_wait / sizeof g_wait[0]) || n <= 0)
        return;
    g_wait[g_nwait].boss = boss;
    g_wait[g_nwait].p = p;
    g_wait[g_nwait].n = n;
    g_wait[g_nwait].nboss = nboss;
    memcpy(g_wait[g_nwait].k, k, sizeof(int) * (size_t)n);
    g_nwait++;
    show_next();
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

static void text(int l, int t, int r, int b, const char *s, int size, int bold, unsigned rgb)
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
        ((write_t)VA(VA_WRITE_CENT))(buf, str, rect, len);
    ((fn_v)VA(VA_STRING_CLOSE))(str);
    memcpy(buf + BUF_FONT, saved, 12);
    memcpy(buf + BUF_COLORS, saved_colors, BUF_COLORS_N);
}

/* text broken into lines of about `width` characters (whole words), centred, one under another */
static int text_wrapped(int l, int t, int r, const char *s, int size, int bold, unsigned rgb, int width, int line_h)
{
    char line[96];
    int y = t;
    while (*s) {
        int n = (int)strlen(s), cut = n;
        if (n > width) {
            cut = width;
            while (cut > 0 && s[cut] != ' ')
                cut--;
            if (cut == 0)
                cut = width;
        }
        if (cut > (int)sizeof line - 1)
            cut = (int)sizeof line - 1;
        memcpy(line, s, (size_t)cut);
        line[cut] = 0;
        text(l, y, r, y + line_h, line, size, bold, rgb);
        y += line_h;
        s += cut;
        while (*s == ' ')
            s++;
    }
    return y;
}

static const unsigned k_group_rgb[4] = { 0x9AA4B0u, 0xE6BE3Cu, 0xD8584Au, 0x6CC070u };
static const char *k_group_name[4] = { "BUILDINGS", "HERO", "ARMY", "AT ONCE" };
static const char *k_group_tex[4] = { "card_build", "card_hero", "card_army", "card_now" };

static int inside(int mx, int my, int l, int t, int r, int b) { return mx >= l && mx < r && my >= t && my < b; }

static void card_rect(int i, int n, int *l, int *t)
{
    int x0 = n == 1 ? (PW - CW) / 2 : GAP + i * (CW + GAP);
    *l = x0;
    *t = HEAD;
}

static void draw_card(int i, int k, int hover, float e, int *depth, int base)
{
    int l, t;
    card_rect(i, g_ui.n, &l, &t);
    int g = k_cards[k].group;
    /* the reveal: the card grows and rises into place */
    float s = 0.55f + 0.45f * e;
    int cx = l + CW / 2, cy = t + CH / 2 + (int)(40.0f * (1.0f - e)) - (hover ? 8 : 0);
    int w = (int)(CW * s), h = (int)(CH * s);
    int L = cx - w / 2, T = cy - h / 2, R = L + w, B = T + h;
    unsigned a = (unsigned)(255.0f * e);
    const void *glow = qol_iface_tex("surv_glow");
    *depth = base;
    if (glow && (hover || k_cards[k].rare)) {
        unsigned ga = hover ? 200 : 120;
        add(glow, L - 30, T - 30, R + 30, B + 30, (hover ? ABGR(255, 214, 120, 0) : ABGR(255, 190, 60, 0)) | (ga * a / 255) << 24);
    }
    *depth = base + 1;
    const void *back = qol_iface_tex(k_group_tex[g]);
    if (back)
        add(back, L, T, R, B, ABGR(255, 255, 255, a));
    else {
        unsigned rgb = k_group_rgb[g];
        add(NULL, L, T, R, B, ABGR(22, 24, 28, a * 240 / 255));
        add(NULL, L + 6, T + 6, R - 6, T + 30 * h / CH, ABGR((rgb >> 16) / 3, ((rgb >> 8) & 255) / 3, (rgb & 255) / 3, a));
        frame(L, T, R, B, 3, ABGR(rgb >> 16, (rgb >> 8) & 255, rgb & 255, a));
    }
    *depth = base + 2;
    if (k_cards[k].rare) {
        const void *rim = qol_iface_tex("card_rare");
        if (rim)
            add(rim, L, T, R, B, ABGR(255, 255, 255, a));
        else
            frame(L + 4, T + 4, R - 4, B - 4, 2, ABGR(255, 214, 120, a));
    }
    char name[24];
    snprintf(name, sizeof name, "card_icon_%d", k);
    const void *icon = qol_iface_tex(name);
    int is = ICON * w / CW, iy = T + 40 * h / CH;
    if (icon)
        add(icon, cx - is / 2, iy, cx + is / 2, iy + is, ABGR(255, 255, 255, a));
    if (e < 0.92f) /* the text once the card has arrived */
        return;
    *depth = base + 3;
    unsigned rgb = k_group_rgb[g];
    text(L, T + 10, R, T + 30, lang_text(k_group_name[g]), 11, 1, rgb);
    text(L + 6, T + 124, R - 6, T + 146, lang_text(k_cards[k].title), 16, 1, 0xF4E2A8u);
    text(L + 6, T + 148, R - 6, T + 164, lang_text(k_cards[k].who), 11, 0, 0xA8B0B8u);
    char eff[160];
    amount(eff, sizeof eff, g_ui.p, k, g_ui.boss ? 4 : 1);
    text_wrapped(L + 10, T + 176, R - 10, eff, 14, 1, 0xF0F0F0u, 22, 20);
    if (!g_ui.boss)
        text(L + 6, B - 34, R - 6, B - 18, lang_text("Small card: a quarter of a boss card"), 10, 0, 0x8A9098u);
    else if (k_cards[k].cap && k_cards[k].group != G_NOW) {
        char have[64];
        snprintf(have, sizeof have, lang_text("You have %d/%d"), g_q[g_ui.p][k] / 4, k_cards[k].cap / 4);
        text(L + 6, B - 34, R - 6, B - 18, have, 10, 0, 0x8A9098u);
    }
}

int cards_panel_redraw(void *btn)
{
    if (!cards_panel_is(btn))
        return 0;
    if (!g_ui.on)
        return 1;
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(g_panel, &mx, &my);
    int *depth = (int *)(*(char **)VA(VA_GRAPHSYS) + GS_DEPTH), base = *depth;
    DWORD dt = GetTickCount() - g_ui.t0;
    /* the backdrop: a dark plate, a gold line under the title */
    const void *plate = qol_iface_tex("card_plate");
    if (plate)
        add(plate, 0, 0, PW, PH, ABGR(255, 255, 255, 235));
    else {
        add(NULL, 0, 0, PW, PH, ABGR(10, 12, 14, 205));
        frame(0, 0, PW, PH, 2, ABGR(230, 190, 60, 200));
    }
    for (int i = 0; i < g_ui.n; i++) {
        int l, t;
        card_rect(i, g_ui.n, &l, &t);
        float e = ((float)dt - 110.0f * (float)i) / 380.0f;
        e = e < 0 ? 0 : e > 1 ? 1 : e;
        e = 1.0f - (1.0f - e) * (1.0f - e) * (1.0f - e); /* ease out */
        int hover = e >= 1.0f && inside(mx, my, l, t, l + CW, t + CH);
        if (e > 0)
            draw_card(i, g_ui.k[i], hover, e, depth, base + 1 + 4 * i);
    }
    *depth = base + 1 + 4 * 3 + 1;
    if (g_ui.boss) {
        text(0, 12, PW, 36, lang_text("BOSS WAVE SURVIVED"), 20, 1, 0xE6BE3Cu);
        text(0, 36, PW, 54, lang_text("Choose one card. Its bonus lasts all game."), 12, 0, 0xC8CCD0u);
        text(0, PH - FOOT + 12, PW, PH - 12, lang_text("The game is paused until you choose."), 11, 0, 0x8A9098u);
    } else {
        text(0, 12, PW, 36, lang_text("RUINS EXPLORED"), 20, 1, 0xE6BE3Cu);
        text(0, 36, PW, 54, lang_text("You found a card."), 12, 0, 0xC8CCD0u);
        text(0, PH - FOOT + 12, PW, PH - 12, lang_text("Click to continue."), 11, 0, 0x8A9098u);
    }
    *depth = base;
    return 1;
}

void cards_panel_click(void)
{
    if (!g_ui.on)
        return;
    if (GetTickCount() - g_ui.t0 < 500) /* not before the cards are there */
        return;
    if (!g_ui.boss) { /* the small card is already applied: any click closes it */
        close_ui();
        return;
    }
    int mx = -1, my = -1;
    ((mouse_t)VA(VA_GET_MOUSE_POS))(g_panel, &mx, &my);
    for (int i = 0; i < g_ui.n; i++) {
        int l, t;
        card_rect(i, g_ui.n, &l, &t);
        if (inside(mx, my, l, t, l + CW, t + CH)) {
            apply(g_ui.p, g_ui.k[i], 1);
            close_ui();
            return;
        }
    }
}

/* ---- the ruins ------------------------------------------------------------------------------ */

static char *cell_at(int wx, int wy)
{
    char *w = *(char **)VA(VA_WORLD_PTR);
    if (!w || wx < 0 || wy < 0 || wx >= *(int *)(w + 0) || wy >= *(int *)(w + 4))
        return NULL;
    return *(char **)(w + 0x134) + (size_t)(wy * *(int *)(w + 0) + wx) * CELL_SIZE;
}

static char *object_of(int p, int id)
{
    if (p < 0 || p > 15 || id < 0)
        return NULL;
    char **arr = *(char ***)VA(VA_OBJ_ARRAYS + (UINT_PTR)p * 28);
    return arr ? arr[id] : NULL;
}

/* the cell's item (-1 none) and its last object (NULL: an empty chain) */
static int cell_item(char *cell, char **last)
{
    int id = *(short *)(cell + 8), pl = *(short *)(cell + 0xA);
    *last = NULL;
    for (int guard = 0; id >= 0 && guard < 4096; guard++) {
        char *o = object_of(pl, id);
        if (!o)
            return -1;
        *last = o;
        id = *(short *)(o + O_NEXT_ID);
        pl = *(short *)(o + O_NEXT_P);
    }
    return id == CHAIN_ITEM ? pl : -1;
}

static int item_alive(int idx)
{
    char **items = *(char ***)VA(VA_ITEMS);
    return idx >= 0 && idx < *(int *)VA(VA_ITEMS_N) && items && items[idx] && (items[idx][8] & 1);
}

/* ruins where an enemy building stood (tiles), in the nearest free cell around it */
static void place_ruin(int tx, int ty)
{
    int cx = tx / 4, cy = ty / 4;
    for (int r = 0; r <= 2; r++)
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r)
                    continue;
                char *cell = cell_at(cx + dx, cy + dy), *last;
                if (!cell || *(short *)cell < 0)
                    continue;
                if (cell_item(cell, &last) >= 0)
                    continue;
                short id0 = *(short *)(cell + 8), p0 = *(short *)(cell + 0xA);
                int x = (cx + dx) * WC + 0x180, y = (cy + dy) * WC + 0x180;
                int idx = ((int(__attribute__((thiscall)) *)(void *, int, int, int))VA(VA_INIT_ITEM))(
                    *(void **)VA(VA_OBJECTS), ITEM_GOODY, x, y);
                if (idx < 0)
                    continue;
                if (last) { /* init_item makes the item the cell's whole chain: the objects stay */
                    *(short *)(cell + 8) = id0;
                    *(short *)(cell + 0xA) = p0;
                    *(short *)(last + O_NEXT_ID) = CHAIN_ITEM;
                    *(short *)(last + O_NEXT_P) = (short)idx;
                }
                void *cam = *(void **)VA(VA_CAMERA);
                if (cam)
                    ((fn_v)VA(VA_CAMERA_OUTDATE))(cam);
                logf_("cards: ruins (item %d) at tile %d,%d", idx, (cx + dx) * 4 + 2, (cy + dy) * 4 + 2);
                return;
            }
    logf_("cards: no free cell for ruins near tile %d,%d", tx, ty);
}

/* Unit::explore_goody, wrapped: the item at the unit's cell before, and whether it went */
static int g_pre_item = -1;

__attribute__((used)) void c_cards_explore_pre(char *u)
{
    g_pre_item = -1;
    if (!g_installed || !u)
        return;
    int x = *(int *)(u + 0x10) ^ KEY, y = *(int *)(u + 0x14) ^ KEY;
    char *cell = cell_at(x / WC, y / WC), *last;
    if (cell)
        g_pre_item = cell_item(cell, &last);
}

__attribute__((used)) void c_cards_explore_post(char *u)
{
    int idx = g_pre_item;
    g_pre_item = -1;
    if (idx < 0 || !u || multiplayer() || g_director < 0)
        return;
    int x = *(int *)(u + 0x10) ^ KEY, y = *(int *)(u + 0x14) ^ KEY;
    char *cell = cell_at(x / WC, y / WC), *last;
    if (item_alive(idx) && cell && cell_item(cell, &last) == idx)
        return; /* still there: not explored */
    int p = (unsigned char)u[9];
    if (p > 7 || p == g_director)
        return;
    int k;
    if (pick_kinds(p, 0, 0, &k, 1) != 1)
        return;
    logf_("cards: player %d explored ruins", p);
    apply(p, k, 0);
    if (p == local_player())
        queue_ui(0, p, &k, 1, 0);
}

/* ---- the script's timers ---------------------------------------------------------------------- */

void cards_forget(void)
{
    memset(g_q, 0, sizeof g_q);
    g_np = 0;
    g_cards_on = 0;
    g_director = -1;
    g_nwait = 0;
    if (g_ui.on)
        close_ui();
    g_we_paused = 0;
}

int cards_timer(const char *a, int end, int now)
{
    int p, n;
    if (!strcmp(a, "surv_pick"))
        return g_np > 0 ? now : end;
    if (sscanf(a, "surv_pkb %d", &n) == 1)
        return g_np > 0 && n >= 0 && n < 16 && (g_pending[0] >> n & 1) ? now : end;
    if (!strcmp(a, "surv_pkack")) {
        if (g_np > 0)
            memmove(g_pending, g_pending + 1, sizeof g_pending[0] * (size_t)(--g_np));
        return end;
    }
    if (sscanf(a, "surv_director %d", &p) == 1) {
        g_director = p >= 1 && p <= 8 ? p - 1 : -1;
        return end;
    }
    if (multiplayer())
        return strncmp(a, "surv_cards ", 11) && strncmp(a, "surv_bosscards ", 15) && strncmp(a, "surv_ruin ", 10) ? -2 : end;
    if (!strncmp(a, "surv_cards ", 11)) {
        /* "surv_cards P q0 q1 ...": the script's counts, unless a pick of P's is still on its way */
        char *s;
        p = (int)strtol(a + 11, &s, 10) - 1;
        if (p < 0 || p > 7)
            return end;
        for (int i = 0; i < g_np; i++)
            if ((g_pending[i] & 7) == p)
                return end;
        for (int k = 0; k < NK && *s; k++) {
            char *e;
            long v = strtol(s, &e, 10);
            if (e == s)
                break;
            s = e;
            if (k_cards[k].group != G_NOW && k != K_H_LEVEL)
                g_q[p][k] = v < 0 ? 0 : v > 400 ? 400 : (int)v;
        }
        any_on();
        return end;
    }
    if (sscanf(a, "surv_bosscards %d %d", &p, &n) >= 1) {
        p--;
        if (p < 0 || p > 7)
            return end;
        int k[3], got = pick_kinds(p, 1, n, k, 3);
        if (got <= 0)
            return end;
        if (p == local_player())
            queue_ui(1, p, k, got, n);
        else
            apply(p, k[rnd() % (unsigned)got], 1);
        return end;
    }
    int x, y;
    if (sscanf(a, "surv_ruin %d %d", &x, &y) == 2) {
        place_ruin(x, y);
        return end;
    }
    return -2;
}

/* ---- install ---------------------------------------------------------------------------------- */

void *g_orig_explore;
void hk_cards_explore(void);
__asm__(".intel_syntax noprefix\n"
        /* Unit::explore_goody() (ecx = the unit, no arguments): the item before, the original, and
           what became of it */
        ".globl _hk_cards_explore\n_hk_cards_explore:\n"
        "    push ecx\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push dword ptr [ebx+24]\n"
        "    call _c_cards_explore_pre\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    pop ecx\n"
        "    push ecx\n"
        "    call dword ptr [_g_orig_explore]\n"
        "    pop ecx\n"
        "    push eax\n" HOOK_ENTER
        "    sub esp, 12\n"
        "    push dword ptr [ebx+24]\n"
        "    call _c_cards_explore_post\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    pop eax\n"
        "    ret\n"
        ".att_syntax prefix\n");

void cards_install(void)
{
    /* push ebp / mov ebp,esp / push -1 */
    static const unsigned char sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    unsigned char *at = (unsigned char *)VA(VA_EXPLORE_GOODY);
    static const unsigned char item_sig[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    if (memcmp(at, sig, sizeof sig) != 0 || memcmp((void *)VA(VA_INIT_ITEM), item_sig, sizeof item_sig) != 0) {
        logf_("cards: signature mismatch at Unit::explore_goody / Objects::init_item, no ruins cards");
        return;
    }
    unsigned char *mem = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    g_orig_explore = hook_trampoline(at, 5, mem);
    hook_jmp5(at, hk_cards_explore);
    g_rand ^= GetTickCount();
    g_installed = 1;
    logf_("cards: Survival reward cards and ruins installed");
}
