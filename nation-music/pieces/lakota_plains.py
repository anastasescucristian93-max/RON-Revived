"""Lakota, Ancient to Industrial ages, economy: The Plains.

The courting flute of the Plains peoples, its breathy low voice playing long phrases that fall
step by step, over a slow frame drum and the wind of the open grassland. E minor pentatonic
(E G A B D). Original, instrumental.
"""
from kit import Piece, drone

TITLE = "Plains"
NATION, AGES, MOOD = "Lakota", (0, 1, 2, 3, 4, 5), "ECONOMIC"

A = ("B4:2 D5:1 B4:1 | A4:1 G4:1 E4:2 | G4:1 A4:1 B4:1 D5:1 | E5:3 r:1 | "
     "D5:1 B4:1 A4:1 B4:1 | D5:2 B4:2 | A4:1 G4:0.5 A4:0.5 G4:1 E4:1 | E4:3 r:1")
B = ("E5:2 G5:1 E5:1 | D5:1 B4:1 D5:2 | E5:1 D5:1 B4:1 A4:1 | B4:3 r:1 | "
     "G4:1 A4:1 B4:2 | D5:1 E5:1 B4:2 | A4:1 G4:0.5 A4:0.5 G4:1 E4:1 | E4:3 r:1")


def build():
    p = Piece(68, 4)
    flute = p.track("cedar flute", 77, gain=0.95, pan=-0.1, vib=(16, 4.2, 0.45), legato=0.97, rev=0.55)
    flute2 = p.track("second flute", 73, gain=0.55, pan=0.25, vib=(12, 4.5, 0.4), rev=0.55)
    drum = p.track("frame drum", 116, gain=0.45, rev=0.35)
    wind = p.track("wind", 122, bank=3, gain=0.15, rev=0.3, legato=1.0)
    pad = p.track("drone", 49, gain=0.16, rev=0.55, legato=1.0)
    p.strain("A", A, ["Em"] * 8)
    p.strain("B", B, ["Em"] * 8)
    p.add(drone(pad, ["E3", "B3"], 40), lo=2)
    p.perc(drum, [(0, 45, 80), (1, 45, 58), (2, 45, 70), (3, 45, 58)], lo=1)

    wind.add(0, 12, 60, 70)
    p.t += 4
    p.play("A", [(flute, 82)], 0)
    p.play("B", [(flute, 84)], 1)
    p.play("A", [(flute, 84), (flute2, 60, 12)], 2)
    p.play("B", [(flute2, 80, 12), (flute, 66)], 2)
    p.play("A", [(flute, 82)], 1)
    wind.add(p.t - 4, 12, 60, 64)
    p.ritard(2, 52)
    p.final([(flute, "E4:5", 72), (pad, "E3+B3:5", 40), (drum, "A2:1", 80)])
    return p.s
