"""Romania, Medieval to Industrial ages, battle: a sarba of the haiduci.

The sarba is the fast round dance in running triplets that speeds up until the
dancers drop; here it is the band playing the haiduci (the outlaws of the
Carpathian forests) into the fight. D with the augmented second (D Eb F# G A Bb C,
the lautari's "mode of the hijaz"): fiddle, clarinet (the taragot's cousin),
accordion, tambal, cobza, bass and the big toba drum.
"""
from score import Score, chord, transpose

TITLE = "Sarba"
NATION, AGES, MOOD = "Romania", (2, 3, 4, 5), "BATTLE_VICTORY"

A = ("D5:1/3 F#5 A5 G5 F#5 Eb5 | F#5 Eb5 D5 Eb5:2/3 r:1/3 | "
     "Eb5:1/3 F#5 G5 A5 Bb5 A5 | G5 F#5 Eb5 D5:2/3 r:1/3 | "
     "D5:1/3 F#5 A5 D6 C6 Bb5 | A5 Bb5 A5 G5 F#5 Eb5 | "
     "F#5:1/3 G5 F#5 Eb5 D5 C5 | D5:1 r:1",
     ["D", "D", "Cm", "D", "D", "Gm", "D", "D"])
B = ("G5:1/3 A5 Bb5 A5 G5 F#5 | G5 A5 Bb5 C6:2/3 r:1/3 | "
     "F#5:1/3 G5 A5 G5 F#5 Eb5 | F#5 G5 A5 Bb5:2/3 r:1/3 | "
     "C6:1/3 Bb5 A5 Bb5 A5 G5 | A5 G5 F#5 G5 F#5 Eb5 | "
     "D5:1/3 Eb5 F#5 G5 A5 Bb5 | A5:1 D5:2/3 r:1/3",
     ["Gm", "Cm", "D", "Gm", "Cm", "D", "D", "D"])
BAR = 2  # two beats, each a dotted quarter of three triplets


def build():
    s = Score(116, BAR)
    vln = s.track("vioara", 40, gain=1.0, pan=-0.2, vib=(12, 6.5, 0.15), legato=0.9)
    clar = s.track("clarinet", 71, gain=0.8, pan=0.25, legato=0.9)
    acc = s.track("acordeon", 21, gain=0.7, pan=0.1, legato=0.85)
    tam = s.track("tambal", 15, gain=0.5, pan=-0.4, ring=1.1)
    cob = s.track("cobza", 24, gain=0.45, pan=0.35, legato=0.3)
    bass = s.track("contrabas", 32, gain=0.65, legato=0.5)
    toba = s.track("toba", 0, drums=True, gain=0.75, rev=0.15)
    zil = s.track("talger", 0, drums=True, gain=0.3, pan=0.3, rev=0.15)
    tom = s.track("tom", 0, drums=True, gain=0.55, rev=0.2)

    def band(chords, at, runs=True, drums=True):
        for i, ch in enumerate(chords):
            a = at + i * BAR
            root = chord(ch, 38)[0]
            bass.add(a, 0.5, root, 90)
            bass.add(a + 1, 0.5, root + 7 if root + 7 <= 50 else root - 5, 80)
            for beat in (0, 1):
                for k in chord(ch, 57):
                    cob.add(a + beat + 1 / 3, 0.15, k, 52)
                    cob.add(a + beat + 2 / 3, 0.15, k, 68)
                if drums:
                    toba.add(a + beat, 0.2, 36, 100 if beat == 0 else 86)
                    zil.add(a + beat + 2 / 3, 0.1, 42, 60)
            if runs:
                n = chord(ch, 62)
                seq = n + [n[0] + 12] + n[::-1][:-1]
                for j in range(6):
                    tam.add(a + j / 3, 1 / 3, seq[j % len(seq)], 48 + (12 if j % 3 == 0 else 0))

    def part(p, at, leads, runs=True):
        mel, chords = p
        for tr, vel, shift in leads:
            tr.play(transpose(mel, shift) if shift else mel, at, vel)
        band(chords, at, runs)
        return at + len(chords) * BAR

    t = 0.0
    # the toba calls them: two bars of drum, then the band on D
    for i, (k, v) in enumerate([(36, 100), (41, 80), (41, 84), (43, 88), (36, 104), (45, 90), (47, 96), (48, 104)]):
        (toba if k == 36 else tom).add(t + i / 2, 0.3, k, v)
    t += 2 * BAR
    band(["D", "D"], t)
    t += 2 * BAR
    t = part(A, t, [(vln, 84, 0)])
    t = part(A, t, [(vln, 88, 0), (clar, 74, -12)])
    s.tempo_at(t, 124)
    t = part(B, t, [(vln, 88, 0)], runs=False)
    t = part(B, t, [(vln, 90, 0), (clar, 80, 0), (acc, 66, -12)])
    t = part(A, t, [(clar, 86, 0), (acc, 70, -12)])
    t = part(B, t, [(clar, 86, 0), (vln, 74, -12)], runs=False)
    # break: the drums alone, the toms rolling
    for j in range(12):
        tom.add(t + j / 3, 0.2, [45, 47, 48, 50][j // 3], 70 + j * 3)
    toba.add(t, 0.2, 36, 110)
    toba.add(t + 1, 0.2, 36, 110)
    t += BAR
    s.tempo_at(t, 132)
    t = part(A, t, [(vln, 96, 0), (clar, 86, 0), (acc, 78, -12)])
    t = part(B, t, [(vln, 96, 0), (clar, 86, 0), (acc, 78, -12)])
    s.tempo_at(t, 140)
    t = part(A, t, [(vln, 100, 0), (clar, 90, 0), (acc, 82, -12)])
    # the stop
    for tr, v in ((vln, 104), (clar, 94)):
        tr.play("D5:1/3 F#5 A5 D6:1! r:1", t, v)
    acc.play("D4+F#4+A4:1/3 r:2/3 D4+F#4+A4:1! r:1", t, 90)
    bass.add(t, 0.3, 38, 100)
    bass.add(t + 1, 0.5, 38, 110)
    toba.add(t + 1, 0.3, 36, 120)
    tom.add(t + 1, 0.3, 41, 110)
    zil.add(t + 1, 1.5, 49, 100)
    tam.play("D3+A3+D4+F#4+A4:1! r:1", t + 1, 100)

    # the first triplet of every beat leans in
    for tr in (vln, clar, acc):
        tr.notes = [(b, l, k, v + (8 if abs(b * 3 - round(b * 3)) < 1e-6 and round(b * 3) % 3 == 0 else 0))
                    for b, l, k, v in tr.notes]
    return s
