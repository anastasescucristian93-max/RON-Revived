"""Inca, Ancient to Gunpowder ages, battle: Pututu.

The Inca armies were called with the pututu, the conch-shell trumpet, and marched to the
wankar drum; the chroniclers write of the din of pipes and drums before a battle. Conch calls,
big drums, shrill pan pipes. A minor pentatonic. Original.
"""
from kit import CABASA, KICK, Piece, drone

TITLE = "Pututu"
NATION, AGES, MOOD = "Inca", (0, 1, 2, 3), "BATTLE_VICTORY"

A = ("A5:1 A5:1 C6:1 A5:1 | G5:0.5 A5:0.5 G5:0.5 E5:0.5 D5:2 | E5:1 G5:1 A5:1 C6:1 | D6:2 C6:2 | "
     "A5:1 C6:1 D6:1 E6:1 | D6:0.5 C6:0.5 A5:1 G5:2 | E5:0.5 G5:0.5 A5:0.5 G5:0.5 E5:1 D5:1 | A5:4")
B = ("E6:1 E6:0.5 D6:0.5 C6:1 D6:1 | E6:2 A5:2 | G5:1 G5:0.5 A5:0.5 C6:1 A5:1 | G5:2 E5:2 | "
     "A5:0.5 C6:0.5 D6:0.5 E6:0.5 G6:1 E6:1 | D6:1 C6:1 A5:2 | G5:0.5 E5:0.5 D5:0.5 E5:0.5 G5:1 E5:1 | A5:4")


def build():
    p = Piece(120, 4)
    pipes = p.track("antara pipes", 75, gain=0.95, pan=-0.1, legato=0.85)
    whistle = p.track("whistles", 72, gain=0.6, pan=0.2, legato=0.85)
    conch = p.track("pututu", 60, gain=0.7, rev=0.45, legato=1.0)
    low = p.track("low", 70, gain=0.4, legato=1.0)
    wankar = p.track("wankar", 116, gain=0.9, rev=0.25)
    tinya = p.track("tinya", 116, gain=0.5, pan=0.2, rev=0.2)
    rattle = p.track("rattles", 0, drums=True, gain=0.3, pan=-0.25)
    p.strain("A", A, [None] * 8)
    p.strain("B", B, [None] * 8)
    p.add(drone(low, ["A2"], 60), lo=2)
    p.perc(wankar, [(0, 41, 112), (1, 41, 84), (1.5, 41, 78), (2, 41, 104), (3, 41, 86), (3.5, 41, 78)], lo=1)
    p.perc(tinya, [(0.5, 53, 70), (2.5, 53, 74), (3.75, 53, 66)], lo=2)
    p.perc(rattle, [(i * 0.25, CABASA, 60 if i % 2 == 0 else 40) for i in range(16)], lo=2)

    conch.play("A3:3 G3:1 A3:4", 0, 90)
    p.groove([None] * 2, 1)
    p.roll(wankar, 41, 4, 50, 112)
    p.groove([None], 1)
    for strain, leads, lev in (("A", [(pipes, 90)], 2), ("A", [(pipes, 92), (whistle, 72, 12)], 2),
                               ("B", [(pipes, 92)], 2), ("B", [(pipes, 94), (whistle, 76)], 2)):
        p.play(strain, leads, lev)
    conch.play("E3:2 A3:2", p.t, 94)
    p.roll(wankar, 41, 4, 60, 120)
    p.groove([None], 1)
    p.play("A", [(pipes, 98), (whistle, 84, 12)], 3)
    p.final([(wankar, "F2:1 F2:1 F2:2", 124), (conch, "A3:4", 96), (pipes, "A5:2! r:2", 100)])
    return p.s
