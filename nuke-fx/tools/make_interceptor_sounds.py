r"""Synthesise the Interceptor's sounds: candidates to listen to, then the chosen ones for the game.

    python make_interceptor_sounds.py                      the candidates into nuke-fx\sound_choices\
    python make_interceptor_sounds.py use L3 I2            copy launch L3 and interception I2 into
                                                           mod_override\sounds\ (what the game plays)

Launch (L*): the Interceptor leaving its silo. Interception (I*): the nuke shot down high in the
air, heard from far away. Grave and sinister, as the user asked (2026-11-05: the first, bright
whoosh sounded cheap): low layers, saturation, a dark reverb, nothing above about 3 kHz. All made here from noise and tones with numpy
and scipy (no recordings), 22050 Hz stereo 16-bit like the game's own sounds. Each candidate is
seeded, so a run makes the same files again.

The game plays them through soundfiles.xml (tags interceptorlaunch / interceptorhit, written by
build_data.py) for the two players involved: the silo's owner and the nuke's.
"""
import os
import shutil
import sys
import wave

import numpy as np
from scipy import signal

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
CHOICES = os.path.join(MOD, "sound_choices")
SOUNDS = os.path.join(MOD, "mod_override", "sounds")
RATE = 22050
PEAK = 0.72
GAME_FILES = {"L": "interceptor_launch.wav", "I": "interceptor_hit.wav"}


# ---- building blocks ----------------------------------------------------------------------

def t_axis(secs):
    return np.arange(int(secs * RATE)) / RATE


def noise(rng, secs):
    return rng.standard_normal(int(secs * RATE))


def lowpass(x, hz, order=4):
    return signal.sosfilt(signal.butter(order, hz, "low", fs=RATE, output="sos"), x)


def highpass(x, hz, order=4):
    return signal.sosfilt(signal.butter(order, hz, "high", fs=RATE, output="sos"), x)


def bandpass(x, lo, hi, order=3):
    return signal.sosfilt(signal.butter(order, [lo, hi], "band", fs=RATE, output="sos"), x)


def sweep_filter(x, f0, f1, q=2.5, curve=1.0, kind="band", passes=2):
    """A state-variable filter whose centre moves from f0 to f1 over the sound (curve > 1 = late),
    run `passes` times for a steeper slope."""
    for _ in range(passes - 1):
        x = sweep_filter(x, f0, f1, q, curve, kind, 1)
    n = len(x)
    u = (np.arange(n) / max(1, n - 1)) ** curve
    fc = f0 * (f1 / f0) ** u
    g = np.tan(np.pi * np.minimum(fc, RATE * 0.45) / RATE)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    out = np.zeros(n)
    ic1 = ic2 = 0.0
    for i in range(n):
        v3 = x[i] - ic2
        v1 = a1[i] * ic1 + g[i] * a1[i] * v3
        v2 = ic2 + g[i] * v1
        ic1 = 2 * v1 - ic1
        ic2 = 2 * v2 - ic2
        out[i] = v1 if kind == "band" else v2 if kind == "low" else x[i] - k * v1 - v2
    return out


def env_adsr(n, attack, hold, decay, curve=2.0):
    """Rise in `attack` s, stay `hold` s, fall over `decay` s (power `curve`), zero after."""
    t = np.arange(n) / RATE
    e = np.zeros(n)
    a = t < attack
    e[a] = (t[a] / attack) ** 0.7 if attack > 0 else 1
    h = (t >= attack) & (t < attack + hold)
    e[h] = 1
    d = (t >= attack + hold) & (t < attack + hold + decay)
    e[d] = (1 - (t[d] - attack - hold) / decay) ** curve
    return e


def exp_decay(n, secs):
    return np.exp(-np.arange(n) / RATE / secs)


def thump(secs, f0, f1, decay):
    """A pitched drop: a sine falling from f0 to f1, decaying."""
    t = t_axis(secs)
    f = f1 + (f0 - f1) * np.exp(-t / (decay * 0.5))
    ph = 2 * np.pi * np.cumsum(f) / RATE
    return taper(np.sin(ph) * exp_decay(len(t), decay))


def taper(x, frac=0.3):
    """Fades the last `frac` of a piece to zero, so it never ends with a click."""
    n = len(x)
    k = max(1, int(n * frac))
    e = np.ones(n)
    e[n - k:] = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, k))
    return x * e


def crackle(rng, secs, density, decay_secs):
    """Sparse clicks, thinning out."""
    n = int(secs * RATE)
    clicks = (rng.random(n) < density / RATE) * rng.choice([-1.0, 1.0], n) * rng.random(n)
    clicks = clicks * exp_decay(n, decay_secs)
    return highpass(signal.lfilter([1.0], [1.0, -0.6], clicks), 1500)


def pad(x, secs):
    n = int(secs * RATE)
    return np.concatenate([x, np.zeros(max(0, n - len(x)))])[: max(n, len(x))]


def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[: len(p)] += p
    return out


def delayed(x, secs, gain=1.0):
    return np.concatenate([np.zeros(int(secs * RATE)), taper(x, 0.1) * gain])


def stereo(mono, rng, width=0.25, drift=None):
    width = width * 0.3
    """Left/right from one mono signal: a little decorrelated noise-free delay, and a pan that
    can drift (the missile climbing past)."""
    n = len(mono)
    d = int(0.0007 * RATE)
    other = np.concatenate([np.zeros(d), mono[: n - d]])
    left = mono * (1 - width / 2) + other * (width / 2)
    right = other * (1 - width / 2) + mono * (width / 2)
    if drift is not None:
        p = np.linspace(drift[0], drift[1], n)  # -1 left .. 1 right
        left = left * np.sqrt((1 - p) / 2) * np.sqrt(2)
        right = right * np.sqrt((1 + p) / 2) * np.sqrt(2)
    return np.stack([left, right], axis=1)


def finish(st, fade_in=0.004, fade_out=0.08):
    n = len(st)
    e = np.ones(n)
    a, b = int(fade_in * RATE), int(fade_out * RATE)
    e[:a] = np.linspace(0, 1, a)
    e[n - b:] *= np.linspace(1, 0, b)
    st = highpass(st.T, 30, 2).T * e[:, None]  # no DC, without the offset a mean would leave
    st = st / (np.abs(st).max() + 1e-9) * PEAK
    level = np.abs(st).max(axis=1)
    loud = np.nonzero(level > PEAK * 10 ** (-60 / 20))[0]
    end = min(n, loud[-1] + int(0.05 * RATE)) if len(loud) else n  # the silent end cut off
    st = st[:end]
    k = min(end, int(0.03 * RATE))
    st[end - k:] *= np.linspace(1, 0, k)[:, None]
    return st


# ---- grave building blocks (the user, 2026-11-05: deep and sinister, never cheap or sharp) ----

def brown(rng, secs):
    """Brown noise: the deep rumble white noise lacks."""
    x = np.cumsum(rng.standard_normal(int(secs * RATE)))
    x = highpass(x, 18, 2)
    return x / (np.abs(x).max() + 1e-9)


def drone(secs, f0, f1, beat=1.6):
    """Two low sines a little apart (they beat slowly): an ominous hum, its pitch sliding f0 -> f1."""
    t = t_axis(secs)
    f = f0 * (f1 / f0) ** (t / secs)
    a = np.sin(2 * np.pi * np.cumsum(f) / RATE)
    b = np.sin(2 * np.pi * np.cumsum(f + beat) / RATE + 1.3)
    return taper((a + 0.8 * b) / 1.8, 0.4)


def mid_noise(rng, secs, lo=160, hi=850):
    """The audible part of a roar: deep but in the range small speakers play (brown noise alone
    is all sub-bass)."""
    x = bandpass(rng.standard_normal(int(secs * RATE)), lo, hi, 2)
    x = lowpass(x, hi * 0.8, 2)
    return x / (np.abs(x).max() + 1e-9)


def sat(x, drive=2.5):
    """Soft saturation: weight without clipping."""
    x = x / (np.abs(x).max() + 1e-9)
    return np.tanh(x * drive) / np.tanh(drive)


def reverb(st, rng, rt=2.2, wet=0.45, tone=1500):
    """A dark room: two decaying noise tails (one per side) convolved with the sound."""
    n = int(rt * 1.3 * RATE)
    t = np.arange(n) / RATE
    out = []
    for ch in range(2):
        ir = lowpass(rng.standard_normal(n), tone, 2) * np.exp(-6.9 * t / rt)
        ir[: int(0.012 * RATE)] *= np.linspace(0, 1, int(0.012 * RATE))  # the tail starts after the sound
        ir /= np.sqrt((ir ** 2).sum())
        out.append(signal.fftconvolve(st[:, ch], ir))
    w = np.stack(out, axis=1)
    dry = np.vstack([st, np.zeros((len(w) - len(st), 2))])
    return dry * (1 - wet * 0.5) + w * wet * 2.0


def dark(st, hz=2800):
    """Nothing sharp: the whole sound under ~3 kHz."""
    return lowpass(st.T, hz, 4).T


# ---- the launches -------------------------------------------------------------------------

def launch_deep_ignition(rng):
    """L1: the motor lights with a heavy low boom, a deep roar swells and climbs away, a low hum
    under it."""
    secs = 3.0
    n = int(secs * RATE)
    boom = sat(thump(1.2, 70, 28, 0.4), 2.0)
    roar = sat(lowpass(brown(rng, secs), 520) + 1.2 * mid_noise(rng, secs, 150, 900), 1.8) * env_adsr(n, 0.25, 0.35, 2.2, 1.6)
    body = sat(bandpass(brown(rng, secs), 55, 220), 2.0) * env_adsr(n, 0.15, 0.25, 1.8, 1.8)
    hum = drone(secs, 49, 41) * env_adsr(n, 0.4, 0.6, 1.8, 1.5) * 0.35
    st = stereo(mix(boom * 0.9, roar * 0.9, body * 0.8, hum), rng, 0.3, drift=(0.0, 0.25))
    return dark(reverb(st, rng, 2.2, 0.4))


def launch_silo_flyover(rng):
    """L2: the ground of the silo rumbles, then a deep roar rises out of it and passes over,
    from one side to the other, darker as it goes."""
    secs = 3.4
    n = int(secs * RATE)
    rumble = sat(lowpass(brown(rng, secs), 160), 2.2) * env_adsr(n, 0.6, 0.3, 2.3, 1.6)
    roar = sweep_filter(brown(rng, secs) + 1.3 * mid_noise(rng, secs, 150, 1000), 1000, 260, q=0.7, kind="low")
    roar = sat(roar, 1.8) * env_adsr(n, 0.9, 0.4, 2.0, 1.5)
    st = stereo(mix(rumble, delayed(roar, 0.3) * 0.9), rng, 0.4, drift=(-0.55, 0.55))
    return dark(reverb(st, rng, 2.5, 0.45))


def launch_dark_thrust(rng):
    """L3: a pulsing deep thrust, like a heavy motor's flutter, over a hum that sinks."""
    secs = 2.8
    n = int(secs * RATE)
    t = t_axis(secs)
    flutter = 1 + 0.45 * np.sin(2 * np.pi * (16 - 5 * t / secs) * t)
    thrust = sat((lowpass(brown(rng, secs), 380) + 1.1 * mid_noise(rng, secs, 140, 750)) * flutter, 2.2) * env_adsr(n, 0.12, 0.6, 2.0, 1.4)
    hum = drone(secs, 55, 38, 1.2) * env_adsr(n, 0.2, 0.8, 1.6, 1.4) * 0.45
    boom = sat(thump(0.9, 60, 30, 0.3), 2.0) * 0.8
    st = stereo(mix(boom, thrust, hum), rng, 0.3, drift=(0.0, 0.3))
    return dark(reverb(st, rng, 2.0, 0.4), 2400)


def launch_far_away(rng):
    """L4: a launch heard from far off: a muffled thud, then a long low roar like distant
    thunder rising and fading, in a big space."""
    secs = 3.4
    n = int(secs * RATE)
    thud = sat(lowpass(thump(0.9, 80, 32, 0.3) + 0.4 * brown(rng, 0.9) * exp_decay(int(0.9 * RATE), 0.15), 220), 2.0)
    roar = sat(lowpass(brown(rng, secs), 340) + 0.99 * mid_noise(rng, secs, 130, 650), 1.7) * env_adsr(n, 0.8, 0.5, 2.0, 1.4)
    st = stereo(mix(thud, delayed(roar, 0.15) * 0.85), rng, 0.5, drift=(0.1, 0.35))
    return dark(reverb(st, rng, 3.0, 0.6), 2000)


# ---- the interceptions --------------------------------------------------------------------

def hit_deep_airburst(rng):
    """I1: a deep, muffled boom high up, the sound falling away into a long dark tail."""
    secs = 3.4
    n = int(secs * RATE)
    boom = sat(lowpass(brown(rng, secs), 300) + 1.1 * mid_noise(rng, secs, 120, 700), 2.2) * env_adsr(n, 0.02, 0.1, 2.6, 2.4)
    sub = sat(thump(1.6, 62, 26, 0.55), 1.8)
    st = stereo(mix(boom, sub * 0.9), rng, 0.6)
    return dark(reverb(st, rng, 3.0, 0.5), 2200)


def hit_rolling_thunder(rng):
    """I2: a heavy blow, then a rumble that rolls on like far thunder."""
    secs = 4.0
    n = int(secs * RATE)
    blow = sat((lowpass(brown(rng, 0.5), 700) + 1.3 * mid_noise(rng, 0.5, 150, 900)) * env_adsr(int(0.5 * RATE), 0.03, 0.05, 0.4, 2.0), 2.0)
    roll = (lowpass(brown(rng, secs), 220) + 0.99 * mid_noise(rng, secs, 110, 550)) * (1 + 0.6 * lowpass(rng.standard_normal(n), 3))
    roll = sat(roll, 1.8) * env_adsr(n, 0.15, 0.4, 3.2, 1.8)
    st = stereo(mix(blow, delayed(roll, 0.08) * 0.9, thump(1.2, 70, 30, 0.4) * 0.7), rng, 0.6)
    return dark(reverb(st, rng, 2.8, 0.45), 2200)


def hit_sinister(rng):
    """I3: a deep boom that leaves a low, uneasy hum hanging in the air."""
    secs = 4.0
    n = int(secs * RATE)
    boom = sat((lowpass(brown(rng, 1.8), 260) + 1.1 * mid_noise(rng, 1.8, 110, 650)) * env_adsr(int(1.8 * RATE), 0.02, 0.08, 1.6, 2.4), 2.2)
    sub = sat(thump(1.4, 58, 27, 0.5), 1.8)
    hum = drone(secs - 0.3, 41, 37, 1.1) * env_adsr(int((secs - 0.3) * RATE), 0.6, 0.8, 2.3, 1.5) * 0.4
    st = stereo(mix(boom, sub * 0.8, delayed(hum, 0.3)), rng, 0.6)
    return dark(reverb(st, rng, 3.0, 0.5), 2000)


# ---- from the game's own nuke sounds (the user, 2026-11-05: the nuke's sound, shorter, slow at
# first and then gathering speed fast, as the Interceptor does) ------------------------------

GAME_SOUNDS = os.path.join(os.environ.get("RON_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Rise of Nations"),
                           "sounds")


def read_game(name):
    with wave.open(os.path.join(GAME_SOUNDS, name)) as w:
        x = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, w.getnchannels()).astype(float) / 32768
        rate = w.getframerate()
    if x.shape[1] == 1:
        x = np.repeat(x, 2, axis=1)
    src_t = np.arange(len(x)) / rate  # the game's files are 22039..22069 Hz: to 22050
    t = np.arange(int(len(x) / rate * RATE)) / RATE
    return np.stack([np.interp(t, src_t, x[:, c]) for c in range(2)], axis=1)


def varispeed(x, secs, r0, r1, curve=2.0):
    """Plays x at a speed (and pitch) rising from r0 to r1 over `secs`: slow at first, then faster
    and faster (curve > 1). Stops early if the source runs out."""
    n = int(secs * RATE)
    u = np.arange(n) / n
    pos = np.cumsum(r0 + (r1 - r0) * u ** curve)  # source sample read at each output sample
    pos = pos[pos < len(x) - 1]
    out = np.stack([np.interp(pos, np.arange(len(x)), x[:, c]) for c in range(2)], axis=1)
    k = min(len(out), int(0.6 * RATE))  # it leaves: the last 0.6 s fade away
    out[len(out) - k:] *= (0.5 + 0.5 * np.cos(np.linspace(0, np.pi, k)))[:, None]
    return out


def launch_from_nuke_launch(rng):
    """L5: the game's nuke launch (icbm.wav), 2.4 s: from 0.6 times its speed, deep and slow,
    to 1.8 times, fast at the end; the top kept dark."""
    st = varispeed(read_game("icbm.wav"), 2.4, 0.6, 1.8, 2.2)
    return dark(st, 3500)


def launch_from_nuke_blast(rng):
    """L6: the same from the game's nuclear explosion (supernuke.wav): its first seconds, slow
    and heavy at first, then rushing."""
    st = varispeed(read_game("supernuke.wav"), 2.4, 0.55, 1.9, 2.2)
    return dark(st, 3000)


CANDIDATES = {
    "L1": ("launch_1_deep_ignition", launch_deep_ignition),
    "L2": ("launch_2_silo_rumble_flyover", launch_silo_flyover),
    "L3": ("launch_3_dark_pulsing_thrust", launch_dark_thrust),
    "L4": ("launch_4_far_away", launch_far_away),
    "L5": ("launch_5_nuke_launch_gathering_speed", launch_from_nuke_launch),
    "L6": ("launch_6_nuke_blast_gathering_speed", launch_from_nuke_blast),
    "I1": ("intercept_1_deep_airburst", hit_deep_airburst),
    "I2": ("intercept_2_rolling_thunder", hit_rolling_thunder),
    "I3": ("intercept_3_boom_and_uneasy_hum", hit_sinister),
}


def write(path, st):
    data = (np.clip(st, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def make(code):
    name, fn = CANDIDATES[code]
    return name, finish(fn(np.random.default_rng(1000 + list(CANDIDATES).index(code))))


def main():
    args = sys.argv[1:]
    if args[:1] == ["use"]:
        os.makedirs(SOUNDS, exist_ok=True)
        for code in args[1:]:
            code = code.upper()
            if code not in CANDIDATES:
                raise SystemExit(f"unknown candidate {code}: {', '.join(CANDIDATES)}")
            name, st = make(code)
            out = os.path.join(SOUNDS, GAME_FILES[code[0]])
            write(out, st)
            print(f"{code} {name} -> {out}")
        return
    os.makedirs(CHOICES, exist_ok=True)
    for f in os.listdir(CHOICES):
        if f.endswith(".wav"):
            os.remove(os.path.join(CHOICES, f))
    for code in CANDIDATES:
        name, st = make(code)
        write(os.path.join(CHOICES, f"{code}_{name}.wav"), st)
        print(f"{code}_{name}.wav  {len(st) / RATE:.2f} s")


if __name__ == "__main__":
    main()
