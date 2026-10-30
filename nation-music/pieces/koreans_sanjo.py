"""Koreans, Classical to Enlightenment ages, economy: Gayageum Song.

The gayageum, the 12-string zither of the Gaya kingdom, with the daegeum (the big bamboo
flute with its buzzing membrane) and the haegeum fiddle, over the janggu hourglass drum in
semachi, the lilting 9/8 rhythm of Korean folk song. Gyemyeonjo mode on A (A C D E G),
with the wide vibrato on the held notes. Original.
"""
from kit import BONGO_HI, CONGA_LO, CONGA_MUTE, Piece, arp

TITLE = "Gayageum"
NATION, AGES, MOOD = "Koreans", (1, 2, 3, 4), "ECONOMIC"

T = 1 / 3
A = ("A5:1 C6:2/3 A5:1/3 G5:1 | E5:2 D5:1 | E5:2/3 G5:1/3 A5:1 C6:1 | A5:3 | "
     "C6:1 D6:2/3 C6:1/3 A5:1 | G5:2/3 A5:1/3 G5:2/3 E5:1/3 D5:1 | E5:1 G5:2/3 E5:1/3 D5:1 | A4:3")
B = ("E6:2 D6:1 | C6:1 D6:2/3 C6:1/3 A5:1 | G5:2 A5:1 | E5:3 | "
     "G5:1 A5:1 C6:1 | D6:2 C6:1 | A5:2/3 C6:1/3 A5:2/3 G5:1/3 E5:1 | A5:3")


def build():
    p = Piece(66, 3)
    gaya = p.track("gayageum", 107, bank=8, gain=0.85, pan=-0.2, ring=1.6, rev=0.35)
    daegeum = p.track("daegeum", 77, gain=0.95, pan=0.1, vib=(35, 4.5, 0.35), legato=0.97, rev=0.4)
    haegeum = p.track("haegeum", 110, gain=0.75, pan=0.25, vib=(30, 5, 0.25), legato=0.97)
    janggu = p.track("janggu", 0, drums=True, gain=0.45, rev=0.2)
    pad = p.track("drone", 49, gain=0.16, rev=0.5, legato=1.0)
    p.strain("A", A, ["Am"] * 8)
    p.strain("B", B, ["Am"] * 8)
    p.add(arp(gaya, T, order=(0, 2, 3, 1, 3, 4, 3, 2, 1), low=45, vel=48, accent=12, every=9), lo=1)
    p.add(lambda ch, a, n, lev: [pad.add(a, n, k, 38) for k in (45, 52)], lo=2)
    # semachi: deong (both heads) - kung (low) - deok (high)
    p.perc(janggu, [(0, CONGA_LO, 84), (0, BONGO_HI, 70), (1, CONGA_LO, 66), (1 + 2 * T, BONGO_HI, 60),
                    (2, CONGA_LO, 70), (2 + T, CONGA_MUTE, 54), (2 + 2 * T, BONGO_HI, 58)], lo=2)

    gaya.play("A4:1/3 C5 D5 E5 G5 A5 C6:1 | A5:1/3 G5 E5 D5 C5 G4 A4:1", 0, 64)
    p.t += 6
    p.play("A", [(gaya, 82, -12)], 1)
    p.play("A", [(daegeum, 82)], 2)
    p.play("B", [(daegeum, 84), (gaya, 64, -12)], 2)
    p.play("B", [(haegeum, 84), (daegeum, 66)], 2)
    p.play("A", [(daegeum, 86), (haegeum, 70)], 3)
    p.ritard(2, 50)
    p.final([(gaya, "A3:1/3 E4 A4 C5 E5:2", 70), (daegeum, "A5:3", 72), (pad, "A2+E3:3", 38)])
    return p.s
