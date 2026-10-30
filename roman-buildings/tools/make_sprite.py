"""Turns an AI-edited building image (on a flat magenta background) into a game sprite.

The game's building .tga uses the alpha channel as a category, not as transparency:
    0 = empty, 255 = building, 248 = player colour (grey, tinted in game), 145 = shadow (black)
so edges cannot be anti-aliased with alpha; they are decided per pixel.

    python make_sprite.py <edited.jpg|png> <original.tga> <out_dir> <name> [dy] [cloth_hue] [--shadow]

No ground shadow unless --shadow is given (the user's rule, 2026-10-27).

Writes <name>_A.tga (red stays red), <name>_B.tga (red cloth becomes player colour) and
previews of both, tinted blue and red, next to the original.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageChops, ImageFilter

import color_match

MAGENTA = (255, 0, 255)  # replaced by the image's own background colour, sampled at its corners
SHADOW_LEN = 5  # pixels, at the game's sprite size
DY = 0  # vertical shift of the fitted content, set from the command line
TEAM, SHADOW, SOLID = 248, 145, 255
MIN_TEAM_AREA = 12  # smallest red area (pixels) that becomes player colour
CLOTH_HUE = 0  # PIL hue (0-255), 0 = off. Set (e.g. 6) when red brick or tiles turn into player
               # colour: cloth red is below it, terracotta and brick above. Command line arg 6.


def background(img):
    """Mean colour of the four corners: Gemini's "magenta" is sometimes a duller purple."""
    W, H = img.size
    px = [img.getpixel((x, y)) for cx, cy in ((0, 0), (W - 16, 0), (0, H - 16), (W - 16, H - 16))
          for x in range(cx, cx + 16) for y in range(cy, cy + 16)]
    return tuple(sum(p[i] for p in px) // len(px) for i in range(3))


def key_alpha(img):
    """Soft alpha from the distance to magenta (1 = object)."""
    r, g, b = img.split()
    # magenta has high R and B and low G: "magentaness" = min(R, B) - G
    m = ImageChops.subtract(ImageChops.darker(r, b), g)
    # thresholds scale with the background's own magentaness (pure magenta: about 40..160)
    mb = min(MAGENTA[0], MAGENTA[2]) - MAGENTA[1]
    lo, span = mb // 6, mb // 2
    return m.point(lambda v: 255 - min(255, max(0, (v - lo) * 255 // span)))


def unmix(img, alpha):
    """Removes the magenta that bled into edge pixels: c = (p - (1 - a) * M) / a."""
    src, a = img.load(), alpha.load()
    out = img.copy()
    o = out.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            av = a[x, y] / 255
            if 0.05 < av < 0.98:
                p = src[x, y]
                o[x, y] = tuple(max(0, min(255, round((p[i] - (1 - av) * MAGENTA[i]) / av))) for i in range(3))
    return out


def despill(rgb, body, band=3):
    """Magenta left in edge pixels (unmix cannot remove it all from dark JPEG outlines) shows
    in game as a pink halo. Near the outline, pull red and blue down to green wherever both
    exceed it: magenta spill raises both, real reds (cloth, tiles) raise only red."""
    near = body.point(lambda v: 255 - v).filter(ImageFilter.MaxFilter(2 * band + 1))
    rp, np_, bp = rgb.load(), near.load(), body.load()
    W, H = rgb.size
    for y in range(H):
        for x in range(W):
            if bp[x, y] and np_[x, y]:
                r, g, b = rp[x, y]
                spill = min(r, b) - g
                if spill > 0:
                    rp[x, y] = (r - spill, g, b - spill)


def drop_specks(mask, min_area, hsv=None):
    """Keeps only connected areas of at least min_area pixels: cloth and flags stay, single
    red pixels in roof tiles or bricks do not turn into player colour. With hsv, an area is
    also dropped unless most of it has the deep red hue of cloth (terracotta tiles and bricks
    are a few steps more orange)."""
    hp = hsv.load() if hsv else None
    W, H = mask.size
    mp = mask.load()
    seen = set()
    out = Image.new("L", (W, H))
    op = out.load()
    for y in range(H):
        for x in range(W):
            if not mp[x, y] or (x, y) in seen:
                continue
            area, stack = [], [(x, y)]
            seen.add((x, y))
            while stack:
                cx, cy = stack.pop()
                area.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < W and 0 <= ny < H and mp[nx, ny] and (nx, ny) not in seen:
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            if hp and sum(1 for p in area if hp[p][0] < CLOTH_HUE or hp[p][0] > 246) * 2 < len(area):
                continue
            if len(area) >= min_area:
                for p in area:
                    op[p] = 255
    return out


def project_shadow(body, a, b):
    """Ground shadow of a sprite outline: each pixel at height h above the bottom of its
    column is projected to (x - h*a, ground_y - h*b). Light from the east = west shadows."""
    W, H = body.size
    bp = body.load()
    out = Image.new("L", (W, H))
    op = out.load()
    for x in range(W):
        ground = -1
        for y in range(H - 1, -1, -1):
            if bp[x, y]:
                ground = y
                break
        if ground < 0:
            continue
        for y in range(ground, -1, -1):
            if not bp[x, y]:
                continue
            h = ground - y
            sx, sy = round(x - h * a), round(ground - h * b)
            if 0 <= sx < W and 0 <= sy < H:
                op[sx, sy] = 255
    out = out.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3))  # close holes
    return ImageChops.subtract(out, body)


def main(edited, original, out_dir, name):
    os.makedirs(out_dir, exist_ok=True)
    orig = Image.open(original).convert("RGBA")
    W, H = orig.size
    oa = orig.getchannel("A")

    big = Image.open(edited).convert("RGB")
    global MAGENTA
    MAGENTA = background(big)
    a_big = key_alpha(big)
    big = unmix(big, a_big)
    # premultiply, downscale, un-premultiply so edges keep the object's colour
    pre = Image.composite(big, Image.new("RGB", big.size), a_big)
    # Fit the new content into the box the original building occupies: same scale on both
    # axes (AI images rarely keep the aspect ratio), centred, standing on the same bottom.
    nb = a_big.point(lambda v: 255 if v >= 128 else 0).getbbox()
    ob = oa.point(lambda v: 255 if v in (SOLID, TEAM) else 0).getbbox()
    nw, nh = nb[2] - nb[0], nb[3] - nb[1]
    ow, oh = ob[2] - ob[0], ob[3] - ob[1]
    # Match the original's height; the width may use the whole canvas (a tower with a wide
    # gallery would otherwise shrink to the width of a narrow original).
    s = min(oh / nh, (W - 2) / nw)
    tw, th = max(1, round(nw * s)), max(1, round(nh * s))
    ox = min(max(1, ob[0] + (ow - tw) // 2), W - 1 - tw)
    oy = ob[3] - th + DY  # DY < 0 raises the content (a frontal base sits lower than an isometric one)
    pre_fit = pre.crop(nb).resize((tw, th), Image.LANCZOS)
    a_fit = a_big.crop(nb).resize((tw, th), Image.LANCZOS)
    pre = Image.new("RGB", (W, H))
    pre.paste(pre_fit, (ox, oy))
    a = Image.new("L", (W, H))
    a.paste(a_fit, (ox, oy))
    rgb = Image.new("RGB", (W, H))
    pp, ap, rp = pre.load(), a.load(), rgb.load()
    for y in range(H):
        for x in range(W):
            av = ap[x, y]
            if av:
                rp[x, y] = tuple(min(255, v * 255 // av) for v in pp[x, y])
    body = a.point(lambda v: 255 if v >= 128 else 0)
    despill(rgb, body)

    # Shadow: project every pixel onto the ground. A pixel's height is its distance above
    # the bottom of the object in its column; its shadow lands (h*a, h*b) away from that
    # ground point. a and b are fitted to the original sprite's shadow, so tall towers get
    # long shadows and low camps short ones, in the game's light direction (west).
    # Off unless --shadow is given: the user does not want shadows on the new buildings (2026-10-27).
    osolid = oa.point(lambda v: 255 if v in (SOLID, TEAM) else 0)
    oshadow = oa.point(lambda v: 255 if v == SHADOW else 0)
    best, best_score = (0.3, 0.0), -1.0
    for ai in range(0, 31 if SHADOWS else 0):
        for bi in range(-8, 9):
            a_, b_ = ai * 0.05, bi * 0.05
            proj = project_shadow(osolid, a_, b_)
            inter = sum(1 for p, q in zip(proj.getdata(), oshadow.getdata()) if p and q)
            union = sum(1 for p, q in zip(proj.getdata(), oshadow.getdata()) if p or q)
            score = inter / union if union else 0
            if score > best_score:
                best, best_score = (a_, b_), score
    shadow = project_shadow(body, *best) if SHADOWS else Image.new("L", (W, H))

    # Player colour candidates: saturated red cloth, banners, shields.
    hsv = rgb.convert("HSV")
    hp = hsv.load()
    red = Image.new("L", (W, H))
    rdp = red.load()
    for y in range(H):
        for x in range(W):
            h, s, v = hp[x, y]
            if (h < 10 or h > 246) and s > 150 and v > 110:
                rdp[x, y] = 255
    red = ImageChops.multiply(red, body)
    red = drop_specks(red, MIN_TEAM_AREA)
    # fill small holes and patches in the cloth so a worn awning is one solid player colour
    red = ImageChops.multiply(red.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5)), body)
    # thin red lines (brick bands, roof tile edges) are not cloth: open, then drop what is left small
    red = drop_specks(red.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(3)), 2 * MIN_TEAM_AREA,
                      hsv if CLOTH_HUE else None)

    for variant in ("A", "B"):
        out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        op = out.load()
        bp, sp, rpx, cp = body.load(), shadow.load(), red.load(), rgb.load()
        for y in range(H):
            for x in range(W):
                if bp[x, y]:
                    if variant == "B" and rpx[x, y]:
                        r, g, b = cp[x, y]
                        lum = (r * 299 + g * 587 + b * 114) // 1000
                        grey = max(40, min(140, lum * 95 // 70))
                        op[x, y] = (grey, grey, grey, TEAM)
                    else:
                        op[x, y] = cp[x, y] + (SOLID,)
                elif sp[x, y]:
                    op[x, y] = (0, 0, 0, SHADOW)
        # the magenta backdrop tints the whole building: match the original's neutral balance
        a = color_match.match(np.array(out), np.array(orig))[0]
        # edge pixels blended with the backdrop (sand, dust) stay pink: red far above green and
        # blue not below it. Repaint them as sand of the same brightness (g = 0.80 r, b = 0.62 r).
        # Saturated reds (cloth, paint, g low) and terracotta (blue well below green) are kept.
        r, g, b = (a[..., i].astype(int) for i in range(3))
        pink = (a[..., 3] == SOLID) & (r - g > 45) & (b > g - 8) & (g > 80)
        a[..., 1] = np.where(pink, r * 80 // 100, g).astype(np.uint8)
        a[..., 2] = np.where(pink, r * 62 // 100, b).astype(np.uint8)
        out = Image.fromarray(a)
        out.save(os.path.join(out_dir, f"{name}_{variant}.tga"))

    print(f"{name}: {W}x{H}, shadow shift {best}, player-colour pixels {sum(1 for v in red.getdata() if v)}")


if __name__ == "__main__":
    SHADOWS = "--shadow" in sys.argv
    if SHADOWS:
        sys.argv.remove("--shadow")
    DY =int(sys.argv[5]) if len(sys.argv) > 5 else 0  # optional vertical shift in game pixels
    CLOTH_HUE = int(sys.argv[6]) if len(sys.argv) > 6 else 0
    main(*sys.argv[1:5])
