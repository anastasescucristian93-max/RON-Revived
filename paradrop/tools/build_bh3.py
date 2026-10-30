r"""Builds the transport helicopter's art from make_model.py's output (the XML is build_data.py's).

    python build_bh3.py <game folder> <model folder (heli.obj, heli.png, rotors.json)> <mod_override folder>

Writes:
  art\chinook.bh3            the mesh, with the skeleton of the game's chopper.bh3 (same nodes, so
                             the game's animation code finds what it expects); "ammo node 2" moved
                             to the front rotor hub, "ammo node 3" to the rear one and turned like
                             node 2, so both get the main rotor's spinning disc;
  art\chinook.tga            256 x 256 RGBA; the red band becomes player colour (grey, alpha 66,
                             as in chopper.tga);
  art\chinook_propspin.bha   helicopter_propspin.bha with node 3's track (the chopper's tail rotor)
                             replaced by a copy of node 2's (the main rotor).

BH3 (see ENGINE_NOTES.md): chunks [u32 size incl. header][u16 type][u16 children]. Mesh (type 1):
2 = vertices (u32 n, n x float4, w = 1), 3 = normals (u32 n, n x float3, then n x 4 bytes of
0xCD fill), 4 = UVs (u32 n, n x float2, V from the top), 5 = triangles (u32 index count, u16,
wound against the normals). Nodes (type 6) start with a type 7 chunk
[u32 first vertex][u32 vertex count][u32 name length][name\0][quaternion xyzw][position xyz]
[the quaternion's x again]; the children's positions are in mesh coordinates.
"""
import copy
import json
import os
import re
import struct
import sys

import numpy as np
from PIL import Image

LENGTH = 150.0      # game units nose to tail (the gunship chopper.bh3 is 115)
GROUND = 0.04       # model Y that becomes 0 (chopper.bh3 runs from -5.7 to 28.9)
TEX = 256
TEAM_ALPHA = 66


# ---- chunks -----------------------------------------------------------------------------

def parse(d, off=0):
    size, typ, n = struct.unpack_from("<IHH", d, off)
    if typ in (0, 1, 6, 8):
        kids, c = [], off + 8
        while c < off + size:
            kid = parse(d, c)
            kids.append(kid)
            c += kid["size"]
        return {"type": typ, "n": n, "kids": kids, "size": size}
    return {"type": typ, "n": n, "data": d[off + 8:off + size], "size": size}


def write(ch):
    if "kids" in ch:
        body = b"".join(write(k) for k in ch["kids"])
        n = len(ch["kids"])
    else:
        body, n = ch["data"], ch["n"]
    return struct.pack("<IHH", 8 + len(body), ch["type"], n) + body


def leaf(typ, data):
    return {"type": typ, "n": 0, "data": data}


# ---- mesh -------------------------------------------------------------------------------

def load_obj(path):
    v, vt, f = [], [], []
    for line in open(path):
        p = line.split()
        if not p:
            continue
        if p[0] == "v":
            v.append([float(x) for x in p[1:4]])
        elif p[0] == "vt":
            vt.append([float(x) for x in p[1:3]])
        elif p[0] == "f":
            f.append([[int(x) - 1 for x in c.split("/")[:2]] for c in p[1:4]])
    v, vt, f = np.array(v), np.array(vt), np.array(f)
    # one game vertex per (position, uv) pair
    keys, out_v, out_uv, faces = {}, [], [], []
    for tri in f:
        row = []
        for vi, ti in tri:
            if (vi, ti) not in keys:
                keys[(vi, ti)] = len(out_v)
                out_v.append(v[vi])
                out_uv.append(vt[ti])
            row.append(keys[(vi, ti)])
        faces.append(row)
    return np.array(out_v), np.array(out_uv), np.array(faces)


def smooth_normals(v, f):
    """Area-weighted, shared by every vertex at the same position (UV seams do not crease)."""
    fn = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
    pos = {}
    key = [tuple(np.round(p, 5)) for p in v]
    for k in key:
        pos.setdefault(k, np.zeros(3))
    for tri, n in zip(f, fn):
        for i in tri:
            pos[key[i]] += n
    n = np.array([pos[k] for k in key])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def to_game(p):
    p = np.asarray(p, np.float64)
    return np.c_[p[..., 0] * LENGTH, (p[..., 1] - GROUND) * LENGTH, p[..., 2] * LENGTH]


def mesh_chunk(v, uv, f):
    n = len(v)
    normals = smooth_normals(v, f)
    verts = np.c_[v, np.ones(n)].astype("<f4").tobytes()
    norms = normals.astype("<f4").tobytes() + b"\xCD" * (4 * n)
    uvs = np.c_[uv[:, 0], 1 - uv[:, 1]].astype("<f4").tobytes()
    idx = f[:, [0, 2, 1]].astype("<u2").tobytes()  # the game winds against the normals
    return {"type": 1, "kids": [
        leaf(2, struct.pack("<I", n) + verts),
        leaf(3, struct.pack("<I", n) + norms),
        leaf(4, struct.pack("<I", n) + uvs),
        leaf(5, struct.pack("<I", len(f) * 3) + idx),
    ]}


# ---- skeleton ---------------------------------------------------------------------------

def node_fields(data):
    vs, vc, ln = struct.unpack_from("<III", data)
    name = data[12:12 + ln]
    q = list(struct.unpack_from("<4f", data, 12 + ln))
    p = list(struct.unpack_from("<3f", data, 28 + ln))
    return vs, vc, name, q, p


def node_data(vs, vc, name, q, p):
    return struct.pack("<III", vs, vc, len(name)) + name + struct.pack("<8f", *q, *p, q[0])


def skeleton(template, n_verts, front, rear):
    nodes = template["kids"][1]
    all_nodes = []  # names repeat ("dummy" twice), so keep the chunks themselves

    def walk(ch):
        all_nodes.append(ch["kids"][0])
        for k in ch["kids"][1:]:
            walk(k)
    walk(nodes)
    rotor_q = next(node_fields(d["data"])[3] for d in all_nodes if node_fields(d["data"])[2] == b"ammo node 2\0")
    for d in all_nodes:
        vs, vc, name, q, p = node_fields(d["data"])
        if vc:
            vs, vc = 0, n_verts  # center: the whole mesh hangs on it
        elif vs:
            vs = n_verts  # the other nodes point past the mesh; the root keeps 0
        if name == b"ammo node 2\0":
            p = list(front)
        elif name == b"ammo node 3\0":
            q, p = list(rotor_q), list(rear)
        d["data"] = node_data(vs, vc, name, q, p)
    return nodes


# ---- animation --------------------------------------------------------------------------

def propspin(bha):
    """The chopper's propspin with the tail rotor's track (node 3) replaced by the main rotor's."""
    root = parse(bha)
    center = root["kids"][0]["kids"][1]  # root track -> [data, center track]
    tracks = center["kids"]  # [data, ammo 0, ammo 1, ammo 2, ammo 3, ...]
    tracks[4] = copy.deepcopy(tracks[3])
    return write(root)


# ---- texture ----------------------------------------------------------------------------

# The baked windows are a grey blue (about 87, 103, 111) that reads as painted metal in game, so
# they are repainted as glass: dark blue in shade to light sky blue where lit.
GLASS_DARK = np.array([18, 52, 115.0])
GLASS_LIGHT = np.array([120, 195, 245.0])


def paint_glass(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    w = np.clip((b - r - 6) / 10.0, 0, 1) * (b >= g)  # 0 = metal .. 1 = surely window, soft edge
    lum = a.mean(axis=2)
    t = np.clip((lum - 60) / 80.0, 0, 1)[..., None]
    glass = GLASS_DARK + (GLASS_LIGHT - GLASS_DARK) * t
    return a * (1 - w[..., None]) + glass * w[..., None], int((w > 0.5).sum())


def team_texture(png):
    a = np.asarray(Image.open(png).convert("RGB").resize((TEX, TEX), Image.LANCZOS)).astype(np.float64)
    a, glass = paint_glass(a)
    print("chinook.tga: %d window pixels repainted" % glass)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    red = (r > 120) & (g < 90) & (b < 90) & (r > 1.8 * g)
    out = np.zeros((TEX, TEX, 4))
    out[..., :3] = a
    out[..., 3] = 255
    grey = np.clip(r * 0.6, 0, 255)
    out[red, 0] = out[red, 1] = out[red, 2] = grey[red]
    out[red, 3] = TEAM_ALPHA
    return Image.fromarray(out.astype(np.uint8), "RGBA"), int(red.sum())


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    game, model, out = sys.argv[1:]
    art, data = os.path.join(game, "art"), os.path.join(game, "Data")
    os.makedirs(os.path.join(out, "art"), exist_ok=True)
    os.makedirs(os.path.join(out, "Data"), exist_ok=True)

    v, uv, f = load_obj(os.path.join(model, "heli.obj"))
    hubs = json.load(open(os.path.join(model, "rotors.json")))
    gv = to_game(v)
    # the hub caps do not survive the reduction: put each rotor on top of its reduced pylon
    tops = [[h[0], v[np.abs(v[:, 0] - h[0]) < 0.06, 1].max(), 0.0] for h in (hubs["front"], hubs["rear"])]
    front, rear = to_game(tops[0])[0], to_game(tops[1])[0]
    template = parse(open(os.path.join(art, "chopper.bh3"), "rb").read())
    root = {"type": 0, "kids": [mesh_chunk(gv, uv, f), skeleton(template, len(gv), front, rear)]}
    with open(os.path.join(out, "art", "chinook.bh3"), "wb") as fh:
        fh.write(write(root))
    print("chinook.bh3: %d vertices, %d triangles, rotors at %s and %s, size %s .. %s" % (
        len(gv), len(f), front.round(1), rear.round(1), gv.min(0).round(1), gv.max(0).round(1)))

    tex, team = team_texture(os.path.join(model, "heli.png"))
    tex.save(os.path.join(out, "art", "chinook.tga"))
    print("chinook.tga: %d player-colour pixels" % team)

    with open(os.path.join(art, "helicopter_propspin.bha"), "rb") as fh:
        bha = fh.read()
    with open(os.path.join(out, "art", "chinook_propspin.bha"), "wb") as fh:
        fh.write(propspin(bha))


if __name__ == "__main__":
    main()
