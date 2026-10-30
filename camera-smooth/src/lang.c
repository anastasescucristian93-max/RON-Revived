/*
 * Ten languages instead of five ([ui] more_languages), and the DLL's own texts in the chosen one.
 * Engine notes: camera-smooth/ENGINE_NOTES.md, "Languages".
 *
 * The exe's LocalizationManager knows German, English, Spanish, French and Italian (enum 0..4),
 * in four tables of five pointers: names (the options combo), Steam names, file suffixes and
 * short codes (the prefs key Language). XML::init appends the suffix to every XML file it opens
 * and falls back to the plain (English) file. The patches point every reader of the tables at
 * copies with five more languages (Russian .25, Brazilian Portuguese .22, Polish .21, Turkish .31,
 * Romanian .24: the Windows language ids, as the game's own) and raise the bounds from 4 / 5 to
 * 9 / 10. The translated files are translations\'s (mod_override\Data\*.xml.<id>).
 *
 * The DLL's texts (assistant panel, ranks, qol buttons, PLAY) go through lang_text, which looks
 * the English text up in mod_override\Data\revived_strings.xml<suffix>, and lang_string, which
 * makes the game String: ASCII through String(const char *) as before, anything else through
 * String(const wchar_t *), which keeps the pointer, so the wide copy is kept for the whole game.
 */
#include "ron.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VA_GET_LANGUAGE  0x00A42940u /* LocalizationManager::GetLanguage(void), cdecl */
#define VA_STRING_CTOR   0x00A1D660u /* String::String(const char *) */
#define VA_STRING_WCTOR  0x00A1EDD0u /* String::String(const wchar_t *), keeps the pointer */
#define VA_NAMES         0x00B1CA28u /* gLanguageNames */
#define VA_STEAM_NAMES   0x00B1CA40u /* gLanguageSteamNames */
#define VA_SUFFIXES      0x00B1CA54u /* gLanguageSuffix */
#define VA_SHORT_CODES   0x00B1CA80u /* gLanguageShortCodes */
#define LANGS            10
#define MAX_TEXTS        512

typedef int(__cdecl *get_language_t)(void);
typedef void(__attribute__((thiscall)) *str_ctor_t)(void *self, const char *s);
typedef void(__attribute__((thiscall)) *str_wctor_t)(void *self, const wchar_t *s);

static const wchar_t *k_names[LANGS] = { L"Deutsch", L"English", L"Español", L"Français", L"Italiano",
                                         L"Русский", L"Português (Brasil)", L"Polski", L"Türkçe", L"Română" };
static const char *k_steam[LANGS] = { "german", "english", "spanish", "french", "italian",
                                      "russian", "brazilian", "polish", "turkish", "romanian" };
static const wchar_t *k_suffix[LANGS] = { L".7", L"", L".10", L".12", L".16", L".25", L".22", L".21", L".31", L".24" };
static const wchar_t *k_short[LANGS] = { L"DE", L"EN", L"ES", L"FR", L"IT", L"RU", L"PT", L"PL", L"TR", L"RO" };

enum { BOUND_4, BOUND_5, TABLE };

typedef struct {
    UINT_PTR at;
    unsigned char op[3]; /* the instruction's first three bytes */
    int kind;            /* BOUND_4: cmp reg, 4; BOUND_5: cmp reg, 5; TABLE: [reg*4 + table] */
    UINT_PTR table;      /* TABLE: the exe's table the displacement names */
    const char *what;
} lpatch_t;

static const lpatch_t k_patches[] = {
    { 0x0083241Au, { 0x83, 0xFE, 0x04 }, BOUND_4, 0, "OptionsWinPlayer::setup_combos name bound" },
    { 0x0083241Fu, { 0x8B, 0x04, 0xB5 }, TABLE, VA_NAMES, "OptionsWinPlayer::setup_combos names" },
    { 0x008324B2u, { 0x83, 0xFE, 0x05 }, BOUND_5, 0, "OptionsWinPlayer::setup_combos loop" },
    { 0x00A27CDFu, { 0x83, 0xF9, 0x04 }, BOUND_4, 0, "XML::init suffix bound" },
    { 0x00A27CE4u, { 0x8B, 0x04, 0x8D }, TABLE, VA_SUFFIXES, "XML::init suffixes" },
    { 0x00A42830u, { 0x83, 0xF9, 0x04 }, BOUND_4, 0, "GetLanguageSuffix bound" },
    { 0x00A42835u, { 0x8B, 0x04, 0x8D }, TABLE, VA_SUFFIXES, "GetLanguageSuffix" },
    { 0x00A42876u, { 0x83, 0xF9, 0x04 }, BOUND_4, 0, "SetLanguage bound" },
    { 0x00A4287Fu, { 0x8B, 0x04, 0x8D }, TABLE, VA_SHORT_CODES, "SetLanguage" },
    { 0x00A42A10u, { 0x8B, 0x04, 0xBD }, TABLE, VA_SHORT_CODES, "GetLanguage" },
    { 0x00A42A7Fu, { 0x83, 0xFF, 0x05 }, BOUND_5, 0, "GetLanguage loop" },
    { 0x00A42AB0u, { 0x83, 0xF9, 0x04 }, BOUND_4, 0, "GetLanguageName bound" },
    { 0x00A42AB5u, { 0x8B, 0x04, 0x8D }, TABLE, VA_NAMES, "GetLanguageName" },
    { 0x00A42AE3u, { 0xFF, 0x34, 0xB5 }, TABLE, VA_STEAM_NAMES, "GetLanguageFromSteamName" },
    { 0x00A42AF5u, { 0x83, 0xFE, 0x05 }, BOUND_5, 0, "GetLanguageFromSteamName loop" },
    { 0x00A42B36u, { 0x83, 0xF9, 0x04 }, BOUND_4, 0, "SetDefaultLanguage bound" },
    { 0x00A42B3Fu, { 0x8B, 0x04, 0x8D }, TABLE, VA_SHORT_CODES, "SetDefaultLanguage" },
};
#define NPATCHES ((int)(sizeof k_patches / sizeof k_patches[0]))

static int g_enabled = 1;
static int g_installed;

/* the texts of the language loaded last: English (UTF-8) -> translation (UTF-8) */
static int g_lang = -2; /* -2: nothing loaded yet */
static DWORD g_checked;
static int g_n;
static char *g_en[MAX_TEXTS], *g_tr[MAX_TEXTS];

/* wide copies handed to String(const wchar_t *), kept for the whole game */
typedef struct wide_s {
    struct wide_s *next;
    char *utf8;
    wchar_t *wide;
} wide_t;
static wide_t *g_wide[256];

void lang_load_config(const char *ini_path)
{
    g_enabled = GetPrivateProfileIntA("ui", "more_languages", 1, ini_path);
}

static UINT_PTR table_of(UINT_PTR exe_table)
{
    switch (exe_table) {
    case VA_NAMES: return (UINT_PTR)k_names;
    case VA_STEAM_NAMES: return (UINT_PTR)k_steam;
    case VA_SUFFIXES: return (UINT_PTR)k_suffix;
    default: return (UINT_PTR)k_short;
    }
}

/* Called from install_hooks, before the game reads its prefs or opens an XML file. */
void lang_install(void)
{
    if (!g_enabled)
        return;
    for (int i = 0; i < NPATCHES; i++) {
        const lpatch_t *p = &k_patches[i];
        unsigned char *at = (unsigned char *)VA(p->at);
        if (memcmp(at, p->op, 3) || (p->kind == TABLE && *(UINT32 *)(at + 3) != (UINT32)VA(p->table))) {
            logf_("languages: bytes differ at %s (%08X), only the game's five languages", p->what, (unsigned)p->at);
            return;
        }
    }
    for (int i = 0; i < NPATCHES; i++) {
        const lpatch_t *p = &k_patches[i];
        unsigned char *at = (unsigned char *)VA(p->at);
        int len = p->kind == TABLE ? 4 : 1;
        unsigned char *field = at + (p->kind == TABLE ? 3 : 2);
        UINT32 table = (UINT32)table_of(p->table);
        unsigned char bound = p->kind == BOUND_4 ? LANGS - 1 : LANGS;
        DWORD old;
        VirtualProtect(field, len, PAGE_EXECUTE_READWRITE, &old);
        if (p->kind == TABLE)
            memcpy(field, &table, 4);
        else
            *field = bound;
        VirtualProtect(field, len, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    g_installed = 1;
    logf_("languages: %d in the options (5 more: Russian, Portuguese, Polish, Turkish, Romanian), %d patches in",
          LANGS, NPATCHES);
}

/* The languages the options offer, in the game's order (for the launcher's button, which runs
   in patriots.exe, where nothing is patched). */
int lang_count(void)
{
    return g_enabled ? LANGS : 5;
}

const wchar_t *lang_name(int i)
{
    return k_names[i];
}

const wchar_t *lang_short(int i)
{
    return k_short[i];
}

const char *lang_steam(int i)
{
    return k_steam[i];
}

static int current_language(void)
{
    if (!g_installed)
        return 1; /* English: the DLL's texts stay as written */
    return ((get_language_t)VA(VA_GET_LANGUAGE))();
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = n > 0 ? malloc(n + 1) : NULL;
    if (buf && fread(buf, 1, n, f) == (size_t)n) {
        buf[n] = 0;
    } else {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

/* &amp; &lt; &gt; &quot; &apos; in place */
static void unescape(char *s)
{
    static const struct { const char *ent; char c; } k[] = {
        { "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' }, { "&quot;", '"' }, { "&apos;", '\'' }
    };
    char *w = s;
    for (const char *r = s; *r;) {
        int done = 0;
        if (*r == '&')
            for (int i = 0; i < 5 && !done; i++) {
                size_t n = strlen(k[i].ent);
                if (!strncmp(r, k[i].ent, n)) {
                    *w++ = k[i].c;
                    r += n;
                    done = 1;
                }
            }
        if (!done)
            *w++ = *r++;
    }
    *w = 0;
}

/* <STRING en="English">translation</STRING> entries of revived_strings.xml<suffix> */
static void load_texts(int lang)
{
    for (int i = 0; i < g_n; i++) {
        free(g_en[i]);
        free(g_tr[i]);
    }
    g_n = 0;
    if (lang < 0 || lang >= LANGS || !*k_suffix[lang])
        return;
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%smod_override\\Data\\revived_strings.xml%ls", g_dir, k_suffix[lang]);
    char *buf = read_file(path);
    if (!buf) {
        logf_("languages: %s not found, the mod's own texts stay English", path);
        return;
    }
    for (char *p = buf; g_n < MAX_TEXTS && (p = strstr(p, "<STRING en=\"")) != NULL;) {
        char *en = p + 12, *q = strchr(en, '"');
        char *body = q ? strchr(q, '>') : NULL;
        char *end = body ? strstr(body, "</STRING>") : NULL;
        if (!end)
            break;
        *q = 0;
        *end = 0;
        g_en[g_n] = strdup(en);
        g_tr[g_n] = strdup(body + 1);
        unescape(g_en[g_n]);
        unescape(g_tr[g_n]);
        g_n++;
        p = end + 9;
    }
    free(buf);
    logf_("languages: %d of the mod's texts in %ls", g_n, k_names[lang]);
}

/* The chosen language's version of an English text of the DLL (UTF-8), or the text itself. */
const char *lang_text(const char *en)
{
    DWORD now = GetTickCount();
    if (g_lang == -2 || now - g_checked > 1000) {
        g_checked = now;
        int lang = current_language();
        if (lang != g_lang) {
            g_lang = lang;
            load_texts(lang);
        }
    }
    for (int i = 0; i < g_n; i++)
        if (!strcmp(g_en[i], en))
            return g_tr[i];
    return en;
}

static const wchar_t *wide_of(const char *utf8)
{
    unsigned h = 5381;
    for (const unsigned char *p = (const unsigned char *)utf8; *p; p++)
        h = h * 33 + *p;
    wide_t **slot = &g_wide[h & 255];
    for (wide_t *w = *slot; w; w = w->next)
        if (!strcmp(w->utf8, utf8))
            return w->wide;
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wide_t *w = malloc(sizeof *w);
    if (!w || n <= 0)
        return L"";
    w->utf8 = strdup(utf8);
    w->wide = malloc(n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w->wide, n);
    w->next = *slot;
    *slot = w;
    return w->wide;
}

/* Builds a game String from UTF-8 text (close it with String::close as usual). */
void lang_string(void *str, const char *utf8)
{
    const unsigned char *p = (const unsigned char *)utf8;
    while (*p && *p < 0x80)
        p++;
    if (!*p)
        ((str_ctor_t)VA(VA_STRING_CTOR))(str, utf8);
    else
        ((str_wctor_t)VA(VA_STRING_WCTOR))(str, wide_of(utf8));
}
