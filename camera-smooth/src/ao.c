/*
 * Contact shadows (screen-space ambient occlusion): a soft shade where things meet, at the foot of
 * buildings and walls and under units, worked out from the depth buffer each frame. And cloud
 * shadows: soft patches of shade drifting over the map with the wind.
 *
 * postfx.c's hook on D3D11GLSys::end_hdr calls ao_run() after the scene is drawn, while the HDR
 * scene and its depth are still bound (camera-smooth/ENGINE_NOTES.md, "Contact shadows"). It copies
 * the depth, draws three passes of its own at half the screen size (the shade from 12 depth samples
 * around each pixel, then a blur across and down that stops at depth edges) and a fourth that
 * multiplies the shade and the clouds into the HDR scene, so the game's own tone mapper sees a
 * shaded scene. Every piece of device state it touches is read first and put back after.
 *
 * The camera is orthographic. d3dgl keeps the matrices it set last in its D3D11Context
 * (D3D11GLSys+0x4F0; the scene's at this point), in column-vector form: the projection at +0xB0
 * (ndc.x = (m0 x + m3) / m15, ndc.y = (m5 y + m7) / m15, depth = (m10 z + m11) / m15), so a view
 * position is a scale and offset of the screen position and the depth; and the view at +0x70,
 * whose inverse takes a view position back to the world, where the clouds are anchored.
 *
 * Only what writes depth casts or takes the contact shade: the ground, units, trees and the
 * buildings' depth meshes. Drawing only, so it is multiplayer-safe.
 */
#define COBJMACROS
#include "ron.h"
#include <d3d11.h>
#include <stdio.h>
#include <string.h>

typedef long(__stdcall *d3dcompile_t)(const void *src, SIZE_T len, const char *name, const void *defines, void *include,
                                      const char *entry, const char *target, UINT f1, UINT f2, ID3DBlob **code,
                                      ID3DBlob **err);

static const char k_ao_hlsl[] =
    /* gRvApply: x strength, y 1 = comparison (only the right half), z 1 = show the shade only;
       gRvWorld0..3: the inverse view, rows (world = row . (view, 1), then / w); gRvCloud: x
       strength, y threshold of the noise, z 1 / cloud size in world units; gRvWind: the drift */
    "cbuffer RvAoCamera : register(b0) {\n"
    "\tfloat4 gRvScale; float4 gRvOffset; float4 gRvApply;\n"
    "\tfloat4 gRvWorld0; float4 gRvWorld1; float4 gRvWorld2; float4 gRvWorld3;\n"
    "\tfloat4 gRvCloud; float4 gRvWind;\n"
    "};\n"
    "Texture2D<float> RvDepth : register(t0);\n"
    "Texture2D<float> RvAoIn : register(t1);\n"
    "SamplerState RvLinear : register(s0);\n"
    "struct RvVsOut { float4 pos : SV_Position; };\n"
    "RvVsOut VS(uint id : SV_VertexID)\n"
    "{\n"
    "\tRvVsOut o;\n"
    "\tfloat2 t = float2((id << 1) & 2, id & 2);\n"
    "\to.pos = float4(t * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n"
    "\treturn o;\n"
    "}\n"
    /* a full-size depth pixel to view space */
    "float3 rv_view(int2 px, float2 size)\n"
    "{\n"
    "\tfloat d = RvDepth.Load(int3(px, 0));\n"
    "\tfloat2 ndc = float2((px.x + 0.5f) / size.x * 2.0f - 1.0f, 1.0f - (px.y + 0.5f) / size.y * 2.0f);\n"
    "\treturn (float3(ndc, d) - gRvOffset.xyz) / gRvScale.xyz;\n"
    "}\n"
    "static const float rv_bayer[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };\n"
    "float PS_AO(RvVsOut i) : SV_Target\n"
    "{\n"
    "\tuint w, h;\n"
    "\tRvDepth.GetDimensions(w, h);\n"
    "\tfloat2 size = float2(w, h);\n"
    "\tint2 hp = int2(i.pos.xy);\n"
    "\tint2 c = clamp(hp * 2, int2(1, 1), int2(w, h) - 2);\n"
    "\tfloat3 p = rv_view(c, size);\n"
    "\tfloat3 l = rv_view(c - int2(1, 0), size), r = rv_view(c + int2(1, 0), size);\n"
    "\tfloat3 u = rv_view(c - int2(0, 1), size), b = rv_view(c + int2(0, 1), size);\n"
    "\tfloat3 dx = abs(r.z - p.z) < abs(p.z - l.z) ? r - p : p - l;\n"
    "\tfloat3 dy = abs(b.z - p.z) < abs(p.z - u.z) ? b - p : p - u;\n"
    "\tfloat3 n = cross(dx, dy);\n"
    "\tif (dot(n, n) < 1e-12f)\n"
    "\t\treturn 0.0f;\n"
    "\tn = normalize(n);\n"
    /* towards the camera: its view runs along the depth, so the normal must point against it */
    "\tif (n.z * sign(gRvScale.z) > 0.0f)\n"
    "\t\tn = -n;\n"
    "\tfloat2 rpx = RV_AO_RADIUS * abs(gRvScale.xy) * 0.5f * size;\n"
    "\tfloat rot = rv_bayer[(hp.y & 3) * 4 + (hp.x & 3)] * (6.2831853f / 16.0f);\n"
    "\tfloat occ = 0.0f;\n"
    "\t[unroll] for (int k = 0; k < 12; k++) {\n"
    "\t\tfloat a = k * 2.3999632f + rot;\n"
    "\t\tfloat2 o = float2(cos(a), sin(a)) * sqrt((k + 0.5f) / 12.0f) * rpx;\n"
    "\t\tint2 s = clamp(c + int2(o), int2(0, 0), int2(w, h) - 1);\n"
    "\t\tfloat3 v = rv_view(s, size) - p;\n"
    "\t\tfloat dist = length(v);\n"
    "\t\tif (dist > 0.001f)\n"
    "\t\t\tocc += max(0.0f, dot(n, v) / dist - 0.15f) * saturate(1.0f - dist / (2.0f * RV_AO_RADIUS));\n"
    "\t}\n"
    "\treturn saturate(occ * (6.0f / 12.0f));\n"
    "}\n"
    /* the blur: 9 taps along RV_DIR, weighted down where the depth differs (no shade bleeding
       from the ground onto a wall in front of it) */
    "float PS_BLUR(RvVsOut i) : SV_Target\n"
    "{\n"
    "\tuint w, h, dw, dh;\n"
    "\tRvAoIn.GetDimensions(w, h);\n"
    "\tRvDepth.GetDimensions(dw, dh);\n"
    "\tfloat2 size = float2(dw, dh);\n"
    "\tint2 c = int2(i.pos.xy);\n"
    "\tint2 dmax = int2(dw, dh) - 1;\n"
    "\tfloat z0 = rv_view(min(c * 2, dmax), size).z;\n"
    "\tfloat sum = 0.0f, ws = 0.0f;\n"
    "\t[unroll] for (int k = -4; k <= 4; k++) {\n"
    "\t\tint2 s = clamp(c + RV_DIR * k, int2(0, 0), int2(w, h) - 1);\n"
    "\t\tfloat z = rv_view(min(s * 2, dmax), size).z;\n"
    "\t\tfloat wt = exp(-(float)(k * k) / 12.5f) * saturate(1.0f - abs(z - z0) / (0.5f * RV_AO_RADIUS));\n"
    "\t\tsum += RvAoIn.Load(int3(s, 0)) * wt;\n"
    "\t\tws += wt;\n"
    "\t}\n"
    "\treturn sum / max(ws, 0.0001f);\n"
    "}\n"
    /* the clouds: smooth value noise in four octaves, anchored to the world's ground plane (x, y),
       each octave drifting a little differently so the clouds change shape as they go */
    "float rv_hash(float2 p) { return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f); }\n"
    "float rv_noise(float2 x)\n"
    "{\n"
    "\tfloat2 i = floor(x), f = frac(x);\n"
    "\tf = f * f * (3.0f - 2.0f * f);\n"
    "\treturn lerp(lerp(rv_hash(i), rv_hash(i + float2(1.0f, 0.0f)), f.x),\n"
    "\t            lerp(rv_hash(i + float2(0.0f, 1.0f)), rv_hash(i + float2(1.0f, 1.0f)), f.x), f.y);\n"
    "}\n"
    "float rv_clouds(float2 w)\n"
    "{\n"
    "\tfloat2 q = w * gRvCloud.z + gRvWind.xy;\n"
    "\tfloat n = 0.5f * rv_noise(q) + 0.25f * rv_noise(q * 2.03f + float2(17.3f, 9.1f) + gRvWind.xy * 0.4f)\n"
    "\t        + 0.125f * rv_noise(q * 4.01f + float2(5.7f, 31.4f) - gRvWind.xy * 0.3f)\n"
    "\t        + 0.03f * rv_noise(q * 8.05f + float2(41.2f, 3.3f));\n"
    /* a wide, soft edge: cloud shadows have no sharp rim */
    "\treturn smoothstep(gRvCloud.y - 0.08f, gRvCloud.y + 0.16f, n / 0.905f);\n"
    "}\n"
    /* into the HDR scene: multiplied (blend: destination x source), or written as it is to show it */
    "float4 PS_APPLY(RvVsOut i) : SV_Target\n"
    "{\n"
    "\tuint dw, dh;\n"
    "\tRvDepth.GetDimensions(dw, dh);\n"
    "\tfloat2 uv = i.pos.xy / float2(dw, dh);\n"
    "\tif (gRvApply.y > 0.5f && uv.x < 0.5f)\n"
    "\t\treturn float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
    "\tfloat k = 1.0f - gRvApply.x * RvAoIn.SampleLevel(RvLinear, uv, 0);\n"
    "\tif (gRvCloud.x > 0.0f) {\n"
    "\t\tfloat4 v = float4(rv_view(int2(i.pos.xy), float2(dw, dh)), 1.0f);\n"
    "\t\tfloat4 w = float4(dot(gRvWorld0, v), dot(gRvWorld1, v), dot(gRvWorld2, v), dot(gRvWorld3, v));\n"
    /* only on what stands on the map: where nothing wrote depth (outside the map's edge) the
       position lies far along the view and the noise smears into long streaks */
    "\t\tfloat3 p = w.xyz / w.w;\n"
    "\t\tif (p.z > -1500.0f && p.z < 6000.0f)\n"
    "\t\t\tk *= 1.0f - gRvCloud.x * rv_clouds(p.xy);\n"
    "\t}\n"
    "\treturn float4(k.xxx, 1.0f);\n"
    "}\n";

static int g_failed;
static ID3D11Device *g_dev;
static ID3D11VertexShader *g_vs;
static ID3D11PixelShader *g_ps_ao, *g_ps_bh, *g_ps_bv, *g_ps_apply;
static ID3D11Buffer *g_cb;                /* gRvScale, gRvOffset, gRvApply */
static ID3D11BlendState *g_bs_mul;        /* destination x source colour */
static ID3D11SamplerState *g_linear;
static ID3D11Texture2D *g_depth;          /* our copy of the scene's depth */
static ID3D11ShaderResourceView *g_depth_srv;
static ID3D11Texture2D *g_tex[2];         /* the shade, half size: [0] the result, [1] the blur between */
static ID3D11ShaderResourceView *g_srv[2];
static ID3D11RenderTargetView *g_rtv[2];
static UINT g_w, g_h;
static DXGI_FORMAT g_fmt;

#define RELEASE(p)                                \
    do {                                          \
        if (p) {                                  \
            IUnknown_Release((IUnknown *)(p));    \
            (p) = NULL;                           \
        }                                         \
    } while (0)

static void drop_targets(void)
{
    RELEASE(g_depth_srv);
    RELEASE(g_depth);
    for (int i = 0; i < 2; i++) {
        RELEASE(g_rtv[i]);
        RELEASE(g_srv[i]);
        RELEASE(g_tex[i]);
    }
    g_w = g_h = 0;
}

static void drop_all(void)
{
    drop_targets();
    RELEASE(g_vs);
    RELEASE(g_ps_ao);
    RELEASE(g_ps_bh);
    RELEASE(g_ps_bv);
    RELEASE(g_ps_apply);
    RELEASE(g_cb);
    RELEASE(g_bs_mul);
    RELEASE(g_linear);
    g_dev = NULL;
}

static ID3DBlob *compile(d3dcompile_t fn, const char *name, const char *entry, const char *target,
                         const D3D_SHADER_MACRO *defs)
{
    ID3DBlob *code = NULL, *err = NULL;
    long hr = fn(k_ao_hlsl, sizeof k_ao_hlsl - 1, name, defs, NULL, entry, target, 1u << 15 /* OPTIMIZATION_LEVEL3 */,
                 0, &code, &err);
    if (hr < 0 || !code)
        logf_("contact shadows: %s does not compile (%08lX): %.300s", entry, (unsigned long)hr,
              err ? (const char *)ID3D10Blob_GetBufferPointer(err) : "");
    RELEASE(err);
    return hr < 0 ? NULL : code;
}

static int make_ps(ID3D11Device *dev, ID3DBlob *code, ID3D11PixelShader **ps)
{
    return code &&
           ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(code), ID3D10Blob_GetBufferSize(code), NULL,
                                          ps) >= 0;
}

static int make_shaders(ID3D11Device *dev, float radius)
{
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
    d3dcompile_t fn = dc ? (d3dcompile_t)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!fn)
        return 0;
    const char *name = "rv_contact_shadows.hlsl";
    char rad[32];
    snprintf(rad, sizeof rad, "%.2ff", radius);
    D3D_SHADER_MACRO d0[] = { { "RV_AO_RADIUS", rad }, { "RV_DIR", "int2(1, 0)" }, { NULL, NULL } };
    D3D_SHADER_MACRO d1[] = { { "RV_AO_RADIUS", rad }, { "RV_DIR", "int2(0, 1)" }, { NULL, NULL } };
    ID3DBlob *vs = compile(fn, name, "VS", "vs_4_0", d0), *ao = compile(fn, name, "PS_AO", "ps_4_0", d0);
    ID3DBlob *bh = compile(fn, name, "PS_BLUR", "ps_4_0", d0), *bv = compile(fn, name, "PS_BLUR", "ps_4_0", d1);
    ID3DBlob *ap = compile(fn, name, "PS_APPLY", "ps_4_0", d0);
    int ok = vs &&
             ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(vs), ID3D10Blob_GetBufferSize(vs), NULL,
                                             &g_vs) >= 0 &&
             make_ps(dev, ao, &g_ps_ao) && make_ps(dev, bh, &g_ps_bh) && make_ps(dev, bv, &g_ps_bv) &&
             make_ps(dev, ap, &g_ps_apply);
    RELEASE(vs);
    RELEASE(ao);
    RELEASE(bh);
    RELEASE(bv);
    RELEASE(ap);
    D3D11_BUFFER_DESC bd = { 144, D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
    D3D11_BLEND_DESC bl = { 0 };
    bl.RenderTarget[0].BlendEnable = TRUE;
    bl.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO, bl.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
    bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO, bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_SAMPLER_DESC sd = { 0 };
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    return ok && ID3D11Device_CreateBuffer(dev, &bd, NULL, &g_cb) >= 0 &&
           ID3D11Device_CreateBlendState(dev, &bl, &g_bs_mul) >= 0 &&
           ID3D11Device_CreateSamplerState(dev, &sd, &g_linear) >= 0;
}

/* The copy's typeless format and the view's format for a depth format; 0 if we do not know it. */
static int depth_formats(DXGI_FORMAT f, DXGI_FORMAT *copy, DXGI_FORMAT *view)
{
    switch (f) {
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_TYPELESS:
        *copy = DXGI_FORMAT_R32_TYPELESS, *view = DXGI_FORMAT_R32_FLOAT;
        return 1;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
        *copy = DXGI_FORMAT_R24G8_TYPELESS, *view = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        return 1;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        *copy = DXGI_FORMAT_R32G8X24_TYPELESS, *view = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        return 1;
    default:
        return 0;
    }
}

static int make_targets(ID3D11Device *dev, const D3D11_TEXTURE2D_DESC *src)
{
    DXGI_FORMAT copy, view;
    if (!depth_formats(src->Format, &copy, &view) || src->SampleDesc.Count != 1) {
        logf_("contact shadows: depth format %u with %u samples is not one we know, off", (unsigned)src->Format,
              (unsigned)src->SampleDesc.Count);
        return 0;
    }
    D3D11_TEXTURE2D_DESC d = { 0 };
    d.Width = src->Width, d.Height = src->Height, d.MipLevels = 1, d.ArraySize = 1, d.Format = copy;
    d.SampleDesc.Count = 1, d.Usage = D3D11_USAGE_DEFAULT, d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SHADER_RESOURCE_VIEW_DESC sv = { 0 };
    sv.Format = view, sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D, sv.Texture2D.MipLevels = 1;
    if (ID3D11Device_CreateTexture2D(dev, &d, NULL, &g_depth) < 0 ||
        ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)g_depth, &sv, &g_depth_srv) < 0)
        return 0;
    d.Width = (src->Width + 1) / 2, d.Height = (src->Height + 1) / 2, d.Format = DXGI_FORMAT_R8_UNORM;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    for (int i = 0; i < 2; i++)
        if (ID3D11Device_CreateTexture2D(dev, &d, NULL, &g_tex[i]) < 0 ||
            ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)g_tex[i], NULL, &g_srv[i]) < 0 ||
            ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource *)g_tex[i], NULL, &g_rtv[i]) < 0)
            return 0;
    g_w = src->Width, g_h = src->Height, g_fmt = src->Format;
    logf_("contact shadows: depth %ux%u format %u, shade %ux%u", g_w, g_h, (unsigned)g_fmt, d.Width, d.Height);
    return 1;
}

/* The HDR scene: the render target bound when the tone mapper starts, if it has the depth's size
   (a new reference, or NULL). */
static ID3D11RenderTargetView *bound_scene(ID3D11DeviceContext *dc)
{
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11DeviceContext_OMGetRenderTargets(dc, 1, &rtv, NULL);
    if (!rtv)
        return NULL;
    ID3D11Resource *res = NULL;
    ID3D11RenderTargetView_GetResource(rtv, &res);
    D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    D3D11_TEXTURE2D_DESC td = { 0 };
    if (res) {
        ID3D11Resource_GetType(res, &dim);
        if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
            ID3D11Texture2D_GetDesc((ID3D11Texture2D *)res, &td);
        RELEASE(res);
    }
    static int said;
    if (!said++)
        logf_("contact shadows: the scene's target %ux%u format %u", td.Width, td.Height, (unsigned)td.Format);
    if (td.Width != g_w || td.Height != g_h) {
        RELEASE(rtv);
        return NULL;
    }
    return rtv;
}

/* Everything our passes change, read before and put back after. */
typedef struct {
    ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView *dsv;
    D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    UINT nvp;
    ID3D11RasterizerState *rs;
    ID3D11BlendState *bs;
    FLOAT blend[4];
    UINT mask;
    ID3D11DepthStencilState *ds;
    UINT ref;
    ID3D11InputLayout *il;
    D3D11_PRIMITIVE_TOPOLOGY topo;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11ShaderResourceView *srv[2];
    ID3D11Buffer *cb0;
    ID3D11SamplerState *smp;
} saved_t;

static void save(ID3D11DeviceContext *dc, saved_t *s)
{
    memset(s, 0, sizeof *s);
    ID3D11DeviceContext_OMGetRenderTargets(dc, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, s->rtv, &s->dsv);
    s->nvp = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ID3D11DeviceContext_RSGetViewports(dc, &s->nvp, s->vp);
    ID3D11DeviceContext_RSGetState(dc, &s->rs);
    ID3D11DeviceContext_OMGetBlendState(dc, &s->bs, s->blend, &s->mask);
    ID3D11DeviceContext_OMGetDepthStencilState(dc, &s->ds, &s->ref);
    ID3D11DeviceContext_IAGetInputLayout(dc, &s->il);
    ID3D11DeviceContext_IAGetPrimitiveTopology(dc, &s->topo);
    ID3D11DeviceContext_VSGetShader(dc, &s->vs, NULL, NULL);
    ID3D11DeviceContext_PSGetShader(dc, &s->ps, NULL, NULL);
    ID3D11DeviceContext_PSGetShaderResources(dc, 0, 2, s->srv);
    ID3D11DeviceContext_PSGetConstantBuffers(dc, 0, 1, &s->cb0);
    ID3D11DeviceContext_PSGetSamplers(dc, 0, 1, &s->smp);
}

static void restore(ID3D11DeviceContext *dc, saved_t *s)
{
    ID3D11ShaderResourceView *none[2] = { NULL, NULL };
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 2, none);
    ID3D11DeviceContext_OMSetRenderTargets(dc, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, s->rtv, s->dsv);
    ID3D11DeviceContext_RSSetViewports(dc, s->nvp, s->vp);
    ID3D11DeviceContext_RSSetState(dc, s->rs);
    ID3D11DeviceContext_OMSetBlendState(dc, s->bs, s->blend, s->mask);
    ID3D11DeviceContext_OMSetDepthStencilState(dc, s->ds, s->ref);
    ID3D11DeviceContext_IASetInputLayout(dc, s->il);
    ID3D11DeviceContext_IASetPrimitiveTopology(dc, s->topo);
    ID3D11DeviceContext_VSSetShader(dc, s->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(dc, s->ps, NULL, 0);
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 2, s->srv);
    ID3D11DeviceContext_PSSetConstantBuffers(dc, 0, 1, &s->cb0);
    ID3D11DeviceContext_PSSetSamplers(dc, 0, 1, &s->smp);
    for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
        RELEASE(s->rtv[i]);
    RELEASE(s->dsv);
    RELEASE(s->rs);
    RELEASE(s->bs);
    RELEASE(s->ds);
    RELEASE(s->il);
    RELEASE(s->vs);
    RELEASE(s->ps);
    RELEASE(s->srv[0]);
    RELEASE(s->srv[1]);
    RELEASE(s->cb0);
    RELEASE(s->smp);
}

static void pass(ID3D11DeviceContext *dc, ID3D11PixelShader *ps, ID3D11RenderTargetView *out,
                 ID3D11ShaderResourceView *in, UINT w, UINT h)
{
    ID3D11ShaderResourceView *none[2] = { NULL, NULL };
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 2, none);
    ID3D11DeviceContext_OMSetRenderTargets(dc, 1, &out, NULL);
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (FLOAT)w, (FLOAT)h, 0.0f, 1.0f };
    ID3D11DeviceContext_RSSetViewports(dc, 1, &vp);
    ID3D11ShaderResourceView *srv[2] = { g_depth_srv, in };
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 2, srv);
    ID3D11DeviceContext_PSSetShader(dc, ps, NULL, 0);
    ID3D11DeviceContext_Draw(dc, 3, 0);
}

/* The inverse of a 4x4 matrix (row-major, 16 floats); 0 if it has none. */
static int invert4(const float *m, float *out)
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0f)
        return 0;
    for (int i = 0; i < 16; i++)
        out[i] = inv[i] / det;
    return 1;
}

/* After the scene, before end_hdr: the contact shade and the clouds multiplied into the HDR scene.
   `depth` is d3dgl's depth texture (D3D11GLSys+0xD04), used when no depth view is bound at this
   point; `proj` and `viewm` the projection and view it set last; `split` 1 while the comparison
   shows the game's own picture on the left half; `view` 1 to show the shade alone. 1 when it drew. */
int ao_run(void *context, const ao_params_t *ap, void *depth, const float *proj, const float *viewm, int split,
           int view)
{
    float vinv[16];
    if (g_failed || !context || !ap || !proj || proj[0] == 0.0f || proj[10] == 0.0f || proj[15] == 0.0f ||
        !viewm || !invert4(viewm, vinv))
        return 0;
    float radius = ap->radius, strength = ap->strength;
    ID3D11DeviceContext *dc = context;
    ID3D11DepthStencilView *dsv = NULL;
    ID3D11Resource *res = NULL;
    int from_view = 0;
    ID3D11DeviceContext_OMGetRenderTargets(dc, 0, NULL, &dsv);
    if (dsv) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dd;
        ID3D11DepthStencilView_GetDesc(dsv, &dd);
        if (dd.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
            ID3D11DepthStencilView_GetResource(dsv, &res);
        from_view = res != NULL;
        RELEASE(dsv);
    } else if (depth) {
        res = depth;
        IUnknown_AddRef((IUnknown *)res);
    }
    static int said;
    if (!said++)
        logf_("contact shadows: depth from the %s; projection %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g",
              from_view ? "bound depth view" : res ? "game's depth texture" : "(none)", proj[0], proj[1], proj[2],
              proj[3], proj[4], proj[5], proj[6], proj[7], proj[8], proj[9], proj[10], proj[11], proj[12], proj[13],
              proj[14], proj[15]);
    if (said == 1)
        logf_("cloud shadows: view %g %g %g %g / %g %g %g %g / %g %g %g %g / %g %g %g %g", viewm[0], viewm[1],
              viewm[2], viewm[3], viewm[4], viewm[5], viewm[6], viewm[7], viewm[8], viewm[9], viewm[10], viewm[11],
              viewm[12], viewm[13], viewm[14], viewm[15]);
    if (!res)
        return 0;
    D3D11_RESOURCE_DIMENSION dim;
    ID3D11Resource_GetType(res, &dim);
    if (dim != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        RELEASE(res);
        return 0;
    }
    D3D11_TEXTURE2D_DESC td;
    ID3D11Texture2D_GetDesc((ID3D11Texture2D *)res, &td);
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext_GetDevice(dc, &dev);
    if (!dev) {
        RELEASE(res);
        return 0;
    }
    ID3D11Device_Release(dev); /* the context keeps it alive; dev stays valid */
    if (dev != g_dev) {
        drop_all();
        g_dev = dev;
        if (!make_shaders(dev, radius)) {
            logf_("contact shadows: shaders could not be made, off");
            g_failed = 1;
            drop_all();
            RELEASE(res);
            return 0;
        }
        logf_("contact shadows: on, radius %.0f world units, strength %.2f; clouds %.2f, cover %.2f, size %.0f, "
              "drift %.1f units/s", radius, strength, ap->clouds, ap->cover, ap->size, ap->speed);
    }
    if (td.Width != g_w || td.Height != g_h || td.Format != g_fmt) {
        drop_targets();
        if (!make_targets(dev, &td)) {
            g_failed = 1;
            drop_all();
            RELEASE(res);
            return 0;
        }
    }
    ID3D11RenderTargetView *out = bound_scene(dc);
    if (!out) {
        RELEASE(res);
        return 0;
    }
    ID3D11DeviceContext_CopyResource(dc, (ID3D11Resource *)g_depth, res);
    RELEASE(res);

    float m = proj[15];
    /* the noise's threshold for the share of the map under clouds (its values gather around 0.5),
       the drift along a fixed wind, in cloud sizes */
    float size = ap->size > 1.0f ? ap->size : 1.0f;
    float thr = 0.72f - 0.42f * ap->cover, drift = ap->speed * ap->seconds / size;
    float cb[36] = { proj[0] / m, proj[5] / m, proj[10] / m, 0.0f,
                     (proj[3] + proj[12]) / m, (proj[7] + proj[13]) / m, (proj[11] + proj[14]) / m, 0.0f,
                     view ? 1.0f : strength, split ? 1.0f : 0.0f, view ? 1.0f : 0.0f, 0.0f };
    memcpy(cb + 12, vinv, sizeof vinv);
    cb[28] = ap->clouds, cb[29] = thr, cb[30] = 1.0f / size, cb[31] = 0.0f;
    cb[32] = drift * 0.92f, cb[33] = drift * 0.39f, cb[34] = 0.0f, cb[35] = 0.0f;
    ID3D11DeviceContext_UpdateSubresource(dc, (ID3D11Resource *)g_cb, 0, NULL, cb, 0, 0);
    int shade = strength > 0.0f || view;
    saved_t s;
    save(dc, &s);
    UINT hw = (g_w + 1) / 2, hh = (g_h + 1) / 2;
    ID3D11DeviceContext_RSSetState(dc, NULL);
    ID3D11DeviceContext_OMSetBlendState(dc, NULL, NULL, 0xFFFFFFFFu);
    ID3D11DeviceContext_OMSetDepthStencilState(dc, NULL, 0);
    ID3D11DeviceContext_IASetInputLayout(dc, NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(dc, g_vs, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(dc, 0, 1, &g_cb);
    ID3D11DeviceContext_PSSetSamplers(dc, 0, 1, &g_linear);
    if (shade) {
        pass(dc, g_ps_ao, g_rtv[0], NULL, hw, hh);
        pass(dc, g_ps_bh, g_rtv[1], g_srv[0], hw, hh);
        pass(dc, g_ps_bv, g_rtv[0], g_srv[1], hw, hh);
    }
    if (!view)
        ID3D11DeviceContext_OMSetBlendState(dc, g_bs_mul, NULL, 0xFFFFFFFFu);
    pass(dc, g_ps_apply, out, g_srv[0], g_w, g_h);
    restore(dc, &s);
    RELEASE(out);
    return 1;
}
