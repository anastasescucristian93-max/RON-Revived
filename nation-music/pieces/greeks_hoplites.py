"""Greeks, Ancient to Medieval ages, battle: Hoplites.

Thucydides tells that the Spartans advanced "slowly and to the music of many aulos players",
to keep the phalanx in step. Two auloi over the tympanon and the beat of the march, the
salpinx (war trumpet) calling each round. Greek Dorian on E. Original.
"""
from kit import CONGA_LO, CONGA_HI, KICK, Piece, drone

TITLE = "Hoplites"
NATION, AGES, MOOD = "Greeks", (0, 1, 2), "BATTLE_VICTORY"

A = ("E5:1 E5:0.5 F5:0.5 G5:1 E5:1 | A5:1 G5:0.5 F5:0.5 E5:2 | E5:1 G5:1 A5:1 B5:1 | C6:1 B5:0.5 A5:0.5 B5:2 | "
     "B5:1 A5:0.5 G5:0.5 A5:1 F5:1 | G5:1 F5:0.5 E5:0.5 D5:2 | E5:1 F5:1 G5:0.5 F5:0.5 E5:0.5 D5:0.5 | E5:4")
B = ("B5:1 B5:1 C6:1 B5:1 | A5:2 G5:2 | A5:1 A5:1 B5:1 A5:1 | G5:2 F5:2 | "
     "E5:1 G5:1 B5:1 D6:1 | C6:1 B5:1 A5:2 | G5:1 F5:1 E5:1 D5:1 | E5:4")


def build():
    p = Piece(104, 4)
    aulos = p.track("aulos", 68, gain=0.9, pan=-0.1, legato=0.9)
    aulos2 = p.track("aulos 2", 69, gain=0.6, pan=0.15, legato=0.9)
    adrone = p.track("drone", 69, gain=0.3, legato=1.0)
    salpinx = p.track("salpinx", 56, gain=0.7, rev=0.4, legato=0.9)
    low = p.track("low strings", 48, gain=0.35, rev=0.3, legato=1.0)
    drum = p.track("war drum", 116, gain=0.8, rev=0.25)
    frame = p.track("tympana", 0, drums=True, gain=0.45, pan=0.2, rev=0.2)
    kick = p.track("march", 0, drums=True, gain=0.5, rev=0.2)
    p.strain("A", A, ["E5"] * 8)
    p.strain("B", B, ["E5"] * 8)
    p.add(drone(adrone, ["E4"], 56), lo=1)
    p.add(drone(low, ["E2", "B2"], 54), lo=3)
    p.perc(drum, [(0, 43, 108), (2, 43, 98)], lo=1)
    p.perc(kick, [(0, KICK, 90), (1, KICK, 70), (2, KICK, 86), (3, KICK, 70)], lo=2)
    p.perc(frame, [(0.5, CONGA_HI, 60), (1.5, CONGA_LO, 72), (2.5, CONGA_HI, 60), (3.5, CONGA_LO, 76)], lo=2)

    salpinx.play("E4:1 B4:1 E5:2 | B4:1 E5:3", 0, 92)
    p.groove([None] * 2, 1)
    p.play("A", [(aulos, 88)], 1)
    p.play("A", [(aulos, 90), (aulos2, 72, -12)], 2)
    p.play("B", [(aulos, 90)], 2)
    p.play("B", [(aulos, 92), (aulos2, 76, -12)], 2)
    salpinx.play("E4:1 B4:1 E5:2", p.t, 96)
    p.roll(drum, 43, 4, 60, 116)
    p.groove([None], 1)
    p.play("A", [(aulos, 96), (aulos2, 82, -12), (salpinx, 70, -12)], 3)
    p.play("B", [(aulos, 96), (aulos2, 84, -12)], 3)
    p.final([(drum, "G2:1 G2:1 G2:2", 120), (salpinx, "E4+B4:4!", 96), (aulos, "E5:4", 90)])
    return p.s
