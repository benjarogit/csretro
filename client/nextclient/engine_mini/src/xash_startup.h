#pragma once

#include <nitroapi/modules/engine/EngineData.h>

// Process replacement for the retail NET_SendPacket hook. A null value is not a call.
using EngineMiniNetSendPacketFn = void (*)(goldsrc::netsrc_t sock, int length, void* data, goldsrc::netadr_t to);

// Process replacement for the retail Netchan_CopyFileFragments hook. A null value is not a call.
using EngineMiniNetchanCopyFileFragmentsFn = goldsrc::qboolean (*)(goldsrc::netchan_t* chan);

// Process replacement for the retail SVC_TimeScale hook. A null value is not a call.
using EngineMiniSvcTimeScaleFn = void (*)();

// Process replacement for the retail SVC_SendCvarValue hook. A null value is not a call.
using EngineMiniSvcSendCvarValueFn = void (*)();

// Process replacement for the retail CL_HTTPUpdate hook. A null value is not a call.
using EngineMiniHttpUpdateFn = void (*)();

// Process replacement for the retail CL_HTTPStop_f hook. A null value is not a call.
using EngineMiniHttpStopFn = void (*)();

// Process replacement for the retail CL_StartResourceDownloading hook. A null value is not a call.
using EngineMiniStartResourceDownloadingFn = void (*)(const char* message, bool custom);

// Process replacement for the retail CL_ReadPackets hook. A null value is not a call.
using EngineMiniReadPacketsFn = void (*)();

// Process replacement for the retail CL_RequestMissingResources hook. A null value is not a call.
using EngineMiniRequestMissingResourcesFn = goldsrc::qboolean (*)();

// Process replacement for the retail CL_Disconnect hook. A null value is not a call.
using EngineMiniDisconnectFn = void (*)();

// Process replacement for the retail CL_ClearState hook. A null value is not a call.
using EngineMiniClearStateFn = void (*)(goldsrc::qboolean quiet);

// Process replacement for the retail Host_Map_f hook. A null value is not a call.
using EngineMiniHostMapFn = void (*)();

// Process replacement for the retail Host_FilterTime hook. A null value is not a call.
using EngineMiniHostFilterTimeFn = goldsrc::qboolean (*)(float time);

// Process replacement for the retail Mod_ClearAll hook. A null value is not a call.
using EngineMiniModClearAllFn = void (*)();

// Process replacement for the retail Mod_FindName hook. A null value is not a call.
using EngineMiniModFindNameFn = goldsrc::model_t* (*)(goldsrc::qboolean trackCRC, const char* name);

// Process replacement for the retail Mod_Print hook. A null value is not a call.
using EngineMiniModPrintFn = void (*)();

// Process replacement for the retail Mod_ValidateCRC hook. A null value is not a call.
using EngineMiniModValidateCRCFn = goldsrc::qboolean (*)(const char* name, goldsrc::CRC32_t crc);

// Process replacement for the retail Mod_NeedCRC hook. A null value is not a call.
using EngineMiniModNeedCRCFn = void (*)(const char* name, goldsrc::qboolean needCRC);

// Process replacement for the retail Mod_LoadModel hook. A null value is not a call.
using EngineMiniModLoadModelFn = goldsrc::model_t* (*)(goldsrc::model_t* mod, goldsrc::qboolean crash, goldsrc::qboolean trackCRC);

// Process replacement for the retail Mod_ForName hook. A null value is not a call.
using EngineMiniModForNameFn = goldsrc::model_t* (*)(const char* name, goldsrc::qboolean crash, goldsrc::qboolean trackCRC);

// Process replacement for the retail Mod_Extradata hook. A null value is not a call.
using EngineMiniModExtradataFn = void* (*)(goldsrc::model_t* mod);

// Process replacement for the retail SPR_Init hook. A null value is not a call.
using EngineMiniSprInitFn = void (*)();

// Process replacement for the retail SPR_Shutdown hook. A null value is not a call.
using EngineMiniSprShutdownFn = void (*)();

// Process replacement for the retail SPR_Shutdown_NoModelFree hook. A null value is not a call.
using EngineMiniSprShutdownNoModelFreeFn = void (*)();

// Process replacement for the retail SPR_Load hook. A null value is not a call.
using EngineMiniSprLoadFn = goldsrc::HSPRITE_t (*)(const char* pTextureName);

// Process replacement for the retail SPR_Draw hook. A null value is not a call.
using EngineMiniSprDrawFn = void (*)(int frame, int x, int y, const goldsrc::wrect_t* prc);

// Process replacement for the retail SPR_DrawAdditive hook. A null value is not a call.
using EngineMiniSprDrawAdditiveFn = void (*)(int frame, int x, int y, const goldsrc::wrect_t* prc);

// Process replacement for the retail SPR_DrawHoles hook. A null value is not a call.
using EngineMiniSprDrawHolesFn = void (*)(int frame, int x, int y, const goldsrc::wrect_t* prc);

// Process replacement for the retail SPR_DrawGeneric hook. A null value is not a call.
using EngineMiniSprDrawGenericFn = void (*)(
    int frame,
    int x,
    int y,
    const goldsrc::wrect_t* prc,
    int src,
    int dest,
    int width,
    int height);

// Process replacement for the retail SPR_EnableScissor hook. A null value is not a call.
using EngineMiniSprEnableScissorFn = void (*)(int x, int y, int width, int height);

// Process replacement for the retail SPR_DisableScissor hook. A null value is not a call.
using EngineMiniSprDisableScissorFn = void (*)();

// Process replacement for the retail SPR_Set hook. A null value is not a call.
using EngineMiniSprSetFn = void (*)(goldsrc::HSPRITE_t hsprite, int r, int g, int b);

// Process replacement for the retail SPR_Width hook. A null value is not a call.
using EngineMiniSprWidthFn = int (*)(goldsrc::HSPRITE_t hsprite, int frame);

// Process replacement for the retail SPR_Height hook. A null value is not a call.
using EngineMiniSprHeightFn = int (*)(goldsrc::HSPRITE_t hsprite, int frame);

// Process replacement for the retail SPR_Frames hook. A null value is not a call.
using EngineMiniSprFramesFn = int (*)(goldsrc::HSPRITE_t hsprite);

// Process replacement for the retail SPR_GetModelPointer hook. A null value is not a call.
using EngineMiniSprGetModelPointerFn = const goldsrc::model_t* (*)(goldsrc::HSPRITE_t hsprite);

/*
 * Record of the Xash startup sequence. engine_mini fills it from the
 * xash_engine callbacks when the matching retail hook is absent.
 */
struct EngineMiniXashStartup
{
    bool engine_is_xash{};
    int argv_calls{};
    int mode_calls{};
    int init_calls{};
    int initializing_calls{};
    int shutdown_calls{};
    int error_calls{};
    int net_send_calls{};
    int netchan_copy_calls{};
    int svc_timescale_calls{};
    int svc_sendcvarvalue_calls{};
    int http_update_calls{};
    int http_stop_calls{};
    int start_resource_calls{};
    int read_packets_calls{};
    int request_missing_calls{};
    int disconnect_calls{};
    int clear_state_calls{};
    int host_map_calls{};
    int host_filter_calls{};
    int mod_clear_calls{};
    int mod_find_calls{};
    int mod_print_calls{};
    int mod_validate_calls{};
    int mod_need_calls{};
    int mod_load_calls{};
    int mod_forname_calls{};
    int mod_extradata_calls{};
    int spr_init_calls{};
    int spr_shutdown_calls{};
    int spr_shutdown_nomodelfree_calls{};
    int spr_load_calls{};
    int spr_draw_calls{};
    int spr_draw_additive_calls{};
    int spr_draw_holes_calls{};
    int spr_draw_generic_calls{};
    int spr_enable_scissor_calls{};
    int spr_disable_scissor_calls{};
    int spr_set_calls{};
    int spr_width_calls{};
    int spr_height_calls{};
    int spr_frames_calls{};
    int spr_get_model_pointer_calls{};
    int argc{};
    int mode_result{};
    int dedicated{};
    int init_result{};
    void* window{};
    void* device{};
    void* context{};
    char driver[64]{};
    char mode_cmdline[256]{};
    char init_cmdline[256]{};
    char basedir[256]{};
    char error[256]{};
    EngineMiniNetSendPacketFn net_send_packet{};
    EngineMiniNetchanCopyFileFragmentsFn netchan_copy{};
    EngineMiniSvcTimeScaleFn svc_timescale{};
    EngineMiniSvcSendCvarValueFn svc_sendcvarvalue{};
    EngineMiniHttpUpdateFn http_update{};
    EngineMiniHttpStopFn http_stop{};
    EngineMiniStartResourceDownloadingFn start_resource{};
    EngineMiniReadPacketsFn read_packets{};
    EngineMiniRequestMissingResourcesFn request_missing{};
    EngineMiniDisconnectFn disconnect{};
    EngineMiniClearStateFn clear_state{};
    EngineMiniHostMapFn host_map{};
    EngineMiniHostFilterTimeFn host_filter{};
    EngineMiniModClearAllFn mod_clear{};
    EngineMiniModFindNameFn mod_find{};
    EngineMiniModPrintFn mod_print{};
    EngineMiniModValidateCRCFn mod_validate{};
    EngineMiniModNeedCRCFn mod_need{};
    EngineMiniModLoadModelFn mod_load{};
    EngineMiniModForNameFn mod_forname{};
    EngineMiniModExtradataFn mod_extradata{};
    EngineMiniSprInitFn spr_init{};
    EngineMiniSprShutdownFn spr_shutdown{};
    EngineMiniSprShutdownNoModelFreeFn spr_shutdown_nomodelfree{};
    EngineMiniSprLoadFn spr_load{};
    EngineMiniSprDrawFn spr_draw{};
    EngineMiniSprDrawAdditiveFn spr_draw_additive{};
    EngineMiniSprDrawHolesFn spr_draw_holes{};
    EngineMiniSprDrawGenericFn spr_draw_generic{};
    EngineMiniSprEnableScissorFn spr_enable_scissor{};
    EngineMiniSprDisableScissorFn spr_disable_scissor{};
    EngineMiniSprSetFn spr_set{};
    EngineMiniSprWidthFn spr_width{};
    EngineMiniSprHeightFn spr_height{};
    EngineMiniSprFramesFn spr_frames{};
    EngineMiniSprGetModelPointerFn spr_get_model_pointer{};
};

// Defaults to NET_SendPacket. Init stores the pointer and does not call it.
extern EngineMiniNetSendPacketFn g_EngineMiniNetSendPacket;

// Defaults to Netchan_CopyFileFragments. Init stores the pointer and does not call it.
extern EngineMiniNetchanCopyFileFragmentsFn g_EngineMiniNetchanCopyFileFragments;

// Defaults to CL_Parse_Timescale. Init stores the pointer and does not call it.
extern EngineMiniSvcTimeScaleFn g_EngineMiniSvcTimeScale;

// Defaults to CL_Send_CvarValue. Init stores the pointer and does not call it.
extern EngineMiniSvcSendCvarValueFn g_EngineMiniSvcSendCvarValue;

// Defaults to CL_HTTPUpdate. Init stores the pointer and does not call it.
extern EngineMiniHttpUpdateFn g_EngineMiniHttpUpdate;

// Defaults to CL_HTTPStop_f. Init stores the pointer and does not call it.
extern EngineMiniHttpStopFn g_EngineMiniHttpStop;

// Defaults to CL_StartResourceDownloading. Init stores the pointer and does not call it.
extern EngineMiniStartResourceDownloadingFn g_EngineMiniStartResourceDownloading;

// Defaults to CL_ReadPackets. Init stores the pointer and does not call it.
extern EngineMiniReadPacketsFn g_EngineMiniReadPackets;

// Defaults to CL_RequestMissingResources. Init stores the pointer and does not call it.
extern EngineMiniRequestMissingResourcesFn g_EngineMiniRequestMissingResources;

// Defaults to CL_Disconnect. Init stores the pointer and does not call it.
extern EngineMiniDisconnectFn g_EngineMiniDisconnect;

// Defaults to CL_ClearState. Init stores the pointer and does not call it.
extern EngineMiniClearStateFn g_EngineMiniClearState;

// Defaults to Host_Map_f. Init stores the pointer and does not call it.
extern EngineMiniHostMapFn g_EngineMiniHostMap;

// Defaults to Host_FilterTime. Init stores the pointer and does not call it.
extern EngineMiniHostFilterTimeFn g_EngineMiniHostFilterTime;

// Defaults to Mod_ClearAll. Init stores the pointer and does not call it.
extern EngineMiniModClearAllFn g_EngineMiniModClearAll;

// Defaults to Mod_FindName. Init stores the pointer and does not call it.
extern EngineMiniModFindNameFn g_EngineMiniModFindName;

// Defaults to Mod_Print. Init stores the pointer and does not call it.
extern EngineMiniModPrintFn g_EngineMiniModPrint;

// Defaults to Mod_ValidateCRC. Init stores the pointer and does not call it.
extern EngineMiniModValidateCRCFn g_EngineMiniModValidateCRC;

// Defaults to Mod_NeedCRC. Init stores the pointer and does not call it.
extern EngineMiniModNeedCRCFn g_EngineMiniModNeedCRC;

// Defaults to Mod_LoadModel. Init stores the pointer and does not call it.
extern EngineMiniModLoadModelFn g_EngineMiniModLoadModel;

// Defaults to Mod_ForName. Init stores the pointer and does not call it.
extern EngineMiniModForNameFn g_EngineMiniModForName;

// Defaults to Mod_Extradata. Init stores the pointer and does not call it.
extern EngineMiniModExtradataFn g_EngineMiniModExtradata;

// Defaults to SPR_Init. Init stores the pointer and does not call it.
extern EngineMiniSprInitFn g_EngineMiniSprInit;

// Defaults to SPR_Shutdown. Init stores the pointer and does not call it.
extern EngineMiniSprShutdownFn g_EngineMiniSprShutdown;

// Defaults to SPR_Shutdown_NoModelFree. Init stores the pointer and does not call it.
extern EngineMiniSprShutdownNoModelFreeFn g_EngineMiniSprShutdownNoModelFree;

// Defaults to SPR_Load. Init stores the pointer and does not call it.
extern EngineMiniSprLoadFn g_EngineMiniSprLoad;

// Defaults to SPR_Draw. Init stores the pointer and does not call it.
extern EngineMiniSprDrawFn g_EngineMiniSprDraw;

// Defaults to SPR_DrawAdditive. Init stores the pointer and does not call it.
extern EngineMiniSprDrawAdditiveFn g_EngineMiniSprDrawAdditive;

// Defaults to SPR_DrawHoles. Init stores the pointer and does not call it.
extern EngineMiniSprDrawHolesFn g_EngineMiniSprDrawHoles;

// Defaults to SPR_DrawGeneric. Init stores the pointer and does not call it.
extern EngineMiniSprDrawGenericFn g_EngineMiniSprDrawGeneric;

// Defaults to SPR_EnableScissor. Init stores the pointer and does not call it.
extern EngineMiniSprEnableScissorFn g_EngineMiniSprEnableScissor;

// Defaults to SPR_DisableScissor. Init stores the pointer and does not call it.
extern EngineMiniSprDisableScissorFn g_EngineMiniSprDisableScissor;

// Defaults to SPR_Set. Init stores the pointer and does not call it.
extern EngineMiniSprSetFn g_EngineMiniSprSet;

// Defaults to SPR_Width. Init stores the pointer and does not call it.
extern EngineMiniSprWidthFn g_EngineMiniSprWidth;

// Defaults to SPR_Height. Init stores the pointer and does not call it.
extern EngineMiniSprHeightFn g_EngineMiniSprHeight;

// Defaults to SPR_Frames. Init stores the pointer and does not call it.
extern EngineMiniSprFramesFn g_EngineMiniSprFrames;

// Defaults to SPR_GetModelPointer. Init stores the pointer and does not call it.
extern EngineMiniSprGetModelPointerFn g_EngineMiniSprGetModelPointer;

void EngineMini_BindXashStartup(nitroapi::EngineData* engine, bool engine_is_xash);
const EngineMiniXashStartup* EngineMini_XashStartup();
void EngineMini_NoteHostMapCall();
void EngineMini_NoteHostFilterCall();

extern "C" void EngineMini_ReadXashStartup(
    int* argv_calls,
    int* mode_calls,
    int* init_calls,
    int* shutdown_calls,
    int* error_calls);
extern "C" int EngineMini_CheckParm(const char* parm);
extern "C" void EngineMini_ReadFrameGuard(int* host_filter_calls, int* stored_functions, int* nitro_hooks);
extern "C" int EngineMini_HostMapCalls(void);
extern "C" goldsrc::client_state_t* EngineMini_ClientState(void);
// Writes r_worldentity->model from the loaded world model. Does not call R_NewMap or R_DrawWorld.
extern "C" int EngineMini_BindRendererWorld(void);
extern "C" goldsrc::cl_entity_t* EngineMini_RendererWorldEntity(void);
extern "C" int EngineMini_RNewMapHooked(void);
extern "C" int EngineMini_RNewMapCalls(void);
// True when the retail R_DrawWorld hook is installed. This does not install it.
extern "C" int EngineMini_RDrawWorldHooked(void);
// Calls of the retail R_DrawWorld wrapper. The direct world draw does not increment this.
extern "C" int EngineMini_RDrawWorldCalls(void);
// True when r_worldentity->model is set. Does not install or call R_DrawWorld.
extern "C" int EngineMini_RendererWorldBound(void);
// Walks the bound world's faces through surfedges and records one primitive.
// Does not call the retail R_DrawWorld hook, does not open a window, and does
// not write the color buffer.
extern "C" int EngineMini_DrawRendererWorld(void);
extern "C" int EngineMini_WorldDrawCalls(void);
extern "C" int EngineMini_WorldDrawVertexCount(void);
extern "C" const float* EngineMini_WorldDrawVertices(void);
