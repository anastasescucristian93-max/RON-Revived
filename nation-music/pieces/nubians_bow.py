"""Nubians, Ancient to Medieval ages, battle: Land of the Bow.

The Egyptians called Nubia Ta-Seti, "the land of the bow", for its famous archers. War drums
in a running 6/8, horns calling, a shrill flute and the shawm on the tune. A minor
pentatonic (A C D E G). Original.
"""
from kit import CONGA_HI, CONGA_LO, KICK, Piece, drone

TITLE = "LandOfTheBow"
NATION, AGES, MOOD = "Nubians", (0, 1, 2), "BATTLE_VICTORY"

A = ("A5:1/3 C6 A5 G5:2/3 E5:1/3 | A5:1/3 C6 D6 E6:1 | D6:1/3 C6 A5 G5:2/3 A5:1/3 | E5:2 | "
     "G5:1/3 A5 C6 D6:2/3 C6:1/3 | A5:1/3 G5 E5 D5:1 | E5:1/3 G5 A5 C6:2/3 A5:1/3 | A5:2")
B = ("E6:1 D6:2/3 C6:1/3 | D6:1 A5:1 | C6:2/3 A5:1/3 G5:2/3 E5:1/3 | G5:2 | "
     "A5:2/3 C6:1/3 D6:2/3 E6:1/3 | G6:1 E6:1 | D6:2/3 C6:1/3 A5:2/3 G5:1/3 | A5:2")


def build():
    p = Piece(132, 2)
    shawm = p.track("shawm", 111, gain=0.85, pan=-0.1, legato=0.9)
    flute = p.track("flute", 72, gain=0.55, pan=0.2, legato=0.85)
    horn = p.track("horns", 60, gain=0.65, rev=0.4, legato=1.0)
    low = p.track("drone", 70, gain=0.35, legato=1.0)
    drum = p.track("war drum", 116, gain=0.85, rev=0.25)
    hand = p.track("hand drums", 0, drums=True, gain=0.5, pan=0.2, rev=0.15)
    kick = p.track("big drum", 0, drums=True, gain=0.45, rev=0.2)
    T = 1 / 3
    p.strain("A", A, ["A5"] * 8)
    p.strain("B", B, ["A5"] * 8)
    p.add(drone(low, ["A2"], 62), lo=2)
    p.perc(drum, [(0, 45, 112), (1, 45, 100)], lo=1)
    p.perc(kick, [(2 * T, KICK, 74), (1 + 2 * T, KICK, 80)], lo=2)
    p.perc(hand, [(T, CONGA_HI, 66), (2 * T, CONGA_LO, 72), (1 + T, CONGA_HI, 70), (1 + 2 * T, CONGA_HI, 60)], lo=2)

    horn.play("A3:2 E3:2 | A3:4", 0, 92)
    p.groove([None] * 4, 1)
    for strain, leads, lev in (("A", [(shawm, 90)], 2), ("A", [(shawm, 92), (flute, 72, 12)], 2),
                               ("B", [(flute, 86)], 2), ("B", [(shawm, 94), (flute, 76)], 2)):
        p.play(strain, leads, lev)
    horn.play("E3:1 A3:1", p.t, 96)
    p.roll(drum, 45, 2, 60, 118, step=T)
    p.groove([None], 1)
    # the drums alone, answering each other, then the horns lead a round
    p.groove([None] * 4, 2)
    for strain, leads, lev in (("A", [(horn, 88, -12)], 2), ("B", [(horn, 90, -12), (flute, 70)], 2)):
        p.play(strain, leads, lev)
    horn.play("E3:1 A3:1", p.t, 96)
    p.roll(drum, 45, 2, 60, 118, step=T)
    p.groove([None], 1)
    p.play("A", [(shawm, 98), (flute, 84, 12)], 3)
    p.play("B", [(shawm, 98), (flute, 84)], 3)
    p.play("A", [(shawm, 100), (flute, 86, 12), (horn, 80, -12)], 3)
    p.final([(drum, "A2:2/3 A2:2/3 A2:2/3 A2:2", 124), (horn, "A3:4", 96), (shawm, "A5:2! r:2", 100)])
    return p.s
