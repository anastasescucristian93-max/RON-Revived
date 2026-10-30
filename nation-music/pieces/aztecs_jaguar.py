"""Aztecs, Ancient to Gunpowder ages, battle: Jaguar Warriors.

The eagle and jaguar knights went to the "flower wars" behind drums, conch trumpets and shrill
clay whistles. The great huehuetl and the teponaztli drive it, rattles over them, flutes on top.
A minor pentatonic. Original.
"""
from kit import MARACAS, WOOD_HI, WOOD_LO, Piece

TITLE = "Jaguar"
NATION, AGES, MOOD = "Aztecs", (0, 1, 2, 3), "BATTLE_VICTORY"

A = ("A5:0.5 A5 C6 A5 D6:1 C6:1 | A5:0.5 G5 E5 G5 A5:2 | C6:0.5 C6 D6 C6 E6:1 D6:1 | C6:0.5 A5 G5 E5 A5:2 | "
     "E6:1 D6:0.5 C6 D6:1 C6:1 | A5:0.5 C6 D6 E6 G6:2 | E6:0.5 D6 C6 A5 G5:1 E5:1 | A5:2 r:2")
B = ("E6:2 D6:2 | C6:1 A5:1 G5:2 | A5:1 C6:1 D6:1 E6:1 | D6:2 C6:2 | "
     "E6:2 G6:2 | E6:1 D6:1 C6:2 | A5:1 G5:1 E5:1 G5:1 | A5:4")


def build():
    p = Piece(132, 4)
    whistle = p.track("clay whistles", 72, gain=0.8, pan=-0.15, legato=0.85)
    flute = p.track("flutes", 75, gain=0.8, pan=0.15, legato=0.9)
    hue = p.track("huehuetl", 116, gain=0.9, rev=0.25)
    hue2 = p.track("drums", 116, gain=0.55, rev=0.2, pan=-0.2)
    log = p.track("teponaztli", 0, drums=True, gain=0.45, pan=0.25, rev=0.2)
    rattle = p.track("rattles", 0, drums=True, gain=0.28, pan=-0.3, rev=0.15)
    conch = p.track("conch", 60, gain=0.65, rev=0.4, legato=1.0)
    p.strain("A", A, [None] * 8)
    p.strain("B", B, [None] * 8)
    p.perc(hue, [(0, 40, 112), (1.5, 40, 90), (2, 40, 104), (3, 40, 96)], lo=1)
    p.perc(hue2, [(0.5, 50, 70), (1, 50, 84), (2.5, 50, 70), (3.5, 50, 88)], lo=2)
    p.perc(log, [(0, WOOD_LO, 86), (0.5, WOOD_HI, 64), (1, WOOD_HI, 70), (1.5, WOOD_LO, 76),
                 (2, WOOD_LO, 86), (2.75, WOOD_HI, 64), (3, WOOD_HI, 70), (3.5, WOOD_LO, 76)], lo=2)
    p.perc(rattle, [(i * 0.25, MARACAS, 64 if i % 2 == 0 else 44) for i in range(16)], lo=2)

    # the conch calls, the drums answer
    conch.play("A3:3 E3:1 A3:4", 0, 84)
    p.groove([None] * 2, 1)
    p.roll(hue2, 50, 4, 50, 110)
    p.groove([None] * 2, 2)
    for strain, leads in (("A", [(whistle, 92)]), ("A", [(whistle, 92), (flute, 74, -12)]),
                          ("B", [(flute, 90), (whistle, 70)]), ("B", [(flute, 92), (whistle, 78)])):
        p.play(strain, leads, 2)
    conch.play("E3:2 A3:2", p.t, 90)
    p.roll(hue2, 50, 8, 60, 118, step=0.25)
    p.groove([None] * 2, 1)
    p.play("A", [(whistle, 98), (flute, 86)], 3)
    p.play("B", [(whistle, 98), (flute, 90)], 3)
    p.final([(hue, "A1:1 A1:1 A1:2", 124), (conch, "A3:4", 92), (whistle, "A6:2! r:2", 100)])
    return p.s
