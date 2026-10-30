# RON Revived

A mod pack for Rise of Nations: Extended Edition (Steam). For players it is one mod; here it is split into one folder per part. Every change, for players: `CHANGES.md`. Open work: `TODO.md`.

## For players

Download `RON_Revived-<version>.zip` from the [latest release](https://github.com/anastasescucristian93-max/RON-Revived/releases/latest), extract it and double-click `install.bat` (`package/README.txt` is the text that comes with it). In multiplayer every player needs the same version. After that, the game's launcher offers each new version itself (an Update button over its picture).

Liked it? Give the repository a star ⭐ and leave a [review](https://github.com/anastasescucristian93-max/RON-Revived/discussions/1). Found a bug: [report it](https://github.com/anastasescucristian93-max/RON-Revived/issues/new?template=bug_report.yml) with your `ron_camera.log`. Ideas are welcome [here](https://github.com/anastasescucristian93-max/RON-Revived/issues/new?template=idea.yml).

## The parts

| Folder | What it is |
|---|---|
| `camera-smooth/` | `winmm.dll`, the code of the mod (camera, interface, most of the gameplay), settings in `ron_camera.ini` |
| `data-center/`, `paradrop/`, `entrenchment/`, `general-heal/`, `nuke-fx/`, `romania/` | Data mods: the game's XML files (units, buildings, techs, tooltips) and their art, in `mod_override/` |
| `translations/` | Ten languages: the game's five plus Russian, Brazilian Portuguese, Polish, Turkish and Romanian, and the mods' own texts in all of them |
| `survival/` | Survival game mode: a game script (`scenario/Scripts/Survival/`), picked under Script in the game setup |
| `ui-theme/` | The interface look (menus, buttons, flags, the launcher) |
| `*-buildings/` | New building art per nation (`nation_art/`, swapped in by the DLL through `roman-buildings/nation_art.ini`); `NATIONS.md` says what each nation has; `modern-buildings/` is art for every nation (`nation_art/all/`) |
| `farm-crops/`, `roman-units/` | Experiments, not in the package |
| `site/` | The player guide page |
| `tools/` | Reverse-engineering helpers for the game's exe |

## Building

1. `camera-smooth/build.ps1` (the DLL).
2. The data mods, in this order, since several write the same files and each builds on the one before: data-center, paradrop, entrenchment, general-heal, nuke-fx (each `tools/build_data.py "<game folder>" mod_override`), the interface theme (`ui-theme/tools/build_*.py`), romania (`romania/tools/build_data.py`), and the translations last (`translations/tools/build_data.py "<game folder>" mod_override`, run from `translations/`: it copies every file the mods changed into each language).
3. `python package/build_package.py [version]`: it lays the `mod_override/` folders over each other in that order, adds every `nation_art/` folder, the DLL and the `.ini` files, checks them (a DLL older than its sources, a picture `nation_art.ini` names that is missing) and writes `dist/RON_Revived-<version>.zip`. Romania's Enescu music (`romania/music/`) stays out: the recording is not free in the US.
