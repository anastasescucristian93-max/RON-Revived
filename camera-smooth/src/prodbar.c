/*
 * Production bar: a building of yours under the cursor (or selected) that is training a unit
 * or researching shows a thin cyan bar under its health bar, filled as far as the job is done.
 *
 * WallOut::draw_health_bar(int, int) (buildings and walls) ends with
 * GraphicPieces::draw_health_bar(rect, fraction, fill, back); that call (0x0064108B, with
 * ebx = the building) is pointed at hk_prod_hb, which draws the health bar and then ours one
 * bar height lower, with the same function.
 *
 * Progress, as IFaceSelected::update_progress computes it for the selected building:
 * data = object vtable +0x3C (); nothing made when the byte data +0x82 (the number of items
 * queued) is 0, since the queue keeps its last item; item = BuildQueueData::get_queue(data +
 * 0x88, 0) (-1 = idle; 0x29A counts as 0x29B); done = get_job_counter(data + 0x88, 0) /
 * ObjectData::train_time(data, item).
 *
 * GraphicPieces::draw_health_bar takes the fraction in xmm2 (whole-program optimised), the rest
 * on the stack (rect, fill, back), ret 0xC.
 *
 * Over the health bar, the icon of what is made (production_icon). The bar's hook only notes the
 * building and the item; the icon is drawn in the map window's redraw (rankfx.c's
 * Console::on_redraw, with rankfx_anchor for the place), as the interface draws, not the map.
 * It is cut from the type's cell the way IFaceButton::on_redraw does for a unit or research
 * button: types = [0x00E85DDC]; type +0xF0 texture index (signed byte, < 0 = none), +0x120
 * column, +0x138 row, or +0x150 / +0x180 (shorts) an explicit x, y when both are >= 0; the
 * IFaceTex (16 bytes: +4 x0, +6 x1, +8 y0, +0xA y1 shorts, +0xC texture) starts as a copy of the
 * first cell at 0x00ECFB6C and is moved by a cell's width x column and height x row.
 *
 * Only the drawing is changed, so it is multiplayer-safe.
 */
#include "ron.h"
#include <string.h>

#define VA_CALL_SITE    0x0064108Bu /* WallOut::draw_health_bar(int, int): call GraphicPieces::draw_health_bar */
#define VA_GP_BAR       0x00908650u /* GraphicPieces::draw_health_bar(rect *, float (xmm2), Color &, Color &) */
#define VA_GET_QUEUE    0x00630670u /* BuildQueueData::get_queue(int) const, ret 4 */
#define VA_JOB_COUNTER  0x00630690u /* BuildQueueData::get_job_counter(int) const, ret 4 */
#define VA_TRAIN_TIME   0x006508C0u /* ObjectData::train_time(int) const, ret 4 */
#define VA_OBJECTS      0x00C0AB84u /* Object ** per player, stride 28 */
#define VA_CONSOLE      0x00C06210u /* Console *: +0x298 local player, +0x2A0 selection index */
#define VA_MOUSEOVER    0x00C06230u /* MouseOver *: +4 bit 4 object, +0x4C player, +0x58 id */
#define VA_MEMBER       0x0070F8F0u /* GroupData::member(id, player, alive) const, ret 0xC */
#define VA_SELECTIONS   0x00E8D444u
#define VA_CYAN         0x00C8D1DCu
#define VA_BLACK        0x00C8D260u
#define VA_TYPES        0x00E85DDCu /* Types::types: Type * by index (units, buildings, research) */
#define VA_ICON_CELL    0x00ECFB6Cu /* IFaceTex of the first icon cell (IFaceButton::data) */

#define QUEUE_OFF 0x88
#define QUEUE_LEN 0x82 /* BuildData: items really queued (byte); the queue keeps its last item */
#define T_ICON_TEX  0xF0
#define T_ICON_COL  0x120
#define T_ICON_ROW  0x138
#define T_ICON_X    0x150
#define T_ICON_Y    0x180
#define MAX_ICONS   16

typedef int(__attribute__((thiscall)) *int1_t)(void *self, int a);
typedef void *(__attribute__((thiscall)) *data_t)(void *obj);
typedef int(__attribute__((thiscall)) *member_t)(void *group, int id, int player, int alive);

int g_prodbar_enabled;
UINT_PTR g_gp_bar;
static int g_icon_on, g_icon_size_pct = 100;

/* the buildings whose bar was drawn since the last map window redraw */
static struct {
    char *obj;
    int player, id, item;
} g_icons[MAX_ICONS];
static int g_icon_n;

void prodbar_load_config(const char *ini_path)
{
    g_prodbar_enabled = GetPrivateProfileIntA("qol", "production_bar", 1, ini_path);
    g_icon_on = GetPrivateProfileIntA("qol", "production_icon", 1, ini_path);
    g_icon_size_pct = GetPrivateProfileIntA("qol", "production_icon_size", 100, ini_path);
    if (g_icon_size_pct < 25 || g_icon_size_pct > 400)
        g_icon_size_pct = 100;
}

/* GraphicPieces::draw_health_bar(gp, rect, frac, fill, back) from C */
void prod_draw(void *gp, short *rect, float frac, const void *fill, const void *back);
__asm__(".intel_syntax noprefix\n"
        "_prod_draw:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    movss xmm2, dword ptr [ebp+16]\n"
        "    push dword ptr [ebp+24]\n"
        "    push dword ptr [ebp+20]\n"
        "    push dword ptr [ebp+12]\n"
        "    mov ecx, [ebp+8]\n"
        "    call [_g_gp_bar]\n"
        "    pop ebp\n"
        "    ret\n"
        ".att_syntax prefix\n");

static int interesting(int id, int player)
{
    char *console = *(char **)VA(VA_CONSOLE);
    if (!console || player != *(int *)(console + 0x298))
        return 0;
    char *mo = *(char **)VA(VA_MOUSEOVER);
    if (mo && (*(int *)(mo + 4) & 4) && *(int *)(mo + 0x58) == id && *(int *)(mo + 0x4C) == player)
        return 1;
    char *sel = *(char **)VA(VA_SELECTIONS);
    return sel && ((member_t)VA(VA_MEMBER))(sel + *(int *)(console + 0x2A0) * 0x9F0, id, player, 1);
}

/* 0..1, or -1 when the building makes nothing; *made = the type it makes */
static float progress(char *obj, int *made)
{
    void **vt = *(void ***)obj;
    char *data = ((data_t)vt[0x3C / 4])(obj);
    if (!data || !data[QUEUE_LEN])
        return -1.0f;
    int item = ((int1_t)VA(VA_GET_QUEUE))(data + QUEUE_OFF, 0);
    if (item < 0)
        return -1.0f;
    if (item == 0x29A)
        item = 0x29B;
    *made = item;
    int done = ((int1_t)VA(VA_JOB_COUNTER))(data + QUEUE_OFF, 0);
    int total = ((int1_t)VA(VA_TRAIN_TIME))(data, item);
    if (total <= 0)
        return -1.0f;
    float f = (float)done / (float)total;
    return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
}

__attribute__((used)) void c_prod_bar(char *b, short *rect)
{
    int player = (unsigned char)b[9], id = *(short *)(b + 0xA);
    if (player >= 8 || id < 0 || !(b[8] & 1))
        return;
    char **objs = *(char ***)(VA(VA_OBJECTS) + 28 * player);
    char *obj = objs ? objs[id] : NULL;
    if (obj)
        ranks_draw_insignia(obj, rect); /* the rank of a defensive building, any owner */
    if (!obj || !interesting(id, player))
        return;
    int item;
    float f = progress(obj, &item);
    if (f < 0.0f)
        return;
    short h = rect[3] - rect[2];
    if (h < 1)
        h = 1;
    short r[4] = { rect[0], rect[1], (short)(rect[3] + 1), (short)(rect[3] + 1 + h) };
    prod_draw(*(void **)VA(0x00C06214u), r, f, (void *)VA(VA_CYAN), (void *)VA(VA_BLACK));
    if (g_icon_on && g_icon_n < MAX_ICONS) {
        for (int i = 0; i < g_icon_n; i++)
            if (g_icons[i].obj == obj)
                return;
        g_icons[g_icon_n].obj = obj;
        g_icons[g_icon_n].player = player;
        g_icons[g_icon_n].id = id;
        g_icons[g_icon_n].item = item;
        g_icon_n++;
    }
}

/* The icon of a type as its build or research button shows it; 0 when it has none. */
static int icon_of(int item, unsigned char tex[16])
{
    char **types = *(char ***)VA(VA_TYPES);
    char *t = types && item >= 0 ? types[item] : NULL;
    if (!t || (signed char)t[T_ICON_TEX] < 0)
        return 0;
    memcpy(tex, (const void *)VA(VA_ICON_CELL), 16);
    short *r = (short *)(tex + 4); /* x0, x1, y0, y1 */
    int x = *(short *)(t + T_ICON_X), y = *(short *)(t + T_ICON_Y), dx, dy;
    if (x >= 0 && y >= 0) {
        dx = x - r[0];
        dy = y - r[2];
    } else {
        dx = (short)(r[1] - r[0] + 1) * (signed char)t[T_ICON_COL];
        dy = (short)(r[3] - r[2] + 1) * (signed char)t[T_ICON_ROW];
    }
    r[0] += dx, r[1] += dx, r[2] += dy, r[3] += dy;
    *(int *)(tex + 0xC) = (signed char)t[T_ICON_TEX];
    return 1;
}

/* From the map window's redraw (rankfx.c): the icons of the buildings noted since the last one,
   centred over their health bar. */
void prodbar_draw_icons(void)
{
    int n = g_icon_n;
    g_icon_n = 0;
    if (!n)
        return;
    float s = 30.0f * rankfx_zoom();
    s = (s < 16.0f ? 16.0f : s > 32.0f ? 32.0f : s) * (float)g_icon_size_pct / 100.0f;
    for (int i = 0; i < n; i++) {
        int p = g_icons[i].player;
        char **objs = *(char ***)(VA(VA_OBJECTS) + 28 * p);
        char *obj = objs ? objs[g_icons[i].id] : NULL;
        unsigned char tex[16];
        float x, y;
        if (obj != g_icons[i].obj || !(obj[8] & 1) || !icon_of(g_icons[i].item, tex) ||
            !rankfx_anchor(obj, &x, &y))
            continue;
        /* the cell's own shape (61 x 39 on the game's icon pages), not a square (the user, 2026-10-29) */
        short *r = (short *)(tex + 4);
        float cw = (float)(r[1] - r[0] + 1), ch = (float)(r[3] - r[2] + 1);
        float w = s * 1.25f, h = ch > 0 && cw > 0 ? w * ch / cw : s;
        rf_add(tex, x - w * 0.5f, y - 5.0f - h, w, h, 0xFFFFFFFFu);
    }
}

/* in place of GraphicPieces::draw_health_bar(rect, fill, back) with xmm2 = fraction; ebx = building */
__asm__(".intel_syntax noprefix\n"
        "_hk_prod_hb:\n"
        "    push ebp\n"
        "    mov ebp, esp\n"
        "    push dword ptr [ebp+16]\n"
        "    push dword ptr [ebp+12]\n"
        "    push dword ptr [ebp+8]\n"
        "    call [_g_gp_bar]\n"
        "    push eax\n"
        "    push dword ptr [ebp+8]\n"
        "    push ebx\n"
        "    call _c_prod_bar\n"
        "    add esp, 8\n"
        "    pop eax\n"
        "    pop ebp\n"
        "    ret 12\n"
        ".att_syntax prefix\n");
void hk_prod_hb(void);

/* Called from install_hooks once the exe is known to be the supported version. */
void prodbar_install(void)
{
    if (!g_prodbar_enabled)
        return;
    unsigned char *at = (unsigned char *)VA(VA_CALL_SITE);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(VA_GP_BAR)) {
        logf_("production bar: call site mismatch, turned off");
        g_prodbar_enabled = 0;
        return;
    }
    g_gp_bar = VA(VA_GP_BAR);
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)hk_prod_hb - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    logf_("production bar installed%s", g_icon_on ? ", with the icon of what is made" : "");
}
