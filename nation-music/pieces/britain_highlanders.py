"""Britain, Industrial and Modern ages, battle: Highlanders.

The pipes and drums of the Highland regiments, which piped the British army into battle from
Waterloo to the Somme and El Alamein. Great Highland bagpipes over their drones, side drums,
tenor and bass drum. The pipe scale on A with the flat seventh (A B C# D E F# G), the
gracenotes of pipe music. Original tune.
"""
from kit import KICK, SNARE, TOM_LO, Piece, drone

TITLE = "Highlanders"
NATION, AGES, MOOD = "British", (5, 6), "BATTLE_VICTORY"

A = ("A4:0.75 B4:0.25 G5:g C#5:1 E5:1 C#5:1 | B4:0.75 A4:0.25 B4:1 G4:2 | A4:0.75 B4:0.25 G5:g C#5:1 E5:1 F#5:1 | "
     "E5:1 C#5:1 G5:g A4:2 | E5:0.75 F#5:0.25 A5:1 F#5:1 E5:1 | C#5:0.75 E5:0.25 B4:1 G4:2 | "
     "A4:0.75 B4:0.25 C#5:1 B4:0.75 G4:0.25 B4:1 | G5:g A4:4")
B = ("A5:1.5 G5:0.5 F#5:1 E5:1 | F#5:1 E5:0.5 C#5:0.5 G5:g B4:2 | E5:0.75 F#5:0.25 E5:1 C#5:1 A4:1 | "
     "B4:1.5 C#5:0.5 G5:g B4:2 | A5:1.5 G5:0.5 F#5:1 E5:1 | F#5:1 A5:1 E5:2 | "
     "C#5:0.75 E5:0.25 B4:1 G4:1 B4:1 | G5:g A4:4")


def build():
    p = Piece(92, 4)
    pipes = p.track("pipes", 109, gain=0.9, pan=-0.05, legato=0.98, rev=0.3)
    pipes2 = p.track("pipes 2", 109, gain=0.55, pan=0.2, legato=0.98, rev=0.3)
    drones = p.track("drones", 109, gain=0.3, legato=1.0, rev=0.3)
    snare = p.track("side drums", 0, drums=True, gain=0.55, rev=0.2, swing=0.004)
    tenor = p.track("tenor drum", 0, drums=True, gain=0.35, pan=0.2, rev=0.2)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.65, rev=0.25)
    p.strain("A", A, ["A5"] * 8)
    p.strain("B", B, ["A5"] * 8)
    # the drones: one bass and two tenors, A2 and A3
    p.add(drone(drones, ["A2", "A3"], 70), lo=0)
    p.perc(bdrum, [(0, KICK, 104), (2, KICK, 96)], lo=1)
    p.perc(snare, [(0, SNARE, 106), (0.5, SNARE, 68), (0.75, SNARE, 72), (1, SNARE, 92), (1.5, SNARE, 66),
                   (2, SNARE, 104), (2.25, SNARE, 64), (2.5, SNARE, 70), (3, SNARE, 94), (3.5, SNARE, 70),
                   (3.75, SNARE, 74)], lo=2)
    p.perc(tenor, [(0, TOM_LO, 80), (2, TOM_LO, 74)], lo=2)

    # the pipes strike in: the drones swell, two three-pace rolls
    p.groove(["A5"], 0)
    p.roll(snare, SNARE, 2, 50, 110, step=0.125, at=p.t - 2)
    p.groove(["A5"], 1)
    p.play("A", [(pipes, 92)], 1)
    p.play("A", [(pipes, 94), (pipes2, 76)], 2)
    p.play("B", [(pipes, 94)], 2)
    p.play("B", [(pipes, 96), (pipes2, 80)], 2)
    p.roll(snare, SNARE, 4, 50, 112, step=0.125)
    p.groove(["A5"], 1)
    p.play("A", [(pipes, 98), (pipes2, 84)], 3)
    p.play("B", [(pipes, 98), (pipes2, 86)], 3)
    p.final([(pipes, "A4:3 r:1", 96), (drones, "A2+A3:3", 70), (bdrum, "C2:1", 120)])
    return p.s
