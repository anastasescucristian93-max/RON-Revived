"""Iroquois, Ancient to Enlightenment ages, battle: The Longhouse Rises.

For the Five Nations, war was raised in the longhouse with drums and rattles. A fast pulse
on the big drum and the water drum, rattles over it, flutes driving the tune, a low drone
under. D minor pentatonic. Original, instrumental.
"""
from kit import KICK, MARACAS, Piece, drone

TITLE = "Longhouse"
NATION, AGES, MOOD = "Iroquois", (0, 1, 2, 3, 4), "BATTLE_VICTORY"

A = ("D5:0.5 D5 F5 D5 G5:1 F5:1 | D5:0.5 C5 A4 C5 D5:2 | F5:0.5 F5 G5 F5 A5:1 G5:1 | F5:0.5 D5 C5 A4 D5:2 | "
     "A5:1 G5:0.5 F5:0.5 G5:1 F5:1 | D5:0.5 F5 G5 A5 C6:2 | A5:0.5 G5 F5 D5 C5:1 A4:1 | D5:2 r:2")
B = ("C6:1 A5:1 G5:1 A5:1 | C6:1 D6:1 A5:2 | G5:1 F5:1 G5:1 A5:1 | F5:2 D5:2 | "
     "F5:0.5 G5 A5 C6 D6:1 C6:1 | A5:1 G5:1 F5:2 | G5:0.5 F5 D5 C5 D5:1 F5:1 | D5:4")


def build():
    p = Piece(138, 4)
    flute = p.track("flute", 73, gain=0.9, pan=-0.1, legato=0.85)
    pipes = p.track("pipes", 75, gain=0.65, pan=0.2, legato=0.85)
    low = p.track("drone", 70, gain=0.35, legato=1.0)
    big = p.track("big drum", 0, drums=True, gain=0.65, rev=0.2)
    water = p.track("water drums", 117, gain=0.55, pan=-0.2, rev=0.2)
    rattle = p.track("rattles", 0, drums=True, gain=0.3, pan=0.25)
    p.strain("A", A, ["D5"] * 8)
    p.strain("B", B, ["D5"] * 8)
    p.add(drone(low, ["D2", "A2"], 64), lo=2)
    p.perc(big, [(i, KICK, 108 if i in (0, 2) else 92) for i in range(4)], lo=1)
    p.perc(water, [(i * 0.5 + 0.5, 50, 74) for i in range(0, 8, 2)] + [(1.75, 52, 70), (3.75, 52, 76)], lo=2)
    p.perc(rattle, [(i * 0.25, MARACAS, 60 if i % 2 == 0 else 40) for i in range(16)], lo=2)

    p.groove([None] * 2, 1)
    p.roll(water, 52, 4, 50, 110)
    p.groove([None], 1)
    for strain, leads, lev in (("A", [(flute, 90)], 2), ("A", [(flute, 92), (pipes, 74, -12)], 2),
                               ("B", [(pipes, 90)], 2), ("B", [(pipes, 92), (flute, 78, 12)], 2)):
        p.play(strain, leads, lev)
    p.roll(water, 52, 8, 60, 118)
    p.groove([None] * 2, 1)
    p.play("A", [(flute, 98), (pipes, 84)], 3)
    p.final([(big, "C2:1 C2:1 C2:2", 124), (flute, "D6:2! r:2", 100), (low, "D2+A2:4", 80)])
    return p.s
