"""Maya, Ancient to Gunpowder ages, battle: Holcan.

The holcan were the Maya warriors; the murals of Bonampak show the battle and the long wooden
trumpets and drums that went with it. Syncopated drums on tunkul and turtle shells, the long
trumpets on single notes, the flutes on top. G minor pentatonic. Original.
"""
from kit import CLAVES, MARACAS, Piece, drone

TITLE = "Holcan"
NATION, AGES, MOOD = "Maya", (0, 1, 2, 3), "BATTLE_VICTORY"

A = ("G5:1.5 Bb5:0.5 G5:1 F5:1 | D5:1.5 F5:0.5 G5:2 | Bb5:1.5 C6:0.5 D6:1 C6:1 | Bb5:1.5 G5:0.5 F5:2 | "
     "G5:0.5 Bb5:1 C6:0.5 D6:1 F6:1 | D6:1.5 C6:0.5 Bb5:2 | C6:0.5 Bb5:1 G5:0.5 F5:1 D5:1 | G5:4")
B = ("D6:1 D6:0.5 C6:0.5 Bb5:1 C6:1 | D6:2 G5:2 | F5:1 F5:0.5 G5:0.5 Bb5:1 G5:1 | F5:2 D5:2 | "
     "G5:0.5 Bb5:0.5 C6:0.5 D6:0.5 F6:1 D6:1 | C6:1 Bb5:1 G5:2 | F5:0.5 D5:0.5 C5:0.5 D5:0.5 F5:1 D5:1 | G5:4")


def build():
    p = Piece(126, 4)
    pipes = p.track("pan pipes", 75, gain=0.9, pan=-0.1, legato=0.85)
    picc = p.track("whistles", 72, gain=0.5, pan=0.2, legato=0.85)
    tpt = p.track("long trumpets", 58, gain=0.6, rev=0.45, legato=1.0)
    tunkul = p.track("tunkul", 117, gain=0.6, rev=0.2)
    drum = p.track("drum", 116, gain=0.8, rev=0.25)
    shell = p.track("turtle shells", 0, drums=True, gain=0.4, pan=0.25)
    rattle = p.track("rattles", 0, drums=True, gain=0.28, pan=-0.3)
    p.strain("A", A, ["Gm"] * 8)
    p.strain("B", B, ["Gm"] * 8)
    p.add(drone(tpt, ["G2"], 70), lo=3)
    p.perc(drum, [(0, 43, 112), (1.5, 43, 90), (2.5, 43, 100), (3, 43, 86)], lo=1)
    p.perc(tunkul, [(0.5, 55, 74), (1, 50, 80), (2, 55, 70), (3.5, 50, 84)], lo=2)
    p.perc(shell, [(0.25, CLAVES, 60), (0.75, CLAVES, 64), (1.75, CLAVES, 58), (2.75, CLAVES, 66), (3.25, CLAVES, 58)], lo=2)
    p.perc(rattle, [(i * 0.25, MARACAS, 58 if i % 2 == 0 else 38) for i in range(16)], lo=2)

    tpt.play("G2:4 D2:4", 0, 92)
    p.groove([None] * 2, 1)
    p.roll(tunkul, 50, 4, 50, 110)
    p.groove([None], 1)
    for strain, leads, lev in (("A", [(pipes, 90)], 2), ("A", [(pipes, 92), (picc, 72, 12)], 2),
                               ("B", [(pipes, 92)], 2), ("B", [(pipes, 94), (picc, 76, 12)], 2)):
        p.play(strain, leads, lev)
    tpt.play("G2:2 D2:2", p.t, 96)
    p.roll(drum, 43, 4, 60, 118)
    p.groove([None], 1)
    p.play("A", [(pipes, 98), (picc, 84, 12)], 3)
    p.final([(drum, "G2:1 G2:1 G2:2", 124), (tpt, "G2:4", 96), (pipes, "G5:2! r:2", 100)])
    return p.s
