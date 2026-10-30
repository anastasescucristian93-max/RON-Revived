/* GENERAT de tools/gen_winmm_proxy.py - nu edita manual. */
#define WINMM_PROC_COUNT 193
static const struct { const char *name; WORD ordinal; } g_winmm_exports[] = {
    { "CloseDriver", 4 },
    { "DefDriverProc", 5 },
    { "DriverCallback", 6 },
    { "DrvGetModuleHandle", 7 },
    { "GetDriverModuleHandle", 8 },
    { "NotifyCallbackData", 9 },
    { "OpenDriver", 10 },
    { "PlaySound", 11 },
    { "PlaySoundA", 12 },
    { "PlaySoundW", 13 },
    { "SendDriverMessage", 14 },
    { "WOW32DriverCallback", 15 },
    { "WOW32ResolveMultiMediaHandle", 16 },
    { "WOWAppExit", 17 },
    { "aux32Message", 18 },
    { "auxGetDevCapsA", 19 },
    { "auxGetDevCapsW", 20 },
    { "auxGetNumDevs", 21 },
    { "auxGetVolume", 22 },
    { "auxOutMessage", 23 },
    { "auxSetVolume", 24 },
    { "joy32Message", 25 },
    { "joyConfigChanged", 26 },
    { "joyGetDevCapsA", 27 },
    { "joyGetDevCapsW", 28 },
    { "joyGetNumDevs", 29 },
    { "joyGetPos", 30 },
    { "joyGetPosEx", 31 },
    { "joyGetThreshold", 32 },
    { "joyReleaseCapture", 33 },
    { "joySetCapture", 34 },
    { "joySetThreshold", 35 },
    { "mci32Message", 36 },
    { "mciDriverNotify", 37 },
    { "mciDriverYield", 38 },
    { "mciExecute", 3 },
    { "mciFreeCommandResource", 39 },
    { "mciGetCreatorTask", 40 },
    { "mciGetDeviceIDA", 41 },
    { "mciGetDeviceIDFromElementIDA", 42 },
    { "mciGetDeviceIDFromElementIDW", 43 },
    { "mciGetDeviceIDW", 44 },
    { "mciGetDriverData", 45 },
    { "mciGetErrorStringA", 46 },
    { "mciGetErrorStringW", 47 },
    { "mciGetYieldProc", 48 },
    { "mciLoadCommandResource", 49 },
    { "mciSendCommandA", 50 },
    { "mciSendCommandW", 51 },
    { "mciSendStringA", 52 },
    { "mciSendStringW", 53 },
    { "mciSetDriverData", 54 },
    { "mciSetYieldProc", 55 },
    { "mid32Message", 56 },
    { "midiConnect", 57 },
    { "midiDisconnect", 58 },
    { "midiInAddBuffer", 59 },
    { "midiInClose", 60 },
    { "midiInGetDevCapsA", 61 },
    { "midiInGetDevCapsW", 62 },
    { "midiInGetErrorTextA", 63 },
    { "midiInGetErrorTextW", 64 },
    { "midiInGetID", 65 },
    { "midiInGetNumDevs", 66 },
    { "midiInMessage", 67 },
    { "midiInOpen", 68 },
    { "midiInPrepareHeader", 69 },
    { "midiInReset", 70 },
    { "midiInStart", 71 },
    { "midiInStop", 72 },
    { "midiInUnprepareHeader", 73 },
    { "midiOutCacheDrumPatches", 74 },
    { "midiOutCachePatches", 75 },
    { "midiOutClose", 76 },
    { "midiOutGetDevCapsA", 77 },
    { "midiOutGetDevCapsW", 78 },
    { "midiOutGetErrorTextA", 79 },
    { "midiOutGetErrorTextW", 80 },
    { "midiOutGetID", 81 },
    { "midiOutGetNumDevs", 82 },
    { "midiOutGetVolume", 83 },
    { "midiOutLongMsg", 84 },
    { "midiOutMessage", 85 },
    { "midiOutOpen", 86 },
    { "midiOutPrepareHeader", 87 },
    { "midiOutReset", 88 },
    { "midiOutSetVolume", 89 },
    { "midiOutShortMsg", 90 },
    { "midiOutUnprepareHeader", 91 },
    { "midiStreamClose", 92 },
    { "midiStreamOpen", 93 },
    { "midiStreamOut", 94 },
    { "midiStreamPause", 95 },
    { "midiStreamPosition", 96 },
    { "midiStreamProperty", 97 },
    { "midiStreamRestart", 98 },
    { "midiStreamStop", 99 },
    { "mixerClose", 100 },
    { "mixerGetControlDetailsA", 101 },
    { "mixerGetControlDetailsW", 102 },
    { "mixerGetDevCapsA", 103 },
    { "mixerGetDevCapsW", 104 },
    { "mixerGetID", 105 },
    { "mixerGetLineControlsA", 106 },
    { "mixerGetLineControlsW", 107 },
    { "mixerGetLineInfoA", 108 },
    { "mixerGetLineInfoW", 109 },
    { "mixerGetNumDevs", 110 },
    { "mixerMessage", 111 },
    { "mixerOpen", 112 },
    { "mixerSetControlDetails", 113 },
    { "mmDrvInstall", 114 },
    { "mmGetCurrentTask", 115 },
    { "mmTaskBlock", 116 },
    { "mmTaskCreate", 117 },
    { "mmTaskSignal", 118 },
    { "mmTaskYield", 119 },
    { "mmioAdvance", 120 },
    { "mmioAscend", 121 },
    { "mmioClose", 122 },
    { "mmioCreateChunk", 123 },
    { "mmioDescend", 124 },
    { "mmioFlush", 125 },
    { "mmioGetInfo", 126 },
    { "mmioInstallIOProcA", 127 },
    { "mmioInstallIOProcW", 128 },
    { "mmioOpenA", 129 },
    { "mmioOpenW", 130 },
    { "mmioRead", 131 },
    { "mmioRenameA", 132 },
    { "mmioRenameW", 133 },
    { "mmioSeek", 134 },
    { "mmioSendMessage", 135 },
    { "mmioSetBuffer", 136 },
    { "mmioSetInfo", 137 },
    { "mmioStringToFOURCCA", 138 },
    { "mmioStringToFOURCCW", 139 },
    { "mmioWrite", 140 },
    { "mmsystemGetVersion", 141 },
    { "mod32Message", 142 },
    { "mxd32Message", 143 },
    { "sndPlaySoundA", 144 },
    { "sndPlaySoundW", 145 },
    { "tid32Message", 146 },
    { "timeBeginPeriod", 147 },
    { "timeEndPeriod", 148 },
    { "timeGetDevCaps", 149 },
    { "timeGetSystemTime", 150 },
    { "timeGetTime", 151 },
    { "timeKillEvent", 152 },
    { "timeSetEvent", 153 },
    { "waveInAddBuffer", 154 },
    { "waveInClose", 155 },
    { "waveInGetDevCapsA", 156 },
    { "waveInGetDevCapsW", 157 },
    { "waveInGetErrorTextA", 158 },
    { "waveInGetErrorTextW", 159 },
    { "waveInGetID", 160 },
    { "waveInGetNumDevs", 161 },
    { "waveInGetPosition", 162 },
    { "waveInMessage", 163 },
    { "waveInOpen", 164 },
    { "waveInPrepareHeader", 165 },
    { "waveInReset", 166 },
    { "waveInStart", 167 },
    { "waveInStop", 168 },
    { "waveInUnprepareHeader", 169 },
    { "waveOutBreakLoop", 170 },
    { "waveOutClose", 171 },
    { "waveOutGetDevCapsA", 172 },
    { "waveOutGetDevCapsW", 173 },
    { "waveOutGetErrorTextA", 174 },
    { "waveOutGetErrorTextW", 175 },
    { "waveOutGetID", 176 },
    { "waveOutGetNumDevs", 177 },
    { "waveOutGetPitch", 178 },
    { "waveOutGetPlaybackRate", 179 },
    { "waveOutGetPosition", 180 },
    { "waveOutGetVolume", 181 },
    { "waveOutMessage", 182 },
    { "waveOutOpen", 183 },
    { "waveOutPause", 184 },
    { "waveOutPrepareHeader", 185 },
    { "waveOutReset", 186 },
    { "waveOutRestart", 187 },
    { "waveOutSetPitch", 188 },
    { "waveOutSetPlaybackRate", 189 },
    { "waveOutSetVolume", 190 },
    { "waveOutUnprepareHeader", 191 },
    { "waveOutWrite", 192 },
    { "wid32Message", 193 },
    { "wod32Message", 194 },
    { NULL, 2 },
};
void *g_winmm_procs[WINMM_PROC_COUNT];
__asm__(".intel_syntax noprefix\n"
    ".globl _wmstub_CloseDriver\n_wmstub_CloseDriver:\n    jmp dword ptr [_g_winmm_procs + 0]\n"
    ".globl _wmstub_DefDriverProc\n_wmstub_DefDriverProc:\n    jmp dword ptr [_g_winmm_procs + 4]\n"
    ".globl _wmstub_DriverCallback\n_wmstub_DriverCallback:\n    jmp dword ptr [_g_winmm_procs + 8]\n"
    ".globl _wmstub_DrvGetModuleHandle\n_wmstub_DrvGetModuleHandle:\n    jmp dword ptr [_g_winmm_procs + 12]\n"
    ".globl _wmstub_GetDriverModuleHandle\n_wmstub_GetDriverModuleHandle:\n    jmp dword ptr [_g_winmm_procs + 16]\n"
    ".globl _wmstub_NotifyCallbackData\n_wmstub_NotifyCallbackData:\n    jmp dword ptr [_g_winmm_procs + 20]\n"
    ".globl _wmstub_OpenDriver\n_wmstub_OpenDriver:\n    jmp dword ptr [_g_winmm_procs + 24]\n"
    ".globl _wmstub_PlaySound\n_wmstub_PlaySound:\n    jmp dword ptr [_g_winmm_procs + 28]\n"
    ".globl _wmstub_PlaySoundA\n_wmstub_PlaySoundA:\n    jmp dword ptr [_g_winmm_procs + 32]\n"
    ".globl _wmstub_PlaySoundW\n_wmstub_PlaySoundW:\n    jmp dword ptr [_g_winmm_procs + 36]\n"
    ".globl _wmstub_SendDriverMessage\n_wmstub_SendDriverMessage:\n    jmp dword ptr [_g_winmm_procs + 40]\n"
    ".globl _wmstub_WOW32DriverCallback\n_wmstub_WOW32DriverCallback:\n    jmp dword ptr [_g_winmm_procs + 44]\n"
    ".globl _wmstub_WOW32ResolveMultiMediaHandle\n_wmstub_WOW32ResolveMultiMediaHandle:\n    jmp dword ptr [_g_winmm_procs + 48]\n"
    ".globl _wmstub_WOWAppExit\n_wmstub_WOWAppExit:\n    jmp dword ptr [_g_winmm_procs + 52]\n"
    ".globl _wmstub_aux32Message\n_wmstub_aux32Message:\n    jmp dword ptr [_g_winmm_procs + 56]\n"
    ".globl _wmstub_auxGetDevCapsA\n_wmstub_auxGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 60]\n"
    ".globl _wmstub_auxGetDevCapsW\n_wmstub_auxGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 64]\n"
    ".globl _wmstub_auxGetNumDevs\n_wmstub_auxGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 68]\n"
    ".globl _wmstub_auxGetVolume\n_wmstub_auxGetVolume:\n    jmp dword ptr [_g_winmm_procs + 72]\n"
    ".globl _wmstub_auxOutMessage\n_wmstub_auxOutMessage:\n    jmp dword ptr [_g_winmm_procs + 76]\n"
    ".globl _wmstub_auxSetVolume\n_wmstub_auxSetVolume:\n    jmp dword ptr [_g_winmm_procs + 80]\n"
    ".globl _wmstub_joy32Message\n_wmstub_joy32Message:\n    jmp dword ptr [_g_winmm_procs + 84]\n"
    ".globl _wmstub_joyConfigChanged\n_wmstub_joyConfigChanged:\n    jmp dword ptr [_g_winmm_procs + 88]\n"
    ".globl _wmstub_joyGetDevCapsA\n_wmstub_joyGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 92]\n"
    ".globl _wmstub_joyGetDevCapsW\n_wmstub_joyGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 96]\n"
    ".globl _wmstub_joyGetNumDevs\n_wmstub_joyGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 100]\n"
    ".globl _wmstub_joyGetPos\n_wmstub_joyGetPos:\n    jmp dword ptr [_g_winmm_procs + 104]\n"
    ".globl _wmstub_joyGetPosEx\n_wmstub_joyGetPosEx:\n    jmp dword ptr [_g_winmm_procs + 108]\n"
    ".globl _wmstub_joyGetThreshold\n_wmstub_joyGetThreshold:\n    jmp dword ptr [_g_winmm_procs + 112]\n"
    ".globl _wmstub_joyReleaseCapture\n_wmstub_joyReleaseCapture:\n    jmp dword ptr [_g_winmm_procs + 116]\n"
    ".globl _wmstub_joySetCapture\n_wmstub_joySetCapture:\n    jmp dword ptr [_g_winmm_procs + 120]\n"
    ".globl _wmstub_joySetThreshold\n_wmstub_joySetThreshold:\n    jmp dword ptr [_g_winmm_procs + 124]\n"
    ".globl _wmstub_mci32Message\n_wmstub_mci32Message:\n    jmp dword ptr [_g_winmm_procs + 128]\n"
    ".globl _wmstub_mciDriverNotify\n_wmstub_mciDriverNotify:\n    jmp dword ptr [_g_winmm_procs + 132]\n"
    ".globl _wmstub_mciDriverYield\n_wmstub_mciDriverYield:\n    jmp dword ptr [_g_winmm_procs + 136]\n"
    ".globl _wmstub_mciExecute\n_wmstub_mciExecute:\n    jmp dword ptr [_g_winmm_procs + 140]\n"
    ".globl _wmstub_mciFreeCommandResource\n_wmstub_mciFreeCommandResource:\n    jmp dword ptr [_g_winmm_procs + 144]\n"
    ".globl _wmstub_mciGetCreatorTask\n_wmstub_mciGetCreatorTask:\n    jmp dword ptr [_g_winmm_procs + 148]\n"
    ".globl _wmstub_mciGetDeviceIDA\n_wmstub_mciGetDeviceIDA:\n    jmp dword ptr [_g_winmm_procs + 152]\n"
    ".globl _wmstub_mciGetDeviceIDFromElementIDA\n_wmstub_mciGetDeviceIDFromElementIDA:\n    jmp dword ptr [_g_winmm_procs + 156]\n"
    ".globl _wmstub_mciGetDeviceIDFromElementIDW\n_wmstub_mciGetDeviceIDFromElementIDW:\n    jmp dword ptr [_g_winmm_procs + 160]\n"
    ".globl _wmstub_mciGetDeviceIDW\n_wmstub_mciGetDeviceIDW:\n    jmp dword ptr [_g_winmm_procs + 164]\n"
    ".globl _wmstub_mciGetDriverData\n_wmstub_mciGetDriverData:\n    jmp dword ptr [_g_winmm_procs + 168]\n"
    ".globl _wmstub_mciGetErrorStringA\n_wmstub_mciGetErrorStringA:\n    jmp dword ptr [_g_winmm_procs + 172]\n"
    ".globl _wmstub_mciGetErrorStringW\n_wmstub_mciGetErrorStringW:\n    jmp dword ptr [_g_winmm_procs + 176]\n"
    ".globl _wmstub_mciGetYieldProc\n_wmstub_mciGetYieldProc:\n    jmp dword ptr [_g_winmm_procs + 180]\n"
    ".globl _wmstub_mciLoadCommandResource\n_wmstub_mciLoadCommandResource:\n    jmp dword ptr [_g_winmm_procs + 184]\n"
    ".globl _wmstub_mciSendCommandA\n_wmstub_mciSendCommandA:\n    jmp dword ptr [_g_winmm_procs + 188]\n"
    ".globl _wmstub_mciSendCommandW\n_wmstub_mciSendCommandW:\n    jmp dword ptr [_g_winmm_procs + 192]\n"
    ".globl _wmstub_mciSendStringA\n_wmstub_mciSendStringA:\n    jmp dword ptr [_g_winmm_procs + 196]\n"
    ".globl _wmstub_mciSendStringW\n_wmstub_mciSendStringW:\n    jmp dword ptr [_g_winmm_procs + 200]\n"
    ".globl _wmstub_mciSetDriverData\n_wmstub_mciSetDriverData:\n    jmp dword ptr [_g_winmm_procs + 204]\n"
    ".globl _wmstub_mciSetYieldProc\n_wmstub_mciSetYieldProc:\n    jmp dword ptr [_g_winmm_procs + 208]\n"
    ".globl _wmstub_mid32Message\n_wmstub_mid32Message:\n    jmp dword ptr [_g_winmm_procs + 212]\n"
    ".globl _wmstub_midiConnect\n_wmstub_midiConnect:\n    jmp dword ptr [_g_winmm_procs + 216]\n"
    ".globl _wmstub_midiDisconnect\n_wmstub_midiDisconnect:\n    jmp dword ptr [_g_winmm_procs + 220]\n"
    ".globl _wmstub_midiInAddBuffer\n_wmstub_midiInAddBuffer:\n    jmp dword ptr [_g_winmm_procs + 224]\n"
    ".globl _wmstub_midiInClose\n_wmstub_midiInClose:\n    jmp dword ptr [_g_winmm_procs + 228]\n"
    ".globl _wmstub_midiInGetDevCapsA\n_wmstub_midiInGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 232]\n"
    ".globl _wmstub_midiInGetDevCapsW\n_wmstub_midiInGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 236]\n"
    ".globl _wmstub_midiInGetErrorTextA\n_wmstub_midiInGetErrorTextA:\n    jmp dword ptr [_g_winmm_procs + 240]\n"
    ".globl _wmstub_midiInGetErrorTextW\n_wmstub_midiInGetErrorTextW:\n    jmp dword ptr [_g_winmm_procs + 244]\n"
    ".globl _wmstub_midiInGetID\n_wmstub_midiInGetID:\n    jmp dword ptr [_g_winmm_procs + 248]\n"
    ".globl _wmstub_midiInGetNumDevs\n_wmstub_midiInGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 252]\n"
    ".globl _wmstub_midiInMessage\n_wmstub_midiInMessage:\n    jmp dword ptr [_g_winmm_procs + 256]\n"
    ".globl _wmstub_midiInOpen\n_wmstub_midiInOpen:\n    jmp dword ptr [_g_winmm_procs + 260]\n"
    ".globl _wmstub_midiInPrepareHeader\n_wmstub_midiInPrepareHeader:\n    jmp dword ptr [_g_winmm_procs + 264]\n"
    ".globl _wmstub_midiInReset\n_wmstub_midiInReset:\n    jmp dword ptr [_g_winmm_procs + 268]\n"
    ".globl _wmstub_midiInStart\n_wmstub_midiInStart:\n    jmp dword ptr [_g_winmm_procs + 272]\n"
    ".globl _wmstub_midiInStop\n_wmstub_midiInStop:\n    jmp dword ptr [_g_winmm_procs + 276]\n"
    ".globl _wmstub_midiInUnprepareHeader\n_wmstub_midiInUnprepareHeader:\n    jmp dword ptr [_g_winmm_procs + 280]\n"
    ".globl _wmstub_midiOutCacheDrumPatches\n_wmstub_midiOutCacheDrumPatches:\n    jmp dword ptr [_g_winmm_procs + 284]\n"
    ".globl _wmstub_midiOutCachePatches\n_wmstub_midiOutCachePatches:\n    jmp dword ptr [_g_winmm_procs + 288]\n"
    ".globl _wmstub_midiOutClose\n_wmstub_midiOutClose:\n    jmp dword ptr [_g_winmm_procs + 292]\n"
    ".globl _wmstub_midiOutGetDevCapsA\n_wmstub_midiOutGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 296]\n"
    ".globl _wmstub_midiOutGetDevCapsW\n_wmstub_midiOutGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 300]\n"
    ".globl _wmstub_midiOutGetErrorTextA\n_wmstub_midiOutGetErrorTextA:\n    jmp dword ptr [_g_winmm_procs + 304]\n"
    ".globl _wmstub_midiOutGetErrorTextW\n_wmstub_midiOutGetErrorTextW:\n    jmp dword ptr [_g_winmm_procs + 308]\n"
    ".globl _wmstub_midiOutGetID\n_wmstub_midiOutGetID:\n    jmp dword ptr [_g_winmm_procs + 312]\n"
    ".globl _wmstub_midiOutGetNumDevs\n_wmstub_midiOutGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 316]\n"
    ".globl _wmstub_midiOutGetVolume\n_wmstub_midiOutGetVolume:\n    jmp dword ptr [_g_winmm_procs + 320]\n"
    ".globl _wmstub_midiOutLongMsg\n_wmstub_midiOutLongMsg:\n    jmp dword ptr [_g_winmm_procs + 324]\n"
    ".globl _wmstub_midiOutMessage\n_wmstub_midiOutMessage:\n    jmp dword ptr [_g_winmm_procs + 328]\n"
    ".globl _wmstub_midiOutOpen\n_wmstub_midiOutOpen:\n    jmp dword ptr [_g_winmm_procs + 332]\n"
    ".globl _wmstub_midiOutPrepareHeader\n_wmstub_midiOutPrepareHeader:\n    jmp dword ptr [_g_winmm_procs + 336]\n"
    ".globl _wmstub_midiOutReset\n_wmstub_midiOutReset:\n    jmp dword ptr [_g_winmm_procs + 340]\n"
    ".globl _wmstub_midiOutSetVolume\n_wmstub_midiOutSetVolume:\n    jmp dword ptr [_g_winmm_procs + 344]\n"
    ".globl _wmstub_midiOutShortMsg\n_wmstub_midiOutShortMsg:\n    jmp dword ptr [_g_winmm_procs + 348]\n"
    ".globl _wmstub_midiOutUnprepareHeader\n_wmstub_midiOutUnprepareHeader:\n    jmp dword ptr [_g_winmm_procs + 352]\n"
    ".globl _wmstub_midiStreamClose\n_wmstub_midiStreamClose:\n    jmp dword ptr [_g_winmm_procs + 356]\n"
    ".globl _wmstub_midiStreamOpen\n_wmstub_midiStreamOpen:\n    jmp dword ptr [_g_winmm_procs + 360]\n"
    ".globl _wmstub_midiStreamOut\n_wmstub_midiStreamOut:\n    jmp dword ptr [_g_winmm_procs + 364]\n"
    ".globl _wmstub_midiStreamPause\n_wmstub_midiStreamPause:\n    jmp dword ptr [_g_winmm_procs + 368]\n"
    ".globl _wmstub_midiStreamPosition\n_wmstub_midiStreamPosition:\n    jmp dword ptr [_g_winmm_procs + 372]\n"
    ".globl _wmstub_midiStreamProperty\n_wmstub_midiStreamProperty:\n    jmp dword ptr [_g_winmm_procs + 376]\n"
    ".globl _wmstub_midiStreamRestart\n_wmstub_midiStreamRestart:\n    jmp dword ptr [_g_winmm_procs + 380]\n"
    ".globl _wmstub_midiStreamStop\n_wmstub_midiStreamStop:\n    jmp dword ptr [_g_winmm_procs + 384]\n"
    ".globl _wmstub_mixerClose\n_wmstub_mixerClose:\n    jmp dword ptr [_g_winmm_procs + 388]\n"
    ".globl _wmstub_mixerGetControlDetailsA\n_wmstub_mixerGetControlDetailsA:\n    jmp dword ptr [_g_winmm_procs + 392]\n"
    ".globl _wmstub_mixerGetControlDetailsW\n_wmstub_mixerGetControlDetailsW:\n    jmp dword ptr [_g_winmm_procs + 396]\n"
    ".globl _wmstub_mixerGetDevCapsA\n_wmstub_mixerGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 400]\n"
    ".globl _wmstub_mixerGetDevCapsW\n_wmstub_mixerGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 404]\n"
    ".globl _wmstub_mixerGetID\n_wmstub_mixerGetID:\n    jmp dword ptr [_g_winmm_procs + 408]\n"
    ".globl _wmstub_mixerGetLineControlsA\n_wmstub_mixerGetLineControlsA:\n    jmp dword ptr [_g_winmm_procs + 412]\n"
    ".globl _wmstub_mixerGetLineControlsW\n_wmstub_mixerGetLineControlsW:\n    jmp dword ptr [_g_winmm_procs + 416]\n"
    ".globl _wmstub_mixerGetLineInfoA\n_wmstub_mixerGetLineInfoA:\n    jmp dword ptr [_g_winmm_procs + 420]\n"
    ".globl _wmstub_mixerGetLineInfoW\n_wmstub_mixerGetLineInfoW:\n    jmp dword ptr [_g_winmm_procs + 424]\n"
    ".globl _wmstub_mixerGetNumDevs\n_wmstub_mixerGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 428]\n"
    ".globl _wmstub_mixerMessage\n_wmstub_mixerMessage:\n    jmp dword ptr [_g_winmm_procs + 432]\n"
    ".globl _wmstub_mixerOpen\n_wmstub_mixerOpen:\n    jmp dword ptr [_g_winmm_procs + 436]\n"
    ".globl _wmstub_mixerSetControlDetails\n_wmstub_mixerSetControlDetails:\n    jmp dword ptr [_g_winmm_procs + 440]\n"
    ".globl _wmstub_mmDrvInstall\n_wmstub_mmDrvInstall:\n    jmp dword ptr [_g_winmm_procs + 444]\n"
    ".globl _wmstub_mmGetCurrentTask\n_wmstub_mmGetCurrentTask:\n    jmp dword ptr [_g_winmm_procs + 448]\n"
    ".globl _wmstub_mmTaskBlock\n_wmstub_mmTaskBlock:\n    jmp dword ptr [_g_winmm_procs + 452]\n"
    ".globl _wmstub_mmTaskCreate\n_wmstub_mmTaskCreate:\n    jmp dword ptr [_g_winmm_procs + 456]\n"
    ".globl _wmstub_mmTaskSignal\n_wmstub_mmTaskSignal:\n    jmp dword ptr [_g_winmm_procs + 460]\n"
    ".globl _wmstub_mmTaskYield\n_wmstub_mmTaskYield:\n    jmp dword ptr [_g_winmm_procs + 464]\n"
    ".globl _wmstub_mmioAdvance\n_wmstub_mmioAdvance:\n    jmp dword ptr [_g_winmm_procs + 468]\n"
    ".globl _wmstub_mmioAscend\n_wmstub_mmioAscend:\n    jmp dword ptr [_g_winmm_procs + 472]\n"
    ".globl _wmstub_mmioClose\n_wmstub_mmioClose:\n    jmp dword ptr [_g_winmm_procs + 476]\n"
    ".globl _wmstub_mmioCreateChunk\n_wmstub_mmioCreateChunk:\n    jmp dword ptr [_g_winmm_procs + 480]\n"
    ".globl _wmstub_mmioDescend\n_wmstub_mmioDescend:\n    jmp dword ptr [_g_winmm_procs + 484]\n"
    ".globl _wmstub_mmioFlush\n_wmstub_mmioFlush:\n    jmp dword ptr [_g_winmm_procs + 488]\n"
    ".globl _wmstub_mmioGetInfo\n_wmstub_mmioGetInfo:\n    jmp dword ptr [_g_winmm_procs + 492]\n"
    ".globl _wmstub_mmioInstallIOProcA\n_wmstub_mmioInstallIOProcA:\n    jmp dword ptr [_g_winmm_procs + 496]\n"
    ".globl _wmstub_mmioInstallIOProcW\n_wmstub_mmioInstallIOProcW:\n    jmp dword ptr [_g_winmm_procs + 500]\n"
    ".globl _wmstub_mmioOpenA\n_wmstub_mmioOpenA:\n    jmp dword ptr [_g_winmm_procs + 504]\n"
    ".globl _wmstub_mmioOpenW\n_wmstub_mmioOpenW:\n    jmp dword ptr [_g_winmm_procs + 508]\n"
    ".globl _wmstub_mmioRead\n_wmstub_mmioRead:\n    jmp dword ptr [_g_winmm_procs + 512]\n"
    ".globl _wmstub_mmioRenameA\n_wmstub_mmioRenameA:\n    jmp dword ptr [_g_winmm_procs + 516]\n"
    ".globl _wmstub_mmioRenameW\n_wmstub_mmioRenameW:\n    jmp dword ptr [_g_winmm_procs + 520]\n"
    ".globl _wmstub_mmioSeek\n_wmstub_mmioSeek:\n    jmp dword ptr [_g_winmm_procs + 524]\n"
    ".globl _wmstub_mmioSendMessage\n_wmstub_mmioSendMessage:\n    jmp dword ptr [_g_winmm_procs + 528]\n"
    ".globl _wmstub_mmioSetBuffer\n_wmstub_mmioSetBuffer:\n    jmp dword ptr [_g_winmm_procs + 532]\n"
    ".globl _wmstub_mmioSetInfo\n_wmstub_mmioSetInfo:\n    jmp dword ptr [_g_winmm_procs + 536]\n"
    ".globl _wmstub_mmioStringToFOURCCA\n_wmstub_mmioStringToFOURCCA:\n    jmp dword ptr [_g_winmm_procs + 540]\n"
    ".globl _wmstub_mmioStringToFOURCCW\n_wmstub_mmioStringToFOURCCW:\n    jmp dword ptr [_g_winmm_procs + 544]\n"
    ".globl _wmstub_mmioWrite\n_wmstub_mmioWrite:\n    jmp dword ptr [_g_winmm_procs + 548]\n"
    ".globl _wmstub_mmsystemGetVersion\n_wmstub_mmsystemGetVersion:\n    jmp dword ptr [_g_winmm_procs + 552]\n"
    ".globl _wmstub_mod32Message\n_wmstub_mod32Message:\n    jmp dword ptr [_g_winmm_procs + 556]\n"
    ".globl _wmstub_mxd32Message\n_wmstub_mxd32Message:\n    jmp dword ptr [_g_winmm_procs + 560]\n"
    ".globl _wmstub_sndPlaySoundA\n_wmstub_sndPlaySoundA:\n    jmp dword ptr [_g_winmm_procs + 564]\n"
    ".globl _wmstub_sndPlaySoundW\n_wmstub_sndPlaySoundW:\n    jmp dword ptr [_g_winmm_procs + 568]\n"
    ".globl _wmstub_tid32Message\n_wmstub_tid32Message:\n    jmp dword ptr [_g_winmm_procs + 572]\n"
    ".globl _wmstub_timeBeginPeriod\n_wmstub_timeBeginPeriod:\n    jmp dword ptr [_g_winmm_procs + 576]\n"
    ".globl _wmstub_timeEndPeriod\n_wmstub_timeEndPeriod:\n    jmp dword ptr [_g_winmm_procs + 580]\n"
    ".globl _wmstub_timeGetDevCaps\n_wmstub_timeGetDevCaps:\n    jmp dword ptr [_g_winmm_procs + 584]\n"
    ".globl _wmstub_timeGetSystemTime\n_wmstub_timeGetSystemTime:\n    jmp dword ptr [_g_winmm_procs + 588]\n"
    ".globl _wmstub_timeGetTime\n_wmstub_timeGetTime:\n    jmp dword ptr [_g_winmm_procs + 592]\n"
    ".globl _wmstub_timeKillEvent\n_wmstub_timeKillEvent:\n    jmp dword ptr [_g_winmm_procs + 596]\n"
    ".globl _wmstub_timeSetEvent\n_wmstub_timeSetEvent:\n    jmp dword ptr [_g_winmm_procs + 600]\n"
    ".globl _wmstub_waveInAddBuffer\n_wmstub_waveInAddBuffer:\n    jmp dword ptr [_g_winmm_procs + 604]\n"
    ".globl _wmstub_waveInClose\n_wmstub_waveInClose:\n    jmp dword ptr [_g_winmm_procs + 608]\n"
    ".globl _wmstub_waveInGetDevCapsA\n_wmstub_waveInGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 612]\n"
    ".globl _wmstub_waveInGetDevCapsW\n_wmstub_waveInGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 616]\n"
    ".globl _wmstub_waveInGetErrorTextA\n_wmstub_waveInGetErrorTextA:\n    jmp dword ptr [_g_winmm_procs + 620]\n"
    ".globl _wmstub_waveInGetErrorTextW\n_wmstub_waveInGetErrorTextW:\n    jmp dword ptr [_g_winmm_procs + 624]\n"
    ".globl _wmstub_waveInGetID\n_wmstub_waveInGetID:\n    jmp dword ptr [_g_winmm_procs + 628]\n"
    ".globl _wmstub_waveInGetNumDevs\n_wmstub_waveInGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 632]\n"
    ".globl _wmstub_waveInGetPosition\n_wmstub_waveInGetPosition:\n    jmp dword ptr [_g_winmm_procs + 636]\n"
    ".globl _wmstub_waveInMessage\n_wmstub_waveInMessage:\n    jmp dword ptr [_g_winmm_procs + 640]\n"
    ".globl _wmstub_waveInOpen\n_wmstub_waveInOpen:\n    jmp dword ptr [_g_winmm_procs + 644]\n"
    ".globl _wmstub_waveInPrepareHeader\n_wmstub_waveInPrepareHeader:\n    jmp dword ptr [_g_winmm_procs + 648]\n"
    ".globl _wmstub_waveInReset\n_wmstub_waveInReset:\n    jmp dword ptr [_g_winmm_procs + 652]\n"
    ".globl _wmstub_waveInStart\n_wmstub_waveInStart:\n    jmp dword ptr [_g_winmm_procs + 656]\n"
    ".globl _wmstub_waveInStop\n_wmstub_waveInStop:\n    jmp dword ptr [_g_winmm_procs + 660]\n"
    ".globl _wmstub_waveInUnprepareHeader\n_wmstub_waveInUnprepareHeader:\n    jmp dword ptr [_g_winmm_procs + 664]\n"
    ".globl _wmstub_waveOutBreakLoop\n_wmstub_waveOutBreakLoop:\n    jmp dword ptr [_g_winmm_procs + 668]\n"
    ".globl _wmstub_waveOutClose\n_wmstub_waveOutClose:\n    jmp dword ptr [_g_winmm_procs + 672]\n"
    ".globl _wmstub_waveOutGetDevCapsA\n_wmstub_waveOutGetDevCapsA:\n    jmp dword ptr [_g_winmm_procs + 676]\n"
    ".globl _wmstub_waveOutGetDevCapsW\n_wmstub_waveOutGetDevCapsW:\n    jmp dword ptr [_g_winmm_procs + 680]\n"
    ".globl _wmstub_waveOutGetErrorTextA\n_wmstub_waveOutGetErrorTextA:\n    jmp dword ptr [_g_winmm_procs + 684]\n"
    ".globl _wmstub_waveOutGetErrorTextW\n_wmstub_waveOutGetErrorTextW:\n    jmp dword ptr [_g_winmm_procs + 688]\n"
    ".globl _wmstub_waveOutGetID\n_wmstub_waveOutGetID:\n    jmp dword ptr [_g_winmm_procs + 692]\n"
    ".globl _wmstub_waveOutGetNumDevs\n_wmstub_waveOutGetNumDevs:\n    jmp dword ptr [_g_winmm_procs + 696]\n"
    ".globl _wmstub_waveOutGetPitch\n_wmstub_waveOutGetPitch:\n    jmp dword ptr [_g_winmm_procs + 700]\n"
    ".globl _wmstub_waveOutGetPlaybackRate\n_wmstub_waveOutGetPlaybackRate:\n    jmp dword ptr [_g_winmm_procs + 704]\n"
    ".globl _wmstub_waveOutGetPosition\n_wmstub_waveOutGetPosition:\n    jmp dword ptr [_g_winmm_procs + 708]\n"
    ".globl _wmstub_waveOutGetVolume\n_wmstub_waveOutGetVolume:\n    jmp dword ptr [_g_winmm_procs + 712]\n"
    ".globl _wmstub_waveOutMessage\n_wmstub_waveOutMessage:\n    jmp dword ptr [_g_winmm_procs + 716]\n"
    ".globl _wmstub_waveOutOpen\n_wmstub_waveOutOpen:\n    jmp dword ptr [_g_winmm_procs + 720]\n"
    ".globl _wmstub_waveOutPause\n_wmstub_waveOutPause:\n    jmp dword ptr [_g_winmm_procs + 724]\n"
    ".globl _wmstub_waveOutPrepareHeader\n_wmstub_waveOutPrepareHeader:\n    jmp dword ptr [_g_winmm_procs + 728]\n"
    ".globl _wmstub_waveOutReset\n_wmstub_waveOutReset:\n    jmp dword ptr [_g_winmm_procs + 732]\n"
    ".globl _wmstub_waveOutRestart\n_wmstub_waveOutRestart:\n    jmp dword ptr [_g_winmm_procs + 736]\n"
    ".globl _wmstub_waveOutSetPitch\n_wmstub_waveOutSetPitch:\n    jmp dword ptr [_g_winmm_procs + 740]\n"
    ".globl _wmstub_waveOutSetPlaybackRate\n_wmstub_waveOutSetPlaybackRate:\n    jmp dword ptr [_g_winmm_procs + 744]\n"
    ".globl _wmstub_waveOutSetVolume\n_wmstub_waveOutSetVolume:\n    jmp dword ptr [_g_winmm_procs + 748]\n"
    ".globl _wmstub_waveOutUnprepareHeader\n_wmstub_waveOutUnprepareHeader:\n    jmp dword ptr [_g_winmm_procs + 752]\n"
    ".globl _wmstub_waveOutWrite\n_wmstub_waveOutWrite:\n    jmp dword ptr [_g_winmm_procs + 756]\n"
    ".globl _wmstub_wid32Message\n_wmstub_wid32Message:\n    jmp dword ptr [_g_winmm_procs + 760]\n"
    ".globl _wmstub_wod32Message\n_wmstub_wod32Message:\n    jmp dword ptr [_g_winmm_procs + 764]\n"
    ".globl _wmstub_ord2\n_wmstub_ord2:\n    jmp dword ptr [_g_winmm_procs + 768]\n"
    ".att_syntax prefix\n");
