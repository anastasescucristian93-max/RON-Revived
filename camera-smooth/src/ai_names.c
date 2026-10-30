/*
 * Computer players named after their nation's leaders ("Julius Caesar", "Queen Chabi")
 * instead of "Computer".
 *
 * The game has the code for it: Player::generate_name picks a <LEADER> from the tribe's
 * XML (tribes\*.xml). Game::init_rules_and_teams calls it for every computer player, yet in
 * skirmish games most of them still show "Computer", the setup screen's placeholder. So the
 * first time a player's name is shown in a game (Player::nameWithPlatformSymbol, reached
 * through LeaderData::get_name by the score list and the rest of the interface), a player
 * still called "Computer" gets generate_name. The name is display only.
 *
 * Why: init_rules_and_teams names the players only when its first argument is 0, and
 * Game::run passes game+0x544 != 0 there. When it is set, every computer keeps "Computer",
 * and the score list does not go through nameWithPlatformSymbol. So the call in Game::run
 * is also wrapped: right after it, every computer player still called "Computer" is named.
 *
 * The score list showing "Computer" at the start of a game is not this: StatWin::
 * setup_normal_entries writes a placeholder (string table, not the player's name) for an
 * enemy you have not met yet, and the real name once you meet them. That is the game's
 * rule (the leader's name would give away the nation). The loading screen does the same
 * (LoadingWin::draw_other_player_info writes "Computer" and a placeholder for the nation).
 *
 * reveal_ai_players lifts that rule in single player games: seven short jumps of the game
 * (two for the name and two for the nation on the loading screen, two for the name and one
 * for the nation in the score list) are switched at the start of each game, so every
 * computer shows its leader and nation from the start. Display only.
 */
#include "ron.h"
#include <string.h>

#define VA_NAME_SYMBOL   0x006EE1D0u /* Player::nameWithPlatformSymbol() const, returns String, ret 4 */
#define VA_GENERATE_NAME 0x006EE3C0u /* Player::generate_name() */
#define VA_GAME_PTR      0x00C061ECu /* Game* */
#define VA_STRING_AS_CHAR 0x0042D710u
#define VA_INIT_RULES    0x00589BB0u /* Game::init_rules_and_teams(int, int), ret 8 */
#define AT_INIT_RULES    0x00584885u /* its call in Game::run */

#define GAME_PLAYERS 0x44 /* Player[8], 0x8C each */
#define GAME_SEED    0x10 /* per-game random base generate_name uses */
#define PLAYER_SIZE  0x8C
#define PL_FLAGS     0x30 /* word: bit 0 in the game, bit 2 human */
#define PL_NAME      0x40 /* String */

typedef void *(__attribute__((thiscall)) *name_t)(void *self, void *out);
typedef void(__attribute__((thiscall)) *fn_v_t)(void *self);
typedef const char *(__attribute__((thiscall)) *as_char_t)(void *self);

#define GAME_FLAGS 0x820 /* byte, bit 2: multiplayer game */

int g_ai_names_enabled;
int g_reveal_ai;
void *g_orig_name_symbol;

void ai_names_load_config(const char *ini_path)
{
    g_ai_names_enabled = GetPrivateProfileIntA("qol", "ai_leader_names", 1, ini_path);
    g_reveal_ai = GetPrivateProfileIntA("qol", "reveal_ai_players", 1, ini_path);
}

/* The game's tests that hide an enemy computer you have not met: each is a short je that
   skips the placeholder when the game type is 3 (or, the last one, adds the nation once met).
   Revealed: jmp (always the real text), or two nops for the last one (always the nation). */
static const struct { UINT_PTR at; unsigned char reveal; } k_reveal[] = {
    { 0x008348BBu, 0xEB }, /* loading screen, name (layout with teams) */
    { 0x00834DD8u, 0xEB }, /* loading screen, name (other layout) */
    { 0x00834A07u, 0xEB }, /* loading screen, nation (layout with teams) */
    { 0x00834F16u, 0xEB }, /* loading screen, nation (other layout) */
    { 0x007ECF6Cu, 0xEB }, /* score list, name */
    { 0x007EDDADu, 0xEB }, /* score list, name (other layout) */
    { 0x007EDE15u, 0x90 }, /* score list, " (nation)" after the name */
};
#define N_REVEAL (int)(sizeof k_reveal / sizeof k_reveal[0])
static unsigned char g_reveal_orig[N_REVEAL][2];
static int g_reveal_ok, g_revealed;

static void reveal_set(int on)
{
    if (!g_reveal_ok || on == g_revealed)
        return;
    for (int i = 0; i < N_REVEAL; i++) {
        unsigned char *at = (unsigned char *)VA(k_reveal[i].at);
        unsigned char b[2] = { g_reveal_orig[i][0], g_reveal_orig[i][1] };
        if (on) {
            b[0] = k_reveal[i].reveal;
            if (b[0] == 0x90)
                b[1] = 0x90;
        }
        DWORD old;
        VirtualProtect(at, 2, PAGE_EXECUTE_READWRITE, &old);
        memcpy(at, b, 2);
        VirtualProtect(at, 2, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    g_revealed = on;
    logf_("ai names: computer players %s", on ? "shown from the start" : "hidden until met");
}

static void reveal_prepare(void)
{
    for (int i = 0; i < N_REVEAL; i++) {
        unsigned char *at = (unsigned char *)VA(k_reveal[i].at);
        if (at[0] != 0x74) {
            logf_("ai names: reveal mismatch at %08x, computer players stay hidden until met",
                  (unsigned)k_reveal[i].at);
            return;
        }
        memcpy(g_reveal_orig[i], at, 2);
    }
    g_reveal_ok = 1;
}

__attribute__((used)) void *c_name_symbol(char *player, void *out)
{
    static int seen_seed[8], seen[8];
    char *game = *(char **)VA(VA_GAME_PTR);
    if (game) {
        int i = (int)(player - (game + GAME_PLAYERS)) / PLAYER_SIZE;
        int seed = *(int *)(game + GAME_SEED);
        if (i >= 0 && i < 8 && player == game + GAME_PLAYERS + i * PLAYER_SIZE &&
            (!seen[i] || seen_seed[i] != seed)) {
            seen[i] = 1;
            seen_seed[i] = seed;
            unsigned short flags = *(unsigned short *)(player + PL_FLAGS);
            char before[64];
            strncpy(before, ((as_char_t)VA(VA_STRING_AS_CHAR))(player + PL_NAME), sizeof before - 1);
            before[sizeof before - 1] = 0;
            /* the setup screen's placeholder for computer players */
            if ((flags & 1) && !strcmp(before, "Computer"))
                ((fn_v_t)VA(VA_GENERATE_NAME))(player);
            logf_("ai names: player %d flags %04x \"%s\" -> \"%s\"", i, flags, before,
                  ((as_char_t)VA(VA_STRING_AS_CHAR))(player + PL_NAME));
        }
    }
    return ((name_t)g_orig_name_symbol)(player, out);
}

static const char *name_of(char *player) { return ((as_char_t)VA(VA_STRING_AS_CHAR))(player + PL_NAME); }

typedef int(__attribute__((thiscall)) *init_rules_t)(void *self, int a, int b);

/* Names every computer player still called "Computer"; returns how many. */
static int name_computers(char *game, const char *when)
{
    int n = 0;
    for (int i = 0; i < 8; i++) {
        char *player = game + GAME_PLAYERS + i * PLAYER_SIZE;
        unsigned short flags = *(unsigned short *)(player + PL_FLAGS);
        if ((flags & 1) && !(flags & 4) && !strcmp(name_of(player), "Computer")) {
            ((fn_v_t)VA(VA_GENERATE_NAME))(player);
            logf_("ai names: player %d named \"%s\" %s", i, name_of(player), when);
            n++;
        }
    }
    return n;
}

__attribute__((thiscall)) static int init_rules(char *game, int a, int b)
{
    int r = ((init_rules_t)VA(VA_INIT_RULES))(game, a, b);
    if (g_ai_names_enabled)
        name_computers(game, "at the start");
    /* single player only: in multiplayer it would show one player what the others cannot see */
    if (g_reveal_ai)
        reveal_set(!(game[GAME_FLAGS] & 4));
    return r;
}

void hk_name_symbol(void);

__asm__(".intel_syntax noprefix\n"
        /* Player::nameWithPlatformSymbol: ecx = the Player, [esp+4] = the String to fill */
        ".globl _hk_name_symbol\n"
        "_hk_name_symbol:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+36]\n"
        "    push ecx\n"
        "    call _c_name_symbol\n"
        "    add esp, 16\n"
        "    mov [ebx+28], eax\n" HOOK_LEAVE
        "    ret 4\n"
        ".att_syntax prefix\n");

void ai_names_install(void)
{
    if (!g_ai_names_enabled && !g_reveal_ai)
        return;
    if (g_reveal_ai)
        reveal_prepare();
    unsigned char *call = (unsigned char *)VA(AT_INIT_RULES);
    if (call[0] == 0xE8 && (UINT_PTR)(call + 5 + *(INT32 *)(call + 1)) == VA(VA_INIT_RULES)) {
        DWORD old;
        VirtualProtect(call, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(call + 1) = (INT32)((UINT_PTR)init_rules - (UINT_PTR)(call + 5));
        VirtualProtect(call, 5, old, &old);
        logf_("ai names: start of game wrapped");
    } else
        logf_("ai names: Game::run call mismatch, names only on first display, computer players hidden until met");
    if (!g_ai_names_enabled)
        return;
    /* push ebp / mov ebp,esp / push -1 | push offset ehhandler */
    static const unsigned char sig[6] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
    unsigned char *at = (unsigned char *)VA(VA_NAME_SYMBOL);
    unsigned char *mem = VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem || memcmp(at, sig, sizeof sig) != 0) {
        logf_("ai names: signature mismatch, computer players keep their names");
        return;
    }
    g_orig_name_symbol = hook_trampoline(at, 5, mem);
    hook_jmp5(at, hk_name_symbol);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("hook nameWithPlatformSymbol (leader names for computer players) installed");
}
