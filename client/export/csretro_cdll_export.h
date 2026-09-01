#ifndef CSRETRO_CDLL_EXPORT_H
#define CSRETRO_CDLL_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Einziger Xash-Client-Einstieg. p zeigt auf engine/engine/cdll_exp.h → cldll_func_t.
 * Das Layout liegt in csretro_cdll_export.cpp (nicht Ref-A-APIProxy-cldll_func_t).
 */
int GetClientAPI( void *p );
#ifdef __cplusplus
}
#endif

/*
 * CS Retro — Xash-Client-Export.
 *
 * Lader: engine/engine/client/dll_int/cl_game.c → CL_LoadProgs
 *   1. Client-Lib laden (cstrike/cl_dlls/client_amd64.so bzw. client_arm64 / client.dll)
 *   2. GetClientAPI(cldll_func_t *) — Ref-A-F() ist entfernt (inkompatibles Layout)
 *   3. pfnInitialize(&gEngfuncs, CLDLL_INTERFACE_VERSION) muss 1 liefern
 *
 * Pflicht-Namen bleiben zusätzlich als einzelne Symbole exportiert (Xash-Fallback).
 * Menü ist ein zweiter Ladeweg (GetMenuAPI), nicht dieser Export.
 *
 * Kein Steam, kein NitroApi, kein 8684, keine zweite Client-Lib, kein 32-Bit.
 */

#endif
