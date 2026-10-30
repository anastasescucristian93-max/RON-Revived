"""Germans, Enlightenment to Modern ages, battle: Prussian March.

The Prussian army of Frederick the Great made the military march a German art; the regimental
bands of the 19th century added brass and the glockenspiel. Snare and bass drum, trumpets,
trombones, tuba and a glockenspiel on the tune. F major. Original.
"""
from kit import CRASH, KICK, SNARE, Piece, bass, pad, pluck, ROOT, FIFTH

TITLE = "PrussianMarch"
NATION, AGES, MOOD = "Germans", (4, 5, 6), "BATTLE_VICTORY"

A = ("F5:1 F5:0.5 F5:0.5 A5:1 C6:1 | Bb5:1 G5:1 C6:2 | A5:1 A5:0.5 Bb5:0.5 C6:1 A5:1 | G5:2 C5:2 | "
     "F5:1 F5:0.5 F5:0.5 A5:1 C6:1 | D6:1 Bb5:1 F6:2 | E6:1 C6:1 D6:0.5 C6:0.5 Bb5:0.5 G5:0.5 | F5:3 r:1")
B = ("C6:1 C6:1 D6:0.5 C6:0.5 Bb5:0.5 A5:0.5 | Bb5:1 Bb5:1 C6:0.5 Bb5:0.5 A5:0.5 G5:0.5 | "
     "A5:1 F5:1 G5:1 A5:1 | Bb5:2 G5:2 | C6:1 C6:1 D6:0.5 C6:0.5 Bb5:0.5 A5:0.5 | "
     "D6:1 Bb5:1 G5:1 E5:1 | F5:1 A5:1 G5:1 E5:1 | F5:4")


def build():
    p = Piece(112, 4)
    tpt = p.track("trumpets", 56, gain=0.9, pan=-0.15, legato=0.85)
    glock = p.track("glockenspiel", 9, gain=0.45, pan=0.25, rev=0.3)
    clar = p.track("clarinets", 71, gain=0.6, pan=0.2, legato=0.85)
    horn = p.track("horns", 60, gain=0.5, pan=-0.25, rev=0.3)
    trb = p.track("trombones", 57, gain=0.55, pan=0.1)
    tuba = p.track("tuba", 58, gain=0.55, legato=0.6)
    snare = p.track("snare", 0, drums=True, gain=0.55, rev=0.2, swing=0.004)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.7, rev=0.25)
    cym = p.track("cymbals", 48, drums=True, gain=0.35, rev=0.3)
    p.strain("A", A, ["F", "C", "F", "C", "F", "Bb", "C", "F"])
    p.strain("B", B, ["F", "C", "F", "Bb", "F", "Bb C", "C", "F"])
    p.add(bass(tuba, [(0, ROOT), (2, FIFTH)], low=29, vel=82), lo=1)
    # the band's after-beats
    p.add(pluck(horn, [(1, "c"), (3, "c")], low=53, vel=56, dur=0.4), lo=1)
    p.add(pad(trb, low=48, vel=54), lo=3)
    p.perc(bdrum, [(0, KICK, 104), (2, KICK, 96)])
    p.perc(cym, [(0, CRASH, 64), (2, CRASH, 56)], lo=2)
    p.perc(snare, [(0, SNARE, 104), (1, SNARE, 70), (1.5, SNARE, 64), (1.75, SNARE, 66), (2, SNARE, 100),
                   (3, SNARE, 72), (3.5, SNARE, 66), (3.75, SNARE, 70)])

    p.roll(snare, SNARE, 2, 40, 100, step=0.125)
    p.final([(snare, "D2:1! D2:1!", 110)], at=2)  # D2 = key 38, two stick strokes
    p.t += 4
    p.groove(["F", "C"], 1)
    p.play("A", [(tpt, 90), (glock, 70, 12)], 1)
    p.play("B", [(clar, 86), (glock, 70, 12)], 2)
    p.play("A", [(tpt, 94), (clar, 76)], 2)
    p.play("B", [(tpt, 94), (glock, 74, 12)], 2)
    p.roll(snare, SNARE, 4, 50, 112, step=0.125)
    p.groove([None], 1)
    p.play("A", [(tpt, 100), (glock, 80, 12), (trb, 76, -12)], 3)
    p.play("B", [(tpt, 100), (clar, 84), (glock, 80, 12)], 3)
    p.ritard(1, 96)
    p.final([(tpt, "C5+A5:1 F5+C6:3!", 104), (trb, "C3+E3:1 F3+A3:3", 88), (tuba, "C2:1 F1:3", 96)])
    cym.add(p.t + 1, 3, CRASH, 112)
    bdrum.add(p.t + 1, 1, KICK, 120)
    return p.s
