"""Japan, Medieval and Gunpowder ages, battle: taiko of the Sengoku.

In the warring-states period the war drum (jin-daiko) gave the orders on the
field. Here a taiko ensemble: the great odaiko, the chu-daiko, the high
shime-daiko and the wooden "ka" of the rims, under a shinobue (the bamboo
fife of the festivals) and the shamisen. Miyako-bushi scale on D (D Eb G A Bb).
"""
from score import Score, transpose

TITLE = "Taiko"
NATION, AGES, MOOD = "Japanese", (2, 3), "BATTLE_VICTORY"

A = ("A5:1.5 Bb5:0.5 A5:1 G5:1 | Bb5:g A5:3 r:1 | D6:1.5 Bb5:0.5 A5:1 G5:0.5 Eb5:0.5 | D5:3 r:1 | "
     "G5:1 A5:1 Bb5:1 D6:1 | Eb6:1.5 D6:0.5 Bb5:1 A5:1 | G5:0.5 A5:0.5 Bb5:0.5 A5:0.5 G5:1 Eb5:1 | D5:4")
B = ("D5:0.5 Eb5 G5 A5 Bb5 A5 G5 Eb5 | D5:1 G5:1 A5:2 | D6:0.5 Bb5 A5 G5 A5 Bb5 A5 G5 | Eb5:1 G5:1 D5:2 | "
     "G5:0.5 A5 Bb5 D6 Eb6 D6 Bb5 A5 | Bb5:1 A5:1 G5:2 | A5:0.5 Bb5 A5 G5 Eb5 G5 A5 G5 | D5:2! r:2")
BAR = 4


def build():
    s = Score(126, BAR)
    odaiko = s.track("odaiko", 116, gain=1.0, rev=0.3, ring=1.5)
    chu = s.track("chu-daiko", 116, gain=0.7, rev=0.25, pan=-0.2)
    shime = s.track("shime-daiko", 116, gain=0.35, rev=0.15, pan=0.25)
    ka = s.track("ka", 0, drums=True, gain=0.3, rev=0.15, pan=0.3)
    fue = s.track("shinobue", 73, gain=0.95, pan=-0.1, vib=(20, 5.5, 0.3), swing=0.006)
    sham = s.track("shamisen", 106, gain=0.7, pan=0.2, ring=1.2, swing=0.006)
    shaku = s.track("shakuhachi", 77, gain=0.7, pan=0.05, vib=(22, 4.5, 0.6), legato=0.97)

    def groove(at, bars, full=True, ka_on=True, busy=False):
        for i in range(bars):
            a = at + i * BAR
            odaiko.add(a, 1, 40, 112)
            odaiko.add(a + 2.5, 1, 40, 100)
            if i % 2:
                odaiko.add(a + 3.5, 0.5, 40, 92)
            if full:
                for off, v in ((0, 100), (1, 84), (1.5, 78), (2, 96), (3, 88)):
                    chu.add(a + off, 0.5, 50, v)
            step = 0.25 if busy else 0.5
            for j in range(int(BAR / step)):
                shime.add(a + j * step, step, 64, 74 if (j * step) % 2 == 0 else 50)
            if ka_on:
                for off in (0.5, 2.5, 3.25):
                    ka.add(a + off, 0.2, 76, 70)

    def roll(at, beats, v0=60, v1=118):
        n = int(beats / 0.25)
        for j in range(n):
            chu.add(at + j * 0.25, 0.25, 50, v0 + (v1 - v0) * j // max(n - 1, 1))

    t = 0.0
    # the drums call the army: the odaiko alone, then the others join
    odaiko.add(t, 2, 40, 104)
    odaiko.add(t + BAR, 2, 40, 108)
    odaiko.add(t + BAR + 2, 2, 40, 100)
    for i in (2, 3):
        a = t + i * BAR
        odaiko.add(a, 1, 40, 110)
        odaiko.add(a + 2, 1, 40, 104)
        for off in range(4):
            chu.add(a + off, 0.5, 50, 70 + off * 6)
    shaku.play("r:1 D4:4 Eb4:g D4:3 | G4:1.5 A4:0.5 G4:1 Eb4:1 D4:4", t, 84)
    t += 4 * BAR
    groove(t, 3, ka_on=False)
    roll(t + 3 * BAR, 4)
    t += 4 * BAR
    # the fue takes the tune
    fue.play(A, t, 92)
    groove(t, 8)
    t += 8 * BAR
    fue.play(A, t, 96)
    sham.play(transpose(A, -12), t, 86)
    groove(t, 8)
    t += 8 * BAR
    # the shamisen alone, the drums lighter
    sham.play(B, t, 92)
    groove(t, 8, full=False)
    t += 8 * BAR
    fue.play(B, t, 96)
    sham.play(transpose(B, -12), t, 90)
    groove(t, 8)
    t += 8 * BAR
    # break: the drums answer each other
    for i in range(4):
        a = t + i * BAR
        odaiko.add(a, 1, 40, 118)
        roll(a + 1, 2, 64 + i * 10, 96 + i * 8)
        for off in (3, 3.5):
            odaiko.add(a + off, 0.5, 40, 110)
            chu.add(a + off, 0.5, 50, 110)
    t += 4 * BAR
    # all together, the shime doubled
    fue.play(A, t, 102)
    sham.play(transpose(A, -12), t, 94)
    shaku.play(transpose(A, -12), t, 80)
    groove(t, 8, busy=True)
    t += 8 * BAR
    # the last blows
    for off, v in ((0, 120), (1, 112), (2, 124)):
        odaiko.add(t + off, 2, 40, v)
        chu.add(t + off, 0.5, 50, v - 6)
    fue.play("D6:2! r:2", t + 2, 100)
    sham.play("D4+A4+D5:1! r:3", t + 2, 100)
    return s
