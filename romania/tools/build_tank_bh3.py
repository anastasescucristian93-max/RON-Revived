r"""Builds the R-2's game art from make_tank.py's output (the XML is build_data.py's).

    python build_tank_bh3.py <game folder> <model folder (hull.obj, turret.obj, tank.png, team.png, parts.json)> <mod_override folder>

Writes:
  art\r2.bh3   on the skeleton of the game's Panzer (panzer.bh3): the hull on "body1", the turret
               and its guns on "pivot node 0" (the node the game turns toward the target), the
               gun's recoil node "Dummy03" left empty (no recoil), "Ammo Node 0" (the shot and the
               muzzle flash) moved to the R-2's muzzle, the exhaust smoke nodes to its rear;
  art\r2.tga   256 x 256 RGBA; the light grey turret band is player colour (grey, the alpha
               panzer.tga uses for it). The rest is turned from Tripo's grey khaki to the olive
               of the game's tanks (OLIVE_HUE, OLIVE_SAT).

A node's vertices are stored in that node's own frame: a child's frame is its parent's moved by
the child's position and turned by the conjugate of its quaternion (checked by putting the
Panzer together: romania\ENGINE_NOTES.md, "Tank models").
"""
import json
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "paradrop", "tools"))
import struct  # noqa: E402

from build_bh3 import load_obj, mesh_chunk, node_data, node_fields, parse, smooth_normals, write  # noqa: E402

TEMPLATE = "panzer.bh3"
LENGTH = 96.0   # game units, front to back (the Panzer's hull is about 106; the R-2 is the smaller tank)
TEX = 256
TRACK_X = 0.185 * LENGTH  # the track blocks (make_tank.py TRACK_Z, TRACK_TOP), in game units
TRACK_Z = 0.22 * LENGTH
TRACK_UP = 1.0           # added upward before normalising: a side normal comes out 45 degrees up
# The game's tanks (median of their textures, 2026-10-29): hue 49-70 degrees, saturation 0.4-0.6
# (mediumtank_russian 52 / 0.51, hvytank 70 / 0.40); Tripo's R-2 came out 46 / 0.20, grey next to
# them (the user: "sa o faci verzuie cum sunt restul tancurilor"). Its median is moved to these.
OLIVE_HUE = 60.0  # degrees
OLIVE_SAT = 0.50


def qmat(q):
    """The rotation of a node's quaternion (x, y, z, w), conjugated as the game applies it."""
    x, y, z, w = -q[0], -q[1], -q[2], q[3]
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def to_game(p):
    """Tripo (X length, front at +X; Y up; Z width) -> game (X width; Y length, front at -Y; Z up)."""
    p = np.atleast_2d(np.asarray(p, np.float64))
    return np.c_[-p[:, 2], -p[:, 0], p[:, 1]] * LENGTH


def nodes_of(tree):
    out = []

    def walk(ch, parent):
        out.append((ch, parent))
        for k in ch["kids"][1:]:
            walk(k, ch)
    walk(tree, None)
    return out


def world(nodes):
    """Each node's frame in the model: (rotation, origin)."""
    frames = {}
    for ch, parent in nodes:
        _, _, _, q, p = node_fields(ch["kids"][0]["data"])
        R, t = frames[id(parent)] if parent is not None else (np.eye(3), np.zeros(3))
        frames[id(ch)] = (R @ qmat(q), R @ np.array(p) + t)
    return frames


def name_of(ch):
    return node_fields(ch["kids"][0]["data"])[2].rstrip(b"\0").decode()


def team_alpha(art):
    a = np.asarray(Image.open(os.path.join(art, "panzer.tga")).convert("RGBA"))[..., 3]
    vals, counts = np.unique(a[a < 255], return_counts=True)
    return int(vals[counts.argmax()])


def olive(img, team):
    """The paint turned to the game tanks' olive: hue moved and saturation scaled so the median
    of the non-team pixels lands on OLIVE_HUE / OLIVE_SAT; brightness kept."""
    hsv = np.asarray(img.convert("HSV"), np.float64)
    paint = ~team & (hsv[..., 2] > 30)
    shift = OLIVE_HUE / 360 * 255 - np.median(hsv[paint, 0])
    scale = OLIVE_SAT * 255 / max(np.median(hsv[paint, 1]), 1)
    hsv[~team, 0] = (hsv[~team, 0] + shift) % 256
    hsv[~team, 1] = np.clip(hsv[~team, 1] * scale, 0, 255)
    return np.asarray(Image.fromarray(hsv.astype(np.uint8), "HSV").convert("RGB"), np.float64)


def texture(model, alpha):
    team = np.asarray(Image.open(os.path.join(model, "team.png")).convert("L").resize((TEX, TEX), Image.LANCZOS)) > 127
    rgb = olive(Image.open(os.path.join(model, "tank.png")).convert("RGB").resize((TEX, TEX), Image.LANCZOS), team)
    out = np.zeros((TEX, TEX, 4))
    out[..., :3], out[..., 3] = rgb, 255
    grey = rgb.mean(axis=2)
    for c in range(3):
        out[team, c] = grey[team]
    out[team, 3] = alpha
    return Image.fromarray(out.astype(np.uint8), "RGBA"), int(team.sum())


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    game, model, out = sys.argv[1:]
    art = os.path.join(game, "art")
    os.makedirs(os.path.join(out, "art"), exist_ok=True)

    hv, huv, hf = load_obj(os.path.join(model, "hull.obj"))
    tv, tuv, tf = load_obj(os.path.join(model, "turret.obj"))
    parts = json.load(open(os.path.join(model, "parts.json")))
    hull, turret = to_game(hv), to_game(tv)
    axis, muzzle = to_game(parts["turret_axis"])[0], to_game(parts["muzzle"])[0]
    smoke = to_game([[-0.47, 0.27, 0.1], [-0.47, 0.27, -0.1]])

    template = parse(open(os.path.join(art, TEMPLATE), "rb").read())
    tree = template["kids"][1]
    nodes = nodes_of(tree)
    by_name = {name_of(ch): ch for ch, _ in nodes}
    parent_of = {id(ch): parent for ch, parent in nodes}
    # centre the R-2 where the Panzer's hull is
    frames = world(nodes)
    R, t = frames[id(by_name["body1"])]
    d = template["kids"][0]["kids"][0]["data"]
    pv = np.frombuffer(d, "<f4", int.from_bytes(d[:4], "little") * 4, 4).reshape(-1, 4)[:, :3]
    pz_hull = pv[:263] @ R.T + t
    shift = (pz_hull.max(0) + pz_hull.min(0)) / 2 - (hull.max(0) + hull.min(0)) / 2
    shift[2] = 0  # both stand on Z = 0
    hull, turret, axis, muzzle, smoke = hull + shift, turret + shift, axis + shift, muzzle + shift, smoke + shift

    def place(name, target):
        """Moves a node so its origin is at target (model coordinates); its children follow."""
        ch = by_name[name]
        Rp, tp = world(nodes)[id(parent_of[id(ch)])]
        vs, vc, nm, q, _ = node_fields(ch["kids"][0]["data"])
        ch["kids"][0]["data"] = node_data(vs, vc, nm, q, list(Rp.T @ (target - tp)))

    place("pivot node 0", axis)
    place("Ammo Node 0", muzzle)
    place("smoke node 1", smoke[0])
    place("smoke node 2", smoke[1])
    frames = world(nodes)

    # vertex runs in tree order: body1 the hull, pivot node 0 the turret, every other node none
    runs = {"body1": hull, "pivot node 0": turret}
    local, counter = {}, 0
    for ch, _ in nodes:
        name = name_of(ch)
        vs, vc, nm, q, p = node_fields(ch["kids"][0]["data"])
        n = len(runs.get(name, ()))
        if n:
            R, t = frames[id(ch)]
            local[name] = (runs[name] - t) @ R  # into the node's frame: R^T (v - t)
        ch["kids"][0]["data"] = node_data(counter, n, nm, q, p)
        counter += n
    v = np.r_[local["body1"], local["pivot node 0"]]
    uv = np.r_[huv, tuv]
    f = np.r_[hf, tf + len(hv)]
    mesh = mesh_chunk(v, uv, f)
    # the track blocks' normals bent up halfway: with plain side normals the track on the side away
    # from the light was much darker than the other one in game (the user, 2026-10-29)
    whole = np.r_[hull, turret]
    n = smooth_normals(whole, f)
    tracks = (np.abs(whole[:, 0]) >= TRACK_X) & (whole[:, 2] <= TRACK_Z)
    n[tracks] += [0, 0, TRACK_UP]
    # a vertex on a fin of two back-to-back faces (the reduction makes a few) sums to no normal:
    # it takes one of its faces' (NaN normals otherwise, 2026-10-29)
    flat = np.where(np.linalg.norm(n, axis=1) < 1e-6)[0]
    for i in flat:
        a, b, c = whole[f[np.where(f == i)[0][0]]]
        n[i] = np.cross(b - a, c - a)
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    Rb, Rt = frames[id(by_name["body1"])][0], frames[id(by_name["pivot node 0"])][0]
    n_local = np.r_[n[:len(hv)] @ Rb, n[len(hv):] @ Rt]
    mesh["kids"][1]["data"] = struct.pack("<I", len(v)) + n_local.astype("<f4").tobytes() + b"\xCD" * (4 * len(v))
    print("track vertices with bent normals: %d" % int(tracks.sum()))
    root = {"type": 0, "kids": [mesh, tree]}
    with open(os.path.join(out, "art", "r2.bh3"), "wb") as fh:
        fh.write(write(root))
    print("r2.bh3: hull %d vertices, turret %d, %d triangles; model %s .. %s; turret axis %s, muzzle %s" % (
        len(hv), len(tv), len(f), np.r_[hull, turret].min(0).round(1), np.r_[hull, turret].max(0).round(1),
        axis.round(1), muzzle.round(1)))

    tex, team = texture(model, team_alpha(art))
    tex.save(os.path.join(out, "art", "r2.tga"))
    print("r2.tga: %d player-colour pixels" % team)


if __name__ == "__main__":
    main()
