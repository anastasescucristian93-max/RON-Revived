/*
 * A better look for the game's picture: colour (black level, contrast, vibrance, warmth, a soft
 * vignette) and sharpening, drawn by the game's own post passes, so the interface is untouched;
 * and better water: real reflections, sun glints, a touch of sky, livelier waves.
 *
 * d3dgl compiles every shader at run time with D3DCompileFromFile / D3DCompile from
 * shaders\*.hlsl (camera-smooth/ENGINE_NOTES.md, "Picture look" and "Water"). Scene::render runs
 * the tone mapper (ps_tonemap: the HDR scene to screen colours, then bloom) and FXAA (ps_fxaa)
 * on the scene alone; health bars, overlays and the interface are drawn after them. The water is
 * a permutation of the uber shader ps_global (ANIMATE_WATER_NORMALS, only the water has it).
 * postfx.c reads the game's files, adds its code with the ini values as constants and serves the
 * result in their place (override_redirect). The includes still come from the game's shaders
 * folder: the compiler resolves them from the name it was given, not from the file it got.
 *
 * Also fixed in ps_tonemap: FXAA takes the brightness from the alpha channel, which only the
 * bloom pass writes. With bloom off the tone mapper's alpha 1 reached FXAA, which then smoothed
 * nothing. The new code writes the brightness into alpha itself.
 *
 * Ctrl+F9 switches the new look off and on, Ctrl+F10 shows the game's own picture on the left
 * half and the new one on the right, with a thin line between. The switch is a constant buffer in
 * slot b7 (free in d3dgl), bound before every tone mapper run (its vtable's do_run is ours) and
 * left bound, so the next frame's water sees it too; unbound it reads 0, which is "look on".
 * Drawing only, so it is multiplayer-safe.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define VA_GRAPHSYS 0x00E335A0u /* GraphSys *: d3dgl's D3D11GLSys, +0x47C the immediate context */
#define SYS_CONTEXT 0x47C

/* d3dgl.dll, built 2024-06-20 */
#define D3DGL_TIMESTAMP     0x667485A4u
#define D3DGL_PREF_BASE     0x10000000u
#define D3DGL_TONEMAP_VTBL  0x100E4FC4u /* D3D11ToneMapper vtable: +4 do_run(bool bloom) */
#define D3DGL_TONEMAP_RUN   0x10044A30u

#define COM_FN(obj, idx) ((*(void ***)(obj))[idx])
#define DC_GET_DEVICE    3
#define DC_PS_SET_CB     16
#define DC_UPDATE_SUB    48
#define DEV_CREATE_BUFFER 3
#define BLOB_PTR  3
#define BLOB_SIZE 4

typedef unsigned long(__stdcall *release_t)(void *self);
typedef void(__stdcall *get_device_t)(void *dc, void **dev);
typedef void(__stdcall *cb_t)(void *dc, UINT slot, UINT n, void **bufs);
typedef void(__stdcall *update_sub_t)(void *dc, void *res, UINT sub, const void *box, const void *data, UINT pitch, UINT depth);
typedef long(__stdcall *create_buffer_t)(void *dev, const void *desc, const void *init, void **buf);
typedef void *(__stdcall *blob_ptr_t)(void *blob);
typedef long(__stdcall *d3dcompile_t)(const void *src, SIZE_T len, const char *name, const void *defines, void *include,
                                      const char *entry, const char *target, UINT f1, UINT f2, void **code, void **err);
typedef void(__attribute__((thiscall)) *do_run_t)(void *self, int bloom);

int g_postfx_enabled;
static struct {
    float sharpen, vibrance, contrast, warmth, black, vignette;
    int fix_fxaa, keys;
    int water;
    float reflect, glint, sky, waves;
} cfg;

static float g_mode[4];         /* x: 1 = the game's own look, y: 1 = comparison */
static float g_have[4] = { -1.0f };
static void *g_dev, *g_cb;
static do_run_t g_real_run;
static int g_shots; /* [debug] postfx_shots: test pictures, then quit */
static int g_water_debug; /* [debug] water_debug: water pixels with a reflection in magenta */

void postfx_load_config(const char *ini_path)
{
    g_postfx_enabled = GetPrivateProfileIntA("graphics", "postfx", 1, ini_path);
    cfg.sharpen = ini_float(ini_path, "graphics", "sharpen", 0.5f);
    cfg.vibrance = ini_float(ini_path, "graphics", "vibrance", 0.15f);
    cfg.contrast = ini_float(ini_path, "graphics", "contrast", 0.12f);
    cfg.warmth = ini_float(ini_path, "graphics", "warmth", 0.0f);
    cfg.black = ini_float(ini_path, "graphics", "black_level", 0.02f);
    cfg.vignette = ini_float(ini_path, "graphics", "vignette", 0.12f);
    cfg.fix_fxaa = GetPrivateProfileIntA("graphics", "fix_fxaa", 1, ini_path);
    cfg.keys = GetPrivateProfileIntA("graphics", "compare_keys", 1, ini_path);
    cfg.water = GetPrivateProfileIntA("graphics", "water", 1, ini_path);
    cfg.reflect = ini_float(ini_path, "graphics", "water_reflections", 0.5f);
    cfg.glint = ini_float(ini_path, "graphics", "water_glints", 1.5f);
    cfg.sky = ini_float(ini_path, "graphics", "water_sky", 0.25f);
    cfg.waves = ini_float(ini_path, "graphics", "water_waves", 1.25f);
    g_shots = GetPrivateProfileIntA("debug", "postfx_shots", 0, ini_path);
    g_water_debug = GetPrivateProfileIntA("debug", "water_debug", 0, ini_path);
#define CLAMP(v, lo, hi) v = v < lo ? lo : v > hi ? hi : v
    CLAMP(cfg.sharpen, 0.0f, 1.0f);
    CLAMP(cfg.vibrance, -1.0f, 1.0f);
    CLAMP(cfg.contrast, -1.0f, 1.0f);
    CLAMP(cfg.warmth, -0.2f, 0.2f);
    CLAMP(cfg.black, 0.0f, 0.2f);
    CLAMP(cfg.vignette, 0.0f, 0.5f);
    CLAMP(cfg.reflect, 0.0f, 1.0f);
    CLAMP(cfg.glint, 0.0f, 10.0f);
    CLAMP(cfg.sky, 0.0f, 2.0f);
    CLAMP(cfg.waves, 0.25f, 3.0f);
#undef CLAMP
}

/* ---- the shaders -------------------------------------------------------------------------- */

static const char k_common[] =
    "// ---- RON Revived (camera-smooth postfx.c): settings from ron_camera.ini [graphics] ----\n"
    "cbuffer RevivedFx : register(b7) { float4 gRvMode; };\n"
    "bool rv_original(float2 uv) { return gRvMode.x > 0.5f || (gRvMode.y > 0.5f && uv.x < 0.5f); }\n";

/* After the game's colour: levels, contrast, vibrance, warmth, vignette (postfx_preview.py has
   the same math). */
static const char k_grade[] =
    "float4 rv_grade(float4 src, float2 uv)\n"
    "{\n"
    "\tfloat px = abs(ddx(uv.x));\n"
    "\tif (gRvMode.y > 0.5f && gRvMode.x < 0.5f && abs(uv.x - 0.5f) < px)\n"
    "\t\treturn float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
    "\tif (rv_original(uv))\n"
    "\t\treturn src;\n"
    "\tfloat3 c = saturate((src.rgb - RV_BLACK) / (1.0f - RV_BLACK));\n"
    "\tc += RV_CONTRAST * (c * c * (3.0f - 2.0f * c) - c);\n"
    "\tfloat sat = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));\n"
    "\tfloat l = dot(c, cLuminanceWeight);\n"
    "\tc = l + (c - l) * (1.0f + RV_VIBRANCE * (1.0f - sat));\n"
    "\tc *= float3(1.0f + RV_WARMTH, 1.0f, 1.0f - RV_WARMTH);\n"
    "\tfloat2 d = uv - 0.5f;\n"
    "\tc *= 1.0f - RV_VIGNETTE * smoothstep(0.1f, 0.5f, dot(d, d));\n"
    "\tc = saturate(c);\n"
    "\treturn float4(c, RV_FIX_FXAA ? dot(c, cLuminanceWeight) : src.a);\n"
    "}\n\n";

/* After FXAA: contrast-adaptive sharpening (AMD's CAS, the 5-tap form) from the four neighbours. */
static const char k_sharpen[] =
    "float3 rv_sharpen(float3 e, float2 pos, float2 px)\n"
    "{\n"
    "\tif (RV_SHARPEN <= 0.0f || rv_original(pos))\n"
    "\t\treturn e;\n"
    "\tfloat3 b = ColorTexture.SampleLevel(Sampler, pos + float2(0.0f, -px.y), 0).rgb;\n"
    "\tfloat3 d = ColorTexture.SampleLevel(Sampler, pos + float2(-px.x, 0.0f), 0).rgb;\n"
    "\tfloat3 f = ColorTexture.SampleLevel(Sampler, pos + float2(px.x, 0.0f), 0).rgb;\n"
    "\tfloat3 h = ColorTexture.SampleLevel(Sampler, pos + float2(0.0f, px.y), 0).rgb;\n"
    "\tfloat3 mn = min(e, min(min(b, d), min(f, h)));\n"
    "\tfloat3 mx = max(e, max(max(b, d), max(f, h)));\n"
    "\tfloat3 amp = sqrt(saturate(min(mn, 1.0f - mx) / max(mx, 0.0001f)));\n"
    "\tfloat3 w = amp * (-1.0f / lerp(8.0f, 5.0f, RV_SHARPEN));\n"
    "\treturn saturate((e + (b + d + f + h) * w) / (1.0f + 4.0f * w));\n"
    "}\n\n";

/* Water, in the uber shader. The game's reflection marched 10 steps of 10 units (half a tile) and
   so almost never met anything; this one marches up to about 9 tiles in growing steps, compares
   linear depths (the previous frame's depth and colour, d3dgl's ReflectionManager copies), finds
   the meeting point by halving, and fades out at the screen's edges and the end of the ray. */
static const char k_water[] =
    "float rv_water_on(float2 px) { return rv_original(px / gScreenSize) ? 0.0f : 1.0f; }\n"
    "#ifdef HAS_SCREEN_REFLECTIONS\n"
    /* The game's camera is orthographic (_34 = 0, _44 = 1); a perspective one is handled too.
       Distances along the view: s is +1 or -1, the way the view space looks. */
    "static bool rv_persp = abs(gProjection._34) > 0.5f;\n"
    "float rv_lin(float d, float s)\n"
    "{\n"
    "\tif (rv_persp)\n"
    "\t\treturn gProjection._34 * gProjection._43 / (d * gProjection._34 - gProjection._33);\n"
    "\treturn (d - gProjection._43) / gProjection._33 * s;\n"
    "}\n"
    "float4 rv_project(float3 v, float s)\n"
    "{\n"
    "\tfloat4 p = mul(float4(v, 1.0f), gProjection);\n"
    "\tfloat2 uv = p.xy / p.w * 0.5f + 0.5f;\n"
    "\treturn float4(uv.x, 1.0f - uv.y, rv_persp ? p.w : v.z * s, p.w);\n"
    "}\n"
    "float4 rv_ssr(float3 pos, float3 N, float3 V)\n"
    "{\n"
    "\tfloat s = pos.z < 0.0f ? -1.0f : 1.0f;\n"
    "\tfloat3 dir = reflect(rv_persp ? normalize(pos) : float3(0.0f, 0.0f, s), N);\n"
    "\tfloat len = RV_SSR_STEP;\n"
    "\tfloat3 ray = pos, prev = pos;\n"
    "\t[loop]\n"
    "\tfor (uint i = 0; i < 24; ++i)\n"
    "\t{\n"
    "\t\tprev = ray;\n"
    "\t\tray += dir * len;\n"
    "\t\tlen *= 1.1f;\n"
    "\t\tfloat4 p = rv_project(ray, s);\n"
    "\t\tif (p.z <= 0.0f || p.x < 0.0f || p.x > 1.0f || p.y < 0.0f || p.y > 1.0f)\n"
    "\t\t\tbreak;\n"
    "\t\tfloat diff = p.z - rv_lin(DepthTex.SampleLevel(SamplerA, p.xy, 0).x, s);\n"
    "\t\tif (diff > 2.0f && diff < RV_SSR_THICK)\n"
    "\t\t{\n"
    "\t\t\tfloat3 a = prev, b = ray;\n"
    "\t\t\t[unroll]\n"
    "\t\t\tfor (int k = 0; k < 5; ++k)\n"
    "\t\t\t{\n"
    "\t\t\t\tfloat3 m = (a + b) * 0.5f;\n"
    "\t\t\t\tfloat4 q = rv_project(m, s);\n"
    "\t\t\t\tif (q.z > rv_lin(DepthTex.SampleLevel(SamplerA, q.xy, 0).x, s))\n"
    "\t\t\t\t\tb = m;\n"
    "\t\t\t\telse\n"
    "\t\t\t\t\ta = m;\n"
    "\t\t\t}\n"
    "\t\t\tfloat4 h = rv_project(b, s);\n"
    "\t\t\tfloat2 e = min(h.xy, 1.0f - h.xy);\n"
    "\t\t\tfloat fade = saturate(min(e.x, e.y) * 8.0f) * (1.0f - i / 24.0f);\n"
    "\t\t\treturn float4(BackTex.SampleLevel(SamplerA, h.xy, 0).xyz, fade);\n"
    "\t\t}\n"
    "\t}\n"
    "\treturn 0.0f;\n"
    "}\n"
    "#endif\n\n";

/* Inside the lit, HDR branch of PS, before the game adds its lighting up: glints, sky, our
   reflection in place of the game's. */
static const char k_water_light[] =
    "#ifdef ANIMATE_WATER_NORMALS\n"
    "\tfloat3 rvRefl = 0.0f;\n"
    "\tfloat rvAmt = 0.0f;\n"
    "\tif (rv_water_on(IN.Position.xy) > 0.5f)\n"
    "\t{\n"
    "\t\tspecular.xyz += hdrMultiplier * gLight.Diffuse.xyz * RV_GLINT * pow(saturate(dot(N, H)), 300.0f) * saturate(NdotL * 4.0f);\n"
    "\t\tspecular.xyz += hdrMultiplier * gLight.Diffuse.xyz * RV_SKY * pow(1.0f - saturate(dot(N, V)), 3.0f) * float3(0.55f, 0.7f, 0.9f);\n"
    "#ifdef HAS_SCREEN_REFLECTIONS\n"
    "\t\tfloat4 rvHit = rv_ssr(IN.ViewPos.xyz, N, V);\n"
    "\t\trvRefl = rvHit.xyz;\n"
    "\t\trvAmt = RV_REFLECT * rvHit.w;\n"
    "\t\treflection = 0.0f;\n"
    "#endif\n"
    "\t}\n"
    "#endif\n\t";

static const char k_water_mix[] =
    "\n#ifdef ANIMATE_WATER_NORMALS\n"
    "\tcolor.xyz = lerp(color.xyz, rvRefl, rvAmt);\n"
    "\tcolor.a = lerp(color.a, 1.0f, rvAmt);\n"
    "#if RV_DEBUG\n" /* magenta: a reflection; green: reflections on, none here; yellow: off */
    "#ifdef HAS_SCREEN_REFLECTIONS\n"
    "\tcolor = rvAmt > 0.0f ? float4(4.0f, 0.0f, 4.0f, 1.0f) : float4(0.0f, 3.0f, 0.0f, 1.0f);\n"
    "#else\n"
    "\tcolor = float4(3.0f, 3.0f, 0.0f, 1.0f);\n"
    "#endif\n"
    "#endif\n"
    "#endif";

/* [debug] water_debug=2: every pixel of the uber shader in a colour from its permutation, to see
   which one a surface is drawn with (red ENABLE_LIGHTING, green USE_NORMAL_MAP, bright green
   ANIMATE_WATER_NORMALS, blue HAS_SCREEN_REFLECTIONS; NO_HDR halves it). */
static const char k_debug_defs[] =
    "\n#if RV_DEBUG == 2\n"
    "\t{\n"
    "\t\tfloat4 dbg = float4(0.0f, 0.0f, 0.0f, 1.0f);\n"
    "#ifdef ENABLE_LIGHTING\n"
    "\t\tdbg.r = 4.0f;\n"
    "#endif\n"
    "#ifdef USE_NORMAL_MAP\n"
    "\t\tdbg.g = 1.0f;\n"
    "#endif\n"
    "#ifdef ANIMATE_WATER_NORMALS\n"
    "\t\tdbg.g = 4.0f;\n"
    "#endif\n"
    "#ifdef HAS_SCREEN_REFLECTIONS\n"
    "\t\tdbg.b = 4.0f;\n"
    "#endif\n"
    "#ifdef NO_HDR\n"
    "\t\tdbg.rgb *= 0.12f;\n"
    "#endif\n"
    "\t\tOUT.Color = float4(dbg.rgb, OUT.Color.a);\n"
    "\t}\n"
    "#endif\n"
    /* water_debug=3: on the water, red = the projection is perspective (_34), green = it is
       orthographic (_44), blue = a point 200 units up the reflected ray is still on screen */
    "#if RV_DEBUG == 3 && defined(ANIMATE_WATER_NORMALS) && defined(HAS_SCREEN_REFLECTIONS)\n"
    "\t{\n"
    "\t\tfloat4 q = rv_project(IN.ViewPos.xyz + normalize(-reflect(normalize(-IN.ViewPos.xyz), normalize(IN.Normal))) * 200.0f, 1.0f);\n"
    "\t\tbool on = q.x >= 0.0f && q.x <= 1.0f && q.y >= 0.0f && q.y <= 1.0f && q.w > 0.0f;\n"
    "\t\tOUT.Color = float4(abs(gProjection._34) > 0.5f ? 4.0f : 0.0f, abs(gProjection._44) > 0.5f ? 4.0f : 0.0f, on ? 4.0f : 0.0f, 1.0f);\n"
    "\t}\n"
    "#endif\n"
    /* water_debug=4: on the water, red = the stored depth under it is neither 0 nor 1, green =
       it is farther than the water's own depth, blue = ViewPos.z > 0 */
    "#if RV_DEBUG == 4 && defined(ANIMATE_WATER_NORMALS) && defined(HAS_SCREEN_REFLECTIONS)\n"
    "\t{\n"
    "\t\tfloat d = DepthTex.SampleLevel(SamplerA, IN.Position.xy / gScreenSize, 0).x;\n"
    "\t\tOUT.Color = float4(d > 0.0001f && d < 0.9999f ? 4.0f : 0.0f, d > IN.Position.z ? 4.0f : 0.0f, IN.ViewPos.z > 0.0f ? 4.0f : 0.0f, 1.0f);\n"
    "\t}\n"
    "#endif";

typedef struct {
    const char *anchor; /* found once in the game's file */
    int after;          /* 0: the text goes in front of the anchor, 1: after it */
    const char *text;   /* NULL: our functions (k_common, the values, `funcs`) */
} edit_t;

static const edit_t k_tonemap_edits[] = {
    { "PS_OUTPUT PS(", 0, NULL },
    { "OUT.Color = float4(lerp(desaturated, color.xyz, gSaturation), 1.0f) * finalTint;", 1,
      "\n\tOUT.Color = rv_grade(OUT.Color, IN.UV);" },
};
static const edit_t k_fxaa_edits[] = {
    { "PS_OUTPUT PS(", 0, NULL },
    { "OUT.Color = float4(FxaaPixelShader(pos, tex, rcpFrame), 1.0f);", 1,
      "\n\tOUT.Color.rgb = rv_sharpen(OUT.Color.rgb, pos, rcpFrame);" },
};
static const edit_t k_global_edits[] = {
    { "PS_OUTPUT PS( PS_INPUT IN )", 0, NULL },
    { "float3 readNormal = nA.xyz + nC.xyz + nD.xyz;", 1,
      "\n\treadNormal.xy *= lerp(1.0f, RV_WAVES, rv_water_on(IN.Position.xy));" },
    { "color.xyz = pow(color.xyz, 1.45f);", 0, k_water_light },
    { "color.xyz = color.xyz * (diffuse.xyz + ambient.xyz) + specular.xyz + reflection.xyz;", 1, k_water_mix },
    { "OUT.Color = max(0.0f, color);", 1, k_debug_defs },
};

static const struct {
    const char *name;
    const char *funcs;
    const edit_t *edits;
    int nedits;
} k_shaders[3] = {
    { "ps_tonemap.hlsl", k_grade, k_tonemap_edits, 2 },
    { "ps_fxaa.hlsl", k_sharpen, k_fxaa_edits, 2 },
    { "ps_global.hlsl", k_water, k_global_edits, 5 },
};
#define NSHADERS 3
static char g_out[NSHADERS][MAX_PATH]; /* our versions, served in place of the game's */
static int g_made[NSHADERS];

static char *read_all(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = n > 0 && n < (1 << 20) ? malloc(n + 1) : NULL;
    if (buf && (long)fread(buf, 1, n, f) != n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf) {
        buf[n] = 0;
        *len = n;
    }
    return buf;
}

static int write_all(const char *path, const char *text, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    int ok = fwrite(text, 1, n, f) == n;
    return (fclose(f) == 0) & ok;
}

/* The game's shader i with our code in it, written to g_out[i]. */
static int make_shader(int i, const char *dir)
{
    char path[MAX_PATH];
    long n;
    snprintf(path, sizeof path, "%sshaders\\%s", g_dir, k_shaders[i].name);
    char *src = read_all(path, &n);
    if (!src) {
        logf_("postfx: %s not found", path);
        return 0;
    }
    char defs[768];
    snprintf(defs, sizeof defs,
             "#define RV_SHARPEN %.4ff\n#define RV_VIBRANCE %.4ff\n#define RV_CONTRAST %.4ff\n"
             "#define RV_WARMTH %.4ff\n#define RV_BLACK %.4ff\n#define RV_VIGNETTE %.4ff\n#define RV_FIX_FXAA %d\n"
             "#define RV_REFLECT %.4ff\n#define RV_GLINT %.4ff\n#define RV_SKY %.4ff\n#define RV_WAVES %.4ff\n"
             "#define RV_SSR_STEP 20.0f\n#define RV_SSR_THICK 200.0f\n#define RV_DEBUG %d\n",
             cfg.sharpen, cfg.vibrance, cfg.contrast, cfg.warmth, cfg.black, cfg.vignette, cfg.fix_fxaa ? 1 : 0,
             cfg.reflect, cfg.glint, cfg.sky, cfg.waves, g_water_debug);
    /* each edit splices into the text built so far; every anchor must be there exactly once */
    for (int e = 0; e < k_shaders[i].nedits; e++) {
        const edit_t *ed = &k_shaders[i].edits[e];
        char *at = strstr(src, ed->anchor);
        if (!at || strstr(at + 1, ed->anchor)) {
            logf_("postfx: %s is not the version we know, it stays the game's", k_shaders[i].name);
            free(src);
            return 0;
        }
        if (ed->after)
            at += strlen(ed->anchor);
        char *ins = ed->text ? (char *)ed->text : NULL, *made = NULL;
        if (!ins) {
            size_t m = strlen(k_common) + strlen(defs) + strlen(k_shaders[i].funcs) + 1;
            made = ins = malloc(m);
            snprintf(ins, m, "%s%s%s", k_common, defs, k_shaders[i].funcs);
        }
        size_t head = at - src, add = strlen(ins);
        char *out = malloc(n + add + 1);
        memcpy(out, src, head);
        memcpy(out + head, ins, add);
        memcpy(out + head + add, at, n - head + 1);
        free(made);
        free(src);
        src = out;
        n += (long)add;
    }
    snprintf(g_out[i], sizeof g_out[i], "%s%s", dir, k_shaders[i].name);
    int ok = write_all(g_out[i], src, n);
    free(src);
    if (!ok)
        logf_("postfx: could not write %s", g_out[i]);
    return ok;
}

/* At DLL load, before override_install: write the shaders and send the game to them. */
void postfx_init(void)
{
    if (!g_postfx_enabled)
        return;
    if (!g_override_enabled) {
        logf_("postfx: needs file_overrides=1, the look is off");
        g_postfx_enabled = 0;
        return;
    }
    char dir[MAX_PATH];
    DWORD n = GetTempPathA(sizeof dir, dir);
    if (n == 0 || n >= sizeof dir - 32) {
        g_postfx_enabled = 0;
        return;
    }
    strcat(dir, "RON Revived\\");
    CreateDirectoryA(dir, NULL);
    strcat(dir, "shaders\\");
    CreateDirectoryA(dir, NULL);
    int any = 0;
    for (int i = 0; i < NSHADERS; i++) {
        if (i == 2 && !cfg.water)
            continue;
        if (!make_shader(i, dir))
            continue;
        char rel[64];
        snprintf(rel, sizeof rel, "shaders\\%s", k_shaders[i].name);
        override_redirect(rel, g_out[i]);
        g_made[i] = any = 1;
    }
    if (!any) {
        g_postfx_enabled = 0;
        return;
    }
    logf_("postfx: sharpen %.2f, vibrance %.2f, contrast %.2f, warmth %.2f, black %.3f, vignette %.2f, fxaa fix %d; "
          "water %s (reflections %.2f, glints %.2f, sky %.2f, waves %.2f)",
          cfg.sharpen, cfg.vibrance, cfg.contrast, cfg.warmth, cfg.black, cfg.vignette, cfg.fix_fxaa,
          g_made[2] ? "on" : "off", cfg.reflect, cfg.glint, cfg.sky, cfg.waves);
}

/* The uber shader is compiled per permutation: try the water's two (d3dgl's "Normal Water" with
   reflections, "Min Water" without) and a plain one; the post shaders have one each. */
static const char *const k_water_defs[][16] = {
    { "PHOENIX_SHADER_COMPILE", "ENABLE_LIGHTING", "USE_UV0", "HAS_TEX0", "USE_NORMAL_MAP", "ANIMATE_WATER_NORMALS",
      "SPEC_MODEL_TORRANCE", "HAS_SCREEN_REFLECTIONS", NULL },
    { "PHOENIX_SHADER_COMPILE", "ENABLE_LIGHTING", "USE_UV0", "HAS_TEX0", "USE_NORMAL_MAP", "ANIMATE_WATER_NORMALS",
      "SPEC_MODEL_TORRANCE", NULL },
    { "PHOENIX_SHADER_COMPILE", "ENABLE_LIGHTING", "USE_UV0", "HAS_TEX0", NULL },
    { "PHOENIX_SHADER_COMPILE", "USE_UV0", "HAS_TEX0", "ENABLE_VERTEX_COLOR_DIFFUSE", NULL },
};

static int try_compile(d3dcompile_t compile, int i, const char *text, long n, const char *const *defs)
{
    char name[MAX_PATH];
    /* the game's path as the name: its includes are found next to it */
    snprintf(name, sizeof name, "%sshaders\\%s", g_dir, k_shaders[i].name);
    const char *macros[2 * 16 + 2] = { 0 };
    int m = 0;
    for (; defs && defs[m / 2] && m < 32; m += 2) {
        macros[m] = defs[m / 2];
        macros[m + 1] = "1";
    }
    void *code = NULL, *err = NULL;
    long hr = compile(text, n, name, m ? macros : NULL, (void *)1 /* D3D_COMPILE_STANDARD_FILE_INCLUDE */, "PS",
                      "ps_4_0", 0, 0, &code, &err);
    int ok = hr >= 0 && code;
    if (!ok)
        logf_("postfx: %s does not compile (%08lX), the game's own is used: %.300s", k_shaders[i].name,
              (unsigned long)hr, err ? (const char *)((blob_ptr_t)COM_FN(err, BLOB_PTR))(err) : "");
    if (err)
        ((release_t)COM_FN(err, 2))(err);
    if (code)
        ((release_t)COM_FN(code, 2))(code);
    return ok;
}

/* Our files compile? Done once before the display (and d3dgl's shaders) is made, outside the
   loader lock. If one does not, the game's own text goes into it: the game's look, no error. */
static void check_compile(void)
{
    HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
    d3dcompile_t compile = dc ? (d3dcompile_t)GetProcAddress(dc, "D3DCompile") : NULL;
    if (!compile)
        return; /* d3dgl could not compile them either */
    for (int i = 0; i < NSHADERS; i++) {
        if (!g_made[i])
            continue;
        long n;
        char *text = read_all(g_out[i], &n);
        if (!text)
            continue;
        int ok = 1;
        if (i == 2)
            for (int p = 0; ok && p < (int)(sizeof k_water_defs / sizeof k_water_defs[0]); p++)
                ok = try_compile(compile, i, text, n, k_water_defs[p]);
        else
            ok = try_compile(compile, i, text, n, NULL);
        free(text);
        if (ok) {
            logf_("postfx: %s compiles", k_shaders[i].name);
            continue;
        }
        char name[MAX_PATH];
        snprintf(name, sizeof name, "%sshaders\\%s", g_dir, k_shaders[i].name);
        char *orig = read_all(name, &n);
        if (orig) {
            write_all(g_out[i], orig, n);
            free(orig);
        }
    }
}

/* ---- the keys and the switch --------------------------------------------------------------- */

static int key_edge(int vk, int *was)
{
    int down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(vk) & 0x8000);
    int edge = down && !*was;
    *was = down;
    return edge;
}

static void read_keys(void)
{
    static int was_off, was_split;
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    if (!cfg.keys || pid != GetCurrentProcessId())
        return;
    if (key_edge(VK_F9, &was_off)) {
        g_mode[0] = g_mode[0] > 0.5f ? 0.0f : 1.0f;
        logf_("postfx: Ctrl+F9, the look %s", g_mode[0] > 0.5f ? "off (the game's own)" : "on");
    }
    if (key_edge(VK_F10, &was_split)) {
        g_mode[1] = g_mode[1] > 0.5f ? 0.0f : 1.0f;
        logf_("postfx: Ctrl+F10, comparison %s", g_mode[1] > 0.5f ? "on (left the game's, right ours)" : "off");
    }
}

static void bind_switch(void)
{
    char *sys = *(char **)VA(VA_GRAPHSYS);
    void *dc = sys ? *(void **)(sys + SYS_CONTEXT) : NULL;
    if (!dc)
        return;
    void *dev = NULL;
    ((get_device_t)COM_FN(dc, DC_GET_DEVICE))(dc, &dev);
    if (dev)
        ((release_t)COM_FN(dev, 2))(dev); /* the context keeps it alive */
    if (dev != g_dev) {
        if (g_cb)
            ((release_t)COM_FN(g_cb, 2))(g_cb);
        g_cb = NULL;
        g_dev = dev;
        g_have[0] = -1.0f;
        UINT bd[6] = { 16, 0, 4, 0, 0, 0 }; /* D3D11_BUFFER_DESC: 16 bytes, DEFAULT, CONSTANT_BUFFER */
        if (!dev || ((create_buffer_t)COM_FN(dev, DEV_CREATE_BUFFER))(dev, bd, NULL, &g_cb) < 0)
            g_cb = NULL;
    }
    if (!g_cb)
        return; /* unbound reads 0: the look stays on */
    if (memcmp(g_have, g_mode, sizeof g_mode)) {
        ((update_sub_t)COM_FN(dc, DC_UPDATE_SUB))(dc, g_cb, 0, NULL, g_mode, 0, 0);
        memcpy(g_have, g_mode, sizeof g_mode);
    }
    ((cb_t)COM_FN(dc, DC_PS_SET_CB))(dc, 7, 1, &g_cb);
}

static void __attribute__((thiscall)) pf_do_run(void *self, int bloom)
{
    read_keys();
    bind_switch();
    g_real_run(self, bloom);
}

/* ---- test pictures ([debug] postfx_shots, for development) ---------------------------------
   Once a game is on screen: the comparison (left the game's look, right ours) at the start
   position and at points around it, each saved from the screen target right after FXAA (the
   scene without the interface) to %TEMP%\RON Revived\shots\, then the game quits. */

#define D3DGL_AA_VTBL 0x100E44ACu /* D3D11AntiAlias vtable: +0 run() */
#define D3DGL_AA_RUN  0x100186D0u
#define SYS_SCREEN_RT 0xCF4       /* D3D11GLBuffer of the screen target: +0x30 its RTV */
#define DC_MAP           14
#define DC_UNMAP         15
#define DC_COPY_RESOURCE 47
#define DEV_CREATE_TEX2D 5

typedef void(__attribute__((thiscall)) *aa_run_t)(void *self);
typedef long(__stdcall *create_tex_t)(void *dev, const UINT *desc, const void *init, void **tex);
typedef void(__stdcall *copy_res_t)(void *dc, void *dst, void *src);
typedef long(__stdcall *map_t)(void *dc, void *res, UINT sub, UINT type, UINT flags, void *mapped);
typedef void(__stdcall *unmap_t)(void *dc, void *res, UINT sub);
typedef void(__stdcall *get_res_t)(void *view, void **res);
typedef void(__stdcall *tex_desc_t)(void *tex, UINT *desc);

static aa_run_t g_real_aa;

static int save_screen(const char *path)
{
    char *sys = *(char **)VA(VA_GRAPHSYS);
    void *dc = sys ? *(void **)(sys + SYS_CONTEXT) : NULL;
    void *rtv = sys ? *(void **)(sys + SYS_SCREEN_RT + 0x30) : NULL;
    if (!dc || !rtv)
        return 0;
    void *tex = NULL, *dev = NULL, *stage = NULL;
    ((get_res_t)COM_FN(rtv, 7))(rtv, &tex);
    if (!tex)
        return 0;
    UINT d[11];
    ((tex_desc_t)COM_FN(tex, 10))(tex, d);
    UINT w = d[0], h = d[1], fmt = d[4];
    d[2] = d[3] = 1;       /* one mip, one slice */
    d[5] = 1, d[6] = 0;    /* no multisampling */
    d[7] = 3;              /* D3D11_USAGE_STAGING */
    d[8] = 0;              /* no bind flags */
    d[9] = 0x20000;        /* CPU read */
    d[10] = 0;
    ((get_device_t)COM_FN(dc, DC_GET_DEVICE))(dc, &dev);
    int ok = 0;
    if (dev && ((create_tex_t)COM_FN(dev, DEV_CREATE_TEX2D))(dev, d, NULL, &stage) >= 0 && stage) {
        ((copy_res_t)COM_FN(dc, DC_COPY_RESOURCE))(dc, stage, tex);
        struct {
            unsigned char *data;
            UINT row, depth;
        } m;
        if (((map_t)COM_FN(dc, DC_MAP))(dc, stage, 0, 1 /* READ */, 0, &m) >= 0) {
            FILE *f = fopen(path, "wb");
            if (f) {
                /* 24-bit BMP, bottom-up */
                UINT row = (w * 3 + 3) & ~3u, size = 54 + row * h;
                unsigned char hd[54] = { 'B', 'M' };
                *(UINT *)(hd + 2) = size, *(UINT *)(hd + 10) = 54, *(UINT *)(hd + 14) = 40;
                *(INT32 *)(hd + 18) = (INT32)w, *(INT32 *)(hd + 22) = (INT32)h;
                *(unsigned short *)(hd + 26) = 1, *(unsigned short *)(hd + 28) = 24;
                *(UINT *)(hd + 34) = row * h;
                fwrite(hd, 1, 54, f);
                unsigned char *line = calloc(row, 1);
                for (UINT y = h; y-- > 0;) {
                    const unsigned char *p = m.data + (size_t)y * m.row;
                    for (UINT x = 0; x < w; x++, p += 4) {
                        unsigned r, g, b;
                        if (fmt == 24) { /* R10G10B10A2 */
                            UINT v = *(const UINT *)p;
                            r = (v & 1023) >> 2, g = ((v >> 10) & 1023) >> 2, b = ((v >> 20) & 1023) >> 2;
                        } else if (fmt == 87 || fmt == 91) /* B8G8R8A8 */
                            b = p[0], g = p[1], r = p[2];
                        else /* R8G8B8A8 */
                            r = p[0], g = p[1], b = p[2];
                        line[x * 3] = (unsigned char)b, line[x * 3 + 1] = (unsigned char)g, line[x * 3 + 2] = (unsigned char)r;
                    }
                    fwrite(line, 1, row, f);
                }
                free(line);
                ok = fclose(f) == 0;
            }
            ((unmap_t)COM_FN(dc, DC_UNMAP))(dc, stage, 0);
        }
        ((release_t)COM_FN(stage, 2))(stage);
    }
    if (dev)
        ((release_t)COM_FN(dev, 2))(dev);
    ((release_t)COM_FN(tex, 2))(tex);
    logf_("postfx shots: %s %ux%u format %u -> %s", ok ? "saved" : "FAILED", w, h, fmt, path);
    return ok;
}

#define VA_OBJECTS   0x00C0618Cu /* Objects *: +0x14 + 0x1C * p = Object **, +0x184 + 4 * p building count */
#define OBJS_BUILD_N 0x184
#define FIRST_BUILD  2000
#define COORD_KEY    0x63637     /* object x, y (+0x10, +0x14) are stored xor this, world units */

/* Spots to look at: one building of every group at least a screen apart, any player. */
static int find_spots(float (*xy)[2], int max)
{
    char *objs = *(char **)VA(VA_OBJECTS);
    int n = 0;
    for (int p = 0; objs && p < 8 && n < max; p++) {
        char **arr = *(char ***)(objs + 0x14 + 0x1C * p);
        int count = *(int *)(objs + OBJS_BUILD_N + 4 * p);
        for (int id = FIRST_BUILD; arr && id < count && n < max; id++) {
            char *o = arr[id];
            if (!o || !(o[8] & 1))
                continue;
            float x = (float)(*(int *)(o + 0x10) ^ COORD_KEY), y = (float)(*(int *)(o + 0x14) ^ COORD_KEY);
            int crowded = 0;
            for (int k = 0; k < n && !crowded; k++)
                crowded = fabsf(xy[k][0] - x) < 2500.0f && fabsf(xy[k][1] - y) < 2500.0f;
            if (!crowded)
                xy[n][0] = x, xy[n][1] = y, n++;
        }
    }
    return n;
}

static void __attribute__((thiscall)) pf_aa_run(void *self)
{
    g_real_aa(self);
    /* each spot twice, the game's look ("old") and ours ("new"), the same view */
    static int frame, step, spots;
    static float xy[12][2];
    static DWORD start;
    enum { SETTLE = 60 };
    if (!start) {
        start = GetTickCount();
        return;
    }
    if (GetTickCount() - start < 8000)
        return; /* let the start settle */
    if (!spots) {
        spots = find_spots(xy, 12);
        logf_("postfx shots: %d spots with buildings", spots);
        /* few buildings (the main menu's map has none): the view and 8 points a screen around it */
        float x0, y0;
        if (spots < 4 && camera_get_loc(&x0, &y0))
            for (int k = 0; k < 9 && spots < 12; k++, spots++) {
                float r = k ? 3500.0f : 0.0f, a = (k - 1) * 0.7854f;
                xy[spots][0] = x0 + r * cosf(a), xy[spots][1] = y0 + r * sinf(a);
            }
        if (!spots)
            return;
    }
    int spot = step / 2, ours = step & 1;
    if (frame == 0) {
        if (!ours)
            camera_set_loc(xy[spot][0], xy[spot][1]);
        g_mode[0] = ours ? 0.0f : 1.0f;
        g_mode[1] = 0.0f;
    }
    if (++frame < SETTLE)
        return;
    char dir[MAX_PATH], path[MAX_PATH];
    GetTempPathA(sizeof dir, dir);
    strcat(dir, "RON Revived\\shots\\");
    CreateDirectoryA(dir, NULL);
    snprintf(path, sizeof path, "%sspot%02d_%s.bmp", dir, spot, ours ? "new" : "old");
    save_screen(path);
    frame = 0;
    if (++step >= 2 * spots) {
        logf_("postfx shots: done, quitting");
        ExitProcess(0);
    }
}

static void hook_shots(UINT_PTR delta)
{
    void **slot = (void **)(D3DGL_AA_VTBL + delta);
    if ((UINT_PTR)*slot != D3DGL_AA_RUN + delta) {
        logf_("postfx shots: anti-alias vtable differs, no pictures");
        return;
    }
    DWORD old;
    if (VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) {
        g_real_aa = (aa_run_t)*slot;
        *slot = (void *)pf_aa_run;
        VirtualProtect(slot, sizeof *slot, old, &old);
        logf_("postfx shots: on (Anti-Aliasing must be on)");
    }
}

/* Pre-hook on GraphSys::init_display_quick (ui_scale.c): d3dgl is loaded, its shaders are not
   made yet. */
void postfx_display(void)
{
    static int done;
    if (done || !g_postfx_enabled)
        return;
    done = 1;
    check_compile();
    unsigned char *dll = (unsigned char *)GetModuleHandleA("d3dgl.dll");
    if (!dll)
        return;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(dll + ((IMAGE_DOS_HEADER *)dll)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != D3DGL_TIMESTAMP) {
        logf_("postfx: unknown d3dgl.dll, the look stays on without Ctrl+F9 / Ctrl+F10");
        return;
    }
    UINT_PTR delta = (UINT_PTR)dll - D3DGL_PREF_BASE;
    void **slot = (void **)(D3DGL_TONEMAP_VTBL + delta) + 1;
    if ((UINT_PTR)*slot != D3DGL_TONEMAP_RUN + delta) {
        logf_("postfx: tone mapper vtable differs, no Ctrl+F9 / Ctrl+F10");
        return;
    }
    DWORD old;
    if (VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old)) {
        g_real_run = (do_run_t)*slot;
        *slot = (void *)pf_do_run;
        VirtualProtect(slot, sizeof *slot, old, &old);
        logf_("postfx: Ctrl+F9 look off / on, Ctrl+F10 comparison");
    }
    if (g_shots)
        hook_shots(delta);
}
