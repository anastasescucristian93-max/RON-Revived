"""Mongols, Classical to Gunpowder ages, battle: The Horde.

The tumens rode to the great naccara drums on horseback, the drum-signal starting the charge.
A gallop in triplets on the drums, the morin khuur racing, horns calling, the low throat drone
under it. D minor pentatonic (D F G A C). Original.
"""
from kit import CONGA_LO, CONGA_MUTE, KICK, Piece, drone

TITLE = "Horde"
NATION, AGES, MOOD = "Mongols", (1, 2, 3, 4), "BATTLE_VICTORY"

A = ("D5:1/3 F5 G5 A5:1 G5:1/3 F5 D5 F5:1 | G5:1/3 A5 C6 D6:1 C6:1/3 A5 G5 A5:1 | "
     "D6:1/3 C6 A5 G5:1 A5:1/3 G5 F5 D5:1 | F5:1/3 G5 F5 D5:3 | "
     "A5:1/3 C6 D6 F6:1 D6:1/3 C6 A5 C6:1 | D6:1/3 C6 A5 G5:1 F5:1/3 G5 A5 G5:1 | "
     "F5:1/3 D5 C5 D5:1 F5:1/3 G5 A5 D6:1 | D6:4")
B = ("A5:2 G5:1 F5:1 | G5:2 D5:2 | F5:1 G5:1 A5:1 C6:1 | D6:4 | "
     "C6:1 A5:1 G5:1 F5:1 | G5:1 A5:1 D5:2 | F5:1 G5:1/3 F5 D5 C5:1 D5:1 | D5:4")


def build():
    p = Piece(132, 4)
    khuur = p.track("morin khuur", 40, gain=0.9, pan=-0.1, vib=(14, 6, 0.2), legato=0.9)
    limbe = p.track("limbe", 73, gain=0.6, pan=0.2, legato=0.9)
    horns = p.track("horns", 60, gain=0.75, rev=0.4, legato=0.95)
    throat = p.track("throat drone", 70, gain=0.4, legato=1.0)
    naccara = p.track("naccara", 116, gain=0.9, rev=0.25)
    gallop = p.track("gallop drums", 0, drums=True, gain=0.5, pan=-0.2, rev=0.15)
    kick = p.track("big drum", 0, drums=True, gain=0.5, rev=0.2)
    p.strain("A", A, ["D5"] * 8)
    p.strain("B", B, ["D5"] * 8)
    p.add(drone(throat, ["D2"], 66), lo=1)
    p.perc(naccara, [(0, 38, 112), (2, 38, 104)], lo=1)
    p.perc(kick, [(1, KICK, 86), (3, KICK, 90)], lo=2)
    p.perc(gallop, [(b + o, k, v) for b in range(4) for o, k, v in ((0, CONGA_LO, 80), (1 / 3, CONGA_MUTE, 60),
                                                                   (2 / 3, CONGA_LO, 92))], lo=1)

    horns.play("D3:2 A3:2 | D4:4", 0, 92)
    p.groove([None] * 2, 1)
    p.play("A", [(khuur, 90)], 2)
    p.play("B", [(horns, 88, -12), (khuur, 72)], 2)
    p.play("A", [(khuur, 94), (limbe, 76)], 2)
    p.play("B", [(horns, 92, -12), (limbe, 80, 12)], 2)
    p.roll(naccara, 38, 4, 60, 118)
    p.groove([None], 1)
    p.play("A", [(khuur, 98), (limbe, 84), (horns, 76, -12)], 3)
    p.final([(naccara, "D2:1 D2:1 D2:2", 124), (horns, "D3+A3+D4:4", 98), (khuur, "D5:2! r:2", 100)])
    return p.s
