"""Germans, Enlightenment and Industrial ages, economy: Ode to Joy.

The theme of the last movement of Beethoven's Ninth Symphony (1824, public domain), set
on Schiller's "An die Freude". It enters as in the symphony: low strings alone, then the
violins, then horns and woodwinds, then the whole orchestra with the timpani. D major.
"""
from kit import Piece, bass, pad, ROOT, FIFTH

TITLE = "OdeToJoy"
NATION, AGES, MOOD = "Germans", (4, 5), "ECONOMIC"

L1 = "F#5:1 F#5:1 G5:1 A5:1 | A5:1 G5:1 F#5:1 E5:1 | D5:1 D5:1 E5:1 F#5:1 | F#5:1.5 E5:0.5 E5:2"
L2 = "F#5:1 F#5:1 G5:1 A5:1 | A5:1 G5:1 F#5:1 E5:1 | D5:1 D5:1 E5:1 F#5:1 | E5:1.5 D5:0.5 D5:2"
L3 = "E5:1 E5:1 F#5:1 D5:1 | E5:1 F#5:0.5 G5:0.5 F#5:1 D5:1 | E5:1 F#5:0.5 G5:0.5 F#5:1 E5:1 | D5:1 E5:1 A4:2"
THEME = " | ".join([L1, L2, L3, L2])
HARM = (["D", "A", "D", "A"] + ["D", "A", "D", "A D"] + ["A", "D", "A", "D A"] + ["D", "A", "D", "A D"])


def build():
    p = Piece(100, 4)
    cellos = p.track("cellos", 42, gain=0.9, pan=0.2, vib=(10, 5, 0.3), legato=0.95)
    vlns = p.track("violins", 48, gain=0.9, pan=-0.2, legato=0.95)
    horns = p.track("horns", 60, gain=0.75, pan=0.25, rev=0.35, legato=0.95)
    winds = p.track("woodwinds", 73, gain=0.6, pan=-0.1, legato=0.9)
    oboe = p.track("oboe", 68, gain=0.5, pan=0.1, legato=0.9)
    strpad = p.track("strings", 49, gain=0.35, rev=0.4, legato=1.0)
    bassi = p.track("basses", 43, gain=0.55, legato=0.9)
    timp = p.track("timpani", 47, gain=0.5, rev=0.3)
    tpt = p.track("trumpets", 56, gain=0.55, pan=0.1, legato=0.9)
    p.strain("T", THEME, HARM)
    p.add(bass(bassi, [(0, ROOT), (2, FIFTH)], low=31, vel=66, dur=1.8), lo=2)
    p.add(pad(strpad, low=55, vel=48), lo=2)
    p.add(bass(timp, [(0, ROOT), (2, FIFTH)], low=38, vel=80, dur=0.5), lo=3)

    p.play("T", [(cellos, 76, -12)], 1)
    p.play("T", [(vlns, 80), (cellos, 60, -12)], 2)
    p.play("T", [(horns, 84, -12), (winds, 80), (oboe, 70)], 2)
    p.play("T", [(vlns, 92), (winds, 86, 12), (tpt, 80), (horns, 82, -12)], 3)
    p.ritard(2, 76)
    p.final([(strpad, "D3+A3+D4+F#4+A4:6", 70), (tpt, "D5+A5:4", 84), (horns, "D4+F#4:5", 80),
             (timp, "D2:0.25 D2 D2 D2 D2 D2 D2 D2 D2:2", 96), (bassi, "D2:5", 80)])
    return p.s
