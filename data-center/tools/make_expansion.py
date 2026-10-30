r"""Art for the three Data Center Expansions (1x1 buildings):

    python make_expansion.py template      for-gemini\expansion_camera_reference.png
    python make_expansion.py placeholder   work\art\dc_exp_<kind>.tga + z.bh3 + offsets, for every
                                           kind that has no art from Gemini yet

The template is a plain grey block standing on one map tile, 3:2 (height : width), drawn with
the game's camera at 6x the game's size on flat magenta: Gemini turns it into the building, so the
camera matches. The placeholders are the same block at game size, in the kind's colour, with a
player-colour band and strips of light, until the real art is made.

Lights: every Expansion pulses. Besides <name>.tga (lit), FRAMES sprites <name>_<f>.tga go from
lit (0) to dark (FRAMES - 1); nation_art.c plays them back and forth (rules in
roman-buildings\nation_art.ini, [all]). frames() makes them from the lit sprite and a mask of
its lights.

Projection (fitted on the game's towers and lookouts against their depth meshes, as in
entrenchment\tools\make_nest.py): sx = x0 + A (x - y), sy = y0 + C (x + y) - E z, world units.
A 2x2 building's footprint is 42 units from its centre to a side, so one tile is 42 wide.
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "entrenchment", "tools"))
sys.path.insert(0, os.path.join(ROOT, "paradrop", "tools"))
from make_nest import write_bh3, write_tga  # noqa: E402

WORK_ART = os.path.join(HERE, "..", "work", "art")
FOR_GEMINI = os.path.join(HERE, "..", "for-gemini")
A, C, E = 0.76, 0.30, 0.90
TEAM, SHADOW, SOLID = 248, 145, 255
HALF = 19.0     # half side of the block, world units (a tile is 42: a little room around it)
HEIGHT = 71.0   # height of the block, world units: the sprite is 3:2 (height : width)
MAGENTA = (255, 0, 255)
FRAMES = 8      # lit .. dark; nation_art.ini plays them back and forth

# kind -> sprite name, body colour of the placeholder, colour of its lights
KINDS = {
    "population": ("dc_exp_population", (205, 210, 200), (90, 255, 120)),
    "commerce": ("dc_exp_commerce", (90, 90, 95), (255, 200, 60)),
    "efficiency": ("dc_exp_efficiency", (190, 195, 205), (60, 170, 255)),
}


def project(x, y, z, x0, y0, k=1.0):
    return x0 + k * A * (x - y), y0 + k * (C * (x + y) - E * z)


def block_faces(x0, y0, k, h=HALF, z=HEIGHT):
    """The three faces the camera sees: top, left (y = +h) and right (x = +h), as polygons."""
    def p(x, y, zz):
        return project(x, y, zz, x0, y0, k)
    top = [p(-h, -h, z), p(h, -h, z), p(h, h, z), p(-h, h, z)]
    left = [p(-h, h, 0), p(h, h, 0), p(h, h, z), p(-h, h, z)]
    right = [p(h, h, 0), p(h, -h, 0), p(h, -h, z), p(h, h, z)]
    return top, left, right


def template():
    k = 6.0
    w, hgt = 1024, 1024
    img = Image.new("RGB", (w, hgt), MAGENTA)
    d = ImageDraw.Draw(img)
    x0, y0 = w / 2, hgt / 2 + k * E * HEIGHT / 2
    top, left, right = block_faces(x0, y0, k)
    for poly, grey in ((left, 150), (right, 115), (top, 200)):
        d.polygon(poly, fill=(grey, grey, grey), outline=(60, 60, 60))
    os.makedirs(FOR_GEMINI, exist_ok=True)
    path = os.path.join(FOR_GEMINI, "expansion_camera_reference.png")
    img.save(path)
    print(path)


def shade(colour, f):
    return tuple(max(0, min(255, int(c * f))) for c in colour)


def wall_quad(face, u1, u2, z1, z2, x0, y0):
    """A rectangle on a front wall: u along the wall (0 = left end, 1 = right end), z in units."""
    if face == "left":  # y = +HALF, from x = -HALF to x = +HALF
        def pt(u, z):
            return project(-HALF + 2 * HALF * u, HALF, z, x0, y0)
    else:  # x = +HALF, from y = +HALF to y = -HALF
        def pt(u, z):
            return project(HALF, HALF - 2 * HALF * u, z, x0, y0)
    return [pt(u1, z1), pt(u2, z1), pt(u2, z2), pt(u1, z2)]


def placeholder(name, colour, light):
    m = 4
    xs, ys = [], []
    for x in (-HALF, HALF):
        for y in (-HALF, HALF):
            for z in (0, HEIGHT):
                sx, sy = project(x, y, z, 0, 0)
                xs.append(sx)
                ys.append(sy)
    x0, y0 = int(np.ceil(-min(xs))) + m, int(np.ceil(-min(ys))) + m
    w, h = int(np.ceil(max(xs))) + x0 + m, int(np.ceil(max(ys))) + y0 + m
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    mask = Image.new("L", (w, h), 0)  # where the lights are
    d, dm = ImageDraw.Draw(img), ImageDraw.Draw(mask)
    top, left, right = block_faces(x0, y0, 1.0)
    d.polygon(left, fill=shade(colour, 0.85) + (SOLID,))
    d.polygon(right, fill=shade(colour, 0.6) + (SOLID,))
    d.polygon(top, fill=shade(colour, 1.1) + (SOLID,))
    for face, grey in (("left", 120), ("right", 90)):
        # the player colour: a band low on both front walls
        d.polygon(wall_quad(face, 0, 1, HEIGHT * 0.10, HEIGHT * 0.17, x0, y0), fill=(grey, grey, grey, TEAM))
        # lights: a strip up the middle of each wall and three window rows
        for q in [wall_quad(face, 0.44, 0.56, HEIGHT * 0.25, HEIGHT * 0.94, x0, y0)] + \
                 [wall_quad(face, 0.12, 0.34, z, z + 4, x0, y0) for z in (HEIGHT * 0.35, HEIGHT * 0.55, HEIGHT * 0.75)] + \
                 [wall_quad(face, 0.66, 0.88, z, z + 4, x0, y0) for z in (HEIGHT * 0.35, HEIGHT * 0.55, HEIGHT * 0.75)]:
            d.polygon(q, fill=light + (SOLID,))
            dm.polygon(q, fill=255)
    for poly in (left, right, top):
        d.line(poly + [poly[0]], fill=(30, 30, 30, SOLID))
    os.makedirs(WORK_ART, exist_ok=True)
    write_tga(img, os.path.join(WORK_ART, f"{name}.tga"))
    frames(name, img, mask)
    verts, tris = box_mesh()
    write_bh3(verts, tris, os.path.join(WORK_ART, f"{name}z.bh3"))
    open(os.path.join(WORK_ART, "..", f"offsets_{name}.txt"), "w").write(f"{x0} {y0}\n")
    print(f"{name}: {w}x{h}, spritexoff={x0} spriteyoff={y0}, {FRAMES} frames")


def frames(name, lit, mask):
    """<name>_0.tga (lit) .. <name>_<FRAMES - 1>.tga (dark): the lights fade to an unlit glass of the
    same hue, and a faint glow around them on the building fades with them. The rest of the
    sprite, its outline and alpha stay as they are."""
    a = np.asarray(lit.convert("RGBA")).astype(np.float32)
    rgb, alpha = a[..., :3], a[..., 3:]
    k = np.asarray(mask).astype(np.float32)[..., None] / 255
    glow = np.asarray(mask.filter(ImageFilter.GaussianBlur(2.5))).astype(np.float32)[..., None] / 255
    glow = np.clip(glow * 1.6 - k, 0, 1) * (alpha > 0)
    off = rgb.mean(axis=2, keepdims=True) * 0.18 + rgb * 0.10  # dark glass with a hint of the colour
    tint = (rgb * k).sum(axis=(0, 1)) / max(k.sum(), 1)       # the lights' mean colour
    for f in range(FRAMES):
        on = 1 - f / (FRAMES - 1)
        lights = off + (rgb - off) * on
        out = rgb * (1 - k) + lights * k
        out = out + (tint - out) * glow * 0.35 * on  # the glow spills a little onto the walls
        frame = np.concatenate([np.clip(out, 0, 255), alpha], axis=2).astype(np.uint8)
        write_tga(Image.fromarray(frame, "RGBA"), os.path.join(WORK_ART, f"{name}_{f}.tga"))


def box_mesh(h=HALF, z=HEIGHT):
    """The block as the depth mesh: four walls and the top, both sides of every face."""
    c = [(-h, -h), (h, -h), (h, h), (-h, h)]
    verts = [(x, y, 0.0) for x, y in c] + [(x, y, z) for x, y in c]
    faces = [(4, 5, 6, 7)] + [(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i) for i in range(4)]
    tris = []
    for a, b, cc, dd in faces:
        tris += [(a, b, cc), (a, cc, dd), (a, cc, b), (a, dd, cc)]
    return np.array(verts, np.float32), np.array(tris, np.uint16)


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else ""
    if what == "template":
        template()
    elif what == "placeholder":
        for kind, (name, colour, light) in KINDS.items():
            if os.path.exists(os.path.join(WORK_ART, f"{name}.tga")) and "--force" not in sys.argv:
                print(f"{name}: art already there, kept")
                continue
            placeholder(name, colour, light)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
