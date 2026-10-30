"""French, Enlightenment and Industrial ages, battle: Pas de charge.

The Revolutionary and Napoleonic armies attacked in column to the drums' "pas de charge",
the bands playing the Marseillaise (Rouget de Lisle, 1792, public domain), whose opening call
starts each round here; the rest is an original march. Snare drums, trumpets, horns and
trombones, G major.
"""
from kit import CRASH, KICK, SNARE, Piece, bass, pad, ROOT, FIFTH

TITLE = "PasDeCharge"
NATION, AGES, MOOD = "French", (4, 5), "BATTLE_VICTORY"

# "Allons enfants de la Patrie, le jour de gloire est arrive", then the march's own tune
A = ("r:3 D5:0.75 D5:0.25 | G5:1 G5:1 A5:1 A5:1 | D6:1.5 B5:0.5 G5:0.75 G5:0.25 B5:0.75 G5:0.25 | "
     "E5:1 C6:2 A5:0.75 F#5:0.25 | G5:2 r:2 | G5:0.75 A5:0.25 B5:1 B5:1 B5:0.75 C6:0.25 | "
     "B5:1 A5:1 A5:2 | A5:0.75 B5:0.25 C6:1 C6:1 C6:0.75 D6:0.25 | B5:3 r:1")
B = ("D6:1 D6:0.5 D6:0.5 D6:1 B5:1 | C6:1 C6:0.5 C6:0.5 C6:1 A5:1 | B5:1 G5:1 C6:1 A5:1 | D6:2 D5:2 | "
     "G5:1 B5:1 D6:1 G6:1 | F#6:1 E6:1 D6:2 | C6:1 B5:1 A5:1 F#5:1 | G5:4")


def build():
    p = Piece(120, 4)
    tpt = p.track("trumpets", 56, gain=0.9, pan=-0.15, legato=0.85)
    horn = p.track("horns", 60, gain=0.55, pan=0.2, rev=0.3)
    trb = p.track("trombones", 57, gain=0.5, pan=0.1, legato=0.8)
    fl = p.track("piccolo", 72, gain=0.5, pan=0.25, legato=0.85)
    snare = p.track("drums", 48, drums=True, gain=0.6, rev=0.2, swing=0.004)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.7, rev=0.25)
    cym = p.track("cymbals", 48, drums=True, gain=0.35, rev=0.3)
    p.strain("A", A, ["D", "G D", "G", "C D", "G", "G", "D", "D", "G"])
    p.strain("B", B, ["G", "D", "G C", "D", "G", "D", "D", "G"])
    p.add(pad(horn, low=55, vel=56), lo=2)
    p.add(bass(trb, [(0, ROOT), (1, FIFTH), (2, ROOT), (3, FIFTH)], low=36, vel=80, dur=0.5), lo=2)
    p.perc(bdrum, [(0, KICK, 104), (2, KICK, 96)])
    # the pas de charge: an accent every beat, the drummers' "ra-ta-plan" between
    p.perc(snare, [(0, SNARE, 112), (0.5, SNARE, 70), (0.75, SNARE, 72), (1, SNARE, 104), (2, SNARE, 110),
                   (2.5, SNARE, 70), (2.75, SNARE, 72), (3, SNARE, 104), (3.5, SNARE, 74)])
    p.perc(cym, [(0, CRASH, 76), (2, CRASH, 66)], lo=3)

    p.roll(snare, SNARE, 4, 40, 112, step=0.125)
    p.t += 4
    p.groove([None], 1)
    p.play("A", [(tpt, 90)], 1)
    p.play("B", [(tpt, 90), (fl, 70, 12)], 2)
    p.play("A", [(tpt, 94), (horn, 72, -12)], 2)
    p.play("B", [(tpt, 94), (fl, 76, 12)], 2)
    p.roll(snare, SNARE, 4, 50, 116, step=0.125)
    p.groove([None], 1)
    p.play("A", [(tpt, 100), (fl, 84, 12), (trb, 76, -12)], 3)
    p.play("B", [(tpt, 100), (fl, 86, 12)], 3)
    p.ritard(1, 96)
    p.final([(tpt, "D5+B5:1 G5+D6:3!", 104), (horn, "G3+B3+D4:4", 84), (trb, "G2:1 G2:3", 92)])
    bdrum.add(p.t + 1, 1, KICK, 120)
    cym.add(p.t + 1, 3, CRASH, 112)
    return p.s
