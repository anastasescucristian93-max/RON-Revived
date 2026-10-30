# Translating a chunk

The rules for whoever translates a chunk (a person or an agent). The tools are in `tools\`; run them from that folder.

## The job

- Input: `work\chunks\<chunk>.json`, a list of texts: `{"id", "en", "file", "ref"?, "was"?}`.
  - `en`: the English text. Inline tags (`<BULLET/>`, `<SPACE/>`, `<LINE/>`, comments) are XML. `&amp;`, `&lt;` and `&gt;` are entities.
  - `file`: where the text is shown. `typenames.xml` holds names of units, buildings, technologies, wonders and ages. `options.xml` and `playerprofile.xml` hold buttons, commands and key names. `string_lookup.xml` and `translated_strings.xml` hold menus, messages, scenario and campaign texts. `help.xml` holds tooltips and the encyclopedia. `DLL` holds our mod's own panel and tooltip texts.
  - `ref`: the game's own German, Spanish, French and Italian for short texts. Use them to understand what a short label means, and as a size guide: the box on screen fits the longest of them, no more.
  - `was`: for our mods' texts, the English text the mod replaced. `ref` is then the game's translation of that old text: keep its terms and style.
- Output: `work\<code>\<chunk>.json`, one JSON object `{"<id>": "<translation>", ...}` with every id of the chunk, written as UTF-8.
- Check: `python merge.py check <code> <chunk>`. Fix everything it reports and run it again until it reports 0 problems.

## Rules

1. Keep every token exactly, the same number of times: `$NUMBER0`, `$NUM0`, `$TRIBE0`, `$PLAYERNAME0` (any `$NAME`), `#ICON27`, `%d`, `%s`, `%%`, `\n`, inline tags, and the marks `{` `}` `[` `]` `|`. Words in braces are highlighted (`{The Aztecs have the Power of Sacrifice.}`). Put the braces around the translated phrase. Tokens may move within the sentence where the grammar needs it.
2. Inside the text, write `&` as `&amp;` and `<` as `&lt;` (the text is XML content). Quotes and apostrophes need nothing.
3. Keep the line breaks and the `<BULLET/>` structure of long texts. The indentation inside them does not matter.
4. Labels, buttons, menu entries and names should be as short as the language allows: never longer than the longest `ref`, and close to the English when there is no `ref`. Prefer the usual short form of a game UI (infinitive or noun, abbreviations a player knows) over a full sentence.
5. Names: use the glossary `work\glossary\<code>.json` (English name -> name in your language) for every unit, building, technology, wonder and age, so a name is the same everywhere. When a name is not there, use the established name in your language for historical names (Colosseum, Great Wall, Napoleon), else translate it.
6. Leave nothing in English, except proper names that stay the same and words a game keeps in English in your language.
7. The game is Rise of Nations, a historical real-time strategy game, from the Ancient Age to the Information Age. The six resources are Food, Timber, Metal, Oil, Wealth and Knowledge. Use the gaming terms players of strategy games in your language know.

## Style per language

- `ru` Russian: formal «вы», lowercase. Standard Russian RTS terms. Use «ё» only where it is usual.
- `pt` Brazilian Portuguese: «você». Brazilian spelling and gaming terms.
- `pl` Polish: second person singular, as in Polish game localizations. Correct diacritics (ą ć ę ł ń ó ś ź ż).
- `tr` Turkish: formal «siz». Correct letters (ç ğ ı İ ö ş ü).
- `ro` Romanian: second person singular («tu»). Diacritics with the comma below (ș ț, not ş ţ), and ă â î.
- `de` `es` `fr` `it`: the style of the game's own translation (the `ref` texts).
