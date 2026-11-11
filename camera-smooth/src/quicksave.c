/*
 * Quicksave: one key ([qol] quicksave_key, Ctrl+S by default) saves the game at once, without the
 * Save Game window, as "Quicksave" in the game's Saves folder (overwritten each time). Single
 * player and campaign only: in multiplayer and replays the key does what it did before.
 * Engine notes: camera-smooth/ENGINE_NOTES.md, "Quicksave".
 *
 * The game's own "Save Game" key command (Console::exec_keymap case 0x67, unbound by default)
 * opens the window through SaveGame::save_game_screen, which calls the virtual
 * SaveGame::save_game(name, 0). The autosave (Game::do_frame) and the save on quitting a Conquer
 * the World battle call it with 1 (overwrite without asking). This does the same with the
 * Saves folder (PlayerProfile::get_save_directory) + "Quicksave"; save_game adds ".SVX". In a
 * Conquer the World battle (game+0x822 & 2) save_game itself writes the campaign header and
 * ties the save to the conquest, as a save from the menu does.
 *
 * The key comes through heal.c's Console::on_key_down hook (quicksave_key, before the others).
 */
#include "ron.h"
#include <string.h>

#define VA_GAMEC         0x00C061E8u /* const Game *: +0x20, +0x820..+0x822 flags */
#define VA_IFACE_CURRENT 0x00E39964u /* IFaceMainBase * MiscAccess::iface_current */
#define VA_IS_VISIBLE    0x00A54740u /* Window::is_visible(void) */
#define VA_SAVE_GAME_OBJ 0x00C12BA4u /* struct SaveGame save_game; vtable +8 = save_game(const String &, int) */
#define VA_PROFILE       0x00ECD030u /* struct PlayerProfile player_profile */
#define VA_SAVE_DIR      0x005D09A0u /* PlayerProfile::get_save_directory(void) -> String (hidden out), ret 4 */
#define VA_STR_APPEND_W  0x00A1DAD0u /* String::operator+=(const wchar_t *) */
#define VA_STR_AS_CHAR   0x0042D710u /* String::str_as_char(void) const */
#define VA_STRING_CLOSE  0x00A1CF40u
#define VA_MESSAGE_WIN   0x00C06208u /* MessageWin & MiscAccess::message_win */
#define VA_MSG_COLOR     0x007EA1C0u /* MessageWin::set_message_color(const Color &): +0x580, 12 bytes */
#define VA_SIMPLE_MSG    0x007E9A50u /* MessageWin::add_simple_message(const String &, int, int) */
#define VA_WHITE         0x00C8D26Cu /* class Color WHITE */

#define KEY_SHIFT 0x10000
#define KEY_CTRL  0x20000
#define KEY_ALT   0x40000

typedef int(__attribute__((thiscall)) *fn0_t)(void *self);
typedef void *(__attribute__((thiscall)) *save_dir_t)(void *self, void *out);
typedef void(__attribute__((thiscall)) *append_w_t)(void *self, const wchar_t *s);
typedef const char *(__attribute__((thiscall)) *as_char_t)(void *self);
typedef int(__attribute__((thiscall)) *save_game_t)(void *self, void *name, int overwrite);
typedef void(__attribute__((thiscall)) *msg_color_t)(void *self, const void *color);
typedef void(__attribute__((thiscall)) *simple_msg_t)(void *self, void *str, int a, int b);

int g_quicksave_key; /* VK | KEY_SHIFT / KEY_CTRL / KEY_ALT, 0 = off */

/* "Ctrl+S", "F5", "Shift+F9": optional Ctrl+ / Shift+ / Alt+, then a letter, a digit or F1..F12. */
static int parse_key(const char *s)
{
    int mods = 0;
    for (;;) {
        if (_strnicmp(s, "Ctrl+", 5) == 0)
            mods |= KEY_CTRL, s += 5;
        else if (_strnicmp(s, "Shift+", 6) == 0)
            mods |= KEY_SHIFT, s += 6;
        else if (_strnicmp(s, "Alt+", 4) == 0)
            mods |= KEY_ALT, s += 4;
        else
            break;
    }
    int c = s[0] >= 'a' && s[0] <= 'z' ? s[0] - 32 : s[0];
    if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) && !s[1])
        return mods | c;
    if (c == 'F' && s[1] >= '1' && s[1] <= '9') {
        int n = s[1] - '0';
        if (s[2] >= '0' && s[2] <= '9' && !s[3])
            n = n * 10 + s[2] - '0';
        else if (s[2])
            return 0;
        if (n >= 1 && n <= 12)
            return mods | (VK_F1 + n - 1);
    }
    return 0;
}

void quicksave_load_config(const char *ini_path)
{
    char key[32];
    GetPrivateProfileStringA("qol", "quicksave_key", "Ctrl+S", key, sizeof key, ini_path);
    g_quicksave_key = key[0] ? parse_key(key) : 0;
    if (key[0] && !g_quicksave_key)
        logf_("quicksave: [qol] quicksave_key \"%s\" not understood, no quicksave", key);
}

/* The same checks as the game's own Save Game key (exec_keymap case 0x67), plus no multiplayer. */
static int can_save(void)
{
    unsigned char *g = *(unsigned char **)VA(VA_GAMEC);
    void *iface = *(void **)VA(VA_IFACE_CURRENT);
    if (!g || !iface)
        return 0;
    if (g[0x820] & (0x04 | 0x10 | 0x40)) /* multiplayer, replay, 0x40 as the game's key */
        return 0;
    if (*(unsigned *)(g + 0x20) & 0x800 || g[0x822] & 8)
        return 0;
    return ((fn0_t)VA(VA_IS_VISIBLE))(iface) != 0;
}

static void message(const char *en)
{
    char *mw = *(char **)VA(VA_MESSAGE_WIN);
    if (!mw)
        return;
    unsigned char str[64] = { 0 }, old[12];
    memcpy(old, mw + 0x580, sizeof old);
    ((msg_color_t)VA(VA_MSG_COLOR))(mw, (void *)VA(VA_WHITE));
    lang_string(str, lang_text(en));
    ((simple_msg_t)VA(VA_SIMPLE_MSG))(mw, str, -3, 0); /* the arguments of a chat line */
    ((fn0_t)VA(VA_STRING_CLOSE))(str);
    memcpy(mw + 0x580, old, sizeof old);
}

/* From heal.c's Console::on_key_down hook: 1 = the key was the quicksave and it is handled. */
int quicksave_key(int key)
{
    if (!g_quicksave_key || key != g_quicksave_key || !can_save())
        return 0;
    unsigned char name[64] = { 0 };
    ((save_dir_t)VA(VA_SAVE_DIR))((void *)VA(VA_PROFILE), name);
    const char *dir = ((as_char_t)VA(VA_STR_AS_CHAR))(name);
    size_t n = dir ? strlen(dir) : 0;
    ((append_w_t)VA(VA_STR_APPEND_W))(name, n && (dir[n - 1] == '\\' || dir[n - 1] == '/') ? L"Quicksave" : L"\\Quicksave");
    void *obj = (void *)VA(VA_SAVE_GAME_OBJ);
    int r = ((save_game_t)(*(void ***)obj)[2])(obj, name, 1);
    ((fn0_t)VA(VA_STRING_CLOSE))(name);
    logf_("quicksave: SaveGame::save_game -> %d (0 = saved)", r);
    message(r == 0 ? "Game saved (Quicksave)" : "Quicksave failed");
    return 1;
}
