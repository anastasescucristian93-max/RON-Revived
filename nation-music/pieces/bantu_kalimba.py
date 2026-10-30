"""Bantu, Ancient to Enlightenment ages, economy: Kalimba.

The mbira / kalimba (thumb piano) of the Bantu peoples of southern and central Africa,
interlocking with a marimba over the 12/8 bell pattern; voices answer the lead in
call-and-response. F major, a four-chord cycle. Original.
"""
from kit import AGOGO_HI, CABASA, CONGA_HI, CONGA_LO, CONGA_MUTE, Piece, arp, bass, ROOT

TITLE = "Kalimba"
NATION, AGES, MOOD = "Bantu", (0, 1, 2, 3, 4), "ECONOMIC"

T = 1 / 3
A = ("C6:2/3 A5:1/3 C6:2/3 D6:1/3 C6:1 A5:1 | D6:2/3 C6:1/3 Bb5:2/3 A5:1/3 G5:2 | "
     "A5:2/3 C6:1/3 F6:2/3 E6:1/3 D6:1 C6:1 | Bb5:2/3 A5:1/3 G5:2/3 A5:1/3 C6:2 | "
     "C6:2/3 A5:1/3 C6:2/3 D6:1/3 F6:1 D6:1 | D6:2/3 F6:1/3 D6:2/3 C6:1/3 Bb5:2 | "
     "A5:2/3 G5:1/3 F5:2/3 G5:1/3 A5:1 C6:1 | G5:2/3 A5:1/3 G5:2/3 E5:1/3 F5:2")
CALL = ("F6:1 D6:1 C6:2 | r:4 | D6:1 C6:1 A5:2 | r:4 | "
        "C6:2/3 D6:1/3 F6:2/3 D6:1/3 C6:2 | r:4 | A5:2/3 C6:1/3 A5:2/3 G5:1/3 F5:2 | r:4")
ANSWER = ("r:4 | Bb4+D5+F5:2 A4+C5+F5:2 | r:4 | G4+C5+E5:2 A4+C5+F5:2 | "
          "r:4 | Bb4+D5+F5:4 | r:4 | G4+C5+E5:2 A4+C5+F5:2")
CYCLE = ["F", "Bb", "F", "C", "F", "Bb", "F", "C"]


def build():
    p = Piece(84, 4)
    kal = p.track("kalimba", 108, gain=1.0, pan=-0.15, ring=1.5)
    mar = p.track("marimba", 12, gain=0.55, pan=0.25, ring=1.2)
    voices = p.track("voices", 53, gain=0.7, rev=0.4, legato=0.95)
    flute = p.track("flute", 73, gain=0.7, pan=0.1, vib=(14, 5, 0.3))
    b = p.track("bass", 32, gain=0.55, legato=0.7)
    bell = p.track("bell", 0, drums=True, gain=0.3, pan=0.3)
    shaker = p.track("shaker", 0, drums=True, gain=0.25, pan=-0.3)
    drum = p.track("drum", 0, drums=True, gain=0.45, rev=0.2)
    p.strain("A", A, CYCLE)
    p.strain("CALL", CALL, CYCLE)
    # the marimba's interlocking part: off the beat of the kalimba
    p.add(arp(mar, T, order=(0, 2, 1, 3, 1, 2), low=53, vel=52, accent=10, every=3), lo=1)
    p.add(bass(b, [(0, ROOT), (2 + 2 * T, ROOT)], low=36, vel=78), lo=2)
    # the standard 12/8 bell pattern
    p.perc(bell, [(i * T, AGOGO_HI, 80 if i == 0 else 62) for i in (0, 2, 4, 5, 7, 9, 11)], lo=1)
    p.perc(shaker, [(i * T, CABASA, 54 if i % 3 == 0 else 38) for i in range(12)], lo=2)
    p.perc(drum, [(0, CONGA_LO, 92), (1 + T, CONGA_MUTE, 70), (2, CONGA_HI, 80),
                  (2 + 2 * T, CONGA_LO, 84), (3 + T, CONGA_MUTE, 66)], lo=2)

    p.groove(CYCLE[:4], 1)
    p.play("A", [(kal, 84)], 1)
    p.play("A", [(kal, 86)], 2)
    voices.play(ANSWER, p.t, 76)
    p.play("CALL", [(kal, 90)], 2)
    p.play("A", [(flute, 84), (kal, 66)], 2)
    voices.play(ANSWER, p.t, 80)
    p.play("CALL", [(flute, 88), (kal, 70)], 2)
    p.play("A", [(kal, 90), (flute, 72, 12)], 3)
    p.ritard(2, 70)
    p.final([(kal, "F5:1/3 A5 C6 F6:3", 86), (voices, "A4+C5+F5:4", 70), (mar, "F3+C4+F4:3", 70), (b, "F2:2", 80)])
    return p.s
