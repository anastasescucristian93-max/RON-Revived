r"""Turn a generated siren into the game's nuke alarm.

    python make_alarm.py [work\siren_3a.wav] [--fade 0.6]

The game plays sounds\nukelaunch.wav (soundfiles.xml tag `nukedetected`, the global
NUKE_DETECTED) once for every launch it detects; the original is 3.2 s. The siren gets a short
fade-in, a fade-out over its last --fade seconds (ElevenLabs cut it off at full volume) and the
original's peak level, then goes to nuke-fx\mod_override\sounds\nukelaunch.wav.
"""
import os
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
GAME = os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations")
OUT = os.path.join(MOD, "mod_override", "sounds", "nukelaunch.wav")


def read(path):
    with wave.open(path) as w:
        a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
        return a.reshape(-1, w.getnchannels()), w.getframerate()


def main():
    args = sys.argv[1:]
    fade = 0.6
    if "--fade" in args:
        i = args.index("--fade")
        fade = float(args[i + 1])
        del args[i:i + 2]
    src = args[0] if args else os.path.join(MOD, "work", "siren_3a.wav")
    a, rate = read(src)
    orig, _ = read(os.path.join(GAME, "sounds", "nukelaunch.wav"))

    n = len(a)
    env = np.ones(n)
    fin = int(rate * 0.02)
    env[:fin] = np.linspace(0, 1, fin)
    fout = min(n, int(rate * fade))
    env[n - fout:] *= np.cos(np.linspace(0, np.pi / 2, fout)) ** 2
    a = a * env[:, None]
    a *= np.abs(orig).max() / max(np.abs(a).max(), 1e-9)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with wave.open(OUT, "wb") as w:
        w.setnchannels(a.shape[1])
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((np.clip(a, -1, 1) * 32767).astype(np.int16).tobytes())
    print(f"{OUT}: {n / rate:.1f} s from {os.path.basename(src)}, fade-out {fade} s")


if __name__ == "__main__":
    main()
