/*
 * Interface scaling for high-resolution screens.
 *
 * The game lays out its 2D interface in pixels, so on a 2560x1600 screen everything is
 * tiny. Its renderer (d3dgl.dll) already has a virtual-resolution layer, used by the
 * original game only to shrink the interface on screens below 1024x720: GraphSys holds
 * a scale s (physical pixels per logical pixel), the interface is laid out in a logical
 * screen of physical / s, screen-space vertices and viewports are multiplied by s and
 * mouse coordinates are divided by s. Setting s > 1 enlarges the interface (and the map,
 * like a lower resolution does), while everything is still drawn at native resolution.
 *
 * The scale is set before GraphSys::init_display_quick (startup) and again inside
 * D3D11Context::change_resolution, which resets it on every resolution or window mode
 * change. Auto mode follows the Windows display scale (125 %, 150 %, ...).
 */
#include "ron.h"
#include <stdio.h>
#include <string.h>

#define VA_GRAPHSYS   0x00E335A0u /* GraphSys *graphsys (the D3D11GLSys object of d3dgl.dll) */
#define VA_INIT_XS    0x00EE1458u /* System::init_graphics statics: requested resolution */
#define VA_INIT_YS    0x00EE145Cu
#define VA_INIT_FULL  0x00EE1464u /* 1 = exclusive fullscreen (pref "Fullscreen" == 1) */

/* GraphSys fields */
#define GS_PHYS_W  0x2F8
#define GS_PHYS_H  0x2FC
#define GS_SCALE_X 0x350 /* physical pixels per logical pixel */
#define GS_SCALE_Y 0x354
#define GS_INV_X   0x358 /* 1 / scale */
#define GS_INV_Y   0x35C
#define GS_LOG_W   0x360 /* logical screen = physical / scale */
#define GS_LOG_H   0x364

/* d3dgl.dll, built 2024-06-20 */
#define D3DGL_TIMESTAMP      0x667485A4u
#define D3DGL_PREF_BASE      0x10000000u
#define D3DGL_AFTER_RESET    0x1001B7D5u /* change_resolution: mov ecx,edi / call close_for_restore */
#define D3DGL_CLOSE_RESTORE  0x1001BF30u /* D3D11Context::close_for_restore */
#define CTX_WINDOWED         0x5C        /* D3D11Context: byte, 1 = windowed or borderless */
#define D3DGL_SET_VIEWPORT   0x1001A350u /* D3D11Context::set_viewport(rect, zmin xmm2, zmax xmm3) */
#define D3DGL_CLIP_VERTS     0x1003A360u /* D3D11GLSprite::clip_verts_ordered, works on the two arrays below */
#define D3DGL_SPRITE_VERTS   0x100F8200u /* D3D11GLSprite::verts: +4 count, +0x10 data (x, y, z, w floats) */
#define D3DGL_SPRITE_TEXS    0x100F81E0u /* D3D11GLSprite::texs: +4 count, +0x10 data (u, v floats) */

#define MIN_LOG_W 1024 /* the game's own minimum logical screen */
#define MIN_LOG_H 720

int g_ui_scale_enabled;
static float g_cfg_scale; /* 0 = auto */
static int g_d3dgl_hooked;

/* The scale applied, kept so it can be switched off while drawing into smaller textures. */
static struct {
    int active;
    int phys_w, phys_h;
    float sx, sy, ix, iy;
} g_cur;

/* The scale d3dgl's drawing code uses (viewports, screen-space vertices, sprite clipping):
   scale x, scale y, 1/scale x, 1/scale y. Those instructions are redirected here from the
   GraphSys fields, so the drawing scale can be 1 while rendering into smaller textures
   while the GraphSys fields (mouse, layout) always keep the interface scale. */
_Alignas(16) float g_render_scale[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

void *g_orig_init_display_quick;
void *g_ui_close_for_restore; /* used by the asm stubs */
void *g_ui_resume;
void *g_ui_orig_clip_verts;
static char *g_sprite_verts, *g_sprite_texs;

void ui_scale_load_config(const char *ini_path)
{
    char buf[32];
    GetPrivateProfileStringA("ui", "ui_scale", "auto", buf, sizeof buf, ini_path);
    g_cfg_scale = _stricmp(buf, "auto") == 0 ? 0.0f : (float)atof(buf);
    /* ui_scale=1 means off: the game is left exactly as it is. */
    g_ui_scale_enabled = g_cfg_scale == 0.0f || g_cfg_scale > 1.0f;
}

/* Scale for a w x h game screen. `windowed`: the game pixels are the monitor's pixels;
   in exclusive fullscreen at a lower resolution the monitor already enlarges them. */
/* The interface scale in use (screen pixels per interface pixel), 1 when off. */
float ui_scale_now(void)
{
    return g_cur.active ? g_cur.sx : 1.0f;
}

static float pick_scale(int w, int h, int windowed)
{
    float s = g_cfg_scale;
    if (s <= 0.0f) {
        HDC dc = GetDC(NULL);
        int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
        if (dc)
            ReleaseDC(NULL, dc);
        s = dpi / 96.0f;
        DEVMODEA dm;
        memset(&dm, 0, sizeof dm);
        dm.dmSize = sizeof dm;
        if (!windowed && EnumDisplaySettingsA(NULL, ENUM_REGISTRY_SETTINGS, &dm) && (int)dm.dmPelsHeight > h)
            s *= (float)h / (float)dm.dmPelsHeight;
    }
    float max_x = (float)w / MIN_LOG_W, max_y = (float)h / MIN_LOG_H;
    if (s > max_x)
        s = max_x;
    if (s > max_y)
        s = max_y;
    return s;
}

/* Scale and inverse for one axis. The game derives sizes as (int)(phys * (1.0f / scale)),
   so the scale is nudged down until that truncation gives exactly `logical`. */
static void axis(int phys, int logical, float *scale, float *inv)
{
    volatile float s = (float)phys / (float)logical, i = 1.0f / s;
    for (int n = 0; n < 8 && (int)((float)phys * i) < logical; n++) {
        s = s * (1.0f - 1.0f / 8388608.0f);
        i = 1.0f / s;
    }
    *scale = s;
    *inv = i;
}

static void apply_scale(char *gs, int w, int h, int windowed, const char *when)
{
    if (!gs || w <= 0 || h <= 0)
        return;
    float s = pick_scale(w, h, windowed);
    g_cur.active = 0;
    if (s < 1.02f) { /* nothing to enlarge: leave the game's own values */
        /* and forget the last scale: a change from an enlarged resolution to one that needs
           none kept scaling every screen-sized viewport and vertex by the old factor */
        g_cur.sx = g_cur.sy = g_cur.ix = g_cur.iy = 1.0f;
        g_render_scale[0] = g_render_scale[1] = g_render_scale[2] = g_render_scale[3] = 1.0f;
        logf_("ui scale (%s): %dx%d, scale %.2f, interface not enlarged", when, w, h, s);
        return;
    }
    int lw = (int)((float)w / s + 0.5f), lh = (int)((float)h / s + 0.5f);
    float sx, sy, ix, iy;
    axis(w, lw, &sx, &ix);
    axis(h, lh, &sy, &iy);
    *(float *)(gs + GS_SCALE_X) = sx;
    *(float *)(gs + GS_SCALE_Y) = sy;
    *(float *)(gs + GS_INV_X) = ix;
    *(float *)(gs + GS_INV_Y) = iy;
    *(int *)(gs + GS_LOG_W) = lw;
    *(int *)(gs + GS_LOG_H) = lh;
    g_cur.phys_w = w;
    g_cur.phys_h = h;
    g_cur.sx = sx;
    g_cur.sy = sy;
    g_cur.ix = ix;
    g_cur.iy = iy;
    g_cur.active = 1;
    g_render_scale[0] = sx;
    g_render_scale[1] = sy;
    g_render_scale[2] = ix;
    g_render_scale[3] = iy;
    logf_("ui scale (%s): %dx%d %s, scale %.3f -> interface laid out at %dx%d", when, w, h,
          windowed ? "windowed" : "fullscreen", s, lw, lh);
}

/* Mid-function hook in d3dgl's D3D11Context::change_resolution, after it has reset the
   scale for the new resolution (ecx = d3d_sys, edi = the context). */
__attribute__((used)) void c_ui_resolution_changed(char *ctx)
{
    char *gs = *(char **)VA(VA_GRAPHSYS);
    if (gs)
        apply_scale(gs, *(int *)(gs + GS_PHYS_W), *(int *)(gs + GS_PHYS_H), *(unsigned char *)(ctx + CTX_WINDOWED),
                    "resolution change");
}

/* Interface sprites are sampled with linear filtering from texture pages whose unused
   space is magenta. At scale 1 every screen pixel lands on a texel centre; at a scale
   like 1.5 the pixels along a sprite's edge also pick up part of the texel beyond it,
   which drew thin pink lines on every seam of the tiled frames and buttons. Runs before
   D3D11GLSprite::clip_verts_ordered, when the sprite's quads (6 vertices each) are in
   the shared arrays: each quad's texture coordinates are pulled in by half a texel. */
__attribute__((used)) void c_ui_inset_uvs(void)
{
    /* Sub-pixel sprites (subpixel.c): shift the quad of the building being drawn. */
    subpixel_apply(*(float **)(g_sprite_verts + 0x10), *(int *)(g_sprite_verts + 4));

    char *gs = *(char **)VA(VA_GRAPHSYS);
    if (!gs || g_render_scale[0] < 1.02f) /* not on screen-sized targets: nothing to correct */
        return;
    int n = *(int *)(g_sprite_verts + 4);
    if (n <= 0 || n % 6 || n != *(int *)(g_sprite_texs + 4))
        return;
    float *v = *(float **)(g_sprite_verts + 0x10), *t = *(float **)(g_sprite_texs + 0x10);
    for (int q = 0; q < n; q += 6) {
        float x0 = v[q * 4], x1 = x0, y0 = v[q * 4 + 1], y1 = y0;
        float u0 = t[q * 2], u1 = u0, w0 = t[q * 2 + 1], w1 = w0;
        for (int i = q + 1; i < q + 6; i++) {
            x0 = v[i * 4] < x0 ? v[i * 4] : x0;
            x1 = v[i * 4] > x1 ? v[i * 4] : x1;
            y0 = v[i * 4 + 1] < y0 ? v[i * 4 + 1] : y0;
            y1 = v[i * 4 + 1] > y1 ? v[i * 4 + 1] : y1;
            u0 = t[i * 2] < u0 ? t[i * 2] : u0;
            u1 = t[i * 2] > u1 ? t[i * 2] : u1;
            w0 = t[i * 2 + 1] < w0 ? t[i * 2 + 1] : w0;
            w1 = t[i * 2 + 1] > w1 ? t[i * 2 + 1] : w1;
        }
        if (x1 - x0 < 1.0f || y1 - y0 < 1.0f)
            continue;
        /* Half a texel, for sprites drawn one texel per logical pixel (the interface). */
        float du = 0.5f * (u1 - u0) / (x1 - x0), dv = 0.5f * (w1 - w0) / (y1 - y0);
        for (int i = q; i < q + 6; i++) {
            float *u = &t[i * 2], *w = &t[i * 2 + 1];
            if (*u == u0)
                *u += du;
            else if (*u == u1)
                *u -= du;
            if (*w == w0)
                *w += dv;
            else if (*w == w1)
                *w -= dv;
        }
    }
}

/* The same for the interface atlas (frames, buttons, bars), whose quads are built by
   IFaceRenderManager::add_smooth in the exe with u = x0 / W .. (x0 + w) / W, i.e. on the
   outer edge of the piece. Mid-function hook right after those four values are computed
   (frame: [ebp-0x34] u0, [ebp-0x28] u1, [ebp-0x30] v0, [ebp-0x24] v1, [ebp-0x20] w and
   [ebp-0x1c] h in texels); the stub reloads the two passed in registers afterwards. */
void *g_orig_iface_uvs;
void *g_ui_iface_resume;

__attribute__((used)) void c_ui_iface_uvs(char *frame)
{
    g_ui_iface_resume = (char *)g_orig_iface_uvs + 8; /* trampoline: stolen 8 bytes, then jmp back */
    if (!g_cur.active) /* interface quads are built for the screen: the interface scale */
        return;
    float *u0 = (float *)(frame - 0x34), *u1 = (float *)(frame - 0x28);
    float *v0 = (float *)(frame - 0x30), *v1 = (float *)(frame - 0x24);
    int w = *(int *)(frame - 0x20), h = *(int *)(frame - 0x1C);
    if (w >= 1 && w < 4096) {
        float d = 0.5f * (*u1 - *u0) / (float)w;
        *u0 += d;
        *u1 -= d;
    }
    if (h >= 1 && h < 4096) {
        float d = 0.5f * (*v1 - *v0) / (float)h;
        *v0 += d;
        *v1 -= d;
    }
}

/* The renderer scales every viewport and screen-space vertex, whatever it draws into.
   Besides the screen (and the HDR buffer of the same size) the game renders into smaller
   textures: terrain texture cache tiles (512x512), the minimap, half-size bloom buffers.
   Scaled, those came out magnified and cropped (black or garbled terrain, glows in the
   wrong place). So the scale is on only while the bound target is screen-sized:
    - pre-hook on D3D11Context::set_viewport: decide for the target bound right now;
    - the context's OMSetRenderTargets: when a pass switches to a target of the other
      kind without setting a viewport again (the camera sets its viewport while a small
      texture is still bound, then the scene is drawn into the full-size HDR buffer, or
      the bloom and minimap passes set a viewport the size of their texture while the
      screen is still bound), the last viewport is set again with the right scale, straight
      through RSSetViewports: d3dgl's set_viewport skips an unchanged (e.g. NULL) rect. */
typedef void(__stdcall *om_get_rt_t)(void *dc, UINT n, void **rtv, void **dsv);
typedef void(__stdcall *om_set_rt_t)(void *dc, UINT n, void *const *rtvs, void *dsv);
typedef void(__stdcall *get_resource_t)(void *view, void **res);
typedef void(__stdcall *tex_desc_t)(void *tex, UINT *desc);
typedef ULONG(__stdcall *release_t)(void *obj);
typedef void(__stdcall *rs_set_vp_t)(void *dc, UINT n, const float *vps);
typedef void(__stdcall *rs_get_vp_t)(void *dc, UINT *n, float *vps);
#define COM_FN(obj, idx) ((*(void ***)(obj))[idx])
#define DC_OM_SET_RT 33 /* ID3D11DeviceContext vtable */
#define DC_OM_GET_RT 89
#define DC_RS_SET_VP 44
#define DC_RS_GET_VP 95
#define CTX_DEVICE_CONTEXT 0x14 /* D3D11Context: ID3D11DeviceContext* */

void *g_ui_orig_set_viewport;

static UINT_PTR g_d3dgl_delta;
static om_set_rt_t g_real_om_set;
static rs_set_vp_t g_real_rs_set_vp;
static int g_trace; /* events left to log; F11 starts a trace of the render target / viewport sequence */
static struct {
    void *ctx;
    int has_rect;
    short rect[4];
    int full; /* the scale the current viewport was set with */
} g_vp;

/* Is the bound target screen-sized? Size in *w, *h (0 when unknown). A pass with no
   colour target and only a depth buffer (a shadow map) is judged by the depth buffer:
   taking it for the screen scaled its viewport, and the shadows came out stretched into
   long dark streaks across the ground (2026-10-24). */
static int target_full(void *rtv, void *dsv, UINT *w, UINT *h)
{
    *w = *h = 0;
    void *view = rtv ? rtv : dsv;
    if (!view)
        return 1;
    void *res = NULL;
    ((get_resource_t)COM_FN(view, 7))(view, &res); /* ID3D11View::GetResource */
    if (!res)
        return 1;
    UINT desc[11] = { 0 }; /* D3D11_TEXTURE2D_DESC: Width, Height, ... */
    ((tex_desc_t)COM_FN(res, 10))(res, desc); /* ID3D11Texture2D::GetDesc */
    ((release_t)COM_FN(res, 2))(res);
    *w = desc[0];
    *h = desc[1];
    return (int)desc[0] == g_cur.phys_w && (int)desc[1] == g_cur.phys_h;
}

static void set_scale(int full)
{
    full = full && g_cur.active;
    g_render_scale[0] = full ? g_cur.sx : 1.0f;
    g_render_scale[1] = full ? g_cur.sy : 1.0f;
    g_render_scale[2] = full ? g_cur.ix : 1.0f;
    g_render_scale[3] = full ? g_cur.iy : 1.0f;
}

/* Diagnostic: each distinct (target size, decision, rect, caller) once, up to 300 lines. */
static void log_viewport(const char *what, int full, UINT w, UINT h, const short *rect, UINT_PTR caller)
{
    if (!g_debug_viewports) /* [debug] viewport_log: up to 300 lines a session otherwise */
        return;
    static struct {
        UINT w, h, caller;
        int full;
        short r[4];
    } seen[300];
    static int n;
    short r[4] = { -1, -1, -1, -1 };
    if (rect)
        memcpy(r, rect, sizeof r);
    for (int i = 0; i < n; i++)
        if (seen[i].w == w && seen[i].h == h && seen[i].caller == caller && seen[i].full == full &&
            !memcmp(seen[i].r, r, sizeof r))
            return;
    if (n >= 300)
        return;
    seen[n].w = w, seen[n].h = h, seen[n].caller = caller, seen[n].full = full;
    memcpy(seen[n].r, r, sizeof r);
    n++;
    UINT_PTR exe = (UINT_PTR)GetModuleHandleA(NULL);
    UINT_PTR dll = (UINT_PTR)GetModuleHandleA("d3dgl.dll");
    UINT_PTR rva = !caller ? 0
                   : caller >= dll && caller < dll + 0x200000 ? caller - g_d3dgl_delta
                                                              : caller - exe + 0x400000;
    logf_("%s: target %ux%u -> scale %s, rect %d-%d x %d-%d, from %08lX", what, w, h, full ? "on" : "off", r[0],
          r[1], r[2], r[3], (unsigned long)rva);
}

/* ID3D11DeviceContext::RSSetViewports: only logs the final values while tracing. */
static void __stdcall rs_set_vp_hook(void *dc, UINT n, const float *vps)
{
    if (g_trace > 0 && n && vps) {
        g_trace--;
        logf_("trace:   RSSetViewports %.0f,%.0f %.0fx%.0f", vps[0], vps[1], vps[2], vps[3]);
    }
    g_real_rs_set_vp(dc, n, vps);
}

/* ID3D11DeviceContext::OMSetRenderTargets, patched into the context's vtable. */
static void __stdcall om_set_hook(void *dc, UINT n, void *const *rtvs, void *dsv)
{
    g_real_om_set(dc, n, rtvs, dsv);
    if (!g_trace && (GetAsyncKeyState(VK_F11) & 0x8000)) {
        g_trace = 400;
        logf_("trace: render targets and viewports, next %d events", g_trace);
    }
    if (!g_cur.active || !g_vp.ctx)
        return;
    UINT w, h;
    int full = target_full(n && rtvs ? rtvs[0] : NULL, dsv, &w, &h);
    if (g_trace > 0) {
        g_trace--;
        logf_("trace: OMSetRenderTargets n=%u target %ux%u (%s)", n, w, h, full ? "full" : "small");
    }
    if (full == g_vp.full)
        return;
    set_scale(full);
    /* The last viewport was meant for this target when it is the target's whole area: the
       logical screen for a screen-sized target (the camera's viewport, set while a small
       texture was still bound), the texture's size for a small one (bloom and minimap set
       theirs while the screen is still bound). Other viewports are stale (they belonged
       to the previous target): the game sets its own after binding. */
    char *gs = *(char **)VA(VA_GRAPHSYS);
    int whole_screen = !g_vp.has_rect || (gs && g_vp.rect[0] == 0 && g_vp.rect[2] == 0 &&
                                          g_vp.rect[1] == *(int *)(gs + GS_LOG_W) &&
                                          g_vp.rect[3] == *(int *)(gs + GS_LOG_H));
    int whole_texture = g_vp.has_rect && w && g_vp.rect[0] == 0 && g_vp.rect[2] == 0 &&
                        g_vp.rect[1] == (short)w && g_vp.rect[3] == (short)h;
    if (full ? !whole_screen : !whole_texture) {
        g_vp.full = full;
        return;
    }
    g_vp.full = full;
    float vp[6] = { 0 }; /* D3D11_VIEWPORT: x, y, width, height, min depth, max depth */
    UINT nvp = 1;
    ((rs_get_vp_t)COM_FN(dc, DC_RS_GET_VP))(dc, &nvp, vp); /* keeps the game's depth range */
    if (!nvp)
        vp[4] = 0.0f, vp[5] = 1.0f;
    if (!full) {
        vp[0] = vp[1] = 0.0f;
        vp[2] = (float)w;
        vp[3] = (float)h;
    } else if (g_vp.has_rect) {
        vp[0] = g_vp.rect[0] * g_cur.sx;
        vp[1] = g_vp.rect[2] * g_cur.sy;
        vp[2] = (g_vp.rect[1] - g_vp.rect[0]) * g_cur.sx;
        vp[3] = (g_vp.rect[3] - g_vp.rect[2]) * g_cur.sy;
    } else {
        vp[0] = vp[1] = 0.0f;
        vp[2] = (float)g_cur.phys_w;
        vp[3] = (float)g_cur.phys_h;
    }
    log_viewport("viewport again", full, w, h, g_vp.has_rect ? g_vp.rect : NULL, 0);
    if (g_trace > 0) {
        g_trace--;
        logf_("trace: viewport set again for this target");
    }
    rs_set_vp_hook(dc, 1, vp);
}

static void hook_om_set(void *dc)
{
    void **vt = *(void ***)dc;
    if (vt[DC_OM_SET_RT] == (void *)om_set_hook)
        return;
    g_real_rs_set_vp = (rs_set_vp_t)vt[DC_RS_SET_VP];
    DWORD old0;
    if (VirtualProtect(&vt[DC_RS_SET_VP], sizeof(void *), PAGE_READWRITE, &old0)) {
        vt[DC_RS_SET_VP] = (void *)rs_set_vp_hook;
        VirtualProtect(&vt[DC_RS_SET_VP], sizeof(void *), old0, &old0);
    }
    g_real_om_set = (om_set_rt_t)vt[DC_OM_SET_RT];
    DWORD old;
    if (VirtualProtect(&vt[DC_OM_SET_RT], sizeof(void *), PAGE_READWRITE, &old)) {
        vt[DC_OM_SET_RT] = (void *)om_set_hook;
        VirtualProtect(&vt[DC_OM_SET_RT], sizeof(void *), old, &old);
        logf_("hook OMSetRenderTargets installed");
    }
}

__attribute__((used)) void c_ui_set_viewport(char *ctx, const short *rect, UINT_PTR caller, float zmin, float zmax)
{
    if (!g_cur.active)
        return;
    void *dc = *(void **)(ctx + CTX_DEVICE_CONTEXT);
    int full = 1;
    UINT tw = 0, th = 0;
    if (dc) {
        hook_om_set(dc);
        void *rtv = NULL, *dsv = NULL;
        ((om_get_rt_t)COM_FN(dc, DC_OM_GET_RT))(dc, 1, &rtv, &dsv);
        full = target_full(rtv, dsv, &tw, &th);
        if (dsv)
            ((release_t)COM_FN(dsv, 2))(dsv);
        if (rtv)
            ((release_t)COM_FN(rtv, 2))(rtv);
    }
    log_viewport("viewport", full, tw, th, rect, caller);
    if (g_trace > 0) {
        g_trace--;
        short r[4] = { -1, -1, -1, -1 };
        if (rect)
            memcpy(r, rect, sizeof r);
        logf_("trace: set_viewport %d-%d x %d-%d, bound %ux%u -> scale %s, from %08lX", r[0], r[1], r[2], r[3], tw,
              th, full ? "on" : "off",
              (unsigned long)(caller >= (UINT_PTR)GetModuleHandleA("d3dgl.dll") ? caller - g_d3dgl_delta : caller));
    }
    (void)zmin, (void)zmax;
    g_vp.ctx = ctx;
    g_vp.full = full;
    g_vp.has_rect = rect != NULL;
    if (rect)
        memcpy(g_vp.rect, rect, sizeof g_vp.rect);
    set_scale(full);
}

void hk_ui_resolution_changed(void);
void hk_ui_clip_verts(void);
void hk_ui_set_viewport(void);

static void write_jmp5(unsigned char *at, const void *to)
{
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    at[0] = 0xE9;
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)to - ((UINT_PTR)at + 5));
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
}

static void hook_d3dgl(void)
{
    g_d3dgl_hooked = 1;
    unsigned char *dll = (unsigned char *)GetModuleHandleA("d3dgl.dll");
    if (!dll) {
        logf_("ui scale: d3dgl.dll not loaded, resolution changes keep the game's scale");
        return;
    }
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(dll + ((IMAGE_DOS_HEADER *)dll)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != D3DGL_TIMESTAMP) {
        logf_("ui scale: unknown d3dgl.dll (timestamp %08lX), resolution changes keep the game's scale",
              nt->FileHeader.TimeDateStamp);
        return;
    }
    UINT_PTR delta = (UINT_PTR)dll - D3DGL_PREF_BASE;
    g_d3dgl_delta = delta;
    unsigned char *tramp = VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

    /* The interface-scale hooks; the clip_verts_ordered hook below is shared with the
       sub-pixel sprites (subpixel.c) and is installed for either feature. */
    if (g_ui_scale_enabled) {
    unsigned char *at = (unsigned char *)(D3DGL_AFTER_RESET + delta);
    /* mov ecx,edi / call rel32 close_for_restore */
    INT32 rel = (INT32)(D3DGL_CLOSE_RESTORE - (D3DGL_AFTER_RESET + 7));
    unsigned char sig[7] = { 0x8B, 0xCF, 0xE8 };
    memcpy(sig + 3, &rel, 4);
    if (memcmp(at, sig, sizeof sig) == 0) {
        g_ui_close_for_restore = (void *)(D3DGL_CLOSE_RESTORE + delta);
        g_ui_resume = at + 7;
        write_jmp5(at, hk_ui_resolution_changed);
        logf_("hook change_resolution (d3dgl) installed");
    } else
        logf_("ui scale: signature mismatch at change_resolution (d3dgl), resolution changes keep the game's scale");

    /* D3D11GLGraphicCard::add_clear_rect scales its rectangle to physical pixels, then
       draws it through the vertex stream, which scales every screen vertex again: a
       window's clear rectangle (drawn behind e.g. the resource panel) came out 2.25x
       its size, as black areas around the panel. Drop its eight `mulss xmmN,[reg+0x350/
       0x354]`; with no rectangle it uses the viewport, which still covers the screen. */
    static const UINT_PTR clear_mul[8] = { 0x1002C44E, 0x1002C470, 0x1002C4A5, 0x1002C4B2,
                                           0x1002C4DA, 0x1002C505, 0x1002C528, 0x1002C53A };
    int clear_ok = 1;
    for (int i = 0; i < 8; i++) {
        unsigned char *p = (unsigned char *)(clear_mul[i] + delta);
        clear_ok &= p[0] == 0xF3 && p[1] == 0x0F && p[2] == 0x59 && (p[4] == 0x50 || p[4] == 0x54) && p[5] == 0x03 &&
                    p[6] == 0 && p[7] == 0;
    }
    if (clear_ok) {
        static const unsigned char nop8[8] = { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
        for (int i = 0; i < 8; i++) {
            unsigned char *p = (unsigned char *)(clear_mul[i] + delta);
            DWORD old;
            VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old);
            memcpy(p, nop8, 8);
            VirtualProtect(p, 8, old, &old);
        }
        FlushInstructionCache(GetCurrentProcess(), NULL, 0);
        logf_("patch add_clear_rect scale (d3dgl) applied");
    } else
        logf_("ui scale: bytes differ in add_clear_rect (d3dgl), patch skipped");

    /* Drawing reads of the scale (`movss/mulss xmmN, [reg+0x350..0x35c]`, 8 bytes): the
       modrm becomes an absolute address (same length) into g_render_scale. */
    static const UINT_PTR draw_reads[] = {
        0x1001A465, 0x1001A47A,                         /* D3D11Context::set_viewport */
        0x1003A389, 0x1003A3C3,                         /* D3D11GLSprite::clip_verts_ordered */
        0x10047283, 0x1004729D, 0x100473A3, 0x100473C5, /* D3D11VertStream::copy (...) */
        0x100474CD, 0x100474EF, 0x10047795, 0x100477B7,
        0x100478BB, 0x100478D5,
        0x10048570, 0x1004858A,                         /* copy_vert_offset */
        0x10048767, 0x10048779,                         /* copy(Vert4Array *, int, int) */
    };
    int reads_ok = 1, nreads = (int)(sizeof draw_reads / sizeof draw_reads[0]);
    for (int i = 0; i < nreads; i++) {
        unsigned char *p = (unsigned char *)(draw_reads[i] + delta);
        UINT32 disp = *(UINT32 *)(p + 4);
        reads_ok &= p[0] == 0xF3 && p[1] == 0x0F && (p[2] == 0x59 || p[2] == 0x10) && (p[3] & 0xC0) == 0x80 &&
                    (p[3] & 7) != 4 && disp >= 0x350 && disp <= 0x35C && (disp & 3) == 0;
    }
    if (reads_ok) {
        for (int i = 0; i < nreads; i++) {
            unsigned char *p = (unsigned char *)(draw_reads[i] + delta);
            UINT32 disp = *(UINT32 *)(p + 4);
            DWORD old;
            VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old);
            p[3] = (unsigned char)((p[3] & 0x38) | 0x05); /* [disp32] */
            *(UINT32 *)(p + 4) = (UINT32)(UINT_PTR)&g_render_scale[(disp - 0x350) / 4];
            VirtualProtect(p, 8, old, &old);
        }
        FlushInstructionCache(GetCurrentProcess(), NULL, 0);
        logf_("patch drawing scale reads (d3dgl) applied");
    } else
        logf_("ui scale: bytes differ in the drawing scale reads (d3dgl), offscreen fix off");

    /* set_viewport: push ebp / mov ebp,esp / and esp,-16 (stolen) | sub esp,0x68 */
    static const unsigned char vp_sig[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x68 };
    unsigned char *vp = (unsigned char *)(D3DGL_SET_VIEWPORT + delta);
    if (reads_ok && tramp && memcmp(vp, vp_sig, sizeof vp_sig) == 0) {
        unsigned char *t = tramp + 32;
        memcpy(t, vp, 6);
        t[6] = 0xE9;
        *(INT32 *)(t + 7) = (INT32)((UINT_PTR)(vp + 6) - ((UINT_PTR)t + 11));
        g_ui_orig_set_viewport = t;
        write_jmp5(vp, hk_ui_set_viewport);
        logf_("hook set_viewport (d3dgl) installed");
    } else
        logf_("ui scale: signature mismatch at set_viewport (d3dgl), offscreen textures stay scaled");
    }

    /* clip_verts_ordered: push ebx / mov ebx,esp / sub esp,8 (stolen) | and esp,-8 */
    static const unsigned char clip_sig[] = { 0x53, 0x8B, 0xDC, 0x83, 0xEC, 0x08, 0x83, 0xE4, 0xF8 };
    unsigned char *clip = (unsigned char *)(D3DGL_CLIP_VERTS + delta);
    if (tramp && memcmp(clip, clip_sig, sizeof clip_sig) == 0) {
        g_sprite_verts = (char *)(D3DGL_SPRITE_VERTS + delta);
        g_sprite_texs = (char *)(D3DGL_SPRITE_TEXS + delta);
        memcpy(tramp, clip, 6);
        tramp[6] = 0xE9;
        *(INT32 *)(tramp + 7) = (INT32)((UINT_PTR)(clip + 6) - ((UINT_PTR)tramp + 11));
        g_ui_orig_clip_verts = tramp;
        write_jmp5(clip, hk_ui_clip_verts);
        logf_("hook clip_verts_ordered (d3dgl) installed");
    } else
        logf_("ui scale: signature mismatch at clip_verts_ordered (d3dgl), sprite edges not corrected");
}

/* Pre-hook on GraphSys::init_display_quick: the display is about to be created at the
   resolution from the options. d3dgl derives the logical size from the scale set here
   (and sizes the screen buffer from the inverse before refreshing it, so both are set). */
__attribute__((used)) void c_init_display_quick(void)
{
    postfx_display();
    if (!g_d3dgl_hooked)
        hook_d3dgl();
    if (g_ui_scale_enabled)
        apply_scale(*(char **)VA(VA_GRAPHSYS), *(int *)VA(VA_INIT_XS), *(int *)VA(VA_INIT_YS),
                    *(int *)VA(VA_INIT_FULL) != 1, "startup");
}
