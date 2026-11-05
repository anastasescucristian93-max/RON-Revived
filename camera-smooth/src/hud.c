/*
 * The resource panel (top left) and the population / city counters easier to read:
 *  - their numbers drawn larger (hud_scale, 1.25 = 25% larger). The interface theme
 *    (ui-theme\tools\build_menu_theme.py, HUD_SCALE) makes the panel's rects larger by the
 *    same factor and moves the counters next to the commerce cap; here the digits follow.
 *  - the income rates ("/+40") green instead of grey (green_rates).
 * The glyphs are point-sampled pixel art: at a total scale (interface scale times this one)
 * that is not a whole number, some glyph pixels get one screen pixel more than others and
 * the outlines show holes (2026-10-25, 1.5 x 1.25). So the factor used is the one closest
 * to hud_scale that makes the total a whole number (1.5: 4/3, every glyph pixel 2x2), or 1.
 *
 * The numbers are not a font: IFaceRenderManager::write draws one textured quad per
 * character (glyphs at render manager + 0xCE4, 16 bytes each, 13 per style) through
 * add_smooth, at the glyph's own size. So while IFaceResources::on_redraw,
 * IFaceMainBase::draw_pop_cap or draw_city_cap runs, write is wrapped: each glyph quad is
 * scaled around the text's start point, and write's result (the x after the text) and
 * write_width's are scaled to match, so the callers place the next piece of text right.
 *
 * add_smooth(tex, x, y, w, h, color, type) takes x and y in xmm2 and xmm3 (the compiler's
 * own convention), w and h as floats on the stack after tex.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define VA_ADD_SMOOTH   0x0051B700u /* IFaceRenderManager::add_smooth, ret 0x14 */
#define VA_WRITE        0x0051F660u /* IFaceRenderManager::write(String &, x, y, Color, style), returns x after, ret 0x1C */
#define VA_WRITE_WIDTH  0x0051F5C0u /* IFaceRenderManager::write_width(String &, style), ret 8 */
#define VA_RES_REDRAW   0x0081B3B0u /* IFaceResources::on_redraw() */
#define VA_POP_CAP      0x0080FF90u /* IFaceMainBase::draw_pop_cap() */
#define VA_CITY_CAP     0x0080FBE0u /* IFaceMainBase::draw_city_cap() */
#define AT_RATE_COLOR   0x00818B6Eu /* IFaceResources::draw_resource: mov esi, offset GREY (the rate's colour) */
#define VA_GREY         0x00C8D218u
#define VA_GREEN        0x00C8D1D0u

static float g_scale = 1.25f;
static int g_green = 1;

/* read by the asm stubs */
int g_hud_zone;      /* inside one of the three drawing functions */
int g_hud_text;      /* inside write, in a zone: add_smooth scales */
float g_hud_s, g_hud_x0, g_hud_y0;
void *g_orig_add_smooth, *g_orig_write, *g_orig_write_width, *g_orig_res_redraw, *g_orig_pop_cap,
    *g_orig_city_cap;

void hud_load_config(const char *ini_path)
{
    g_scale = ini_float(ini_path, "ui", "hud_scale", 1.25f);
    if (g_scale < 1.0f || g_scale > 2.0f)
        g_scale = 1.0f;
    g_green = GetPrivateProfileIntA("ui", "green_rates", 1, ini_path);
}

typedef int(__attribute__((thiscall)) *write_t)(void *self, int s, int x, int y, int c0, int c1, int c2, int style);
typedef int(__attribute__((thiscall)) *write_width_t)(void *self, int s, int style);

/* args: String *, x, y, Color (3 dwords), style */
/* hud_scale adjusted so that the glyphs land on whole screen pixels (see the top). When that
   would undo the enlargement altogether (interface at 100 %: 1.25 rounds to 1, while the theme
   still makes the panel 25 % larger), hud_scale is used as it is: a few glyph pixels one screen
   pixel wider beat digits too small for their panel. */
static float pixel_exact(void)
{
    float ui = ui_scale_now(), best = 1.0f;
    for (int n = (int)(ui + 0.999f); n <= (int)(ui * g_scale) + 1; n++) {
        float r = n / ui;
        if (r >= 1.0f && fabsf(r - g_scale) <= fabsf(best - g_scale))
            best = r;
    }
    return best == 1.0f && g_scale > 1.0f ? g_scale : best;
}

/* the numbers' enlargement while drawing the panel or the counters (respanel.c sizes its lines by it) */
float hud_text_scale(void)
{
    return g_hud_zone && g_scale != 1.0f ? pixel_exact() : 1.0f;
}

/* The game's number font is pixel art of about 7x13 texels: enlarged it came out in steps, with a
   yellowish fringe (colour hidden in its transparent texels, spread by the filtering). Inside the
   zones the text is drawn instead with ui-theme's digits from a real font (build_hud_digits.py:
   IFACETEX "hud_char_<n>" in the IFACEMAINBASE block, HUD_PX_PER_UNIT texels per interface unit,
   white with a black outline, tinted with the text's colour), as tall as the enlarged glyphs.
   Text with any other character, or without the theme, keeps the game's font. */
#define HUD_CHARS       "0123456789+-/" /* build_hud_digits.py CHARS */
#define HUD_PX_PER_UNIT 3
#define HUD_OVERLAP     2 /* texels of neighbouring glyphs' outlines that overlap (build_hud_digits.py STROKE) */
#define HUD_SIZE        0.92f /* of the game's enlarged glyph height */
#define VA_STRING_AS_CHAR 0x0042D710u /* String::str_as_char() const */
#define VA_RM_ADD_RECT  0x0051F3D0u /* IFaceRenderManager::add(IFaceTex *, const short rect[4], 0xAABBGGRR, type) */
#define VA_IFACE_RM     0x00EBD1C0u /* IFaceRenderManager iface_render_manager */
#define VA_FONT_TOP     0x00EBDEACu /* short top, bottom of the game's number glyphs (style 0) */
#define HUD_STYLE_SIZE  0xD0        /* the next style's glyphs: 13 IFaceTex of 16 bytes on */

typedef const char *(__attribute__((thiscall)) *as_char_t)(const void *self);
typedef void(__attribute__((thiscall)) *add_rect_t)(void *rm, const void *tex, const short *rect, unsigned color,
                                                     int type);

static const unsigned short *g_chars[sizeof HUD_CHARS - 1]; /* IFaceTex: +4 short left, right, top, bottom */

/* the glyphs, looked up each time by the first one: a new game's interface brings its own */
static int chars_ready(void)
{
    const unsigned short *first = qol_iface_tex("hud_char_0");
    if (!first)
        return 0;
    if (first != g_chars[0])
        for (int i = 0; i < (int)(sizeof g_chars / sizeof g_chars[0]); i++) {
            char name[16];
            wsprintfA(name, "hud_char_%d", i);
            g_chars[i] = qol_iface_tex(name);
            if (!g_chars[i]) {
                g_chars[0] = NULL;
                return 0;
            }
        }
    return 1;
}

/* the String's text when every character has one of ours (copied into buf), else NULL */
static const char *own_text(const void *str, char *buf, int size)
{
    const char *s = str ? ((as_char_t)VA(VA_STRING_AS_CHAR))(str) : NULL;
    if (!s || !*s)
        return NULL;
    lstrcpynA(buf, s, size);
    for (const char *p = buf; *p; p++)
        if (!strchr(HUD_CHARS, *p))
            return NULL;
    return chars_ready() ? buf : NULL;
}

/* the game's glyph height, enlarged, in interface units */
static float own_box(void)
{
    int fh = *(short *)VA(VA_FONT_TOP + 2) - *(short *)VA(VA_FONT_TOP);
    return fh * (g_scale != 1.0f ? pixel_exact() : 1.0f);
}

/* interface units per atlas texel: a little smaller than the game's glyphs (the user, 2026-11-01:
   at their full height the second counter's tops looked cut) */
static float own_scale(void)
{
    const unsigned short *t = g_chars[0];
    return own_box() * HUD_SIZE / (t[5] - t[4]);
}

static float own_width(const char *text, float k)
{
    float w = 0;
    for (const char *p = text; *p; p++) {
        const unsigned short *t = g_chars[strchr(HUD_CHARS, *p) - HUD_CHARS];
        w += (t[3] - t[2] - (p[1] ? HUD_OVERLAP : 0)) * k;
    }
    return w;
}

static int own_write(void *rm, const char *text, int x, int y, unsigned color, int style)
{
    float k = own_scale(), fx = (float)x;
    if (style == 0) /* the panel's numbers: centred where the game's enlarged glyphs were */
        y += (int)(own_box() * (1 - HUD_SIZE) / 2 + 0.5f);
    else { /* another style (the market's prices, style 1): centred on the middle of its glyphs,
              where the game centres the text in its button */
        const short *f = (const short *)VA(VA_FONT_TOP + style * HUD_STYLE_SIZE);
        y += (int)((f[1] - f[0]) / 2.0f - own_box() * HUD_SIZE / 2 + 0.5f);
    }
    for (const char *p = text; *p; p++) {
        const unsigned short *t = g_chars[strchr(HUD_CHARS, *p) - HUD_CHARS];
        short r[4] = { (short)(fx + 0.5f), (short)(fx + (t[3] - t[2]) * k + 0.5f), (short)y,
                       (short)(y + (t[5] - t[4]) * k + 0.5f) };
        ((add_rect_t)VA(VA_RM_ADD_RECT))(rm, t, r, color, 1);
        fx += (t[3] - t[2] - HUD_OVERLAP) * k;
    }
    return (int)(fx + HUD_OVERLAP * k + 0.5f);
}

/* A number in these digits, `h` interface units tall, its left edge at x, in the current window's
   coordinates (kills.c: the selection's kills). Returns its width, 0 without the theme's digits;
   with draw = 0 it only measures. */
int hud_number(int value, int x, int y, int h, unsigned color, int draw)
{
    char text[16];
    wsprintfA(text, "%d", value);
    if (!chars_ready())
        return 0;
    const unsigned short *t0 = g_chars[0];
    float k = (float)h / (t0[5] - t0[4]);
    int w = (int)(own_width(text, k) + HUD_OVERLAP * k + 0.5f);
    if (!draw)
        return w;
    float fx = (float)x;
    for (const char *p = text; *p; p++) {
        const unsigned short *t = g_chars[strchr(HUD_CHARS, *p) - HUD_CHARS];
        short r[4] = { (short)(fx + 0.5f), (short)(fx + (t[3] - t[2]) * k + 0.5f), (short)y, (short)(y + h) };
        ((add_rect_t)VA(VA_RM_ADD_RECT))((void *)VA(VA_IFACE_RM), t, r, color, 1);
        fx += (t[3] - t[2] - HUD_OVERLAP) * k;
    }
    return w;
}

__attribute__((used)) int c_hud_write(void *self, int *a)
{
    if (!g_hud_zone)
        return ((write_t)g_orig_write)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    char text[64];
    if (own_text((const void *)a[0], text, sizeof text))
        return own_write(self, text, a[1], a[2], (unsigned)a[3], a[6]);
    g_hud_s = pixel_exact();
    if (g_hud_s == 1.0f)
        return ((write_t)g_orig_write)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    g_hud_x0 = (float)a[1];
    g_hud_y0 = (float)a[2];
    g_hud_text = 1;
    int end = ((write_t)g_orig_write)(self, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
    g_hud_text = 0;
    return a[1] + (int)((end - a[1]) * g_hud_s + 0.5f);
}

__attribute__((used)) int c_hud_write_width(void *self, int s, int style)
{
    char text[64];
    if (g_hud_zone && own_text((const void *)s, text, sizeof text))
        return (int)(own_width(text, own_scale()) + 0.5f);
    int w = ((write_width_t)g_orig_write_width)(self, s, style);
    return g_hud_zone ? (int)(w * pixel_exact() + 0.5f) : w;
}

void hk_hud_add_smooth(void);
void hk_hud_write(void);
void hk_hud_write_width(void);
void hk_hud_res_redraw(void);
void hk_hud_pop_cap(void);
void hk_hud_city_cap(void);

__asm__(".intel_syntax noprefix\n"
        /* add_smooth: in a zone's write, x' = x0 + (x - x0) * s (xmm2), the same for y
           (xmm3), w and h ([esp+8], [esp+12]) times s. xmm7 is kept. */
        ".globl _hk_hud_add_smooth\n"
        "_hk_hud_add_smooth:\n"
        "    cmp dword ptr [_g_hud_text], 0\n"
        "    je 1f\n"
        "    sub esp, 16\n"
        "    movdqu [esp], xmm7\n"
        "    movss xmm7, dword ptr [_g_hud_s]\n"
        "    subss xmm2, dword ptr [_g_hud_x0]\n"
        "    mulss xmm2, xmm7\n"
        "    addss xmm2, dword ptr [_g_hud_x0]\n"
        "    subss xmm3, dword ptr [_g_hud_y0]\n"
        "    mulss xmm3, xmm7\n"
        "    addss xmm3, dword ptr [_g_hud_y0]\n"
        "    mulss xmm7, dword ptr [esp+24]\n"
        "    movss dword ptr [esp+24], xmm7\n"
        "    movss xmm7, dword ptr [_g_hud_s]\n"
        "    mulss xmm7, dword ptr [esp+28]\n"
        "    movss dword ptr [esp+28], xmm7\n"
        "    movdqu xmm7, [esp]\n"
        "    add esp, 16\n"
        "1:\n"
        "    jmp dword ptr [_g_orig_add_smooth]\n"

        /* write: 7 stack arguments, passed as a pointer */
        ".globl _hk_hud_write\n_hk_hud_write:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    lea eax, [ebx+36]\n"
        "    push eax\n"
        "    push ecx\n"
        "    call _c_hud_write\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret 0x1c\n"

        ".globl _hk_hud_write_width\n_hk_hud_write_width:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_hud_write_width\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret 8\n"

        /* the three drawing functions: thiscall, no arguments */
        ".globl _hk_hud_res_redraw\n_hk_hud_res_redraw:\n"
        "    inc dword ptr [_g_hud_zone]\n"
        "    call dword ptr [_g_orig_res_redraw]\n"
        "    dec dword ptr [_g_hud_zone]\n"
        "    ret\n"
        ".globl _hk_hud_pop_cap\n_hk_hud_pop_cap:\n"
        "    inc dword ptr [_g_hud_zone]\n"
        "    call dword ptr [_g_orig_pop_cap]\n"
        "    dec dword ptr [_g_hud_zone]\n"
        "    ret\n"
        ".globl _hk_hud_city_cap\n_hk_hud_city_cap:\n"
        "    inc dword ptr [_g_hud_zone]\n"
        "    call dword ptr [_g_orig_city_cap]\n"
        "    dec dword ptr [_g_hud_zone]\n"
        "    ret\n"
        ".att_syntax prefix\n");

static void green_rates(void)
{
    UINT_PTR *imm = (UINT_PTR *)VA(AT_RATE_COLOR);
    if (*imm != VA(VA_GREY)) {
        logf_("hud: rate colour not found, rates stay grey");
        return;
    }
    DWORD old;
    VirtualProtect(imm, 4, PAGE_EXECUTE_READWRITE, &old);
    *imm = VA(VA_GREEN);
    VirtualProtect(imm, 4, old, &old);
}

void hud_install(void)
{
    if (g_green)
        green_rates();
    if (g_scale == 1.0f)
        return;
    static const struct {
        const char *name;
        UINT_PTR va;
        unsigned char sig[6];
        int steal;
        void *hook;
        void **orig;
    } hooks[] = {
        /* push ebp / mov ebp,esp / push -1 */
        { "add_smooth", VA_ADD_SMOOTH, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5, hk_hud_add_smooth, &g_orig_add_smooth },
        /* push ebp / mov ebp,esp / sub esp,0x18 */
        { "write", VA_WRITE, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x18 }, 6, hk_hud_write, &g_orig_write },
        /* push ebp / mov ebp,esp / push ecx / push ebx */
        { "write_width", VA_WRITE_WIDTH, { 0x55, 0x8B, 0xEC, 0x51, 0x53 }, 5, hk_hud_write_width,
          &g_orig_write_width },
        /* push ebp / mov ebp,esp / and esp,-8 */
        { "IFaceResources::on_redraw", VA_RES_REDRAW, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 }, 6, hk_hud_res_redraw,
          &g_orig_res_redraw },
        { "draw_pop_cap", VA_POP_CAP, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5, hk_hud_pop_cap, &g_orig_pop_cap },
        { "draw_city_cap", VA_CITY_CAP, { 0x55, 0x8B, 0xEC, 0x6A, 0xFF }, 5, hk_hud_city_cap, &g_orig_city_cap },
    };
    int n = (int)(sizeof hooks / sizeof hooks[0]);
    for (int i = 0; i < n; i++)
        if (memcmp((void *)VA(hooks[i].va), hooks[i].sig, hooks[i].steal) != 0) {
            logf_("hud: signature mismatch at %s, numbers keep their size", hooks[i].name);
            return;
        }
    unsigned char *mem = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem)
        return;
    for (int i = 0; i < n; i++) {
        unsigned char *at = (unsigned char *)VA(hooks[i].va);
        *hooks[i].orig = hook_trampoline(at, hooks[i].steal, mem + i * 32);
        hook_jmp5(at, hooks[i].hook);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("hud: resource panel and counters numbers x%.2f", g_scale);
}
