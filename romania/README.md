# Romania

A 25th nation for Rise of Nations: Extended Edition: **Romania**, with its own name, flag, leaders, cities and nation power. It shows in the setup screen's nation list with the other 24, can be played by you or a computer player, and can come up as a Random or Random Defensive nation.

Status: first version done 2026-10-26, installed, not yet tried in game. No unique units and no buildings of its own yet (see "Later").

## For players

**The Power of the Carpathians**
- Attrition damage to enemy units in your territory increased by 100%. Attrition upgrades (Allegiance, Oath of Fealty, Patriotism, Nationalism) are free.
- +20% Oil gathering.
- Forts and Towers 25% cheaper. Forts 50% faster to build, exert +3 National Borders, and get their research for free.
- The Militia, Minuteman and Partisan upgrades are free.

Each piece is the game's own code for another nation's power (Russian attrition and oil, Roman forts, Korean militia), switched on for Romania. They can be turned off one by one in `romania.ini`.

**Units:** the European style. In every unit line Romania has the model the Russians have (Eastern European equipment, as the Romanian army had in the Cold War), but not the Russian unique units: where the Russians have Cossacks, Red Guards, the T-80 or the Katyusha, Romania has the ordinary Light Cavalry line, Infantry, Main Battle Tank and Howitzer.

**Units of its own:** quality against quantity, the price following the strength (`ROMANIAN_UNITS` in `tools/build_data.py`). An elite that costs more: Falxmen, Roxolani Lancers, Viteji, Vanatori de Munte. Levies, reservists and older machines that cost less: Comati, Voinici, Darabani, Dorobanti, Automitraliera, R-2, TR-77. The cost is scaled and rounded to tens (the game's unit), then attack and hit points by the ratio the cost really got, so power per resource stays the usual. `tools/build_site.py` writes the player guide's Romania section from the built data: every military unit with its icon and each stat against the average of the 24 original nations (`tools/stat_vs_average.py`).

**Units with a model of their own** (`OWN_ART` in `tools/build_data.py`): the **IAR-80** (2026-10-28), Romania's Fighter. The user made the model in Tripo from a Gemini picture (`models/iar80/source/iar80_tripo.glb`). `tools/make_plane.py` reduces it to 400 triangles as one whole mesh (no cutting or mirroring: the user's rule, they leave holes), bakes a new texture, paints Michael's Cross over the German crosses Gemini drew and marks the light grey bands as player colour. `tools/build_plane_bh3.py` puts it on the P-40's skeleton (so the P-40's animations, propeller disc and gun flashes work), at the P-40's size, as `art/iar80.bh3` and `iar80.tga`. In the rules it is a 12th unit of Romania's own (slot 342, the campaign's Barbarian Javelineers) whose graphics family `BARBARIANJAVELINEERS` becomes the P-40's entry with the new model, with the Fighter's sounds and help text.

```
python tools/make_plane.py models/iar80/source/iar80_tripo.glb models/iar80/work/low400 400 512
python tools/build_plane_bh3.py "<game folder>" models/iar80/work/low400 mod_override
python tools/build_data.py "<game folder>" mod_override
```

The **R-2** (2026-10-29), Romania's Light Tank (slot 349, family `MORTARSHIP`, the Panzer's entry and skeleton): the user's Tripo model (`models/r2/source/r2_tripo.glb`). `tools/make_tank.py` cuts the turret off at its seat (a plane over the superstructure roof, about 2 degrees down to the front) and caps both cuts, covers the running gear with closed blocks following its side outline (the wheels and tracks of the Tripo model are painted on them by looking straight in), drops the running gear hidden inside them and closes its openings, reduces the hull to 260 and the turret to 100 triangles (0 open edges, checked) and replaces the barrels with cylinders (reduced, they turned into cones). The whole tank has 815 vertices: the first build had 1962 and stopped the game with "too many verts for Pre-Transform Batch Transform" when several were on screen (`ENGINE_NOTES.md`, "Vertex limit"; keep new models near the game's own sizes, about 400 for a tank). `tools/build_tank_bh3.py` puts the hull on `body1`, the turret on `pivot node 0` and the shot on the muzzle (`ENGINE_NOTES.md`, "Tank models"). The texture's paint is moved to the olive of the game's tanks when it is built (`olive` in `tools/build_tank_bh3.py`, median hue 60 degrees, saturation 0.5; Tripo's came out a grey khaki, 46 / 0.2; the user asked 2026-10-29).

```
python tools/make_tank.py models/r2/source/r2_tripo.glb models/r2/work/d260 260 100 512 drop
python tools/build_tank_bh3.py "<game folder>" models/r2/work/d260 mod_override
python tools/build_data.py "<game folder>" mod_override
```

**Unit names** (2026-10-26): every Romanian unit has a name from Romanian history, in `source/units.csv` with a line on where it comes from: Dacian Slingers, Falxmen and Roxolani Lancers in antiquity; Voinici, Curteni, Viteji and the Pusca in the age of the voivodes; Darabani, Seimeni, Arnauts and Graniceri later; Pandurs and Dorobanti in the 19th century; Vanatori, Rosiori, the IAR-80 and the R-2 tank in the World Wars; TR-85M1 Bizonul, LAROM, MLI-84 and the F-16 today. You see them when you play Romania; a Romanian enemy's units show them when you select them. A name can change with the age: `LATE_UNITS` in `tools/build_data.py` writes `[late]` in `romania_units.ini` (NAME = first age, name), and from that age the unit keeps the game's own icon of its graphics tier (`romania.c`). The Supply Wagon is the Ox Cart until the Enlightenment Age and the DAC Truck from the Industrial Age, when the game draws it as a truck (the user, 2026-10-29).

**Unit icons:** Romania's own icons come from Gemini sheets of 12, one at a time (see "Building it"). Units without one keep the game's icon.

**Music**: playing Romania, George Enescu's Romanian Rhapsodies come up among the game's own tracks. In the Ancient to Medieval Ages, Rhapsody No. 2 and the slow first half of No. 1; from the Gunpowder Age on, the fast dances of the second half of No. 1. The recording is Constantin Silvestri's with the Czech Philharmonic (Supraphon, 1956), from [IMSLP](https://imslp.org/wiki/2_Romanian_Rhapsodies,_Op.11_(Enescu,_George)), marked there "Public Domain - Non-PD US": free in the EU, not in the US. So it is **not** in the mod pack: download the two MP3s from that page into `music\source\` (`silvestri_rhapsody_1.mp3`, `silvestri_rhapsody_2.mp3`) and run `tools/build_music.py` (needs ffmpeg). It writes `music\mod_override\` (the WAVs in `sounds\tracks\` and `Data\sound.xml`), to copy into the game's `mod_override\`.

**Buildings:** the game's Northern European style (Greco-Roman in ages 1-3), with none of this repo's per-nation art yet.

**Leaders** (names of computer players): Burebista, Decebalus, Basarab, Mircea the Elder, John Hunyadi, Vlad the Impaler, Stephen the Great, Michael the Brave, Lady Chiajna, Constantin Brancoveanu, Alexandru Ioan Cuza, Carol I, Queen Marie, Ferdinand. **Cities** from Bucuresti, Sarmizegetusa and Alba Iulia on. Names are without diacritics: the game's fonts have no s and t with a comma below.

Multiplayer: every player needs the mod (the same files and `romania.ini`). With players who do not have it, the game cannot start: they have 24 nations.

Saved games made before the mod was installed may not load with it.

## Files

| File | What it is |
|---|---|
| `romania.ini` | Goes in the game folder. Turns the mod on in `winmm.dll`; the power's pieces. |
| `mod_override\` | Goes into the game's `mod_override\`. Made by `tools/build_data.py`: `Data\rules.xml` (25 nations), `Data\unitrules.xml`, `buildingrules.xml`, `techrules.xml`, `craftrules.xml` (25-bit nation masks), `Data\help.xml` (the power, `TRIBE24`), `tribes\romania.xml`. |
| `source\romania.xml` | The nation: name, leaders, cities, unit and building style. |
| `source\units.csv` | Romanian unit names, with where each comes from. |
| `romania_units.ini` | Goes in the game folder. Made by `tools/build_data.py`: the names and the icon cells. |
| `icons\` | One PNG per unit (`<unit name>.png`, lower case), cut from the Gemini sheets. |
| `gemini\` | The Gemini icon sheets. |

The flag is in the interface theme (`ui-theme`, `flag_24` in `setupwin.xml`). The engine side is `camera-smooth/src/romania.c` (part of `winmm.dll`). What was found in the game's code is in `ENGINE_NOTES.md`.

## Building it

```
python tools/build_data.py "<game folder>" mod_override
```

Icons: a sheet of 12 is painted in Gemini with the game's own icons of the same units as reference (`tools/icon_refs.py` makes that reference, enlarged, on magenta), then cut into `icons\`:

```
python tools/icon_refs.py ../_for-gemini/1_game_icons_same_order.png 4 SLINGERS JAVELINEERS ...
python tools/cut_icons.py gemini/<sheet>.png Slingers Javelineers ...
python tools/build_data.py "<game folder>" mod_override
```

`build_data.py` packs the icons into `art\romania_units_0.tga` / `_1` (and `_half`), 96 per page, declares them (`TEX_ROMANIA_0`, ...) in `Data\interface.xml` (built on the interface theme's) and writes their cells into `romania_units.ini`.

It builds on the newest versions the other mods made of the same files (Nuclear weapons, then General's Heal, then Entrenchment, then Airdrop, then Data Center), so build those first, and the interface theme's `interface.xml` (`ui-theme`) too. After rebuilding one of them, build this one again and install it last: a rules file from another mod has 24-bit nation masks, and Romania would have no units, buildings or research at all.

`winmm.dll` puts its changes in only when `romania.ini` is in the game folder **and** the `rules.xml` the game reads lists 25 nations; otherwise the game stays as it was (the game refuses a nation list of the wrong length).

## Later

- Unique units (they need campaign-only unit slots, as the Airdrop's Paratroopers): ideas to choose from: Dacian Falxmen (heavy infantry), Calarasi and Rosiori (light cavalry), Vanatori de Munte (mountain infantry), IAR-80 (fighter), TACAM (tank destroyer), TR-85 (main battle tank).
- Buildings of its own in every age (see `NATIONS.md`).
- Its own music, and its own computer player habits (it plays with the game's general AI).
