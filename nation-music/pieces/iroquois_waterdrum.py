"""Iroquois, Ancient to Enlightenment ages, economy: Water Drum.

The Haudenosaunee water drum (a small drum partly filled with water, its tone ringing) and
the horn rattle keep a steady pulse for the social dances; a wooden flute plays over them.
D minor pentatonic (D F G A C). Original, instrumental.
"""
from kit import MARACAS, Piece, drone

TITLE = "WaterDrum"
NATION, AGES, MOOD = "Iroquois", (0, 1, 2, 3, 4), "ECONOMIC"

A = ("A4:2 C5:1 A4:1 | G4:1 F4:1 D4:2 | F4:1 G4:1 A4:1 C5:1 | D5:3 r:1 | "
     "C5:1 A4:1 G4:1 A4:1 | C5:2 A4:2 | G4:1 F4:1 G4:1 A4:1 | D4:3 r:1")
B = ("D5:2 F5:1 D5:1 | C5:1 A4:1 C5:2 | D5:1 C5:1 A4:1 G4:1 | A4:3 r:1 | "
     "F4:1 G4:1 A4:2 | C5:1 D5:1 A4:2 | G4:1 F4:0.5 G4:0.5 F4:1 D4:1 | D4:3 r:1")


def build():
    p = Piece(92, 4)
    flute = p.track("cedar flute", 73, gain=0.95, pan=-0.1, vib=(14, 4.5, 0.4), legato=0.95, rev=0.45)
    flute2 = p.track("flute 2", 75, gain=0.6, pan=0.2, vib=(10, 4.5, 0.4), rev=0.45)
    water = p.track("water drum", 117, gain=0.6, rev=0.3)
    rattle = p.track("horn rattle", 0, drums=True, gain=0.3, pan=0.25)
    pad = p.track("drone", 49, gain=0.18, rev=0.5, legato=1.0)
    p.strain("A", A, ["D5"] * 8)
    p.strain("B", B, ["D5"] * 8)
    p.add(drone(pad, ["D3", "A3"], 40), lo=2)
    p.perc(water, [(i, 50, 84 if i == 0 else 72) for i in range(4)], lo=1)
    p.perc(rattle, [(i * 0.5, MARACAS, 58 if i % 2 == 0 else 40) for i in range(8)], lo=2)

    p.groove([None] * 2, 1)
    p.play("A", [(flute, 84)], 1)
    p.play("B", [(flute, 84)], 2)
    p.play("A", [(flute2, 80, 12), (flute, 66)], 2)
    p.play("B", [(flute, 86), (flute2, 70, 12)], 2)
    p.play("A", [(flute, 84)], 1)
    p.ritard(2, 70)
    p.final([(flute, "D4:4", 74), (water, "D3:1", 84), (pad, "D3+A3:4", 40)])
    return p.s
