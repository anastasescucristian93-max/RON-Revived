# The General's Heal

A fifth ability for every General: **Heal**. It costs a full bar of craft points (1000), like his other four. For 5 seconds your units in the General's circle get back 25 % of their hit points in total, a little at a time (every 0.2 s). The circle moves with the General; a unit that walks in later gets the rest of the share. Decoys and buildings are not healed, and soldiers of a squad who already died do not come back.

Status: done 2026-10-27, installed, not yet tried in game.

**No more decoys** (2026-10-27, not yet tried in game): the General's **Create Decoys** ability is gone, for every player and the computer players too. Its button no longer appears, and the General's tooltip and the loading tip about Generals no longer mention it. Decoys placed by a scenario still work as before.

Icon: a field medic's bag (`source/heal_gemini.jpg`, from Gemini, cut to `icons/heal.png`), in the free cell (6, 10) of the unit icon page `art\iface_units_03.tga`. Hotkey: E with a General selected (`[heal] hotkey` in `ron_camera.ini`); the game's key table has no place for it, so the DLL catches the key, and the game's hotkey settings do not list it.

The ability is the game's own unfinished "Rally!" craft: its button place next to the other four, the General's rally animation and its sound were already in the game, but casting it did nothing. Engine notes: `camera-smooth/ENGINE_NOTES.md`, "The General's Heal".

## Files

| | |
|---|---|
| `mod_override\` | Goes into the game's `mod_override\`. Made by `tools/build_data.py`: `Data\craftrules.xml` (Rally! switched on for the General, 1000 craft points, its icon; Create Decoys taken away, `FROM None`), `typenames.xml` (shown as "Heal"), `help.xml` (its tooltip; the decoy lines removed from the General's tooltip and tip 50), `art\iface_units_03.tga` (+ `_half`, the icon). |
| `camera-smooth/src/heal.c` | The healing, in the mod's `winmm.dll`. `general_heal` and `[heal]` (`percent`, `seconds`) in `ron_camera.ini`. |

The rules change the game: in multiplayer every player needs the same files and settings.

## Building

```
python tools/build_data.py "<game folder>" mod_override
```

It builds on the newest versions the other mods made of the same files (Entrenchment, then Airdrop, then Data Center), so build those first (the icon page too: rebuilding Entrenchment overwrites it, so build this one again afterwards). Romania builds on this one: build Romania again afterwards and install it last.

## The effect

Drawn by `camera-smooth/src/heal.c`, nothing in this folder: green rings on the ground under every healed unit, and since 2026-10-29 a green halo and rising green crosses over its head (the cross is `heal_plus` in the rank atlas, `ui-theme/tools/build_rank_icons.py`, so the interface theme must be built and installed too).

## Later

- Whether computer players use it.
