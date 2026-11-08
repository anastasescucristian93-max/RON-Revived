/* Shared between the modules of the DLL. */
#pragma once
#include <windows.h>

extern UINT_PTR g_delta; /* actual exe base - preferred base (ASLR) */
#define VA(a) ((UINT_PTR)(a) + g_delta)

extern char g_dir[MAX_PATH]; /* DLL folder (= game folder), with trailing backslash */

void logf_(const char *fmt, ...);
float ini_float(const char *path, const char *section, const char *key, float def);

/* lang.c (ten languages; the DLL's texts in the chosen one) */
void lang_load_config(const char *ini_path);
void lang_install(void);
const char *lang_text(const char *en);       /* UTF-8 translation of an English text, or the text */
void lang_string(void *str, const char *utf8); /* a game String from UTF-8 text */
int lang_count(void);                        /* 10, or the game's 5 with more_languages=0 */
const wchar_t *lang_name(int i);             /* i in the game's order: DE EN ES FR IT RU PT PL TR RO */
const wchar_t *lang_short(int i);            /* the prefs code, "DE" ... */
const char *lang_steam(int i);               /* Steam's name, "german" ... */

/* fog.c */
void fog_load_config(const char *ini_path);
void fog_init(void);
extern int g_fog_enabled;
void hk_fill_fog_texcoords(void);
void hk_invalidate_fog_cache(void);
extern void *g_orig_fill_fog_texcoords;
extern void *g_orig_invalidate_fog_cache;

/* nation_art.c */
extern int g_nation_art_enabled;
extern void *g_orig_draw_build;
void nation_art_load_config(const char *dir);
void hk_draw_build(void);

/* build_grid.c */
extern int g_grid_enabled;
extern void *g_orig_draw_template;
void grid_load_config(const char *ini_path);
void hk_draw_template(void);

/* subpixel.c */
extern int g_subpixel_enabled;
extern int g_dist_patch_applied;
void subpixel_load_config(const char *ini_path);
void subpixel_begin(const char *chad, const char *rdata);
void subpixel_end(void);
void subpixel_apply(float *verts, int n);

/* ron_camera.c: hooks shared by several features */
extern int g_hook_draw_build;   /* nation art || subpixel */
extern int g_hook_init_display; /* ui scale || subpixel || postfx (d3dgl hooks) */

/* ui_scale.c */
extern int g_ui_scale_enabled;
extern void *g_orig_init_display_quick;
extern void *g_orig_iface_uvs;
void ui_scale_load_config(const char *ini_path);
void hk_init_display_quick(void);
void hk_ui_iface_uvs(void);
float ui_scale_now(void);

/* qol.c */
extern int g_qol_enabled;
extern int g_show_hp;
extern int g_group_selection;
void qol_load_config(const char *ini_path);
void qol_create_buttons(void *iface);
int qol_button_clicked(void *button);
void qol_tick(double now);

void qol_install(void);
int qol_military_row(const void *build_type);
int qol_button_redraw(void *button);
void *qol_iface_tex(const char *name); /* an IFACETEX of interface.xml's IFACEMAINBASE block */

/* panel.c */
extern int g_panel_enabled;
void panel_load_config(const char *ini_path);
void panel_install(void);

/* override.c */
extern int g_override_enabled;
void override_load_config(const char *ini_path);
void override_install(void);
void override_redirect(const char *rel, const char *path);
void *hook_export(HMODULE mod, const char *name, void *repl);

/* launcher.c (patriots.exe, the launcher started before the game) */
#define LAUNCHER_TIMESTAMP 0x597669EDu
void launcher_install(void);
void *launcher_patch_import(HMODULE module, const char *func, void *repl);
DWORD *launcher_load_bgra(const char *name, int straight, int *w, int *h); /* launcher_res\<name> */
int launcher_picture_rect(RECT *r); /* the picture's panel in the background window */
/* launcher_lang.c (the launcher's language button) */
void launcher_lang_install(const char *res_dir, int scale_percent, const WCHAR *font);
void launcher_lang_attach(HWND dlg); /* the controls' dialog, once laid out */
void launcher_sound(int which);      /* launcher.c: 0 = the mouse came onto a button, 1 = pressed */
void launcher_text(const char *key, const WCHAR *def, WCHAR *out, int size); /* in the chosen language */
/* launcher_update.c (the Update plate when a new version is out) */
void launcher_update_install(const char *res_dir, int scale_percent, const WCHAR *font);
void launcher_update_attach(HWND dlg);
void launcher_update_relabel(void); /* the language changed */
int launcher_http_get(const char *url, char *out, int size); /* `url` into `out`; the bytes read, or -1 */
/* the list of the latest release, for the updater and the extras (package/build_package.py) */
#define RON_REVIVED_INDEX "https://github.com/anastasescucristian93-max/RON-Revived/releases/latest/download/RON_Revived_update.txt"
/* launcher_extras.c (the Extras tray: optional downloads) */
void launcher_extras_install(const char *res_dir, int scale_percent, const WCHAR *font);
void launcher_extras_attach(HWND dlg);
void launcher_extras_relabel(void);

/* kills.c */
extern int g_kills_enabled;
void kills_load_config(const char *ini_path);
void kills_install(void);
extern int g_sel_kills_row; /* selpanel.c keeps the group grid's top row for the kills */
void kills_draw_count(char *panel, int kills, int right, int y, int h); /* skull + number, ending at right */
/* hud.c */
void hud_load_config(const char *ini_path);
void hud_install(void);
float hud_text_scale(void); /* the numbers' enlargement, inside the panel's and counters' drawing */
int hud_number(int value, int x, int y, int h, unsigned color, int draw); /* real-font digits; width, 0 = none */
/* respanel.c (the resource panel's worker bars, market on the right) */
void respanel_load_config(const char *ini_path);
void respanel_install(void);
/* flags.c */
extern int g_flags_enabled;
void flags_load_config(const char *ini_path);
void flags_install(void);
void *flags_tex(int tribe);
int flags_nation_combo(char *combo);
void flags_draw(void *tex, const short *rect, unsigned color);
void flags_draw_bg(const short *rect, unsigned color);
/* nation_grid.c */
extern int g_ngrid_enabled;
void ngrid_load_config(const char *ini_path);
void ngrid_install(void);
/* ai_names.c */
extern int g_ai_names_enabled;
extern int g_reveal_ai;
void ai_names_load_config(const char *ini_path);
void ai_names_install(void);
/* assistant.c */
extern int g_assist_enabled;
void assistant_load_config(const char *ini_path);
void assistant_install(void);
int assistant_kinds(void);
const char *assistant_kind_label(int k);
int assistant_allowed(int k);
void assistant_set_allowed(int k, int on);
int assistant_reserve(void);
void assistant_set_reserve(int v);
/* assist_orders.c */
void assist_orders_load_config(const char *ini_path);
void assist_orders_install(void);
/* assist_panel.c */
void assist_panel_create(char *iface, char *parent);
void assist_panel_toggle(char *parent);
int assist_panel_is(void *btn);
void assist_panel_click(void);
int assist_panel_redraw(void *btn);
/* armageddon.c */
extern int g_arm_enabled;
void armageddon_load_config(const char *ini_path);
void armageddon_install(void);

/* corpses.c (dead units and wrecks last longer; simulation) */
void corpses_load_config(const char *ini_path);
void corpses_install(void);
/* animfix.c (zero-length animations no longer freeze the game) */
void animfix_install(void);
extern int g_zoom_trace, g_debug_viewports; /* [debug] in ron_camera.ini */
int arm_type_blocked(int type);
/* nonukes.c (the Nukes slider of the game setup: None, a count, Unlimited) */
extern int g_nonukes_enabled;
void nonukes_load_config(const char *ini_path);
void nonukes_install(void);
int nonukes_type_blocked(int type); /* datacenter.c's type_eligible hook: 0 = not offered */
int nonukes_launch_full(void);       /* armageddon.c's can_nuke: the chosen count is used up */
void nonukes_frame(void);            /* airdrop.c's Objects::process_all: the Armageddon constants */
int nukefx_nukes_in_flight(void);    /* nukefx.c: Nuclear Missiles and ICBMs in the air now */
/* nukefx.c (a nuclear blast's white flash fades with its distance from the screen centre) */
void nukefx_load_config(const char *ini_path);
void nukefx_install(void);
extern int g_nuke_sim_enabled;
void nukefx_frame(void);
void nukefx_make_options(char *b);
int nukefx_is_option(int code, int type);
int nukefx_charges(void);
int nukefx_reach_ring(int type);
int nukefx_drops_offer(int type);
/* notices.c */
void notices_load_config(const char *ini_path);
void notices_install(void);
/* minimap.c (the minimap at the top right while the interface is folded, in three sizes) */
#define MINIMAP_BUTTON 30 /* the size button, interface units */
extern int g_minimap_timer_top; /* the game's clocks start at this y (45, or under the minimap) */
void minimap_load_config(const char *ini_path);
void minimap_install(void);
void minimap_tick(void);
int minimap_button_pos(int *x, int *y); /* screen, interface units; 0 = no button now */
void minimap_cycle_size(void);
int minimap_frame_wanted(void); /* the steel frame round the folded minimap is on */
void minimap_draw_frame(void);  /* from rankfx.c's Console::on_redraw, under the minimap */
/* selpanel.c (the full selection panel while the interface is folded, grouped by kind) */
void selpanel_load_config(const char *ini_path);
void selpanel_install(void);
/* survival.c */
void survival_load_config(const char *ini_path);
void survival_install(void);
/* defenseramp.c */
void defenseramp_load_config(const char *ini_path);
void defenseramp_install(void);
/* attrition.c */
void attrition_load_config(const char *ini_path);
void attrition_install(void);
/* datacenter.c (data-center mod: the Data Center and its Expansions) */
extern int g_dc_enabled;
void dc_load_config(const char *dir);
void dc_install(void);
/* building types of the three Data Center Expansions */
#define DC_EXP_POPULATION 0x1C0 /* El Dorado's slot, building 34 */
#define DC_EXP_COMMERCE   0x207 /* Courtyard Fountain's slot, building 105 */
#define DC_EXP_EFFICIENCY 0x1E6 /* the last scenario House (HOUSE35), building 72 */
/* the Tank Traps (entrenchment mod, 2026-11-05): a 1x1 building Citizens build from the military
   menu, in the Washington Monument's slot (the Data Center's until 2026-10-29). Computer players
   never plan it (entrench.c, LeaderData::type_avail). */
#define TANK_TRAPS        0x1C1 /* building 35 of buildingrules.xml */
/* the Expansions' technician, through entrench.c's and airdrop.c's hooks */
int dc_is_expansion(int type);
int dc_is_technician(int type);
int dc_hides_option(int code, int type);     /* Options::add */
void dc_make_options(char *building);         /* BuildOut::make_options */
int dc_group_builds(char *group, int type);   /* GroupData::count, for a build button */
int dc_action_build(char *group, int *a);     /* Group::action_build: 1 = handled */
void dc_frame(void);                          /* Objects::process_all */

/* airdrop.c (paradrop mod: the Airbase's Airdrop ability) */
extern int g_airdrop_enabled;
void airdrop_load_config(const char *dir);
void airdrop_install(void);
int airdrop_is_option(int code, int type);
int airdrop_cooldown_permille(void);
int airdrop_charges(int *bases); /* drops the local selection can call now; *bases = Airbases selected */
void airdrop_ui_tick(void);      /* once per rendered frame: re-arms the targeting after a drop */

/* airstrike.c (a strike button on the Airbase; aircraft take off together) */
void airstrike_load_config(const char *ini_path);
void airstrike_install(void);
int airstrike_wanted(void);              /* the button needs airdrop.c's make_options hook */
void airstrike_make_options(char *self); /* from airdrop.c's BuildOut::make_options hook */
int airstrike_area_wanted(void);         /* the area strike (right-drag circle from the Airbases) */
void airstrike_area_install(int drawing); /* from nukefx_install: 1 = its draw_underlays hook is in */
void airstrike_draw(void);               /* from nukefx.c's Nuke::draw_underlays hook, once a frame */

/* facing.c (smooth unit turning, render only) */
extern int g_facing_enabled;
void facing_load_config(const char *ini_path);
void facing_install(void);

/* entrench.c (entrenchment mod: the Sandbag Nest) */
extern int g_entrench_enabled;
void entrench_load_config(const char *dir);
void entrench_install(void);
void entrench_frame(void);               /* from airdrop.c's Objects::process_all hook */
void entrench_make_options(char *self);  /* from airdrop.c's BuildOut::make_options hook */
/* the Entrench, Dig and Leave Nest buttons are crafts: airdrop.c's spell hooks hand them over
   (a = the hooked function's stack arguments: type, target id, target player, x, y) */
int entrench_is_craft(int type);
int entrench_caster(char *group, int type);
int entrench_validate(char *group, int *a);
void entrench_action(char *group, int *a);

/* ranks.c (veteran ranks; the bonuses are applied from the shared hooks of kills.c, entrench.c
   and airdrop.c) */
extern int g_ranks_enabled;
void ranks_load_config(const char *ini_path);
void ranks_install(void);
void ranks_frame(void);
int ranks_damage(char *attacker, int dmg);
int ranks_recharge(char *u, int r);
int ranks_range(char *u, int r);
void ranks_before_hit(char *victim, int *whole, int *part, int attacker_id);
void ranks_after_hit(char *victim, int before, int vmax, int was_decoy, int attacker_id, int attacker_p);
void ranks_draw_button(char *btn); /* from panel.c's IFaceButton::on_redraw hook: the portrait */
int ranks_draw_panel(char *panel, char *u, int kills, int show_kills); /* kills.c: one unit's right half */
void ranks_draw_insignia(char *o, short *rect); /* from prodbar.c: beside a building's health bar */

/* rankfx.c (the promotion effect over the map; drawing only) */
void rankfx_load_config(const char *ini_path);
void rankfx_install(void);
void rankfx_promoted(char *o, int level); /* from ranks.c when a unit or building ranks up */
int rankfx_anchor(char *o, float *x, float *y); /* o's health bar in the render manager's coordinates */
float rankfx_zoom(void);                         /* 1 at the closest zoom level */
void rf_add(const void *tex, float x, float y, float w, float h, unsigned color); /* add_smooth, type 1 */
void rankfx_console_to_rm(float *x, float *y); /* a health bar's point in the render manager's coordinates */
void ranks_draw_insignia_queue(void);           /* ranks.c, from rankfx.c's map window redraw */

/* heal.c (the General's Heal; started from ranks.c's SpellType::cast hook, run from airdrop.c's
   Objects::process_all) */
extern int g_heal_enabled;
void heal_draw_overhead(void); /* heal.c, from rankfx.c's map window redraw */
void heal_load_config(const char *ini_path);
void heal_start(char *general);
void heal_frame(void);
void heal_install(void);                    /* the hotkey */
const char *heal_hotkey(int code, int type); /* for panel.c: the letter on the Heal button */

/* shove.c (idle units step aside for their own moving units; own units walk through each other) */
extern int g_make_way_enabled, g_pass_enabled;
void make_way_load_config(const char *ini_path);
void make_way_install(void);
int entrench_holds(char *u); /* entrench.c: a soldier held in a Nest */

/* formation.c (mixed groups move without a formation; formations are compact blocks) */
extern int g_formation_enabled;
void formation_load_config(const char *ini_path);
void formation_install(void);

/* hover.c */
extern int g_hover_enabled;
void hover_load_config(const char *ini_path);
void hover_install(void);
int hover_relation(int player); /* 0 = the local player, 1 = an ally, 2 = anyone else */

/* postfx.c (colour and sharpening in the game's tone mapper and FXAA shaders) */
extern int g_postfx_enabled;
void postfx_load_config(const char *ini_path);
void postfx_init(void);    /* at DLL load, before override_install */
void postfx_display(void); /* before the display is made (init_display_quick pre-hook) */
int camera_get_loc(float *x, float *y); /* ron_camera.c: the screen centre on the ground */
void camera_set_loc(float x, float y);

/* outline.c (an outline around the 3D model of the hovered unit or squad; d3dgl / D3D11) */
extern int g_outline_enabled;
void outline_load_config(const char *ini_path);
void outline_install(void);

/* squadhp.c (a health bar over every soldier of a squad) */
extern int g_squad_hp_enabled;
void squad_hp_load_config(const char *ini_path);
void squad_hp_install(void);
int squad_hovered(int id, int player); /* the unit under the cursor is in the squad of (id, player) */

/* bribe.c */
extern int g_bribe_enabled;
void bribe_load_config(const char *ini_path);
void bribe_install(void);

/* follow.c */
void follow_load_config(const char *ini_path);
void follow_install(void);

/* rally.c */
extern int g_rally_enabled;
void rally_load_config(const char *ini_path);
void rally_install(void);

/* costs.c */
extern int g_costs_enabled;
void costs_load_config(const char *ini_path);
void costs_install(void);

/* prodbar.c */
extern int g_prodbar_enabled;
void prodbar_load_config(const char *ini_path);
void prodbar_install(void);
void prodbar_draw_icons(void); /* from rankfx.c's map window redraw */

/* romania.c */
extern int g_romania_enabled;
extern int g_num_tribes; /* nations in the game: 24, or 25 with Romania */
void romania_load_config(const char *dir);
void romania_install(void);
void romania_frame(void); /* from airdrop.c's Objects::process_all: unit names by age */
void crash_load_config(const char *ini_path); /* crash.c: [debug] crash_reports */
void crash_install(void);
void crash_test_now(void);    /* crash.c: [debug] crash_test=1 crashes at PLAY */      /* crash.c: the crash report filter; called again to re-arm it */
void stdio_raise(void);        /* override.c: ucrtbase's open FILE limit 512 -> 8192 */
void mem_log(const char *why); /* ron_camera.c: address space in use and largest free block, now */
void mem_frame(void);     /* ron_camera.c, the same hook: address space in the log once a minute */

/* cityrepair.c (Large and Major Cities repair their own buildings; a switch on the city panel) */
void cityrepair_load_config(const char *ini_path);
void cityrepair_install(void);          /* after ranks_install (Options::describe) */
int cityrepair_wanted(void);
void cityrepair_frame(void);            /* airdrop.c's Objects::process_all */
void cityrepair_make_options(char *self); /* airdrop.c's BuildOut::make_options */
int cityrepair_describe(const int *opt);  /* ranks.c's Options::describe: 1 = the text is ours */
int cityrepair_button_off(char *btn);     /* panel.c: the switch of cities that are off */
void cityrepair_draw_overhead(void);      /* rankfx.c's map window redraw: the pliers */
/* play_button.c */
extern int g_play_enabled;
void play_load_config(const char *ini_path);
void play_install(void);

/* For asm stubs in the modules that install their own hooks (kills.c, qol.c): save every
   register, align esp to 16 for the C code (it uses movaps on stack locals), keep the
   original esp in ebx. After HOOK_ENTER: [ebx+0..31] = pushad (edi, esi, ebp, esp, ebx,
   edx, ecx, eax), [ebx+32] = return address, [ebx+36] = first stack argument. */
#define HOOK_ENTER                 \
    "    pushad\n"                 \
    "    mov ebx, esp\n"           \
    "    sub esp, 128\n"           \
    "    and esp, -16\n"           \
    "    movdqa [esp+0], xmm0\n"   \
    "    movdqa [esp+16], xmm1\n"  \
    "    movdqa [esp+32], xmm2\n"  \
    "    movdqa [esp+48], xmm3\n"  \
    "    movdqa [esp+64], xmm4\n"  \
    "    movdqa [esp+80], xmm5\n"  \
    "    movdqa [esp+96], xmm6\n"  \
    "    movdqa [esp+112], xmm7\n"
#define HOOK_LEAVE                 \
    "    movdqa xmm0, [esp+0]\n"   \
    "    movdqa xmm1, [esp+16]\n"  \
    "    movdqa xmm2, [esp+32]\n"  \
    "    movdqa xmm3, [esp+48]\n"  \
    "    movdqa xmm4, [esp+64]\n"  \
    "    movdqa xmm5, [esp+80]\n"  \
    "    movdqa xmm6, [esp+96]\n"  \
    "    movdqa xmm7, [esp+112]\n" \
    "    mov esp, ebx\n"           \
    "    popad\n"

/* Writes a 5-byte jmp at `at` to `to`; copies `n` whole instructions from `at` into `mem`
   followed by a jmp back, and returns `mem` (the trampoline). In ron_camera.c. */
void hook_jmp5(unsigned char *at, const void *to);
void *hook_trampoline(unsigned char *at, int n, unsigned char *mem);
