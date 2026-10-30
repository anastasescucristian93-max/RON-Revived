"""Picture of a rendered track (loudness over time + spectrogram) and the numbers
that show clicks or clipping, since the renders cannot be listened to here.

python inspect_wav.py file.wav out.png
"""
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile
from scipy.signal import spectrogram


def read(path):
    """a WAV as the game hears it: PCM, or our ADPCM decoded"""
    with open(path, "rb") as f:
        head = f.read(64)
    if head[head.index(b"fmt ") + 8] == 2:
        import os
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import adpcm
        return adpcm.decode(path)
    return wavfile.read(path)


def main(path, png):
    sr, x = read(path)
    x = x.astype(np.float32) / 32768
    m = x.mean(1)
    win = int(sr * 0.05)
    rms = np.sqrt(np.convolve(m ** 2, np.ones(win) / win, "same"))
    db = 20 * np.log10(np.maximum(rms, 1e-5))
    clip = int((np.abs(x) > 0.995).sum())
    # a click: a sample-to-sample jump far above what the music around it does
    d = np.abs(np.diff(m))
    local = np.convolve(d, np.ones(441) / 441, "same")
    clicks = np.nonzero(d > 12 * local + 0.05)[0]
    print(f"{path}: {len(m) / sr:.1f}s  rms {20 * np.log10(np.sqrt(np.mean(x ** 2))):.1f} dB  "
          f"peak {np.abs(x).max():.3f}  clipped {clip}  clicks {len(clicks)}"
          + (f" at {', '.join(f'{c / sr:.2f}s' for c in clicks[:8])}" if len(clicks) else ""))
    f, t, S = spectrogram(m, sr, nperseg=2048, noverlap=1536)
    fig, ax = plt.subplots(2, 1, figsize=(16, 7), sharex=True, gridspec_kw={"height_ratios": [1, 3]})
    ax[0].plot(np.arange(len(db)) / sr, db, lw=0.5)
    ax[0].set_ylim(-60, 0)
    ax[0].set_ylabel("dB")
    ax[1].pcolormesh(t, f, 10 * np.log10(S + 1e-12), shading="auto", cmap="magma", vmin=-110, vmax=-30)
    ax[1].set_yscale("symlog", linthresh=200)
    ax[1].set_ylim(50, sr / 2)
    ax[1].set_xlabel("s")
    fig.tight_layout()
    fig.savefig(png, dpi=70)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
