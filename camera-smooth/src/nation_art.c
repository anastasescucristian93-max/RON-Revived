/*
 * Per-nation building art.
 *
 * nation_art.ini (game folder) lists, per nation, which building sprites to replace:
 *
 *     [romans]                                   ; key from tribes\*.xml
 *     barracks_0m.tga = nation_art\romans\barracks_0m.tga
 *     village_0ma.tga@2 = nation_art\romans\village_age2.tga   ; only while the owner is in age 2
 *
 * Ages count from 1 (Ancient) to 8 (Information). A rule with an age wins over one without.
 * The [all] section applies to every nation, after the nation's own rules: the mods' own
 * buildings use it for a look the game has no entry for (the Field Fortification in age 8).
 * On winter maps the game draws "<name>_snow.tga"; a rule for that name gives a nation its own
 * snowy art, otherwise its rule for "<name>.tga" is used.
 * The game itself picks one sprite for several ages (village_0ma for ages 1-3), so a per-age
 * rule only takes effect in the ages where the game draws that sprite.
 *
 * Animated art: "<name>~<frames>:<ms>" plays <frames> sprites (the file name holds %d for the
 * frame, 0 to frames - 1) back and forth over <ms> milliseconds: frame 0, up to the last, back
 * to 0. "<name>~<frames>:<ms>:loop" plays them in a loop instead, 0 to the last in <ms>, then 0
 * again (turning fans). The Data Center Expansions use it for their flickering lights and roof
 * fans. Each building gets its own phase, so they do not move together. Drawing only: the frame
 * comes from the clock, not the simulation. The depth mesh is frame 0's.
 *
 * A building's render data (RData) holds a SpritePacket (slot 15) whose Sprite is shared
 * by every nation of that art style. GraphicPieces::draw_build runs rdata->execute()
 * synchronously, which reads that Sprite while building the quad, so swapping the Sprite
 * pointer for the duration of draw_build changes only that building's drawing. Replacement
 * sprites are loaded with the game's own ImageIO::load, with the packet's discrete scales.
 *
 * Depth: the sprite writes no depth, the building's zbuffer mesh does (building_graphics.xml
 * zbuffer=). A replacement keeps the original's mesh unless a mesh sits next to its file:
 * nation_art\romans\tower_6.tga -> nation_art\romans\tower_6z.bh3 (made by
 * roman-buildings/tools/depth_mesh.py). It is loaded once into an RData of our own, the way
 * GraphicPieces::init_build_data loads a building, and its geometry packets are swapped in for
 * the duration of draw_build like the sprite. Without it, crops, water and units behind the
 * building show through every part of the new art outside the old mesh.
 */
#include "ron.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VA_LEADERS          0x00E3A390u /* LeaderData[8], 0x6eec each; +0xc = tribe index */
#define LEADER_STATE        0x6EB8      /* -> per-player state, +0xdc = age ^ AGE_KEY (LeaderData::get_age) */
#define STATE_AGE           0xDC
#define AGE_KEY             0x62766u
#define VA_TRIBES           0x00E7FA20u /* Tribes */
#define VA_TRIBES_FIND      0x006EF2D0u /* Tribes::find(const String &) */
#define VA_ALL_SPRITES      0x00E896BCu /* NamedArray<Sprite*>: +0x10 Sprite**, +0x18 String[] names */
#define VA_SPRITEPKT_VFT    0x00B55860u
#define VA_SPRITE_CTOR      0x0053DBF0u /* Sprite::Sprite() */
#define VA_IMAGEIO_SET_TINT 0x00511A00u /* ImageIO::set_tint(Color) */
#define VA_IMAGEIO_LOAD     0x00512560u /* ImageIO::load(String &, PixFormat, MemType, int, int, int, Array<float> *) */
#define VA_STRING_CTOR      0x00A1D660u
#define VA_STRING_CLOSE     0x00A1CF40u
#define VA_STRING_AS_CHAR   0x0042D710u /* String::str_as_char() */
#define VA_BLACK            0x00C8D260u /* Color BLACK */
#define VA_RDATA_CTOR       0x0091A340u /* RData::RData() */
#define VA_RDATA_SET_PARAM  0x0091A460u /* RData::set_param(const String &, RParam), ret 0xc */
#define VA_RDATA_LOAD       0x0091A480u /* RData::load() -> RMap::load -> each RMethod::load */
#define VA_TRANSFORM_ROT_Z  0x00421C50u /* Transform<float>::rotate_z(int degrees) */
#define VA_INT_STR_ARRAY    0x00C06378u /* StringTable *; +0x10 -> the strings (String, 0x14 each) */

/* GraphicPieces::init_build_data 0x008F2F20, what it gives a building's RData before load */
#define GP_RMAP_ZBUFFER 0xA6C   /* RMap of buildings with a zbuffer mesh (0xA68: sprite only) */
#define GP_PARAM_1040   0xF54   /* int parameter, key 0x1040 */
#define GP_SCALES       0xF7C   /* Array<float> discrete sprite scales, key 0x12BEC */
#define KEY_ZBUFFER     0x10CC  /* parameter names: offsets into the string table */
#define KEY_TEXTURE     0x12BC4
#define KEY_1040        0x1040
#define KEY_TRANSFORM   0x12BD8 /* Transform: rotate_z(270), scale (1, -1, 1) */
#define KEY_SCALES      0x12BEC
#define RDATA_ALLOC     0xA8    /* refcount dword + 0xA4 object, as init_build_data makes it */
#define RDATA_PACKETS   0x08    /* RPacket *[24], indexed by get_packet_type() */
#define RDATA_EXTRA     0x98    /* -> 0x30 bytes init_build_data allocates */
#define PKT_ANIMATION   19      /* AnimationPacket: PreTransformMethod::load poses the mesh with it */
/* Geometry packets of the mesh: Vert3 1, Vert4 2, scratch 3, Norm 4, 5, Texcrd 6, 7, Diffuse 8,
   9, Face 10, 11, VStream 12, IStream 13, AnimMesh 22. Not swapped: Texture 14, Sprite 15,
   Hierarchy 16 and Node 17 (the addons' attachment points), Transform 18 (draw_build writes the
   building's place there), Animation 19, BoundingVolume 20, Event 21, Sound 23. */
#define MESH_SLOTS      0x00403FFEu

#define RDATA_SPRITEPKT 0x44 /* RPacket *packets[] at RData + 8; SpritePacket::get_type() == 15 */
#define SPKT_SPRITE     0x08 /* Sprite * */
#define SPKT_INDEX      0x0C /* index into all_sprites */
#define SPKT_SCALES     0x18 /* Array<float> discrete scales */
#define SPRITE_ALLOC    0x94 /* refcount dword + 0x90 object, as SpritePacket::set_sprite does */
#define SPRITE_TINT     0x04 /* Color (3 dwords) stored by ImageIO::set_tint */

typedef void(__attribute__((thiscall)) *fn_v)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_cstr)(void *self, const char *s);
typedef int(__attribute__((thiscall)) *fn_i_p)(void *self, const void *p);
typedef const char *(__attribute__((thiscall)) *fn_cstr)(void *self);
typedef void(__attribute__((thiscall)) *fn_tint)(void *self, unsigned c0, unsigned c1, unsigned c2);
typedef int(__attribute__((thiscall)) *fn_load)(void *self, void *name, int fmt, int mem, int a, int b, int c, void *scales);
typedef int(__attribute__((thiscall)) *fn_draw)(void *self, void *chad, void *rdata);
typedef int(__attribute__((thiscall)) *fn_set_param)(void *self, const void *key, const void *p, int type);
typedef int(__attribute__((thiscall)) *fn_i)(void *self);
typedef void(__attribute__((thiscall)) *fn_v_i)(void *self, int i);
#define CALL(type, va) ((type)VA(va))

int g_nation_art_enabled;
void *g_orig_draw_build;

/* ---- config ------------------------------------------------------------------------ */

#define MAX_RULES 256
#define MAX_FRAMES 32
#define TRIBE_ALL (-3) /* rules of the [all] section */
static struct {
    char nation[32];
    char original[64]; /* lower-case file name, e.g. barracks_0m.tga */
    char file[MAX_PATH]; /* animated: frame 0's file */
    char pattern[MAX_PATH]; /* animated: the file name with %d */
    int frames, period; /* animated: frame count (1 = still), milliseconds of one cycle */
    int loop;          /* animated: 0 = back and forth, 1 = in a loop */
    int age;           /* 1-8, 0 = every age */
    int tribe;         /* resolved on first use, -2 = not yet */
} g_rules[MAX_RULES];
static int g_rule_n;

static void lower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

void nation_art_load_config(const char *dir)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%snation_art.ini", dir);
    g_rule_n = 0;
    char sections[2048];
    DWORD n = GetPrivateProfileSectionNamesA(sections, sizeof sections, ini);
    for (char *sec = sections; n && *sec; sec += strlen(sec) + 1) {
        char body[8192];
        DWORD m = GetPrivateProfileSectionA(sec, body, sizeof body, ini);
        for (char *line = body; m && *line; line += strlen(line) + 1) {
            char *eq = strchr(line, '=');
            if (!eq || *line == ';' || g_rule_n >= MAX_RULES)
                continue;
            char key[64], val[MAX_PATH];
            snprintf(key, sizeof key, "%.*s", (int)(eq - line), line);
            snprintf(val, sizeof val, "%s", eq + 1);
            for (char *p = key + strlen(key); p > key && isspace((unsigned char)p[-1]);)
                *--p = 0; /* trim */
            char *v = val;
            while (isspace((unsigned char)*v))
                v++;
            lower(key);
            char *tilde = strchr(key, '~');
            int frames = 1, period = 0;
            if (tilde) {
                if (sscanf(tilde + 1, "%d:%d", &frames, &period) != 2 || frames < 2 || frames > MAX_FRAMES ||
                    period < 100 || !strstr(v, "%d")) {
                    logf_("nation art: bad animation in '%s', ignored", line);
                    continue;
                }
                *tilde = 0;
            }
            g_rules[g_rule_n].frames = frames;
            g_rules[g_rule_n].period = period;
            g_rules[g_rule_n].loop = tilde && strstr(tilde + 1, ":loop") != NULL;
            char *at = strchr(key, '@');
            g_rules[g_rule_n].age = at ? atoi(at + 1) : 0;
            if (at)
                *at = 0;
            if (g_rules[g_rule_n].age < 0 || g_rules[g_rule_n].age > 8) {
                logf_("nation art: bad age in '%s', ignored", line);
                continue;
            }
            snprintf(g_rules[g_rule_n].nation, sizeof g_rules[0].nation, "%s", sec);
            snprintf(g_rules[g_rule_n].original, sizeof g_rules[0].original, "%s", key);
            snprintf(g_rules[g_rule_n].pattern, sizeof g_rules[0].pattern, ".\\%s", v);
            if (frames > 1)
                snprintf(g_rules[g_rule_n].file, sizeof g_rules[0].file, g_rules[g_rule_n].pattern, 0);
            else
                snprintf(g_rules[g_rule_n].file, sizeof g_rules[0].file, "%s", g_rules[g_rule_n].pattern);
            g_rules[g_rule_n].tribe = -2;
            g_rule_n++;
        }
    }
    g_nation_art_enabled = g_rule_n > 0;
    if (g_rule_n)
        logf_("nation art: %d replacement(s) in nation_art.ini", g_rule_n);
}

/* ---- sprites ------------------------------------------------------------------------ */

static int tribe_index(const char *key)
{
    unsigned char str[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(str, key);
    int t = CALL(fn_i_p, VA_TRIBES_FIND)((void *)VA(VA_TRIBES), str);
    CALL(fn_v, VA_STRING_CLOSE)(str);
    return t;
}

/* The registry's name (a String, the path the game loaded it from) of a game sprite. */
static void *sprite_name(int index)
{
    int count = *(int *)(VA(VA_ALL_SPRITES) + 4);
    char *names = *(char **)(VA(VA_ALL_SPRITES) + 0x18);
    return index >= 0 && index < count && names ? names + index * 0x14 : NULL;
}

/* File name of a game sprite (from the all_sprites registry), lower-case, no folders. */
static void sprite_file_name(int index, char *out, size_t size)
{
    out[0] = 0;
    void *str = sprite_name(index);
    if (!str)
        return;
    const char *s = CALL(fn_cstr, VA_STRING_AS_CHAR)(str);
    const char *base = s ? s : "";
    for (const char *p = base; *p; p++)
        if (*p == '\\' || *p == '/')
            base = p + 1;
    snprintf(out, size, "%s", base);
    lower(out);
}

static void *load_sprite(const char *file, void *scales, void *orig)
{
    /* Never freed: lives for the whole session. Layout as SpritePacket::set_sprite makes it. */
    unsigned char *mem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SPRITE_ALLOC);
    if (!mem)
        return NULL;
    *(int *)mem = 1;
    void *sprite = mem + 4;
    CALL(fn_v, VA_SPRITE_CTOR)(sprite);
    /* Same tint as the sprite we replace (SpriteMethod::load passes BLACK unless the render
       data overrides it; ImageIO::set_tint keeps the Color at +4). */
    const unsigned *tint = orig ? (const unsigned *)((char *)orig + SPRITE_TINT) : (const unsigned *)VA(VA_BLACK);
    CALL(fn_tint, VA_IMAGEIO_SET_TINT)(sprite, tint[0], tint[1], tint[2]);
    logf_("nation art: %s tint %08x %08x %08x", file, tint[0], tint[1], tint[2]);
    unsigned char str[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(str, file);
    int err = CALL(fn_load, VA_IMAGEIO_LOAD)(sprite, str, 0x10, 1, 0, 1, 0x30, scales);
    CALL(fn_v, VA_STRING_CLOSE)(str);
    if (err) {
        logf_("nation art: could not load %s (error %d)", file, err);
        return NULL;
    }
    return sprite;
}

/* Loaded files, so a file used for several ages or nations is loaded once per original sprite
   (the original's packet supplies the scales it is loaded with). */
typedef struct {
    void *orig;
    const char *file;
    void *sprite;
    char *mesh;     /* RData holding the depth mesh, NULL when there is none */
    int mesh_tried; /* the mesh is loaded on the first draw, which supplies the building */
} Art;
static Art g_loaded[MAX_RULES * 2];
static int g_loaded_n;

static Art *loaded_art(void *orig, const char *file, void *scales)
{
    for (int i = 0; i < g_loaded_n; i++)
        if (g_loaded[i].orig == orig && !strcmp(g_loaded[i].file, file))
            return &g_loaded[i];
    void *s = load_sprite(file, scales, orig);
    if (!s || g_loaded_n >= (int)(sizeof g_loaded / sizeof g_loaded[0]))
        return NULL;
    Art *a = &g_loaded[g_loaded_n++];
    a->orig = orig;
    a->file = file;
    a->sprite = s;
    return a;
}

/* ---- depth meshes ------------------------------------------------------------------- */

static const void *param_key(unsigned offset)
{
    char *strings = *(char **)(*(char **)VA(VA_INT_STR_ARRAY) + 0x10);
    return strings + offset;
}

static unsigned packet_mask(char *rdata)
{
    unsigned m = 0;
    for (int t = 0; t < 24; t++)
        if (((void **)(rdata + RDATA_PACKETS))[t])
            m |= 1u << t;
    return m;
}

/* Loads <file without .tga>z.bh3 as GraphicPieces::init_build_data loads a building's zbuffer
   mesh: an RData with the zbuffer RMap and the same parameters (texture = the original sprite,
   so the sprite method has what it expects). NULL when the file is missing or loads to other
   packets than the original's. */
static char *load_mesh(char *pieces, char *orig_rdata, const char *file, void *texture)
{
    char path[MAX_PATH];
    size_t len = strlen(file);
    if (len < 4 || len + 2 > sizeof path || !texture)
        return NULL;
    snprintf(path, sizeof path, "%.*sz.bh3", (int)(len - 4), file);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        return NULL;

    /* Never freed: lives for the whole session. */
    unsigned char *mem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, RDATA_ALLOC);
    void *extra = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x30);
    if (!mem || !extra)
        return NULL;
    *(int *)mem = 1;
    char *rd = (char *)mem + 4;
    CALL(fn_v, VA_RDATA_CTOR)(rd);
    *(void **)(rd + RDATA_EXTRA) = extra;
    *(void **)(rd + 4) = *(void **)(pieces + GP_RMAP_ZBUFFER);

    /* the transform init_build_data gives every building mesh (0x008F3A65) */
    float t[16] __attribute__((aligned(16))) = { 0 };
    t[7] = 1.0f;
    CALL(fn_v_i, VA_TRANSFORM_ROT_Z)(t, 270);
    unsigned char *flags = (unsigned char *)t + 0x2C;
    if (*flags & 0x40) {
        t[9] = -t[9];
    } else {
        *flags |= 0x40;
        t[8] = 1.0f;
        t[9] = -1.0f;
        t[10] = 1.0f;
    }
    unsigned char mesh_name[64] = { 0 };
    CALL(fn_v_cstr, VA_STRING_CTOR)(mesh_name, path);
    fn_set_param set = CALL(fn_set_param, VA_RDATA_SET_PARAM);
    set(rd, param_key(KEY_ZBUFFER), mesh_name, 4);
    set(rd, param_key(KEY_TEXTURE), texture, 4);
    set(rd, param_key(KEY_1040), *(void **)(pieces + GP_PARAM_1040), 1);
    set(rd, param_key(KEY_TRANSFORM), t, 3);
    set(rd, param_key(KEY_SCALES), pieces + GP_SCALES, 3);
    /* PreTransformMethod::load poses the mesh with the building's animation; borrowed for the load */
    void **anim = (void **)(rd + RDATA_PACKETS) + PKT_ANIMATION;
    *anim = ((void **)(orig_rdata + RDATA_PACKETS))[PKT_ANIMATION];
    int err = CALL(fn_i, VA_RDATA_LOAD)(rd);
    *anim = NULL;
    CALL(fn_v, VA_STRING_CLOSE)(mesh_name);

    unsigned mine = packet_mask(rd) & MESH_SLOTS, theirs = packet_mask(orig_rdata) & MESH_SLOTS;
    logf_("nation art: %s: load %d, mesh packets %06x (original %06x)", path, err, mine, theirs);
    if (err || mine != theirs || !(mine & 2)) {
        logf_("nation art: %s not used, the original depth mesh stays", path);
        return NULL;
    }
    return rd;
}

/* Cache: original sprite + tribe + age -> replacement (NULL when there is none) and its rule. */
#define MAX_CACHE 512
static struct {
    void *orig;
    int tribe, age, rule;
    Art *repl;
} g_cache[MAX_CACHE];
static int g_cache_n;

/* The rule for `name` (an original texture file name) for this tribe and age: one with the
   age wins over one for every age. -1 when there is none. */
static int find_rule(const char *name, int tribe, int age)
{
    /* the nation's own rule for this age, then its rule for every age, then the same two of
       [all] (every nation: art of the mods' own buildings, such as the Field Fortification) */
    int best = -1, best_score = 0;
    for (int i = 0; i < g_rule_n; i++) {
        if (g_rules[i].tribe == -2) {
            g_rules[i].tribe = strcmp(g_rules[i].nation, "all") ? tribe_index(g_rules[i].nation) : TRIBE_ALL;
            if (i == 0 || strcmp(g_rules[i - 1].nation, g_rules[i].nation)) /* once per nation */
                logf_("nation art: [%s] is tribe %d", g_rules[i].nation, g_rules[i].tribe);
        }
        if ((g_rules[i].tribe != tribe && g_rules[i].tribe != TRIBE_ALL) || strcmp(g_rules[i].original, name))
            continue;
        if (g_rules[i].age && g_rules[i].age != age)
            continue;
        int score = (g_rules[i].tribe == tribe ? 2 : 0) + (g_rules[i].age == age ? 2 : 1);
        if (score > best_score) {
            best = i;
            best_score = score;
        }
    }
    return best;
}

static Art *replacement(void *orig, int index, void *scales, int tribe, int age, int *rule)
{
    for (int i = 0; i < g_cache_n; i++)
        if (g_cache[i].orig == orig && g_cache[i].tribe == tribe && g_cache[i].age == age) {
            *rule = g_cache[i].rule;
            return g_cache[i].repl;
        }

    char name[64];
    sprite_file_name(index, name, sizeof name);
    int best = find_rule(name, tribe, age);
    /* Winter maps draw "<name>_snow.tga" (art\snow, Data\overlayimages.xml). Without a rule
       for the snow version, the nation's normal replacement is used, so the nation keeps its
       own art in winter (without the snow). */
    char base[64];
    size_t len = strlen(name);
    if (best < 0 && len > 9 && !strcmp(name + len - 9, "_snow.tga")) {
        snprintf(base, sizeof base, "%.*s.tga", (int)(len - 9), name);
        best = find_rule(base, tribe, age);
    }
    Art *repl = best >= 0 ? loaded_art(orig, g_rules[best].file, scales) : NULL;
    /* logged once, when it goes into the cache: with the cache full this ran on every frame */
    if (repl && g_cache_n < MAX_CACHE)
        logf_("nation art: %s -> %s for %s, age %d", name, g_rules[best].file, g_rules[best].nation, age);
    if (g_cache_n < 40)
        logf_("nation art: tribe %d age %d draws sprite '%s'%s", tribe, age, name, repl ? " (replaced)" : "");
    if (g_cache_n == MAX_CACHE - 1)
        logf_("nation art: sprite cache full (%d), later sprites are looked up on every draw", MAX_CACHE);
    if (g_cache_n < MAX_CACHE) {
        g_cache[g_cache_n].orig = orig;
        g_cache[g_cache_n].tribe = tribe;
        g_cache[g_cache_n].age = age;
        g_cache[g_cache_n].repl = repl;
        g_cache[g_cache_n].rule = best;
        g_cache_n++;
    }
    *rule = best;
    return repl;
}

/* The sprite of an animated rule for this moment: back and forth (or in a loop) over the period,
   with a phase of its own for each building (`key`, the building's GraphicChad). Frame 0's when
   a frame does not load. */
static void *animated_sprite(int rule, Art *first, void *scales, const void *key)
{
    static char *names[MAX_RULES][MAX_FRAMES]; /* never freed */
    static char failed[MAX_RULES][MAX_FRAMES];  /* a frame that did not load is not tried again */
    int n = g_rules[rule].frames, period = g_rules[rule].period;
    unsigned phase = (unsigned)((UINT_PTR)key >> 4) * 2654435761u;
    unsigned t = (GetTickCount() + phase) % (unsigned)period;
    unsigned steps = g_rules[rule].loop ? (unsigned)n : 2u * (n - 1); /* 0 .. n-1 (.. 1) */
    int f = (int)(t * steps / (unsigned)period);
    if (f > n - 1)
        f = (int)steps - f;
    if (f <= 0 || failed[rule][f])
        return first->sprite;
    if (!names[rule][f]) {
        names[rule][f] = HeapAlloc(GetProcessHeap(), 0, MAX_PATH);
        if (!names[rule][f])
            return first->sprite;
        snprintf(names[rule][f], MAX_PATH, g_rules[rule].pattern, f);
    }
    Art *a = loaded_art(first->orig, names[rule][f], scales);
    failed[rule][f] = !a;
    return a ? a->sprite : first->sprite;
}

static int player_tribe(int player)
{
    if (player < 0 || player > 7)
        return -1;
    return *(int *)(VA(VA_LEADERS) + player * 0x6EEC + 0xC);
}

/* Current age of a player, 1 (Ancient) to 8 (Information); 0 when unknown. */
static int player_age(int player)
{
    char *state = *(char **)(VA(VA_LEADERS) + player * 0x6EEC + LEADER_STATE);
    if (!state)
        return 0;
    unsigned age = *(unsigned *)(state + STATE_AGE) ^ AGE_KEY;
    return age < 8 ? (int)age + 1 : 0;
}

/* The game's draw_build, with the sub-pixel fraction (subpixel.c) of the sprite now in the
   packet: ours when it has been swapped in. */
static int draw_game(void *self, char *chad, char *rdata)
{
    subpixel_begin(chad, rdata);
    int r = ((fn_draw)g_orig_draw_build)(self, chad, rdata);
    subpixel_end();
    return r;
}

/* GraphicPieces::draw_build(GraphicChad &chad, RData *rdata). chad +8 = owner player. */
static int draw_build_nation_art(void *self, char *chad, char *rdata)
{
    if (!g_nation_art_enabled)
        return draw_game(self, chad, rdata);
    int player = chad && rdata ? *(signed char *)(chad + 8) : -1;
    int tribe = player_tribe(player);
    if (tribe < 0)
        return draw_game(self, chad, rdata);

    char *packet = *(char **)(rdata + RDATA_SPRITEPKT);
    if (!packet || *(UINT_PTR *)packet != VA(VA_SPRITEPKT_VFT))
        return draw_game(self, chad, rdata);
    void **slot = (void **)(packet + SPKT_SPRITE);
    if (!*slot)
        return draw_game(self, chad, rdata);
    int index = *(int *)(packet + SPKT_INDEX), rule;
    Art *repl = replacement(*slot, index, packet + SPKT_SCALES, tribe, player_age(player), &rule);
    if (!repl)
        return draw_game(self, chad, rdata);
    if (!repl->mesh_tried) {
        repl->mesh_tried = 1;
        repl->mesh = load_mesh(self, rdata, repl->file, sprite_name(index));
    }

    void *saved = *slot;
    *slot = g_rules[rule].frames > 1 ? animated_sprite(rule, repl, packet + SPKT_SCALES, chad) : repl->sprite;
    void **pk = (void **)(rdata + RDATA_PACKETS), *saved_pk[24];
    if (repl->mesh)
        for (int t = 0; t < 24; t++)
            if (MESH_SLOTS >> t & 1) {
                saved_pk[t] = pk[t];
                pk[t] = ((void **)(repl->mesh + RDATA_PACKETS))[t];
            }
    int r = draw_game(self, chad, rdata);
    if (repl->mesh)
        for (int t = 0; t < 24; t++)
            if (MESH_SLOTS >> t & 1)
                pk[t] = saved_pk[t];
    *slot = saved;
    return r;
}

/* The draw_build hook serves two features: sub-pixel placement (subpixel.c) and the
   per-nation art above. */
__attribute__((used)) int c_draw_build(void *self, char *chad, char *rdata)
{
    return draw_build_nation_art(self, chad, rdata);
}
