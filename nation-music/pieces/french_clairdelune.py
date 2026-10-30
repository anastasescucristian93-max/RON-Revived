"""French, Enlightenment age, economy: Au clair de la lune.

The 18th-century French song (traditional, public domain) dressed as a gavotte for the court
of Versailles: harpsichord continuo, strings, a transverse flute and an oboe, and on the
repeat the "double", the tune broken into running eighths as the clavecinistes did. G major.
"""
from kit import Piece, bass, pad, pluck, ROOT

TITLE = "ClairDeLune"
NATION, AGES, MOOD = "French", (4,), "ECONOMIC"

L1 = "G5:1 G5:1 G5:1 A5:1 | B5:2 A5:2 | G5:1 B5:1 A5:1 A5:1 | G5:4"
L3 = "A5:1 A5:1 A5:1 A5:1 | E5:2 E5:2 | A5:1 G5:1 F#5:1 E5:1 | D5:4"
SONG = " | ".join([L1, L1, L3, L1])
DOUBLE = " | ".join([
    "G5:0.5 D5 G5 B5 G5 D5 A5 F#5 | B5:0.5 D6 B5 G5 A5:1 D5:1 | G5:0.5 A5 B5 G5 A5 F#5 D5 F#5 | G5:2 G4:2",
    "G5:0.5 B5 D6 B5 G5 B5 A5 C6 | B5:0.5 G5 D5 G5 A5 F#5 D5 F#5 | G5:0.5 D5 B5 G5 A5 D5 F#5 A5 | G5:2 D5:2",
    "A5:0.5 F#5 D5 F#5 A5 C6 A5 F#5 | E5:0.5 F#5 G#5 E5 A5:2 | A5:0.5 G5 F#5 E5 F#5 G5 E5 C#5 | D5:2 D4:2",
    "G5:0.5 D5 G5 B5 G5 D5 A5 F#5 | B5:0.5 D6 B5 G5 A5:1 D5:1 | G5:0.5 A5 B5 G5 A5 F#5 D5 F#5 | G5:4"])
HARM = ["G", "G D", "G D", "G"] * 2 + ["D", "A", "D", "D"] + ["G", "G D", "G D", "G"]


def build():
    p = Piece(92, 4)
    flute = p.track("flute", 73, gain=0.9, pan=-0.1, vib=(10, 5, 0.3), legato=0.9)
    oboe = p.track("oboe", 68, gain=0.8, pan=0.15, vib=(8, 5, 0.3), legato=0.9)
    hpsd = p.track("harpsichord", 6, gain=0.55, pan=-0.3)
    vlns = p.track("strings", 48, gain=0.4, pan=0.2, rev=0.3, legato=0.95)
    cello = p.track("cello", 42, gain=0.5, legato=0.8)
    p.strain("S", SONG, HARM)
    p.strain("D", DOUBLE, HARM)
    p.add(pluck(hpsd, [(0, "c"), (1, "c"), (2, "c"), (3, "c")], low=55, vel=52, dur=0.5))
    p.add(bass(cello, [(0, ROOT), (1, ROOT), (2, ROOT), (3, ROOT)], low=36, vel=64, dur=0.9))
    p.add(pad(vlns, low=60, vel=44), lo=2)

    p.play("S", [(hpsd, 72)], 1)
    p.play("S", [(flute, 82)], 2)
    p.play("D", [(oboe, 80)], 2)
    p.play("S", [(flute, 84), (oboe, 70, -12)], 3)
    p.play("D", [(flute, 82), (hpsd, 60)], 3)
    p.ritard(2, 70)
    p.final([(hpsd, "G3+B3+D4+G4:4", 70), (flute, "G5:4", 76), (cello, "G2:4", 70), (vlns, "G4+B4+D5:4", 50)])
    return p.s
