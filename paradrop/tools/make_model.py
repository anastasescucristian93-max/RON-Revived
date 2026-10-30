r"""Turns the Tripo transport helicopter into a game-ready low-poly model.

    python make_model.py <in.glb> <out folder> [triangles] [texture size]

Steps:
  1. drop the lowered rear ramp (a flat plate hanging under the tail);
  2. cut the model along its long axis (Z = 0) and mirror the +Z half, so both sides match;
  3. reduce the whole model to <triangles> (default 500) with quadric edge collapse on the
     shape alone (the result is symmetric up to the collapse order);
  4. unwrap the low model (xatlas) and bake the colours of the detailed one into a new texture
     (<texture size>, default 512): each texel takes the colour of the closest point of the
     detailed surface;
  5. drop the few loose triangles left of the rotor blade stubs;
  6. write <out>\heli.obj, heli.mtl, heli.png and rotors.json (the tops of the two rotor
     pylons, where the game's spinning rotor discs go).

Reducing with the Tripo UVs does not work: their many seams pin the edges, the texture tears
and the shape goes elsewhere (the engines vanished). Reducing only the half and mirroring it
does not work either: the open cut edge makes the collapse fold the sides into long fans.

Axes as Tripo exports them: X length (nose at -X), Y up, Z width.
"""
import json
import os
import sys

import numpy as np
import pymeshlab
import trimesh
from PIL import Image

RAMP = lambda c: (c[:, 0] > 0.37) & (c[:, 1] < 0.085) & (np.abs(c[:, 2]) < 0.085)  # noqa: E731  face centres of the ramp
# blade stubs over the hub caps (caps end at y 0.354 front, 0.453 rear); the game draws the rotors
STUBS = lambda c: ((c[:, 0] < -0.2) & (c[:, 1] > 0.354)) | ((c[:, 0] > 0.2) & (c[:, 1] > 0.453))  # noqa: E731


def load(path):
    m = trimesh.load(path, force="mesh")
    tex = m.visual.material.baseColorTexture
    return np.asarray(m.vertices, np.float64), np.asarray(m.faces), np.asarray(m.visual.uv, np.float64), tex


def drop_faces(v, f, uv, mask):
    f = f[~mask]
    used = np.unique(f)
    remap = -np.ones(len(v), int)
    remap[used] = np.arange(len(used))
    return v[used], remap[f], uv[used]


def cut_half(v, f, uv):
    """Keeps the +Z half. Triangles crossing Z = 0 are split, new points get interpolated UVs."""
    v, uv = list(v), list(uv)
    out = []
    cache = {}

    def mid(a, b):
        key = (min(a, b), max(a, b))
        if key not in cache:
            za, zb = v[a][2], v[b][2]
            t = za / (za - zb)
            p = v[a] + (v[b] - v[a]) * t
            p[2] = 0.0
            v.append(p)
            uv.append(uv[a] + (uv[b] - uv[a]) * t)
            cache[key] = len(v) - 1
        return cache[key]

    for tri in f:
        z = [v[i][2] for i in tri]
        inside = [zz >= 0 for zz in z]
        n = sum(inside)
        if n == 3:
            out.append(list(tri))
        elif n == 0:
            continue
        else:
            # rotate so the odd vertex is first
            k = inside.index(n == 1)
            a, b, c = tri[k], tri[(k + 1) % 3], tri[(k + 2) % 3]
            ab, ac = mid(a, b), mid(a, c)
            if n == 1:  # a inside
                out.append([a, ab, ac])
            else:  # a outside, b and c inside
                out += [[ab, b, c], [ab, c, ac]]
    v, uv = np.array(v), np.array(uv)
    v[np.abs(v[:, 2]) < 1e-6, 2] = 0.0
    return drop_faces(v, np.array(out), uv, np.zeros(len(out), bool))


def mirror(v, f, uv):
    n = len(v)
    vm = v * [1, 1, -1]
    fm = f[:, ::-1] + n
    return np.r_[v, vm], np.r_[f, fm], np.r_[uv, uv]


WHEELS = "(y0 < %f) && (y1 < %f) && (y2 < %f)" % ((0.068,) * 3)  # faces under the hull: wheels, struts
BODY = "(y0 >= %f) || (y1 >= %f) || (y2 >= %f)" % ((0.068,) * 3)  # the rest (muparser has no "!")
WHEEL_SHARE = 0.24  # of the triangles; one budget for all would crush the wheels
# The rear wheels are too small for any reduction (they came out as flat tabs), so they are
# cut out of the reduced model and rebuilt as 8-sided cylinders on a short strut. Measured on
# the Tripo model: centre x 0.367, y 0.032, tyre z 0.060..0.076, strut up to the hull at y 0.07.
REAR_WHEEL = dict(x=0.367, y=0.032, z=0.068, radius=0.019, half_width=0.009, sides=8)
REAR_STRUT = dict(x=0.367, z=0.055, y0=0.032, y1=0.075, half=0.004)
REAR_CUT = lambda c: (c[:, 0] > 0.34) & (c[:, 0] < 0.395) & (c[:, 1] < 0.066) & (np.abs(c[:, 2]) > 0.035)  # noqa: E731


def decimate(v, f, target):
    """Shape only. Closes the hole the ramp left, then reduces the wheels and the body apart."""
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=v, face_matrix=f))
    ms.meshing_merge_close_vertices(threshold=pymeshlab.PercentageValue(0.01))
    ms.meshing_remove_duplicate_faces()
    ms.meshing_remove_unreferenced_vertices()
    ms.meshing_close_holes(maxholesize=300)
    # with selected=True the target counts the selected faces only
    wheel_target = int(target * WHEEL_SHARE)
    ms.compute_selection_by_condition_per_face(condselect=WHEELS)
    ms.meshing_decimation_quadric_edge_collapse(targetfacenum=wheel_target, selected=True,
                                                preservenormal=True, autoclean=True)
    ms.compute_selection_by_condition_per_face(condselect=BODY)
    ms.meshing_decimation_quadric_edge_collapse(targetfacenum=target - wheel_target, selected=True,
                                                preservenormal=True, autoclean=True)
    m = ms.current_mesh()
    return rebuild_rear_wheels(*drop_loose_tops(m.vertex_matrix(), m.face_matrix()))


def cylinder(cx, cy, cz, radius, half_width, sides):
    """Closed cylinder along Z."""
    a = np.arange(sides) * 2 * np.pi / sides
    ring = np.c_[cx + radius * np.cos(a), cy + radius * np.sin(a)]
    v = np.r_[np.c_[ring, np.full(sides, cz - half_width)], np.c_[ring, np.full(sides, cz + half_width)],
              [[cx, cy, cz - half_width], [cx, cy, cz + half_width]]]
    f = []
    for i in range(sides):
        j = (i + 1) % sides
        f += [[i, j, sides + j], [i, sides + j, sides + i],  # tread
              [2 * sides, j, i], [2 * sides + 1, sides + i, sides + j]]  # caps
    return v, np.array(f)


def box(x0, x1, y0, y1, z0, z1):
    v = np.array([[x, y, z] for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)], float)
    f = np.array([[0, 1, 3], [0, 3, 2], [4, 6, 7], [4, 7, 5], [0, 4, 5], [0, 5, 1],
                  [2, 3, 7], [2, 7, 6], [0, 2, 6], [0, 6, 4], [1, 5, 7], [1, 7, 3]])
    return v, f


def rebuild_rear_wheels(v, f):
    keep = ~REAR_CUT(v[f].mean(1))
    parts = [(v, f[keep])]
    w, s = REAR_WHEEL, REAR_STRUT
    for side in (1, -1):
        parts.append(cylinder(w["x"], w["y"], side * w["z"], w["radius"], w["half_width"], w["sides"]))
        parts.append(box(s["x"] - s["half"], s["x"] + s["half"], s["y0"], s["y1"],
                         side * s["z"] - s["half"], side * s["z"] + s["half"]))
    vs, fs, n = [], [], 0
    for pv, pf in parts:
        vs.append(pv)
        fs.append(pf + n)
        n += len(pv)
    v, f = np.concatenate(vs), np.concatenate(fs)
    used = np.unique(f)
    remap = -np.ones(len(v), int)
    remap[used] = np.arange(len(used))
    return v[used], remap[f]


def drop_loose_tops(v, f, max_faces=4, above=0.3):
    """Removes the few loose triangles left of the rotor blade stubs over the hubs."""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    rows = np.repeat(np.arange(len(f)), 3)
    g = coo_matrix((np.ones(len(rows)), (rows, f.ravel())), shape=(len(f), len(v))).tocsr()
    n, label = connected_components(g @ g.T, directed=False)
    drop = np.zeros(len(f), bool)
    for i in range(n):
        faces = label == i
        if faces.sum() <= max_faces and v[f[faces]][:, :, 1].min() > above:
            drop |= faces
    f = f[~drop]
    used = np.unique(f)
    remap = -np.ones(len(v), int)
    remap[used] = np.arange(len(used))
    return v[used], remap[f]


def rotor_hubs(v):
    """Tops of the front and rear rotor pylons, centred on Z = 0."""
    hubs = []
    for side in (v[:, 0] < -0.2, v[:, 0] > 0.2):
        s = v[side]
        top = s[s[:, 1] > s[:, 1].max() - 0.01]
        hubs.append([round(float(top[:, 0].mean()), 4), round(float(s[:, 1].max()), 4), 0.0])
    return hubs


def unwrap(v, f):
    import xatlas
    vmap, f2, uv = xatlas.parametrize(v.astype(np.float32), f.astype(np.uint32))
    return v[vmap], f2.astype(int), uv.astype(np.float64)


def bake(lo_v, lo_f, lo_uv, hi_v, hi_f, hi_uv, hi_tex, size, pad=4):
    """Colour of the closest detailed surface point for every texel the low UVs cover."""
    from scipy import ndimage
    hi = trimesh.Trimesh(hi_v, hi_f, process=False)
    tex = np.asarray(hi_tex.convert("RGB"), np.float64)
    th, tw = tex.shape[:2]
    img = np.zeros((size, size, 3))
    filled = np.zeros((size, size), bool)
    pix, pts = [], []
    for tri in lo_f:
        p = lo_uv[tri] * size  # texel space, V up
        x0, x1 = int(np.floor(p[:, 0].min())), int(np.ceil(p[:, 0].max()))
        y0, y1 = int(np.floor(p[:, 1].min())), int(np.ceil(p[:, 1].max()))
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = p
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1 - w0 - w1
        e = -0.02  # a little outside the triangle too, against seam gaps
        inside = (w0 >= e) & (w1 >= e) & (w2 >= e) & (gx >= 0) & (gx < size) & (gy >= 0) & (gy < size)
        if not inside.any():
            continue
        w = np.c_[w0[inside], w1[inside], w2[inside]]
        pts.append(w @ lo_v[tri])
        pix.append(np.c_[gx[inside].astype(int), gy[inside].astype(int)])
    pts, pix = np.concatenate(pts), np.concatenate(pix)
    closest, _, tid = trimesh.proximity.closest_point(hi, pts)
    bary = trimesh.triangles.points_to_barycentric(hi.triangles[tid], closest)
    uv = np.einsum("ij,ijk->ik", bary, hi_uv[hi_f[tid]])
    tx = np.clip((uv[:, 0] % 1) * tw, 0, tw - 1).astype(int)
    ty = np.clip((1 - uv[:, 1] % 1) * th, 0, th - 1).astype(int)
    rows = size - 1 - pix[:, 1]
    img[rows, pix[:, 0]] = tex[ty, tx]
    filled[rows, pix[:, 0]] = True
    # pad the charts so filtering never reads the empty background
    _, (iy, ix) = ndimage.distance_transform_edt(~filled, return_indices=True)
    near = ndimage.distance_transform_edt(~filled) <= pad
    img[near] = img[iy[near], ix[near]]
    return Image.fromarray(img.astype(np.uint8))


def save(out, v, f, uv, tex):
    os.makedirs(out, exist_ok=True)
    tex.save(os.path.join(out, "heli.png"))
    with open(os.path.join(out, "heli.mtl"), "w") as fh:
        fh.write("newmtl heli\nKd 1 1 1\nmap_Kd heli.png\n")
    with open(os.path.join(out, "heli.obj"), "w") as fh:
        fh.write("mtllib heli.mtl\nusemtl heli\n")
        fh.writelines("v %.6f %.6f %.6f\n" % tuple(p) for p in v)
        fh.writelines("vt %.6f %.6f\n" % tuple(t) for t in uv)
        fh.writelines("f %d/%d %d/%d %d/%d\n" % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1) for a, b, c in f)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    target = int(sys.argv[3]) if len(sys.argv) > 3 else 500
    tex_size = int(sys.argv[4]) if len(sys.argv) > 4 else 512
    v, f, uv, tex = load(src)
    print("tripo: %d triangles" % len(f))
    v, f, uv = drop_faces(v, f, uv, RAMP(v[f].mean(1)) | STUBS(v[f].mean(1)))
    print("without the ramp and the blade stubs: %d" % len(f))
    hv, hf, huv = mirror(*cut_half(v, f, uv))
    print("symmetric: %d" % len(hf))
    v, f = decimate(hv, hf, target - 80)  # the rebuilt rear wheels and struts add about 80
    v, f, uv = unwrap(v, f)
    print("model: %d triangles, %d vertices" % (len(f), len(v)))
    save(out, v, f, uv, bake(v, f, uv, hv, hf, huv, tex, tex_size))
    hubs = rotor_hubs(hv)  # from the detailed model: the reduced hub caps are not reliable
    with open(os.path.join(out, "rotors.json"), "w") as fh:
        json.dump({"front": hubs[0], "rear": hubs[1]}, fh)
    print("rotor hubs (x, y, z): front %s, rear %s" % tuple(hubs))


if __name__ == "__main__":
    main()
