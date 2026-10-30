# City districts

Three end-game buildings for Rise of Nations: Extended Edition, for every nation, that each add a bonus for as long as they stand. The folder is still called `data-center`: the districts started as the Data Center's Expansions.

Status: built and installed, not yet tested in game. The rules need the mod's `winmm.dll`, and `data_center.ini` must be in the game folder (it turns them on and holds their values).

## The districts

2x2 buildings, built from the panel of any of your cities (Small, Large or Major City), on a plot next to that city or to one of its districts, with at most 2 empty tiles between them (`[districts] gap`) so units can pass. A builder (the "District Builder", a copy of the Citizen) comes out of the nearest city, builds every site placed (Shift places several), one after the other, then walks back and disappears. It cannot be selected; killed, the next one comes out 20 seconds later.

They unlock once every Library technology up to the last Age of the game is researched (the four last ones excepted), so a game that stops at the Modern or Industrial Age gets them too (`[districts] need_last_age`).

| District | Bonus each | Price of the first |
|---|---|---|
| Housing District | +10 population limit, beyond the usual 200 | 6500 Food + 3400 Knowledge |
| Commerce District | +30 to the resource-rate limit of every resource, beyond the usual 999 | 3000 Knowledge + 3000 Wealth |
| Industrial District | +3% to all resource income | 3000 Knowledge + 2000 Wealth |

Build as many as you like: their bonuses add up, and each district of a kind you own makes the next one of that kind 5% dearer. A destroyed or captured district takes its bonus with it. Computer players do not build them.

The bonuses can be changed in `data_center.ini` (`[expansions]`), the prices and the 5% in `tools/build_data.py` (`EXPANSIONS`, `RAMP`).

## The Data Center (removed)

The first version (2026-10-26) was a Wonder-sized Information Age building in the Washington Monument's slot, with the Expansions ordered at it. The user replaced it with the districts on 2026-10-29 and had it removed the same day: the slot keeps the game's own rules (never offered), and its art (the Roman and the Dutch versions) moved to `retired/nation_art/`. The sources stay in `gemini/`, `sprite/` and `clean/`.

## How it is made

`riseofnations.exe` refuses to start when `buildingrules.xml` has a different number of buildings than it was compiled with, so every new building takes the slot of a scenario-only one that skirmish games never use:

| Building | Slot | Type |
|---|---|---|
| Housing District | El Dorado | 0x1C0 |
| Commerce District | Courtyard Fountain | 0x207 |
| Industrial District | the last scenario House (HOUSE35), renamed "Lighthouse" | 0x1E6 |

The files are generated from the game's own files:

```
python tools/build_data.py "<game folder>" mod_override
```

They change `Data/buildingrules.xml`, `building_graphics.xml`, `unitrules.xml` (the builder), `typenames.xml`, `help.xml`, `masks.txt` and `art/iface_buildings_wonders.tga` (+ `_half`), and add `art/district_<kind>.tga` (+ `z.bh3` depth meshes). They are loaded through `mod_override\`, which needs the mod's `winmm.dll` (file overrides). The other data mods build their versions of the shared files on top of these, so build this mod first (the order is in the main README).

`camera-smooth/src/datacenter.c` in `winmm.dll` does the rest:

- the build rules (the Library unlock, the plot next to a city or district) and the builder;
- the bonuses, counted from the districts each player owns, added after the game's own limits:
  - population after the clamp to 200;
  - resource-rate limit after the clamp to 999;
  - efficiency scales both the rates and the limits, so income grows even at the limit.

The game counts the buildings itself, so saved games need nothing extra. `ENGINE_NOTES.md` lists the game functions used.

Every player in a multiplayer game must have the same files.

## Art

- Districts: one look for every nation for now, communist style: `gemini/district_<kind>_v1.jpg` (prefab blocks, a department store, a workshop), fitted to a 2x2 plot by `tools/district_art.py` (into `work/art`).
- Icons: `icons/district_<kind>.png`, scaled to 61x39.
