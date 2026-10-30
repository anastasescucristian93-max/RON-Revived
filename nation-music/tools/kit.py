"""Arrangement kit: a piece is strains of melody over chords, with accompaniment and percussion
that switch on by level (0 sparse intro, 1 light, 2 full, 3 climax).

    p = Piece(bpm, beats_per_bar)
    p.strain("A", "<notation>", ["Am", "E", "Am E", ...])   # one chord (or two) per bar
    p.add(bass(b, [(0, ROOT), (1, FIFTH)]), lo=1)            # accompaniment from level 1 on
    p.perc(drum, [(0, KICK, 100)], lo=2)                     # a percussion bar from level 2
    p.groove(["Am", "E"], 0)                                 # bars of accompaniment only
    p.play("A", [(violin, 84), (clarinet, 70, -12)], 2)      # the strain, its leads, a level
A strain whose bars do not add up to the bar length is refused, so wrong note lengths show at once.
"""
from score import Score, _dur, chord, diatonic, midi, transpose

# General MIDI drum keys
KICK, STICK, SNARE, CLAP, SNARE2 = 36, 37, 38, 39, 40
TOM_LO, HH, TOM_LO2, TOM_MID, OHH, TOM_MID2, TOM_HI, TOM_HI2 = 41, 42, 43, 45, 46, 47, 48, 50
CRASH, RIDE, CHINA, TAMB, COWBELL, CRASH2 = 49, 51, 52, 54, 56, 57
BONGO_HI, BONGO_LO, CONGA_MUTE, CONGA_HI, CONGA_LO = 60, 61, 62, 63, 64
TIMBALE_HI, TIMBALE_LO, AGOGO_HI, AGOGO_LO, CABASA, MARACAS = 65, 66, 67, 68, 69, 70
CLAVES, WOOD_HI, WOOD_LO, TRIANGLE = 75, 76, 77, 81
ROOT, FIFTH, OCTAVE = 0, 1, 2


def bar_lengths(text):
    """Length in beats of each bar of a line of notation (the rules of Track.play)."""
    out, cur, d = [], 0.0, 1.0
    for tok in text.split():
        if tok == "|":
            out.append(cur)
            cur = 0.0
            continue
        name, _, rest = tok.rstrip("!?").split("~")[0].partition(":")
        if rest == "g":
            continue
        if rest:
            d = _dur(rest)
        cur += d
    out.append(cur)
    return out


class Piece:
    def __init__(self, bpm, bar):
        self.s = Score(bpm, bar)
        self.bar = bar
        self.strains = {}
        self.parts = []  # (lowest level, highest level, function(chord, start, length, level))
        self.hits = []   # (lowest level, highest level, track, pattern, every n bars)
        self.t = 0.0

    def track(self, name, prog, **kw):
        return self.s.track(name, prog, **kw)

    def strain(self, name, mel, chords):
        lens = bar_lengths(mel)
        bad = [(i + 1, round(n, 3)) for i, n in enumerate(lens) if abs(n - self.bar) > 1e-6]
        if bad:
            raise ValueError(f"strain {name}: bars (number, beats) {bad}, a bar is {self.bar}")
        if chords is not None and len(chords) != len(lens):
            raise ValueError(f"strain {name}: {len(lens)} bars but {len(chords)} chords")
        self.strains[name] = (mel, chords or [None] * len(lens))

    def add(self, fn, lo=1, hi=9):
        self.parts.append((lo, hi, fn))

    def perc(self, track, pattern, lo=1, hi=9, every=1):
        """pattern: (beat in the bar, key, velocity) hits, repeated every 'every' bars"""
        self.hits.append((lo, hi, track, pattern, every))

    def groove(self, chords, level):
        """Accompaniment and percussion for one bar per chord entry; returns the beat after."""
        for i, bar in enumerate(chords):
            a = self.t + i * self.bar
            names = bar.split() if bar else []
            each = self.bar / max(len(names), 1)
            for j, ch in enumerate(names):
                for lo, hi, fn in self.parts:
                    if lo <= level <= hi:
                        fn(ch, a + j * each, each, level)
            for lo, hi, tr, pat, every in self.hits:
                if lo <= level <= hi and i % every == 0:
                    for off, key, vel in pat:
                        tr.add(a + off, 0.25, key, vel)
        self.t += len(chords) * self.bar
        return self.t

    def play(self, name, leads, level):
        mel, chords = self.strains[name]
        for lead in leads:
            tr, vel, shift = (tuple(lead) + (0,))[:3]
            # shift: semitones, or a function of the notation (a second voice: kit.third(scale))
            line = shift(mel) if callable(shift) else transpose(mel, shift) if shift else mel
            tr.play(line, self.t, vel)
        return self.groove(chords, level)

    def tempo(self, bpm, at=None):
        """a new tempo from now on (a step; tempo points are otherwise joined by ramps)"""
        a = self.t if at is None else at
        self.s.tempo_at(a - 0.01, self.s.bpm_at(a))
        self.s.tempo_at(a, bpm)

    def ritard(self, bars, bpm):
        """slow down over the last 'bars' bars before now"""
        a = self.t - bars * self.bar
        self.s.tempo_at(a, self.s.bpm_at(a))
        self.s.tempo_at(self.t, bpm)

    def roll(self, track, key, beats, v0=50, v1=110, step=0.25, at=None):
        a = self.t if at is None else at
        n = max(int(round(beats / step)), 1)
        for j in range(n):
            track.add(a + j * step, step, key, v0 + (v1 - v0) * j // max(n - 1, 1))

    def final(self, parts, at=None):
        """parts: (track, notation, velocity) played from now (or 'at'); the end of the piece"""
        a = self.t if at is None else at
        for tr, text, vel in parts:
            tr.play(text, a, vel)


def tremolo(mel, step=0.25, shortest=1.0):
    """a plucked line with every note of 'shortest' beats or more struck again every 'step'
    (pipa, mandolin, balalaika, oud)"""
    out, d = [], 1.0
    for tok in mel.split():
        if tok == "|":
            out.append(tok)
            continue
        name, _, rest = tok.partition(":")
        if rest and rest != "g":
            d = _dur(rest.rstrip("!?").split("~")[0])
        if rest == "g" or name == "r" or d < shortest:
            out.append(f"{name}:{rest}" if rest == "g" else f"{name}:{d}")
            continue
        out += [f"{name}:{step}"] * int(round(d / step))
    return " ".join(out)


def third(scale, steps=-2):
    """a lead's shift: the line moved by scale steps (-2: a third below) in the given scale"""
    return lambda mel: diatonic(mel, steps, scale)


# accompaniment: each returns a function(chord, start, length, level) for Piece.add

def pad(track, low=55, vel=50, top=None):
    """the chord held"""
    def f(ch, a, n, lev):
        for k in chord(ch, low)[:top]:
            track.add(a, n, k, vel)
    return f


def drone(track, keys, vel=50):
    """fixed notes held whatever the chord (bagpipe drones, tanpura, the ison)"""
    keys = [midi(k) if isinstance(k, str) else k for k in keys]

    def f(ch, a, n, lev):
        for k in keys:
            track.add(a, n, k, vel)
    return f


def bass(track, hits=((0, ROOT),), low=36, vel=80, dur=0.5):
    """root / fifth / octave of the chord at the given beats"""
    def f(ch, a, n, lev):
        r = chord(ch, low)[0]
        fifth = r + 7 if r + 7 < low + 12 else r - 5
        for off, which in hits:
            if off < n - 1e-6:
                track.add(a + off, min(dur, n - off), [r, fifth, r + 12][which], vel)
    return f


def pluck(track, hits, low=57, vel=60, dur=0.3):
    """chord tones at the given beats: (beat, i) with i an index into the chord tones going up
    (the root an octave higher after the last one), or (beat, "c") for the whole chord"""
    def f(ch, a, n, lev):
        tones = chord(ch, low)
        tones = tones + [t + 12 for t in tones]
        for off, i in hits:
            if off >= n - 1e-6:
                continue
            for k in (tones[:len(tones) // 2] if i == "c" else [tones[i]]):
                track.add(a + off, dur, k, vel)
    return f


def strum(track, beats, low=52, vel=64, spread=0.04, dur=0.8):
    """the chord strummed (strings struck one after another, 'spread' beats apart) on each beat"""
    def f(ch, a, n, lev):
        for off in beats:
            if off < n - 1e-6:
                for i, k in enumerate(chord(ch, low) + [chord(ch, low)[0] + 12]):
                    track.add(a + off + spread * i, dur, k, vel - 3 * i)
    return f


def arp(track, step, order=(0, 1, 2, 3, 2, 1), low=60, vel=50, accent=12, every=None):
    """running broken chord; the first note of each group of 'every' (default the order) louder"""
    every = every or len(order)

    def f(ch, a, n, lev):
        tones = chord(ch, low)
        tones = tones + [t + 12 for t in tones]
        for i in range(int(round(n / step))):
            track.add(a + i * step, step, tones[order[i % len(order)]], vel + (accent if i % every == 0 else 0))
    return f
