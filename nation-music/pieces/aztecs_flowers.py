"""Aztecs, Ancient to Medieval ages, economy: Song of Flowers.

"In xochitl in cuicatl", flower and song, was the Nahua phrase for poetry. Clay flutes and
ocarinas over the two-tongued teponaztli log drum, the huehuetl's slow beat and seed rattles;
a conch at the start of each round. A minor pentatonic (A C D E G). Original.
"""
from kit import MARACAS, WOOD_HI, WOOD_LO, Piece, drone

TITLE = "Flowers"
NATION, AGES, MOOD = "Aztecs", (0, 1, 2, 3), "ECONOMIC"

A = ("A5:1.5 C6:0.5 D6:1 C6:1 | A5:2 G5:1 E5:1 | G5:1 A5:1 C6:1.5 D6:0.5 | E6:4 | "
     "D6:1 C6:1 D6:1 E6:1 | G6:1.5 E6:0.5 D6:2 | C6:1 A5:1 G5:1 E5:1 | A5:4")
B = ("E6:1 G6:1 E6:1 D6:1 | C6:2 A5:2 | D6:1 E6:1 D6:1 C6:1 | A5:2 G5:2 | "
     "A5:1 C6:1 D6:1 E6:1 | D6:1.5 C6:0.5 A5:2 | G5:1 E5:1 G5:1 A5:1 | A5:4")


def build():
    p = Piece(84, 4)
    oca = p.track("ocarina", 79, gain=1.0, pan=-0.1, vib=(10, 5, 0.3), legato=0.95)
    flute = p.track("clay flute", 75, gain=0.75, pan=0.2, vib=(12, 5, 0.3), legato=0.95)
    log = p.track("teponaztli", 0, drums=True, gain=0.4, pan=0.25, rev=0.25)
    rattle = p.track("rattles", 0, drums=True, gain=0.22, pan=-0.3, rev=0.2)
    huehuetl = p.track("huehuetl", 116, gain=0.55, rev=0.3)
    conch = p.track("conch", 60, gain=0.5, rev=0.5, legato=1.0)
    hum = p.track("drone", 89, gain=0.12, rev=0.5, legato=1.0)  # a warm pad under it, as the game's tracks have
    for name, mel in (("A", A), ("B", B)):
        p.strain(name, mel, ["A5"] * 8)
    p.add(drone(hum, ["A2", "E3"], 40), lo=1)
    p.perc(log, [(0, WOOD_LO, 80), (1, WOOD_HI, 60), (1.5, WOOD_HI, 56), (2, WOOD_LO, 74),
                 (3, WOOD_HI, 60), (3.5, WOOD_LO, 64)], lo=1)
    p.perc(rattle, [(i * 0.5, MARACAS, 60 if i % 2 == 0 else 42) for i in range(8)], lo=2)
    p.perc(huehuetl, [(0, 41, 96), (2, 41, 80)], lo=0)

    conch.add(0, 6, 45, 70)
    p.groove([None] * 2, 0)
    p.groove(["A5"] * 2, 1)
    p.play("A", [(oca, 82)], 1)
    p.play("B", [(oca, 82)], 1)
    conch.add(p.t, 6, 45, 66)
    p.play("A", [(flute, 80), (oca, 64, -12)], 2)
    p.play("B", [(flute, 84), (oca, 70)], 2)
    p.play("A", [(oca, 86), (flute, 70, 12)], 2)
    p.ritard(2, 64)
    p.final([(oca, "A5:6", 70), (hum, "A2+E3:6", 40), (huehuetl, "A1:4", 96)])
    conch.add(p.t, 6, 45, 60)
    return p.s
