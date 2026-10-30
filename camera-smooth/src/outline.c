/*
 * Hover outline: the 3D model of the unit under the cursor (every soldier of a hovered squad)
 * gets a solid outline in the relation colour: white yours, yellow allies, red everyone else.
 *
 * How the game draws a unit (camera-smooth/ENGINE_NOTES.md "Hover outline"): GuyOut::draw ->
 * GraphicPieces::draw -> GraphicPieces::draw_unit (one call, 0x0090B383) -> the land unit RMap,
 * whose AddPrimMethod adds the skinned mesh to the card and flushes it at once; d3dgl's
 * D3D11GLSys::render binds its states and calls ID3D11DeviceContext::DrawIndexed / Draw. So every
 * D3D11 draw of one model happens inside that draw_unit call.
 *
 * That call is pointed at c_ol_draw_unit. For a hovered model it flushes what the card holds,
 * raises g_outline, runs the game's draw_unit and flushes again. DrawIndexed and Draw (patched in
 * the context's vtable, as ui_scale.c does with OMSetRenderTargets) see g_outline and first draw
 * the same primitives 8 times with the viewport moved by `hover_outline_px` pixels around, with
 * our pixel shader (a constant colour, no inputs, so it fits every vertex shader of the game) and
 * a copy of the current depth state without depth writes. The game's own draw then covers the
 * middle, and what stays visible is a ring around the silhouette. Terrain and buildings in front
 * still hide it (depth test). Shaders, constant buffer slot 7 (free in d3dgl), depth state and
 * viewports are restored after the copies, exactly as they were.
 *
 * Drawing only, so it is multiplayer-safe.
 */
#include "ron.h"
#include <string.h>

#define VA_DRAW_UNIT_CALL 0x0090B383u /* GraphicPieces::draw: call GraphicPieces::draw_unit */
#define VA_DRAW_UNIT      0x008E7DA0u /* GraphicPieces::draw_unit(GraphicChad &, RData *, int), ret 0xC */
#define VA_GRAPHSYS       0x00E335A0u /* GraphSys *: d3dgl's D3D11GLSys, +0x47C the immediate context */
#define VA_CARD_PTR       0x00C0AEACu /* D3D11GLGraphicCard *: vtable +0x18 flush */

#define SYS_CONTEXT  0x47C
#define CHAD_PLAYER  0x08 /* signed byte */
#define CHAD_GHOST   0x09
#define CHAD_ID      0x0A /* int16 */
#define CHAD_ALPHA   0x71 /* 0xFF opaque; lower for cloaked units */

#define COM_FN(obj, idx) ((*(void ***)(obj))[idx])
/* ID3D11DeviceContext */
#define DC_GET_DEVICE    3
#define DC_PS_SET_SHADER 9
#define DC_DRAW_INDEXED  12
#define DC_DRAW          13
#define DC_PS_SET_CB     16
#define DC_OM_SET_DS     36
#define DC_RS_SET_VP     44
#define DC_UPDATE_SUB    48
#define DC_PS_GET_SHADER 74
#define DC_PS_GET_CB     77
#define DC_OM_GET_RT     89
#define DC_OM_GET_DS     92
#define DC_RS_GET_VP     95
/* ID3D11Device */
#define DEV_CREATE_BUFFER 3
#define DEV_CREATE_PS     15
#define DEV_CREATE_DS     21
/* ID3D11DepthStencilState */
#define DS_GET_DESC 7
/* ID3DBlob */
#define BLOB_PTR  3
#define BLOB_SIZE 4

typedef struct {
    float x, y, w, h, zmin, zmax;
} viewport_t;

typedef struct {
    int depth_enable, depth_write_mask, depth_func, stencil_enable;
    unsigned char read_mask, write_mask, pad[2];
    int front[4], back[4];
} ds_desc_t;

typedef unsigned long(__stdcall *release_t)(void *self);
typedef void(__stdcall *draw_indexed_t)(void *dc, UINT count, UINT start, INT base);
typedef void(__stdcall *draw_t)(void *dc, UINT count, UINT start);
typedef void(__stdcall *get_device_t)(void *dc, void **dev);
typedef void(__stdcall *ps_set_shader_t)(void *dc, void *ps, void **inst, UINT n);
typedef void(__stdcall *ps_get_shader_t)(void *dc, void **ps, void **inst, UINT *n);
typedef void(__stdcall *cb_t)(void *dc, UINT slot, UINT n, void **bufs);
typedef void(__stdcall *om_set_ds_t)(void *dc, void *ds, UINT ref);
typedef void(__stdcall *om_get_ds_t)(void *dc, void **ds, UINT *ref);
typedef void(__stdcall *om_get_rt_t)(void *dc, UINT n, void **rtv, void **dsv);
typedef void(__stdcall *rs_set_vp_t)(void *dc, UINT n, const viewport_t *vp);
typedef void(__stdcall *rs_get_vp_t)(void *dc, UINT *n, viewport_t *vp);
typedef void(__stdcall *update_sub_t)(void *dc, void *res, UINT sub, const void *box, const void *data, UINT pitch, UINT depth);
typedef long(__stdcall *create_buffer_t)(void *dev, const void *desc, const void *init, void **buf);
typedef long(__stdcall *create_ps_t)(void *dev, const void *code, SIZE_T len, void *linkage, void **ps);
typedef long(__stdcall *create_ds_t)(void *dev, const ds_desc_t *desc, void **ds);
typedef void(__stdcall *ds_get_desc_t)(void *ds, ds_desc_t *desc);
typedef void *(__stdcall *blob_ptr_t)(void *blob);
typedef SIZE_T(__stdcall *blob_size_t)(void *blob);
typedef long(__stdcall *d3dcompile_t)(const void *src, SIZE_T len, const char *name, const void *defines, void *include,
                                      const char *entry, const char *target, UINT f1, UINT f2, void **code, void **err);
typedef int(__attribute__((thiscall)) *draw_unit_t)(void *gp, char *chad, void *rdata, int a);
typedef int(__attribute__((thiscall)) *flush_t)(void *card);

int g_outline_enabled;
static float g_px = 2.0f, g_bright = 1.5f;
static int g_failed;      /* the shader or a state could not be made: no outline */
static int g_outline;     /* inside the draw_unit of a hovered model */
static float g_want[4];   /* the colour for it */
static float g_have[4];   /* the colour in our constant buffer */
static void *g_dev, *g_ps, *g_cb;
static struct {
    void *src, *ours;
} g_ds[16];
static int g_nds;
static draw_indexed_t g_real_di;
static draw_t g_real_draw;

static const char k_ps[] = "cbuffer Outline : register(b7) { float4 gOutline; };\n"
                           "float4 main() : SV_Target { return gOutline; }\n";

void outline_load_config(const char *ini_path)
{
    g_outline_enabled = GetPrivateProfileIntA("qol", "hover_outline", 1, ini_path);
    g_px = ini_float(ini_path, "qol", "hover_outline_px", 2.0f);
    g_bright = ini_float(ini_path, "qol", "hover_outline_brightness", 1.5f);
    if (g_px < 0.5f || g_px > 8.0f)
        g_px = 2.0f;
}

static void release(void *p)
{
    if (p)
        ((release_t)COM_FN(p, 2))(p);
}

static void drop_resources(void)
{
    release(g_ps);
    release(g_cb);
    for (int i = 0; i < g_nds; i++)
        release(g_ds[i].ours);
    g_ps = g_cb = NULL;
    g_nds = 0;
    g_have[3] = -1.0f;
}

static int make_resources(void *dev)
{
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
    d3dcompile_t compile = dc ? (d3dcompile_t)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!compile) {
        logf_("outline: d3dcompiler_47.dll not found, turned off");
        return 0;
    }
    void *code = NULL, *err = NULL;
    long hr = compile(k_ps, sizeof k_ps - 1, "outline", NULL, NULL, "main", "ps_4_0", 0, 0, &code, &err);
    if (hr < 0 || !code) {
        logf_("outline: shader did not compile (%08lX): %s", (unsigned long)hr,
              err ? (const char *)((blob_ptr_t)COM_FN(err, BLOB_PTR))(err) : "");
        release(err);
        release(code);
        return 0;
    }
    release(err);
    hr = ((create_ps_t)COM_FN(dev, DEV_CREATE_PS))(dev, ((blob_ptr_t)COM_FN(code, BLOB_PTR))(code),
                                                   ((blob_size_t)COM_FN(code, BLOB_SIZE))(code), NULL, &g_ps);
    release(code);
    /* D3D11_BUFFER_DESC: 16 bytes, DEFAULT, CONSTANT_BUFFER */
    UINT bd[6] = { 16, 0, 4, 0, 0, 0 };
    long hr2 = ((create_buffer_t)COM_FN(dev, DEV_CREATE_BUFFER))(dev, bd, NULL, &g_cb);
    if (hr < 0 || hr2 < 0 || !g_ps || !g_cb) {
        logf_("outline: CreatePixelShader %08lX, CreateBuffer %08lX, turned off", (unsigned long)hr, (unsigned long)hr2);
        drop_resources();
        return 0;
    }
    logf_("outline: shader and buffer ready");
    return 1;
}

/* the current depth state with depth writes off (NULL = D3D11's default state) */
static void *ds_for(void *ds)
{
    for (int i = 0; i < g_nds; i++)
        if (g_ds[i].src == ds)
            return g_ds[i].ours;
    ds_desc_t d;
    if (ds) {
        ((ds_get_desc_t)COM_FN(ds, DS_GET_DESC))(ds, &d);
    } else {
        static const ds_desc_t def = { 1, 1, 2, 0, 0xFF, 0xFF, { 0, 0 }, { 1, 1, 1, 8 }, { 1, 1, 1, 8 } };
        d = def;
    }
    d.depth_write_mask = 0;
    void *ours = NULL;
    if (((create_ds_t)COM_FN(g_dev, DEV_CREATE_DS))(g_dev, &d, &ours) < 0 || !ours)
        return NULL;
    if (g_nds == (int)(sizeof g_ds / sizeof g_ds[0])) {
        for (int i = 0; i < g_nds; i++)
            release(g_ds[i].ours);
        g_nds = 0;
    }
    g_ds[g_nds].src = ds;
    g_ds[g_nds].ours = ours;
    g_nds++;
    return ours;
}

/* the 8 copies, before the game's own draw of the same primitives */
static void outline_copies(void *dc, int indexed, UINT count, UINT start, INT base)
{
    void *rtv = NULL, *dsv = NULL;
    ((om_get_rt_t)COM_FN(dc, DC_OM_GET_RT))(dc, 1, &rtv, &dsv);
    release(dsv);
    if (!rtv)
        return;
    release(rtv);
    viewport_t vp[16], moved[16];
    UINT n = 16;
    ((rs_get_vp_t)COM_FN(dc, DC_RS_GET_VP))(dc, &n, vp);
    if (!n || n > 16)
        return;

    void *ps = NULL, *cb = NULL, *ds = NULL;
    UINT ref = 0;
    ((ps_get_shader_t)COM_FN(dc, DC_PS_GET_SHADER))(dc, &ps, NULL, NULL);
    ((cb_t)COM_FN(dc, DC_PS_GET_CB))(dc, 7, 1, &cb);
    ((om_get_ds_t)COM_FN(dc, DC_OM_GET_DS))(dc, &ds, &ref);
    void *mine = ds_for(ds);
    if (mine) {
        if (memcmp(g_have, g_want, sizeof g_want)) {
            ((update_sub_t)COM_FN(dc, DC_UPDATE_SUB))(dc, g_cb, 0, NULL, g_want, 0, 0);
            memcpy(g_have, g_want, sizeof g_want);
        }
        ((ps_set_shader_t)COM_FN(dc, DC_PS_SET_SHADER))(dc, g_ps, NULL, 0);
        ((cb_t)COM_FN(dc, DC_PS_SET_CB))(dc, 7, 1, &g_cb);
        ((om_set_ds_t)COM_FN(dc, DC_OM_SET_DS))(dc, mine, ref);
        static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
                                          { 0.7071f, 0.7071f }, { -0.7071f, 0.7071f },
                                          { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
        for (int k = 0; k < 8; k++) {
            memcpy(moved, vp, n * sizeof vp[0]);
            moved[0].x += dirs[k][0] * g_px;
            moved[0].y += dirs[k][1] * g_px;
            ((rs_set_vp_t)COM_FN(dc, DC_RS_SET_VP))(dc, n, moved);
            if (indexed)
                g_real_di(dc, count, start, base);
            else
                g_real_draw(dc, count, start);
        }
        ((rs_set_vp_t)COM_FN(dc, DC_RS_SET_VP))(dc, n, vp);
        ((ps_set_shader_t)COM_FN(dc, DC_PS_SET_SHADER))(dc, ps, NULL, 0);
        ((cb_t)COM_FN(dc, DC_PS_SET_CB))(dc, 7, 1, &cb);
        ((om_set_ds_t)COM_FN(dc, DC_OM_SET_DS))(dc, ds, ref);
    }
    release(ps);
    release(cb);
    release(ds);
}

static void __stdcall di_hook(void *dc, UINT count, UINT start, INT base)
{
    if (g_outline)
        outline_copies(dc, 1, count, start, base);
    g_real_di(dc, count, start, base);
}

static void __stdcall draw_hook(void *dc, UINT count, UINT start)
{
    if (g_outline)
        outline_copies(dc, 0, count, start, 0);
    g_real_draw(dc, count, start);
}

static void patch_slot(void **vt, int idx, void *fn)
{
    DWORD old;
    if (VirtualProtect(&vt[idx], sizeof(void *), PAGE_READWRITE, &old)) {
        vt[idx] = fn;
        VirtualProtect(&vt[idx], sizeof(void *), old, &old);
    }
}

/* the context's draw functions are ours and our resources belong to its device */
static int ready(void)
{
    char *sys = *(char **)VA(VA_GRAPHSYS);
    void *dc = sys ? *(void **)(sys + SYS_CONTEXT) : NULL;
    if (!dc)
        return 0;
    void **vt = *(void ***)dc;
    if (vt[DC_DRAW_INDEXED] != (void *)di_hook) {
        g_real_di = (draw_indexed_t)vt[DC_DRAW_INDEXED];
        patch_slot(vt, DC_DRAW_INDEXED, (void *)di_hook);
    }
    if (vt[DC_DRAW] != (void *)draw_hook) {
        g_real_draw = (draw_t)vt[DC_DRAW];
        patch_slot(vt, DC_DRAW, (void *)draw_hook);
    }
    if (vt[DC_DRAW_INDEXED] != (void *)di_hook || vt[DC_DRAW] != (void *)draw_hook) {
        logf_("outline: could not patch the context, turned off");
        g_failed = 1;
        return 0;
    }
    void *dev = NULL;
    ((get_device_t)COM_FN(dc, DC_GET_DEVICE))(dc, &dev);
    release(dev); /* the context keeps it alive */
    if (dev != g_dev) {
        drop_resources();
        g_dev = dev;
        if (!dev || !make_resources(dev)) {
            g_failed = 1;
            return 0;
        }
    }
    return 1;
}

static void flush_card(void)
{
    void *card = *(void **)VA(VA_CARD_PTR);
    if (card)
        ((flush_t)COM_FN(card, 6))(card);
}

static int __attribute__((thiscall)) c_ol_draw_unit(void *gp, char *chad, void *rdata, int a)
{
    int on = 0;
    if (!g_failed && (unsigned char)chad[CHAD_ALPHA] == 0xFF && !chad[CHAD_GHOST]) {
        int player = (signed char)chad[CHAD_PLAYER], id = *(short *)(chad + CHAD_ID);
        on = player >= 0 && id >= 0 && squad_hovered(id, player) && ready();
        if (on) {
            static const float rgb[3][3] = { { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.8f, 0.1f }, { 1.0f, 0.12f, 0.08f } };
            const float *c = rgb[hover_relation(player)];
            for (int i = 0; i < 3; i++)
                g_want[i] = c[i] * g_bright;
            g_want[3] = 1.0f;
        }
    }
    if (on) {
        flush_card(); /* what was added before is not part of this model */
        g_outline = 1;
    }
    int r = ((draw_unit_t)VA(VA_DRAW_UNIT))(gp, chad, rdata, a);
    if (on) {
        flush_card();
        g_outline = 0;
    }
    return r;
}

/* Called from install_hooks once the exe is known to be the supported version. */
void outline_install(void)
{
    if (!g_outline_enabled)
        return;
    unsigned char *at = (unsigned char *)VA(VA_DRAW_UNIT_CALL);
    if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != VA(VA_DRAW_UNIT)) {
        logf_("outline: call site mismatch, turned off");
        g_outline_enabled = 0;
        return;
    }
    DWORD old;
    VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
    *(INT32 *)(at + 1) = (INT32)((UINT_PTR)c_ol_draw_unit - (UINT_PTR)(at + 5));
    VirtualProtect(at, 5, old, &old);
    logf_("hover outline installed");
}
