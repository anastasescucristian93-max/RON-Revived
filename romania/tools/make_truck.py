r"""Turns the user's Tripo ZiS-150 / SR-101 truck body (no wheels: the game truck's own wheels go
on it, build_truck_bh3.py) into a game-ready low-poly model, whole: one closed body, no cuts.

    python make_truck.py <in.glb> <out folder> [triangles=700] [texture size=512]

The input is the Tripo FBX converted to GLB (README, "ZiS-150"). Axes: X width, Y up, Z length
with the front at +Z (measured: cab z 0.10 .. 0.27 up to y 0.28, bonnet to z 0.5, cargo bed
behind).

  1. weld the UV seams and reduce the whole body (the thin wiper arms and the steering wheel,
     fused into the body, go with the reduction);
  2. bake the texture; on the cab's windows every texel that is not the green paint becomes
     clean glass (the wipers and the steering wheel seen through the glass were painted there);
  3. the light grey band along the cargo bed becomes the player colour;
  4. write <out>\truck.obj, truck.png, team.png.
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_plane import bake, load, open_edges, save, unwrap  # noqa: E402
from make_tank import reduce, weld  # noqa: E402

CAB = dict(z0=0.10, z1=0.275, y0=0.205)  # the windows: the cab above its belt line
CARGO_Z1 = 0.09  # the cargo bed is behind this
GLASS_DARK = np.array([30, 42, 52.0])
GLASS_LIGHT = np.array([120, 145, 160.0])


def truck_paint(p, n, col):
    r, g, b = col[:, 0], col[:, 1], col[:, 2]
    cab = (p[:, 2] > CAB["z0"]) & (p[:, 2] < CAB["z1"]) & (p[:, 1] > CAB["y0"])
    paint = (g > r + 8) & (g > b + 4)  # the green body paint and the window frames
    glass = cab & ~paint & (np.abs(n[:, 1]) < 0.7)  # not the cab roof
    t = np.clip((p[glass, 1] - CAB["y0"]) / 0.07, 0, 1)[:, None]  # darker low, lighter high
    col[glass] = GLASS_DARK + (GLASS_LIGHT - GLASS_DARK) * t
    print("glass: %d texels repainted" % int(glass.sum()))
    lum, sat = col.mean(1), col.max(1) - col.min(1)
    return (p[:, 2] < CARGO_Z1) & (np.abs(n[:, 0]) > 0.6) & (sat < 20) & (lum > 140)


FRONT_SHARE = 0.6  # of the triangles for the cab, bonnet and wings: the thin wings tear first
LOW_Y = 0.13  # under the cargo bed's floor: the rear wings and the chassis


def reduce_zones(t, target, share=FRONT_SHARE):
    """The front (every corner at z > CARGO_Z1) and the cargo bed reduced apart: the same mesh,
    two budgets (one budget for all tore the cab and turned the wings into spikes)."""
    import pymeshlab
    from make_tank import collapse
    ms = pymeshlab.MeshSet()
    ms.add_mesh(pymeshlab.Mesh(vertex_matrix=np.asarray(t.vertices, float), face_matrix=np.asarray(t.faces)))
    # the fine zone: the cab, bonnet and wings, and everything low (the rear wings and the chassis)
    front = " && ".join("((z%d > %f) || (y%d < %f))" % (i, CARGO_Z1, i, LOW_Y) for i in range(3))
    back = " || ".join("((z%d <= %f) && (y%d >= %f))" % (i, CARGO_Z1, i, LOW_Y) for i in range(3))
    ms.compute_selection_by_condition_per_face(condselect=front)
    collapse(ms, int(target * share), True)
    ms.compute_selection_by_condition_per_face(condselect=back)
    collapse(ms, target - int(target * share), True)
    m = ms.current_mesh()
    return m.vertex_matrix(), m.face_matrix()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    target = int(sys.argv[3]) if len(sys.argv) > 3 else 700
    tex_size = int(sys.argv[4]) if len(sys.argv) > 4 else 512
    hv, hf, huv, tex = load(src)
    t = weld(hv, hf)
    body = max(t.split(only_watertight=False), key=lambda p: len(p.faces))
    print("tripo: %d triangles; the body %d, %d open edges (loose specks of %d triangles left out)" % (
        len(hf), len(body.faces), open_edges(body.vertices, body.faces), len(t.faces) - len(body.faces)))
    v, f = reduce_zones(body, target)
    if open_edges(v, f):
        sys.exit("the reduction opened %d edges" % open_edges(v, f))
    print("reduced: %d triangles, %d open edges" % (len(f), open_edges(v, f)))
    v, f, uv = unwrap(v, f)
    img, team = bake(v, f, uv, hv, hf, huv, tex, tex_size, paint=truck_paint)
    os.makedirs(out, exist_ok=True)
    save(out, v, f, uv, img, team)
    for ext in ("obj", "mtl", "png"):  # save() names them plane.*
        os.replace(os.path.join(out, "plane." + ext), os.path.join(out, "truck." + ext))
    with open(os.path.join(out, "truck.obj")) as fh:
        text = fh.read().replace("plane.mtl", "truck.mtl")
    open(os.path.join(out, "truck.obj"), "w").write(text)
    with open(os.path.join(out, "truck.mtl")) as fh:
        text = fh.read().replace("plane.png", "truck.png")
    open(os.path.join(out, "truck.mtl"), "w").write(text)


if __name__ == "__main__":
    main()
