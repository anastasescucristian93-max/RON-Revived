r"""Makes the Tank Traps' art (a 1x1 building) from one Gemini picture with four variations on
plain white:

    C:\AI\ComfyUI-Easy-Install\ComfyUI-Easy-Install\python_embeded\python.exe ^
        ..\..\roman-buildings\tools\rmbg_matte.py <picture> ..\work\traps_matte.png
    python make_traps.py <picture> ..\work\traps_matte.png [--grid 2x2] [--order 0,1,2,3] [--width 76]
    python make_traps.py <picture> <matte> --age 1 --grid 1x1 --order 0,0,0,0

--age N makes the look of one age (1 Ancient .. 8 Information; the user, 2026-11-10: wooden
stakes in the first ages, sandbags later): tank_traps_age<N>_<i>.tga on the canvas and origin of
tank_traps_0.tga (made first, without --age), scaled to fit it, so they stand on the same tile,
depth mesh and offsets; nation_art.ini's [all] rule "tank_traps_0.tga@N~4:pick" swaps them in
for that age (an age rule wins over the every-age one). No mesh, offsets or icon of their own.

The picture is cut into its variations by --grid (columns x rows, default: 4x1 for a wide picture,
2x2 otherwise); --order picks and orders the cells (0 = top left, row by row), so a bad
variation can be left out or repeated. Each one is cut with the AI matte (rmbg_matte.py, as
roman-buildings\tools\make_sprite_white.py does), scaled together so the widest is --width pixels (it
fills one map tile: the tile's diamond is 2 A TILE = 88 pixels wide) and stood on the same baseline, centred.

Writes into ..\work\art:
  tank_traps_<i>.tga     i = 0..3, all on one canvas with one origin, as nation_art.c needs to
                         swap them at draw time (the "~4:pick" rule in nation_art.ini gives every
                         trap one of them for good, so a line of traps does not repeat one picture)
  tank_traps_0z.bh3      the depth mesh: a low box on the tile, so units behind the traps are
                         hidden by their lower part only (hedgehogs are mostly air)
and ..\work\offsets_tank_traps.txt (spritexoff spriteyoff), ..\work\tank_traps_icon.png (the build
menu icon: variation 0 on grass, 61 x 39) and ..\work\preview_tank_traps.png (the four on grass
with the tile drawn under them). No shadow, no wear (the user's rules, 2026-10-28 and 2026-10-31).

Red paint stays red; red pixels (the player-colour band, if Gemini drew one) get the game's
team alpha (248), as make_nest.py does.

Projection (entrenchment/ENGINE_NOTES.md, refit in roman-buildings/ENGINE_NOTES.md):
sx = x0 + A (x - y), sy = y0 + C (x + y) - E z in sprite pixels, world units; a tile is about 58
units (roman-buildings/ENGINE_NOTES.md, "The plot is the tile square"). The origin (spritexoff,
spriteyoff) is the tile's centre (halfoffset 0, data-center/ENGINE_NOTES.md).
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "paradrop", "tools"))
from make_nest import write_bh3, write_tga  # noqa: E402

WORK = os.path.join(HERE, "..", "work")
WORK_ART = os.path.join(WORK, "art")
A, C, E = 0.76, 0.31, 0.90
TILE = 58.0
TEAM, SOLID = 248, 255
MARGIN = 6
NAME = "tank_traps"
VARIANTS = 4


def cells(size, grid):
    W, H = size
    cols, rows = grid
    for r in range(rows):
        for c in range(cols):
            yield (c * W // cols, r * H // rows, (c + 1) * W // cols, (r + 1) * H // rows)


def variation(pic, soft, box):
    """The variation in one cell: premultiplied colour, alpha (0..1) and its bounding box."""
    x0, y0, x1, y1 = box
    p = pic[y0:y1, x0:x1].astype(np.float32)
    a = soft[y0:y1, x0:x1]
    obj = a >= 0.5
    lab, n = ndimage.label(obj)
    if not n:
        sys.exit(f"cell {box}: nothing in it")
    sizes = ndimage.sum(obj, lab, range(1, n + 1))
    keep = np.isin(lab, [i + 1 for i, s in enumerate(sizes) if s >= max(40, 0.01 * sizes.max())])
    a = a * ndimage.binary_dilation(keep, iterations=2)  # the soft edge stays, specks go
    ys, xs = np.nonzero(a >= 0.5)
    bb = (xs.min(), ys.min(), xs.max() + 1, ys.max() + 1)
    bg = np.median(np.concatenate([p[0], p[-1], p[:, 0], p[:, -1]]), axis=0)
    a3 = np.maximum(a, 0.05)[..., None]
    col = np.clip((p - (1 - a3) * bg) / a3, 0, 255)  # the white taken out of the edge colours
    if WIRE:
        a, col = thicken_wire(a, col)
    return col * a[..., None], a, bb


WIRE = 0  # --wire: how many picture pixels the thin parts grow by on each side


def thicken_wire(a, col):
    """Barbed wire a few pixels thick in a 2800-pixel picture is far below one pixel at game
    size and breaks up into specks. The thin parts (what an opening of the matte takes away:
    the wire, not the beams) grow by WIRE pixels and take the colour of the nearest wire pixel,
    a little darker, so the wire reads as a dark line, as on the game's own forts."""
    obj = a >= 0.5
    thick = ndimage.binary_opening(obj, structure=np.ones((3, 3)), iterations=WIRE + 2)
    thin = obj & ~ndimage.binary_dilation(thick, iterations=2)
    grown = ndimage.binary_dilation(thin, iterations=WIRE) & ~thick
    _, (iy, ix) = ndimage.distance_transform_edt(~thin, return_indices=True)
    col = col.copy()
    col[grown] = col[iy[grown], ix[grown]] * 0.8
    a = np.where(grown, 1.0, a)
    return a, col


def sprite(pre, a, bb, s, canvas, base):
    """One variation at game size (scale s) on the shared canvas, centred, bottom on `base`."""
    W, H = canvas
    nw, nh = bb[2] - bb[0], bb[3] - bb[1]
    tw, th = max(1, round(nw * s)), max(1, round(nh * s))
    ox, oy = (W - tw) // 2, base - th
    al = Image.fromarray((a * 255).astype(np.uint8)).crop(bb).resize((tw, th), Image.LANCZOS)
    pr = Image.fromarray(pre.astype(np.uint8)).crop(bb).resize((tw, th), Image.LANCZOS)
    A_ = np.zeros((H, W), np.float32)
    P = np.zeros((H, W, 3), np.float32)
    A_[oy:oy + th, ox:ox + tw] = np.asarray(al, np.float32)
    P[oy:oy + th, ox:ox + tw] = np.asarray(pr, np.float32)
    body = A_ >= 128
    rgb = np.clip(P * 255 / np.maximum(A_, 1)[..., None], 0, 255).astype(np.uint8)
    hsv = np.asarray(Image.fromarray(rgb).convert("HSV")).astype(int)
    red = body & ((hsv[..., 0] < 10) | (hsv[..., 0] > 246)) & (hsv[..., 1] > 150) & (hsv[..., 2] > 110)
    red = ndimage.binary_opening(red, iterations=1)
    out = np.zeros((H, W, 4), np.uint8)
    out[body, :3] = rgb[body]
    out[body, 3] = SOLID
    out[red, 3] = TEAM
    return Image.fromarray(out)


def box_mesh(half, h):
    """A box on the tile, half side `half`, height `h` (world units), faces seen from both sides."""
    v = [(x, y, z) for z in (0.0, h) for x, y in ((-half, -half), (half, -half), (half, half), (-half, half))]
    quads = [(0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7), (4, 5, 6, 7)]
    tris = []
    for a, b, c, d in quads:
        tris += [(a, b, c), (a, c, d), (a, c, b), (a, d, c)]
    return np.array(v, np.float32), np.array(tris, np.uint16)


def grass(w, h):
    yy = np.linspace(0, 1, h)[:, None, None]
    g = (np.array([86, 118, 52]) * (1 - yy) + np.array([60, 92, 38]) * yy) * np.ones((h, w, 1))
    return Image.fromarray(g.astype(np.uint8))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("picture")
    p.add_argument("matte")
    p.add_argument("--grid", default="")
    p.add_argument("--order", default="")
    p.add_argument("--width", type=float, default=76, help="width of each cluster in game pixels")
    p.add_argument("--mesh-height", type=float, default=0.5, help="depth box height / the art's height")
    p.add_argument("--wire", type=int, default=0, help="grow thin parts (barbed wire) by this many picture pixels")
    p.add_argument("--age", type=int, default=0, help="the look of one age (1..8), on tank_traps_0's canvas")
    args = p.parse_args()
    global WIRE
    WIRE = args.wire

    pic = np.asarray(Image.open(args.picture).convert("RGB"))
    soft = np.asarray(Image.open(args.matte).convert("L").resize((pic.shape[1], pic.shape[0])), np.float32) / 255
    H0, W0 = pic.shape[:2]
    grid = tuple(map(int, args.grid.split("x"))) if args.grid else ((4, 1) if W0 > 1.6 * H0 else (2, 2))
    boxes = list(cells((W0, H0), grid))
    order = [int(i) for i in args.order.split(",")] if args.order else list(range(len(boxes)))
    if len(order) != VARIANTS:
        sys.exit(f"--order must name {VARIANTS} cells (the picture has {len(boxes)})")
    vs = [variation(pic, soft, boxes[i]) for i in order]

    # one scale for all (the widest fills the tile), so the hedgehogs are the same size in every
    # variation; one canvas, as tall as the tallest
    s = args.width / max(bb[2] - bb[0] for _, _, bb in vs)
    heights = [s * (bb[3] - bb[1]) for _, _, bb in vs]
    tip = C * TILE                        # the tile's front corner lies this far below its centre
    if args.age:
        # one age's look on the every-age canvas: same size and origin, scaled down to fit
        W, H = Image.open(os.path.join(WORK_ART, f"{NAME}_0.tga")).size
        if max(heights) > H - MARGIN - 2:
            s *= (H - MARGIN - 2) / max(heights)
            heights = [s * (bb[3] - bb[1]) for _, _, bb in vs]
        out = f"{NAME}_age{args.age}"
    else:
        W = int(round(args.width)) + 2 * MARGIN
        H = int(np.ceil(max(heights))) + MARGIN + 2
        out = NAME
    base = H - 2                          # the clusters' bottom: the tile's front corner, roughly
    x0, y0 = W // 2, int(round(base - tip))
    os.makedirs(WORK_ART, exist_ok=True)
    for i, (pre, a, bb) in enumerate(vs):
        img = sprite(pre, a, bb, s, (W, H), base)
        write_tga(img, os.path.join(WORK_ART, f"{out}_{i}.tga"))
        print(os.path.join(WORK_ART, f"{out}_{i}.tga"), img.size)
    if args.age:
        preview(W, H, x0, y0, out)
        return
    art_h = (max(heights) - 2 * tip) / E  # world units above the tile's middle, roughly
    verts, tris = box_mesh(TILE / 2 * 0.8, max(6.0, art_h * args.mesh_height))
    write_bh3(verts, tris, os.path.join(WORK_ART, f"{NAME}_0z.bh3"))
    open(os.path.join(WORK, f"offsets_{NAME}.txt"), "w").write(f"{x0} {y0}\n")
    print(f"canvas {W}x{H}, origin {x0} {y0}, depth box {verts[:, 2].max():.0f} units high")

    # build menu icon: variation 0 on grass
    pre, a, bb = vs[0]
    cw, ch = 61, 39
    s = min((cw - 4) / (bb[2] - bb[0]), (ch - 3) / (bb[3] - bb[1]))
    tw, th = round((bb[2] - bb[0]) * s), round((bb[3] - bb[1]) * s)
    rgb = Image.fromarray(np.clip(pre / np.maximum(a, 1e-3)[..., None], 0, 255).astype(np.uint8)).crop(bb)
    al = Image.fromarray((a * 255).astype(np.uint8)).crop(bb)
    icon = grass(cw, ch)
    icon.paste(rgb.resize((tw, th), Image.LANCZOS), ((cw - tw) // 2, ch - th - 1), al.resize((tw, th), Image.LANCZOS))
    icon.save(os.path.join(WORK, f"{NAME}_icon.png"))

    preview(W, H, x0, y0, NAME)


def preview(W, H, x0, y0, out):
    """the four on grass at 3x, with the tile's diamond under each"""
    k = 3
    prev = grass(W * VARIANTS * k, H * k).convert("RGB")
    d = ImageDraw.Draw(prev)
    for i in range(VARIANTS):
        cx, cy = (i * W + x0) * k, y0 * k
        h = TILE / 2
        pts = [(cx + k * A * (x - y), cy + k * C * (x + y)) for x, y in ((-h, -h), (h, -h), (h, h), (-h, h))]
        d.polygon(pts, outline=(255, 255, 0))
        spr = Image.open(os.path.join(WORK_ART, f"{out}_{i}.tga")).convert("RGBA")
        spr = spr.resize((W * k, H * k), Image.NEAREST)
        prev.paste(spr, (i * W * k, 0), spr.getchannel("A").point(lambda v: 255 if v else 0))
    prev.save(os.path.join(WORK, f"preview_{out}.png"))
    print(os.path.join(WORK, f"preview_{out}.png"))


if __name__ == "__main__":
    main()
