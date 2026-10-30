r"""Turns the user's Tripo IAR-80 into a game-ready low-poly model, whole: no cutting, mirroring
or dropping of parts (the user's rule: those leave holes and missing faces).

    python make_plane.py <in.glb> <out folder> [triangles] [texture size]

Steps:
  1. weld the UV-seam duplicates (else every seam is an open edge for the reduction);
  2. reduce the whole model to <triangles> (default 400) with quadric edge collapse,
     topology and open edges preserved; the open-edge count before and after is printed;
  3. unwrap the low model (xatlas) and bake the colours of the detailed one into a new texture
     (<texture size>, default 512): each texel takes the colour of the closest point of the
     detailed surface;
  4. while baking, paint over the German crosses Gemini drew with the olive around them, and
     paint the Romanian Michael's Cross in their place (on the wings, top and bottom, and on
     both sides of the fuselage);
  5. write <out>\plane.obj, plane.mtl, plane.png and team.png (white where the light grey bands
     are: the player colour).

Axes as Tripo exports them: X length (nose at +X), Y up, Z the wingspan. The decal and band
places were measured on top and side renders of iar80_tripo.glb.
"""
import os
import sys
from collections import Counter

import numpy as np
import pymeshlab
import trimesh
from PIL import Image

# Michael's Cross: centre (x, y or z) and radius, in Tripo units
WING_CROSS = dict(x=0.18, z=0.312, r=0.06)     # both wings, top and bottom
BODY_CROSS = dict(x=-0.084, y=0.174, r=0.042)  # both sides of the fuselage
BLUE, YELLOW, RED = np.array([25, 55, 140.0]), np.array([225, 185, 35.0]), np.array([175, 30, 30.0])
# light grey bands = player colour: zones, then the colour decides
WING_BAND = lambda p: (np.abs(p[:, 2]) > 0.125) & (np.abs(p[:, 2]) < 0.215)  # noqa: E731
BODY_BAND = lambda p: (p[:, 0] > -0.265) & (p[:, 0] < -0.165) & (np.abs(p[:, 2]) < 0.07)  # noqa: E731


def load(path):
    m = trimesh.load(path, force="mesh")
    return (np.asarray(m.vertices, np.float64), np.asarray(m.faces), np.asarray(m.visual.uv, np.float64),
            m.visual.material.baseColorTexture)


def open_edges(v, f):
    e = Counter(map(tuple, np.sort(np.r_[f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]], 1)))
    return sum(1 for c in e.values() if c == 1)


def reduce(v, f, target):
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=v, face_matrix=f))
    ms.meshing_merge_close_vertices(threshold=pymeshlab.PercentageValue(0.01))
    ms.meshing_remove_duplicate_faces()
    ms.meshing_remove_unreferenced_vertices()
    m = ms.current_mesh()
    print("welded: %d triangles, %d open edges" % (m.face_number(), open_edges(m.vertex_matrix(), m.face_matrix())))
    ms.meshing_decimation_quadric_edge_collapse(targetfacenum=target, preservetopology=True, preserveboundary=True,
                                                boundaryweight=2.0, preservenormal=True, optimalplacement=True,
                                                planarquadric=True, autoclean=True)
    m = ms.current_mesh()
    v, f = m.vertex_matrix(), m.face_matrix()
    print("reduced: %d triangles, %d open edges" % (len(f), open_edges(v, f)))
    return v, f


def unwrap(v, f):
    import xatlas
    vmap, f2, uv = xatlas.parametrize(v.astype(np.float32), f.astype(np.uint32))
    return v[vmap], f2.astype(int), uv.astype(np.float64)


def cross_mask(a, b):
    """Michael's Cross in units of its radius: (blue, yellow, red) masks. Arms that widen outwards,
    a blue edge, the tricolour roundel in the middle."""
    def arms(k):
        return (((np.abs(b) <= k * (0.17 + 0.24 * np.abs(a))) & (np.abs(a) <= k)) |
                ((np.abs(a) <= k * (0.17 + 0.24 * np.abs(b))) & (np.abs(b) <= k)))
    r = np.hypot(a, b)
    blue = arms(1.0) | (r < 0.36)
    yellow = (arms(0.80) & (r >= 0.36)) | ((r < 0.27) & (r >= 0.15))
    red = r < 0.15
    return blue & ~yellow & ~red, yellow, red


def decals(p, n, col):
    """Paints the crosses over the baked colours (p: Tripo points, n: their surface normals,
    col: their colours). Each surface (wing top, wing bottom, each fuselage side) takes the
    olive around its own cross: the undersides are darker."""
    w = WING_CROSS
    a, b = (p[:, 0] - w["x"]) / w["r"], (np.abs(p[:, 2]) - w["z"]) / w["r"]
    for side in (1, -1):
        for face in (1, -1):
            paint_cross(col, a, b, (np.sign(p[:, 2]) == side) & (np.sign(n[:, 1]) == face) & (np.abs(p[:, 2]) > 0.2))
    c = BODY_CROSS
    a, b = (p[:, 0] - c["x"]) / c["r"], (p[:, 1] - c["y"]) / c["r"]
    for side in (1, -1):
        paint_cross(col, a, b, (np.sign(p[:, 2]) == side) & (np.abs(p[:, 2]) > 0.02) & (np.abs(p[:, 2]) < 0.09))


def paint_cross(col, a, b, zone):
    r = np.hypot(a, b)
    ring = zone & (r > 1.35) & (r < 1.8)
    if not ring.any():
        return
    base = np.median(col[ring], axis=0)
    col[zone & (r < 1.3)] = base  # the old cross away
    blue, yellow, red = cross_mask(a, b)
    col[zone & blue], col[zone & yellow], col[zone & red] = BLUE, YELLOW, RED


def plane_paint(p, n, col):
    """The IAR-80's player colour (light grey in the band zones) and crosses."""
    lum, sat = col.mean(1), col.max(1) - col.min(1)
    team = (WING_BAND(p) | BODY_BAND(p)) & (sat < 14) & (lum > 95)
    decals(p, n, col)
    return team


RAY_START = 0.02  # Tripo units outside the low surface


def bake(lo_v, lo_f, lo_uv, hi_v, hi_f, hi_uv, hi_tex, size, paint=plane_paint, rays=None, pad=4):
    """Colour of the closest detailed surface point for every texel the low UVs cover (for the
    texels rays(points) picks: of the first detailed surface straight in through the low face);
    then paint(points, normals, colours) changes the colours in place and returns the
    player-colour mask."""
    from scipy import ndimage
    hi = trimesh.Trimesh(hi_v, hi_f, process=False)
    tex = np.asarray(hi_tex.convert("RGB"), np.float64)
    th, tw = tex.shape[:2]
    pix, pts, nrm = [], [], []
    for tri in lo_f:
        q = lo_uv[tri] * size
        x0, x1 = int(np.floor(q[:, 0].min())), int(np.ceil(q[:, 0].max()))
        y0, y1 = int(np.floor(q[:, 1].min())), int(np.ceil(q[:, 1].max()))
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = q
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1 - w0 - w1
        e = -0.02
        inside = (w0 >= e) & (w1 >= e) & (w2 >= e) & (gx >= 0) & (gx < size) & (gy >= 0) & (gy < size)
        if not inside.any():
            continue
        pts.append(np.c_[w0[inside], w1[inside], w2[inside]] @ lo_v[tri])
        pix.append(np.c_[gx[inside].astype(int), gy[inside].astype(int)])
        fn = np.cross(lo_v[tri[1]] - lo_v[tri[0]], lo_v[tri[2]] - lo_v[tri[0]])
        nrm.append(np.repeat([fn / max(np.linalg.norm(fn), 1e-12)], inside.sum(), axis=0))
    pts, pix, nrm = np.concatenate(pts), np.concatenate(pix), np.concatenate(nrm)
    closest, _, tid = trimesh.proximity.closest_point(hi, pts)
    if rays is not None:
        # these texels look straight in through their face instead: the first surface behind it
        sel = np.flatnonzero(rays(pts))
        loc, ray, tri = hi.ray.intersects_location(pts[sel] + nrm[sel] * RAY_START, -nrm[sel], multiple_hits=False)
        closest[sel[ray]], tid[sel[ray]] = loc, tri
        print("rays: %d of %d texels hit" % (len(ray), len(sel)))
    bary = trimesh.triangles.points_to_barycentric(hi.triangles[tid], closest)
    uv = np.einsum("ij,ijk->ik", bary, hi_uv[hi_f[tid]])
    tx = np.clip((uv[:, 0] % 1) * tw, 0, tw - 1).astype(int)
    ty = np.clip((1 - uv[:, 1] % 1) * th, 0, th - 1).astype(int)
    col = tex[ty, tx]
    team = paint(closest, hi.face_normals[tid], col)

    img, mask = np.zeros((size, size, 3)), np.zeros((size, size))
    filled = np.zeros((size, size), bool)
    rows = size - 1 - pix[:, 1]
    img[rows, pix[:, 0]] = col
    mask[rows, pix[:, 0]] = team * 255.0
    filled[rows, pix[:, 0]] = True
    _, (iy, ix) = ndimage.distance_transform_edt(~filled, return_indices=True)
    near = ndimage.distance_transform_edt(~filled) <= pad
    img[near], mask[near] = img[iy[near], ix[near]], mask[iy[near], ix[near]]
    print("player colour: %d of %d texels" % (int(team.sum()), len(team)))
    return Image.fromarray(img.astype(np.uint8)), Image.fromarray(mask.astype(np.uint8))


def save(out, v, f, uv, tex, team):
    os.makedirs(out, exist_ok=True)
    tex.save(os.path.join(out, "plane.png"))
    team.save(os.path.join(out, "team.png"))
    with open(os.path.join(out, "plane.mtl"), "w") as fh:
        fh.write("newmtl plane\nKd 1 1 1\nmap_Kd plane.png\n")
    with open(os.path.join(out, "plane.obj"), "w") as fh:
        fh.write("mtllib plane.mtl\nusemtl plane\n")
        fh.writelines("v %.6f %.6f %.6f\n" % tuple(p) for p in v)
        fh.writelines("vt %.6f %.6f\n" % tuple(t) for t in uv)
        fh.writelines("f %d/%d %d/%d %d/%d\n" % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1) for a, b, c in f)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    target = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    tex_size = int(sys.argv[4]) if len(sys.argv) > 4 else 512
    hv, hf, huv, tex = load(src)
    print("tripo: %d triangles, %d open edges" % (len(hf), open_edges(hv, hf)))
    v, f = reduce(hv, hf, target)
    v, f, uv = unwrap(v, f)
    print("model: %d triangles, %d vertices" % (len(f), len(v)))
    save(out, v, f, uv, *bake(v, f, uv, hv, hf, huv, tex, tex_size))


if __name__ == "__main__":
    main()
