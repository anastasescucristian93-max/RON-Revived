"""Lakota, Ancient to Industrial ages, battle: Horsemen.

The Lakota were the great horsemen of the Plains. The big drum of the warrior societies in a
driving, even beat with its "honor beats", the eagle-bone whistle, and the flute carrying the
tune over a galloping rhythm. E minor pentatonic. Original, instrumental.
"""
from kit import Piece, drone

TITLE = "Horsemen"
NATION, AGES, MOOD = "Lakota", (0, 1, 2, 3, 4, 5), "BATTLE_VICTORY"

A = ("E6:1 E6:0.5 D6:0.5 B5:1 A5:1 | B5:0.5 A5:0.5 G5:1 E5:2 | G5:1 A5:1 B5:1 D6:1 | E6:2 D6:2 | "
     "B5:1 D6:0.5 B5:0.5 A5:1 G5:1 | A5:0.5 G5:0.5 E5:1 D5:2 | E5:1 G5:1 A5:1 B5:1 | E5:4")
B = ("B5:1 B5:1 D6:1 B5:1 | A5:1 G5:1 E5:2 | D5:1 E5:1 G5:1 A5:1 | B5:2 A5:2 | "
     "E6:1 D6:1 B5:1 A5:1 | G5:1 A5:1 B5:2 | A5:0.5 G5:0.5 E5:1 D5:1 E5:1 | E5:4")


def build():
    p = Piece(144, 4)
    flute = p.track("flute", 73, gain=0.9, pan=-0.1, legato=0.85)
    flute2 = p.track("flute 2", 77, gain=0.6, pan=0.2, legato=0.9)
    whistle = p.track("eagle-bone whistle", 72, gain=0.35, pan=0.3)
    low = p.track("drone", 70, gain=0.35, legato=1.0)
    drum = p.track("big drum", 116, gain=0.9, rev=0.25)
    hoof = p.track("hooves", 117, gain=0.4, pan=-0.25, rev=0.15)
    p.strain("A", A, ["Em"] * 8)
    p.strain("B", B, ["Em"] * 8)
    p.add(drone(low, ["E2", "B2"], 62), lo=2)
    # the even beat of the big drum, an honor beat every fourth bar
    p.perc(drum, [(i, 40, 104 if i == 0 else 90) for i in range(4)], lo=1)
    p.perc(drum, [(3.5, 40, 112)], lo=2, every=4)
    # the gallop: da-da-DUM
    p.perc(hoof, [(b + o, 55, v) for b in range(4) for o, v in ((0, 70), (1 / 3, 56), (2 / 3, 80))], lo=2)

    p.groove([None] * 2, 1)
    whistle.play("E7:0.25 r:0.75 E7:0.25 r:0.75 E7:0.25 r:1.75", 4, 70)
    p.groove([None] * 2, 2)
    for strain, leads, lev in (("A", [(flute, 90)], 2), ("A", [(flute, 92), (flute2, 74, -12)], 2),
                               ("B", [(flute2, 88)], 2), ("B", [(flute, 92), (flute2, 78, -12)], 2)):
        p.play(strain, leads, lev)
    whistle.play("E7:0.25 r:0.25 E7:0.25 r:0.25 E7:0.25 r:2.75", p.t, 76)
    p.roll(drum, 40, 4, 60, 118)
    p.groove([None], 1)
    p.play("A", [(flute, 98), (flute2, 84, -12)], 3)
    p.play("B", [(flute, 98), (flute2, 86, -12)], 3)
    p.final([(drum, "E2:1 E2:1 E2:2", 124), (flute, "E6:2! r:2", 100), (low, "E2+B2:4", 80)])
    return p.s
