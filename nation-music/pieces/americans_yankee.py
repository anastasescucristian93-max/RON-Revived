"""Americans, Enlightenment and Industrial ages, battle: Yankee Doodle.

The tune of the Revolution (traditional, 18th century, public domain): the British sang it to
mock the colonials, the Continental Army took it over and marched to it. Fifes and side drums
first, as at Lexington; then the bugles and the brass of a later army take it up. G major, 2/4.
"""
from kit import KICK, SNARE, CRASH, ROOT, FIFTH, Piece, bass, pad, third

TITLE = "YankeeDoodle"
NATION, AGES, MOOD = "Americans", (4, 5), "BATTLE_VICTORY"
G_MAJOR = "G A B C D E F#"

VERSE = ("G5:0.5 G5:0.5 A5:0.5 B5:0.5 | G5:0.5 B5:0.5 A5:0.5 D5:0.5 | G5:0.5 G5:0.5 A5:0.5 B5:0.5 | "
         "G5:1 F#5:1 | G5:0.5 G5:0.5 A5:0.5 B5:0.5 | C6:0.5 B5:0.5 A5:0.5 G5:0.5 | "
         "F#5:0.5 D5:0.5 E5:0.5 F#5:0.5 | G5:1 G5:1")
CHORUS = ("E5:0.75 F#5:0.25 E5:0.5 D5:0.5 | E5:0.5 F#5:0.5 G5:1 | D5:0.75 E5:0.25 D5:0.5 C5:0.5 | "
          "B4:1 D5:1 | E5:0.75 F#5:0.25 E5:0.5 D5:0.5 | E5:0.5 F#5:0.5 G5:0.5 E5:0.5 | "
          "D5:0.5 G5:0.5 F#5:0.5 A5:0.5 | G5:1 G5:1")


def build():
    p = Piece(120, 2)
    fife = p.track("fife", 72, gain=0.9, pan=-0.1, legato=0.8, rev=0.2)
    fife2 = p.track("fife 2", 72, gain=0.55, pan=0.15, legato=0.8, rev=0.2)
    tpt = p.track("bugles", 56, gain=0.75, pan=0.2, legato=0.85)
    horn = p.track("horns", 60, gain=0.45, pan=-0.25, rev=0.3)
    tuba = p.track("tuba", 58, gain=0.55, legato=0.6)
    snare = p.track("side drum", 48, drums=True, gain=0.6, rev=0.2, swing=0.004)
    bdrum = p.track("bass drum", 48, drums=True, gain=0.7, rev=0.25)
    cym = p.track("cymbal", 48, drums=True, gain=0.3, rev=0.3)
    p.strain("V", VERSE, ["G", "G D", "G", "G D", "G", "C G", "D", "G"])
    p.strain("C", CHORUS, ["C", "C G", "G", "G", "C", "C", "D", "G"])
    p.add(bass(tuba, [(0, ROOT), (1, FIFTH)], low=31), lo=2)
    p.add(pad(horn, low=55, vel=46), lo=2)
    p.perc(bdrum, [(0, KICK, 100), (1, KICK, 86)])
    p.perc(snare, [(0, SNARE, 108), (0.5, SNARE, 66), (0.75, SNARE, 70), (1, SNARE, 100),
                   (1.25, SNARE, 64), (1.5, SNARE, 72), (1.75, SNARE, 66)])

    # the drummers beat the long roll, then the fifes come in
    p.roll(snare, SNARE, 4, 40, 100, step=0.125)
    p.t += 4
    p.groove([None] * 2, 1)
    for strain in ("V", "C", "V", "C"):
        p.play(strain, [(fife, 88), (fife2, 70, third(G_MAJOR))], 1)
    p.roll(snare, SNARE, 2, 50, 110, step=0.125)
    p.groove([None] * 2, 1)
    cym.add(p.t, 1, CRASH, 90)
    for strain in ("V", "C", "V", "C"):
        p.play(strain, [(tpt, 90, -12), (fife, 84)], 2)
    p.ritard(1, 100)
    p.final([(fife, "D5:0.5 G5:0.5 B5:1!", 96), (tpt, "G4+B4:0.5 D5:0.5 G5:1!", 96), (tuba, "G1:0.5 r:0.5 G1:1", 96),
             (horn, "r:1 G3+D4+G4:1", 80)])
    p.roll(snare, SNARE, 1, 60, 110, step=0.125)
    bdrum.add(p.t + 1, 1, KICK, 120)
    cym.add(p.t + 1, 2, CRASH, 110)
    return p.s
