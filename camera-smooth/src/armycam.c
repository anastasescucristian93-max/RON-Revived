/*
 * The camera stays put when a double press selects the whole army (the user, 2026-11-26: the
 * comma pressed twice to select every military unit moved the camera into the middle of the
 * army, away from the battle being watched; a control group's double press is left alone).
 *
 * The key is the game's "Cycle Military" (Options::do_cycle_military 0x0071FDE0, mode 0, camera
 * flag set). Every press asks Options::get_cycle_level 0x00721120: a press within 400 ms of the
 * last one (timeGetTime, the same option) counts up, and level 2 turns the press into "all
 * military" (mode 1; level 3 mode 2), which selects every military unit and moves no camera.
 * But the first press of the pair was a plain cycle: it selected the next military unit (or its
 * army) and moved the camera there at once (Camera::set_loc, or Camera::zoom_to when zooming:
 * six calls inside do_cycle_military). The second press then selected all, the camera already
 * gone.
 *
 * So, for the presses that can become a double press (mode 0 with the camera flag), the cycle's
 * camera move waits 420 ms: a next press of the key before then drops it (it is a double press,
 * or a quick cycle the next press replaces); otherwise the camera goes there as before. The
 * hotkey's other uses (the No Zoom cycle, All Military, the panel's Army button) never move the
 * camera and are unchanged. Interface only, nothing reaches the simulation.
 * Engine notes: ENGINE_NOTES.md, "Select all military: the camera".
 */
#include "ron.h"
#include <string.h>

#define VA_CAMERA_PTR   0x00C06200u /* Camera * (MiscAccess::camera) */
#define VA_CONSOLE_PTR  0x00C06210u
#define VA_SET_LOC      0x00844A60u /* Camera::set_loc(x, y), ret 8 */
#define VA_ZOOM_TO      0x00843E50u /* Camera::zoom_to(x, y), ret 8 */
#define WAIT_MS         420         /* get_cycle_level's double press window is 400 ms */

int g_armycam_enabled;
void *g_orig_cycle_military;
UINT_PTR g_armycam_set_loc, g_armycam_zoom_to;
int g_armycam_now;
static int g_hold;              /* this do_cycle_military call may become a double press */
static int g_pending, g_kind, g_x, g_y;
static DWORD g_since;

typedef void(__attribute__((thiscall)) *cam_xy_t)(void *cam, int x, int y);

void armycam_load_config(const char *ini_path)
{
    g_armycam_enabled = GetPrivateProfileIntA("qol", "army_double_press_keeps_camera", 1, ini_path);
}

/* do_cycle_military(mode, zoom, camera), on entry (g_hooks "cycle_military") */
__attribute__((used)) void c_cycle_military(int mode, int camera)
{
    g_pending = 0; /* another press: the last one's move is dropped */
    g_hold = mode == 0 && camera != 0;
}

/* One of do_cycle_military's camera calls: 1 = make it now, 0 = it waits. */
__attribute__((used)) int c_armycam_move(int kind, int x, int y)
{
    if (!g_hold)
        return 1;
    g_pending = 1, g_kind = kind, g_x = x, g_y = y;
    g_since = GetTickCount();
    return 0;
}

/* Once a frame (ron_camera.c's check_scroll hook). */
void armycam_tick(void)
{
    if (!g_pending || GetTickCount() - g_since < WAIT_MS)
        return;
    g_pending = 0;
    void *cam = *(void **)VA(VA_CAMERA_PTR);
    if (!cam || !*(void **)VA(VA_CONSOLE_PTR))
        return;
    ((cam_xy_t)VA(g_kind ? VA_ZOOM_TO : VA_SET_LOC))(cam, g_x, g_y);
}

void hk_cycle_military(void);
void hk_armycam_set_loc(void);
void hk_armycam_zoom_to(void);

__asm__(".intel_syntax noprefix\n"
        /* Options::do_cycle_military(int mode, int zoom, int camera), entry */
        ".globl _hk_cycle_military\n"
        "_hk_cycle_military:\n" HOOK_ENTER
        "    sub esp, 8\n"
        "    push dword ptr [ebx+44]\n"
        "    push dword ptr [ebx+36]\n"
        "    call _c_cycle_military\n"
        "    add esp, 16\n" HOOK_LEAVE
        "    jmp dword ptr [_g_orig_cycle_military]\n"
        /* the camera calls: ecx = the camera, [esp+4] x, [esp+8] y, callee pops 8 */
        ".globl _hk_armycam_set_loc\n"
        "_hk_armycam_set_loc:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    push 0\n"
        "    call _c_armycam_move\n"
        "    add esp, 16\n"
        "    mov [_g_armycam_now], eax\n" HOOK_LEAVE
        "    cmp dword ptr [_g_armycam_now], 0\n"
        "    je 1f\n"
        "    jmp dword ptr [_g_armycam_set_loc]\n"
        "1:  ret 8\n"
        ".globl _hk_armycam_zoom_to\n"
        "_hk_armycam_zoom_to:\n" HOOK_ENTER
        "    sub esp, 4\n"
        "    push dword ptr [ebx+40]\n"
        "    push dword ptr [ebx+36]\n"
        "    push 1\n"
        "    call _c_armycam_move\n"
        "    add esp, 16\n"
        "    mov [_g_armycam_now], eax\n" HOOK_LEAVE
        "    cmp dword ptr [_g_armycam_now], 0\n"
        "    je 2f\n"
        "    jmp dword ptr [_g_armycam_zoom_to]\n"
        "2:  ret 8\n"
        ".att_syntax prefix\n");

/* The six camera calls inside do_cycle_military go to the stubs. After install_hooks: without
   the entry hook (signature mismatch) nothing is changed. */
void armycam_install(void)
{
    if (!g_armycam_enabled || !g_orig_cycle_military)
        return;
    static const struct {
        UINT32 va;
        int zoom;
    } sites[6] = {
        { 0x00720309u, 1 }, { 0x0072031Bu, 0 }, { 0x00720570u, 1 },
        { 0x0072062Au, 0 }, { 0x00720762u, 1 }, { 0x00720776u, 0 },
    };
    for (int i = 0; i < 6; i++) {
        unsigned char *at = (unsigned char *)VA(sites[i].va);
        UINT_PTR want = VA(sites[i].zoom ? VA_ZOOM_TO : VA_SET_LOC);
        if (at[0] != 0xE8 || (UINT_PTR)(at + 5 + *(INT32 *)(at + 1)) != want) {
            logf_("army camera: call site mismatch at %08lX, the camera moves as in the game", (unsigned long)sites[i].va);
            return;
        }
    }
    g_armycam_set_loc = VA(VA_SET_LOC);
    g_armycam_zoom_to = VA(VA_ZOOM_TO);
    for (int i = 0; i < 6; i++) {
        unsigned char *at = (unsigned char *)VA(sites[i].va);
        const void *to = sites[i].zoom ? (const void *)hk_armycam_zoom_to : (const void *)hk_armycam_set_loc;
        DWORD old;
        VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old);
        *(INT32 *)(at + 1) = (INT32)((UINT_PTR)to - (UINT_PTR)(at + 5));
        VirtualProtect(at, 5, old, &old);
    }
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
    logf_("army camera: a double press of Cycle Military selects the army without moving the camera");
}
