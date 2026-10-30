# Survival

A game mode: hold out for 30 minutes against waves of enemies that come in from the edges of the map. The whole map is yours; the enemy has no cities, only armies, and they are one or two ages behind you.

Status: first version 2026-10-29, compiled and played once by the user. Second version the same day (clocks, spawns at the edge of the territory, no-build zone), installed, not yet tried in game. A game script (`scenario/Scripts/Survival/survival.bhs`) plus support in the mod's `winmm.dll` (`camera-smooth/src/survival.c`, `survival_support` in `ron_camera.ini`). Without the DLL the script still runs, with no clocks and no zone.

## How to play

1. Custom game (skirmish) setup: yourself and **one computer player**, as your enemy.
2. Under **Script**, pick **Survival**.
3. The computer player's **difficulty** sets the waves:

| Difficulty | Enemy age | Wave size |
|---|---|---|
| Easiest, Easy, Moderate | 2 ages behind you | smaller |
| Hard, Tough, Toughest | 1 age behind you | bigger |

Any map, any nation, any starting age. Other computer players can be your allies; every player at war with the wave player is a defender.

## What happens

- At the start the computer player's capital, Citizens and Scout are removed (the first 10 seconds). It is not defeated: it only sends waves.
- **Waves:** the first at 2:00, then every 80 seconds, down to every 45 seconds near the end. Each wave is bigger: about 4 + 1.5 per wave soldiers (times the difficulty), at most 60 a wave and 200 alive. Mix: 40 % light infantry, 25 % heavy infantry, 15 % archers / mounted ranged / machine guns, 20 % cavalry; siege from wave 6.
- **Clocks** at the top right (from the DLL): time left to survive, and time to the next wave (yellow when it is a boss wave, red in the last 10 seconds). `survival_clock_y` in `ron_camera.ini` moves them.
- **Where:** in the fog just outside the defenders' territory. From the target's capital the script walks outwards along a random direction and takes the first land tile that no defender owns and that is more than 16 tiles from every defender object (and, in single player, off screen). If no direction gives one, a point in the no-build zone; last, the edge of the map. The wave attack-moves to the capital (or any building) of one of the defenders.
- **No-build zone:** a tenth of the map on the side away from the defenders' starts (the side opposite their average start position), kept free so the waves always have room. Nobody can build there ("Can't build in enemy territory."), the computer players neither. Flags mark its inner edge (single player only).
- **Age:** the waves use the generic unit of each line in the age of the most advanced defender, less 1 or 2. Ageing up makes the waves stronger too.
- **Boss waves** at 5, 10, 15, 20 and 25 minutes, besides the normal waves: a General, heavy cavalry (tanks later), siege and heavy infantry, more each time.
- Idle wave units are sent at a defender again every 15 seconds.
- The wave player takes no attrition (otherwise your territory, which can cover most of the map, would melt every wave on the way).
- **Victory:** every defender still standing at 30:00 wins. Defeat: the game's normal rules (losing all your cities).

## Settings (top of the script)

`SURVIVE_SECONDS` (1800), `FIRST_WAVE` (120), `MAX_ALIVE` (200), `FOG` (16, tiles from any defender object for a spawn point), `debug_lines` (1 = a chat line per wave: where, which age, for whom). The unit tables by age are the arrays `li`, `hi`, `rg`, `lc`, `hc`, `sg`.

## Not yet

- Its own entry in the game style list (next to Survival of the Fittest): that list is in the exe, so it needs DLL work. For now it is under Script.
- Ideas for later versions: TODO.md, "Survival mode".

## To check in game (second version)

- The clocks at the top right: "Survive 29:xx" and "Next wave in 1:xx" from the start; "Boss wave in" in yellow a minute before 5:00, 10:00 ...; red under 10 seconds; "Survived!" at the end. They must not cover the game's own clock lines or the menu buttons.
- `ron_camera.log`: "survival: clocks and no-build zone ... installed" and "survival: no-build zone x .. y .. (tiles)".
- The zone: on the side of the map away from your start, the flags along its edge, and the placement cursor red with "Can't build in enemy territory." inside it (Citizens and walls too).
- The waves: the `[survival]` chat line says `found 1` (edge of your territory), `2` (zone) or `3` (map edge); they should appear just out of sight and reach you in well under a minute.

## To check in game (first version, done 2026-10-29)

- The start line shows the wave player and the difficulty number (`[survival] wave player 2, difficulty N, ...`): which numbers the game gives for Easiest .. Toughest (the lag rule assumes 0-5 or 1-6).
- The computer player's capital and units disappear, and the game does **not** end in a victory right away (`disable_city_defeat`).
- Waves appear at the edge, off screen, and walk to the capital; the notice at the top; the boss waves.
- The units are of the right age (`age(p)` is 0 for Ancient, as the campaigns use it).
- Victory at 30:00.
