# Citizen icons: engine notes

## Where an icon comes from

- `unit_graphics.xml` names each graphics entry's icon by page and cell: `tex_id="TEX_UNITS_0" tex_col="7" tex_row="8"`. TEX_UNITS_0/1/2 are `art\iface_units_01..03.tga` (512 x 512; cells 61 x 39 on a 62 x 40 step, 8 x 12; the `_half` pages 30 x 19 on 31 x 20).
- The entries are per unit style and icon tier: `CITIZENS-<STYLE>-AGE0` (ages 1-3), `-AGE3` (4-5), `-AGE5` (6-8), each with a `-FEMALE` twin. The styles are DEFAULT (European), ARAB, ASIAN, INDIA, NA, AMERICAN; the Korean Citizen has its own `CITIZENSKOREAN-DEFAULT-AGE*` entries. In the game every one of the 14 AGE5 entries uses the same cell, TEX_UNITS_0 (7, 8).
- `GraphicPieces::update_all_icons` fills each player's icon slots from the player's unit style (romania/ENGINE_NOTES.md), so pointing one style's entries at another cell changes the icon for that style's nations only.
- There is no separate icon for ages 7-8: AGE5 covers ages 6, 7 and 8.

## Finding a free cell

Cells are also named in the rules files in element form, not only in the graphics files' attributes:

```
<TEX_ID>TEX_UNITS_2</TEX_ID>
<TEX_COL>6</TEX_COL>
<TEX_ROW>10</TEX_ROW>
```

(`buildingrules.xml`, `craftrules.xml`, `resourcerules.xml`, `techrules.xml`). A search of only `tex_id="..."` attributes takes the Heal (6, 10) and Entrench (7, 10) cells of TEX_UNITS_2 for free. Search both forms in the game's `Data\*.xml`, in every `*\mod_override\Data\*.xml` and in the game's other XML (scenarios, conquest), which name a few more cells.

Cells named by no file (2026-11-08, after nuke-fx's Interceptor took TEX_UNITS_2 (4, 11)):

- TEX_UNITS_0: (3, 8), an old biplane picture (the Biplane itself uses (2, 8)). Now the Latin American Citizen.
- TEX_UNITS_1: (5, 10), an armoured car picture. Still free.
- TEX_UNITS_2: (7, 9), (5, 11), (6, 11), (7, 11), flat grey. Now the African, East Asian, South Asian and North American Citizens. TEX_UNITS_2 is full after this.

The only icon references by pixel position are `options.xml` (an `<OPTION tex_x="0" tex_y="0">`) and the Goody Box in `itemrules.xml` (TEX_SPECIAL_RESOURCES), so none points into these cells.

For more icons, a new page is the way: Romania loads its own pages with extra `<IFACETEXLOAD id="TEX_ROMANIA_0" ...>` lines in `interface.xml`, and its DLL code (`romania.c`) points the icon slots at them. A `unit_graphics.xml` entry naming such a page directly was not tried.
