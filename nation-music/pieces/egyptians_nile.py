"""Egyptians, Ancient to Medieval ages, economy: The Nile.

The instruments of the temple and tomb paintings: the arched harp, the long end-blown ney
flute, the sistrum rattle and the frame drum. E with a flat second and a raised third
(E F G# A B C D), the mode later called hijaz. Original.
"""
from kit import CONGA_LO, BONGO_HI, TAMB, Piece, arp, drone

TITLE = "Nile"
NATION, AGES, MOOD = "Egyptians", (0, 1, 2), "ECONOMIC"

A = ("B5:2 A5:0.5 G#5:0.5 A5:1 | F5:1.5 E5:0.5 F5:1 G#5:1 | A5:3 B5:0.5 A5:0.5 | G#5:2 F5:1 E5:1 | "
     "E5:1 F5:1 G#5:1 A5:1 | B5:1.5 C6:0.5 B5:1 A5:1 | G#5:1 F5:0.5 G#5:0.5 F5:1 E5:1 | E5:4")
B = ("E6:2 D6:1 C6:1 | B5:1 C6:1 B5:2 | A5:1 B5:0.5 A5:0.5 G#5:1 F5:1 | E5:4 | "
     "A5:1 G#5:1 A5:1 B5:1 | C6:2 B5:1 A5:1 | G#5:1.5 A5:0.5 G#5:1 F5:1 | E5:4")


def build():
    p = Piece(76, 4)
    harp = p.track("harp", 46, gain=0.6, pan=-0.2, ring=1.5, rev=0.4)
    ney = p.track("ney", 77, gain=0.95, pan=0.1, vib=(16, 4.5, 0.4), legato=0.97, rev=0.4)
    flute = p.track("flute", 73, gain=0.7, pan=0.2, vib=(14, 5, 0.3))
    pad = p.track("drone", 49, gain=0.2, rev=0.5, legato=1.0)
    sistrum = p.track("sistrum", 0, drums=True, gain=0.25, pan=0.3, rev=0.3)
    frame = p.track("frame drum", 0, drums=True, gain=0.45, pan=-0.1, rev=0.25)
    p.strain("A", A, ["E", "F", "Am", "E", "E", "Am", "E", "E"])
    p.strain("B", B, ["Am", "E", "Dm", "E", "Am", "Am", "E", "E"])
    p.add(arp(harp, 0.5, order=(0, 1, 2, 3, 2, 1, 0, 2), low=52, vel=54, accent=10, every=8), lo=1)
    p.add(drone(pad, ["E3", "B3"], 44), lo=2)
    p.perc(frame, [(0, CONGA_LO, 86), (1.5, BONGO_HI, 60), (2, CONGA_LO, 74), (3, BONGO_HI, 64), (3.5, BONGO_HI, 56)], lo=2)
    p.perc(sistrum, [(1, TAMB, 60), (3, TAMB, 66)], lo=2)

    harp.play("E4:0.25 F4 G#4 A4 B4 C5 D5 E5 F5:2 | E5:0.25 D5 C5 B4 A4 G#4 F4 E4:2.25", 0, 62)
    p.t += 8
    p.play("A", [(ney, 82)], 1)
    p.play("B", [(ney, 84)], 2)
    p.play("A", [(flute, 82), (ney, 64, -12)], 2)
    p.play("B", [(ney, 88), (flute, 66, 12)], 3)
    p.ritard(2, 58)
    p.final([(harp, "E3:0.25 B3 E4 G#4 B4 E5:2.5", 70), (ney, "E5:4", 72), (pad, "E3+B3:5", 44)])
    return p.s
