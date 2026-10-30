r"""Turns the user's Tripo R-2 into a game-ready low-poly tank in two closed parts: the hull and
the turret with its guns, so the game can turn the turret.

    python make_tank.py <in.glb> <out folder> [hull triangles] [turret triangles] [texture size]
                        [running gear share of the hull's triangles, or "drop": drop_gear()]

Steps:
  1. weld the UV-seam duplicates;
  2. cut along the turret's seat: a plane just over the roof of the superstructure, which slopes
     down to the front by about 2 degrees (measured on side sections of r2_tripo.glb), and close
     both cuts with flat caps; each part is checked for open edges (a hole) and the build stops
     if a part has more than it had before the cut. Loose specks of 1 to 3 triangles that were
     already detached in the Tripo model are dropped;
  3. reduce each part on its own (defaults 440 and 160 triangles), topology preserved;
  4. unwrap both parts into one texture and bake the detailed model's colours into it; the
     light grey band around the turret becomes the player colour;
  5. write <out>\hull.obj, turret.obj (sharing tank.png), team.png and parts.json (the turret's
     turning axis and the muzzle tip, Tripo units).

Axes as Tripo exports them: X length (gun and front at +X), Y up, Z width.
"""
import json
import os
import sys
from collections import Counter

import numpy as np
import pymeshlab
import trimesh
from trimesh.intersections import slice_mesh_plane

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_plane import bake, load, open_edges, unwrap  # noqa: E402

# the cut: through the superstructure roof at (x -0.13, y 0.331) and (x 0.145, y 0.322), 0.004 higher
SEAT_SLOPE = -0.0327
SEAT_POINT = np.array([0.0, 0.331 + 0.004 + SEAT_SLOPE * 0.13, 0.0])
SEAT_NORMAL = np.array([-SEAT_SLOPE, 1.0, 0.0]) / np.hypot(SEAT_SLOPE, 1.0)
SPECK = 3  # loose pieces of this many triangles or fewer are dropped
# the light grey band on the turret walls: y 0.358 .. 0.386, luminance about 140, saturation about 9
# (olive above and below: about 95-110 and 25-30), measured on r2_tripo.glb
TURRET_BAND = lambda p: ((p[:, 1] > 0.355) & (p[:, 1] < 0.389) & (p[:, 0] > -0.165) & (p[:, 0] < 0.18)  # noqa: E731
                         & (np.abs(p[:, 2]) < 0.17))


def weld(v, f):
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=v, face_matrix=f))
    ms.meshing_merge_close_vertices(threshold=pymeshlab.PercentageValue(0.01))
    ms.meshing_remove_duplicate_faces()
    ms.meshing_remove_unreferenced_vertices()
    m = ms.current_mesh()
    return trimesh.Trimesh(m.vertex_matrix(), m.face_matrix(), process=False)


def drop_specks(t):
    parts = t.split(only_watertight=False)
    keep = [p for p in parts if len(p.faces) > SPECK]
    print("   %d loose specks dropped (%d triangles)" % (len(parts) - len(keep), sum(len(p.faces) for p in parts) - sum(len(p.faces) for p in keep)))
    return trimesh.util.concatenate(keep)


def cut(t):
    before = open_edges(t.vertices, t.faces)
    turret = slice_mesh_plane(t, SEAT_NORMAL, SEAT_POINT, cap=True)
    hull = slice_mesh_plane(t, -SEAT_NORMAL, SEAT_POINT, cap=True)
    # the barrels in front of the mantlet go (closed with a cap): gun_cylinders() replaces them
    turret = slice_mesh_plane(turret, [-1.0, 0, 0], [BARREL_CUT_X, 0, 0], cap=True)
    out = []
    for name, part in (("hull", hull), ("turret", turret)):
        part = drop_specks(part)
        n = open_edges(part.vertices, part.faces)
        print("%s: %d triangles, %d open edges" % (name, len(part.faces), n))
        out.append(part)
    if sum(open_edges(p.vertices, p.faces) for p in out) > before:
        sys.exit("the cut left holes: %d open edges before" % before)
    return out


GEAR_Y = 0.215  # below this (Tripo Y): wheels and tracks, under the fenders (0.21 .. 0.24)
GEAR_SHARE = 0.3  # of the hull's triangles: the running gear is hidden in the track blocks


def drop_gear(t):
    """The running gear goes and its openings are closed. It sits inside the track blocks, so it
    is never seen, but it held two thirds of a reduced hull's triangles (a selected reduction of
    it stops near 13 000). The patches are inside the blocks too. 2026-10-29: with 1962 vertices
    the R-2 broke the game's vertex batch ("too many verts for Pre-Transform Batch Transform",
    ENGINE_NOTES.md)."""
    c = t.vertices[t.faces].mean(1)
    gear = (c[:, 1] < GEAR_Y) & (np.abs(c[:, 2]) >= TRACK_Z[0])
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=np.asarray(t.vertices, float), face_matrix=np.asarray(t.faces[~gear])))
    ms.meshing_remove_unreferenced_vertices()
    ms.meshing_close_holes(maxholesize=100000, newfaceselected=False, selfintersection=False)
    m = ms.current_mesh()
    v, f = m.vertex_matrix(), m.face_matrix()
    print("running gear dropped: %d triangles; closed: %d triangles, %d open edges" % (gear.sum(), len(f), open_edges(v, f)))
    if open_edges(v, f):
        sys.exit("the running gear left holes")
    return trimesh.Trimesh(v, f, process=False)


def collapse(ms, target, selected):
    for _ in range(3):  # one pass stops short of the target
        ms.meshing_decimation_quadric_edge_collapse(targetfacenum=target, selected=selected, preservetopology=False,
                                                    preserveboundary=True, boundaryweight=2.0, preservenormal=True,
                                                    optimalplacement=True, planarquadric=True, autoclean=True)


def reduce(t, target, gear_share=0):
    """Without topology preservation: with it the hull stops near 1000 triangles (the running gear
    has many holes) and the cap turns into a star of long slivers. With gear_share the running
    gear (every corner under GEAR_Y) and the rest are reduced apart, the gear keeping that share
    of the triangles: the same mesh, two budgets. Open edges are checked after."""
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=np.asarray(t.vertices, float), face_matrix=np.asarray(t.faces)))
    if gear_share:
        # the running gear: under the fenders and outside the hull's sides, hidden in the track blocks
        y, z = GEAR_Y, TRACK_Z[0]
        gear = " && ".join("(y%d < %f) && (abs(z%d) >= %f)" % (i, y, i, z) for i in range(3))
        rest = " || ".join("(y%d >= %f) || (abs(z%d) < %f)" % (i, y, i, z) for i in range(3))  # muparser has no "!"
        ms.compute_selection_by_condition_per_face(condselect=gear)
        collapse(ms, int(target * gear_share), True)
        ms.compute_selection_by_condition_per_face(condselect=rest)
        collapse(ms, target - int(target * gear_share), True)
    else:
        collapse(ms, target, False)
    m = ms.current_mesh()
    v, f = m.vertex_matrix(), m.face_matrix()
    if open_edges(v, f):
        sys.exit("the reduction opened %d edges" % open_edges(v, f))
    return v, f


# The tracks: Tripo's thin belts and small wheels (24 000 triangles) tear at any budget a game
# model can have. Like the game's own tanks (wheels painted on a flat side), each side gets a closed
# block: the side outline of the running gear (every face at |z| 0.19 .. 0.283 under the fenders),
# a little larger, simplified, pushed across the track's width. It covers the reduced wheels, and
# the bake paints the user's wheels and tracks on it.
TRACK_Z = (0.187, 0.285)
TRACK_TOP = 0.215
TRACK_GROW = 0.003   # the block's outline is this much larger than the running gear's
TRACK_SIMPLIFY = 0.012  # 0.006 until 2026-10-29 (the vertex batch)


def track_blocks(t):
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    c = t.vertices[t.faces]
    centre = c.mean(1)
    blocks = []
    for side in (1, -1):
        s = (side * centre[:, 2] >= TRACK_Z[0] + 0.003) & (centre[:, 1] < TRACK_TOP)
        polys = [Polygon(tri[:, :2]).buffer(0.0005) for tri in c[s] if Polygon(tri[:, :2]).area > 1e-9]
        shape = unary_union(polys).buffer(TRACK_GROW, join_style=2)
        if shape.geom_type == "MultiPolygon":
            shape = max(shape.geoms, key=lambda g: g.area)
        outline = Polygon(shape.exterior).simplify(TRACK_SIMPLIFY, preserve_topology=True)
        block = trimesh.creation.extrude_polygon(outline, TRACK_Z[1] - TRACK_Z[0])
        z0 = TRACK_Z[0] if side > 0 else -TRACK_Z[1]
        block.apply_translation([0, 0, z0])
        print("track block %+d: %d outline points, %d triangles, %d open edges" % (
            side, len(outline.exterior.coords) - 1, len(block.faces), open_edges(block.vertices, block.faces)))
        blocks.append(block)
    return blocks


# The barrels: reduced, a thin round barrel turns into a spike (and its tip pokes through any
# cover). The user's barrels are cut off in front of the mantlet (BARREL_CUT_X, the cut capped)
# and rebuilt as closed cylinders along X, a little thicker than theirs (measured on surface
# samples of r2_tripo.glb's turret, slice by slice): (x from, x to, y, z, radius, sides)
BARREL_CUT_X = 0.26
GUNS = [
    (0.19, 0.256, 0.398, 0.0135, 0.0140, 8),   # 37 mm gun: the sleeve at the mantlet
    (0.25, 0.367, 0.3985, 0.014, 0.0100, 8),   # its barrel
    (0.33, 0.367, 0.3985, 0.014, 0.0118, 8),   # the muzzle end
    (0.19, 0.238, 0.399, -0.0435, 0.0150, 6),   # the machine gun's mount
    (0.234, 0.301, 0.3975, -0.0435, 0.0092, 6),  # its jacket
    (0.298, 0.336, 0.398, -0.043, 0.0058, 6),   # its barrel
]
MUZZLE_X = 0.366  # texels at the 37 mm gun's front cap: the dark bore


def gun_cylinders():
    parts = []
    for x0, x1, y, z, r, n in GUNS:
        a = np.arange(n) * 2 * np.pi / n + np.pi / n
        ring = np.c_[y + r * np.cos(a), z + r * np.sin(a)]
        v = np.r_[np.c_[np.full(n, x0), ring], np.c_[np.full(n, x1), ring], [[x0, y, z], [x1, y, z]]]
        f = []
        for i in range(n):
            j = (i + 1) % n
            f += [[i, j, n + j], [i, n + j, n + i], [2 * n, j, i], [2 * n + 1, n + i, n + j]]
        c = trimesh.Trimesh(v, f, process=False)
        c.fix_normals()  # outwards
        parts.append(c)
    guns = trimesh.util.concatenate(parts)
    print("guns: %d cylinders, %d triangles, %d open edges" % (len(GUNS), len(guns.faces), open_edges(guns.vertices, guns.faces)))
    return guns


def tank_paint(p, n, col):
    x0, x1, y, z, r, _ = GUNS[2]
    bore = (p[:, 0] > MUZZLE_X) & (np.hypot(p[:, 1] - y, p[:, 2] - z) < r * 0.75)
    col[bore] = [28, 28, 26]
    lum, sat = col.mean(1), col.max(1) - col.min(1)
    return TURRET_BAND(p) & (sat < 18) & (lum > 115)


def muzzle(turret):
    """The front end of the 37 mm gun's cylinder (the turret part has no barrels any more)."""
    x0, x1, y, z, r, _ = GUNS[1]
    return np.array([x1, y, z])


def save_obj(path, v, f, uv):
    with open(path, "w") as fh:
        fh.write("mtllib tank.mtl\nusemtl tank\n")
        fh.writelines("v %.6f %.6f %.6f\n" % tuple(p) for p in v)
        fh.writelines("vt %.6f %.6f\n" % tuple(t) for t in uv)
        fh.writelines("f %d/%d %d/%d %d/%d\n" % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1) for a, b, c in f)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    hull_target = int(sys.argv[3]) if len(sys.argv) > 3 else 440
    turret_target = int(sys.argv[4]) if len(sys.argv) > 4 else 160
    tex_size = int(sys.argv[5]) if len(sys.argv) > 5 else 512
    gear_share = sys.argv[6] if len(sys.argv) > 6 else GEAR_SHARE
    hv, hf, huv, tex = load(src)
    t = weld(hv, hf)
    print("tripo: %d triangles; welded: %d open edges" % (len(hf), open_edges(t.vertices, t.faces)))
    hull, turret = cut(t)
    if gear_share == "drop":
        hull, gear_share = drop_gear(hull), 0
    gear_share = float(gear_share)
    tip = muzzle(turret)
    parts = []
    for name, part, target, share in (("hull", hull, hull_target, gear_share), ("turret", turret, turret_target, 0)):
        v, f = reduce(part, target, share)
        print("%s reduced: %d triangles, %d open edges" % (name, len(f), open_edges(v, f)))
        parts.append((v, f))
    blocks = trimesh.util.concatenate(track_blocks(t))
    v, f = parts[0]
    parts[0] = (np.r_[v, blocks.vertices], np.r_[f, blocks.faces + len(v)])
    print("hull with the track blocks: %d triangles" % len(parts[0][1]))
    guns = gun_cylinders()
    v, f = parts[1]
    parts[1] = (np.r_[v, guns.vertices], np.r_[f, guns.faces + len(v)])
    print("turret with the barrels: %d triangles" % len(parts[1][1]))
    # one unwrap for both, so they share the texture; then split back by face count
    nv = len(parts[0][0])
    v = np.r_[parts[0][0], parts[1][0]]
    f = np.r_[parts[0][1], parts[1][1] + nv]
    v2, f2, uv = unwrap(v, f)
    on_tracks = lambda p: (np.abs(p[:, 2]) > TRACK_Z[0] - 0.001) & (p[:, 1] < TRACK_TOP + 0.004)  # noqa: E731
    img, team = bake(v2, f2, uv, hv, hf, huv, tex, tex_size, paint=tank_paint, rays=on_tracks)
    os.makedirs(out, exist_ok=True)
    img.save(os.path.join(out, "tank.png"))
    team.save(os.path.join(out, "team.png"))
    with open(os.path.join(out, "tank.mtl"), "w") as fh:
        fh.write("newmtl tank\nKd 1 1 1\nmap_Kd tank.png\n")
    nf = len(parts[0][1])
    for name, faces in (("hull", f2[:nf]), ("turret", f2[nf:])):
        used = np.unique(faces)
        remap = -np.ones(len(v2), int)
        remap[used] = np.arange(len(used))
        save_obj(os.path.join(out, name + ".obj"), v2[used], remap[faces], uv[used])
    axis = SEAT_POINT.copy()
    axis[0] = (turret.bounds[0][0] + 0.17) / 2  # the middle of the hexagonal turret (x -0.155 .. 0.17)
    axis[1] = SEAT_POINT[1] + SEAT_SLOPE * axis[0]
    with open(os.path.join(out, "parts.json"), "w") as fh:
        json.dump({"turret_axis": axis.round(4).tolist(), "muzzle": tip.round(4).tolist()}, fh)
    print("turret axis %s, muzzle %s" % (axis.round(3), tip.round(3)))


if __name__ == "__main__":
    main()
