"""Romans, Ancient to Medieval ages, economy: Hydraulis.

Music of a Roman villa: the cithara (the big concert lyre), the tibia (double pipe), the pan
pipes, and the hydraulis, the water organ invented in Alexandria that played in Roman
arenas and houses (one was found at Aquincum, Budapest). D Dorian. Original.
"""
from kit import Piece, arp, pad

TITLE = "Hydraulis"
NATION, AGES, MOOD = "Romans", (0, 1, 2), "ECONOMIC"

A = ("D5:1 F5:1 A5:2 | G5:1 F5:0.5 E5:0.5 F5:2 | E5:1 G5:1 C6:1.5 B5:0.5 | A5:4 | "
     "B5:1 A5:1 G5:1 F5:1 | E5:1.5 F5:0.5 G5:2 | F5:1 E5:0.5 D5:0.5 C5:1 E5:1 | D5:4")
B = ("A5:1 B5:1 C6:2 | D6:1 C6:0.5 B5:0.5 A5:2 | G5:1 A5:1 B5:1 G5:1 | A5:4 | "
     "F5:1 G5:1 A5:1 F5:1 | G5:1.5 A5:0.5 E5:2 | F5:1 E5:1 D5:1 C#5:1 | D5:4")


def build():
    p = Piece(84, 4)
    cithara = p.track("cithara", 46, gain=0.65, pan=-0.2, ring=1.4, rev=0.35)
    tibia = p.track("tibia", 68, gain=0.85, pan=0.15, vib=(8, 5, 0.3), legato=0.92)
    syrinx = p.track("pan pipes", 75, gain=0.75, pan=0.25, vib=(10, 5, 0.3))
    organ = p.track("hydraulis", 19, gain=0.35, rev=0.5, legato=1.0)
    p.strain("A", A, ["Dm", "Dm", "C", "Am", "G", "C", "Dm", "Dm"])
    p.strain("B", B, ["Am", "G", "G", "Am", "F", "C", "A", "Dm"])
    p.add(arp(cithara, 0.5, order=(0, 1, 2, 3, 2, 1, 2, 1), low=50, vel=54, accent=12, every=8))
    p.add(pad(organ, low=50, vel=50), lo=2)

    cithara.play("D4:0.5 A4 D5 F5 A5:2 | G5:0.5 F5 E5 C5 D5:2", 0, 66)
    p.t += 8
    p.play("A", [(tibia, 82)], 1)
    p.play("B", [(tibia, 84)], 1)
    p.play("A", [(organ, 70, 12), (syrinx, 80)], 2)
    p.play("B", [(syrinx, 84), (tibia, 66, -12)], 2)
    p.play("A", [(tibia, 86), (syrinx, 70, 12)], 2)
    p.ritard(2, 62)
    p.final([(cithara, "D3:0.25 A3 D4 F4 A4 D5:2.75", 70), (organ, "D3+A3+D4+F4:4", 54), (tibia, "D5:4", 70)])
    return p.s
