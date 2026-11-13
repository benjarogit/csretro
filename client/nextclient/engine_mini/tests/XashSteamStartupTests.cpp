#include "xash_startup.h"
#include "client/cl_main.h"
#include <xash_engine/xash_engine.h>
#include <xash_host/xash_host.h>
#include <goldsrc/server_api.h>
#include <gtest/gtest.h>
#include <memory>

extern bool con_initialized;
extern bool con_debuglog;
extern bool g_bCS_CZ_Flags_Initialized;
extern goldsrc::enginefuncs_t g_engfuncs;
void COM_InitArgv(int argc, char* argv[]);

namespace
{
class XashSteamStartup : public testing::Test
{
protected:
    void TearDown() override
    {
        con_initialized = false;
        con_debuglog = false;
        g_bCS_CZ_Flags_Initialized = false;
        g_engfuncs.pfnGetGameDir = nullptr;
        csr_host_filesystem_clear();
        COM_InitArgv(0, nullptr);
        EngineMini_BindXashStartup(nullptr, false);
    }
};
}

TEST_F(XashSteamStartup, RecordsTerminationWithoutCallingSteam)
{
    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);
    EXPECT_EQ(g_EngineMiniSteamTerminate, Steam_GSTerminateGameConnection);
    csr_xash_engine_mode(nullptr, nullptr, nullptr, "gl", "", 1);
    EXPECT_EQ(EngineMini_XashStartup()->steam_terminate, Steam_GSTerminateGameConnection);
    EXPECT_EQ(engine->Steam_GSTerminateGameConnection.GetHook(), nullptr);
    EXPECT_EQ(engine->CL_Stop_f.GetHook(), nullptr);

    // A call would increment this counter, even if its IP and port were zero.
    static int calls;
    calls = 0;
    const auto spy = +[](uint32_t, uint16_t) { ++calls; };
    g_EngineMiniSteamTerminate = spy;
    csr_xash_engine_mode(nullptr, nullptr, nullptr, "gl", "", 1);
    EXPECT_EQ(EngineMini_XashStartup()->steam_terminate, spy);
    EXPECT_EQ(calls, 0);

    g_EngineMiniSteamTerminate = nullptr;
    csr_xash_engine_mode(nullptr, nullptr, nullptr, "gl", "", 1);
    EXPECT_EQ(EngineMini_XashStartup()->steam_terminate, nullptr);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(EngineMini_XashStartup()->shutdown_calls, 0);
    EXPECT_EQ(EngineMini_XashStartup()->error_calls, 0);

    EngineMini_BindXashStartup(engine.get(), false);
    EXPECT_EQ(EngineMini_XashStartup()->steam_terminate, nullptr);
    EXPECT_EQ(g_EngineMiniSteamTerminate, Steam_GSTerminateGameConnection);
}
