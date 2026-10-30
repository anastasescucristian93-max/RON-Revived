# Engine notes: music

How the game picks the music list (`sound.xml`, `<TRIBE>`, `<AGE>`, moods) is in
`romania/ENGINE_NOTES.md`, "Music". This file is about the audio files themselves.

## Who plays the sound

- `riseofnations.exe` imports no audio API at all (WINMM only for `timeGetTime` and the timer
  events; ole32 for COM). The sound goes through the game's own library **`xasl.dll`** (1.1 MB, in
  the game folder), which loads `xaudio2_9.dll` and calls `XAudio2Create` (its strings: "Could not
  create XAudio engine", "Could not create XAudio source voice", `VoicePool.cpp`, `xasl.cpp`,
  `xasound.cpp`, `WaveData.cpp`).
- `WaveData.cpp` reads the WAV files ("Tried to load WaveData on something that isn't a WAVE
  file!", "No data in this WAVE file!").

## Formats

- Every sound the game ships is PCM (format tag 1), 16 bit, mono or stereo, at rates around
  22050 Hz (21847 to 22311 Hz). Nine music tracks (e.g. `sounds\tracks\Bengal.wav`) and
  `sounds\Thrones_and_Patriots.wav` are 44100 Hz stereo: the music player takes 44.1 kHz.
- `xasl.dll` also has a **Microsoft ADPCM** path (format tag 2): "Read an ADPCM WAVE file with an
  invalid sample per block count! This must be one of 32, 64, 128, 256, or 512 due to XAudio2
  limitations", "Unsupported number of channels while decompressing ADPCM data", "Tried to apply
  ADPCM decompression to something that wasn't ADPCM!". So ADPCM WAVs should play when a block
  holds 32..512 samples (a power of two). ffmpeg's `adpcm_ms` encoder only makes power-of-two
  block sizes in bytes (512 samples stereo needs 524), so `tools/adpcm.py` encodes them.
- 2026-10-29: the user listened to the three versions below and chose ADPCM 44.1 kHz; every
  track is now made that way (`tools/score.py`). Not yet heard in game. Test installed: `mod_override\sounds\tracks\Test_ADPCM.wav`
  (the Doina, ADPCM, 44.1 kHz stereo, 512 samples per block) as the only entry of `<INTROS>` and
  `<FULLINTROS>` in the installed `sound.xml` (which of those lists the main menu plays is not
  known yet either).

## What ADPCM costs

Measured on our renders (signal against the error of encoding and decoding again; "quiet" is
the quietest tenth of the track):

| Version | Size of the Doina | SNR | quiet parts |
|---|---|---|---|
| PCM 22 kHz (what is installed) | 8.7 MB | - | - |
| ADPCM 22 kHz | 2.2 MB | 30.6 dB | 25.5 dB |
| ADPCM 44.1 kHz | 4.4 MB | 40.8 dB | 35.2 dB |

Busy, low pieces lose less (the Taiko at 22 kHz: 46.7 dB). At 44.1 kHz the samples are closer to
each other, so the predictor guesses better and the noise is about 10 dB lower; the music also
keeps everything up to 20 kHz instead of 11 kHz.

## Crash at game start: memory (2026-10-30, suspected, test pending)

- Two games in a row (Big Huge, 8 players) stopped on "art\hammer5.bh3 does not exist for
  Transform Batch Transform" right after the start, although the file is in `art\`. Both logs end
  while the nation tracks are opened (`JukeBox::add_track` opens the tracks of every player's
  tribe, not only the local one's).
- `xasl.dll` decompresses an ADPCM WAV whole into a buffer ("Tried to allocate a decompression
  buffer over 1.5gb!"; no streaming strings in it), so a 6 MB track takes about 25 MB of RAM, and
  `sound.xml` names each nation track 6..15 times (repeated for weight). `riseofnations.exe` is
  32-bit without the large-address-aware flag (characteristics 0x102): 2 GB. Suspected: the
  address space runs out and the next model load fails.
- Test: the installed `mod_override\Data\sound.xml` renamed to `sound.xml.music-test-off` (the
  game's own music); if the same game then starts, the music is the cause.
