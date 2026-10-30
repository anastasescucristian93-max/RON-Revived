"""Romania, Gunpowder to Industrial ages, economy: a hora of the lautari.

The lautari bands of the 17th-19th century boyar courts and village weddings:
fiddle and accordion lead, the tambal runs under them, the cobza and the bass
keep the "um-ta" of the hora. A minor with the raised seventh (the G# that
gives the lautar runs their colour), a middle part in C major.
"""
from score import Score, chord

TITLE = "Hora"
NATION, AGES, MOOD = "Romania", (3, 4, 5), "ECONOMIC"

A1 = ("E5:0.5 A5:0.5 A5:0.5 B5:0.25 A5:0.25 | G#5:0.5 A5:0.5 B5:1 | "
      "C6:0.5 B5:0.25 A5:0.25 B5:0.5 G#5:0.5 | A5:1.5 r:0.5", ["Am", "E", "Am E", "Am"])
A2 = ("C6:0.5 E6:0.5 D6:0.5 C6:0.5 | B5:0.5 C6:0.25 B5:0.25 A5:1 | "
      "G#5:0.5 F5:0.5 E5:0.5 G#5:0.5 | A5:1.5 r:0.5", ["Am", "Am", "E", "Am"])
# the same, the way a fiddler plays it the second time round
A1V = ("E5:0.25 G#5:0.25 A5:0.5 C6:0.25 B5:0.25 A5:0.25 B5:0.25 | G#5:0.25 F5:0.25 G#5:0.25 A5:0.25 B5:1 | "
       "C6:0.25 D6:0.25 C6:0.25 B5:0.25 A5:0.25 B5:0.25 G#5:0.25 E5:0.25 | A5:1.5 E5:0.25 G#5:0.25",
       A1[1])
A2V = ("C6:0.25 B5:0.25 C6:0.25 E6:0.25 D6:0.25 C6:0.25 B5:0.25 C6:0.25 | B5:0.5 C6:0.25 B5:0.25 A5:1 | "
       "G#5:0.25 A5:0.25 F5:0.25 G#5:0.25 E5:0.25 F5:0.25 D5:0.25 E5:0.25 | A5:1.5 r:0.5", A2[1])
B1 = ("G5:0.5 C6:0.5 C6:0.5 D6:0.25 C6:0.25 | B5:0.5 C6:0.5 D6:1 | "
      "E6:0.5 D6:0.25 C6:0.25 D6:0.5 B5:0.5 | C6:1.5 r:0.5", ["C", "G", "C G", "C"])
B2 = ("E6:0.5 F6:0.5 E6:0.5 D6:0.5 | C6:0.5 B5:0.5 A5:1 | "
      "G#5:0.5 B5:0.5 E5:0.5 G#5:0.5 | A5:1.5 r:0.5", ["Dm", "Am", "E", "Am"])
BAR = 2


def build():
    s = Score(104, BAR)
    vln = s.track("vioara", 40, gain=1.0, pan=-0.15, vib=(16, 6.0, 0.12), legato=0.85)
    acc = s.track("acordeon", 21, gain=0.8, pan=0.2, legato=0.85)
    tam = s.track("tambal", 15, gain=0.55, pan=-0.35, ring=1.2)
    cob = s.track("cobza", 24, gain=0.42, pan=0.3, legato=0.35, swing=0.006)
    bass = s.track("contrabas", 32, gain=0.6, legato=0.6)
    fill = s.track("fill", 15, gain=0.5, pan=0.4, ring=1.2)

    def accomp(chords, at, runs):
        t = at
        for bar in chords:
            names = bar.split()
            each = BAR / len(names)
            for j, ch in enumerate(names):
                a = t + j * each
                root = chord(ch, 40)[0]
                bass.add(a, 0.5, root, 88)
                if each == BAR:
                    bass.add(a + 1, 0.5, root + 7 if root + 7 <= 52 else root - 5, 76)
                for off in ([0.5, 1.5] if each == BAR else [0.5]):
                    for k in chord(ch, 57):
                        cob.add(a + off, 0.25, k, 70)
                if runs:
                    notes = chord(ch, 64)
                    seq = notes + [notes[0] + 12] + notes[::-1][:-1]
                    for i in range(int(each / 0.25)):
                        tam.add(a + i * 0.25, 0.25, seq[i % len(seq)], 50 + (14 if i % 4 == 0 else 0))
            t += BAR

    def strain(part, at, lead, vel=82, runs=True, also=None):
        mel, chords = part
        end = lead.play(mel, at, vel)
        if also:
            also[0].play(mel, at, also[1])
        accomp(chords, at, runs)
        return end

    t = 0.0
    # intro: the band tunes into the hora, the tambal on top
    accomp(["Am", "E", "Am", "E"], t, True)
    fill.play("E5:0.25 G#5 B5 E6 D6 B5 G#5 F5", t + 6)
    t += 4 * BAR
    # first time: the fiddle
    for part in (A1, A2, A1V, A2V):
        t = strain(part, t, vln)
    for part in (B1, B2, B1, B2):
        t = strain(part, t, vln, runs=False, also=(tam, 58))
    # second time: the accordion leads, the fiddle plays around it
    for part, fid in ((A1, A1V), (A2, A2V), (A1V, A1), (A2V, A2)):
        vln.play(fid[0], t, 66)
        t = strain(part, t, acc, vel=86)
    for part in (B1, B2, B1, B2):
        t = strain(part, t, acc, vel=86, also=(vln, 72))
    # last time: everybody, then the stop
    for part in (A1V, A2V):
        acc.play(part[0], t, 80)
        t = strain(part, t, vln, vel=92)
    s.tempo_at(t - 2 * BAR, 104)
    s.tempo_at(t + BAR, 88)
    vln.play("A5:0.25 G#5 F5 E5 D5 C5 B4 G#4 | A4+E5:0.5! r:1.5", t, 92)
    acc.play("A4+C5+E5:0.25 r:0.75 A4+C5+E5:0.25 r:0.75 | A4+C5+E5:0.5! r:1.5", t, 84)
    bass.add(t, 0.5, 40, 90)
    bass.add(t + 2, 0.5, 45, 100)
    tam.play("A3+E4+A4+C5+E5:0.5! r:1.5", t + 2, 90)
    return s
