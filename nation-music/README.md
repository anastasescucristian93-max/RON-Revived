# Nation Music

Music of each nation's own, per age and mood, written as notes and rendered here to the game's
track format. When you play a nation, its pieces come up among the game's own tracks.

## The pieces

Ages: 0 Ancient, 1 Classical, 2 Medieval, 3 Gunpowder, 4 Enlightenment, 5 Industrial, 6 Modern,
7 Information. "PD" marks a real old tune (public domain); every other piece is written for this
mod in the culture's style. Each piece's file in `pieces/` says more in its first lines.

| Nation | Peace | Battle |
|---|---|---|
| Americans | Oh! Susanna (PD, 4-6): banjo, fiddle, harmonica | Yankee Doodle (PD, 4-5): fifes, drums, then brass |
| Aztecs | Flowers (0-3): ocarina, clay flutes, teponaztli | Jaguar (0-3): huehuetl, conch, whistles |
| Bantu | Kalimba (0-4): mbira, marimba, 12/8 bell, voices | War Drums (0-4): kudu horns, talking drum |
| British | Greensleeves (PD, 3): Tudor consort | March (4-5): fifes and drums; Highlanders (5-6): pipes and drums |
| Chinese | Jasmine Flower (PD, 2-5): guzheng, dizi, erhu | Ambush Drums (2-4): dagu, suona, pipa |
| Dutch | Carillon (3-4): carillon, recorder, harpsichord | Sea Beggars (3-4): trumpets, drums |
| Egyptians | Nile (0-2): harp, ney, sistrum | Chariots (0-2): trumpets, war drums |
| French | Au clair de la lune (PD, 4): Versailles gavotte | Pas de charge (4-5): opens with La Marseillaise (PD) |
| Germans | Ode to Joy (PD, Beethoven, 4-5): orchestra | Prussian March (4-6): brass band, glockenspiel |
| Greeks | Lyre (0-2): lyre, aulos, syrinx | Hoplites (0-2): auloi, salpinx, drums |
| Inca | Huayno (0-3): siku, quena, bombo | Pututu (0-3): conch, wankar drum, pan pipes |
| Indians | Raga Yaman (1-4): sitar, bansuri, tanpura, tabla in teental | Rajput (2-4): shehnai, nagara |
| Iroquois | Water Drum (0-4): flute, water drum, rattle | Longhouse (0-4): drums, flutes |
| Japanese | Gagaku (0-1): sho, hichiriki; Sakura (PD, 2-4): koto, shakuhachi | Taiko (2-3) |
| Koreans | Gayageum (1-4): gayageum, daegeum, janggu in semachi | Daechwita (2-4): royal military march |
| Lakota | Plains (0-5): courting flute | Horsemen (0-5): big drum, gallop |
| Maya | Temple (0-2): clay flutes, tunkul | Holcan (0-3): long trumpets, turtle shells |
| Mongols | Long Song (1-3): morin khuur, limbe, throat drone | Horde (1-4): gallop, horns |
| Nubians | Kissar (0-2): lyre, 6/8 | Land of the Bow (0-2): shawm, war drums |
| Persians | Santur (0-4): santur, ney, tombak | Immortals (0-4): karnay, sorna, naqqara |
| Romania | Doina (0-2), Hora (3-5), Vals (5-7) | Sarba (2-5) |
| Romans | Hydraulis (0-2): water organ, cithara, tibia | Legion (0-2): cornua, tubae |
| Russians | Korobeiniki (PD, 4-5): bayan, balalaikas | Cossacks (4-6): men's choir, brass |
| Spanish | Andalusia (3-5): guitar, castanets | Tercios (3-4): trumpets, fife, drums |
| Turks | Saz Semai (2-4): ney, oud, kanun, 10/8 aksak | Mehter (2-4): the Janissary band |

## Building it

```
python tools/render_all.py [word ...]     # pieces -> mod_override/sounds/tracks/*.wav and out/midi/*.mid
python tools/build_sound.py [game folder] # -> mod_override/Data/sound.xml
python tools/share_mp3.py [word ...]      # MP3 copies in share/, for sending (needs ffmpeg)
python tools/inspect_wav.py file.wav out.png
```

`render_all.py` renders several pieces at once (one per CPU core). The WAVs are not in git: run it
before `package/build_package.py`, which refuses a `sound.xml` that names a missing track.

- `pieces/*.py`: one piece each: the score in a small text notation (see the top of
  `tools/score.py`), its arrangement through `tools/kit.py`, and `NATION`, `AGES`, `MOOD` for
  `sound.xml`. `NATION` is the game's tribe name (`tribes\*.xml`, `<TRIBE name=...>`).
- `tools/kit.py`: strains of melody over chords; accompaniment (bass, pads, drones, arpeggios,
  strums, plucked chords) and percussion patterns that switch on by level (0 intro to 3 climax);
  rolls, tremolo, a second voice in thirds. A strain whose bars do not add up is refused.
- `tools/score.py`: the notation, humanising (timing and velocity jitter), tempo changes, a
  synthetic hall reverb, levelling to the game's loudness (about -17 dB RMS) and the export:
  Microsoft ADPCM WAV, 44100 Hz stereo, 512 samples per block (`fmt` and `data` chunks only), and
  MIDI. ADPCM is a quarter of 16-bit PCM: the tracks take half the room of the game's own 22 kHz
  PCM while keeping the treble up to 20 kHz; its noise is about 40 dB under the music (the user
  compared it by ear against PCM and chose it, 2026-10-29). Measurements in `ENGINE_NOTES.md`.
- `tools/adpcm.py`: the ADPCM encoder (ffmpeg cannot make the block size the game needs) and a
  decoder, used to check what the encoding loses.
- `tools/sf2.py`: a small SoundFont 2 player in numpy (zones, loops, volume envelope, vibrato,
  the SF2 default velocity modulators). Written here because running downloaded synthesizers
  (FluidSynth) is not allowed on this PC and the `tinysoundfont` wheel does not build on
  Python 3.13.
- `tools/build_sound.py`: the game's `Data\sound.xml` with a `<TRIBE>` block per nation. Each
  block has an `<AGE>` list at age 0 and wherever its set of tracks changes; each list is the
  game's tracks of that mood (20 ECONOMIC, 6 BATTLE_VICTORY, 7 BATTLE_DEFEAT) plus ours, each
  listed `REPEAT` (3) times. When Romania's Enescu recordings are on this PC
  (`romania/music/`), it also writes `romania/music/mod_override/Data/sound.xml`, the same list
  with Enescu added to Romania's, to install after this mod.

## Instruments

`tools/soundfont/GeneralUser-GS.sf2` (GeneralUser GS 2.0.3 by S. Christian Collins, free for any
music, commercial included; `LICENSE.txt` next to it). Not in git (32 MB): get it from
https://github.com/mrbumpy409/GeneralUser-GS.

- GM has no lute, erhu, oud, gayageum and so on; the closest sound stands in (nylon guitar for
  lute and oud, the fiddle with a wide vibrato for erhu and haegeum, koto for guzheng and
  gayageum, shakuhachi for ney and the Plains flute, shanai for suona, zurna and shehnai).
- The koto decays fast; a solo koto line leaves holes, so long notes get light re-plucks and
  soft strings under them.
- Instruments differ by 10x in level (accordion against koto); `score.py` measures each
  track's instrument and levels it before the track's `gain` sets the mix. Drum tracks hold one
  kind of sound each, since a track is levelled by its middle note.
- The orchestral drum kit (bank 128, 48) has the concert snare, bass drum and cymbals.

## Checking

The renders cannot be listened to where they are made: `inspect_wav.py` draws the loudness and
a spectrogram and counts clipped samples and clicks. Every track is checked that way; how they
sound is up to the user's ears.
