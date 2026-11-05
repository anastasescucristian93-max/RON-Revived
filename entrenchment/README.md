# Entrenchment

A defensive building for infantry in Rise of Nations: Extended Edition: the **Sandbag Nest**. Ranged infantry dig into it and fight from it with their own weapons, with bonuses, standing still.

Status: first test 2026-10-25 failed (squads could not get in, Citizens and computer players built it); reworked, then reworked again on 2026-10-26 after the second test (the build button, full cover); installed.

## For players

- Ranged infantry build it, not Citizens (from The Art of War): the build button on their panel, with the price, the building cursor and the outline. They walk beside it, build it and dig into it.
- To dig into one: right-click it with ranged infantry (garrison cursor), or the game's garrison button and hotkey. A squad that built it goes in by itself. One squad per fortification; its three soldiers stand on an even triangle, one at the back and two at the front.
- Dug in: +1 range, attacks 40% faster, +20% damage. They hold their ground.
- The fortification takes every hit meant for them until it is destroyed (half a Tower's hit points for the age). Then they are in the open.
- Melee units, Scouts and Explorers cannot dig in; only foot soldiers can (no tanks or guns).
- Citizens do not have it in their build menu; computer players do not build it.
- Digging in costs 35% of the squad's price, +10% for every squad already dug in, up to +150%.
- The eject button on the Nest empties it.

## Tank Traps

A second building in this mod (2026-11-05): steel anti-tank obstacles on one tile, in the slot of the scenario-only Washington Monument (type 0x1C1). Citizens build them from the military build menu (cell 4, 1) from the Modern Age (ages 7 and 8); computer players do not plan them (`entrench.c`, `LeaderData::type_avail`). Being a building, they block every unit until destroyed. Numbers: `TRAPS_RULES` in `tools/build_data.py`.

Art: one Gemini picture with four variations on white (`gemini/tank_traps.jpg`), cut and sized by `tools/make_traps.py`:

```
C:\AI\ComfyUI-Easy-Install\ComfyUI-Easy-Install\python_embeded\python.exe ..\roman-buildings\tools\rmbg_matte.py gemini/tank_traps.jpg work/traps_matte.png
python tools/make_traps.py gemini/tank_traps.jpg work/traps_matte.png [--grid 2x2] [--order 0,1,2,3]
```

It writes `work/art/tank_traps_0..3.tga` (one canvas and origin for all four), the depth mesh `tank_traps_0z.bh3`, the offsets, the build menu icon and `work/preview_tank_traps.png`. `build_data.py` adds the Tank Traps only once that art is there. The game draws `tank_traps_0.tga`; the `[all]` rule `tank_traps_0.tga~4:pick` in `roman-buildings/nation_art.ini` gives every trap one of the four for good, from its place on the map (`nation_art.c`). Installed now: one hedgehog only (`gemini/tank_traps.jpg`, `--grid 1x1 --order 0,0,0,0 --wire 8`), so all four files are the same picture until three more looks are made.

## Files

| File | What it is |
|---|---|
| `entrench.ini` | Goes in the game folder. Turns the mod on in `winmm.dll` and holds its numbers. |
| `mod_override\` | Goes into the game's `mod_override\`. Made by `tools/build_data.py`. |
| `gemini\` | The Gemini images. |
| `clean\` | The sprite before wear (`weather.py` is run from it). |
| `work\` | Intermediate art, previews, the mesh check. |

The logic is in `camera-smooth/src/entrench.c` (part of `winmm.dll`). What was found in the game's code is in `ENGINE_NOTES.md`.

## Building it

```
python tools/make_nest.py gemini/sandbag_nest_square.jpg work/art [--shape --cx --cy --rx --ry --wall --back-wall]
python tools/check_mesh.py
python ../roman-buildings/tools/weather.py work/art/sandbag_nest.tga work/sandbag_nest_worn.png 1.0
python tools/build_data.py "<game folder>" mod_override
```

`make_nest.py` takes the ring's measurements in the image (centre and half axes of the ground ellipse, wall heights), scales the image to a 2x2 footprint and writes the sprite, its depth mesh (the same ring in the world, so soldiers inside are hidden behind the front wall) and the sprite offsets. `work/check_mesh.png` shows the mesh over the sprite. The weathered PNG is then written back over `work/art/sandbag_nest.tga` with `write_tga` from `make_nest.py`.

Age 8 has no entry of its own in the game: `nest_7.tga` (made with `make_nest.py ... --name nest_7 --like nest_6`) is drawn instead of `nest_6.tga` by the `[all]` rule in `nation_art.ini` (`roman-buildings/nation_art.ini`, installed in the game folder).

`build_data.py` builds on the Data Center's `buildingrules.xml` / `building_graphics.xml` and on the Airdrop's `typenames.xml` / `help.xml` / `craftrules.xml` / `iface_unit_orders.tga`, so build those mods first (Data Center, then Airdrop, then this one). The Leave Nest button is an unused craft; its icon is `icons/exit.png`. The build and garrison buttons are the game's own. The Nest takes the slot of the scenario-only Lenin Statue.

Every player in a multiplayer game needs the same files and `entrench.ini`.
