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

void EngineMini_BindXashStartup(nitroapi::EngineData* engine, bool engine_is_xash);
const EngineMiniXashStartup* EngineMini_XashStartup();
void EngineMini_NoteHostFilterCall();

extern "C" void EngineMini_ReadXashStartup(
    int* argv_calls,
    int* mode_calls,
    int* init_calls,
    int* shutdown_calls,
    int* error_calls);
extern "C" int EngineMini_CheckParm(const char* parm);
extern "C" void EngineMini_ReadFrameGuard(int* host_filter_calls, int* stored_functions, int* nitro_hooks);
