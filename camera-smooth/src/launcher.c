/*
 * Launcher theme. patriots.exe (the "RoN EE Launcher" window shown before the game) loads
 * this DLL too. It draws its window from launcher_res\background.png (or
 * background_offline.png), its buttons from bitmap resources inside the exe (BUTTONU /
 * BUTTOND: normal / pressed, IDB_CLOSE_U, IDB_MINIMIZE_U) and their captions with GDI+ in
 * Palatino Linotype. Here:
 *  - the file overrides (override.c) give it the PNGs from mod_override\launcher_res\;
 *  - user32!LoadBitmapA/W take mod_override\launcher_res\<name>.bmp instead of the resource
 *    (the buttons come from MFC's ANSI CBitmapButton::LoadBitmaps, so through LoadBitmapA);
 *  - the four big buttons take the size of BUTTONU.bmp, centred on their place and moved by
 *    launcher_res\layout.ini (the dialog is created through CreateDialogIndirectParam, hooked
 *    to move them right after), and their captions are centred on the new size;
 *  - every control is scaled by layout.ini [window] scale_percent, the scale the theme's
 *    background was drawn at (the window takes the PNG's size by itself);
 *  - the caption font is replaced ([launcher] font, size_percent in ron_camera.ini);
 *  - a round language button under Workshop (launcher_lang.c);
 *  - a plate over the picture when a new version of the pack is out (launcher_update.c);
 *  - a sound when the mouse comes onto a button and when it is pressed.
 * Nothing is changed when mod_override\launcher_res\ does not exist.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef HBITMAP(WINAPI *load_bitmap_a_t)(HINSTANCE, LPCSTR);
typedef HBITMAP(WINAPI *load_bitmap_w_t)(HINSTANCE, LPCWSTR);
typedef int(WINAPI *font_family_t)(const WCHAR *name, void *collection, void **family);
typedef int(WINAPI *create_font_t)(void *family, float size, int style, int unit, void **font);

typedef HWND(WINAPI *create_dlg_a_t)(HINSTANCE, LPCDLGTEMPLATEA, HWND, DLGPROC, LPARAM);
typedef HWND(WINAPI *create_dlg_w_t)(HINSTANCE, LPCDLGTEMPLATEW, HWND, DLGPROC, LPARAM);

static load_bitmap_a_t r_load_bitmap_a;
static load_bitmap_w_t r_load_bitmap_w;
static create_dlg_a_t r_create_dlg_a;
static create_dlg_w_t r_create_dlg_w;
static font_family_t r_font_family;
static create_font_t r_create_font;
static char g_res_dir[MAX_PATH]; /* <game>\mod_override\launcher_res\ */
static WCHAR g_font[64];
static float g_font_scale = 1.0f;

/* The replacement for the bitmap resource `name`, or NULL. */
static HBITMAP themed_bitmap(const char *name)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s%s.bmp", g_res_dir, name);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        return NULL;
    HBITMAP b = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (b)
        logf_("launcher: bitmap %s from %s", name, path);
    return b;
}

static HBITMAP WINAPI h_load_bitmap_a(HINSTANCE inst, LPCSTR name)
{
    HBITMAP b = IS_INTRESOURCE(name) ? NULL : themed_bitmap(name);
    return b ? b : r_load_bitmap_a(inst, name);
}

static HBITMAP WINAPI h_load_bitmap_w(HINSTANCE inst, LPCWSTR name)
{
    char a[MAX_PATH];
    HBITMAP b = NULL;
    if (!IS_INTRESOURCE(name) && WideCharToMultiByte(CP_ACP, 0, name, -1, a, sizeof a, NULL, NULL))
        b = themed_bitmap(a);
    return b ? b : r_load_bitmap_w(inst, name);
}

/* Size of BUTTONU.bmp, the themed big button; 0 if there is none. */
static SIZE g_button;

static void read_button_size(void)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%sBUTTONU.bmp", g_res_dir);
    HBITMAP b = (HBITMAP)LoadImageA(NULL, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    BITMAP bm;
    if (b && GetObjectA(b, sizeof bm, &bm))
        g_button.cx = bm.bmWidth, g_button.cy = bm.bmHeight;
    if (b)
        DeleteObject(b);
}

/* patriots.exe draws each big button's caption centred on a fixed point, (96, 18) in the
   button: the middle of the template's 191x34 button. Two `mov [ebp-...], float` at RVA
   0x1e6b / 0x1e7c; their constants become the middle of the themed button. */
static void patch_caption_point(void)
{
    static const unsigned char x_ins[10] = {0xC7, 0x85, 0xD0, 0xF9, 0xFF, 0xFF, 0x00, 0x00, 0xC0, 0x42};
    static const unsigned char y_ins[10] = {0xC7, 0x85, 0xD4, 0xF9, 0xFF, 0xFF, 0x00, 0x00, 0x90, 0x41};
    unsigned char *exe = (unsigned char *)GetModuleHandleA(NULL);
    unsigned char *x_at = exe + 0x1e6b, *y_at = exe + 0x1e7c;
    if (memcmp(x_at, x_ins, 10) != 0 || memcmp(y_at, y_ins, 10) != 0) {
        logf_("launcher: caption point not found, captions left at the template's centre");
        return;
    }
    float x = g_button.cx / 2.0f + 0.5f, y = g_button.cy / 2.0f + 1.0f; /* the game's 96 / 18 */
    DWORD old;
    VirtualProtect(x_at, 0x20, PAGE_EXECUTE_READWRITE, &old);
    memcpy(x_at + 6, &x, 4);
    memcpy(y_at + 6, &y, 4);
    VirtualProtect(x_at, 0x20, old, &old);
    logf_("launcher: captions centred at %.1f, %.1f", x, y);
}

/* layout.ini [window] scale_percent: the theme's background is drawn that much larger than
   the game's 752x732 window (the launcher sizes its window to the PNG), so every control
   is scaled by it too. */
static int g_scale = 100;

static int scaled(int v)
{
    return (v * g_scale + (v < 0 ? -50 : 50)) / 100; /* as build_launcher.py's s() */
}

/* Dialog 102 is see-through by colour key (magenta): its background is bitmap 154, 752x732 all
   magenta, set by CDialogEx::SetBackgroundImage(154, BACKGR_TOPLEFT) at RVA 0x2DDE; past it
   the dialog is grey. Grown by the scale, it would show grey along the right and bottom:
   BACKGR_TILE (0) instead fills it all. */
static void patch_background_tile(void)
{
    static const unsigned char ins[11] = {0x6A, 0x01, 0x6A, 0x01, 0x68, 0x9A, 0x00, 0x00, 0x00, 0x8B, 0xCF};
    unsigned char *at = (unsigned char *)GetModuleHandleA(NULL) + 0x2dde;
    if (memcmp(at, ins, sizeof ins) != 0) {
        logf_("launcher: background image call not found, the scaled dialog may show grey edges");
        return;
    }
    DWORD old;
    VirtualProtect(at, sizeof ins, PAGE_EXECUTE_READWRITE, &old);
    at[3] = 0x00; /* push 1 (BACKGR_TOPLEFT) -> push 0 (BACKGR_TILE) */
    VirtualProtect(at, sizeof ins, old, &old);
    logf_("launcher: dialog background tiled");
}

/* The launcher is two windows: the background (dialog 103, its picture shown through
   UpdateLayeredWindow, so the PNG's size) and over it, owned by it, the controls' dialog
   (102, 720x679), centred on the 752x732 background (16,27 inside it). With the theme
   scaled, dialog 102 grows by the same scale and its controls move to the scaled place of
   the background they sat on; the launcher keeps it centred on the background.
   The four big buttons (Play, Achievements, Community, Workshop) take the size of
   BUTTONU.bmp, centred where the dialog template put them (127x21 dialog units) and moved
   down by layout.ini [buttons] dy0..dy3, written by the theme's build script. */
static void layout_dialog(HWND dlg)
{
    static const int ids[] = {1007, 1014, 1015, 1017};
    if (!dlg || !GetWindow(dlg, GW_CHILD) || (!g_button.cx && g_scale == 100))
        return;
    char ini[MAX_PATH], key[8];
    snprintf(ini, sizeof ini, "%slayout.ini", g_res_dir);
    RECT dr;
    GetWindowRect(dlg, &dr);
    int dw = dr.right - dr.left, dh = dr.bottom - dr.top;
    /* client -> background: where MFC's CenterWindow puts the dialog, before and after scaling */
    int ox = 752 / 2 - dw / 2, oy = 732 / 2 - dh / 2;
    int sx = scaled(752) / 2 - scaled(dw) / 2, sy = scaled(732) / 2 - scaled(dh) / 2;
    int n = 0;
    for (HWND w = GetWindow(dlg, GW_CHILD); w; w = GetWindow(w, GW_HWNDNEXT)) {
        RECT r;
        if (!GetWindowRect(w, &r))
            continue;
        MapWindowPoints(NULL, dlg, (POINT *)&r, 2);
        int id = GetDlgCtrlID(w), big = -1;
        for (int i = 0; i < 4; i++)
            if (ids[i] == id)
                big = i;
        if (big >= 0 && g_button.cx) {
            snprintf(key, sizeof key, "dy%d", big);
            int cx = scaled((r.left + r.right) / 2 + ox) - sx;
            int cy = scaled((r.top + r.bottom) / 2 + GetPrivateProfileIntA("buttons", key, 0, ini) + oy) - sy;
            SetWindowPos(w, NULL, cx - g_button.cx / 2, cy - g_button.cy / 2, g_button.cx, g_button.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        } else if (g_scale != 100) {
            SetWindowPos(w, NULL, scaled(r.left + ox) - sx, scaled(r.top + oy) - sy, scaled(r.right - r.left),
                         scaled(r.bottom - r.top), SWP_NOZORDER | SWP_NOACTIVATE);
        } else
            continue;
        n++;
    }
    if (g_scale != 100)
        SetWindowPos(dlg, NULL, dr.left - (scaled(dw) - dw) / 2, dr.top - (scaled(dh) - dh) / 2, scaled(dw), scaled(dh),
                     SWP_NOZORDER | SWP_NOACTIVATE);
    logf_("launcher: %d controls laid out at %d%%, big buttons %ldx%ld", n, g_scale, g_button.cx, g_button.cy);
}

/* Button sounds: a short tick when the mouse comes onto a button and the game's menu button
   sound when it is pressed ([launcher] hover_sound, click_sound: wav files from the game folder,
   empty = none; sound_volume in percent). The wavs are read into memory with their 16-bit
   samples scaled by the volume and played with the system winmm's PlaySoundA (SND_MEMORY |
   SND_ASYNC: a new sound cuts the last one). Every Button control of the launcher's dialogs is
   subclassed for it; launcher_lang.c calls launcher_sound for its own button. */
typedef BOOL(WINAPI *play_sound_t)(LPCSTR, HMODULE, DWORD);
static play_sound_t r_play_sound;
static char *g_snd[2]; /* hover, click: whole wav files in memory */
static const char PROP_PROC[] = "ron_snd_proc", PROP_HOT[] = "ron_snd_hot";

static char *load_wav(const char *rel, int volume)
{
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s%s", g_dir, rel);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD size = GetFileSize(f, NULL), got = 0;
    char *buf = size > 44 && size < (4u << 20) ? malloc(size) : NULL;
    if (buf && (!ReadFile(f, buf, size, &got, NULL) || got != size || memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4))) {
        free(buf);
        buf = NULL;
    }
    CloseHandle(f);
    if (!buf)
        return NULL;
    int bits = 0;
    for (DWORD at = 12; at + 8 <= size;) {
        DWORD len = *(DWORD *)(buf + at + 4);
        if (!memcmp(buf + at, "fmt ", 4) && len >= 16 && *(WORD *)(buf + at + 8) == 1) /* PCM */
            bits = *(WORD *)(buf + at + 22);
        if (!memcmp(buf + at, "data", 4) && bits == 16 && volume != 100) {
            short *s = (short *)(buf + at + 8);
            for (DWORD i = 0; i < len / 2 && (char *)(s + i + 1) <= buf + size; i++)
                s[i] = (short)(s[i] * volume / 100);
        }
        at += 8 + len + (len & 1);
    }
    return buf;
}

static void read_sounds(const char *ini)
{
    char sys[MAX_PATH], name[MAX_PATH];
    GetSystemDirectoryA(sys, sizeof sys);
    strncat(sys, "\\winmm.dll", sizeof sys - strlen(sys) - 1);
    HMODULE winmm = LoadLibraryA(sys); /* the real one, already loaded by our proxy */
    r_play_sound = winmm ? (play_sound_t)GetProcAddress(winmm, "PlaySoundA") : NULL;
    int volume = GetPrivateProfileIntA("launcher", "sound_volume", 70, ini);
    volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    static const char *keys[2] = {"hover_sound", "click_sound"};
    static const char *defaults[2] = {"sounds\\tabs.wav", "sounds\\buttonsoundtopbuttons.wav"};
    for (int i = 0; i < 2; i++) {
        GetPrivateProfileStringA("launcher", keys[i], defaults[i], name, sizeof name, ini);
        g_snd[i] = name[0] && volume && r_play_sound ? load_wav(name, volume) : NULL;
        logf_("launcher: %s %s", keys[i], g_snd[i] ? name : "off");
    }
}

/* 0 = the mouse came onto a button, 1 = a button was pressed. */
void launcher_sound(int which)
{
    if (r_play_sound && which >= 0 && which < 2 && g_snd[which])
        r_play_sound(g_snd[which], NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

static LRESULT CALLBACK button_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC orig = (WNDPROC)GetPropA(w, PROP_PROC);
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!GetPropA(w, PROP_HOT)) {
            SetPropA(w, PROP_HOT, (HANDLE)1);
            TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, w, 0};
            TrackMouseEvent(&tme);
            if (IsWindowEnabled(w))
                launcher_sound(0);
        }
        break;
    case WM_MOUSELEAVE:
        RemovePropA(w, PROP_HOT);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        if (IsWindowEnabled(w))
            launcher_sound(1);
        break;
    case WM_NCDESTROY:
        (IsWindowUnicode(w) ? SetWindowLongPtrW : SetWindowLongPtrA)(w, GWLP_WNDPROC, (LONG_PTR)orig);
        RemovePropA(w, PROP_PROC);
        RemovePropA(w, PROP_HOT);
        break;
    }
    return IsWindowUnicode(w) ? CallWindowProcW(orig, w, msg, wp, lp) : CallWindowProcA(orig, w, msg, wp, lp);
}

static BOOL CALLBACK attach_sound(HWND w, LPARAM lp)
{
    char cls[16];
    (void)lp;
    if (GetClassNameA(w, cls, sizeof cls) && !lstrcmpiA(cls, "Button") && !GetPropA(w, PROP_PROC)) {
        LONG_PTR orig = IsWindowUnicode(w) ? GetWindowLongPtrW(w, GWLP_WNDPROC) : GetWindowLongPtrA(w, GWLP_WNDPROC);
        SetPropA(w, PROP_PROC, (HANDLE)orig);
        (IsWindowUnicode(w) ? SetWindowLongPtrW : SetWindowLongPtrA)(w, GWLP_WNDPROC, (LONG_PTR)button_proc);
        (*(int *)lp)++;
    }
    return TRUE;
}

static void attach_sounds(HWND dlg)
{
    int n = 0;
    if (!dlg || (!g_snd[0] && !g_snd[1]))
        return;
    EnumChildWindows(dlg, attach_sound, (LPARAM)&n);
    logf_("launcher: button sounds on %d buttons", n);
}

static HWND WINAPI h_create_dlg_a(HINSTANCE inst, LPCDLGTEMPLATEA t, HWND parent, DLGPROC proc, LPARAM lp)
{
    HWND dlg = r_create_dlg_a(inst, t, parent, proc, lp);
    layout_dialog(dlg);
    launcher_lang_attach(dlg);
    launcher_update_attach(dlg);
    attach_sounds(dlg);
    return dlg;
}

static HWND WINAPI h_create_dlg_w(HINSTANCE inst, LPCDLGTEMPLATEW t, HWND parent, DLGPROC proc, LPARAM lp)
{
    HWND dlg = r_create_dlg_w(inst, t, parent, proc, lp);
    layout_dialog(dlg);
    launcher_lang_attach(dlg);
    launcher_update_attach(dlg);
    attach_sounds(dlg);
    return dlg;
}

static int WINAPI h_font_family(const WCHAR *name, void *collection, void **family)
{
    if (g_font[0] && r_font_family(g_font, collection, family) == 0) /* Ok */
        return 0;
    return r_font_family(name, collection, family);
}

static int WINAPI h_create_font(void *family, float size, int style, int unit, void **font)
{
    return r_create_font(family, size * g_font_scale, style, unit, font);
}

/* Pictures in the big panel (layout.ini [slides], written by the theme's build script when it
   has several pictures). The background window shows background.png through
   UpdateLayeredWindow; every call for it is given our own copy of that picture instead, in
   which the panel's area (x, y, w, h) holds slide_<cur>.png, and every hold_ms a timer changes
   it for another one, at random, through fog, in fade_ms: in the first half the fog of
   slide_fog.png (its colour; its alpha says where it thickens first) rolls over the picture,
   in the second half it clears from its thinnest places first, showing the next picture; it
   drifts to the left meanwhile (the fog is wider than the panel). The pictures are faded into
   the panel by
   slide_mask.png's alpha over slide_under.png (the panel without picture), as the build script
   does for the first one in background.png. The minimize and close buttons over the panel are
   see-through around their plates (magenta, the controls' dialog's colour key). All images
   premultiplied (GDI+ PARGB), as UpdateLayeredWindow wants for per-pixel alpha, but the fog
   (opaque; its alpha only orders it). */
typedef BOOL(WINAPI *update_layered_t)(HWND, HDC, POINT *, SIZE *, HDC, POINT *, COLORREF, BLENDFUNCTION *,
                                       DWORD);
typedef struct {
    UINT w, h;
    INT stride, format;
    void *scan0;
    UINT_PTR reserved;
} gp_bits_t;
typedef int(WINAPI *gp_from_file_t)(const WCHAR *, void **);
typedef int(WINAPI *gp_lock_t)(void *, const INT *rect, UINT flags, INT format, gp_bits_t *);
typedef int(WINAPI *gp_unlock_t)(void *, gp_bits_t *);
typedef int(WINAPI *gp_dispose_t)(void *);
typedef int(WINAPI *gp_size_t)(void *, UINT *);

#define MAX_SLIDES 16
#define SLIDE_TIMER 0x52E7

static update_layered_t r_update_layered;
static struct {
    int n, x, y, w, h, hold_ms, fade_ms;
    DWORD *pic[MAX_SLIDES], *under, *mask, *fog, *px; /* px: the window's picture (DIB) */
    int fog_w;
    int cur, next, fading, failed;
    DWORD start;
    HWND hwnd;
    HDC dc;
    SIZE size;
    BLENDFUNCTION blend;
    DWORD flags;
} g_show;

/* launcher_res\<name> as BGRA (malloc'd), premultiplied unless `straight`, or NULL; *w, *h its
   size. GDI+ is already started by the launcher, which draws its own background with it. */
DWORD *launcher_load_bgra(const char *name, int straight, int *w, int *h)
{
    HMODULE gp = GetModuleHandleA("gdiplus.dll");
    gp_from_file_t from_file = gp ? (gp_from_file_t)GetProcAddress(gp, "GdipCreateBitmapFromFile") : NULL;
    gp_lock_t lock = gp ? (gp_lock_t)GetProcAddress(gp, "GdipBitmapLockBits") : NULL;
    gp_unlock_t unlock = gp ? (gp_unlock_t)GetProcAddress(gp, "GdipBitmapUnlockBits") : NULL;
    gp_dispose_t dispose = gp ? (gp_dispose_t)GetProcAddress(gp, "GdipDisposeImage") : NULL;
    gp_size_t get_w = gp ? (gp_size_t)GetProcAddress(gp, "GdipGetImageWidth") : NULL;
    gp_size_t get_h = gp ? (gp_size_t)GetProcAddress(gp, "GdipGetImageHeight") : NULL;
    if (!from_file || !lock || !unlock || !dispose || !get_w || !get_h)
        return NULL;
    char path[MAX_PATH];
    WCHAR wpath[MAX_PATH];
    snprintf(path, sizeof path, "%s%s", g_res_dir, name);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    void *bmp = NULL;
    if (from_file(wpath, &bmp) != 0 || !bmp)
        return NULL;
    UINT bw = 0, bh = 0;
    get_w(bmp, &bw);
    get_h(bmp, &bh);
    INT rect[4] = {0, 0, (INT)bw, (INT)bh};
    gp_bits_t bits;
    DWORD *out = NULL;
    if (bw && bh && lock(bmp, rect, 1 /* read */, straight ? 0x0026200A /* 32bpp ARGB */ : 0x000E200B /* PARGB */, &bits) == 0) {
        out = malloc((size_t)bw * bh * 4);
        for (UINT y = 0; out && y < bh; y++)
            memcpy(out + (size_t)y * bw, (unsigned char *)bits.scan0 + (size_t)y * bits.stride, (size_t)bw * 4);
        unlock(bmp, &bits);
    }
    dispose(bmp);
    *w = (int)bw, *h = (int)bh;
    return out;
}

/* The panel's area in g_show.px, `t` (0..1) of the way from slide cur to slide next. */
static void show_compose(float t)
{
    static unsigned char ease[256]; /* smoothstep */
    if (!ease[255])
        for (int i = 0; i < 256; i++) {
            float v = i / 255.0f;
            ease[i] = (unsigned char)(v * v * (3.0f - 2.0f * v) * 255.0f + 0.5f);
        }
    int w = g_show.w, rising = t < 0.5f;
    /* the fog's front: 0 (none yet) .. 255 + FOG_SPREAD (everywhere); a pixel's fog order n
       (0..255) holds it back by n * FOG_SPREAD / 255, so the fog spreads unevenly */
    enum { FOG_SPREAD = 400 };
    int q = (int)((rising ? t : t - 0.5f) * 2.0f * (255 + FOG_SPREAD));
    int off = (int)(t * (g_show.fog_w - w));
    const DWORD *pic = g_show.pic[rising ? g_show.cur : g_show.next];
    for (int y = 0; y < g_show.h; y++) {
        DWORD *dst = g_show.px + (size_t)(g_show.y + y) * g_show.size.cx + g_show.x;
        size_t row = (size_t)y * w;
        const DWORD *fog = g_show.fog + (size_t)y * g_show.fog_w + off;
        for (int x = 0; x < w; x++) {
            DWORD p = pic[row + x], fg = fog[x], u = g_show.under[row + x];
            int n = (int)(fg >> 24), v = q - (rising ? n : 255 - n) * FOG_SPREAD / 255;
            unsigned f = ease[v < 0 ? 0 : v > 255 ? 255 : v];
            if (!rising)
                f = 255 - f;
            unsigned m = g_show.mask[row + x] >> 24, out = 0;
            for (int s = 0; s < 32; s += 8) {
                unsigned c = s == 24 ? 255 : (((p >> s) & 255) * (255 - f) + ((fg >> s) & 255) * f + 127) / 255;
                out |= ((c * m + ((u >> s) & 255) * (255 - m) + 127) / 255) << s;
            }
            dst[x] = out;
        }
    }
}

static void show_present(void)
{
    POINT zero = {0, 0};
    r_update_layered(g_show.hwnd, NULL, NULL, &g_show.size, g_show.dc, &zero, 0, &g_show.blend, g_show.flags);
}

/* The next slide, at random, but none of the last three shown (fewer with fewer slides): a
   picture comes back only after three others. */
static int show_pick(void)
{
    static unsigned seed;
    static int recent[3], shown;
    if (!seed)
        seed = GetTickCount() | 1;
    int keep = g_show.n - 1 < 3 ? g_show.n - 1 : 3, free_[MAX_SLIDES], n = 0;
    for (int i = 0; i < g_show.n; i++) {
        int used = 0;
        for (int j = 0; j < keep && j < shown; j++)
            used |= recent[j] == i;
        if (!used)
            free_[n++] = i;
    }
    seed = seed * 1103515245u + 12345u;
    int pick = free_[(seed >> 16) % (unsigned)n];
    memmove(recent + 1, recent, sizeof recent - sizeof recent[0]);
    recent[0] = pick;
    shown++;
    return pick;
}

static void CALLBACK show_tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD now)
{
    (void)msg, (void)now;
    if (!g_show.fading) {
        g_show.fading = 1;
        g_show.next = show_pick();
        g_show.start = GetTickCount();
        SetTimer(hwnd, id, 20, show_tick);
    }
    float t = (float)(GetTickCount() - g_show.start) / (float)g_show.fade_ms;
    if (t >= 1.0f) {
        g_show.cur = g_show.next;
        g_show.fading = 0;
        t = 0.0f;
        SetTimer(hwnd, id, (UINT)g_show.hold_ms, show_tick);
    }
    show_compose(t);
    if (!IsIconic(hwnd))
        show_present();
}

/* Loads the slides for the background window `hwnd` (its picture `size`); 0 if it cannot. */
static int show_start(HWND hwnd, const SIZE *size, const BLENDFUNCTION *blend, DWORD flags)
{
    char name[32] = "";
    int bw, bh, w, h;
    DWORD *bg = launcher_load_bgra("background.png", 0, &bw, &bh);
    if (!bg || bw != size->cx || bh != size->cy || g_show.x < 0 || g_show.y < 0 || g_show.x + g_show.w > bw ||
        g_show.y + g_show.h > bh) {
        logf_("launcher: slideshow off, background.png not loaded or not the window's size (%ldx%ld)", size->cx,
              size->cy);
        free(bg);
        return 0;
    }
    g_show.fog = launcher_load_bgra("slide_fog.png", 1, &g_show.fog_w, &h) /* alpha is data */;
    if (!g_show.fog || g_show.fog_w < g_show.w || h != g_show.h) {
        logf_("launcher: slideshow off, slide_fog.png missing or not %d high and at least %d wide", g_show.h,
              g_show.w);
        free(bg);
        return 0;
    }
    for (int i = 0; i <= g_show.n + 1; i++) {
        if (i < g_show.n)
            snprintf(name, sizeof name, "slide_%d.png", i + 1);
        DWORD **to = i < g_show.n ? &g_show.pic[i] : i == g_show.n ? &g_show.under : &g_show.mask;
        *to = launcher_load_bgra(i < g_show.n ? name : i == g_show.n ? "slide_under.png" : "slide_mask.png", 0, &w, &h);
        if (!*to || w != g_show.w || h != g_show.h) {
            logf_("launcher: slideshow off, %s missing or not %dx%d", i < g_show.n ? name : "slide_under/mask.png",
                  g_show.w, g_show.h);
            free(bg);
            return 0;
        }
    }
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), bw, -bh, 1, 32, BI_RGB}};
    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    g_show.dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!dib || !g_show.dc) {
        free(bg);
        return 0;
    }
    SelectObject(g_show.dc, dib);
    g_show.px = bits;
    memcpy(g_show.px, bg, (size_t)bw * bh * 4);
    free(bg);
    g_show.hwnd = hwnd;
    g_show.size = *size;
    g_show.blend = *blend;
    g_show.flags = flags;
    g_show.cur = show_pick(); /* the first one at random too */
    g_show.next = g_show.cur;
    show_compose(0.0f);
    SetTimer(hwnd, SLIDE_TIMER, (UINT)g_show.hold_ms, show_tick);
    logf_("launcher: slideshow of %d pictures at %d,%d %dx%d, every %d ms", g_show.n, g_show.x, g_show.y, g_show.w,
          g_show.h, g_show.hold_ms);
    return 1;
}

static BOOL WINAPI h_update_layered(HWND hwnd, HDC dst, POINT *at, SIZE *size, HDC src, POINT *src_at,
                                    COLORREF key, BLENDFUNCTION *blend, DWORD flags)
{
    if (!g_show.hwnd && !g_show.failed && src && size && blend)
        g_show.failed = !show_start(hwnd, size, blend, flags);
    if (hwnd == g_show.hwnd && src && size && size->cx == g_show.size.cx && size->cy == g_show.size.cy) {
        POINT zero = {0, 0};
        return r_update_layered(hwnd, dst, at, size, g_show.dc, &zero, key, blend, flags);
    }
    return r_update_layered(hwnd, dst, at, size, src, src_at, key, blend, flags);
}

/* The picture's panel in the background window (layout.ini [slides]); 0 if the theme has none. */
int launcher_picture_rect(RECT *r)
{
    if (g_show.w <= 0 || g_show.h <= 0)
        return 0;
    SetRect(r, g_show.x, g_show.y, g_show.x + g_show.w, g_show.y + g_show.h);
    return 1;
}

static void read_slides(const char *ini)
{
    g_show.n = GetPrivateProfileIntA("slides", "count", 0, ini);
    g_show.x = GetPrivateProfileIntA("slides", "x", 0, ini);
    g_show.y = GetPrivateProfileIntA("slides", "y", 0, ini);
    g_show.w = GetPrivateProfileIntA("slides", "w", 0, ini);
    g_show.h = GetPrivateProfileIntA("slides", "h", 0, ini);
    g_show.hold_ms = GetPrivateProfileIntA("slides", "hold_ms", 6000, ini);
    g_show.fade_ms = GetPrivateProfileIntA("slides", "fade_ms", 3000, ini);
    if (g_show.n > MAX_SLIDES)
        g_show.n = MAX_SLIDES;
    if (g_show.n < 2 || g_show.w <= 0 || g_show.h <= 0 || g_show.hold_ms < 500 || g_show.fade_ms < 100)
        g_show.n = 0;
}

/* Replaces the import `func` of `module`; returns the original, or NULL if not imported. */
void *launcher_patch_import(HMODULE module, const char *func, void *repl)
{
    unsigned char *base = (unsigned char *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress)
        return NULL;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); d->Name; d++) {
        IMAGE_THUNK_DATA *names = (IMAGE_THUNK_DATA *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk
                                                                                     : d->FirstThunk));
        IMAGE_THUNK_DATA *addrs = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, addrs++) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData);
            if (strcmp((const char *)ibn->Name, func) != 0)
                continue;
            void *orig = (void *)addrs->u1.Function;
            DWORD old;
            VirtualProtect(&addrs->u1.Function, sizeof(void *), PAGE_READWRITE, &old);
            addrs->u1.Function = (UINT_PTR)repl;
            VirtualProtect(&addrs->u1.Function, sizeof(void *), old, &old);
            return orig;
        }
    }
    return NULL;
}

void launcher_install(void)
{
    snprintf(g_res_dir, sizeof g_res_dir, "%smod_override\\launcher_res\\", g_dir);
    DWORD a = GetFileAttributesA(g_res_dir);
    if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY)) {
        logf_("launcher: no %s, launcher left as it is", g_res_dir);
        return;
    }
    override_install();

    char ini[MAX_PATH], buf[64];
    snprintf(ini, sizeof ini, "%sron_camera.ini", g_dir);
    GetPrivateProfileStringA("launcher", "font", "Bahnschrift SemiBold Condensed", buf, sizeof buf, ini);
    MultiByteToWideChar(CP_ACP, 0, buf, -1, g_font, 64);
    g_font_scale = GetPrivateProfileIntA("launcher", "font_size_percent", 115, ini) / 100.0f;
    read_sounds(ini);
    snprintf(ini, sizeof ini, "%slayout.ini", g_res_dir);
    g_scale = GetPrivateProfileIntA("window", "scale_percent", 100, ini);
    if (g_scale < 50 || g_scale > 300)
        g_scale = 100;
    g_font_scale *= g_scale / 100.0f; /* the captions grow with the buttons */

    read_button_size();
    if (g_button.cx)
        patch_caption_point();
    if (g_scale != 100)
        patch_background_tile();
    HMODULE user32 = GetModuleHandleA("user32.dll");
    if (user32) {
        r_load_bitmap_a = hook_export(user32, "LoadBitmapA", (void *)h_load_bitmap_a);
        r_load_bitmap_w = hook_export(user32, "LoadBitmapW", (void *)h_load_bitmap_w);
        r_create_dlg_a = hook_export(user32, "CreateDialogIndirectParamA", (void *)h_create_dlg_a);
        r_create_dlg_w = hook_export(user32, "CreateDialogIndirectParamW", (void *)h_create_dlg_w);
    }
    HMODULE exe = GetModuleHandleA(NULL);
    r_font_family = launcher_patch_import(exe, "GdipCreateFontFamilyFromName", (void *)h_font_family);
    r_create_font = launcher_patch_import(exe, "GdipCreateFont", (void *)h_create_font);
    read_slides(ini);
    if (g_show.n)
        r_update_layered = launcher_patch_import(exe, "UpdateLayeredWindow", (void *)h_update_layered);
    launcher_lang_install(g_res_dir, g_scale, g_font);
    launcher_update_install(g_res_dir, g_scale, g_font);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("launcher: theme from %s (LoadBitmapA %d, LoadBitmapW %d, CreateDialog %d/%d, font %d, font size %d, "
          "slides %d/%d), font \"%s\" at %.0f%%",
          g_res_dir, !!r_load_bitmap_a, !!r_load_bitmap_w, !!r_create_dlg_a, !!r_create_dlg_w, !!r_font_family,
          !!r_create_font, g_show.n, !!r_update_layered, buf, g_font_scale * 100.0f);
}
