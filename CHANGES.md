# What the mods change

Every change these mods make to Rise of Nations: Extended Edition (Steam), for players.
The game's own files are never modified. The mods load through `winmm.dll` in the game folder, and deleting their files restores the original game.

## Installing

- **One package for everything** (2026-10-28): the whole pack comes as one zip, `RON_Revived-<version>.zip`. Extract it and double-click `install.bat`: it finds the game through Steam, copies everything into place, keeps a copy of any game file it replaces, keeps your `ron_camera.ini` on an update, and removes files an older version had that the new one no longer has. `uninstall.bat` removes it all and puts back what it replaced. It refuses to run while the game is open or when another mod already uses `winmm.dll`. Romania's Enescu music is not in it (the recording is not free in the US).
- **Updates from the launcher** (2026-10-31): when a new version is out, the game's launcher shows it over its picture, with how much there is to download and an **Update** button. Update downloads only the parts of the pack that changed, right in the launcher, with a progress bar in the button's place (click it to cancel), checks that they arrived whole, then closes the launcher for a few seconds, installs them the same way `install.bat` does and starts the game again. The extras (the nation music) are not part of it: the Extras tray shows when one of yours has a new version. Without internet, or with no new version, nothing shows. `check=0` under `[update]` in `ron_camera.ini` turns it off. Works from the first version that has it on; the one before has to be installed by hand one last time.

## Tank Traps in every age (`entrenchment`, `camera-smooth`), new 2026-11-10

- **Citizens build Tank Traps from the Ancient Age**, for 20 Wood (+5 for every one you own) in every age, instead of 20 Metal from the Modern Age.
- **A look of their own in the early ages:** sharpened stakes lashed with rope in the Ancient Age, a beam bristling with iron-tipped spikes in the Classical, Medieval and Gunpowder Ages, a hedgehog of squared timber with iron straps and barbed wire in the Enlightenment and Industrial Ages, the steel hedgehogs from the Modern Age.
- **They grow stronger with your age:** 240 hit points and armor 2 in the Ancient Age, 300 in the Classical, 420, 540, 720 and 960 in the ages after, 1200 and armor 8 from the Modern Age. Traps you built earlier grow when you age up.

## Survival: its own button, its own map, a wave bar (`survival`, `camera-smooth`), new 2026-11-10

- **A Survival button in the Solo menu**, under Quick Battle (the menu's frame is one button taller). It opens the Quick Battle setup: choose your nation, the starting age, the map size and **one computer player as your enemy** (its difficulty sets the waves), then Start. Map Style reads **Survival plain** and Script **Survival**, both greyed: the game runs the Survival script on its own map by itself.
- **The Survival map:** a flat, open, dry plain with no water, no rivers, no mountains or cliffs, light woods and rocks, and **you in the middle**. It replaces whatever map style is chosen in the setup. Picking Survival under Script in a custom game gives the same map (`survival_map=0` under `[gameplay]` in `ron_camera.ini` keeps the chosen map).
- **Enemy buildings:** 36 Barracks, Stables and Siege Factories stand one by one, spread evenly over the map (one in every cell of a 7 x 7 grid, none near the middle), and the waves march out of them: infantry from a Barracks, cavalry from the nearest Stable, siege from the nearest Siege Factory. 12 Towers stand between them and 4 Forts on the diagonals, far from the middle. From one building for the first waves, from two from wave 4, three from wave 10 and four from wave 18. Destroy all of them and the waves come in from the edges of the map instead.
- **Waves three times rarer and three times bigger** (time to build between them): about every 4 minutes at first, every 2 1/4 minutes at the end; a boss wave after every second wave. `WAVE_SPACING` in the script; the wave bar follows it.
- **Reward cards after every boss wave:** when a boss wave's General falls (or 4 minutes after it came), the game pauses and three cards rise in the middle of the screen: pick one, and its bonus lasts all game. **Buildings:** +15 % hit points, +1 range, +12 % damage, +12 % attack speed, -9 % damage taken. **Hero:** +15 % damage, -12 % damage taken, +1 range and sight, +1 level, +1 soldier in the squad, healing 0.6 % of its hit points a second in battle too. **Army:** +6 % damage, -6 % damage taken, +6 % attack speed, +9 % hit points, +1 range for ranged units, every soldier at least veteran level 1; +12 % damage for infantry, +12 % hit points for cavalry and armour, +1 range for siege and artillery. **At once:** resources (180 of each per age), two squads of mercenaries, or a next wave 18 % smaller. Cards of one kind add up, at most three of each (two for range); Reinforcements and War School (gold rim) come from the second boss on. Your computer allies take a card too.
- **Ruins and small cards:** an enemy building you destroy (camps, Towers, Forts) leaves ruins where it stood one time in two. Send any unit to explore them: besides the ruins' usual resources you get a small card, a quarter of a boss card's bonus (Mercenaries never come from ruins), shown the same way in the middle of the screen; a click goes on.
- **The enemy expands:** every 2 minutes it builds a new camp (Barracks, Stable or Siege Factory) or a Tower 8 to 14 tiles from one of its buildings still standing, never in your territory, off the edges and away from the middle. New camps send waves too, and leave ruins when destroyed.
- **Choose the game's length:** the Survival setup's Time Limit list: **Short** (30 minutes), **Medium** (45), **Long** (1 hour) or **Very long** (1 hour 30). The waves keep their rhythm, so a longer game has more of them; their ages stretch over the whole game, and the final assault comes at the end of the time you chose. Your choice is kept for the next game.
- **The waves age on their own:** their age no longer trails yours; it follows the wave number, from the Ancient Age at wave 1 to the Enlightenment (Easiest), the Industrial (Moderate) or the Modern Age (Toughest) by the last waves, faster with the difficulty. The final assault is one age ahead of the last wave.
- **The game ends with a final assault, and you choose what calls it:** three buttons in the Survival setup, beside the hero's: **a Wonder** (finish any Wonder; lose it and you lose), **the Information Age**, or **the time** (45 minutes). Whatever you choose, the final assault comes at 45:00 at the latest. It is the biggest wave of the game, three in one, from all four sides with Generals; destroy every enemy soldier and you win. The clock at the top right counts down to it ("Final assault"). Your choice is kept for the next game.
- **Fixed: the hero squad's hit points** showed more than the maximum (170 / 85, 114 / 77 for the two riders): the maximum now counts the whole squad.
- **Big squads stand side by side:** a squad of more than 3 (the hero's six) stands in a row beside its captain when it stops (in a ring after some moves), instead of piling up on three spots. Squads of 3 stand as before.
- **A bigger hero squad:** the infantry hero starts as one squad of six soldiers (8, 12, 14, 18 at levels 15 to 30), the cavalry hero as one squad of two riders that move together like an infantry squad; a fallen soldier or rider comes back when the squad heals.
- **Waves about a sixth as big** as before (18 %): about 1 + a soldier every three waves (times the difficulty), at most 12 a wave. Siege comes with every other wave only (and every other boss wave), half as much as before. `WAVE_PCT` in the script.
- **Your computer allies get a hero too:** the rider, which becomes an armoured car. Their AI takes care of it: every 5 seconds a hero below 60 % of its hit points, or more than 20 tiles from its capital, is sent back home.
- **Fixed: the waves never reached you.** The clearing that removes the battle director's town in the first 10 seconds never stopped, so it killed every wave as it came out and destroyed the enemy camps. It stops now. Boss waves are now told apart from normal ones properly too (the boss clock was read twice, and the second look always missed it).
- **Fixed: a computer ally started at the edge of the map** (and was cleared away) when the game built its town before yours; your allies now always start round the middle.
- **Co-op:** everyone on your side defends together: other human players and computer allies. Every human player gets a hero, the waves are 60 % bigger for every defender past the first, and each part of a wave goes for one of you. Your allies start round the middle of the map, the battle director at the edge. Without teams, the last computer player is the battle director and every other computer becomes your ally. For a test with computer allies, `coop_test=1` under `[survival]` in `ron_camera.ini` keeps the computer rows and Other Nations in the Survival setup; Other Nations then counts your allies only (one to begin with), and the battle director is added after them when you press Start.
- **The battle director plays in white**, so its buildings and soldiers stand out from yours and your allies' (the game has no grey player colour). The hero buttons now stand at the bottom of the players panel, under the rows.
- **See the whole map:** `reveal_map=1` under `[survival]` in `ron_camera.ini` sets Reveal Map to All Visible in Survival games, to watch the waves (0, Normal, by default).
- **No-build band:** a tenth of the map along all four edges, where the waves come in, instead of one side. Flags mark its inner edge.
- **The wave bar** standing at the right edge of the screen, under the clocks: a circle for every wave down the 45 minutes, grey once it has come, the next one lit amber and pulsing, the boss waves larger and red with a skull, and a small pointer where the game is now; "Wave 7 / 27" under it. The players' score list is hidden in Survival games, so the bar reaches down to 84 % of the screen's height. `survival_bar=0` under `[ui]` turns it off, `survival_bar_top` and `survival_bar_length` move and size it.
- **Metal:** the Survival plain has a mountain by your city (and, with every start in the middle while the map is made, usually a second one) and a few small mountains out on the plain, for mines.
- **The hero's medallion** floats over its head (infantry or cavalry), so you can find it among your soldiers; the Patriots' light is gone again.
- **The heroes grow alike:** a power that rises at levels 15, 20, 25 and 30 (x1.33, x2, x2.33, x3). The infantry hero's squad grows with it (3, 4, 6, 7, 9 soldiers, the new ones coming as the squad heals); the rider stays one unit and gets the same power as hit points (up to x3) and damage and attack rate (up to x1.73 each). Their experience is divided by that power, so a bigger squad does not rank faster.
- **Choose your hero** in the Survival setup with two medallion buttons under your own row (name and nation): the infantryman or the rider; the chosen one is lit. Infantry: a squad of the Barracks' first line (Slingers ... Assault Infantry). Cavalry: the Stable's first cavalry (Light Horse, Light Cavalry, Elite Light Cavalry, Hussar), which becomes an armoured car in the Industrial Age (Armored Car, Armored Scout Car, Armored Cavalry); no Metal in that line, and each nation gets its own version. Your choice is kept for the next Survival game.
- **Resources round you:** the map's resources, woods and rocks no longer pile up round the battle director's start in one corner; they lie round your city and out on the plain.
- **The setup shows only your row:** the waves come from one hidden computer player, the battle director; its rows and Other Nations are gone. Your Difficulty sets the waves.
- **The setup's rules are fixed and locked:** Big Huge map size, Normal speed, Small Town, Standard resources, Normal technology, population 200, Ancient to Information Age, Elimination Conquest, Victory Survival (no wonder, territory or income victory). Turbo Resources, No Nation Powers, No Unique Units, Save to Profile, the Nukes slider and the Units button are gone (no units are turned off in a Survival game). Your nation, colour, Difficulty and Cooperative AI stay yours to choose. Cooperative AI is off in a Survival setup unless you ticked it the last time (kept apart from Quick Battle's profile, under `[survival]` in `ron_camera.ini`).
- **Your hero:** you start with a squad of the first infantry line (Slingers, then Javelineers, Elite Javelineers, Arquebusiers, Musketeers, Riflemen, Infantry, Assault Infantry) beside your capital. It becomes the unit of every new age by itself, ranks on past level 10 up to **level 30** (each level above 10: +2 % damage, +3 % hit points, +2 % attack rate; its rank shows as "Hero"), and if it falls it is gone for good.
- **The wave bar shows a section:** the last 2 waves and the next 6, large, each with its number ("Boss" with a skull), the pointer moving from one to the next; "Wave 7 / 55" under it.
- **The next wave's clock in large letters** at the top centre, under the nation's title (yellow for a boss wave, red in the last 10 seconds); "Survive" stays at the top right. `survival_timer_y` under `[ui]` moves it.
- **45 minutes** to hold out instead of 30, a **boss wave 30 seconds after every fifth wave** (after waves 5, 10, 15 ...; no longer by the clock, which sent the first boss right after wave 1), and **7 minutes of grace**: the first wave comes at 7:00 instead of 2:00.

## Shorter menu buttons (`ui-theme`), new 2026-11-10

- The main menus' buttons say it in a word or two: Tutorial, Campaign, Solo, Multiplayer, Options, Extras; in the Solo menu Quick Battle, Survival, Campaign, Scenarios, Load Game, Replays, Main Menu; in Multiplayer Quick Match, Browse Games, Leaderboards, Replays; in Extras Editor, Workshop, Credits, About. Every one of them is translated in all ten languages.
- The menu buttons are a little taller and narrower (270 x 46, with 10 pixels between them; 340 x 40 before), and the menu's frame is as much narrower.
- **The main menu is Play, Options, Extras, Exit.** Play opens a panel with Tutorial, Campaign, Solo, Multiplayer and Main Menu.
- **The menus' help line** (what the button under the mouse does) stands right under the menu's frame, centred, whatever the menu's length; before it was fixed low and went under the Solo menu's buttons (`menu_help_centred=0` under `[ui]` for the game's place).
- **The menus slide 25 % faster** when one panel falls away and the next falls in (`menu_anim_speed` under `[ui]`, 125 = 25 % faster, 100 = the game's).
- **Conquer the World's screens** have the steel buttons too (New Campaign, Load Campaign, Back, Continue and the battle windows' buttons).
- Fixed: the game wrote the menus' new titles into its own `Data\string_lookup.xml` (it saves every text it meets that the file lacks). That file is put back as it shipped, and such writes now go to `mod_override\` instead.

## Veteran ranks rebalanced (`camera-smooth`), new 2026-11-10

- Every level now gives **+2 % damage, +3 % hit points and +3 % attack rate** (before: +4 % attack and +4 % hit points). Armor is taken off every hit as a flat amount, so extra damage used to break through armor far more than its percent; a faster attack adds exactly what it says. Level 10 in all: +20 % damage, +30 % hit points, +30 % attack rate (before +40 % / +40 %, and +30 % attack rate from levels 5 and 8, which are gone).
- **Healing out of combat from level 8** instead of 9.
- The other bonuses stay: +1 sight at 3, 10 % less damage taken at 6, +1 range at 7, 20 % less at 10. In `ron_camera.ini` the keys are new (`damage_percent_per_level`, `hit_points_percent_per_level`, `attack_rate_percent_per_level` under `[ranks]`), so an older file does not bring the old values back; they are game rules, the same for every player.

## Lines of Tank Traps with Shift + drag (`camera-smooth`), new 2026-11-10

- Placing Tank Traps: hold **Shift**, press, **drag** and let go, and a whole line of traps is laid from where you pressed to where you let go, in any direction, **diagonals too**. While you drag, every trap of the line shows as a ghost, red where it cannot stand (those are skipped).
- **Turn the line:** with Shift still held, start the next drag at the end of the last line and it goes on from there in the new direction, as in other strategy games.
- A diagonal line is solid: each trap touches the next by a side, so nothing slips between two traps on the diagonal (`tank_trap_solid_lines=0` under `[qol]` for a thin diagonal with fewer traps).
- The Citizens build them one after the other, after what they were doing; each trap costs what it costs one by one, so a long line stops where your Metal runs out.
- A plain click (or Shift + click) places one trap as before. `tank_trap_lines=0` under `[qol]` turns the lines off.

## Farms in a block with Shift (`camera-smooth`), new 2026-11-10

- Placing Farms with **Shift** held: after each Farm the building ghost jumps to the next free plot touching the Farms you just placed, so you can click again without moving the mouse.
- The plots fill a square block around the first Farm of the row (the 8 plots around it, then the next ring), not a long strip; the nearest free plot comes first, and among equals the one next to the last Farm.
- Only plots where the game would let the Farm stand are offered (your territory, the city's Farms, no trees or buildings). When none is free, the ghost stays with the mouse.
- Moving the mouse takes the ghost back; letting go of Shift ends it as before.
- `farm_snap=0` under `[qol]` in `ron_camera.ini` turns it off.

## Fix: the AI assistant and the Cooperative AI really stop (`camera-smooth`), new 2026-11-10

- **NO CONTROL now unticks every box**, as FULL CONTROL ticks them all. The boxes are what the computer does: unticking the last one is NO CONTROL, ticking one while it is off lets the computer do that one (CUSTOM).
- **The game's Cooperative AI** (the setup box) now shows in the panel as the computer playing (CUSTOM or FULL CONTROL), and **NO CONTROL turns it off**: the computer no longer moves your units, takes them into its armies, builds or buys for you, and the units you make from then on are yours alone. Before, the panel read NO CONTROL while the Cooperative AI went on moving units in the background.
- **Control groups always work while the computer plays:** the game made the number keys select the computer's armies instead, so Ctrl+number did not seem to work until you switched it off and on again.
- Units you put in a control group count as given an order: the computer leaves them alone, and they are not handed back to it after standing idle.

## More than 128 in a selection or a control group (`camera-smooth`), new 2026-11-10

- **Select All**, a drag box, a double-click and the **control groups** (Ctrl+number) are no longer cut off when the selection is full. The game holds at most 128 in a selection, and the soldiers of a squad count one by one there, so it stopped at about 80 units. What does not fit is now kept by the mod: those units get the selection circle and every order you give the selection (move, attack, gather, garrison, patrol, stance, formation, halt, ...), sent in batches of about 100 right after the order to the others.
- A control group made with more than 128 brings all of its units back when you press its number.
- Crafts (a General's or a Spy's abilities) and Build stay with the units in the selection panel, so a second General does not cast too and a second building site is not laid and paid for.
- The selection panel still shows the game's part only. Each batch keeps its own formation when moving in formation.
- Only in single player games and campaigns: in multiplayer the game's limit stays.
- `big_selection=0` under `[qol]` in `ron_camera.ini` turns it off.

## Fix: the city districts' builder builds again (`camera-smooth`), new 2026-11-09

- The District Builder who comes out of the city stood idle next to a new district's site, and a Citizen had to build it. He now builds it himself, then the next site, as described.

## Quicksave with Ctrl+S (`camera-smooth`), new 2026-11-09

- **Ctrl+S** saves the game at once, without the Save Game window. The save is called "Quicksave" and is in the Load Game list with the other saves; each quicksave replaces the last one. "Game saved (Quicksave)" shows in the message log.
- Only in single player games and in the campaigns: in a Conquer the World battle it is saved as a campaign save, the same as a save from the menu. In multiplayer and in replays Ctrl+S does nothing.
- `quicksave_key` under `[qol]` in `ron_camera.ini` changes the key (for example `F5`, `Ctrl+Q`); empty turns it off.

## Turn units off in the game setup (`camera-smooth`, `ui-theme`), new 2026-11-09

- Beside the Nukes slider in the game setup there is now a **Units** button ("Units: all allowed", or how many are off). It opens a window from the top of the screen, over everything else, in the theme's steel (the setup panels' riveted frame round a brushed-metal plate), with every unit of the game as its picture, in columns, on two pages:
  - **Standard units**: one upgrade line per column (Hoplites to Anti-Tank Missile, Slingers to Assault Infantry, Cataphract to Main Battle Tank, Catapult to Rocket Artillery, the three warship lines, and so on), with the General, Spy and Supply Wagon in a column of their own.
  - **Unique units**: one column per nation, under its flag and name, with every unit only that nation has (Romania's included).
- Point at a picture to see the unit's name under the columns. A unit that is off is darkened and crossed with a red X.
- Click a unit to turn it off: nobody can train it or research it in that game, the computer players neither. Its later upgrades go off with it, since the game only offers an upgrade after the one before (turn off the Light Tank and the Tank and Main Battle Tank go too). Click it again to turn it back on; the units it is upgraded from come back with it.
- The **X** above a column turns the whole column off; on a column that is all off it turns it back on. **Allow all** turns everything back on. A click beside the window, the X in its corner or a right-click closes it.
- The choice is remembered for the next games. Nuclear weapons are not in the list: the Nukes slider has them. Citizens, Scholars, Caravans, Merchants and the transport and trade fleets are not in it either.
- In multiplayer the host chooses for everyone: the other players can open the window and see what is off, greyed. Every player needs this version.
- A saved game keeps its units off when it is loaded in the same session, or when the window still has the same list.
- `unit_bans_option=0` under `[gameplay]` in `ron_camera.ini` removes the button (it needs `no_nukes_option=1`).
- The setup screen's options now sit in the same places in single player and multiplayer: the Nukes slider and the Units button on the first row on the left, Save to Profile to Cooperative AI on the right, and the multiplayer-only boxes (Allow Observer Chat to Require Random Nations) in the free rows. Multiplayer shows Cooperative AI too, greyed: the game has no Cooperative AI there.

## Fix: crops and units no longer show through Romania's buildings (`romanian-buildings`), new 2026-11-09

- Wheat from a farm behind a Romanian building, and units walking behind it, could draw over parts of the building: mostly its left walls, chimneys, church domes and tower tops. Every Romanian building now has a fuller hidden shape behind its picture, so what stands behind it stays hidden behind it. The Smelter, Large City of ages 1-3, Temple of ages 1-3, the age-6 cities and the Modern Age church gained the most.

## The update downloads inside the launcher, with a progress bar (`camera-smooth`, `package`), new 2026-11-08

- Pressing **Update** on the launcher's plate no longer opens a black console window. The launcher downloads the new version itself: the button becomes a progress bar with the percentage and the megabytes so far ("Cancel" when the mouse is on it). When everything has arrived, the plate says it is installing, the launcher closes for a few seconds and the game starts again by itself. If the download fails, the plate says so and the button becomes **Retry**.
- The update no longer downloads the nation music again with the pack. Extras are updated only from the Extras tray, when you choose.
- Works from the version after this one: the update to it still shows the old console window once.

## The jeep scene is back in the main menu, redrawn (`ui-theme`), new 2026-11-08

- The main menu background now plays two scenes in turn, 30 seconds each: the destroyer in the storm and the jeep scene, redrawn. A World War II Willys jeep, headlights on and a helmeted driver at the wheel, bursts out of a shell explosion at night, with clods of earth flying over a muddy field and a ruined building in the dark. The jeep, the explosion and the background drift apart slowly, as in the game's own scene.

## The modern Citizen icon for every people (`citizen-icons`), new 2026-11-08

- In the Industrial, Modern and Information Ages every nation's Citizen icon was the same white European worker. It now has six looks, one per unit style, all with the yellow hard hat and wrench: a European worker; an African worker for the Bantu and Nubians; an East Asian worker for the Chinese, Japanese, Koreans and Mongols; a South Asian worker for the Indians and Persians; a Native North American worker for the Iroquois and Lakota; and an Indigenous Latin American worker for the Aztecs, Inca and Maya.
- The other nations, and the Egyptians and Turks, whose units all use the European style, keep the European worker. Romania shows its Razes icon up to the Industrial Age and the European worker in the Modern and Information Ages.

## Fix: computer players build Bunkers, not plain Towers, in an All Technologies game (`camera-smooth`), new 2026-11-08

- In a game started with "All Technologies" (an Information Age start, for example) your build menu showed the Bunker, but the computer players and the Cooperative AI building for you laid plain Towers: Tower icon and name, 750 hit points, among the Bunkers. The game gave them the newer levels without marking the older ones out of date, as it does when you reach them in a match, so the computer kept choosing the Tower.
- The computer players now build the newest level they have, the one your build menu shows (Bunker, Stockade, Keep; Redoubt, Fortress, Castle for Forts), and the older levels are out of date for them as in a normal game. Towers already standing stay as they are.
- `defense_build_current_level=0` under `[gameplay]` in `ron_camera.ini` gives back the game's way. It changes what gets built, so in multiplayer every player needs the same setting.

## A steel frame round the minimap (`camera-smooth`, `ui-theme`), new 2026-11-08

- With the bottom interface folded, the minimap at the top right now sits in a steel frame in the interface's style: the riveted steel bar of the window frames runs along the four edges of its diamond, with a domed bolt at each corner. The frame follows the diamond's own shape at every minimap size; the minimap sits a little lower to leave room for the top bolt. `minimap_frame=0` under `[ui]` in `ron_camera.ini` turns it off.

## Romania: the communist look stays in the Information Age (`romanian-buildings`), new 2026-11-08

- Romania's Small City, Large City and Major City, and its Auto Plant, now keep their communist look in the Information Age too, the same pictures as in the Modern Age. Before, the Information Age showed the game's own cities and the shared modern Auto Plant among Romania's grey communist buildings. Every Romanian building with a communist look in the Modern Age now has it in both ages.

## Romania: the game's Citizen icon in ages 7-8 (`romania`), new 2026-11-08

- Romanian Citizens keep their Razes icon up to the Atomic Age and show the game's own modern Citizen icon in the Modern and Information Ages. They are still called Razes.

## A new nuclear target sign that closes as the nuke falls (`camera-smooth`, `nuke-fx`), new 2026-11-08

- The sign over a nuke's target is now a plain yellow radiation sign with no rings around it: a disc in the middle and three blades. When the nuke is launched the blades stand far out from the disc, about as far as the blast will reach; they close in on it as the nuke flies and meet it the moment it lands, so you can read how much time is left. The disc stays in place, and the whole sign glows and throbs as before. A dark rim follows its shapes, so it reads on sand and snow too; the dark disc under the old sign is gone.

## The Interceptor has its own icon (`nuke-fx`), new 2026-11-08

- The Interceptor's button on the Missile Silo used the Cruise Missile's picture. It now has its own: a white interceptor missile climbing into a cloudy sky.

## Brighter Auto Plant in age 8 (`modern-buildings`), new 2026-11-08

- The age 8 Auto Plant (the military vehicle plant with solar panels) was much darker than the buildings around it. It is now as bright as the game's own Auto Plant, with its colours unchanged. The dark shadow drawn on the ground at its lower left is gone too, and the building sits a little lower, with its corners on the corners of its grey foundation.

## Command buttons stand out when you can click them (`camera-smooth`), new 2026-11-08

- The command buttons now work like the market's Buy / Sell buttons. A unit, building or command you can give right now is lit and raised: a light edge on its top and left, a shadow on its bottom and right, as if it stood out of the panel. One you cannot give (not enough resources, an ability without craft points, the Airdrop recharging, a button the game disables) is much darker than before and sunk in: a shadow along its top and left edges. Locked ones (wrong age, missing research) are sunk in too and keep their padlock. Research buttons are left as they were (they have their flask). Buttons you cannot afford stay clickable, so you can still queue ahead.
- Under `[qol]` in `ron_camera.ini`: `button_depth=0` keeps only the darker colour, `poor_button_percent` sets how dark an unaffordable button is (40; it was 60 before).

## Arrow keys move the camera at once (`camera-smooth`), new 2026-11-08

- The arrow keys (and numpad 2/4/6/8) start and stop the camera the moment you press or release them. The game reads the keyboard only once per game tick (up to 200 ms, longer at slower game speeds), so the camera used to start late and keep going for a moment after you let go. `instant_keys=0` under `[camera]` in `ron_camera.ini` brings back the game's way.

## Larger health bars (`camera-smooth`), new 2026-11-08

- Every health bar is 75 % larger, wider and taller, at every zoom: units, buildings and walls, the bars of each soldier of a squad, and the bars stacked with them (a spy's or a General's ability bar). The rank insignia beside a unit's bar and the production bar under a building's follow them. `health_bar_percent=175` under `[qol]` in `ron_camera.ini` sets the size (100 = the game's).

## Folded interface: panel for everything, starts folded; unfolded minimap fixed (`camera-smooth`, `ui-theme`), new 2026-11-08

- **Every match starts with the bottom interface folded** (the new look), loaded games too. The arrow button at the bottom right of the screen (the narrow grey bar) unfolds it as in the game. `start_folded=0` under `[ui]` in `ron_camera.ini` keeps the game's own behaviour (it remembers your last choice).
- With the interface folded, selecting something that is not yours showed nothing at the bottom of the screen: an oil patch, a mine, a forest, ruins, an animal, an enemy or allied unit or building. Its panel now shows there (name, picture, "Basic Resource", "Explore here to gain a bonus"...) as in the unfolded interface, with the empty command buttons beside it.
- **Fix:** after folding and unfolding once, the unfolded interface had an empty hole where the minimap belongs. The minimap is back in its frame, in the middle of the bar, every time (without the size button, which is only for the folded interface).
- **Fix: no more thin pink lines** between the stone pieces of the interface's top edge (and the edges of a few other pieces). The game's interface picture keeps its pieces apart with magenta lines, which leaked into the edges when the interface is drawn enlarged; they now take the colour of the piece beside them.

## Fix: the log no longer fills up when two nukes go off together (`camera-smooth`), new 2026-11-05

- With two nuclear blasts lit at the same time, `ron_camera.log` wrote the "nuke flash" line for both on every frame (hundreds of lines a second). Each blast is now named once.
- Tip for laptops with two graphics cards: if the game gets slow late in big matches, check that Windows runs it on the fast card (Settings > System > Display > Graphics > Rise of Nations > High performance). On the integrated card the game can be several times slower.

## Extras tray in the launcher (`camera-smooth`, `package`), new 2026-11-05

- The pack's zip now holds only what every player needs (about 35 MB instead of 255 MB). Optional content comes from the launcher's new **Extras** tray, so you download only what you want. The first extra is the **nation music** (54 tracks, about 220 MB); more will follow (bigger ones like upscaled videos).
- On the launcher's left edge, beside the buttons, sits the front of a tray, like a CD drive's: EXTRAS up its face, a light, how many extras you have of how many there are (for example 1/3) and a red badge with the number of news (extras you have not seen yet, and installed ones with a newer version out). Click it: the tray unlatches and slides out from behind the launcher. Click it again to slide it back in. If there is no room on the screen at the left, the launcher moves right first.
- Each extra has a button that shows its state: **Download** (dim, not installed), **Installed** (lit, green light; it says **Remove** when the mouse is on it), **Update** (amber, a newer version is out: click to update), the download's progress in percent (**Cancel** when the mouse is on it), **Installing...**, and **Retry** with "It did not work" when something failed. A new extra has a NEW tag the first time you see it.
- The download is checked (SHA-256) before anything is installed, and the files go in the same way as the pack's: a game file they replace is kept and put back on Remove or uninstall.
- Installed extras stay when you install a new version of the pack by hand, and the launcher's Update also updates the extras you have (only those).
- If you already had the music from an earlier version, it is still there; the tray offers it once as an Update (its list of tracks moved into the extra).
- `tray=0` under `[extras]` in `ron_camera.ini` hides the tray.

## Larger game setup screen (`ui-theme`, `camera-smooth`), new 2026-11-05

- Every row of the game setup screen is a quarter taller: the players' rows (Name, Nation, Color, Difficulty, Team, and "Other Nations"), the game rules on the right (Teams & Allies, Map Style... and Start With, Resources...), the check boxes and the Nukes slider. The text and the nation flags grow with them. The rules panel grows downwards to make room; in multiplayer the chat starts a little lower.
- The drop-down boxes are a lighter grey, so the screen is less dark.
- The text of the Name, Nation and Difficulty boxes is centred, the flag next to the nation's name.
- In `ron_camera.ini` under `[qol]`: `setup_centre=0` puts the text back on the left, `setup_font_percent` sets the text size (125; 100 = the game's).

## Fix: towers and forts get dearer again with every one you own (`camera-smooth`), new 2026-11-05

- Every Tower, Keep, Stockade or Bunker you own adds 10 Wealth and 25 Metal to the price of the next one, as the game intends. In some games (an Information Age start, for example) the price never rose: the game counted the towers under one level of the line and priced the next level, so every tower cost the base 30 Wealth and 50 Metal, and the computer players covered the map with them.
- Now all four levels count together, and the same for Forts, Castles, Fortresses and Redoubts. Nation bonuses on these prices (Korean, Roman, Indian) work as before.
- `defense_ramp_whole_line=0` under `[gameplay]` in `ron_camera.ini` gives back the game's count. It changes the price you pay, so in multiplayer every player needs the same setting.

## Crash reports (`camera-smooth`), new 2026-11-05

- When the game crashes, a panel opens: "Rise of Nations closed because of an error". Write what you were doing (optional), and your Discord name if you want to be asked more, then press **Send report**. The report goes to the RON Revived Discord, where the cause can be found and fixed.
- What it sends: where the game failed, the end of the mod's log, your Windows version and memory, and a small memory dump of the game (a checkbox leaves it out). Your Windows user name is taken out first. **See the report** shows the text before you send it.
- The reports stay in the game's `crash` folder (the newest ten).
- The panel speaks the game's language (all ten).
- `crash_reports=0` under `[debug]` in `ron_camera.ini` turns it off. `crash_test=1` crashes the game on purpose when PLAY is pressed, to try the panel.

## Tank Traps (`entrenchment`, `camera-smooth`), new 2026-11-05

- A new building: **Tank Traps**, steel anti-tank obstacles on a single tile. Citizens build them from the **military** build menu (next to the Lookout), in the Modern and Information Ages (ages 7 and 8).
- Nothing gets through them, your own units neither: an enemy has to destroy them to pass. Lay them in a line to close a gap or a bridge, and leave your army a way out.
- 20 Metal (+5 for every one you own), 1200 hit points, armor 8 (as a Redoubt: rifles barely scratch them, tanks, artillery and aircraft break them), no weapon, built in about 13 seconds by one Citizen. They stay yours when the city nearby is captured, like Towers.
- The look: a steel hedgehog wrapped in barbed wire.
- Computer players do not build them (they would close in their own armies).
- They take the place of the scenario-only Washington Monument, so scenarios that use the monument show Tank Traps instead. In multiplayer every player needs the same mod files.

## Cities repair their own buildings (`camera-smooth`, `ui-theme`, `translations`), new 2026-11-05

- A **Large City** or **Major City** repairs the damaged buildings of its own city by itself, one at a time, the most damaged first. The buildings of a city are the ones it got when they were placed (inside its circle, nearest city). Wonders within the city's circle are repaired too. Other cities are not: they heal by themselves, as before.
- It pays the way a Citizen does: every hit point back costs its share of the building's price. Without the resources it waits.
- Speed: a Large City repairs as fast as one Citizen, a Major City as two (the game's rule for a second Citizen: half as much again). Koreans repair faster, as with Citizens.
- A building is repaired only after 45 seconds without being hit (`city_repair_wait` under `[gameplay]` in `ron_camera.ini`).
- While a city repairs, small pliers float over the building, squeezing and throwing sparks, for every player who can see it.
- The city's panel has a **Repair** button that turns it off and on for the selected cities. On by default, computer players included. The button is darker while it is off; its tooltip says what the city is doing. The setting is kept in saved games.
- Safe in multiplayer (the switch is the game's own command), as long as every player has this version. `city_repair=0` under `[gameplay]` turns it off for good.

## Nukes slider in the game setup (`camera-smooth`, `ui-theme`, `nuke-fx`), new 2026-11-05

The No Nukes check box is now a slider, **Nukes**, that sets how many nuclear weapons may go off in the game:
- **Far left, None:** the old No Nukes. Nobody can build Nuclear Missiles, ICBMs or Interceptors, computer players included; Cruise Missiles stay.
- **1 to 250:** the Armageddon clock starts at that number, whatever the number of nations, teams or the game style. Once that many nuclear weapons (Nuclear Missiles and ICBMs) have gone off or are in the air, nobody can launch another one, computer players included. A nuke that would still go over the number (several launched with one order) does not leave: it stays in its silo, not lost. The steps get wider to the right: one by one up to 10, then by 2 up to 30, by 5 up to 60, by 10 up to 100, by 20 up to 200, then 225 and 250.
- **Far right, Unlimited:** no Armageddon clock. It never shows on the screen and never runs out.
- Untouched, or after a **right-click** on it, the game's own rule stays: 4, plus 1 per nation and 2 per team.
- Drag the knob or click anywhere on the track. The label shows the value ("Nukes: 25"); the tooltip explains it all. Translated into all ten languages.
- Multiplayer: only the host can move it; the other players see the host's choice, greyed. Safe in multiplayer, as long as every player has this version of the mod. `no_nukes_option=0` under `[gameplay]` in `ron_camera.ini` removes it.
- The choice is kept in saved games.

## Interceptors: faster, a lighter airburst, their own sounds (`camera-smooth`, `nuke-fx`), new 2026-11-05

- The Interceptor leaves its silo almost at once (a quarter of a second instead of a second) and flies 4 times as fast as the nuke (was 2.5), so it meets the nuke sooner and higher up.
- Where they meet: a smaller fireball in the air and a short, faint light over the screen, as if the hit happened far up; a little of it shows even when it happens off your screen (`interception_burst_size` and `interception_flash_percent` under `[nuke]` in `ron_camera.ini`).
- Two new sounds, for the two players concerned (the silo's owner and the nuke's): the Interceptor's fast launch, and the hit in the air (it replaces the game's Missile Shield sound).
- `interceptor_speed_percent=250`, the old default that earlier installs keep in their `ron_camera.ini`, now counts as 400, so everyone in a multiplayer game has the same speed.

## Selected unit: rank, experience and kills in the panel (`camera-smooth`), new 2026-11-02

With one unit selected, the right half of the selection panel, empty until now, shows:
- the rank's insignia and name, and the unit's kills (the skull) on the right;
- the level (Lv 3) and a large experience bar with how far the unit is to the next rank, in per cent;
- every bonus the rank gives now, one per line (attack, hit points, sight and range, attack rate, damage taken, healing).

The small yellow bar on the unit's portrait is gone; units in a group still have theirs.

**Fix (2026-11-05):** selecting a single unit or building (a Citizen, a Fort) could close the game at once. That no longer happens.

## Fix: the selection panel no longer slips under the screen (`camera-smooth`), new 2026-11-02

With the bottom interface folded, after selecting a group, the panel of a single unit or building stayed as low as the group's and its last rows of stats went under the screen's edge (worst for forts). It now goes back up every time.

## Fix: Romania's Modern Senate sits on its foundation (`romanian-buildings`), new 2026-11-02

It stood too far back and to the left, and its back wall reached past its paved foundation. It is now moved right and down, its back wall along the foundation's edge.

## Fix: tooltips no longer hide under the round buttons (`camera-smooth`), new 2026-11-02

The help box over the bottom interface used to open where the row of round buttons (Military, Army, AI assistant...) stands, and the buttons covered its text. It now opens just above them.

## Fix: crops no longer show through Romania's Modern Small City (`romanian-buildings`), new 2026-11-02

The wheat of a farm next to Romania's Small City in the Modern Age showed through the left walls of its blocks. Its depth outline now covers those walls.

## Russians: more of their own buildings in ages 1-3 (`russian-buildings`), new 2026-11-01

Three of the game's Greco-Roman buildings now come in the dark Russian log style of the other Russian buildings:
- **Senate:** a wooden terem with stepped steep roofs, a carved balcony and a covered porch.
- **University:** a carved log house with an onion-shaped cupola round the courtyard, the lectern and the two benches exactly where the game's own stand.
- **Small City:** a log tower house and thatched log houses round a campfire with dark wooden benches.
- **Stable:** the same layout in logs and wooden shingles, so its animation still fits.
- **One tone:** every Russian building of ages 1-3 now has the same warm brown as the new Stable, including the game's own Granary, Lumber Mill, Smelter, Siege Factory, Dock, Mine, Woodcutter and Lookout (for the Russians only). The Russian buildings have no ground shadows any more, and the Small City, Large City and Stable sit better on their ground.
- **Church (ages 1-3):** about 40 % bigger. It was already as tall as the game allows a building picture to be, so the mod now lets a nation's building art be bigger than the game's own and puts it back in its place (`nation_art.ini`: `<file>, dx, dy`).

## French: their own buildings in ages 1-3 (`french-buildings`), new 2026-10-31

The French buildings of the Ancient, Classical and Medieval Ages are being redone one at a time. The British, Germans and Dutch keep the old shared ones.
- **Temple:** a small Romanesque village church with a square bell tower and a round apse, a little lighter than before. It stands alone on its ground, without the churchyard wall and graves of the shared church, and it is smaller, so it no longer spills past its footprint. The tower band and the door are in the player's colour.
- **Small City:** six half-timbered houses round a well. The chapel is now an ordinary house (the church is only the Temple), and the smoke, hay, carts and campfire are gone. It is smaller and sits on its ground like the game's own.
- **Large City:** the same half-timbered town round a market square, with the church at the back turned into tall town houses (the church is only the Temple). No dark patch in the square, and a player-colour band on the big house at the back.
- **Library:** the same cloister library, lighter and smaller, so it now sits on its ground instead of reaching past the back and sides of it.
- **Market:** a new, bigger half-timbered market hall on posts with a cross-gabled roof, all the goods and stalls round and under it, in the tone of the others, with clean outlines.
- **Barracks:** the same camp with all its props (campfire, benches, weapon racks, target), much lighter, in the tone of the other French buildings.
- **Senate:** a half-timbered town hall over a stone ground floor with doors and arched windows, and a stone bell tower with a spire (2026-11-01).
- **University:** an L-shaped college of stone and half-timber with a round stair tower, round the same courtyard with the lectern and the two benches (2026-11-01).
- **Fort:** the same keep, three watchtowers and palisade, with dark mossy shingle roofs (2026-11-01).
- **Tower:** an eight-sided half-timbered watchtower with a mossy shingle roof and sharpened stakes (2026-11-01).
- **Library (new):** two half-timbered wings on stone round a paved court, curved steps and a lectern, red cloths on the posts; it no longer looks like the University (2026-11-01).
- **Siege Factory:** stone and half-timber with a shingle roof, the kilns and the furnace where they were, so its animation still fits (2026-11-01).
- The church is a little bigger than at first, and the Library too.
- **Shadows:** none has a ground shadow any more. The old ones were hard grey slabs (the church tower's was the worst).

## Fix: no more crash with many buildings selected (`camera-smooth`), new 2026-11-02

Selecting many buildings at once whose rally lines were long crashed the game: all those dots went to the graphics card as one batch, more than it takes in one go. The dots now go in several batches when there are that many. The heal rings and the drop zone of the paratroopers are drawn the same way.

## Minimap in the top right corner, in three sizes (`camera-smooth`, `ui-theme`), new 2026-11-01

With the bottom interface folded (its arrow button), the minimap moves from the bottom right to the top right corner of the screen, where it is out of the way of your units. A round button fixed in the very corner of the screen, in the empty triangle beside the map, switches it between small (as before), medium (20 % larger) and large (another 20 %); the size you pick is kept for the next games. The game's clocks in the top right (the Armageddon count, wonders, time limit, Survival) and the pictures of the wonders being built line up a little under the minimap, whatever its size. Unfolded, the minimap is back in its frame as in the game. `minimap_top_right=0` under `[ui]` in `ron_camera.ini` keeps it at the bottom right.

- **No empty boxes in the top right corner:** with the interface folded, the game hides its help and menu buttons but still drew their frame, two empty boxes. They are gone (the buttons come back with the unfolded interface).
- **The row of round buttons** (military buildings, army, airbases, docks, assistant) moves to the left end, at the same height.
- **Selection panel with the interface folded:** selecting a unit or a building shows the full panel of the unfolded interface next to the command buttons (name, hit points, attack, armor, range, the production queue) instead of the game's one-line strip. Several units or buildings show a panel six cells wide with one picture per kind and how many of it are selected, on the same empty cells as the command buttons, over the interface's own stone background, a little apart from them, in the selection panel's frame with the command panel's stone frieze on top and its upright stone on the right; its cells end on the bottom edge of the screen like the command buttons, so the panel stands a little lower than them. `folded_selection_panel=0` under `[ui]` brings back the strip.
- **New kills skull:** a clean painted skull and crossbones replaces the game's tiny pixel one everywhere the kills show (a single unit's stats, a group, the army's kills row).
- **Kills of an army, large:** with soldiers selected, the top row of the selection panel, a little lower than the rows of units, is kept for a new, larger skull and the kills of the whole selection, as large as the row takes them, for every multiple selection (before, a small skull showed only for named groups); the units start on the row under it.

## Fix: no more streaks across the ground at the farthest zoom (`camera-smooth`), new 2026-11-01

On a map where one ground covers most of the view (a desert, for example), zooming all the way out could fill the screen with long black and sand-coloured streaks, also past the map's edge, and the game stuttered while they were there; they could stay after zooming back in. The game can only draw so many tiles of one ground at once. The farthest zoom now stops just before that limit where it would be passed (on varied maps it goes all the way, as before), and streaks left from before are cleared.

## Fix: the game no longer freezes mid-match over a unit's animation (`camera-smooth`), new 2026-10-31

A match could freeze for good, with the picture and sound stopped, when a unit got a walking animation that lasts no time at all. It was seen with a computer player's Artillery in the Industrial Age. The game kept starting that animation over and over and never got to the next frame. This is a bug in the game itself.
- Such an animation now gets a short length, so the unit just walks on and the game goes on.
- The first few times it happens, `ron_camera.log` names the unit, which helps find what caused it.
- It is always on and changes nothing in a game that would not have frozen anyway, so it is safe in multiplayer.

## No Nukes option in the game setup (`camera-smooth`, `ui-theme`, `nuke-fx`), new 2026-10-31

Since 2026-11-05 the check box is the far left of the Nukes slider (above).

A new check box in the game setup, **No Nukes**, next to Turbo Resources, No Unique Units and the others. Ticked, nobody can build Nuclear Missiles or ICBMs in that game, computer players included. The Interceptors of the Missile Silo go too, since there is nothing left for them to shoot down. Cruise Missiles stay.
- Single player: the box is at the top of the left column. Multiplayer: it is the last box of the right column, and only the host can tick it; the other players see the host's choice.
- It has a tooltip like the other boxes and is translated into all ten languages.
- Safe in multiplayer, as long as every player has this version of the mod. `no_nukes_option=0` under `[gameplay]` in `ron_camera.ini` removes the box.

## Fix: the game no longer hangs at start (`camera-smooth`), new 2026-10-30

Since late October 2026 many players find that after Play nothing opens: the launcher closes, no window comes, and `riseofnations.exe` stays in Task Manager (or there is a black screen and the game closes). It happens on about every second start. The game was not changed; it started with the Windows updates of 13 October 2026 and the Steam update of 21 October 2026. At start, Steam's part of the game and the game's online part (PlayFab) both reach for the same piece of Windows (its random number generator) at the same moment, and each waits for the other forever.
- With the mod, that piece of Windows is made ready at the very start of the game, before the other two run, so neither waits any more.
- Nothing else changes and there is nothing to set. Safe in multiplayer.
- Without the mod, players can end `riseofnations.exe` in Task Manager and start again, or start `riseofnations.exe` from the game folder directly.

## Sharper, richer picture (`camera-smooth`), new 2026-10-29

The map, units and buildings look clearer and less washed out. The interface stays exactly as it was.
- **Sharper:** fine detail (bricks, roofs, logs, trees, terrain) stands out more, without bright outlines around things.
- **Richer colour:** slightly deeper blacks, a little more contrast, more colour in the dull tones (strong colours are left alone), and the corners of the map a little darker.
- **Anti-aliasing works again with bloom off.** The game's own smoothing of jagged edges did nothing in that case.
- **Compare it yourself:** in a game, Ctrl+F10 shows the game's own picture on the left half of the screen and the new one on the right. Ctrl+F9 turns the new look off and on.
- Every part can be changed or turned off under `[graphics]` in `ron_camera.ini` (`postfx=0` turns it all off). Sharpening needs the game's Anti-Aliasing on (the default).
- Only the picture changes, so it is safe in multiplayer.
- **Better water:** buildings, trees and units on the shore are seen in seas, lakes and rivers, sunlight sparkles on the waves, the sky's light shows on them, and the small waves are a little livelier. The game had reflections, but they only looked half a tile away and almost never showed anything. The reflections need the game's high water quality. Ctrl+F9 / Ctrl+F10 switch the water too; `water=0` under `[graphics]` keeps the game's own water.
- Also fixed: the launcher's button sounds. The settings file shipped with broken paths for them.

## Music of every nation (`nation-music`), new 2026-10-29

Every one of the 25 nations has music of its own, chosen for its history and culture, which plays among the game's tracks when you play that nation. Peaceful pieces for building up, battle pieces for the fighting, each in the ages it belongs to.
- **54 pieces, 2 to 4 per nation.** Some are real old tunes (all public domain): *Sakura Sakura*, *Greensleeves*, *Oh! Susanna*, *Yankee Doodle*, *Jasmine Flower*, *Au clair de la lune*, the opening of *La Marseillaise*, Beethoven's *Ode to Joy*, *Korobeiniki*. The rest are written for the mod in each culture's own style and scales.
- **Each culture's own instruments:** the nai and tambal of a Romanian doina, koto and shakuhachi, taiko drums, the bagpipes of the Highland regiments, the Janissary mehter band, the sitar and tabla of a raga, the santur, the mbira, Andean pan pipes, Aztec clay flutes and log drums, the Mongol horse-head fiddle, the Roman water organ, and more.
- **By age:** for example, Romania has a doina in the first ages, a lautari hora from the Gunpowder Age and a belle-epoque waltz from the Industrial Age. Japan has gagaku court music at the start, *Sakura* in the middle ages and taiko drums in battle.
- Each of your nation's pieces is listed three times among the game's tracks, so in peace about one track in seven is your nation's (more where an age has two), and in battle about one in three; the game's own music plays the rest.
- With Romania's Enescu recordings added by hand, Romania hears both.
- **Fix (2026-11-05):** big games (about 6-8 players) stopped at the start on "art\logchop5.bh3 does not exist for Transform Batch Transform" (or another model, such as `hammer5.bh3` or `workbench5_weld.bh3`), although the file is there. The game opens every music track of every player's nation at the start, and with the new music that went past the 512 files the game may have open at once, so the next model could not be opened. The limit is now 8192.

## Languages (`translations`), new 2026-10-29

The game in ten languages. Choose under Options > Player > Language, or in the launcher.
- **Language button in the launcher:** a round flag under Workshop. Click it and the other languages open around it like a flower; pick a flag and the game starts in that language. The launcher's own buttons and the Workshop agreement change to it at once. `language_button=0` under `[ui]` removes it. Its frame is a riveted steel ring, and the button panel behind it now has a fine dark grain instead of the fog.
- **Five new languages:** Russian, Brazilian Portuguese, Polish, Turkish and Romanian, next to the game's English, German, Spanish, French and Italian.
  - Translated: every menu, button, name, message, scenario text, tooltip and encyclopedia page.
  - Short texts were measured in the game's own font and shortened where they were wider than the game's own translations.
- **The mods in every language:** the new buildings and units, the nation powers, the assistant panel, the unit ranks, the button tooltips and the PLAY button show in your language.
  - Before, playing in German, Spanish, French or Italian lost most of the mods' texts, and some names showed the wrong building.
- To turn the five new languages off: `more_languages=0` under `[ui]` in `ron_camera.ini`.

## Survival mode (`survival`), new 2026-10-29

A new way to play: hold out for 30 minutes. Since 2026-11-10 it has its own button in the Solo menu, its own map and a wave bar (above). The computer player's cities and units are removed at the start, so the whole map is yours; instead it sends waves of soldiers that march on your capital.
- **Clocks** at the top right: the time left to survive and the time to the next wave (yellow for a boss wave, red in the last 10 seconds).
- The first wave comes at 2:00, then one every 80 seconds, down to 45 seconds by the end; every wave is bigger than the one before. Infantry, archers, cavalry, and siege from the sixth wave on.
- The waves are **1 or 2 ages behind you**: 2 ages on Easiest to Moderate, 1 age on Hard to Toughest (the computer player's difficulty). Age up and they come stronger too.
- **Boss waves** at 5, 10, 15, 20 and 25 minutes: a General with heavy cavalry (tanks later), siege and heavy infantry.
- The waves take no attrition in your territory. You win when the 30 minutes are up.

## Smooth Camera and interface (`winmm.dll`, settings in `ron_camera.ini`)

Fixed 2026-10-28:
- **Rank insignia beside the health bar:** the selected or hovered unit and building show the rank's insignia, the same picture as on the portrait. It used to be the old small coloured bars. The insignia is 18 to 30 pixels tall, larger when zoomed in (`insignia_size` under `[ranks]`). Without the interface theme the bars stay.
- `command_panel=0` no longer switches off the hotkeys on the buttons, the rank insignia, the Airdrop cooldown and charges, the capital star, the assistant panel and the round buttons; it turns off only the mixed selections and the padlock / darker looks.
- Changing to a resolution that needs no interface scaling no longer keeps the old scale.
- `hud_scale` works with the interface at 100 % too. Before, the digits stayed small inside the larger panel.
- `ron_camera.log` starts new with every game; the previous game's log is kept as `ron_camera.old.log`. Before, it grew without end. Repeated lines are gone: per-frame nation art lines when many sprites were on screen, and one line per Roman rule. The zoom trace (`ron_zoom_trace.csv`) and the viewport lines are off, under `[debug]` in `ron_camera.ini`.

### Camera
- **Smooth scrolling:** diagonal scrolling is no longer 1.41x faster than straight scrolling. The keyboard and the screen edge no longer add up to double speed. Acceleration and braking are optional.
- **Smooth zoom:** the camera glides between zoom levels instead of jumping, and the tilt follows gradually. Zoom keeps the point under the cursor fixed (or uses the screen centre, or the game's old behaviour).
- **Smooth camera jumps** (for example to an event) instead of instant cuts.
- **One more zoom step each way:** you can zoom out one step farther than before (the wider view the game keeps for replays) and in one step closer than the closest level (`extra_zoom` in `ron_camera.ini`). Up close, buildings are enlarged and look a little soft.
- **No vibrating buildings:** buildings stay steady against the terrain while you zoom or scroll (sub-pixel placement).
  - 2026-10-28, confirmed in game 2026-10-29: the nations' new building art still shook while zooming (it keeps the original picture size around smaller art, and the game placed that empty margin to whole pixels). Now it stays steady too.
- **No black ground after a zoom:** zooming out no longer leaves part of the map (sometimes half the screen) as black ground until the camera is moved.
- **No black ground after a minimap jump** (2026-10-30): clicking the minimap sometimes left part of the new view as black, speckled ground with only the buildings and units drawn on it. The ground is now redrawn for a moment after every camera move, however the camera got there.
- **Stuck camera fix:** the camera no longer keeps scrolling in one direction after Alt+Tab.
- **Smooth sprites while moving:** buildings use smooth filtering while you zoom or scroll (no shimmering pixels) and are sharp again at rest (`zoom_linear_filter`).
- The Conquer the World campaign map keeps the game's own camera; its battles use the new one.
- **Safe after game updates:** the mod checks the game version and every place it changes; any part that does not match turns itself off (see `ron_camera.log`) and the rest keeps working.

### Graphics
- **Smooth fog of war:** soft, round edges instead of the jagged tile grid, with a gradual fade at the edge of vision.
- **No teeth on the fog edge at the closest zoom** (new 2026-10-29): close up, the edge of vision still showed small dark teeth, one per row of the ground's grid. Every row of the ground now gets exactly the same blur, and the fade at the edge of vision is wider (new defaults `fog_blur=1.0`, `fog_edge=1.0`). If you kept your own `ron_camera.ini` on an update, set those two values under `[fog]` yourself.
- **No sawtooth on the fog edge** (new 2026-10-26): the edge of vision no longer shows small dark teeth along the ground's triangles. The mod serves the game a version of its fog fade with the sharp step at the edge of vision replaced by an even fade (made at start-up in the Windows temp folder; the game's own file is not changed).
- **Smooth unit turning** (new 2026-10-26, confirmed in game 2026-10-29): units, ships and their shadows are drawn at their exact facing; the game had only 256 directions, so slow turns moved in visible steps. Aircraft now turn smoothly between game ticks instead of snapping from angle to angle. Drawing only, so it does not affect multiplayer (`smooth_facing` in `ron_camera.ini`).
- **Shadows with interface scaling:** with the interface scaled up, shadows could stretch into long dark streaks across the ground.
- **The nuclear flash only when you look at it** (new 2026-10-27): a nuclear blast no longer turns your whole screen white and grey wherever it is on the map. The flash is full when the blast is near the middle of your screen and fades out with distance (settings in `[nuke]` of `ron_camera.ini`; `flash_by_distance=0` brings back the game's flash). Drawing only, so it does not affect multiplayer.
- **Nuclear target sign, longer scorch** (new 2026-10-27, needs the nuke-fx files): while a nuke flies, everyone sees the radiation sign on the ground over its target, shrinking towards the centre and turning slowly. The scorched earth stays 3 times as long and the fires smoke 3 times as long. Drawing only.
- **Interface scaling** for high-resolution screens. By default it follows the Windows display scale, and it is still drawn at full resolution. Best in borderless window at the screen's native resolution; the mouse cursor keeps its original size.

### Gameplay rules
These change the game itself: in multiplayer every player needs the same settings.
- **The Senator's Bribe has a key** (new 2026-10-28): the same key as the Spy's Bribe (`senator_bribe_hotkey`).
- **Bribing a moving unit works** (fix, new 2026-10-30): a Senator or Spy sent to bribe a unit that walks away no longer runs to where the unit was, stops, looks around and runs off again (sometimes back the way it came) until it is killed. It now follows the unit: it bribes as soon as the unit is in range, and changes its way when the unit walks off. The same goes for every ability cast on a unit (`cast_follow` in `ron_camera.ini`; 0 = the game's behaviour). It changes the game itself: every player of a multiplayer game needs the same setting. The game's own rule stays: the Senator can bribe only in your or your allies' territory, and gives up with a message when the unit leaves it.
- **Escorts and followers keep up** (fix, new 2026-10-30): units told to guard (escort) a unit that moves walked to where it had been and changed their way only every 4 seconds, so they ran, stopped and ran again behind it. The game does the same with the rest of an army when it attacks with artillery or siege: the other units guard the guns, so such armies moved in fits and starts. Units told to follow a unit changed their way only once it was 12 tiles off. Now both change their way within about a second once the unit walks off (`escort_follow` in `ron_camera.ini`; 0 = the game's behaviour). Nothing changes while the unit stands still. Every player of a multiplayer game needs the same setting.
- **Missile defence and radiation** (new 2026-10-27, needs the nuke-fx files): Missile Silos train Interceptors, up to 2 each; a silo fires one by itself at an enemy nuke within 40 tiles that it can still catch, and a nuke shot down does no damage and does not count for Armageddon. A blast that goes off leaves a minute of radiation under a green haze: every unit inside loses hit points over time. A new air-raid siren when an enemy launches a nuke. The Interceptor is seen flying from the silo and blows up in the air with the nuke; computer players build Interceptors too; Interceptors and radiation are kept in saved games. Since 2026-10-29 a selected Missile Silo of yours shows its interception range (40 tiles) as a ring in your colour on the ground of the map; the minimap keeps the attack range.
- **Computer players keep Generals and Supply Wagons in proportion to their army** (new 2026-10-27): instead of a crowd of Generals and Supply Wagons and no soldiers, a computer player (and the assistant playing for you) gets one General per 20 land army units (infantry, cavalry, tanks, guns; a squad counts once), at most 3, and one Supply Wagon per 15, at most 4, and at most one new one of each kind a minute (`ai_army_per_general`, `ai_max_generals`, `ai_army_per_supply_wagon`, `ai_max_supply_wagons`, `ai_support_seconds` in `ron_camera.ini`; max -1 = the game's rule). Every player of a multiplayer game needs the same values.
- **The dead stay longer** (new 2026-10-27, a minute since 2026-11-05): fallen soldiers, tank and vehicle wrecks and the burn or blood marks under them stay on the ground about a minute after they fall instead of about 9 seconds (`corpse_time_percent` in `ron_camera.ini`, 667 = about a minute, 100 = the game's time, 1000 at most). The map now keeps up to 400 dead at once instead of the game's 100 (`corpse_limit`), so even an 8-player battle does not wipe its dead early; past that the oldest go first, so memory never fills up. The death animations play at their usual speed. Tank wrecks block the ground for that long too, as they do in the game for their shorter time.
- **No Armageddon defeat:** when the Armageddon clock runs out, the game no longer ends with everyone defeated. Instead, from then on nobody can build or launch nuclear weapons (`no_armageddon` in `ron_camera.ini`).
- **Fishing boats retry quickly:** a fishing boat left without a fish (two boats picked the same one, or its way is blocked by ships) looks for another free fish after a few seconds instead of about a minute, so the idle alert stops ringing (`fishing_retry` in `ron_camera.ini`).
- **No more decoys** (new 2026-10-27, needs the general-heal files): Generals no longer have the Create Decoys ability, for anyone, computer players included. It replaces the earlier rule that decoys died from the first hit (`fragile_decoys`, removed). Decoys a scenario places still work and do not count on the kill counter.
- **Veteran ranks** (new 2026-10-26): units gain experience from the damage they deal, more for hurting valuable enemies, and rise through 10 levels (3 tiers of 3, then level 10). A selected unit, or one under the mouse (yours or the enemy's), shows its rank as 1-3 small bronze, silver or gold bars beside its health bar. A squad shares its rank, and units keep it when their line is upgraded and in saved games. Every level gives +4 % attack and +4 % hit points (+36 % at level 9); level 3 +1 line of sight, 5 attacks 10 % faster, 6 takes 10 % less damage, 7 +1 range (ranged units), 8 attacks 20 % faster, 9 heals slowly (1 % a second) after 10 seconds out of combat. Experience needed, in units of the same cost destroyed: 0.5, 1, 2, 3, 4.5, 6, 8, 10.5, 13.5. Computer players' units rank up the same way (`ranks` and `[ranks]` in `ron_camera.ini`).
  - **Rank in the selection panel** (new 2026-10-26): the selected unit's portrait shows its rank insignia (a small shield with 1-3 bronze, silver or gold chevrons) in the top-right corner and an experience bar along the bottom (yellow since 2026-10-27, light blue before; gold at the highest rank). Its tooltip, on the portrait or on the map, names the rank ("Rank: Sergeant (level 3 of 9)"), how far it is to the next one and the bonuses it has now.
  - **Spies and Generals rank up too** (new 2026-10-26): they have no weapon, so they earn experience from their abilities. A Spy's (or the Senator's) successful Bribe counts like destroying the bribed unit; every other ability that costs mana (a General's Decoys, Ambush, Forced March and Entrench, a Spy's Counterintelligence and Informer) gives 35 % of the unit's own cost per full mana bar (`ability_xp_percent`): a General reaches level 1 after 2 abilities and level 9 after about 39. Their attack bonuses do not apply; hit points, sight, damage taken and healing do.
  - **Defensive buildings rank up too** (new 2026-10-26): every building that shoots (Cities, Towers and their later forms, Forts and their later forms, Lookouts and Observation Posts, the air defence guns, the Red Fort and the Forbidden City) gains experience from the damage it deals and gets the same bonuses as units: attack, hit points, sight, attack rate, range, less damage taken (no healing; repair them as usual). Their rank shows beside their health bar, on their portrait and in their tooltip, and is kept in saved games. A building's experience is counted against its own base cost, but at least 150 (`building_min_cost`), so cheap Cities do not shoot up the ranks at once. A captured building starts again from no rank. **Fix (2026-10-27):** shooting at buildings gave no experience (a Fort or the Red Fort firing on an enemy Fort never ranked up): each hit on something with thousands of hit points was worth a fraction that was rounded away. The fractions now add up from hit to hit, for units too.
  - **Level 10 and faster experience** (new 2026-10-27): a tenth rank, Brigadier General, above Colonel: +40 % attack and hit points in all, and damage taken -20 % instead of -10 %. It is reached at 16 units of the same cost destroyed (the most experience a unit can hold). Its insignia is the gold shield with a gold rim and a star over the chevrons; beside the health bar it shows as 4 gold bars. Units and buildings also gain experience 15 % faster (`xp_gain_percent=115` under `[ranks]` in `ron_camera.ini`), so with the new rate level 9 comes at about 11.7 kills of equal value and level 10 at about 13.9. Healing out of combat stays from level 9.
  - **Attack in the selection panel** (fix 2026-10-27): the attack box showed only the base attack (a level 7 Rocket Artillery read 48 while its tooltip said +28 % attack). The rank's bonus was already counted in combat; the box now shows it the way the game shows other bonuses, for example "48+13".
  - **Promotion effect** (new 2026-10-27): when a unit or a defensive building reaches a new rank, its new insignia pops up over its head in a flash of light in the tier's colour, rises while it spins around twice, stops facing you and fades away, with a few sparks flying off it. You see it for every promotion you can see on the map, yours and the other players'. Drawing only, so it does not affect multiplayer (`promotion_effect` and `promotion_effect_size` under `[ranks]` in `ron_camera.ini`: 2 = only your own units, 0 = off).
- **The General's Heal** (new 2026-10-27): Generals get a fifth ability, Heal, on the free button after their other four, with its own icon (a field medic's bag) and the hotkey E (`[heal] hotkey` in `ron_camera.ini`; with no General selected, E does what it did before; the game's hotkey settings do not list it). It costs a full bar of craft points (1000), like the other four. For 5 seconds your units in the General's circle get back 25 % of their hit points in total, a little at a time; the circle moves with the General, and a unit that walks in later gets the rest of the share. Decoys and buildings are not healed, and soldiers of a squad who already died do not come back. While it runs, every unit in the circle stands in bright green rings on the ground, one of them pulsing outwards every second (2026-10-27, not yet seen in game). Since 2026-10-29 (not yet seen in game) it is easier to see: each healed unit also gets a soft green halo where its health bar is and green crosses rising from its head and fading, as long as the heal runs. It is the game's own unfinished "Rally!" ability (its animation and sound were already there). Needs the General's Heal data files (`general-heal`) and `general_heal` / `[heal]` in `ron_camera.ini`. Whether computer players use it is not known yet.
- **Stronger attrition** (new 2026-10-27): units in enemy territory without supply lose hit points 1.5 times as often (a hit every 32 frames instead of 48), so armies deep in enemy land wear down faster and Supply Wagons, and raids on them, matter more in the late game. Supply Wagons, Forage / Supply / Logistics, the Mongol and Russian powers, the Kremlin and the Statue of Liberty work as before on top of it; peace-time border attrition is unchanged (`attrition_percent` in `ron_camera.ini`; 100 = the game's attrition).
- **Aircraft take off together** (new 2026-10-27): an Airbase let one aircraft take off only every 15 frames (about a second), so a strike reached its target one plane at a time and the anti-aircraft guns picked them off one by one. Now they take off almost together, from the strike button and from a right-click alike (`air_launch_frames` in `ron_camera.ini`; 15 = the game's rule).
- **Units make way** (new 2026-10-27): a unit whose way is blocked by one of your idle units (one with no orders) no longer has to squeeze around it or get stuck behind it: the idle unit, with its whole squad, steps a little to the side and the moving unit waits for it to pass. A unit that just stepped aside is left alone for 3 seconds. Dug-in units, units in an entrenchment Nest, decoys, other players' units (allies too), ships and aircraft are never moved (`make_way` in `ron_camera.ini`). It changes the game itself: every player of a multiplayer game needs the same setting.
- **Units walk through each other** (new 2026-10-27): your land units no longer bump into each other, wait and walk around. A unit walks straight through your moving units, and through your standing ones too until the last two tiles of its way, so arriving units do not end up on top of the ones already standing there (those step aside, see above). Your allies' land units are walked through the same way, and their units walk through yours. Enemy units still block, since running into them is how units start fighting them (`pass_through` in `ron_camera.ini`, `pass_clear` and `pass_allies` under `[make_way]`).
- **No formation for mixed groups** (new 2026-10-27): a group of different units (for example Riflemen, Cavalry and Artillery together) no longer moves in a formation that waits for its slowest unit: every unit goes at its own speed and they gather around the target. Generals and Supply Wagons do not count, so an army of one unit type with its General keeps its formation. A group of one unit type moves in formation as before, and the units keep the formation you gave them. Computer players keep their formations (`mixed_no_formation` and `computer_players` under `[formations]`).
- **Compact formations** (new 2026-10-27): Line and the other row formations (Refused, Envelop, the Echelons, Sparse) put a large group in a block about twice as wide as it is deep instead of two very long rows: 100 Riflemen stand in about 7 rows of 15 instead of 2 rows of 50. Up to 5 units still stand in one row; Square, Wedge and Column keep their shapes. For every player, computer players included (`compact` and `width_ratio` under `[formations]`).
- **Assistant settings** (first version, single player): when the computer plays your nation for you, `[assistant]` in `ron_camera.ini` decides what it may buy: economy, military, research, new cities, wonders and the Data Center (off by default). It also sets how much of each resource it must leave you (`reserve`). Changes apply while playing. A new button (Senate icon), in the row of new buttons, opens the AI assistant panel: a checkbox for each kind of purchase and a slider for the reserve (0 to 5000 of each resource). The panel saves to the same settings.
  - **Full control / No control switch and new look** (new 2026-10-27): the panel has a switch at the top. One click on FULL CONTROL ticks every kind and lets the computer play your nation; one click on NO CONTROL takes it back (no more console commands `computer <your name>` / `human <your name>`, which still work). With some kinds unticked while the computer plays, the switch reads CUSTOM. Single player only. The panel is now drawn in the theme's look: a dark steel plate with a riveted frame, round gold checkboxes like the new buttons, a gold slider knob and a close button.
  - **The computer leaves the units you command alone** (new 2026-10-30): while it plays your nation, the computer no longer takes over units you have given an order to. Before, it pulled your soldiers back to its own armies in the middle of a fight, and took the Citizens building your wonder off to build its own buildings. Now a unit (a whole squad) you order stays yours: the computer does not put it in its armies, does not move it and does not send it to build. It still fights back and works like a human player's unit. Once it has stood idle for a minute, the computer may use it again (`keep_ordered_units` and `ordered_units_idle_seconds` under `[assistant]` in `ron_camera.ini`; -1 = never give it back). **Fix 2026-10-31:** it now also works with the game's own **Cooperative AI** check box (a computer player sharing control of your nation while you play). Before, it only worked when the assistant panel had the computer play for you, so with Cooperative AI on, the computer still took your soldiers and Citizens.

### Gameplay helpers
These change only your interface, apart from auto-trade, training from a mixed selection and the Senator's right-click Bribe, which send normal game commands (a multiplayer test is still to be done).
- **The Senator bribes on right-click** (fix, new 2026-10-26): the Senator (patriot) can Bribe like the Spy, but the game gave it only the button: over an enemy unit the cursor stayed the normal one. Now, as with the Spy, the cursor over an enemy unit it can bribe is the bribe cursor and a right-click bribes it. Over enemy spies, citizens and buildings the Senator keeps its normal cursor, since it cannot use the Spy's other abilities (`senator_bribe` in `ron_camera.ini`).
- **Cooperative AI with any number of teams** (new 2026-10-26): with Cooperative AI on, "Teams of 2" (and of 3 / 4) no longer needs exactly 4 (6 / 8) players; the game said "Not enough players for a 'Teams of 2' game" with 8. It now asks what it asks without Cooperative AI: at least 3 (5 / 7) players and no team bigger than 2 (3 / 4) (`coop_ai_any_teams` in `ron_camera.ini`).
- **Play button after loading** (single player): when the game has loaded, the loading bar turns into a PLAY button, with a light sweeping across it and a slow glow, and the game starts only when you click it (or press Enter or Space). You can read the loading screen (players, unique units, rules) for as long as you like (`play_button` in `ron_camera.ini`).
- **Hover highlight:** the unit or building under the mouse gets the same mark as a selected one (the circle under a unit, the rectangle around a building), in white for yours, yellow for allies and red for everyone else (`hover_highlight` in `ron_camera.ini`). Since 2026-10-27, hovering any soldier of a squad marks the whole squad, and the unit's 3D model (every soldier of the squad) also gets an outline in the same colour (`hover_outline`, `hover_outline_px`, `hover_outline_brightness`).
- **Health bars for every soldier of a squad** (new 2026-10-27): a selected or hovered squad shows a small health bar over each soldier, with that soldier's own hit points, so you can see who is about to fall. The game showed one bar over the captain only, and that bar did not show how much health the squad really had left. Drawing only (`squad_health_bars`, `squad_bar_width_percent` in `ron_camera.ini`).
- **Production bar** (new 2026-10-26): a building of yours under the mouse, or selected, that is training a unit or researching shows a thin cyan bar under its health bar with how far the job is (`production_bar` in `ron_camera.ini`). Since 2026-10-27 the icon of the unit or research it is working on shows over the health bar, the same picture as on its button, larger when you zoom in (`production_icon`, `production_icon_size`). Fix (2026-10-27): an idle building kept showing the icon and the bar of the last thing it had made (a Citizen over a city, the last research over a Library); both now show only while something is really being made. Since 2026-10-29 the icon keeps its own shape (as on the button, wider than tall) instead of being squeezed into a square.
- **Announcements at the top** (new 2026-10-27): the big announcements (age reached, city or capital under attack, wonders started or finished, cities captured, nukes, players defeated, the wonder victory timers) no longer cover the middle of the map: they appear at the top centre of the screen, under the one-line messages there (lower while the mini library is open), and when their time is up they leave to the right along the top instead of flying across the map to the bottom-right corner. Which ones show, and for how long, is still set in the game's options (Notifications, Message Duration) (`notices_top` and `notices_leave_top` under `[ui]` in `ron_camera.ini`; `notices_top=0` = in the middle, as in the game). Drawing only.
- **Rally line** (new 2026-10-26): a selected building of yours shows a dotted line on the ground, in your colour, from the building to each of its rally flags (`rally_line` in `ron_camera.ini`).
- **What you are short of** (new 2026-10-26): in the cost tooltips the game already writes a price you cannot pay in red; now it also shows how much is missing, for example "120 (-45)" (`missing_resources` in `ron_camera.ini`).
- **Build grid:** while you place a building, the tiles of your territory around the cursor show green (the building fits) or red (blocked by trees, resources, buildings, water or cliffs).
- **Row of new buttons:** just above the bottom interface at its left end (moved from the right 2026-11-01, where the folded minimap and the score list are), so it no longer covers the selection panel. The Airbases and Docks buttons show only once you have built an Airbase or a Dock. They are round gold medallions (a tower, a plane, an anchor, a gear in laurels) that light up under the mouse and darken when pressed; they need the interface theme, without it they keep the game's square icons. Fix: their tooltips were empty, they now show what each button does.
- **Military buildings button:** selects every Barracks, Stable/Auto Plant, Siege Factory/Factory and Airbase.
- **Army button** (Hoplites icon), back next to it: selects all your military units, like the game's "All Military" key. It had been removed by mistake (2026-10-24) when the round buttons came in; for now it keeps the game's square look, until it has a round medallion of its own.
- **Airbases button:** selects every Airbase. You can then build aircraft from all of them, or use the air commands at the bottom of the command panel to send all their aircraft at once.
- **Strike button on the Airbase** (new 2026-10-27): the Airbase had only Scramble, Repeat and Recall; a strike needed a right-click. It now has a strike button with the Attack icon: click it, then the target, and its aircraft attack it (click the ground to patrol there instead). With several Airbases selected (Airbases button), the aircraft of all of them go. It is the game's own Air Mission command, the one aircraft already had (`airbase_strike` in `ron_camera.ini`).
- **Area strike from the Airbase** (new 2026-10-29): with only Airbases selected, hold the right mouse button and drag. A red circle opens on the ground from where you pressed and grows with the mouse; the enemy targets your aircraft will hit get a red ring. Let go and each ready aircraft flies at one target in the circle, nearest to the centre first. **Bombers** go for the buildings, and for the units only when the circle holds no building. **Fighters** go for the units, and for the buildings only when the circle holds no unit. With more aircraft than targets, several go at the same target. A plain right-click still works as before; a circle with no target sends every aircraft to patrol it. Only targets you can see count. Aircraft still refuelling stay at the base (`airbase_area_strike` in `ron_camera.ini`).
- **Docks button:** selects every Dock (Anchorage and Shipyard in later ages), to build ships from all of them at once.
- **Mixed building selection:** selecting different military buildings together, for example Barracks + Stable, shows the units of all of them, one row per kind: infantry, cavalry and tanks, artillery, aircraft (the rally point moves to the bottom-right corner). Clicking a unit queues one in every selected building that can train it. In the original game this showed nothing.
- **Clearer buttons:** locked options (wrong age, missing research) show a padlock. Options you cannot afford right now are drawn darker but stay clickable, so you can queue ahead.
- **Resource panel easier to read:** the panel in the top left (resources, commerce limit, farms and camps) is 25% larger, numbers included, and the income rates ("/+40") are green instead of grey. The population and city counters moved from the top right onto the panel's first row, right of the commerce limit, also 25% larger; the help and objectives buttons stay in the corner. `hud_scale` and `green_rates` in `ron_camera.ini` (the larger panel itself comes with the interface theme). The numbers are enlarged by the nearest amount that keeps their pixels whole (with the interface at 150%: a third larger), so their outlines stay clean; at 25% they had holes.
- **Worker bars in the resource panel** (new 2026-11-01): the panel no longer has a column for farms, woodcutters, mines and the rest. Under each resource there is a thin bar with one segment per place of work (farms, woodcutter places, mines, oil wells, merchants, scholars), green for the ones in use, and the count ("7/7") at the end of the row, so you see at a glance where workers are missing. The handle under the rows hides or shows the bars; without them the rows are shorter. The market opens to the right of the resources, its Buy (green) and Sell (yellow) buttons level with their resource, instead of pushing the whole panel aside. The population and city counters sit one under the other in the market's column, on a plain dark background: at the top, over a flat bar that opens the market, or under the prices while it is open (side by side on the first row when the whole panel is folded). The market's Buy and Sell cells fill their whole row, without the extra strip under them, and their prices are as large as the other numbers. The numbers in the panel, the market and the counters are drawn with a real font (Palatino bold, white with a black outline) instead of the game's tiny pixel font, which came out blocky when enlarged, with a yellowish fringe and white dashes over some digits; the "/" between a number and its income or limit is a thin straight line. `resource_bars=0` under `[ui]` in `ron_camera.ini` brings back the game's panel.
- **Wider resource rows** (2026-11-05): the resource rows are about a quarter wider, so a large amount, a large income and the worker count (for example 29125, +1059 and 31/51) no longer run into each other.
- **Hotkeys on the buttons:** every command button with a keyboard shortcut shows its key in the top-left corner, as in Company of Heroes 3, so there is no need to hover for the tooltip (`show_hotkeys` in `ron_camera.ini`). The letters are larger and bold (`hotkey_font_size`, `hotkey_bold`). Unit upgrade buttons show no key (2026-10-27): their "CTRL + key" did not fit and covered the picture.
- **Craft points bar** (new 2026-10-27): Generals, Spies, patriots and every other unit with craft points show a blue bar of the points left right under their health bar, as wide, so you see at a glance when an ability is ready (`mana_bar` in `ron_camera.ini`; drawn with the veteran rank bars, so it needs `ranks=1`). Decoys show none.
- **Abilities you cannot use now are dimmed** (new 2026-10-27): a General's, Spy's or patriot's ability button is drawn darker, like a unit you cannot afford, while none of the selected units that have it can cast it (not enough craft points, ...).
- **Selections grouped by kind** (new 2026-10-27): with several units selected, the panel shows one picture per kind of unit with how many there are, instead of the same picture again and again; the game did that only when the units no longer fitted (`group_selection` in `ron_camera.ini`).
- **Range circles:** a selected Tower, Fort, City or other building that attacks shows its attack range as a circle on the map (`range_circles` in `ron_camera.ini`). The game drew it only while the mouse was over the range in the selection panel. The circle is semi-transparent, in your colour, on the map only. Fixed 2026-10-27: the first version unlocked the wrong circle, the minimum range, which is 0 for nearly every building, so no circle showed; it now unlocks the attack range itself (a rounded square, as the game draws it; it includes the garrison and rank bonuses).
- **Settings that were ignored:** `fog_blur`, `fog_edge` and `auto_trade_delay` in `ron_camera.ini` now work (they were read from the wrong section).
- **Hit points as numbers** (current/max) under the health bar of the selected unit or building.
- **Kill counter:** a skull stat showing how many enemy units and buildings the selected military unit or building has destroyed. It counts from the start of the session. With several units selected (2026-10-27), the skull sits at the left end of the panel's top bar with the kills of the whole selection added up.
  Airbases and Carriers also show the total kills made by their aircraft, and a Missile Silo the kills of its missiles (if the game links the missile to its silo; to be confirmed in game).
- **Auto-trade:** idle merchants start trading on their own after a few seconds (`auto_trade`, `auto_trade_delay`), on the route the game itself would pick.
- **Leader names for computer players:** computer players are named after a leader of their nation (Julius Caesar, Queen Chabi, ...) instead of "Computer" (`ai_leader_names` in `ron_camera.ini`). Since 2026-10-27, in single player they also show with their leader and nation from the start, on the loading screen and in the score list (`reveal_ai_players`). The original game showed "Computer" there for an enemy until you met them; `reveal_ai_players=0` brings that back. Multiplayer keeps the original rule.
- **New models load from `mod_override`:** the game checks that a unit's model exists before loading it, and that check could not see files kept only in `mod_override` (the game stopped with "NO DEFAULT UNIT GPIECE"). Now it does, so mods can add new 3D models without touching the game's `art` folder. `ron_camera.log` also lists every replaced file once, not only the first 50. To confirm in game.

## Interface theme (`ui-theme`)
- Aircraft (2026-10-27): the Air Mission button (strike or patrol, shown when planes or helicopters are selected, and on the Airbase with `winmm.dll`) uses the Attack icon (red arrow and sword) instead of the plane with a curved arrow, which looked like Return to Base. Not yet seen in game.
- Selection panel (2026-10-27): the capital's star sits on the picture, at its top edge, in the middle, instead of under the hit points. The picture used to cover it; it is now drawn on top of the picture, in the player's colour, as tall as the rank insignia. Not yet seen in game.
- New look for the launcher window: steel frames, a big picture panel that changes every few seconds, through rolling fog and in random order, between paintings of the soldiers of history's great empires (a Roman legionary in Rome, a crusader over Jerusalem, a British redcoat in London, an American soldier in Washington; new 2026-10-26), new minimize and close buttons, and steel Play / Achievements / Website / Workshop buttons in the main menu's style instead of the blue ones. The buttons are narrower and taller than the originals (150x40 instead of 191x34), with centred captions, grouped under Play with even 14 px gaps.
- The launcher window is 15% larger (865x842 instead of 752x732), with everything in it scaled to match: frames, buttons, captions, minimize and close. The logo in the top-left box is 15% smaller than before, with more room around it. The buttons are centred in their panel, and minimize and close are no longer cut off at the edges.
- **Launcher button sounds** (new 2026-10-29): a short tick when the mouse comes onto a launcher button (the four big ones, minimize, close and the language flag) and the game's own menu button sound when you click it. `hover_sound`, `click_sound` and `sound_volume` (70 %) under `[launcher]` in `ron_camera.ini`; an empty name turns that sound off.
- New main menu: the background loops the destroyer scene only (sky, ship, sea, waves and spray redrawn, the sea lowered so the ship stays visible), with a new overlay and menu frames. The menu buttons have narrower, centred steel plates (320 px instead of 454), are taller (40 px instead of 37), and have even gaps between them. The menu panel and its frame are narrower too (434 px instead of 548), so the frame fits the buttons.
- Game setup screen: the players and rules panels sit further from the screen edges (4% instead of 2% at the sides, 6.5% at the top), clear of the screen frame's corner brackets, and their frames are steel all round (a strip on the right side was still bronze).
- Game setup screen: **Start and Back** are wide buttons with larger text, in a holder twice the original width at the bottom right of the players panel (single player and LAN setup). The holder was then made 10% wider and 7% taller, with the buttons centred in it.
- **Nation flags** on the game setup screen: each nation box shows the nation's round flag left of its name, and so does every row of the open nation list. The nation column is wider (the player name column gives up the space). All 24 nations have a flag. The flags need both this theme and `winmm.dll` (`nation_flags` in `ron_camera.ini`).
- **Nation picker as flag cards:** opening a nation box on the game setup screen shows a panel instead of the long list: the 24 nations as a grid of round flags (6 per row, no names), and Random, Offensive, Defensive, Economic and X-Pack as buttons on its left. The card under the mouse lights up and the chosen nation has a gold frame. Hovering a flag shows the game's own tooltip for that nation (its name and powers) beside the panel. Click a card to choose it; the keyboard still works as in the list. Needs the flags above (`nation_grid` in `ron_camera.ini`).
  - The card under the mouse now glows with pulsing gold rings. Fix attempt for the tooltip not showing over the flags: its hover zone is now matched to where the game reads the mouse (with interface scaling the two differ).
- The nation flags have clean round edges (no white fringe or bits of background) and are twice as sharp. The grey ring that still showed around them in the flag panel is gone.
- Dialog buttons (OK / Cancel in pop-ups such as "Are you sure you want to quit?", and the buttons of the game setup, options and Workshop screens) are steel plates in the same style instead of blue.
- Question boxes have the steel frame (at half size) instead of the wooden one, without the dark band behind their buttons. The "Are you sure you want to quit?" pop-up of the main menu (and the game's other pop-up questions) has the steel OK / Cancel buttons too.
- The in-game menu (Return to Game, Options, Save Game, ...) has the steel buttons instead of the blue ones.
- The end of game window (Victory / Defeat: Examine Map, Game Stats, View Playback, Quit), ranked games included, has the steel buttons instead of the blue ones (2026-10-27).
- Fix: in a game, the pop-up questions (such as "Do you want to resign this game?") showed blank white OK / Cancel buttons. They now have the steel buttons too.
- Game setup, options and other screens built from the shared interface pieces: wooden frames and section bars are steel, glossy blue buttons, tabs, check boxes and drop-down arrows are gunmetal, drop-down fields are grey with a fine grain instead of white (2026-11-05; the greyed-out ones darker, dark text stays readable), and selected items are marked in the theme's orange. Pictures and status icons keep their colours.
- Main menu logo: the game's own Rise of Nations logo (gold globe and gear, whole and still lit by the cursor), with REVIVED in place of the "Extended Edition" ribbon. REVIVED gets lit by the cursor too.
- Launcher logo: "Rise of Nations / REVIVED", with REVIVED drawn in front of the gear. The small logo fits inside its frame.

## Roman nation art (`roman-buildings`)
Buildings drawn in a Roman style for the Romans only. Other nations keep the original art. Since 2026-10-25 they carry a medium layer of wear (grime, rain streaks, darker wall bases), like the game's own art.

On winter maps the Roman buildings are shown too (for now without snow on them; before, the Romans got the original snowy buildings there).

The colours of every new building are balanced to match the game's own art. Images made on a magenta background used to come out with a pinkish tint, most visible on the Large City 6-8.

Each player's buildings follow that player's own age, so a Roman computer player that is behind shows its older city.

**Depth for the new buildings (all nations' new art, 2026-10-25):** crops, water and units behind a new building no longer show through the parts of it that the original building did not have (seen on the Roman Tower 7-8 between its supports, the Roman Large City 6-8, and the Egyptian buildings). Each new building now comes with its own depth shape, made to fit its picture. Thin parts that hang in the air (palm crowns, spears, flags) are not covered.

Fixed 2026-10-26: the first in-game test (Dutch, age 1) showed wheat and the territory border cutting through the new Small City and Market in vertical strips. The depth shapes were built inside out, so the game threw most of them away. All of them have been rebuilt.

Fixed 2026-10-29: trees and crops behind the Tower 7-8 (Roman, Dutch and Romanian) drew over the top of the tower, where it is wider than its shaft. The top now has depth again.

| Building | Ages | Description |
|---|---|---|
| Barracks | 1-3, 4-5 | Roman military camp buildings |
| Tower | 1-3 | A frontier watchtower of rough logs as on Trajan's Column, with an open lookout, a tiled roof, a shield and a flag in the player's colour waving on the roof (redone 2026-10-27, not yet seen in game; the old one was brighter than the rest of the game) |
| Tower | 4 | Stone tower with a red-tiled lookout |
| Tower | 5-6 | Crenellated stone tower with cannons and red banners |
| Tower | 7-8 | High-tech steel tower with a radar dome, a golden legion eagle (aquila), a bronze laurel frieze and a concrete gun base. |
| Small City | 1, 2, 3 | A different Roman town for each age |
| Small City | 4-5 | Renaissance and Baroque Rome: ochre houses, a domed Baroque church, a fountain, balconies, awnings |
| Large City | 3 | Roman city |
| Large City | 6-8 | Modern Rome: a colonnaded travertine tower crowned by a bronze eagle and solar panels, a glass-roofed arcade, a statue on a curved colonnade, rooftop gardens with cypresses, shops with terracotta awnings |
| Market | 1, 2, 3 | A different Roman market for each age |
| Fort | 7-8 | Modern Roman fortress: steel command tower with a radar dome and aquila, triumphal-arch gate, gun bastions, laurel frieze. Banners and flags show the player colour. |
| Data Center | 8 | A temple of data: Corinthian portico with a bronze aquila on the pediment, a concrete server hall with a bronze laurel frieze, glowing server racks in an open light well, cooling units and solar panels on the roof, formal gardens with fountains. Banners show the player colour. |

## Egyptian nation art (`egyptian-buildings`), started
Buildings drawn in an Egyptian style for the Egyptians only. Before, in ages 1-3 they shared their buildings with the Aztecs, Turks and seven other nations. They carry a medium layer of wear (grime, rain streaks, darker wall bases), like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Small City | 1-3 | Mud-brick houses with flat roofs, palm-log beams, outside stairs and reed awnings, a small temple with painted pylons and hieroglyphs, date palms, a water basin. Flags show the player colour. |
| Large City | 3 | A walled mud-brick town around a market square with a well: a gate between two painted pylons with hieroglyphs, flat-roofed houses with palm-log beams and reed awnings, fruit stalls, a date palm. Flags show the player colour. |
| Temple | 1-3 | An open shrine around an obelisk with a gilded tip and hieroglyphs: painted papyrus columns under linen awnings, an offering table with lotus flowers, jars, two sphinxes at the stairs. Flags show the player colour. |
| Barracks | 1-3 | Two flat-roofed mud-brick barracks with reed roofs and an open armoury on painted pillars: spear racks, bows, hide shields, archery targets, a chest of arrows, a campfire. A painted band on the walls and a cloth in the armoury show the player colour. |
| Market | 1-3 | A mud-brick trading house with an outside stair and a linen awning over a roof terrace full of jars, bread ovens, bales of cloth, stalls with grain, dates, figs, onions and melons. A painted band and a cloth over the door show the player colour. |
| Library | 1-3 | A House of Life (school of scribes): flat-roofed mud-brick halls with shelves of papyrus scrolls, a tower, a roof terrace under a linen awning with scribes' desks, palettes and ink pots. Painted bands and cloths over the doors show the player colour. |
| Senate | 1-3 | The pharaoh's palace: a gate between two tall pylons painted with gods and hieroglyphs, a long portico of painted papyrus columns with lotus capitals and, inside, a gilded throne under a white linen canopy, blue and green Egyptian cornices, a lotus pool, two date palms and small sphinxes by the stairs. A painted band on the gate and the cloth over the door show the player colour. |

## Northern European nation art (`northern-european-buildings`), started
One set drawn for the British, French, Germans and Dutch together. Before, in ages 1-3 they used the Greek and Roman buildings. They carry a medium layer of wear, like the game's own art.

New buildings that were too big for their ground (2026-10-26): several new buildings stood past the edge of the ground they occupy, for example the northern European Small City (houses behind the city's border, empty ground on the right) and Library. Nineteen buildings of the Romans, Egyptians, Russians, northern Europeans, Americans, Persians and Nubians were shrunk by 7-25 % to fit their ground like the game's own buildings, and the northern European Small City was moved onto the middle of its ground.

No patch of ground under the new buildings (2026-10-25): the Russian Large Cities, the Mongol camp and the northern European Small and Large City first had a yard of earth, mud or paving under them, which looked wrong on maps with other terrain. It is gone; the buildings now stand directly on the map's own ground, like the game's buildings.

| Building | Ages | Description |
|---|---|---|
| Small City | 1-3 | An early medieval village of northern Europe: half-timbered houses with wattle and daub and steep mossy thatched roofs, the lord's hall with an outside stair, a small stone chapel with a wooden bell tower, a fenced garden with a roofed well, carts, a haystack, barrels, a cooking fire. A painted band under the hall's roof and a cloth over a cottage door show the player colour. |
| Large City | 3 | A high medieval market town: a large Romanesque stone church with a long nave and a tall square bell tower, two- and three-storey half-timbered town houses with jettied floors and thatched or shingle roofs closing a ring around a market square with a stone cross and striped stalls. A painted band under the eaves of the biggest house and the awning of a stall show the player colour. |
| Temple | 1-3 | An early medieval stone church in place of the round Greek temple: thick walls of rough grey stone overgrown with ivy, a stout square tower with paired arched openings and a shingle pyramid roof, a round apse, a carved arched door, a churchyard with a Celtic cross and old grave stones inside a drystone wall. A painted band under the tower roof and a cloth over the door show the player colour. |
| Barracks | 1-3 | The camp of a Saxon or Norman warband in place of the hide tents: two plank halls on stone footings with mossy thatched roofs, an armoury with axes, spears and kite shields, a spear rack, an archery target, a training post, a campfire with log benches, barrels and a water tub. No patch of ground under it, so it sits on any map. A painted band under the middle hall's roof and a cloth over a door show the player colour. |
| Market | 1-3 | A half-timbered market hall on thick oak posts with a mossy thatched roof and an outside stair, sacks of grain, barrels of ale, wheels of cheese, rolls of wool cloth, baskets of apples and onions, clay jugs, two plank-roofed stalls with fruit and fish, a rack of drying hides. No patch of ground under it. A painted band under the upper floor and a cloth on the drying rack show the player colour. |
| Library | 1-3 | A monastery school in place of the Greek school: one L-shaped building with a grey stone ground floor, a half-timbered upper floor with small arched windows and mossy thatched roofs, a cloister walk of round stone arches along the inner side, a small bell tower where the wings meet, a lectern with a big open book and a stone bench. No patch of ground under it. A painted band between the floors of the right wing and a cloth over the lectern show the player colour. |

## Dutch nation art (`dutch-buildings`), started
Buildings drawn for the Dutch only. Before, they used the same buildings as the British, French and Germans, and in the modern ages the same as most nations. Medium wear like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Temple | 4-8 | A Golden Age Amsterdam church in place of the Gothic cathedral (new 2026-10-26): a brick hall with white stone bands and pilasters, tall arched windows, a slate roof with stone dormers, a stepped gable with volutes, and a tower like the Westertoren with a clock, open lantern stages and the blue and gold imperial crown on top. A painted band under the eave and a cloth over the door show the player colour. |
| Tower | 7-8 | A 1930s Dutch concrete observation tower in place of the lookout on stilts (new 2026-10-26), in the family of the Dutch fort: a dark brick base with an armoured door, a tall tapering concrete shaft with formwork joints and two rows of machine-gun slits, and a platform with a riveted steel cupola and two machine guns. A painted band under the platform and the door show the player colour. |
| Fort | 7-8 | A 1930s Dutch casemate fort, like Kornwerderzand on the Afsluitdijk, in place of the sandbag-coloured bunker: a tall stepped concrete keep with a riveted steel cupola and twin guns in two deep embrasures, a low outer wall with three cupola turrets on its bastions, poured concrete with formwork joints, rain streaks and rust, a dark brick base and an armoured gate. Like the game's own fort, it stays the same in age 8. A painted band along the outer wall and the gate show the player colour. |

## American nation art (`american-buildings`), started
Buildings drawn for the Americans only. Before, until age 4 they used the Greek and Roman buildings. Medium wear like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Temple | 3 | A Puritan meeting house like Old Ship Church in Hingham (1681), in place of the Greek round temple: a large two-storey timber hall of weathered grey boards on a fieldstone base, a steep hip roof of mossy shingles with small gables, a railed deck with an open belfry and its bronze bell, a gabled porch, and beside it stocks, a well with a sweep, slate gravestones, a barrel and a bench. Ages 1-2 keep the game's temple, where this building would make no sense. The red-painted door, the band under the eaves and the trim of the porch gable show the player colour. |

## Spanish nation art (`spanish-buildings`), started
Buildings drawn for the Spanish only. Before, they shared every building with the Greeks and Romans. Medium wear like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Senate | 4-8 | The Alcázar of Segovia in place of the domed Roman senate (new 2026-10-26): the royal palace Philip II rebuilt, with the massive Juan II keep and its corner turrets, a long wing under slate roofs with pencil spires and dormers, round towers with tall conical slate roofs, battlements and an arched gate. A band along the front wall and a cloth over the gate show the player colour. |

## Persian nation art (`persian-buildings`), started
Buildings drawn for the Persians only. Before, in ages 1-3 they shared the desert buildings with the Aztecs, Turks and seven other nations. Medium wear like the game's own art.
- **Fixed 2026-10-27:** these buildings never showed in game; `nation_art.ini` named the nation `persians`, while the game calls it `persian`.

| Building | Ages | Description |
|---|---|---|
| Temple | 1-3 | A Zoroastrian fire temple (chahar taq, like the Sasanian temples of Niasar and Takht-e Soleyman) in place of the stone circle: four massive stone piers joined by four open arches under a plastered dome, a stepped stone fire altar burning in the middle, a low stepped platform, two small fire bowls, a stone stele with the winged Faravahar, clay jars and a stack of firewood. A painted band at the foot of the dome and a cloth hanging in one arch show the player colour. |
| Senate | 1-2 | The Apadana of Persepolis, the audience hall of Darius I: a high terrace of grey limestone with a double stair carved with tribute bearers and a lion attacking a bull, a flat-roofed hall with a portico of very tall fluted columns crowned by double-bull capitals, corner towers with a narrow frieze of glazed-brick archers, black stone door frames, stepped battlements, and a gate guarded by two great lamassu under the winged Faravahar. Age 3 keeps the game's senate for now. A band over the gate and the cloths in the doorways show the player colour. |

## Nubian nation art (`nubian-buildings`), started
Buildings drawn for the Nubians only. Before, in ages 1-3 they shared the desert buildings with the Aztecs, Turks and seven other nations. Medium wear like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Temple | 1-3 | The royal pyramids of Meroë in place of the stone circle: three tall, very steep Nubian pyramids of reddish sandstone (the middle one tallest), each with a small pylon chapel at its foot, two stone rams and an offering table with jars, seen from the game's camera (made as a 3D model and rendered from the game's angle). A painted band over the middle chapel's gate and a cloth in its doorway show the player colour. Fixed 2026-10-26: the front corner of the right pyramid was cut off; it is whole now. |

## Aztec nation art (`aztec-buildings`), started
Buildings drawn for the Aztecs only. Before, in ages 1-3 they shared the desert buildings with the Maya, Inca, Turks and six other nations. Medium wear like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Temple | 1-3 | An early Templo Mayor of Tenochtitlan in place of the stone circle: a stepped pyramid of rough grey stone and patchy lime plaster with a double stair on its front, and on top the twin shrines of Huitzilopochtli (whitewashed) and Tlaloc (blue and white stripes) under tall thatched roofs, with small stone altars and fire bowls. A painted band around Huitzilopochtli's shrine and a cloth over its door show the player colour. |

## Mongol nation art (`mongol-buildings`), started
Buildings drawn in a Mongol style for the Mongols only. Before, they shared every building with the Chinese, Japanese and Koreans. They carry a medium layer of wear, like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Small City | 1-3 | A nomad camp of the steppe: six felt yurts of different sizes with patched, soot-stained felt and hides on the roofs, the chief's yurt on a plank platform, a cart carrying a small yurt, a rack of drying meat, milk buckets, saddles, dried dung fuel, a campfire with stools. A painted band around the chief's yurt and a felt door show the player colour. |

Fix (2026-10-26): the camp stood too far back and to the left of its city ground; the back yurts were behind the yellow outline. It now sits on its ground, a little smaller (90%).

## Russian nation art (`russian-buildings`), started
Buildings drawn in a Russian style for the Russians only. Before, they shared their buildings with the British, Dutch, French and Germans. All of them carry a medium layer of wear (grime, rain streaks, darker wall bases), like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Small City | 1-3 | An early Slavic village of ancient Rus: low log houses with thatched roofs, a log watchtower on stilts, a smithy with an anvil and a forge, a farmstead with a wattle fence, a granary on posts and sheaves of grain, a campfire with log benches. A painted band under the watchtower roof and a cloth over a door show the player colour. |
| Large City | 3 | A fortified town of ancient Rus: the prince's two-storey log hall with a carved gallery and a shingled roof with a small tower, log houses with thatched and plank roofs around a yard, a sharpened log palisade, a log watchtower with a tented roof, a small wooden church with a shingled onion dome, a well with a sweep. A painted band along the hall's gallery and a cloth over a door show the player colour. |
| Barracks | 1-3 | The camp of a druzhina (the prince's warband) in place of the hide tents: two log longhouses with plank roofs, a log armoury with spears and axes, racks of spears, round wooden shields, a training post, a sharpened-stake palisade, a campfire with log benches. A painted band on the middle longhouse and a cloth over a door show the player colour. |
| Market | 1-3 | A trading place (torg) of ancient Rus in place of the stone hall: a log storehouse on carved wooden pillars with a plank roof terrace under linen awnings, bundles of furs, honey barrels, jars, sacks of grain, wax and cloth, a rack of furs drying, stalls with cabbages and fish. A painted band on the log wall and a cloth on the drying rack show the player colour. |
| Library | 1-3 | A monastery school and scriptorium of ancient Rus in place of the Greek school: two log wings with carved wooden pillars on a stone base, plank roof terraces with writing desks, books, rolls of birch bark and an icon, a plank courtyard with wooden steps, a lectern with a big open book, young birches. A painted band on the right wing and a cloth over the lectern show the player colour. |
| Fort | 1-3 | A wooden fortress (detinets) of ancient Rus in place of the thatched fort: a tall keep and corner towers of rough log cribs with jutting upper storeys, sagging shingle roofs with moss and missing shingles, a palisade of uneven, patched stakes on an earth rampart, a heavy plank gate, spears and round shields. A painted band on the keep and a cloth over the gate show the player colour. |
| Tower | 1-3 | A square log watchtower of ancient Rus in place of the round thatched one: rough horizontal logs, a jutting top storey with archer slits, a sagging mossy shingle roof with a small lookout. A painted band under the roof shows the player colour. |
| Temple | 1-3 | An old wooden church of ancient Rus in place of the round Greek temple: log walls on a stone base, a covered porch on carved posts, steep shingle roofs, a tall tented drum with a silvery shingled onion dome and a gold cross, a side wing. A painted band along the porch roof and a cloth over the door show the player colour. |
| Temple | 4-8 | An Orthodox cathedral in place of the Gothic one: white walls with kokoshnik arches, green copper roofs, a gold central onion dome and four blue domes with gold stars, a gold cross on every dome, a bell tower with a tented roof. A painted band under the gold dome and the arch over the main door show the player colour. |
| Small City | 4-5 | A Russian village around a fenced yard: log houses (izba) with carved white window frames, a merchant's house with a white stone ground floor and a carved porch, a market house with grain, barrels and baskets, a wooden well, firewood stacks and carts. A painted band on the merchant's house and a cloth over a door show the player colour. |
| Large City | 4-5 | A Russian town around a market square: a boyar's palace (terem) with a white stone ground floor, carved log upper floor, tented green roofs and two small onion domes, a wooden watchtower with a log palisade, log and merchant houses with carved window frames, a small white church with a blue onion dome, market stalls with grain, barrels and furs, wells. A painted band on the palace and a cloth over a door show the player colour. |
| Barracks | 4-5 | A streltsy (musketeer) camp in place of the knights' tents: two log barracks with carved window frames, a wooden watchtower, a sharpened log palisade, racks of berdysh axes and matchlock muskets, shields, a training dummy, a cannon, a log storehouse with powder barrels, a campfire with a cauldron. A painted band on the long barracks and a cloth over a door show the player colour. |
| Market | 4-5 | A trading house (gostiny dvor): white stone arcades with shops under a log upper floor with carved window frames, a carved outside stair with a little tented roof, a roof terrace with grain sacks, honey barrels, apples, cabbages, furs, cloth and copper samovars, two plank-roofed stalls with fish and pottery. A painted band on the building and a cloth on a stall show the player colour. |
| Library | 4-5 | A monastery library and school of book copyists: white stone arcades with shelves of books under carved log upper floors and green copper roofs, a small tower with a blue onion dome and a cross, a front deck with icons on easels, lecterns with open books, writing tables and candles, a terrace with a reading desk. A painted band on the building and a cloth over the reading desk show the player colour. |
| Tower | 4 | A round tower of weathered grey stone like the corner towers of the Russian fort: a jutting top storey with cannon ports, a tented roof of dark wooden shingles with a small lookout. A painted band under the roof shows the player colour. |
| Fort | 7 | The modern fort of every nation, made Russian: concrete in olive-green camouflage, angular Soviet-style steel turrets with long guns, a concrete command post with an antenna mast on the central tower, dark green steel gates. A star painted on the tower and a band under the command post show the player colour. |
| Fort | 8 | The same fort upgraded for the Information Age: heavier mottled camouflage, twin-gun turrets, a grid of missile-launch hatches on the roof of the central tower, a flat panel radar fixed to its side. A star and a band on the central tower show the player colour. |
| Fort | 4 | A Russian stone fortress built for war, like those of Pskov and Izborsk, in place of the Western castle: weathered grey limestone walls and squat round towers with cannon ports, tall tented roofs of dark wooden shingles with lookout cabins, a heavy square keep, a covered wooden fighting gallery, a gate with an iron portcullis and a drawbridge, piles of cannonballs and powder barrels. A painted band over the gate and cloths hanging on it show the player colour. |

## Modern art for every nation (`modern-buildings`), started
Buildings of the late ages that every nation shares, made to look like their age. Medium wear like the game's own art.

| Building | Ages | Description |
|---|---|---|
| Auto Plant | 8 | A military vehicle plant in place of the brick garage (new 2026-10-28), in the same layout: a dark steel hall with solar panels and a green roof, an octagonal annex with ventilation units, wide open bays with a Humvee, workbenches and a drone inside, supply crates and two small tracked robots by a wire fence in front. Bands under the hall's eaves and along the annex parapet show the player colour. Ages 6-7 keep the garage. |

## City districts (`data-center`), new 2026-10-29
The Data Center is gone: its Expansions became **city districts**, for every nation.
- **Where:** built from the panel of any of your cities (bottom row), on a 2x2 plot next to that city or to one of its districts, with at most 2 empty tiles between them so units can pass. A builder comes out of the nearest city.
- **When:** once you have researched every Library technology up to the last Age of the game (the four last Information Age ones excepted). A game that stops at the Modern or Industrial Age gets them too.
- **What:** Housing District +10 population above 200 (6500 Food + 3400 Knowledge), Commerce District +30 to every resource-rate limit above 999 (3000 Knowledge + 3000 Wealth), Industrial District +3% to all income (3000 Knowledge + 2000 Wealth). As many as you like; each one of a kind makes the next 5% dearer. (Cheaper and smaller since 2026-10-29: they were +20 / +75 / +9% for 15000 + 7500, 7500 + 7500 and 9000 + 6000, 6% dearer each.) A destroyed district takes its bonus with it.
- **Look:** grey communist blocks, a department store and a workshop, the same for every nation for now.

- **The Data Center is removed** (2026-10-29): it is no longer in the game at all, not even hidden; its slot is the game's own scenario-only Washington Monument again, and its pictures are gone. The districts' builder is called District Builder.

(Older, replaced and removed:) ## Data Center (`data-center`)
A new end-game building for every nation, from the Information Age.
- **The building:** the size of a Wonder (7x7), 400 Knowledge + 400 Wealth, 2500 HP, in the economy menu. Its art is drawn from the game's own camera angle and footprint, so it sits on the map like the other buildings. The Romans get their own version; for now the other nations show it too, until the generic version is ready.
- **Dutch version** (new 2026-10-26): Dutch modernism. A long brick server hall with tall glass curtain walls (servers glowing blue behind them), stepped canal-house gables redrawn in glass at the front corner, solar panels and five cooling towers on the roof whose fans turn, and a service yard of chillers in the courtyard. A thin band under the cornice shows the player colour.
- **Needs a final technology** (new 2026-10-26): at least one of the four last Library technologies, Missile Shield, World Government, Global Prosperity or Artificial Intelligence. Until then its button shows a padlock.
- **One at a time:** you cannot start a Data Center while you own one, built or under construction (computer players included). Captured Data Centers can be kept and used, so a conqueror may own several, but cannot build another while owning any.
- **Expansions** (new 2026-10-26, replacing the upgrades): three small buildings, one tile each, anywhere in your territory. They are ordered at the Data Center, not by Citizens: select your Data Center, click an Expansion on its panel and place it (hold Shift to place several). The Data Center's own technician comes out, builds every site you placed, one after the other, and goes back inside. He cannot be selected or given orders; if he is killed, the next one comes out 20 seconds later. Build as many Expansions as you like: their bonuses add up, but each Expansion of a kind you own makes the next one of that kind 2% dearer (2026-10-26). An Expansion's bonus lasts only while it stands: when it is destroyed or captured, the bonus goes with it. Fixed 2026-10-26: the Expansions were drawn about half a tile off their place, both while placing them and once built. Also fixed: their shadow fell to the front and was cut off by the edge of the picture; it now falls the way the game's sun casts every other shadow (to the left, towards the back) and has the shape of the tower.

  | Expansion | Bonus each | Price of the first | Each one you own adds |
  |---|---|---|---|
  | Population Expansion | +7 population limit | 5000 Food + 2500 Knowledge | 100 Food + 50 Knowledge |
  | Commerce Expansion | +25 resource-rate limit for every resource | 2500 Knowledge + 2500 Wealth | 50 Knowledge + 50 Wealth |
  | Efficiency Expansion | +3% to all resource income | 3000 Knowledge + 2000 Wealth | 60 Knowledge + 40 Wealth |

  Each price is the price of the last one-time upgrade of its branch in the first version.

  - The population limit can go above the game's maximum of 200, and the resource-rate limit above 999.
  - Efficiency raises the income of every resource, including resources already at their limit.
  - The Commerce bonus raises only the limit: a resource gathered below its limit does not change. A resource already at its limit (where the game shows the commerce limit message) earns more right away.
  - The commerce limit shown at the top of the screen and the gather rates in tooltips no longer stop at 999, and the Dutch (whose interest on the treasury went through a fixed 999 limit) get the higher limit too.
  - Their art (new 2026-10-26): one tower for all three, a worn steel server tower on a concrete plinth, with a broad player-colour band and four fans on the roof. Its lights tell the kinds apart: green for Population, gold for Commerce, blue for Efficiency. The lights breathe: they brighten slowly to full and fade back, every 2.4 seconds, the LEDs under the roof twinkling a little; the roof fans turn once in the same time. Each building moves at its own pace. Their icons are the painted branch icons: a garden arcology tower (Population), cargo and coins (Commerce), a robot arm and a drone (Efficiency).
  - The bonus values can be changed in `data_center.ini`. In multiplayer every player needs the same mod files.
  - The Expansions take the places of three scenario-only buildings (El Dorado, the Courtyard Fountain and one of the scenario Houses), so scenarios that use them may look wrong with the mod installed.
- **Removed 2026-10-26:** the 15 upgrades researched at the Data Center (four one-time upgrades and a repeatable one per branch), with their green ticks, the `<save>.dc` files next to saved games and the Tab stop at the Data Center. The Expansions do their job. The campaign units those upgrades had taken over are back to normal.
- Computer players build the Data Center only late: once their Knowledge and Wealth income are both at least 400 (`[ai] min_income` in `data_center.ini`).
- **Planned:** it can be built only in the capital.

## Airdrop (`paradrop`), first test in game passed: the drop works
A special ability for the Airbase, for taking cities faster on large maps.
- **How it works:**
  - From the Information Age (age 8; it was the Modern Age until 2026-10-28), an Airbase has an **Airdrop** button on the aircraft row, next to the helicopter. Click it, then click a spot on the map.
  - A transport helicopter takes off from that Airbase and flies there. It comes down to the ground, 2 squads of Paratroopers get out (6 soldiers), then it climbs back to flying height and flies home.
  - At its Airbase it lands and disappears.
- **Price:** 400 Food + 350 Oil, shown with resource icons like any other price; a third comes back when the helicopter lands at its Airbase again.
- **Population:** the helicopter and both squads take population like any other unit (3 in all). Since 2026-10-26 a drop is refused when there is no room for them under your population limit, the same way as when you cannot pay.
- **Cooldown:** each Airbase can send one every 45 seconds. While it waits, a dark clock wipe over the button shows how much time is left. Fixed 2026-10-26: the button's tooltip now always shows the price; before, it lost it while the Airbase was not ready or you could not pay. A click then also gets the game's usual red refusal instead of nothing.
- **Aiming and several Airbases** (new 2026-10-26):
  - While you aim, two rings in your colour on the ground under the cursor show where the squads will land.
  - With several Airbases selected, the button shows a number: how many drops you can call right now (Airbases that are ready, as far as your food, oil and population room go).
  - After a drop, the aiming stays on while drops are left, so you can click several targets one after another. Each click sends the next ready Airbase. Right-click or Esc stops aiming as before.
- **The helicopter:**
  - You cannot select or command it.
  - Enemy anti-air can shoot it down. Then the drop is lost: no squad if it had not landed yet, no refund.
  - If its Airbase is lost, it flies to your nearest other Airbase.
- **Computer players use Airdrop too, but sparingly:** only on an enemy city already bombed down to under half its hit points, with few soldiers around, within reach of one of their Airbases. They also need plenty of food and oil to spare, and have only one helicopter in the air at a time. Their squad then walks into the city.
- **Paratroopers:** elite infantry: Assault Infantry in a black uniform. 230 hit points, attack 30, armor 5, speed 38, range 13. They can capture cities. Each one is its own unit: losses are final, and healing only restores hit points.
- **Transport Helicopter:** a twin-rotor helicopter (Chinook) with both rotors spinning, blue cockpit glass and the player colour on a band along its side, with the helicopter sounds. Its look was seen working in game.
- **Settings:** the numbers can be changed in `airdrop.ini` in the game folder.
- **Not saved:** a helicopter in the air when the game is saved is gone after loading.
- **Fixed 2026-10-27:** the Airbases' cooldowns and the helicopters' flights carried over into the next game played without restarting, so an Airbase could stay blocked for a long time and multiplayer games could go out of sync. They now start fresh with every new or loaded game.
- An earlier try, the game's own switched-off "Paradrop" ability, crashed the game when used (the developers never finished it). The new button uses its targeting, but the drop itself is the mod's.
- These units take the place of two campaign-only units, and the Paratroopers use the look of a third (Advisor Fouché), so the Conquest campaigns that use those may look wrong. In multiplayer every player needs the same mod files.

## Entrenchment (`entrenchment`)
A new defensive building for infantry: the **Field Fortification**.
- **Building:** ranged infantry build it themselves, not Citizens: after The Art of War (like the Tower), their panel has the fortification's build button, with its price, the building cursor and the outline where it will stand. They walk beside it, build it, then dig into it. Soldiers fighting or on the move stop working on it. Citizens do not have it in their build menu, and computer players do not build it. It changes with the ages: an earth bank with a palisade of sharpened stakes in the Ancient to Medieval Ages, gabions (wicker baskets of earth) in the Gunpowder Age, a stone artillery battery with embrasures and chevaux-de-frise from the Enlightenment Age, a low square of sandbags in the Modern Age and HESCO barriers with armour plates on a concrete floor in the Information Age (the Industrial look is still to come). Where the walls are high (the stone battery) the soldiers stand further in, so they can be seen over the front walls. 2x2, 60 Timber (more for each one you own). Its hit points are half a Tower's of your age (375 with the Tower, 500 Keep, 650 Stockade, 850 Bunker, plus the game's building bonuses). It has no weapon of its own.
- **Digging in:** right-click the fortification with ranged infantry (the garrison cursor shows over it), or use the game's garrison button and its hotkey, which ranged infantry get once you have a fortification. A squad that built it digs into it by itself. One squad fits in a fortification (since 2026-10-27; it was 3 before). Its three soldiers stand evenly spread on a triangle over the floor, one at the back and two at the front (new 2026-10-27; before, they stood in a line along one wall). They fight with their own weapons. They hold their ground and do not chase enemies.
- **While dug in:** +1 range, attacks 40% faster, +20% damage dealt.
- **Cover:** the fortification takes every hit meant for the soldiers in it, with the attacker's damage against a building, so the soldiers take no damage until it is destroyed. It counts as one target: a shell that bursts over it hits it once, however many soldiers the burst catches. Then they are out in the open, without the bonuses. To clear a fortification, bring what breaks buildings: artillery, siege, tanks.
- **Melee units cannot dig in** (spearmen, swordsmen and the like), nor Scouts and Explorers. Only foot soldiers dig in: no tanks, guns or other vehicles.
- **Price of digging in:** 35% of the squad's own price, +10% for every squad you already have dug in anywhere, up to +150% (87.5% of the squad's price). Nothing comes back when they leave.
- **Leaving:** the **Leave Fortification** button on the fortification puts everyone out in front of it; the fortification stays and can be filled again. Giving dug-in soldiers a move order also takes them out.
- **Soldiers stay inside:** dug-in soldiers that the game pushed out onto the edge of the fortification are put back on their places inside it.
- **Saved games:** dug-in soldiers stay dug in.
- **Fixed 2026-10-27:**
  - The soldiers' places inside a fortification carried over into the next game played without restarting, so a soldier could be pulled to a spot from the old game (and multiplayer could go out of sync). They now start fresh with every new or loaded game.
  - With more than 255 fortifications on the map, soldiers could drop out of theirs or its cover could go to another fortification. The limit is now 512.
  - The Leave Fortification button's tooltip text sat with the buildings' texts instead of the abilities', so the button probably showed no description.
  - The squads that built a fortification were turned away as if it were full when they tried to dig in at the moment it was finished, so only some of them got in.
- **Selecting:** the Army button (All Military) selects dug-in soldiers too.
- **Settings:** the numbers are in `entrench.ini` in the game folder.
- The fortification takes the place of the scenario-only Lenin Statue, so scenarios that use the statue show a Field Fortification instead. The Leave Fortification button takes the place of an ability the developers left switched off (Move Capital Here). In multiplayer every player needs the same mod files.

## Romania (`romania`)
A 25th nation: **Romania**, in the setup screen's nation list with its flag, playable by you or a computer player, and one of the nations Random and Random Defensive can pick.
- **The Power of the Carpathians:** attrition damage to enemy units in your territory +100%, and the attrition research (Allegiance, Oath of Fealty, Patriotism, Nationalism) is free; +20% Oil gathering; Forts and Towers 25% cheaper, Forts 50% faster to build, +3 National Borders and their research free; the Militia, Minuteman and Partisan upgrades free. Each piece is the game's own code for another nation's power (Russian, Roman, Korean); `romania.ini` can turn them off one by one.
- **Buildings of its own** (`romanian-buildings`, started 2026-10-27): the Temple of ages 1-3 is the Great Round Sanctuary of Sarmizegetusa Regia, the Dacian capital: a ring of andesite, groups of timber posts, a round log shrine with a tall shingle roof and the andesite "Sun" altar in front. The Small City of ages 1-3 is an old Romanian village, like the houses of the Village Museum in Bucharest: limewashed clay houses with tall thatched roofs and a painted band at their foot in the player's colour, a log house behind a carved wooden gate, a granary on stone legs, wattle fences. The Fort of ages 1-3 is a Dacian hill fortress like Costesti-Cetatuie: a ring of rough limestone with a palisade of sharpened logs on top, a stone and timber tower-house with a thatched roof and a band in the player's colour, thatched huts, a log gate and a wooden watchtower. The Tower of ages 1-3 is a Dacian watchtower to match: a rough limestone shaft with beam ends, an overhanging log storey with a band in the player's colour and a steep thatched roof. The Fort of the Gunpowder Age is the Seat Fortress of Suceava as Stephen the Great rebuilt it: a rough stone keep with a band in the player's colour, an outer wall with round towers and stepped buttresses, steep wooden shingle roofs, old and patched. The Tower of the Gunpowder Age is the Chindia Tower of Targoviste: a round stone tower with an arched lookout gallery, a band in the player's colour painted under it and a pointed shingle roof. The Fort of the Enlightenment and Industrial Ages is an 18th-century Habsburg fortress like Alba Carolina: low sloping walls with a band in the player's colour, four round corner bastions with cannons pointing out, a plain arched gate and a keep with one big gun on top. The Tower of the Enlightenment and Industrial Ages is a Romanian Army artillery tower of 1877-1890: a sloping stone and brick base, riveted iron bands and armour with a band in the player's colour, a Krupp-style gun and an iron observation cupola. The Tower of the Modern and Information Ages is a concrete watch and gun tower of the Focsani-Namoloasa-Braila line (1939-1944), in the Romanian Army's faded three-colour camouflage, with sandbagged embrasures, a band in the player's colour and a steel cupola on the roof. The Fort of the Modern and Information Ages is a position of the same line to match: a camouflaged concrete blockhouse with a band in the player's colour, octagonal corner casemates with steel cupolas, barbed wire and sandbags. The Large City of ages 1-3 is an old Romanian market town in the same style as its Small City: an inn with a stone cart arch and a long balcony, a boyar's house with a deep porch and a shingle roof, white thatched houses with the band in the player's colour, a log house, and a well with a tall well sweep in the yard. The Small City of the Industrial Age (2026-10-28) is a Romanian town of 1900-1930: a Neo-Romanian villa with an arched loggia and a turret, a Bucharest Art Nouveau house with a mansard roof and an iron balcony, town houses with arched loggias and a carved porch, and a corner shop, with a band of glazed tiles in the player's colour under the eaves. The Small City of the Modern Age is a corner of a communist housing estate: grey prefab panel blocks with stained joints, closed balconies each done up differently, TV aerials, a grocery, a clinic, rusty garages and a brick boiler house with a tall chimney, a thin line in the player's colour along the foot of the roof parapets (thinned on 2026-10-28: the first, wide band was too heavy). The Large City of the Industrial Age is an old town centre around 1900: a Neo-Romanian town hall with arched loggias, glazed tile friezes and a turret, an Art Nouveau corner building with a domed turret and iron balconies, French eclectic houses with shops below, and a band of glazed tiles in the player's colour under the eaves. The Large City of the Modern Age is a communist town centre: a ten-storey prefab tower block with laundry on the balconies, L-shaped blocks over a long row of shop windows, a newspaper kiosk and garages, with the same thin line. The Major City of the Modern Age (2026-10-28) is the game's own Major City redrawn as a run-down communist estate: prefab concrete blocks with closed balconies, satellite dishes and rusty panels, a wrecked car in the alley, roads at the edges that break up into loose asphalt, and some balcony panels in the player's colour. Since 2026-11-08 the Information Age shows the same communist cities. Fixes (2026-10-29): the Small and Large City of the Modern Age lost their heavy grey ground shadows, their player-colour line is 4 pixels high (2 looked too thin next to the other buildings), both are sharper and more contrasted (they looked hazy next to the game's buildings), and the Small City's buildings stand on their plot (they sat too far back: the tall block was off the plot), at 80% of their size.
- **Communist buildings, ages 7-8** (2026-10-29): the Senate (the former Communist Party Central Committee building), the Temple (an old Orthodox church), the Market (a 1980s Alimentara), the Library (the National Library), the Factory and Auto Plant in grey concrete, the Barracks (concrete huts), the University (a flying-saucer dome), the Missile Silo (a plain 1950s silo) and the Airbase (a 1950s airfield), all in one grey colour grade with the cities; the band in the player's colour.
- **Units:** European style, in every line the model the Russians have, without the Russian unique units (Romania has the ordinary Light Cavalry line, Infantry, Main Battle Tank and Howitzer instead).
- **R-2** (2026-10-29): Romania's Tank of the Modern Age (it replaces the TR-77, and the Industrial Age's Light Tank before it is now called Renault FT), the Czech-built LT vz. 35 of the Romanian 1st Armoured Division, with its own icon, has a model of its own instead of the British First World War Mark IV it showed: a small riveted tank with an octagonal turret that turns toward its target, a 37 mm gun and a machine gun, its wheels and tracks, and a band in the player's colour around the turret.
- **IAR-80** (2026-10-28): Romania's Fighter is the IAR-80, the Romanian-built fighter of the Second World War, with a model of its own: olive green, the round radial engine nose, Michael's Cross on the wings and the fuselage, the tricolour on the rudder, and bands in the player's colour on the wings and the tail. It is a unique unit of Romania with the usual Fighter's strength and cost, and it flies, shoots and sounds like the other fighters.
- **Quality against quantity:** 11 units of Romania's own. An elite that costs more: Falxmen (+24% attack and hit points, +24% cost), Roxolani Lancers (+18 to +20%), Viteji (+15 to +17%), Vanatori de Munte (+12 to +14%). Levies, reservists and older machines that cost less: Comati (-10 to -13%), Voinici (-12 to -13%), Darabani (-6 to -8%), Dorobanti (-13 to -14%), Automitraliera (-15%), R-2 (-21%), TR-77 (-5 to -7%). Attack and hit points follow the price exactly, so each gives the usual strength for its resources (the game keeps costs in tens, hence the uneven steps). The other 41 military units are the usual ones. Figures against the average of the 24 original nations; the guide's Romania section shows them all.
- **Ox Cart, then DAC Truck** (2026-10-29): from the Industrial Age the Supply Wagon is a truck for every nation, but Romania's was still called Ox Cart with an ox-cart icon. From the Industrial Age it is now the DAC Truck, with the game's truck icon.
- **Fix (2026-10-29):** the R-2 was a grey khaki next to the game's other tanks; it is now painted the same olive green as them. Not yet seen in game.
- **Fix (2026-10-29):** with several R-2 tanks on screen the game stopped on "too many verts for Pre-Transform Batch Transform": its model was five times as detailed as the game's own tanks. It is now lighter (815 points instead of 1962) and looks the same.
- **Fix:** Romania could not have the units that come in other looks for some nations (tanks, aircraft): the game gave those only to its first 24 nations.
- **Fix (2026-10-26):** starting a game with Romania crashed on the loading screen as soon as the mouse moved. The loading screen has room for 8 unique units and Romania has more; it now shows the first 8.
- **Fix (2026-10-26):** the loading screen showed Romania's own units with the icons of the units they replace (a Greek hoplite for the Falxmen, ...), and only 8 of the 11. It now has room for 12, in rows of 3, and shows Romania's own icons.
- **Fix (2026-10-26):** Romania's own units (Voinici, Darabani, Dorobanti, R-2, TR-77, ...) kept the icons of the units they replace everywhere in the game, and some Romanian names did not show: the mod read only the first 160 unit types it had a name or icon for.
- **Unit names from Romanian history:** Dacian Slingers, Falxmen, Roxolani Lancers, Voinici, Curteni, Viteji, Darabani, Seimeni, Arnauts, Graniceri, Pandurs, Dorobanti, Rosiori, Vanatori, the IAR-80, the R-2 and TR-85M1 Bizonul tanks, LAROM and more, 109 in all. You see them when you play Romania, and on a Romanian enemy's units when you select them. 95 of them already have Romanian icons painted for the mod (the Dacians, the voivodes' host, the principalities, the World Wars, today's army and armour, the aircraft, the navy); the last few (anti-aircraft, government leaders, civilians) follow.
- **Music:** playing Romania, George Enescu's Romanian Rhapsodies play among the game's own tracks: the slow, lyrical parts in the Ancient to Medieval Ages, the fast dances from the Gunpowder Age on. The recording (Silvestri, 1956) is free in the EU but not in the US, so it is not in the download; `romania/README.md` says where to get it and how to add it.
- **Buildings:** the game's Northern European style; no art of its own yet.
- **Leaders and cities:** computer players are named after Burebista, Decebalus, Mircea the Elder, Vlad the Impaler, Stephen the Great, Michael the Brave, Queen Marie and others; cities from Bucuresti, Sarmizegetusa and Alba Iulia on (no diacritics: the game's fonts lack them).
- In multiplayer every player needs the mod. Games saved before the mod was installed may not load with it.

## Tools for modders
- **Art_Test script** (`scenario\Scripts\Art_Test`): at game start it builds one of every building next to player 1. Keys: 0 = next age, 9 = previous age, 8 = build everything again. It is for checking building art.
- **Wear for new building art** (`roman-buildings/tools/weather.py`): AI-made buildings come out too clean next to the game's own. The script adds grime, rain streaks, fine grain, darker wall bases and deeper crevices to a finished sprite, without touching the player colour or the shadow. `python weather.py <sprite> <out.png> [strength]`; 0.7 is light, 1.0 medium (used for the Russian buildings), 1.5 is too much. Keep the clean sprite and always weather from it (`clean/` in each nation's folder).
- **Depth meshes for new building art** (`roman-buildings/tools/depth_mesh.py`): makes, for every sprite in `nation_art.ini`, a depth mesh that fits the new picture (`<name>z.bh3` next to the sprite, also copied to the game), plus a check picture in the mod's `work/`. Run it again after changing a sprite: `python depth_mesh.py [name ...]`.
- **3D route for the camera angle** (`tools/trellis/`): when the image generator draws a good building from the wrong angle, `run_trellis.py` turns the picture into a textured 3D model with TRELLIS.2 in a local ComfyUI, and `render_iso.py` renders it from the game's camera on magenta, ready for `make_sprite.py`. First used for the Nubian temple.
