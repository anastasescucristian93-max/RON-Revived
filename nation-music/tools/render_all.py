"""Render every piece in ../pieces: the WAV (game format) to ../mod_override/sounds/tracks/,
the MIDI to ../out/midi/. Several pieces render at once, one per CPU core.

python render_all.py [word ...]   only the pieces whose module name contains a given word
"""
import importlib
import os
import sys
from concurrent.futures import ProcessPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "pieces"))
TRACKS = os.path.join(ROOT, "mod_override", "sounds", "tracks")
MIDI = os.path.join(ROOT, "out", "midi")


def pieces(words=()):
    """(module name, module) of every piece, or of those matching a word"""
    out = []
    for f in sorted(os.listdir(os.path.join(ROOT, "pieces"))):
        mod = f[:-3]
        if f.endswith(".py") and (not words or any(w in mod for w in words)):
            out.append((mod, importlib.import_module(mod)))
    return out


def track_name(piece):
    return f"{piece.NATION}_{piece.TITLE.replace(' ', '')}"


def render(mod):
    sys.path[:0] = [HERE, os.path.join(ROOT, "pieces")]
    piece = importlib.import_module(mod)
    name = track_name(piece)
    path, secs = piece.build().save(name, TRACKS, MIDI)
    return f"{name}: {secs // 60:.0f}:{secs % 60:04.1f}  ages {piece.AGES}  {piece.MOOD}"


def main(words):
    mods = [m for m, _ in pieces(words)]
    with ProcessPoolExecutor(max_workers=max(1, (os.cpu_count() or 2) - 1)) as ex:
        for line in ex.map(render, mods):
            print(line, flush=True)


if __name__ == "__main__":
    main(sys.argv[1:])
