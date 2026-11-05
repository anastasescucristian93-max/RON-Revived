RON REVIVED for Rise of Nations: Extended Edition (Steam)
========================================================

A mod pack: a smoother camera and interface, new nation art for the buildings, Romania as a
25th nation, and new gameplay (city districts, Airdrop, Field Fortifications, the General's
Heal, missile defence against nukes, veteran ranks and more). CHANGES.md lists every change.

The game's own files are not modified: everything goes next to them, and uninstall.bat takes it
all away again.


INSTALL
-------
1. Extract the whole zip (right-click, Extract All). Do not run it from inside the zip.
2. Close the game.
3. Double-click install.bat. The game is found through Steam; if it is not, paste the game
   folder (the one with riseofnations.exe) when asked.
4. Start the game from Steam as usual.

UPDATES: when a new version is out, the game's launcher shows it, with an Update button. It
downloads only what changed, installs it and starts the game again. (Turn it off with check=0
under [update] in ron_camera.ini.) You can also install a new version by hand, the same way
over the old one. Either way your ron_camera.ini (your settings) is kept; the new defaults are
written next to it as ron_camera.default.ini.

EXTRAS: optional content (the nation music, and more later) is not in this zip. Click the tray
on the left edge of the game's launcher (EXTRAS) to slide it out, and download only what you
want; Remove takes an extra out again. Extras you installed stay when you update the pack.

Manual install: copy everything inside the game\ folder into the game folder
(...\steamapps\common\Rise of Nations), next to riseofnations.exe, and say yes to merging the
mod_override and nation_art folders.


UNINSTALL
---------
Double-click uninstall.bat. It removes the files listed in RON_Revived_files.txt and puts back
any game file the install had replaced (kept in RON_Revived_backup\).


SETTINGS
--------
ron_camera.ini in the game folder: camera speeds, the interface, and every feature can be turned
off one by one (each with a comment). The other .ini files (airdrop, entrench, data_center,
romania, nation_art) hold the mod's own data; leave them as they are.


MULTIPLAYER
-----------
The pack changes the game's rules (units, buildings, techs, Romania). In a multiplayer game
every player needs the same version of the pack, with the same gameplay settings in
ron_camera.ini. Players without it, or with another version, will go out of sync.


GOOD TO KNOW
------------
- Works with the Steam version built 2024-06-20. If the game gets an update, the camera and
  gameplay part turns itself off (see ron_camera.log) until a new version of the pack is out.
- Not made to run together with other mods that change the game's rules: turn them off in
  the game's Mods menu.
- Romania's music (George Enescu's Romanian Rhapsodies) is not included: the recording is free
  in the EU but not in the US. Romania plays with the game's own music.
- ron_camera.log in the game folder shows what the mod did; send it along with a bug report.
- Some antivirus tools flag winmm.dll mod files (the way the mod loads) as false positives.


FEEDBACK
--------
Downloads, reviews, bug reports and ideas: https://github.com/anastasescucristian93-max/RON-Revived
- Reviews:     https://github.com/anastasescucristian93-max/RON-Revived/discussions/1
- Bug reports: the Issues tab (attach ron_camera.log from the game folder)
Liked it? A star on the GitHub page helps other players find it.
