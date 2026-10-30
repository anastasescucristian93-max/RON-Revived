"""Greeks, Ancient to Medieval ages, economy: Lyre and Aulos.

The two instruments of ancient Greece: the plucked lyre of Apollo and the double-reed aulos
of Dionysus, whose second pipe held a drone. Here a lyre, an aulos over its drone, and a
syrinx (pan pipes) answering. The Greek Dorian mode (E F G A B C D, the modern Phrygian).
Original.
"""
from kit import Piece, arp, drone

TITLE = "Lyre"
NATION, AGES, MOOD = "Greeks", (0, 1, 2), "ECONOMIC"

A = ("E5:2 F5:1 G5:1 | A5:2 G5:1 F5:1 | E5:1 F5:1 G5:1 A5:1 | B5:3 A5:1 | "
     "C6:1 B5:1 A5:1 G5:1 | F5:1.5 G5:0.5 A5:2 | G5:1 F5:1 E5:1 D5:1 | E5:4")
B = ("A5:1 B5:1 C6:2 | D6:1 C6:1 B5:2 | A5:1 G5:1 A5:1 B5:1 | C6:2 A5:2 | "
     "B5:1 A5:1 G5:1 F5:1 | G5:2 E5:2 | F5:1 E5:1 D5:1 F5:1 | E5:4")


def build():
    p = Piece(80, 4)
    lyre = p.track("lyre", 46, gain=0.65, pan=-0.2, ring=1.4, rev=0.35)
    aulos = p.track("aulos", 68, gain=0.9, pan=0.1, vib=(10, 5, 0.3), legato=0.95)
    adrone = p.track("aulos drone", 69, gain=0.3, pan=0.1, legato=1.0)
    syrinx = p.track("syrinx", 75, gain=0.75, pan=0.25, vib=(12, 5, 0.3))
    frame = p.track("tympanon", 0, drums=True, gain=0.35, rev=0.3)
    p.strain("A", A, ["E5", "A5", "E5", "E5", "A5", "D5", "D5", "E5"])
    p.strain("B", B, ["A5", "D5", "A5", "A5", "E5", "E5", "D5", "E5"])
    p.add(arp(lyre, 0.5, order=(0, 1, 2, 1, 3, 1, 2, 1), low=52, vel=54, accent=12, every=8))
    p.add(drone(adrone, ["E4"], 48), lo=2)
    p.perc(frame, [(0, 64, 76), (2, 64, 64), (3, 63, 54)], lo=2)

    lyre.play("E4:0.5 B4 E5 F5 G5:2 | A5:0.5 G5 F5 E5 B4:2", 0, 66)
    p.t += 8
    p.play("A", [(aulos, 82)], 1)
    p.play("B", [(aulos, 84)], 2)
    p.play("A", [(syrinx, 82), (aulos, 64, -12)], 2)
    p.play("B", [(aulos, 86), (syrinx, 70, 12)], 2)
    p.play("A", [(lyre, 92, 12), (syrinx, 62)], 2)
    p.ritard(2, 62)
    p.final([(lyre, "E3:0.25 B3 E4 B4 E5:3", 70), (adrone, "E4:4", 48), (aulos, "E5:4", 70)])
    return p.s
