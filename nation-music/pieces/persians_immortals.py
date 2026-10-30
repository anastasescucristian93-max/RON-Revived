"""Persians, Ancient to Enlightenment ages, battle: The Immortals.

From the Achaemenid Immortals to the Safavid army, Persian hosts marched to the karnay
(long trumpets), the sorna shawm and the naqqara kettledrums of the naqqareh-khaneh, the
drum house over the city gate. Dastgah Mahur, the bright Persian major (D E F# G A B C).
Original.
"""
from kit import CRASH2, KICK, Piece, bass, drone, ROOT, FIFTH

TITLE = "Immortals"
NATION, AGES, MOOD = "Persians", (0, 1, 2, 3, 4), "BATTLE_VICTORY"

A = ("D5:1 F#5:1 A5:1 F#5:1 | G5:1 F#5:0.5 E5:0.5 D5:2 | A5:1 B5:1 C6:1 B5:0.5 A5:0.5 | G5:2 A5:2 | "
     "D6:1 C6:0.5 B5:0.5 A5:1 G5:1 | F#5:1 G5:0.5 F#5:0.5 E5:2 | F#5:1 E5:0.5 D5:0.5 E5:1 C5:1 | D5:4")
B = ("A5:1 A5:0.5 B5:0.5 C6:1 A5:1 | D6:2 A5:2 | G5:1 G5:0.5 A5:0.5 B5:1 G5:1 | C6:2 G5:2 | "
     "F#5:1 A5:1 D6:1 F#6:1 | E6:1 D6:0.5 C6:0.5 B5:2 | A5:1 G5:0.5 F#5:0.5 E5:1 C5:1 | D5:4")


def build():
    p = Piece(112, 4)
    sorna = p.track("sorna", 111, gain=0.9, pan=-0.1, legato=0.9, vib=(10, 6, 0.2))
    karnay = p.track("karnay", 58, gain=0.6, rev=0.45, legato=1.0)
    tpt = p.track("trumpets", 56, gain=0.6, pan=0.2, legato=0.9)
    low = p.track("low strings", 48, gain=0.4, legato=0.8)
    naqqara = p.track("naqqara", 47, gain=0.6, rev=0.3)
    drum = p.track("war drum", 116, gain=0.8, rev=0.25)
    kick = p.track("big drum", 0, drums=True, gain=0.5, rev=0.2)
    cym = p.track("sanj", 0, drums=True, gain=0.35, rev=0.3)
    p.strain("A", A, ["D", "G D", "C", "G", "G", "D", "C", "D"])
    p.strain("B", B, ["C", "D", "G", "C", "D", "C G", "C", "D"])
    p.add(drone(karnay, ["D2"], 66), lo=3)
    p.add(bass(low, [(0, ROOT), (1, ROOT), (2, FIFTH), (3, ROOT)], low=36, vel=78, dur=0.5), lo=2)
    p.add(bass(naqqara, [(0, ROOT), (1.5, FIFTH), (2, ROOT)], low=38, vel=88, dur=0.4), lo=1)
    p.perc(drum, [(0, 40, 110), (2, 40, 104), (3.5, 40, 90)], lo=1)
    p.perc(kick, [(1, KICK, 80), (3, KICK, 84)], lo=2)
    p.perc(cym, [(0, CRASH2, 74)], lo=2, every=2)

    karnay.play("D2:4 A2:2 D3:2", 0, 96)
    p.groove([None] * 2, 1)
    for strain, leads, lev in (("A", [(sorna, 90)], 2), ("A", [(sorna, 92), (tpt, 74, -12)], 2),
                               ("B", [(tpt, 88)], 2), ("B", [(sorna, 94), (tpt, 80, -12)], 2)):
        p.play(strain, leads, lev)
    karnay.play("D2:2 A2:2", p.t, 98)
    p.roll(drum, 40, 4, 60, 118)
    p.groove([None], 1)
    p.play("A", [(sorna, 98), (tpt, 86, -12)], 3)
    p.final([(drum, "E2:1 E2:1 E2:2", 124), (karnay, "D2:4", 100), (tpt, "D4+A4+D5:4!", 96), (naqqara, "D2:0.25 D2 D2 D2 D2:3", 100)])
    cym.add(p.t + 2, 3, CRASH2, 110)
    return p.s
