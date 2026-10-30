r"""Builds the IAR-80's game art from make_plane.py's output (the XML is build_data.py's).

    python build_plane_bh3.py <game folder> <model folder (plane.obj, plane.png, team.png)> <mod_override folder>

Writes:
  art\iar80.bh3   the mesh, whole on the skeleton of the game's P-40 (fighter_p40.bh3: root,
                  wing guns "ammo node 0" / "1", "front body" -> "ammo node 2" the propeller disc,
                  smoke nodes), so the P-40's animations and events work unchanged; placed and
                  sized like the P-40 (wingspan, nose tip), whose graphics entry scales it 0.765;
  art\iar80.tga   256 x 256 RGBA; the light grey bands are player colour (grey, alpha 57, as in
                  fighter_p40.tga).

The BH3 chunk code is the Airdrop's (paradrop\tools\build_bh3.py).
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "paradrop", "tools"))
from build_bh3 import load_obj, mesh_chunk, node_data, node_fields, parse, write  # noqa: E402

TEMPLATE = "fighter_p40.bh3"
TEX = 256
TEAM_ALPHA = 57


def p40_frame(art):
    """The P-40 mesh's wingspan (X), nose tip (lowest Y) and the nose tip's height (Z)."""
    root = parse(open(os.path.join(art, TEMPLATE), "rb").read())
    data = root["kids"][0]["kids"][0]["data"]
    v = np.frombuffer(data, "<f4", offset=4).reshape(-1, 4)[:, :3]
    nose = v[v[:, 1] < v[:, 1].min() + 3]
    return root, np.ptp(v[:, 0]), v[:, 1].min(), nose[:, 2].mean()


def to_game(v, span, nose_y, nose_z):
    """Tripo (X length, nose at +X; Y up; Z wingspan) -> game (X wingspan; Y length, nose at -Y;
    Z up), a rotation (no mirror), then scaled to the P-40's span and moved onto its nose tip."""
    g = np.c_[-v[:, 2], -v[:, 0], v[:, 1]]
    g *= span / np.ptp(g[:, 0])
    tip = g[g[:, 1] < g[:, 1].min() + 0.5].mean(0)
    return g + [-(g[:, 0].max() + g[:, 0].min()) / 2, nose_y - tip[1], nose_z - tip[2]]


def skeleton(template, n_verts):
    """The P-40's nodes: the root carries the whole mesh, the others point past it."""
    nodes = template["kids"][1]

    def walk(ch):
        d = ch["kids"][0]
        vs, vc, name, q, p = node_fields(d["data"])
        d["data"] = node_data(0, n_verts, name, q, p) if vc else node_data(n_verts if vs else 0, 0, name, q, p)
        for k in ch["kids"][1:]:
            walk(k)
    walk(nodes)
    return nodes


def texture(model):
    rgb = np.asarray(Image.open(os.path.join(model, "plane.png")).convert("RGB").resize((TEX, TEX), Image.LANCZOS),
                     np.float64)
    team = np.asarray(Image.open(os.path.join(model, "team.png")).convert("L").resize((TEX, TEX), Image.LANCZOS)) > 127
    out = np.zeros((TEX, TEX, 4))
    out[..., :3], out[..., 3] = rgb, 255
    grey = rgb.mean(axis=2)
    for c in range(3):
        out[team, c] = grey[team]
    out[team, 3] = TEAM_ALPHA
    return Image.fromarray(out.astype(np.uint8), "RGBA"), int(team.sum())


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    game, model, out = sys.argv[1:]
    art = os.path.join(game, "art")
    os.makedirs(os.path.join(out, "art"), exist_ok=True)

    v, uv, f = load_obj(os.path.join(model, "plane.obj"))
    template, span, nose_y, nose_z = p40_frame(art)
    gv = to_game(v, span, nose_y, nose_z)
    tri = gv[f]
    volume = np.einsum("ij,ij->i", tri[:, 0], np.cross(tri[:, 1], tri[:, 2])).sum() / 6
    if volume <= 0:
        sys.exit("plane.obj: faces wound inwards (volume %.1f)" % volume)
    root = {"type": 0, "kids": [mesh_chunk(gv, uv, f), skeleton(template, len(gv))]}
    with open(os.path.join(out, "art", "iar80.bh3"), "wb") as fh:
        fh.write(write(root))
    print("iar80.bh3: %d vertices, %d triangles, size %s .. %s" % (
        len(gv), len(f), gv.min(0).round(1), gv.max(0).round(1)))

    tex, team = texture(model)
    tex.save(os.path.join(out, "art", "iar80.tga"))
    print("iar80.tga: %d player-colour pixels" % team)


if __name__ == "__main__":
    main()
