/*
 * Sub-pixel placement of building sprites.
 *
 * Buildings are 2D sprites composited over the 3D terrain. BuildOut::draw_underlays
 * projects the building's position with MiscAccess::world_to_console, which ROUNDS the
 * result to whole pixels; SpriteMethod::execute then subtracts the sprite's hotspot
 * offset TRUNCATED to int, and Sprite::draw takes ints. The terrain moves with sub-pixel
 * precision, so during a smooth zoom or scroll every building jumps from pixel to pixel
 * against it (1.5 physical pixels at ui_scale 1.5): the "vibration" that remained after
 * the zoom itself was made continuous.
 *
 * Fix: around GraphicPieces::draw_build the same projection is done in float (same
 * matrix and viewport maths as world_to_console, same hotspot term as SpriteMethod) and
 * the fraction the game dropped is kept. d3dgl's D3D11GLSprite builds the sprite's quad
 * from the ints; right before clip_verts_ordered (hook shared with ui_scale.c) that
 * fraction is added to the quad's vertices. The texture, its scale and the draw order
 * stay the game's.
 *
 * Sprite::draw adds one more truncated term: the transparent margin the loader cut off the
 * sprite, scaled by an integer division. The game's art has none; the nations' new art
 * (nation_art.c) has up to 90 px, so only those buildings still shook while zooming.
 */
#include "ron.h"
#include <math.h>
#include <stdio.h>

#define VA_ACTIVE_CAMERA 0x00E335A8u /* BaseCamera::active_camera */
#define VA_GRAPHSYS      0x00E335A0u /* GraphSys *graphsys */
#define VA_CONSOLE       0x00C06210u /* MiscAccess::console */
#define VA_CAMERA        0x00C06200u /* MiscAccess::camera */
#define VA_GAME          0x00C061E8u /* GameAccessConst::gamec */
#define VA_SPRITEPKT_VFT 0x00B55860u

#define CAM_VIEWPROJ    0x240 /* BaseCamera: 4x4 float matrix used by world_to_console */
#define CAM_DIST        0x2F0
#define CAM_DIST_CLOSE  0x338 /* level_dist[6], the closest zoom level */
#define GS_LOG_W        0x360 /* logical screen size */
#define GS_LOG_H        0x364
#define GS_HEIGHT_K     0x16C /* float factor applied to the logical height */
#define CONSOLE_Y_OFF   0x12A /* int16 added to every projected y */
#define GAME_DIST_K     0x928 /* float: draw_build's scale numerator = closest * this */
#define CHAD_WORLD      0x2C  /* GraphicChad: float x, y, z (the point passed to world_to_console) */
#define CHAD_SCREEN_X   0x5C  /* int, rounded by world_to_console */
#define CHAD_SCREEN_Y   0x60  /* int, rounded + CONSOLE_Y_OFF */
#define RDATA_SPRITEPKT 0x44
#define SPKT_SPRITE     0x08 /* Sprite * */
#define SPKT_OFF_X      0x10 /* int hotspot offset, SpritePacket::set_offset */
#define SPKT_OFF_Y      0x14
#define SPRITE_TRIM_X   0x48 /* fully transparent columns cut off the left at load (Sprite::init_32) */
#define SPRITE_TRIM_Y   0x4C /* fully transparent rows cut off the top */

int g_subpixel_enabled;
int g_dist_patch_applied; /* draw_build passes the zoom distance as a float (ron_camera.c) */

static float g_sub_x, g_sub_y; /* logical pixels to add to the current sprite's quad */
static int g_sub_active;
static int g_logged;

void subpixel_load_config(const char *ini_path)
{
    g_subpixel_enabled = GetPrivateProfileIntA("camera", "subpixel_sprites", 1, ini_path);
}

/* Before GraphicPieces::draw_build(chad, rdata): the fraction between the sprite position
   the game is about to use (ints) and the exact one. Called after nation_art.c has put its
   sprite in the packet, since the trim term depends on the sprite drawn. */
void subpixel_begin(const char *chad, const char *rdata)
{
    g_sub_active = 0;
    if (!g_subpixel_enabled || !chad || !rdata)
        return;
    const char *cam = *(const char **)VA(VA_ACTIVE_CAMERA);
    const char *gs = *(const char **)VA(VA_GRAPHSYS);
    const char *con = *(const char **)VA(VA_CONSOLE);
    const char *mcam = *(const char **)VA(VA_CAMERA);
    const char *game = *(const char **)VA(VA_GAME);
    if (!cam || !gs || !con || !mcam || !game)
        return;

    /* MiscAccess::world_to_console, in the same order of float operations. */
    const float *m = (const float *)(cam + CAM_VIEWPROJ);
    const float *p = (const float *)(chad + CHAD_WORLD);
    float x = p[0], y = p[1], z = p[2];
    float cx = m[4] * y + m[0] * x + m[8] * z + m[12];
    float cy = m[5] * y + m[1] * x + m[9] * z + m[13];
    float cw = m[7] * y + m[3] * x + m[11] * z + m[15];
    if (cw != 0.0f && cw != 1.0f) {
        float inv = 1.0f / cw;
        cx *= inv;
        cy *= inv;
    }
    int w = *(const int *)(gs + GS_LOG_W);
    int h = (int)((float)*(const int *)(gs + GS_LOG_H) * *(const float *)(gs + GS_HEIGHT_K));
    float sx = (cx + 1.0f) * (float)w * 0.5f;
    float sy = (1.0f - cy) * (float)h * 0.5f;

    int yoff = *(const short *)(con + CONSOLE_Y_OFF);
    int gx = *(const int *)(chad + CHAD_SCREEN_X), gy = *(const int *)(chad + CHAD_SCREEN_Y) - yoff;
    float fx = sx - (float)gx, fy = sy - (float)gy;
    if (!(fabsf(fx) <= 1.0f && fabsf(fy) <= 1.0f)) {
        /* Not the point the game projected (or a stale chad): leave this sprite alone. */
        if (g_logged < 5) {
            g_logged++;
            logf_("subpixel: projection mismatch, game (%d, %d) vs %.2f, %.2f (world %.1f %.1f %.1f)", gx, gy, sx,
                  sy, x, y, z);
        }
        return;
    }

    /* SpriteMethod::execute: x -= (int)(offset_x * closest / dist), same for y. */
    const char *pkt = *(const char **)(rdata + RDATA_SPRITEPKT);
    if (pkt && *(const UINT_PTR *)pkt == VA(VA_SPRITEPKT_VFT)) {
        float closest = (float)(int)*(const float *)(mcam + CAM_DIST_CLOSE) * *(const float *)(game + GAME_DIST_K);
        float d = *(const float *)(mcam + CAM_DIST);
        float dist = g_dist_patch_applied ? d : (float)(int)d;
        if (dist > 0.0f && closest > 0.0f) {
            float ratio = closest / dist;
            float ox = (float)*(const int *)(pkt + SPKT_OFF_X) * ratio;
            float oy = (float)*(const int *)(pkt + SPKT_OFF_Y) * ratio;
            fx -= ox - (float)(int)ox;
            fy -= oy - (float)(int)oy;

            /* Sprite::draw: x += trim_x * (int)closest / (int)dist, an integer division (the
               scale SpriteMethod::execute gives GraphSys, vtable +0xF4). The game's own sprites
               are cut to the edge (trim 0); ours keep the original canvas around smaller art,
               up to 90 px, so they stepped a pixel at a time while zooming. */
            const char *spr = *(const char **)(pkt + SPKT_SPRITE);
            int ci = (int)closest, di = (int)dist;
            if (spr && ci > 0 && di > 0) {
                int tx = *(const int *)(spr + SPRITE_TRIM_X), ty = *(const int *)(spr + SPRITE_TRIM_Y);
                fx += (float)tx * (float)ci / (float)di - (float)(tx * ci / di);
                fy += (float)ty * (float)ci / (float)di - (float)(ty * ci / di);
            }
        }
    }
    g_sub_x = fx;
    g_sub_y = fy;
    g_sub_active = 1;
}

void subpixel_end(void)
{
    g_sub_active = 0;
}

/* d3dgl, before D3D11GLSprite::clip_verts_ordered: the sprite's quad(s), n vertices of
   (x, y, z, w) in logical pixels. Only while a building is being drawn. */
void subpixel_apply(float *verts, int n)
{
    if (!g_sub_active || !verts)
        return;
    for (int i = 0; i < n; i++) {
        verts[i * 4] += g_sub_x;
        verts[i * 4 + 1] += g_sub_y;
    }
}
