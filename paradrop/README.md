# Airdrop

A special ability for the Airbase, for faster city captures on large maps. A transport helicopter flies out of the Airbase, lands at the spot you chose, drops an elite squad of Paratroopers and flies home. When it lands at its Airbase again, part of the price comes back.

Status: everything is written (data files, model, and `camera-smooth/src/airdrop.c` in the mod's `winmm.dll`), not yet tried in game. `airdrop.ini` in the game folder turns it on and holds the numbers. Computer players use it too, only when they see an opening (see below). `--test` builds also let you train both units to look at them.

## In the DLL (`camera-smooth/src/airdrop.c`)

The button is the game's own unfinished "Paradrop" craft (type 0x284), whose targeting works; its casting is replaced:

| Hooked | What it does |
|---|---|
| `BuildOut::make_options` | from the Information Age (age 8; the Modern Age until 2026-10-28), a local player's Airbase gets the craft's button at (3, 0), on the aircraft row; panel.c draws the cooldown on it as a dark clock wipe |
| `GroupData::get_spellcaster`, `get_valid_spellcaster` | answer with a ready Airbase of the selection, so the game starts its own targeting |
| `GroupOut::validate_spell` | valid when the selection has a ready Airbase and the spot is on the map |
| `Group::action_spell` | the command, on every machine: pay, make the helicopter at the Airbase, send it |
| `Objects::process_all` | every frame, after the units moved: fly out, come down to the ground (2 s), the squad appears, lift back to flying height (2 s), fly back, come down, disappear, refund; computer players' decisions every 10 s. While it lands, the helicopter's model height is set here, after the air physics |
| `SelectGroup::add` | the helicopter cannot be selected |
| `Options::draw_cursor_follows` | while aiming: two rings in the player's colour on the ground under the cursor, the landing zone (2 tiles) and its centre |
| `Options::target_spell` | the click that orders a drop (interface only): counted as pending until its command runs, and the targeting starts again on the next frame while drops are left (`airdrop_ui_tick`, from `Camera::check_scroll`) |

With several Airbases selected, panel.c writes on the button how many drops can be called now (`airdrop_charges`): Airbases off cooldown, as far as food, oil and population room go, less the drops ordered but not yet carried out. One click per drop; each command takes the next ready Airbase of the selection.

If the helicopter is shot down, the drop is lost (no squad if it had not landed yet, no refund). If its Airbase is lost, it flies to the nearest other one; with none left it is removed without a refund. Missions are not saved: a helicopter in the air when a game is saved is removed after loading.

Computer players drop only on an enemy city that has lost at least `city_damage_percent` of its hit points, with at most `max_defenders` soldiers within 8 tiles, within `range_tiles` of one of their ready Airbases, and only with `food_reserve` / `oil_reserve` to spare above the price and no other helicopter in the air. Their squad then walks into the city.

## Plan

| | |
|---|---|
| Squads | 2 Paratrooper units, from the Information Age. An infantry unit is a squad of 3 soldiers (`UBER_SIZE 3`), so that is 6 soldiers (it was 3 units, 9 soldiers, until 2026-10-25) |
| Paratrooper | 230 HP, attack 30, armor 5, speed 38, range 13, can capture cities (Assault Infantry: 180, 25, 4, 34, 14) |
| Price | 400 Food + 350 Oil (about 5 Infantry) |
| Refund | a third, when the helicopter is back at its Airbase |
| Cooldown | 45 seconds per Airbase |
| Helicopter | flies to the spot, lands, the squad appears, it lifts off and flies back; the player cannot select or command it |

## How it is made

The game refuses extra unit types, so two campaign-only units are taken over (as the Data Center does):

| Unit slot | Becomes | Look |
|---|---|---|
| 330 "Guerilla Fighters" (type 0x17C) | Paratroopers | Assault Infantry's model, animations and sounds in a black uniform (`art/paratrooper.tga`), under `GRAPH FOUCHE`: the campaign-only Advisor Fouché's graphics entry, sounds and description, taken over (no graphics family is free) |
| 338 "Harrier Jet" (type 0x184) | Transport Helicopter | the Chinook (`GRAPH FIGHTERBOMBER`, whose one graphics entry belonged to the campaign-only Fighter Bomber) |

The Conquest campaigns that use those units may look wrong with the mod installed.

| File | Change |
|---|---|
| `Data/unitrules.xml` | the two slots (on top of the Data Center's version) |
| `Data/typenames.xml` | shown names "Paratroopers", "Transport Helicopter" (on top of the Data Center's version) |
| `Data/help.xml` | the helicopter's description (on top of the Data Center's version) |
| `Data/unit_graphics.xml` | `FIGHTERBOMBER-DEFAULT-AGE0` = the gunship's entry with the Chinook model, both rotor discs, no weapon |
| `Data/anim_graphics.xml` | the unused "LightBomber Default" plays `chinook_propspin.bha` (the game may not take extra animations) |
| `Data/soundtypes.xml` | `FIGHTERBOMBER` gets the helicopter's sounds |
| `Data/craftrules.xml` | "Paradrop" switched on (`PREQ0 None`; `FROM` stays None, so no unit gets it); `COST2 40f/35o` so the tooltip shows the price with resource icons (the DLL charges it) |
| `Data/typenames.xml`, `help.xml` | the craft is shown as "Airdrop", with its description |
| `art/iface_unit_orders.tga` | the button's icon in the craft's own cell (4, 4) |
| `airdrop.ini` (game folder) | turns the DLL part on; price, refund, cooldown, squad size, computer players |
| `art/chinook.bh3`, `chinook.tga`, `chinook_propspin.bha` | the model, its texture, the rotor spin |

Build, in this order (the Data Center's files first, because this mod builds on them):

```
python tools/make_model.py source/transport_helicopter_tripo.glb work/heli500 500 512
python tools/build_bh3.py "<game folder>" work/heli500 mod_override
python tools/build_data.py "<game folder>" mod_override [--test]
```

`tools/make_icon.py` cuts the button icon (`icons/airdrop.png`). Everything loads through `mod_override\`, which needs the mod's `winmm.dll`. Every player in a multiplayer game must have the same files.

`ENGINE_NOTES.md` has the BH3 / BHA format, the rotor setup and what was learned on the way (the engine's own unfinished "Paradrop" ability, the "NO DEFAULT UNIT GPIECE" error).
