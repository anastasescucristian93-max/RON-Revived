# Nuclear weapons (nuke-fx)

Missile defence, radiation and a nuclear blast that is easier on the eyes. The game side is in `camera-smooth\src\nukefx.c` (settings under `[nuke]` in `ron_camera.ini`); this folder holds the data, art and sounds.

Status: done 2026-10-27, installed, not yet tried in game.

## What it does

- **Interceptors.** Missile Silos train Interceptors (button at (3, 0), 200 Knowledge + 250 Oil, 2.5 times a Nuclear Missile's time). Each silo holds up to 2; the number shows on the button. A silo fires one by itself at an enemy Nuclear Missile or ICBM that flies within 40 tiles of it, if the Interceptor (faster than the nuke) can still meet it before it lands. You see the Interceptor fly up from the silo and both blow up in the air. A nuke shot down does no damage, leaves no crater and does not count for Armageddon. Computer players build Interceptors too when an enemy has or can build nukes. Ordinary anti-aircraft cannot shoot nukes down (it could not in the game either).
- **Radiation.** A blast that goes off leaves a radiation zone over its whole radius for a minute, under a light green haze. Once a second every unit inside, yours and the enemy's, loses 2% of its max hit points at first, less and less until the end (60% in total for a unit that stays the whole minute). Aircraft and buildings are not hurt.
- **Target sign.** While a nuke flies, everyone sees a glowing radiation target sign on the ground over its target (drawn with Gemini, `source\target_sign_gemini.jpg`), shrinking towards the centre, turning slowly and throbbing; a dark disc under it keeps it clear on sand.
- **Only nukes aimed at you.** A silo fires only at a nuke aimed at its owner (a unit or building of yours, or a point in your territory); `interceptor_protect_allies=1` covers allies too.
- **Longer scorch and smoke.** The scorched earth stays 3 times as long (about 86 s instead of 29), and the fires left burning smoke 3 times as long (33 s). The crater itself already stayed for the whole game.
- **The white flash only when you look at it.** Full when the blast is near the middle of the screen, fading with distance, nothing far away.
- **A new alarm** when an enemy launches a nuke: a 4-second air-raid siren (ElevenLabs), in place of the game's `sounds\nukelaunch.wav`.

In multiplayer every player needs the mod with the same `[nuke]` settings (interceptors and radiation change the game). Interceptors, interceptions in flight and radiation zones are kept in saved games (inside the save itself, so they reach the other players of a multiplayer game too).

## Files

| Path | What |
|---|---|
| `mod_override\` | Goes into the game's `mod_override\`. Made by the tools below. |
| `tools\build_data.py` | `Data\unitrules.xml`, `typenames.xml`, `help.xml`, `unit_graphics.xml` (the Interceptor in unit slot 337 "SAS"), `Data\effects_graphics.xml` (the target sign, its glow, the haze and the longer smoke), `art\nuke_haze.tga`, `Data\soundfiles.xml` (the game's, with the Interceptor's two sounds at the end; run `make_interceptor_sounds.py use ...` first). Builds on the other data mods (General's Heal, Entrenchment, Airdrop, Data Center); Romania builds on this one, so rebuild Romania afterwards and install it last. |
| `tools\make_target_decal.py` | `art\nuke_target.tga` and `art\nuke_target_glow.tga` from `source\target_sign_gemini.jpg`: the magenta un-mixed from the picture, a dark disc under it, the bright parts blurred into a glow. |
| `tools\gen_sfx.py`, `tools\prompts.json` | Sound generation with ElevenLabs (key in `ELEVENLABS_API_KEY`); results in `source\elevenlabs\` (mp3) and `work\` (wav). |
| `tools\make_interceptor_sounds.py` | The Interceptor's launch and hit sounds, made from noise and tones (numpy, scipy). With no arguments: the candidates into `sound_choices\` to listen to (L1-L5 launches, I1-I5 hits). `use L<n> I<n>`: the chosen ones into `mod_override\sounds\interceptor_launch.wav` and `interceptor_hit.wav`. |
| `tools\make_alarm.py` | `mod_override\sounds\nukelaunch.wav` from the chosen siren (`work\siren_3a.wav`): fade-out, the original's level. |

## Build

```
python tools/make_target_decal.py
python tools/make_interceptor_sounds.py use L5 I2
python tools/build_data.py
python tools/make_alarm.py
python ../romania/tools/build_data.py "<game folder>" ../romania/mod_override
```

Then copy `mod_override\` into the game's `mod_override\`, and Romania's after it.
