# Engine notes: combat damage modifiers

Findings of 2026-10-26, from `riseofnations.exe` (Steam EE, timestamp 6674863F) with the names of `sbl/rise_z.map` (`tools/rdis.py`). Not tied to one mod; useful for any new unit.

## Where the "X is good against Y" numbers live

- `Balance::type_damage(TypeIndex attacker, TypeIndex target)` 0x0057FB50 (about 8 KB) is the hard-coded counter table. It starts at 100 and multiplies by N/100 for every rule that matches, then returns the percent. The rules test:
  - the OBJ_MASK of both types: `[type + 0x1E4]`, bit 0 = `A` ... bit 25 = `Z`, bits 26-31 = `1`-`6` (the same order as the `Flag_*_OBJMASK_*` names in `Data/balance.xml`);
  - whether the target is a unit (index 0x32-0x19D), a building (0x19E-0x21E) or a wonder (0x20E-0x21E);
  - single types through the virtual `ObjectTypeData::is(int type, int)` 0x0065F7D0 (vtable +0x60): true if the index matches or the type is in that upgrade line (`[type + 0x280]` count, `[type + 0x28C]` word list). For example 0x83 = Flamethrower, 0x112 = Mortar, 0x117 = Flaming Arrow, 0x139 = V2 Rocket.
  - `UnitTypeData::is_siege` (vtable +0x10C) = FLAGS `[type + 0x2B4] & 0x20000` (the `r` flag);
  - the age gap between attacker and target (units only), with bonuses from `Constants + 0x70 .. 0x80`.
- Type indices: unit = 0x32 + its position in `unitrules.xml` (364 units, end at 0x19E); building = 0x19E + its position in `buildingrules.xml` (129 buildings).
- `Balance::compute_modifier` 0x00581CC0 collects the matching rows and columns of `Data/balance.xml` (per type, BUILDINGS/CITIES/FORTS/TOWERS, AGE_n and the Flag_* entries) and combines them with that. The result is cached in a 493 x 493 table of shorts at 0x00C06AFC, which `Balance::return_modifier` 0x00581CA0 reads. So `balance.xml` is only fine tuning on top of the hard-coded table. For example, it gives Howitzer, Rocket Artillery and Katyusha 125% against CITIES, and Hoplites 88% against BUILDINGS.

## Against buildings (from type_damage)

- Siege attacker (mask `S`): x4.30 against any building. Together with the other rules, every Siege Factory unit comes out at **520%**.
- Bombard attacker (mask `B`, only the Flamethrower among foot units): **x2.50**.
- Heavy infantry and melee (Hoplites, Pikemen, Legions): 169%. Fusiliers, Bazooka and Anti-tank Rifle: 130%. Javelineers: 100%. Arquebusiers and Musketeers: 87%. Riflemen and Infantry: 66%. Machine Gun: 50%. Bowmen and Knights: 33%. Tanks: 66%. Commandos: 66% (they use Sabotage instead). Samurai: 65% (the Japanese get their own bonus against buildings, `JAPANESE_DAMAGE` in `rules.xml`).
- A V2 Rocket attacker against a building that is not a wonder: x0.5.
- Not checked: how armor is subtracted after the multiplier, and the extra rows `compute_modifier` adds.

## Tool

`tools/emu_damage.py "<attackers,...>" "<targets,...>"` runs the real `type_damage` under unicorn (`pip install unicorn`) with fake type objects built from the XML and prints the percents. Sanity checks: Pikemen vs Knight 387%, Slingers vs Bowmen 210%, Knight vs Small City 33%. The `balance.xml` layer is not included, so add it by hand.
