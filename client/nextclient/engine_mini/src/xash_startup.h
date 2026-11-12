#pragma once

#include <nitroapi/modules/engine/EngineData.h>

// Process replacement for the retail NET_SendPacket hook. A null value is not a call.
using EngineMiniNetSendPacketFn = void (*)(goldsrc::netsrc_t sock, int length, void* data, goldsrc::netadr_t to);

// Process replacement for the retail Netchan_CopyFileFragments hook. A null value is not a call.
using EngineMiniNetchanCopyFileFragmentsFn = goldsrc::qboolean (*)(goldsrc::netchan_t* chan);

// Process replacement for the retail SVC_TimeScale hook. A null value is not a call.
using EngineMiniSvcTimeScaleFn = void (*)();

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
};

// Defaults to NET_SendPacket. Init stores the pointer and does not call it.
extern EngineMiniNetSendPacketFn g_EngineMiniNetSendPacket;

// Defaults to Netchan_CopyFileFragments. Init stores the pointer and does not call it.
extern EngineMiniNetchanCopyFileFragmentsFn g_EngineMiniNetchanCopyFileFragments;

// Defaults to CL_Parse_Timescale. Init stores the pointer and does not call it.
extern EngineMiniSvcTimeScaleFn g_EngineMiniSvcTimeScale;

void EngineMini_BindXashStartup(nitroapi::EngineData* engine, bool engine_is_xash);
const EngineMiniXashStartup* EngineMini_XashStartup();

extern "C" void EngineMini_ReadXashStartup(
    int* argv_calls,
    int* mode_calls,
    int* init_calls,
    int* shutdown_calls,
    int* error_calls);
extern "C" int EngineMini_CheckParm(const char* parm);
