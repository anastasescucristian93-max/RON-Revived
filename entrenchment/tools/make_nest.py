r"""Makes the Sandbag Nest's art from a Gemini image (one ring of sandbags on flat magenta):

    python make_nest.py <gemini image> <out art folder> [options]

Writes <name>.tga (the building sprite, with the usual wear) and <name>z.bh3 (its depth mesh)
into the art folder, keeps the clean sprite in clean\, and writes the spritexoff / spriteyoff for
building_graphics.xml to work\offsets_<name>.txt. One sprite per age group, named after the
game's age suffix: nest_0 (ages 1-3), nest_3 (4), nest_4 (5), nest_5 (6), nest_6 (7-8).

The ring in the image is described by four measurements, in image pixels (defaults: the square
Nest, entrenchment/gemini/sandbag_nest_square.jpg; the first, round one was --shape ellipse
--cx 330 --cy 520 --rx 260 --ry 195 --wall 145 --back-wall 40 --inner 0.84):
  --cx, --cy   centre of the ring's ground ellipse
  --rx, --ry   half width and half height of the ring's outer outline on the ground (for a square:
               of its rhombus, corner to corner)
  --wall       height of the wall
  --inner      inner radius / outer radius (the wall's thickness)

The game draws a building sprite without depth; the depth comes from the .bh3 mesh alone. Units
standing inside the ring are hidden where the mesh is in front of them, so the mesh is the same
ring in the world: outer and inner wall faces and the top. The camera projection was fitted on
the game's own towers and lookouts against their depth meshes (sx = x0 + A (x - y),
sy = y0 + C (x + y) - E z):
"""
import argparse
import os
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "roman-buildings", "tools"))
sys.path.insert(0, os.path.join(HERE, "..", "..", "paradrop", "tools"))
import color_match  # noqa: E402
import make_sprite as ms  # noqa: E402

GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
A, C, E = 0.76, 0.30, 0.90  # projection, game pixels per world unit
TEAM, SHADOW, SOLID = 248, 145, 255
SHADOW_AB = (0.55, 0.10)    # shadow projection (west), like the game's low buildings
SEGMENTS = 32               # a multiple of 4: the square's corners are vertices
ROUND = 0.12                # how much a square ring's corners are rounded


def keyed(img):
    """Object alpha; the image's own shadow (darker magenta) counts as background."""
    ms.MAGENTA = ms.background(img)
    a = np.array(ms.key_alpha(img)).astype(np.int32)
    rgb = np.array(img).astype(np.int32)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    shade = (np.minimum(r, b) - g > 50) & (np.abs(r - b) < 45)
    a[shade] = 0
    return Image.fromarray(a.astype(np.uint8))


def sprite(src, args):
    big = Image.open(src).convert("RGB")
    a_big = keyed(big)
    big = ms.unmix(big, a_big)
    pre = Image.composite(big, Image.new("RGB", big.size), a_big)
    box = a_big.point(lambda v: 255 if v >= 128 else 0).getbbox()

    # the ring's outer half width in the game: a circle of that radius, or a square of that half side
    rx_px = (A * np.sqrt(2) if args.shape == "ellipse" else 2 * A) * args.radius
    s = rx_px / args.rx
    W = int(round((box[2] - box[0]) * s)) + 2 * args.margin
    H = int(round((box[3] - box[1]) * s)) + args.margin
    tw, th = round((box[2] - box[0]) * s), round((box[3] - box[1]) * s)
    left, top = args.margin, H - th - 2
    pre_fit = pre.crop(box).resize((tw, th), Image.LANCZOS)
    a_fit = a_big.crop(box).resize((tw, th), Image.LANCZOS)
    pre_c = Image.new("RGB", (W, H))
    pre_c.paste(pre_fit, (left, top))
    a_c = Image.new("L", (W, H))
    a_c.paste(a_fit, (left, top))
    rgb = Image.new("RGB", (W, H))
    pp, ap, rp = pre_c.load(), a_c.load(), rgb.load()
    for y in range(H):
        for x in range(W):
            if ap[x, y]:
                rp[x, y] = tuple(min(255, v * 255 // ap[x, y]) for v in pp[x, y])
    body = a_c.point(lambda v: 255 if v >= 128 else 0)
    ms.despill(rgb, body)
    shadow = ms.project_shadow(body, *SHADOW_AB)

    hsv = rgb.convert("HSV").load()
    red = Image.new("L", (W, H))
    rdp = red.load()
    for y in range(H):
        for x in range(W):
            h, sat, v = hsv[x, y]
            if (h < 10 or h > 246) and sat > 150 and v > 110:
                rdp[x, y] = 255
    from PIL import ImageChops
    red = ImageChops.multiply(red, body)
    red = ms.drop_specks(red, ms.MIN_TEAM_AREA)
    red = ImageChops.multiply(red.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3)), body)

    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    op, bp, sp, rpx = out.load(), body.load(), shadow.load(), red.load()
    for y in range(H):
        for x in range(W):
            if bp[x, y]:
                if rpx[x, y]:
                    r, g, b = rgb.getpixel((x, y))
                    lum = (r * 299 + g * 587 + b * 114) // 1000
                    grey = max(40, min(140, lum * 95 // 70))
                    op[x, y] = (grey, grey, grey, TEAM)
                else:
                    op[x, y] = rgb.getpixel((x, y)) + (SOLID,)
            elif sp[x, y]:
                op[x, y] = (0, 0, 0, SHADOW)
    ref = Image.open(os.path.join(GAME, "art", "tower_6.tga")).convert("RGBA")  # sandbags of the same game
    a = color_match.match(np.array(out), np.array(ref))[0]
    # thin parts (stake tips, sack edges) keep magenta from the backdrop: red far above green and
    # blue not below it. Repaint them as wood / sand of the same brightness, as make_sprite.py does.
    r, g, b = (a[..., i].astype(int) for i in range(3))
    pink = (a[..., 3] == SOLID) & (r - g > 12) & (b > g - 4) & (g > 50)  # saturated reds (paint) keep
    a[..., 1] = np.where(pink, r * 80 // 100, g).astype(np.uint8)
    a[..., 2] = np.where(pink, r * 62 // 100, b).astype(np.uint8)
    out = Image.fromarray(a)

    # the world origin (the building's centre) in sprite pixels
    x0 = left + (args.cx - box[0]) * s
    y0 = top + (args.cy - box[1]) * s
    return out, int(round(x0)), int(round(y0)), s


def write_tga(img, path):
    """Uncompressed 32-bit, bottom-up, like the game's own building sprites."""
    W, H = img.size
    hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, W, H, 32, 8)
    px = img.load()
    data = bytearray()
    for y in range(H - 1, -1, -1):
        for x in range(W):
            r, g, b, a = px[x, y]
            data += bytes((b, g, r, a))
    open(path, "wb").write(hdr + bytes(data))


def ring_mesh(args, s):
    """The ring in world units. u runs along the screen's horizontal (x - y), v towards the
    camera (x + y); a circle on the ground shows as the image's ellipse when u and v get the
    radii that the projection turns into rx and ry."""
    ru = args.rx * s / (A * np.sqrt(2))
    rv = args.ry * s / (C * np.sqrt(2))
    h = args.mesh_wall if args.mesh_wall else args.wall * s / E
    # Gemini draws the back wall lower than the front one (its camera is steeper than the game's),
    # so the mesh's wall height goes from the front's to the back's around the ring
    hb = h * args.back_wall / args.wall
    k = args.inner
    verts, tris = [], []

    def uv_to_xy(u, v):
        return (u + v) / np.sqrt(2), (v - u) / np.sqrt(2)

    for i in range(SEGMENTS):
        t = 2 * np.pi * i / SEGMENTS
        ht = hb + (h - hb) * (1 + np.sin(t)) / 2  # sin = 1 at the front (towards the camera)
        # a square on the map shows as a rhombus with its corners left, right, up and down: in
        # (u, v) that is |u| / ru + |v| / rv = 1, its corners slightly rounded
        f = 1.0 if args.shape == "ellipse" else \
            (1 - ROUND) / (abs(np.cos(t)) + abs(np.sin(t))) + ROUND
        for scale, z in ((f, 0.0), (f, ht), (f * k, ht), (f * k, 0.0)):  # outer foot, outer top, inner top, inner foot
            x, y = uv_to_xy(ru * scale * np.cos(t), rv * scale * np.sin(t))
            verts.append((x, y, z))
    for i in range(SEGMENTS):
        j = (i + 1) % SEGMENTS
        for q in range(3):  # outer face, top, inner face
            a, b, c, d = i * 4 + q, i * 4 + q + 1, j * 4 + q + 1, j * 4 + q
            tris += [(a, b, c), (a, c, d), (a, c, b), (a, d, c)]  # both sides
    return np.array(verts, np.float32), np.array(tris, np.uint16), h


def write_bh3(verts, tris, path):
    import build_bh3 as bh
    template = bh.parse(open(os.path.join(GAME, "art", "tower_4z.bh3"), "rb").read())
    old_n = struct.unpack_from("<I", template["kids"][0]["kids"][0]["data"])[0]
    n = len(verts)
    normals = np.zeros((n, 3), np.float32)
    normals[:, 2] = 1
    mesh = {"type": 1, "kids": [
        bh.leaf(2, struct.pack("<I", n) + np.c_[verts, np.ones(n)].astype("<f4").tobytes()),
        bh.leaf(3, struct.pack("<I", n) + normals.astype("<f4").tobytes() + b"\xCD" * (4 * n)),
        bh.leaf(4, struct.pack("<I", n) + np.zeros((n, 2), "<f4").tobytes()),
        bh.leaf(5, struct.pack("<I", tris.size) + tris.astype("<u2").tobytes()),
    ]}

    def fix_nodes(ch):  # the nodes name the vertex count of the mesh they own
        if "kids" in ch:
            for k in ch["kids"]:
                fix_nodes(k)
        elif ch["type"] == 7:
            vs, vc = struct.unpack_from("<II", ch["data"])
            if vc == old_n:
                ch["data"] = struct.pack("<II", vs, n) + ch["data"][8:]
    nodes = template["kids"][1]
    fix_nodes(nodes)
    root = {"type": 0, "kids": [mesh, nodes]}
    open(path, "wb").write(bh.write(root))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("image")
    p.add_argument("out")
    p.add_argument("--cx", type=float, default=320)
    p.add_argument("--cy", type=float, default=537)
    p.add_argument("--rx", type=float, default=287)
    p.add_argument("--ry", type=float, default=176)
    p.add_argument("--wall", type=float, default=65)
    p.add_argument("--inner", type=float, default=0.80)
    p.add_argument("--back-wall", type=float, default=70, help="height of the back wall in the image")
    p.add_argument("--radius", type=float, default=40, help="outer radius (circle) or half side (square), world units; a 2x2 footprint is 42")
    p.add_argument("--shape", choices=("square", "ellipse"), default="square")
    p.add_argument("--mesh-wall", type=float, default=0, help="depth mesh wall height, world units (0 = from the image)")
    p.add_argument("--margin", type=int, default=12)
    p.add_argument("--name", default="nest_6", help="nest_0 (ages 1-3) .. nest_6 (ages 7-8), nest_7 (age 8)")
    p.add_argument("--like", default="", help="the sprite this one replaces at draw time: same canvas and origin")
    args = p.parse_args()
    os.makedirs(args.out, exist_ok=True)
    img, x0, y0, s = sprite(args.image, args)
    if args.like:
        # a sprite that replaces another one at draw time (nation_art.c, [all]) keeps that one's
        # canvas and origin: the game places it with the other's offsets and depth mesh
        ref = Image.open(os.path.join(args.out, f"{args.like}.tga"))
        rx0, ry0 = map(int, open(os.path.join(args.out, "..", f"offsets_{args.like}.txt")).read().split())
        canvas = Image.new("RGBA", ref.size, (0, 0, 0, 0))
        canvas.paste(img, (rx0 - x0, ry0 - y0))
        img, x0, y0 = canvas, rx0, ry0
    work = os.path.join(args.out, "..")
    clean = os.path.join(HERE, "..", "clean", f"{args.name}.tga")
    os.makedirs(os.path.dirname(clean), exist_ok=True)
    write_tga(img, clean)
    img.save(os.path.join(work, f"preview_{args.name}.png"))
    # wear, as on every new building sprite (roman-buildings/tools/weather.py, medium), from the clean copy
    worn = os.path.join(work, f"{args.name}_worn.png")
    subprocess.run([sys.executable, os.path.join(HERE, "..", "..", "roman-buildings", "tools", "weather.py"),
                    clean, worn, "1.0"], check=True)
    write_tga(Image.open(worn).convert("RGBA"), os.path.join(args.out, f"{args.name}.tga"))
    verts, tris, h = ring_mesh(args, s)
    write_bh3(verts, tris, os.path.join(args.out, f"{args.name}z.bh3"))
    print(f"sprite {img.size[0]}x{img.size[1]}, scale {s:.3f}, spritexoff={x0} spriteyoff={y0}; "
          f"depth ring {len(verts)} vertices, wall {h:.1f} units")
    open(os.path.join(work, f"offsets_{args.name}.txt"), "w").write(f"{x0} {y0}\n")


if __name__ == "__main__":
    main()
