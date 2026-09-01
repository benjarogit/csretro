#ifndef CSRETRO_CDLL_EXPORT_H
#define CSRETRO_CDLL_EXPORT_H

/*
 * CS Retro — Xash-Client-Exportvertrag (Phase 2).
 *
 * Keine Funktionsrümpfe, kein Body. Phase 3 füllt GetClientAPI und die
 * Pflicht-Namen und hängt den A1-Body plus NextClient-Funktionen daran.
 *
 * Lader: engine/engine/client/dll_int/cl_game.c → CL_LoadProgs
 * Struct: engine/engine/cdll_exp.h → cldll_func_t
 *
 * Reihenfolge:
 *   1. Client-Lib laden (cstrike/cl_dlls/client.so bzw. client.dll)
 *   2. GetClientAPI(cldll_func_t *) oder F, sonst alle Namen aus cdll_exports[]
 *   3. pfnInitialize(&gEngfuncs, CLDLL_INTERFACE_VERSION) muss 1 liefern
 *
 * Pflicht-Namen (cdll_exports):
 *   Initialize, HUD_VidInit, HUD_Init, HUD_Shutdown, HUD_Redraw,
 *   HUD_UpdateClientData, HUD_Reset, HUD_PlayerMove, HUD_PlayerMoveInit,
 *   HUD_PlayerMoveTexture, HUD_ConnectionlessPacket, HUD_GetHullBounds,
 *   HUD_Frame, HUD_PostRunCmd, HUD_Key_Event, HUD_AddEntity,
 *   HUD_CreateEntities, HUD_StudioEvent, HUD_TxferLocalOverrides,
 *   HUD_ProcessPlayerState, HUD_TxferPredictionData, HUD_TempEntUpdate,
 *   HUD_DrawNormalTriangles, HUD_DrawTransparentTriangles, HUD_GetUserEntity,
 *   Demo_ReadBuffer, CAM_Think, CL_IsThirdPerson, CL_CameraOffset,
 *   CL_CreateMove, IN_*, V_CalcRefdef, KB_Find
 *
 * Optional (cdll_new_exports): Studio, Director, Voice, ChatInput,
 * Xash-Render/Clip/Touch/Sound.
 *
 * Menü ist ein zweiter Ladeweg (GetMenuAPI), nicht dieser Export.
 *
 * Kein Steam, kein NitroApi, kein 8684, keine parallele zweite Client-Lib.
 */

#endif
