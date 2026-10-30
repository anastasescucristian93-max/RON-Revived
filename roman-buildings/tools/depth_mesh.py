r"""Makes the depth (zbuffer) mesh of every per-nation building sprite in nation_art.ini:

    python depth_mesh.py [name ...]      e.g. tower_6 village_0s; no name: every rule

For a rule "tower_6.tga = nation_art\romans\tower_6.tga" it reads the new sprite from the repo
(<mod>\nation_art\romans\tower_6.tga), writes tower_6z.bh3 next to it and into the game's
nation_art\romans\, and a check picture to <mod>\work\depth_tower_6.png (grey: sprite covered by
the mesh, red: sprite not covered, blue: mesh outside the sprite). nation_art.c swaps the mesh in
with the sprite.

The game draws a building sprite without depth; the depth comes from the zbuffer mesh alone, so
crops, water and units behind a building show through every part of new art outside the old
mesh. The new mesh is a set of columns on a grid over the old mesh's ground area: each column is
as high as the sprite is solid straight above its ground point (and its back and side corners),
rounded down to 8 units, with walls on the two sides that face the camera. It stays inside the
sprite (a mesh over empty space would hide what is behind it); thin parts that hang in the air,
such as palm crowns and spears, are not covered. Projection, fitted on 11 of the game's
buildings against their own meshes (bh3 units, z up, origin at spritexoff / spriteyoff):
    sx = x0 + 0.70 (x - y),  sy = y0 - 0.31 (x + y) - 0.90 z
"""
import os
import re
import shutil
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "entrenchment", "tools"))
sys.path.insert(0, os.path.join(REPO, "paradrop", "tools"))
from make_nest import write_bh3  # noqa: E402

GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
A, C, E = 0.70, 0.31, 0.90  # projection, sprite pixels per world unit
STEPS = (8.0, 10.0, 12.0, 14.0, 16.0, 20.0, 24.0, 28.0, 32.0, 40.0)  # grid, world units: the finest under MAX_VERTS
Q = 8.0                     # column heights are rounded down to this
PAD = 0.15                  # the grid covers the old mesh's ground area and this much around it
CARDS = {"tower_6"}  # sprites whose overhangs get camera-facing cards (cards()); Romania's modern tower
CELLS = (1, 2, 3, 4, 5, 6, 8)  # card block sizes, pixels: pixel runs make 300-500 card vertices on ragged edges
MAX_VERTS = 500            # the game's own depth meshes: at most 1096 vertices, median 129. Every depth mesh
                           # and model on screen shares one Pre-Transform batch of 3 x 16383 vertices
                           # (PreTransformBatchMethod::execute); meshes of 1000-6300 vertices filled it in a
                           # Romanian age-7 game and the game stopped ("too many verts", 2026-10-29).
TEAM, SOLID = 248, 255


def chunks(d, off, end):
    while off + 8 <= end:
        size, typ, n = struct.unpack_from("<IHH", d, off)
        if size < 8 or off + size > end:
            return
        yield off, size, typ, n
        off += size


def mesh_vertices(path):
    """Every vertex of every mesh in a .bh3."""
    d = open(path, "rb").read()
    out = []

    def walk(off, end):
        for o, s, t, _ in chunks(d, off, end):
            if t == 0:
                walk(o + 8, o + s)
            elif t == 1:
                for co, _, ct, _ in chunks(d, o + 8, o + s):
                    if ct == 2:
                        n = struct.unpack_from("<I", d, co + 8)[0]
                        out.append(np.frombuffer(d, np.float32, n * 4, co + 12).reshape(n, 4)[:, :3])
    walk(0, len(d))
    return np.vstack(out)


def build_entries(texture):
    """(zbuffer file, spritexoff, spriteyoff) of the <BUILD> entries that draw this texture."""
    found = set()
    for xml in (os.path.join(GAME, "mod_override", "Data", "building_graphics.xml"),
                os.path.join(GAME, "Data", "building_graphics.xml")):
        if not os.path.exists(xml):
            continue
        for tag in re.findall(r"<BUILD [^>]*>", open(xml, encoding="utf-8", errors="replace").read()):
            attr = dict(re.findall(r'(\w+)="([^"]*)"', tag))
            if os.path.basename(attr.get("texture", "")).lower() == texture and attr.get("zbuffer"):
                found.add((os.path.basename(attr["zbuffer"]), int(attr["spritexoff"]), int(attr["spriteyoff"])))
    return sorted(found)


def project(x, y, z, x0, y0):
    return x0 + A * (x - y), y0 - C * (x + y) - E * z


def solid_height(solid, x, y, x0, y0):
    """How high the sprite is solid straight above the ground point (x, y), world units."""
    sx, sy = project(x, y, 0, x0, y0)
    px, py = int(round(sx)), int(round(sy))
    h_px, w_px = solid.shape
    if not (0 <= px < w_px and 0 <= py < h_px) or not solid[py, px]:
        return 0.0
    top = py
    while top > 0 and solid[top - 1, px]:
        top -= 1
    return (py - top) / E


def heights(solid, x0, y0, lo, hi, step):
    """Grid over lo..hi (world x, y): per cell, a column height that keeps the column's top inside
    the sprite: the lowest of the solid heights at the cell's centre and at its back and side
    corners (the front corner is lower on screen than the centre), rounded down. A mesh outside
    the sprite would hide crops, water and units behind empty space."""
    xs = np.arange(lo[0], hi[0] + step, step)
    ys = np.arange(lo[1], hi[1] + step, step)
    H = np.zeros((len(ys), len(xs)))
    s = step / 2
    for j, y in enumerate(ys):
        for i, x in enumerate(xs):
            h = solid_height(solid, x, y, x0, y0)
            if h > 0:
                h = min([h] + [solid_height(solid, x + dx, y + dy, x0, y0) for dx, dy in ((s, -s), (-s, s), (s, s))])
            H[j, i] = np.floor(h / Q) * Q
    return xs, ys, H


def columns(xs, ys, H, step):
    """Columns on the grid cells: tops merged into rectangles of one height, and walls on the two
    sides that face the camera (smaller x, smaller y) from the lower neighbour up, merged along
    each grid line. Vertices are shared."""
    vid, verts, tris = {}, [], []

    def v(p):
        p = tuple(round(float(c), 2) for c in p)
        if p not in vid:
            vid[p] = len(verts)
            verts.append(p)
        return vid[p]

    def quad(a, b, c, d):
        a, b, c, d = v(a), v(b), v(c), v(d)
        tris.extend([(a, b, c), (a, c, d)])

    ny, nx = H.shape
    gx = lambda i: xs[0] + (i - 0.5) * step  # noqa: E731  cell edges
    gy = lambda j: ys[0] + (j - 0.5) * step  # noqa: E731
    done = np.zeros(H.shape, bool)
    for j in range(ny):
        for i in range(nx):
            h = H[j, i]
            if h <= 0 or done[j, i]:
                continue
            w = 1
            while i + w < nx and H[j, i + w] == h and not done[j, i + w]:
                w += 1
            d = 1
            while j + d < ny and (H[j + d, i:i + w] == h).all() and not done[j + d, i:i + w].any():
                d += 1
            done[j:j + d, i:i + w] = True
            quad((gx(i), gy(j), h), (gx(i + w), gy(j), h), (gx(i + w), gy(j + d), h), (gx(i), gy(j + d), h))
    Hp = np.pad(H, ((1, 0), (1, 0)))
    for j in range(ny):  # walls facing -y, on the line y = gy(j)
        i = 0
        while i < nx:
            lo, hi = Hp[j, i + 1], H[j, i]
            if hi <= lo:
                i += 1
                continue
            w = 1
            while i + w < nx and H[j, i + w] == hi and Hp[j, i + w + 1] == lo:
                w += 1
            quad((gx(i), gy(j), lo), (gx(i + w), gy(j), lo), (gx(i + w), gy(j), hi), (gx(i), gy(j), hi))
            i += w
    for i in range(nx):  # walls facing -x, on the line x = gx(i)
        j = 0
        while j < ny:
            lo, hi = Hp[j + 1, i], H[j, i]
            if hi <= lo:
                j += 1
                continue
            d = 1
            while j + d < ny and H[j + d, i] == hi and Hp[j + d + 1, i] == lo:
                d += 1
            quad((gx(i), gy(j + d), lo), (gx(i), gy(j), lo), (gx(i), gy(j), hi), (gx(i), gy(j + d), hi))
            j += d
    # The quads above wind counter-clockwise seen from outside. The game mirrors every building
    # mesh on load (scale (1, -1, 1)), which reverses the winding, and culls the faces that then
    # turn away: its own meshes are stored the other way round, so these are too.
    return np.array(verts, np.float32), np.array(tris, np.uint16)[:, ::-1].copy()


def cards(uncovered, x0, y0, k, n0):
    """Upright quads facing the camera in the plane x + y = k (the building's middle depth) over
    the sprite pixels the columns leave uncovered: parts wider than what stands under them, like
    the overhanging top of Romania's modern tower (seen in game 2026-10-29: the wheat behind drew
    over it). Pixel runs per row, merged down while equal. A pixel (px, py) on the plane is
    u = x - y = (px - x0) / A, z = (y0 - C k - py) / E; pixels that would be under the ground are
    left out. Wound like columns() (reversed at the end); vertex indices start at n0."""
    verts, tris, rect = [], [], {}
    h, w = uncovered.shape
    runs_prev = {}
    rects = []
    for py in range(h):
        row = uncovered[py]
        runs, x = [], 0
        while x < w:
            if row[x]:
                s = x
                while x < w and row[x]:
                    x += 1
                runs.append((s, x))
            else:
                x += 1
        cur = {}
        for r in runs:
            if r in runs_prev:
                rects[runs_prev[r]][3] = py + 1
                cur[r] = runs_prev[r]
            else:
                rects.append([r[0], r[1], py, py + 1])
                cur[r] = len(rects) - 1
        runs_prev = cur
    for px0, px1, py0, py1 in rects:
        z_hi, z_lo = (y0 - C * k - py0) / E, (y0 - C * k - py1) / E
        if z_hi <= 0:
            continue
        z_lo = max(z_lo, 0.0)
        u0, u1 = (px0 - x0) / A, (px1 - x0) / A
        p = [((k + u0) / 2, (k - u0) / 2, z_lo), ((k + u1) / 2, (k - u1) / 2, z_lo),
             ((k + u1) / 2, (k - u1) / 2, z_hi), ((k + u0) / 2, (k - u0) / 2, z_hi)]
        b = n0 + len(verts)
        verts += p
        tris += [(b, b + 1, b + 2), (b, b + 2, b + 3)]
    if not verts:
        return np.zeros((0, 3), np.float32), np.zeros((0, 3), np.uint16)
    return np.array(verts, np.float32), np.array(tris, np.uint16)[:, ::-1].copy()


def blocks(mask, c):
    """mask snapped to c x c pixel blocks (a block is set when half its pixels are): straight
    edges, so cards() merges its runs into few quads."""
    h, w = mask.shape
    p = np.zeros((-(-h // c) * c, -(-w // c) * c), bool)
    p[:h, :w] = mask
    g = p.reshape(p.shape[0] // c, c, p.shape[1] // c, c).mean((1, 3)) >= 0.5
    return np.repeat(np.repeat(g, c, 0), c, 1)[:h, :w]


def silhouette(verts, tris, size, x0, y0):
    im = Image.new("L", size, 0)
    dr = ImageDraw.Draw(im)
    sx, sy = project(verts[:, 0], verts[:, 1], verts[:, 2], x0, y0)
    for t in tris:
        dr.polygon([(sx[i], sy[i]) for i in t], fill=255)
    return np.array(im) > 0


def rules():
    """(original texture, file in the game folder) of nation_art.ini, without the age. An animated
    rule (<name>~<frames>:<ms>, file with %d) gives its frame 0: nation_art.c loads the mesh
    next to frame 0 for every frame."""
    out = []
    for line in open(os.path.join(REPO, "roman-buildings", "nation_art.ini"), encoding="utf-8"):
        line = line.split(";")[0].strip()
        if "=" in line and not line.startswith("["):
            key, val = (s.strip() for s in line.split("=", 1))
            out.append((key.split("@")[0].split("~")[0].lower(), val.replace("%d", "0")))
    return out


def repo_file(game_rel):
    """The repo copy of a nation_art file: <mod>\\nation_art\\<nation>\\<file>."""
    parts = game_rel.replace("/", "\\").split("\\")
    if parts[0].lower() != "nation_art":
        return None
    for mod in sorted(os.listdir(REPO)):
        p = os.path.join(REPO, mod, *parts)
        if os.path.exists(p):
            return p
    return None


def make(texture, game_rel):
    name = os.path.splitext(os.path.basename(game_rel))[0]
    src = repo_file(game_rel)
    entries = build_entries(texture)
    if not src or not entries:
        print(f"{name}: skipped ({'no repo file' if not src else 'no <BUILD> with a zbuffer draws ' + texture})")
        return
    if len({e[1:] for e in entries}) > 1:
        print(f"{name}: warning, {texture} has several origins {entries}, using the first")
    zfile, x0, y0 = entries[0]
    old = mesh_vertices(os.path.join(GAME, "art", zfile))
    a = np.array(Image.open(src).convert("RGBA"))[..., 3]
    solid = (a == SOLID) | (a == TEAM)
    span = old[:, :2].max(0) - old[:, :2].min(0)
    size = (a.shape[1], a.shape[0])
    best = None
    for step in STEPS:
        xs, ys, H = heights(solid, x0, y0, old[:, :2].min(0) - PAD * span, old[:, :2].max(0) + PAD * span, step)
        verts, tris = columns(xs, ys, H, step)
        if name not in CARDS:
            if len(verts) <= MAX_VERTS:
                best = verts, tris, step, 0
                break
            continue
        # Columns and cards share MAX_VERTS (the cap once dropped the cards silently, and the trees
        # behind Romania's tower drew over its top again, 2026-10-29): try every grid with every
        # card block size, keep the best cover, a pixel of mesh outside the sprite costing three.
        from scipy import ndimage
        mk = silhouette(verts, tris, size, x0, y0)
        uncovered = ndimage.binary_opening(solid & ~mk, np.ones((2, 2)))  # not the 1-pixel edge seams
        k = float((old[:, 0] + old[:, 1]).mean())
        for cell in CELLS:
            cv, ct = cards(blocks(uncovered, cell), x0, y0, k, len(verts))
            if len(verts) + len(cv) > MAX_VERTS:
                continue
            v2, t2 = np.r_[verts, cv], np.r_[tris, ct]
            m2 = silhouette(v2, t2, size, x0, y0)
            score = ((m2 & solid).sum() - 3 * (m2 & ~solid).sum()) / solid.sum()
            if best is None or score > best[4]:
                best = v2, t2, step, len(cv), score
    if best is None:
        print(f"{name}: {len(verts)} vertices, more than {MAX_VERTS}: not written")
        return
    verts, tris, step, ncards = best[:4]
    if ncards:
        print(f"{name}: cards over the overhangs, {ncards} vertices")
    out = os.path.splitext(src)[0] + "z.bh3"
    write_bh3(verts, tris, out)
    game_dir = os.path.join(GAME, os.path.dirname(game_rel))
    os.makedirs(game_dir, exist_ok=True)
    shutil.copy(out, game_dir)

    mk = silhouette(verts, tris, (a.shape[1], a.shape[0]), x0, y0)
    vis = np.zeros(a.shape + (3,), np.uint8)
    vis[solid] = (150, 150, 150)
    vis[solid & ~mk] = (255, 40, 40)
    vis[mk & ~solid] = (40, 40, 160)
    work = os.path.join(os.path.dirname(src), "..", "..", "work")
    os.makedirs(work, exist_ok=True)
    Image.fromarray(vis).resize((a.shape[1] * 2, a.shape[0] * 2), Image.NEAREST).save(
        os.path.join(work, f"depth_{name}.png"))
    print(f"{name}: grid {step:.0f}, {len(verts)} vertices, {len(tris)} triangles; sprite covered "
          f"{(mk & solid).sum() / solid.sum():.0%}, mesh outside the sprite {(mk & ~solid).sum() / mk.sum():.0%} "
          f"(old mesh {zfile})")


def main():
    names = {n.lower().removesuffix(".tga") for n in sys.argv[1:]}
    done = set()
    for texture, game_rel in rules():
        name = os.path.splitext(os.path.basename(game_rel))[0].lower()
        if game_rel.lower() in done or names and name not in names and texture.removesuffix(".tga") not in names:
            continue
        done.add(game_rel.lower())  # one file listed for several nations
        make(texture, game_rel)


if __name__ == "__main__":
    main()
