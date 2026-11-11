/*
 * The main menu's help line (the text under the menu for the button under the mouse, "Click to
 * quit the game."): right under the menu's frame and centred, line by line (the user,
 * 2026-11-10: it stood too low under the short main menu and went under the buttons of the long
 * Solo menu).
 *
 * TopMenu::on_window_redraw (the "helpwin" window) writes it with Buffer::wrap, left-aligned,
 * in a rect 500 wide centred on the screen whose top is the menu's top ([TopMenu+0x1238]) plus
 * a fixed 400 (440 in the Options menu), whatever the menu's length. Here:
 *  - TopMenuSelection::on_redraw's call of IFaceRenderManager::draw_border (the frame round the
 *    buttons, which follows them) notes the frame's bottom;
 *  - the "+ 400" (add ax, di / mov [ebp-0x10], ax) becomes that bottom + HELP_GAP;
 *  - the call of Buffer::wrap becomes menuhelp's own wrap: the same words to a line, each line
 *    centred (Buffer::write_cent_l).
 * Only the menus' drawing changes.
 *
 * And the menus' slide: a panel falls in from above and the old one falls out (GravWindow::animate,
 * y = y0 + v t + g t^2 / 2 with g = 10000 a second squared, set at 0x00839CB4). menu_anim_speed
 * (percent, 125 = 25 % faster, the user 2026-11-10) multiplies g by its square, so the same
 * fall takes 1 / 1.25 of the time.
 */
#include "ron.h"
#include <string.h>
#include <wchar.h>

#define VA_TOP_ADD       0x0059CED5u /* TopMenu::on_window_redraw: add ax, di / mov [ebp-0x10], ax */
#define VA_WRAP_CALL     0x0059CF94u /* ...: call Buffer::wrap(String const &, String const &, rect *, array *) */
#define VA_WRAP          0x0050FAF0u
#define VA_BORDER_CALL   0x0059F0B1u /* TopMenuSelection::on_redraw: call IFaceRenderManager::draw_border */
#define VA_DRAW_BORDER   0x0051D260u
#define VA_STR_STR       0x00A1F0A0u /* String::str() */
#define VA_STR_WCTOR     0x00A1EDD0u /* String::String(wchar_t const *): keeps the pointer */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_TEXT_WIDTH    0x0050EF70u /* Buffer::text_width(String const &, int len) */
#define VA_LINE_HEIGHT   0x0050F370u /* Buffer::text_line_height() */
#define VA_WRITE_CENT    0x0050F850u /* Buffer::write_cent_l(String const &, rect *, int len) */
#define HELP_GAP         14
#define MAX_TEXT         600
#define VA_GRAV_G        0x00839CB4u /* GravWindow::animate: mov dword [esi+0x490], 10000.0f */

static int g_on = 1;
static int g_anim_pct = 125;
int g_menu_frame_bottom = -1; /* the last drawn menu frame's bottom, its window's units */

typedef const wchar_t *(__attribute__((thiscall)) *str_t)(void *s);
typedef void(__attribute__((thiscall)) *wctor_t)(void *s, const wchar_t *w);
typedef void(__attribute__((thiscall)) *close_t)(void *s);
typedef int(__attribute__((thiscall)) *width_t)(void *buf, void *s, int len);
typedef int(__attribute__((thiscall)) *lh_t)(void *buf);
typedef int(__attribute__((thiscall)) *cent_t)(void *buf, void *s, short *rect, int len);

void menuhelp_load_config(const char *ini_path)
{
    g_on = GetPrivateProfileIntA("ui", "menu_help_centred", 1, ini_path);
    g_anim_pct = GetPrivateProfileIntA("ui", "menu_anim_speed", 125, ini_path);
}

/* the width of w[0..n) in the buffer's font */
static int width_of(void *buf, wchar_t *tmp, const wchar_t *w, int n)
{
    memcpy(tmp, w, n * sizeof(wchar_t));
    tmp[n] = 0;
    unsigned char s[0x14] __attribute__((aligned(4))) = { 0 };
    ((wctor_t)VA(VA_STR_WCTOR))(s, tmp);
    int r = ((width_t)VA(VA_TEXT_WIDTH))(buf, s, n);
    ((close_t)VA(VA_STRING_CLOSE))(s);
    return r;
}

static void write_line(void *buf, wchar_t *tmp, const wchar_t *w, int n, short *rect)
{
    memcpy(tmp, w, n * sizeof(wchar_t));
    tmp[n] = 0;
    unsigned char s[0x14] __attribute__((aligned(4))) = { 0 };
    ((wctor_t)VA(VA_STR_WCTOR))(s, tmp);
    ((cent_t)VA(VA_WRITE_CENT))(buf, s, rect, n);
    ((close_t)VA(VA_STRING_CLOSE))(s);
}

/* In place of Buffer::wrap: the words to lines as wide as the rect, each line centred in it. */
__attribute__((used, thiscall)) int c_menuhelp_wrap(void *buf, void *text, void *text2, short *rect, void *arr)
{
    (void)text2;
    (void)arr;
    if (!text || !rect)
        return 0;
    const wchar_t *w = ((str_t)VA(VA_STR_STR))(text);
    int len = *(unsigned short *)((char *)text + 8);
    if (!w || len <= 0)
        return 0;
    if (len > MAX_TEXT)
        len = MAX_TEXT;
    static wchar_t tmp[MAX_TEXT + 1];
    int lh = ((lh_t)VA(VA_LINE_HEIGHT))(buf);
    if (lh <= 0 || lh > 64)
        lh = 18;
    int room = rect[1] - rect[0], y = rect[2], lines = 0;
    int start = 0;
    while (start < len && y + lh <= rect[3] + lh) {
        while (start < len && w[start] == L' ')
            start++;
        if (start >= len)
            break;
        /* the longest run of whole words that fits; a line break in the text ends a line */
        int end = start, last_fit = -1;
        while (end < len) {
            int next = end;
            while (next < len && w[next] != L' ' && w[next] != L'\n')
                next++;
            if (width_of(buf, tmp, w + start, next - start) > room && last_fit >= 0)
                break;
            last_fit = next;
            end = next;
            if (end < len && w[end] == L'\n')
                break;
            if (end < len)
                end++; /* the space */
        }
        if (last_fit < 0)
            last_fit = len;
        short r[4] = { rect[0], rect[1], (short)y, (short)(y + lh) };
        write_line(buf, tmp, w + start, last_fit - start, r);
        lines++;
        y += lh;
        start = last_fit;
        if (start < len && w[start] == L'\n')
            start++;
    }
    return lines;
}

void hk_menuhelp_border(void);
void hk_menuhelp_top(void);
void *g_menuhelp_border = 0;

__asm__(".intel_syntax noprefix\n"
        /* draw_border(rect *, ...): the frame's bottom (short +6), then draw_border itself */
        ".globl _hk_menuhelp_border\n_hk_menuhelp_border:\n"
        "    mov eax, [esp+4]\n"
        "    movsx eax, word ptr [eax+6]\n"
        "    mov dword ptr [_g_menu_frame_bottom], eax\n"
        "    jmp dword ptr [_g_menuhelp_border]\n"

        /* in place of add ax, di / mov [ebp-0x10], ax: ax = the menu's top + its frame's bottom +
           the gap (or + di as before, before any frame was drawn) */
        ".globl _hk_menuhelp_top\n_hk_menuhelp_top:\n"
        "    cmp dword ptr [_g_menu_frame_bottom], 0\n"
        "    jle 1f\n"
        "    add ax, word ptr [_g_menu_frame_bottom]\n"
        "    add ax, 14\n"
        "    jmp 2f\n"
        "1:\n"
        "    add ax, di\n"
        "2:\n"
        "    mov word ptr [ebp-0x10], ax\n"
        "    ret\n"
        ".att_syntax prefix\n");

static int call_to(UINT_PTR at, UINT_PTR target)
{
    unsigned char *p = (unsigned char *)VA(at);
    return p[0] == 0xE8 && (UINT_PTR)(p + 5 + *(INT32 *)(p + 1)) == VA(target);
}

static void write_call(UINT_PTR at, const void *to, int nops)
{
    unsigned char *p = (unsigned char *)VA(at);
    DWORD old;
    VirtualProtect(p, 5 + nops, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE8;
    *(INT32 *)(p + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(p + 5));
    for (int i = 0; i < nops; i++)
        p[5 + i] = 0x90;
    VirtualProtect(p, 5 + nops, old, &old);
}

static void anim_speed(void)
{
    static const unsigned char sig[10] = { 0xC7, 0x86, 0x90, 0x04, 0, 0, 0x00, 0x40, 0x1C, 0x46 };
    if (g_anim_pct == 100 || g_anim_pct < 25 || g_anim_pct > 400)
        return;
    unsigned char *p = (unsigned char *)VA(VA_GRAV_G);
    if (memcmp(p, sig, sizeof sig) != 0) {
        logf_("menu slide: signature mismatch, speed left as it is");
        return;
    }
    float k = g_anim_pct / 100.0f, g = 10000.0f * k * k;
    DWORD old;
    VirtualProtect(p, 10, PAGE_EXECUTE_READWRITE, &old);
    memcpy(p + 6, &g, 4);
    VirtualProtect(p, 10, old, &old);
    logf_("menu slide: %d%% of the game's speed", g_anim_pct);
}

void menuhelp_install(void)
{
    anim_speed();
    if (!g_on)
        return;
    static const unsigned char top_sig[7] = { 0x66, 0x03, 0xC7, 0x66, 0x89, 0x45, 0xF0 };
    if (memcmp((void *)VA(VA_TOP_ADD), top_sig, 7) != 0 || !call_to(VA_WRAP_CALL, VA_WRAP) ||
        !call_to(VA_BORDER_CALL, VA_DRAW_BORDER)) {
        logf_("menu help: signature mismatch, the menus' help line left as it is");
        return;
    }
    g_menuhelp_border = (void *)VA(VA_DRAW_BORDER);
    write_call(VA_BORDER_CALL, hk_menuhelp_border, 0);
    write_call(VA_TOP_ADD, hk_menuhelp_top, 2);
    write_call(VA_WRAP_CALL, c_menuhelp_wrap, 0);
    logf_("menu help: under the menu's frame, centred");
}
