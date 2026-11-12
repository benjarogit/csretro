#pragma once

#include <nitroapi/modules/engine/EngineData.h>

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
};

void EngineMini_BindXashStartup(nitroapi::EngineData* engine, bool engine_is_xash);
const EngineMiniXashStartup* EngineMini_XashStartup();

extern "C" void EngineMini_ReadXashStartup(
    int* argv_calls,
    int* mode_calls,
    int* init_calls,
    int* shutdown_calls,
    int* error_calls);
extern "C" int EngineMini_CheckParm(const char* parm);
