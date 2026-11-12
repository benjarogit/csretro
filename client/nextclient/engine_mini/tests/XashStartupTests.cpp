#include "xash_startup.h"

#include "xash_host/xash_host.h"

#include <xash_engine/xash_engine.h>

#include <goldsrc/cvar.h>
#include <goldsrc/engine/video.h>
#include <goldsrc/limits.h>
#include <goldsrc/server_api.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <functional>

#include "client/spriteapi.h"
#include "common/model.h"

#include <algorithm>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

int COM_CheckParm(const char* parm);
void COM_InitArgv(int argc, char* argv[]);
extern bool con_initialized;
extern bool con_debuglog;
extern bool g_bCS_CZ_Flags_Initialized;
extern bool g_bIsCStrike;
extern bool g_bIsCZero;
extern bool g_bIsCZeroRitual;
extern bool g_bIsTerrorStrike;
extern bool g_bIsTFC;
extern bool g_bIsHL1;
extern goldsrc::enginefuncs_t g_engfuncs;

void NET_SendPacket(goldsrc::netsrc_t sock, int length, void* data, goldsrc::netadr_t to);
goldsrc::qboolean Netchan_CopyFileFragments(goldsrc::netchan_t* chan);
void CL_Parse_Timescale();
void CL_Send_CvarValue();
void CL_HTTPUpdate();
void CL_HTTPStop_f();
void CL_StartResourceDownloading(const char* message, bool custom);
void CL_ReadPackets();
goldsrc::qboolean CL_RequestMissingResources();
void CL_Disconnect();
void CL_ClearState(goldsrc::qboolean quiet);
void Host_Map_f();
goldsrc::qboolean Host_FilterTime(float time);
extern double oldrealtime;
extern double* realtime;
extern double* host_frametime;
extern goldsrc::cvar_t* host_framerate;
extern goldsrc::cvar_t* sys_timescale;
extern goldsrc::cvar_t** p_cvar_vars;
extern goldsrc::mod_known_info_t mod_known_info[];
extern SPRITELIST* gSpriteList;
extern int gSpriteCount;
extern goldsrc::msprite_t* gpSprite;
extern goldsrc::HSPRITE_t ghCrosshair;
extern std::unordered_set<std::string> gLoadedSpriteNames;
extern goldsrc::viddef_t* p_vid;
void EnableScissorTest(int x, int y, int width, int height);
void EngineMini_ReadScissorTest(int* x, int* y, int* width, int* height, bool* enabled);

namespace
{

    bool MapsContain(const std::filesystem::path& fragment)
    {
        std::ifstream maps("/proc/self/maps");
        std::string line;

        while (std::getline(maps, line))
        {
            if (line.find(fragment.string()) != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    bool ReadOpenSockets(std::vector<std::string>* links)
    {
        links->clear();
        std::error_code error;
        std::filesystem::directory_iterator it("/proc/self/fd", error);
        if (error)
        {
            return false;
        }
        const std::filesystem::directory_iterator end;
        for (; !error && it != end; it.increment(error))
        {
            std::error_code link_error;
            const std::filesystem::path target = std::filesystem::read_symlink(it->path(), link_error);
            if (!link_error && target.string().rfind("socket:", 0) == 0)
            {
                links->push_back(target.string());
            }
        }
        if (error)
        {
            return false;
        }
        std::sort(links->begin(), links->end());
        return true;
    }

    std::string CaptureStandardOutput(const std::function<void()>& body)
    {
        int pipe_fds[2] = {-1, -1};
        if (pipe(pipe_fds) != 0)
        {
            body();
            return "pipe-failed";
        }
        const int saved = dup(STDOUT_FILENO);
        if (saved < 0)
        {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
            body();
            return "pipe-failed";
        }
        fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK);
        dup2(pipe_fds[1], STDOUT_FILENO);
        body();
        fflush(stdout);
        dup2(saved, STDOUT_FILENO);
        close(saved);
        close(pipe_fds[1]);
        std::string text;
        char buffer[512];
        ssize_t count = 0;
        while ((count = read(pipe_fds[0], buffer, sizeof(buffer))) > 0)
        {
            text.append(buffer, static_cast<size_t>(count));
        }
        close(pipe_fds[0]);
        return text;
    }

    std::string ReadFileBytes(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        std::string bytes;
        char byte = '\0';
        while (input.get(byte))
        {
            bytes.push_back(byte);
        }
        return bytes;
    }

    std::vector<std::string> RegularFileNames(const std::filesystem::path& root)
    {
        std::vector<std::string> names;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root))
        {
            if (entry.is_regular_file())
            {
                names.push_back(entry.path().filename().string());
            }
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    class WorkingDirectory
    {
        std::filesystem::path previous_;

    public:
        explicit WorkingDirectory(const std::filesystem::path& next) :
            previous_(std::filesystem::current_path())
        {
            std::filesystem::current_path(next);
        }

        ~WorkingDirectory()
        {
            std::filesystem::current_path(previous_);
        }
    };

    // Host_FilterTime writes the process realtime globals. Point them at the planted values.
    class ProcessHostClock
    {
        double* previous_realtime_;
        double* previous_frametime_;
        goldsrc::cvar_t* previous_framerate_;

    public:
        ProcessHostClock(double* host_time, double* frame_time, goldsrc::cvar_t* framerate) :
            previous_realtime_(realtime),
            previous_frametime_(host_frametime),
            previous_framerate_(host_framerate)
        {
            realtime = host_time;
            host_frametime = frame_time;
            host_framerate = framerate;
        }

        ~ProcessHostClock()
        {
            realtime = previous_realtime_;
            host_frametime = previous_frametime_;
            host_framerate = previous_framerate_;
        }
    };

    // Mod_ClearAll rewrites known model slots. Restore the two slots after the test.
    class PlantedModelList
    {
        int previous_count_;
        goldsrc::model_t slot0_;
        goldsrc::model_t slot1_;

    public:
        PlantedModelList() :
            previous_count_(mod_numknown),
            slot0_(mod_known[0]),
            slot1_(mod_known[1])
        {
        }

        ~PlantedModelList()
        {
            mod_known[0] = slot0_;
            mod_known[1] = slot1_;
            mod_numknown = previous_count_;
        }
    };

    class XashStartup : public testing::Test
    {
    protected:
        void TearDown() override
        {
            con_initialized = false;
            con_debuglog = false;
            g_bCS_CZ_Flags_Initialized = false;
            g_bIsCStrike = false;
            g_bIsCZero = false;
            g_bIsCZeroRitual = false;
            g_bIsTerrorStrike = false;
            g_bIsTFC = false;
            g_bIsHL1 = false;
            g_engfuncs.pfnGetGameDir = nullptr;
            sys_timescale = nullptr;
            p_cvar_vars = nullptr;
            csr_host_filesystem_clear();
            COM_InitArgv(0, nullptr);
            EngineMini_BindXashStartup(nullptr, false);
        }
    };

} // namespace

TEST_F(XashStartup, AbsentHooksRecordTheSequenceAndInitArgv)
{
    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    const EngineMiniXashStartup* record = EngineMini_XashStartup();
    ASSERT_NE(record, nullptr);
    EXPECT_TRUE(record->engine_is_xash);

    con_initialized = false;
    con_debuglog = true;

    char arg0[] = "xash";
    char arg1[] = "-console";
    char* argv[] = {arg0, arg1, nullptr};
    csr_xash_engine_argv(2, argv);

    record = EngineMini_XashStartup();
    EXPECT_EQ(record->argv_calls, 1);
    EXPECT_EQ(record->argc, 2);
    EXPECT_NE(COM_CheckParm("-console"), 0);

    int window = 0;
    int device = 0;
    int context = 0;
    csr_xash_engine_mode(&window, &device, &context, "gl", "-windowed", 1);

    record = EngineMini_XashStartup();
    EXPECT_EQ(record->mode_calls, 1);
    EXPECT_EQ(record->window, &window);
    EXPECT_EQ(record->device, &device);
    EXPECT_EQ(record->context, &context);
    EXPECT_STREQ(record->driver, "gl");
    EXPECT_STREQ(record->mode_cmdline, "-windowed");
    EXPECT_EQ(record->mode_result, 1);
    EXPECT_EQ(record->initializing_calls, 1);
    EXPECT_EQ(record->shutdown_calls, 0);
    EXPECT_EQ(record->error_calls, 0);
    EXPECT_TRUE(con_initialized);
    EXPECT_FALSE(con_debuglog);
    EXPECT_FALSE(g_bCS_CZ_Flags_Initialized);
    EXPECT_EQ(g_engfuncs.pfnGetGameDir, nullptr);
    EXPECT_NE(engine->client_state, nullptr);
    EXPECT_NE(engine->cl_enginefunc, nullptr);
    EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
    EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);

    char cmdline[] = "-game cstrike";
    char basedir[] = "valve";
    csr_xash_engine_init_game(cmdline, basedir, &window, 0, 1);
    csr_xash_engine_shutdown();
    csr_xash_engine_error("fatal");

    record = EngineMini_XashStartup();
    EXPECT_EQ(record->init_calls, 1);
    EXPECT_EQ(record->dedicated, 0);
    EXPECT_EQ(record->init_result, 1);
    EXPECT_STREQ(record->init_cmdline, "-game cstrike");
    EXPECT_STREQ(record->basedir, "valve");
    EXPECT_EQ(record->shutdown_calls, 1);
    EXPECT_EQ(record->error_calls, 1);
    EXPECT_STREQ(record->error, "fatal");
}

TEST_F(XashStartup, NullBindingIsNotACall)
{
    EngineMini_BindXashStartup(nullptr, true);
    con_initialized = false;
    con_debuglog = true;

    char arg0[] = "xash";
    char* argv[] = {arg0, nullptr};
    int window = 0;
    char cmdline[] = "line";
    char basedir[] = "valve";

    csr_xash_engine_argv(1, argv);
    csr_xash_engine_mode(&window, nullptr, nullptr, "gl", nullptr, 0);
    csr_xash_engine_init_game(cmdline, basedir, &window, 1, 0);
    csr_xash_engine_shutdown();
    csr_xash_engine_error("fatal");

    const EngineMiniXashStartup* record = EngineMini_XashStartup();
    EXPECT_TRUE(record->engine_is_xash);
    EXPECT_EQ(record->argv_calls, 0);
    EXPECT_EQ(record->mode_calls, 0);
    EXPECT_EQ(record->init_calls, 0);
    EXPECT_EQ(record->shutdown_calls, 0);
    EXPECT_EQ(record->error_calls, 0);
    EXPECT_EQ(record->initializing_calls, 0);
    EXPECT_FALSE(con_initialized);
    EXPECT_TRUE(con_debuglog);
}

TEST_F(XashStartup, ConInitReadsCondebugAndLeavesTheLog)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-con-init";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::filesystem::path log = root / "qconsole.log";
    {
        std::ofstream stream(log);
        ASSERT_TRUE(stream.is_open());
        stream << "keep";
    }

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);
    con_initialized = false;
    con_debuglog = false;

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-condebug";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        EXPECT_TRUE(con_initialized);
        EXPECT_TRUE(con_debuglog);
        EXPECT_NE(COM_CheckParm("-condebug"), 0);
        EXPECT_EQ(EngineMini_XashStartup()->initializing_calls, 1);
        EXPECT_EQ(EngineMini_XashStartup()->shutdown_calls, 0);
        EXPECT_EQ(EngineMini_XashStartup()->error_calls, 0);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_FALSE(g_bCS_CZ_Flags_Initialized);
        EXPECT_EQ(g_engfuncs.pfnGetGameDir, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::ifstream kept(log);
        std::string text;
        ASSERT_TRUE(static_cast<bool>(kept));
        std::getline(kept, text);
        EXPECT_EQ(text, "keep");
    }

    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, WiredCallDoesNotOpenHwSo)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-startup-wire";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        char cmdline[] = "-game cstrike";
        char basedir[] = "valve";

        con_initialized = false;
        con_debuglog = true;
        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        csr_xash_engine_init_game(cmdline, basedir, &window, 0, 1);
        EXPECT_EQ(EngineMini_XashStartup()->initializing_calls, 1);
        EXPECT_TRUE(con_initialized);
        EXPECT_FALSE(con_debuglog);
        EXPECT_FALSE(g_bCS_CZ_Flags_Initialized);
        EXPECT_EQ(g_engfuncs.pfnGetGameDir, nullptr);
        EXPECT_EQ(EngineMini_XashStartup()->shutdown_calls, 0);
        EXPECT_EQ(EngineMini_XashStartup()->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        csr_xash_engine_shutdown();
        csr_xash_engine_error("fatal");
    }

    EXPECT_EQ(EngineMini_XashStartup()->argv_calls, 1);
    EXPECT_NE(COM_CheckParm("-console"), 0);
    EXPECT_FALSE(MapsContain(root / "hw.so"));

    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SetCStrikeFlagsReadsTheMountedDirectory)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-gamedir";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::filesystem::path game = root / "cstrike";
    std::filesystem::create_directory(game);

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);
    csr_host_filesystem_clear();

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        csr_xash_engine_argv(2, argv);

        EXPECT_EQ(csr_host_filesystem_set_root(""), 0);
        EXPECT_EQ(csr_host_filesystem_set_root("/no/such/csretro-content-root"), 0);
        EXPECT_EQ(csr_host_filesystem_mounted(), 0);

        g_bCS_CZ_Flags_Initialized = false;
        g_bIsCStrike = false;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        EXPECT_EQ(EngineMini_XashStartup()->initializing_calls, 1);
        EXPECT_TRUE(con_initialized);
        EXPECT_EQ(g_engfuncs.pfnGetGameDir, nullptr);
        EXPECT_FALSE(g_bCS_CZ_Flags_Initialized);
        EXPECT_FALSE(g_bIsCStrike);
        EXPECT_EQ(EngineMini_XashStartup()->shutdown_calls, 0);
        EXPECT_EQ(EngineMini_XashStartup()->error_calls, 0);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        ASSERT_EQ(csr_host_filesystem_set_root(root.c_str()), 1);
        g_bCS_CZ_Flags_Initialized = false;
        g_bIsCStrike = false;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        ASSERT_NE(g_engfuncs.pfnGetGameDir, nullptr);
        Dl_info game_dir{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(g_engfuncs.pfnGetGameDir), &game_dir), 0);
        ASSERT_NE(game_dir.dli_fname, nullptr);
        EXPECT_EQ(std::string(game_dir.dli_fname).find("hw.so"), std::string::npos);

        char written[goldsrc::MAX_OSPATH];
        written[0] = '\0';
        g_engfuncs.pfnGetGameDir(written);
        EXPECT_EQ(std::string(written), root.filename().string());
        EXPECT_NE(std::string(written), "cstrike");
        EXPECT_TRUE(g_bCS_CZ_Flags_Initialized);
        EXPECT_FALSE(g_bIsCStrike);
        EXPECT_FALSE(g_bIsCZero);
        EXPECT_FALSE(g_bIsHL1);

        ASSERT_EQ(csr_host_filesystem_set_root(game.c_str()), 1);
        g_bCS_CZ_Flags_Initialized = false;
        g_bIsCStrike = false;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        ASSERT_NE(g_engfuncs.pfnGetGameDir, nullptr);
        g_engfuncs.pfnGetGameDir(written);
        EXPECT_STREQ(written, "cstrike");
        EXPECT_TRUE(g_bCS_CZ_Flags_Initialized);
        EXPECT_TRUE(g_bIsCStrike);
        EXPECT_FALSE(g_bIsHL1);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(EngineMini_XashStartup()->shutdown_calls, 0);
        EXPECT_EQ(EngineMini_XashStartup()->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        csr_host_filesystem_clear();
        g_bCS_CZ_Flags_Initialized = false;
        g_bIsCStrike = false;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        EXPECT_EQ(g_engfuncs.pfnGetGameDir, nullptr);
        EXPECT_FALSE(g_bCS_CZ_Flags_Initialized);
        EXPECT_FALSE(g_bIsCStrike);
    }

    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    csr_host_filesystem_clear();
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, NetSendPacketUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-net-send";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->net_send_packet, nullptr);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(g_EngineMiniNetSendPacket, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->NET_SendPacket.GetHook(), nullptr);
        EXPECT_EQ(engine->Netchan_CopyFileFragments.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info send_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->net_send_packet), &send_info), 0);
        ASSERT_NE(send_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(send_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);

        g_EngineMiniNetSendPacket = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->net_send_packet, nullptr);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->NET_SendPacket.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
    }

    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, NetchanCopyFileFragmentsUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-netchan-copy";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");

    const std::filesystem::path fragment = root / "incoming-fragment.bin";
    {
        std::ofstream planted(fragment, std::ios::binary);
        planted << "csretro-fragment-sentinel";
    }
    const std::string fragment_bytes = ReadFileBytes(fragment);
    const std::filesystem::file_time_type fragment_time = std::filesystem::last_write_time(fragment);
    const std::vector<std::string> files_before = RegularFileNames(root);

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->netchan_copy, nullptr);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(g_EngineMiniNetchanCopyFileFragments, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->Netchan_CopyFileFragments.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_TimeScale.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info copy_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->netchan_copy), &copy_info), 0);
        ASSERT_NE(copy_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(copy_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(ReadFileBytes(fragment), fragment_bytes);
        EXPECT_EQ(std::filesystem::last_write_time(fragment), fragment_time);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniNetchanCopyFileFragments = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->netchan_copy, nullptr);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->Netchan_CopyFileFragments.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_TimeScale.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(ReadFileBytes(fragment), fragment_bytes);
        EXPECT_EQ(std::filesystem::last_write_time(fragment), fragment_time);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SvcTimeScaleUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-svc-timescale";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;
    sys_timescale = &timescale;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->svc_timescale, nullptr);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(g_EngineMiniSvcTimeScale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->SVC_TimeScale.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info scale_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->svc_timescale), &scale_info), 0);
        ASSERT_NE(scale_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(scale_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniSvcTimeScale = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->svc_timescale, nullptr);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->SVC_TimeScale.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SvcSendCvarValueUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-svc-sendcvarvalue";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->svc_sendcvarvalue, nullptr);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(g_EngineMiniSvcSendCvarValue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info send_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->svc_sendcvarvalue), &send_info), 0);
        ASSERT_NE(send_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(send_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniSvcSendCvarValue = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->svc_sendcvarvalue, nullptr);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, HttpUpdateUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-http-update";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->http_update, nullptr);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(g_EngineMiniHttpUpdate, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info update_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->http_update), &update_info), 0);
        ASSERT_NE(update_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(update_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniHttpUpdate = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->http_update, nullptr);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, HttpStopUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-http-stop";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->http_stop, nullptr);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(g_EngineMiniHttpStop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info stop_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->http_stop), &stop_info), 0);
        ASSERT_NE(stop_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(stop_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniHttpStop = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->http_stop, nullptr);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, StartResourceDownloadingUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-start-resource";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->start_resource, nullptr);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(g_EngineMiniStartResourceDownloading, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info resource_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->start_resource), &resource_info), 0);
        ASSERT_NE(resource_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(resource_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniStartResourceDownloading = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->start_resource, nullptr);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}


TEST_F(XashStartup, ReadPacketsUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-read-packets";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->read_packets, nullptr);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(g_EngineMiniReadPackets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info packets_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->read_packets), &packets_info), 0);
        ASSERT_NE(packets_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(packets_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniReadPackets = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, nullptr);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}


TEST_F(XashStartup, RequestMissingResourcesUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-request-missing";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->disconnect, nullptr);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(g_EngineMiniDisconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(g_EngineMiniRequestMissingResources, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info request_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->request_missing), &request_info), 0);
        ASSERT_NE(request_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(request_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniRequestMissingResources = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, nullptr);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, DisconnectUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-disconnect";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->disconnect, nullptr);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(g_EngineMiniDisconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info disconnect_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->disconnect), &disconnect_info), 0);
        ASSERT_NE(disconnect_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(disconnect_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        g_EngineMiniDisconnect = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->disconnect, nullptr);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ClearStateUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-clear-state";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->clear_state, nullptr);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(g_EngineMiniClearState, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        EXPECT_EQ(engine->client_state->max_edicts, 0);
        EXPECT_EQ(engine->client_state->servercount, 0);
        EXPECT_STREQ(engine->client_state->levelname, "");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_dedicated);
        EXPECT_EQ(engine->client_static->signon, 0);
        EXPECT_EQ(engine->client_static->challenge, 0);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info clear_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->clear_state), &clear_info), 0);
        ASSERT_NE(clear_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(clear_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(g_EngineMiniClearState, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);

        g_EngineMiniClearState = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->clear_state, nullptr);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, HostMapUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-host-map";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->host_map, nullptr);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(g_EngineMiniHostMap, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info host_map_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->host_map), &host_map_info), 0);
        ASSERT_NE(host_map_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(host_map_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(g_EngineMiniHostMap, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);

        g_EngineMiniHostMap = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->host_map, nullptr);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, HostFilterTimeUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-host-filter";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->host_filter, nullptr);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(g_EngineMiniHostFilterTime, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        Dl_info host_filter_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->host_filter), &host_filter_info), 0);
        ASSERT_NE(host_filter_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(host_filter_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(g_EngineMiniHostFilterTime, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);

        g_EngineMiniHostFilterTime = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->host_filter, nullptr);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModClearAllUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-clear";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
        };

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_clear, nullptr);
        EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
        EXPECT_EQ(g_EngineMiniModClearAll, &Mod_ClearAll);
        EXPECT_EQ(record->mod_clear_calls, 0);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_clear_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_clear), &mod_clear_info), 0);
        ASSERT_NE(mod_clear_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_clear_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
        EXPECT_EQ(g_EngineMiniModClearAll, &Mod_ClearAll);
        EXPECT_EQ(record->mod_clear_calls, 0);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModClearAll = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_clear, nullptr);
        EXPECT_EQ(record->mod_clear_calls, 0);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModFindNameUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-find";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
        };

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_find, nullptr);
        EXPECT_EQ(record->mod_find, &Mod_FindName);
        EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
        EXPECT_EQ(record->mod_find_calls, 0);
        EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
        EXPECT_EQ(record->mod_clear_calls, 0);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_find_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_find), &mod_find_info), 0);
        ASSERT_NE(mod_find_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_find_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_find, &Mod_FindName);
        EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
        EXPECT_EQ(record->mod_find_calls, 0);
        EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
        EXPECT_EQ(record->mod_clear_calls, 0);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModFindName = nullptr;
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_find, nullptr);
        EXPECT_EQ(record->mod_find_calls, 0);
        EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
        EXPECT_EQ(record->mod_clear_calls, 0);
        EXPECT_EQ(record->host_filter, &Host_FilterTime);
        EXPECT_EQ(record->host_filter_calls, 0);
        EXPECT_EQ(record->host_map, &Host_Map_f);
        EXPECT_EQ(record->host_map_calls, 0);
        EXPECT_EQ(record->clear_state, &CL_ClearState);
        EXPECT_EQ(record->clear_state_calls, 0);
        EXPECT_EQ(record->disconnect, &CL_Disconnect);
        EXPECT_EQ(record->disconnect_calls, 0);
        EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
        EXPECT_EQ(record->request_missing_calls, 0);
        EXPECT_EQ(record->read_packets, &CL_ReadPackets);
        EXPECT_EQ(record->read_packets_calls, 0);
        EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
        EXPECT_EQ(record->start_resource_calls, 0);
        EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
        EXPECT_EQ(record->http_stop_calls, 0);
        EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
        EXPECT_EQ(record->http_update_calls, 0);
        EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
        EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
        EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
        EXPECT_EQ(record->svc_timescale_calls, 0);
        EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
        EXPECT_EQ(record->netchan_copy_calls, 0);
        EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
        EXPECT_EQ(record->net_send_calls, 0);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModPrintUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-print";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_print, nullptr);
        EXPECT_EQ(record->mod_print, &Mod_Print);
        EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
        EXPECT_EQ(record->mod_print_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_print_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_print), &mod_print_info), 0);
        ASSERT_NE(mod_print_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_print_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_print, &Mod_Print);
        EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
        EXPECT_EQ(record->mod_print_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModPrint = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_print, nullptr);
        EXPECT_EQ(record->mod_print_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModValidateCrcUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-validate-crc";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_validate, nullptr);
        EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
        EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
        EXPECT_EQ(record->mod_validate_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_validate_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_validate), &mod_validate_info), 0);
        ASSERT_NE(mod_validate_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_validate_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
        EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
        EXPECT_EQ(record->mod_validate_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModValidateCRC = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_validate, nullptr);
        EXPECT_EQ(record->mod_validate_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModNeedCrcUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-need-crc";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_need, nullptr);
        EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
        EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
        EXPECT_EQ(record->mod_need_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_need_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_need), &mod_need_info), 0);
        ASSERT_NE(mod_need_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_need_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
        EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
        EXPECT_EQ(record->mod_need_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModNeedCRC = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_need, nullptr);
        EXPECT_EQ(record->mod_need_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModLoadModelUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-load-model";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_load, nullptr);
        EXPECT_EQ(record->mod_load, &Mod_LoadModel);
        EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
        EXPECT_EQ(record->mod_load_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_load_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_load), &mod_load_info), 0);
        ASSERT_NE(mod_load_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_load_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_load, &Mod_LoadModel);
        EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
        EXPECT_EQ(record->mod_load_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModLoadModel = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_load, nullptr);
        EXPECT_EQ(record->mod_load_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModForNameUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-for-name";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_forname, nullptr);
        EXPECT_EQ(record->mod_forname, &Mod_ForName);
        EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
        EXPECT_EQ(record->mod_forname_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_forname_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_forname), &mod_forname_info), 0);
        ASSERT_NE(mod_forname_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_forname_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_forname, &Mod_ForName);
        EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
        EXPECT_EQ(record->mod_forname_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModForName = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_forname, nullptr);
        EXPECT_EQ(record->mod_forname_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, ModExtradataUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-mod-extra-data";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->mod_extradata, nullptr);
        EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
        EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
        EXPECT_EQ(record->mod_extradata_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        Dl_info mod_extradata_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->mod_extradata), &mod_extradata_info), 0);
        ASSERT_NE(mod_extradata_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(mod_extradata_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
        EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
        EXPECT_EQ(record->mod_extradata_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();

        g_EngineMiniModExtradata = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->mod_extradata, nullptr);
        EXPECT_EQ(record->mod_extradata_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprInitUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-init";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_init, nullptr);
        EXPECT_EQ(record->spr_init, &SPR_Init);
        EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
        EXPECT_EQ(record->spr_init_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(engine->SVC_SendCvarValue.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_sprites();

        Dl_info spr_init_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_init), &spr_init_info), 0);
        ASSERT_NE(spr_init_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_init_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_init, &SPR_Init);
        EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
        EXPECT_EQ(record->spr_init_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_sprites();

        g_EngineMiniSprInit = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_init, nullptr);
        EXPECT_EQ(record->spr_init_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->Mod_ValidateCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_NeedCRC.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Print.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_FindName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ClearAll.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_RequestMissingResources.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ReadPackets.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_StartResourceDownloading.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPStop_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_HTTPUpdate.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprShutdownUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-shutdown";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;

        struct PlantedSpriteList
        {
            SPRITELIST* block;

            PlantedSpriteList() :
                block(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                if (block == nullptr)
                {
                    return;
                }
                block->pSprite = nullptr;
                block->pName = nullptr;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, nullptr);
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_shutdown, nullptr);
        EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
        EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
        EXPECT_EQ(record->spr_shutdown_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_shutdown_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_shutdown), &spr_shutdown_info), 0);
        ASSERT_NE(spr_shutdown_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_shutdown_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
        EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
        EXPECT_EQ(record->spr_shutdown_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprShutdown = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_shutdown, nullptr);
        EXPECT_EQ(record->spr_shutdown_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprShutdownNoModelFreeUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-shutdown-nomodelfree";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_shutdown_nomodelfree, nullptr);
        EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
        EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
        EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_nomodel_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_shutdown_nomodelfree), &spr_nomodel_info), 0);
        ASSERT_NE(spr_nomodel_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_nomodel_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
        EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
        EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprShutdownNoModelFree = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_shutdown_nomodelfree, nullptr);
        EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprLoadUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-load";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, nullptr);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_load, nullptr);
        EXPECT_EQ(record->spr_load, &SPR_Load);
        EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
        EXPECT_EQ(record->spr_load_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_load_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_load), &spr_load_info), 0);
        ASSERT_NE(spr_load_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_load_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_load, &SPR_Load);
        EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
        EXPECT_EQ(record->spr_load_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprLoad = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_load, nullptr);
        EXPECT_EQ(record->spr_load_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprDrawUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-draw";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        goldsrc::msprite_t current_sprite{};
        current_sprite.type = 3;
        current_sprite.numframes = 2;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;
        gpSprite = &current_sprite;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_load, &SPR_Load);
            EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
            EXPECT_EQ(record->spr_load_calls, 0);
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_draw, nullptr);
        EXPECT_EQ(record->spr_draw, &SPR_Draw);
        EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
        EXPECT_EQ(record->spr_draw_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_draw_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_draw), &spr_draw_info), 0);
        ASSERT_NE(spr_draw_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_draw_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw, &SPR_Draw);
        EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
        EXPECT_EQ(record->spr_draw_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprDraw = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw, nullptr);
        EXPECT_EQ(record->spr_draw_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprDrawAdditiveUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-draw-additive";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        goldsrc::msprite_t current_sprite{};
        current_sprite.type = 3;
        current_sprite.numframes = 2;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;
        gpSprite = &current_sprite;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_draw, &SPR_Draw);
            EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
            EXPECT_EQ(record->spr_draw_calls, 0);
            EXPECT_EQ(record->spr_load, &SPR_Load);
            EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
            EXPECT_EQ(record->spr_load_calls, 0);
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_draw_additive, nullptr);
        EXPECT_EQ(record->spr_draw_additive, &SPR_DrawAdditive);
        EXPECT_EQ(g_EngineMiniSprDrawAdditive, &SPR_DrawAdditive);
        EXPECT_EQ(record->spr_draw_additive_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_draw_additive_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_draw_additive), &spr_draw_additive_info), 0);
        ASSERT_NE(spr_draw_additive_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_draw_additive_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw_additive, &SPR_DrawAdditive);
        EXPECT_EQ(g_EngineMiniSprDrawAdditive, &SPR_DrawAdditive);
        EXPECT_EQ(record->spr_draw_additive_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprDrawAdditive = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw_additive, nullptr);
        EXPECT_EQ(record->spr_draw_additive_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprDrawHolesUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-draw-holes";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        goldsrc::msprite_t current_sprite{};
        current_sprite.type = 3;
        current_sprite.numframes = 2;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;
        gpSprite = &current_sprite;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_draw_additive, &SPR_DrawAdditive);
            EXPECT_EQ(g_EngineMiniSprDrawAdditive, &SPR_DrawAdditive);
            EXPECT_EQ(record->spr_draw_additive_calls, 0);
            EXPECT_EQ(record->spr_draw, &SPR_Draw);
            EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
            EXPECT_EQ(record->spr_draw_calls, 0);
            EXPECT_EQ(record->spr_load, &SPR_Load);
            EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
            EXPECT_EQ(record->spr_load_calls, 0);
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_draw_holes, nullptr);
        EXPECT_EQ(record->spr_draw_holes, &SPR_DrawHoles);
        EXPECT_EQ(g_EngineMiniSprDrawHoles, &SPR_DrawHoles);
        EXPECT_EQ(record->spr_draw_holes_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_draw_holes_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_draw_holes), &spr_draw_holes_info), 0);
        ASSERT_NE(spr_draw_holes_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_draw_holes_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw_holes, &SPR_DrawHoles);
        EXPECT_EQ(g_EngineMiniSprDrawHoles, &SPR_DrawHoles);
        EXPECT_EQ(record->spr_draw_holes_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprDrawHoles = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw_holes, nullptr);
        EXPECT_EQ(record->spr_draw_holes_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprDrawGenericUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-draw-generic";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        goldsrc::msprite_t current_sprite{};
        current_sprite.type = 3;
        current_sprite.numframes = 2;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;
        gpSprite = &current_sprite;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_draw_holes, &SPR_DrawHoles);
            EXPECT_EQ(g_EngineMiniSprDrawHoles, &SPR_DrawHoles);
            EXPECT_EQ(record->spr_draw_holes_calls, 0);
            EXPECT_EQ(record->spr_draw_additive, &SPR_DrawAdditive);
            EXPECT_EQ(g_EngineMiniSprDrawAdditive, &SPR_DrawAdditive);
            EXPECT_EQ(record->spr_draw_additive_calls, 0);
            EXPECT_EQ(record->spr_draw, &SPR_Draw);
            EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
            EXPECT_EQ(record->spr_draw_calls, 0);
            EXPECT_EQ(record->spr_load, &SPR_Load);
            EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
            EXPECT_EQ(record->spr_load_calls, 0);
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_draw_generic, nullptr);
        EXPECT_EQ(record->spr_draw_generic, &SPR_DrawGeneric);
        EXPECT_EQ(g_EngineMiniSprDrawGeneric, &SPR_DrawGeneric);
        EXPECT_EQ(record->spr_draw_generic_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_draw_generic_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_draw_generic), &spr_draw_generic_info), 0);
        ASSERT_NE(spr_draw_generic_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_draw_generic_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw_generic, &SPR_DrawGeneric);
        EXPECT_EQ(g_EngineMiniSprDrawGeneric, &SPR_DrawGeneric);
        EXPECT_EQ(record->spr_draw_generic_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprDrawGeneric = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_draw_generic, nullptr);
        EXPECT_EQ(record->spr_draw_generic_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprEnableScissorUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-enable-scissor";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        goldsrc::msprite_t current_sprite{};
        current_sprite.type = 3;
        current_sprite.numframes = 2;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;
        gpSprite = &current_sprite;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_draw_generic, &SPR_DrawGeneric);
            EXPECT_EQ(g_EngineMiniSprDrawGeneric, &SPR_DrawGeneric);
            EXPECT_EQ(record->spr_draw_generic_calls, 0);
            EXPECT_EQ(record->spr_draw_holes, &SPR_DrawHoles);
            EXPECT_EQ(g_EngineMiniSprDrawHoles, &SPR_DrawHoles);
            EXPECT_EQ(record->spr_draw_holes_calls, 0);
            EXPECT_EQ(record->spr_draw_additive, &SPR_DrawAdditive);
            EXPECT_EQ(g_EngineMiniSprDrawAdditive, &SPR_DrawAdditive);
            EXPECT_EQ(record->spr_draw_additive_calls, 0);
            EXPECT_EQ(record->spr_draw, &SPR_Draw);
            EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
            EXPECT_EQ(record->spr_draw_calls, 0);
            EXPECT_EQ(record->spr_load, &SPR_Load);
            EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
            EXPECT_EQ(record->spr_load_calls, 0);
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_enable_scissor, nullptr);
        EXPECT_EQ(record->spr_enable_scissor, &SPR_EnableScissor);
        EXPECT_EQ(g_EngineMiniSprEnableScissor, &SPR_EnableScissor);
        EXPECT_EQ(record->spr_enable_scissor_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DisableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_enable_scissor_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_enable_scissor), &spr_enable_scissor_info), 0);
        ASSERT_NE(spr_enable_scissor_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_enable_scissor_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        ASSERT_NE(engine->vid, nullptr);
        struct VidGuard
        {
            goldsrc::viddef_t* previous;

            explicit VidGuard(goldsrc::viddef_t* next) :
                previous(p_vid)
            {
                p_vid = next;
            }

            ~VidGuard()
            {
                p_vid = previous;
            }
        } vid_guard(engine->vid);
        p_vid->width = 640;
        p_vid->height = 480;
        EnableScissorTest(11, 22, 33, 44);
        int planted_x = 0;
        int planted_y = 0;
        int planted_width = 0;
        int planted_height = 0;
        bool planted_enabled = false;
        EngineMini_ReadScissorTest(&planted_x, &planted_y, &planted_width, &planted_height, &planted_enabled);
        EXPECT_EQ(planted_x, 11);
        EXPECT_EQ(planted_y, 22);
        EXPECT_EQ(planted_width, 33);
        EXPECT_EQ(planted_height, 44);
        EXPECT_TRUE(planted_enabled);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        int after_x = -1;
        int after_y = -1;
        int after_width = -1;
        int after_height = -1;
        bool after_enabled = false;
        EngineMini_ReadScissorTest(&after_x, &after_y, &after_width, &after_height, &after_enabled);
        EXPECT_EQ(after_x, planted_x);
        EXPECT_EQ(after_y, planted_y);
        EXPECT_EQ(after_width, planted_width);
        EXPECT_EQ(after_height, planted_height);
        EXPECT_EQ(after_enabled, planted_enabled);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_enable_scissor, &SPR_EnableScissor);
        EXPECT_EQ(g_EngineMiniSprEnableScissor, &SPR_EnableScissor);
        EXPECT_EQ(record->spr_enable_scissor_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DisableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprEnableScissor = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        int null_x = -1;
        int null_y = -1;
        int null_width = -1;
        int null_height = -1;
        bool null_enabled = false;
        EngineMini_ReadScissorTest(&null_x, &null_y, &null_width, &null_height, &null_enabled);
        EXPECT_EQ(null_x, planted_x);
        EXPECT_EQ(null_y, planted_y);
        EXPECT_EQ(null_width, planted_width);
        EXPECT_EQ(null_height, planted_height);
        EXPECT_EQ(null_enabled, planted_enabled);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_enable_scissor, nullptr);
        EXPECT_EQ(record->spr_enable_scissor_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DisableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}

TEST_F(XashStartup, SprDisableScissorUsesTheProcessFunction)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-spr-disable-scissor";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");
    const std::vector<std::string> files_before = RegularFileNames(root);

    goldsrc::cvar_t timescale{};
    char timescale_text[] = "3.5";
    timescale.name = "sys_timescale";
    timescale.string = timescale_text;
    timescale.flags = 0;
    timescale.value = 3.5f;
    timescale.next = nullptr;

    goldsrc::cvar_t sensitivity{};
    char sensitivity_text[] = "3.0";
    sensitivity.name = "sensitivity";
    sensitivity.string = sensitivity_text;
    sensitivity.flags = goldsrc::FCVAR_ARCHIVE;
    sensitivity.value = 3.0f;
    sensitivity.next = &timescale;
    goldsrc::cvar_t* cvar_head = &sensitivity;
    sys_timescale = &timescale;
    p_cvar_vars = &cvar_head;

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        const PlantedModelList planted_models;
        struct CrcSlots
        {
            goldsrc::mod_known_info_t first;
            goldsrc::mod_known_info_t second;

            CrcSlots() :
                first(mod_known_info[0]),
                second(mod_known_info[1])
            {
            }

            ~CrcSlots()
            {
                mod_known_info[0] = first;
                mod_known_info[1] = second;
            }
        } crc_slots;
        int brush_cache = 91;
        int sprite_cache = 77;
        mod_numknown = 2;
        mod_known[0] = {};
        std::memcpy(mod_known[0].name, "maps/csretro_keep.bsp", sizeof("maps/csretro_keep.bsp"));
        mod_known[0].type = goldsrc::mod_brush;
        mod_known[0].needload = NL_PRESENT;
        mod_known[0].numframes = 3;
        mod_known[0].cache.data = &brush_cache;
        mod_known[1] = {};
        std::memcpy(mod_known[1].name, "sprites/csretro_keep.spr", sizeof("sprites/csretro_keep.spr"));
        mod_known[1].type = goldsrc::mod_sprite;
        mod_known[1].needload = NL_NEEDS_LOADED;
        mod_known[1].numframes = 4;
        mod_known[1].cache.data = &sprite_cache;
        mod_known_info[0] = {};
        mod_known_info[0].shouldCRC = 1;
        mod_known_info[0].firstCRCDone = 1;
        mod_known_info[0].initialCRC = 0xC5C5C5C5u;
        mod_known_info[1] = {};
        mod_known_info[1].shouldCRC = 0;
        mod_known_info[1].firstCRCDone = 0;
        mod_known_info[1].initialCRC = 0x11111111u;

        goldsrc::msprite_t current_sprite{};
        current_sprite.type = 3;
        current_sprite.numframes = 2;

        struct SpriteListGuard
        {
            SPRITELIST* list;
            int count;
            goldsrc::msprite_t* sprite;
            goldsrc::HSPRITE_t crosshair;
            std::unordered_set<std::string> names;

            SpriteListGuard() :
                list(gSpriteList),
                count(gSpriteCount),
                sprite(gpSprite),
                crosshair(ghCrosshair),
                names(gLoadedSpriteNames)
            {
                gSpriteList = nullptr;
                gSpriteCount = 0;
                gpSprite = nullptr;
                ghCrosshair = 9;
                gLoadedSpriteNames.clear();
                gLoadedSpriteNames.emplace("csretro-keep-sprite");
            }

            ~SpriteListGuard()
            {
                gSpriteList = list;
                gSpriteCount = count;
                gpSprite = sprite;
                ghCrosshair = crosshair;
                gLoadedSpriteNames = std::move(names);
            }
        } sprite_list;
        gpSprite = &current_sprite;

        struct PlantedSpriteList
        {
            SPRITELIST* block;
            char* name;

            PlantedSpriteList() :
                block(nullptr),
                name(nullptr)
            {
            }

            void allocate()
            {
                block = static_cast<SPRITELIST*>(std::calloc(1, sizeof(SPRITELIST)));
                name = static_cast<char*>(std::malloc(sizeof("csretro-keep-name")));
                if (block == nullptr || name == nullptr)
                {
                    std::free(block);
                    std::free(name);
                    block = nullptr;
                    name = nullptr;
                    return;
                }
                std::memcpy(name, "csretro-keep-name", sizeof("csretro-keep-name"));
                block->pSprite = nullptr;
                block->pName = name;
                block->frameCount = 17;
                gSpriteList = block;
                gSpriteCount = 1;
            }

            ~PlantedSpriteList()
            {
                if (block != nullptr && gSpriteList == block)
                {
                    std::free(name);
                    std::free(block);
                    gSpriteList = nullptr;
                    gSpriteCount = 0;
                }
            }
        } planted_sprites;

        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        std::vector<std::string> sockets_before;
        ASSERT_TRUE(ReadOpenSockets(&sockets_before));
        const double oldrealtime_before = oldrealtime;
        double host_time = 9.25;
        double frame_time = 0.05;
        engine->realtime = &host_time;
        engine->host_frametime = &frame_time;
        goldsrc::cvar_t framerate{};
        char framerate_text[] = "0";
        framerate.name = "host_framerate";
        framerate.string = framerate_text;
        framerate.value = 0.0f;
        const ProcessHostClock clock(&host_time, &frame_time, &framerate);

        auto expect_models = [&]() {
            EXPECT_EQ(mod_numknown, 2);
            EXPECT_STREQ(mod_known[0].name, "maps/csretro_keep.bsp");
            EXPECT_EQ(mod_known[0].type, goldsrc::mod_brush);
            EXPECT_EQ(mod_known[0].needload, NL_PRESENT);
            EXPECT_EQ(mod_known[0].numframes, 3);
            EXPECT_EQ(mod_known[0].cache.data, &brush_cache);
            EXPECT_EQ(brush_cache, 91);
            EXPECT_STREQ(mod_known[1].name, "sprites/csretro_keep.spr");
            EXPECT_EQ(mod_known[1].type, goldsrc::mod_sprite);
            EXPECT_EQ(mod_known[1].needload, NL_NEEDS_LOADED);
            EXPECT_EQ(mod_known[1].numframes, 4);
            EXPECT_EQ(mod_known[1].cache.data, &sprite_cache);
            EXPECT_EQ(sprite_cache, 77);
            EXPECT_EQ(mod_known_info[0].shouldCRC, 1);
            EXPECT_EQ(mod_known_info[0].firstCRCDone, 1);
            EXPECT_EQ(mod_known_info[0].initialCRC, 0xC5C5C5C5u);
            EXPECT_EQ(mod_known_info[1].shouldCRC, 0);
            EXPECT_EQ(mod_known_info[1].firstCRCDone, 0);
            EXPECT_EQ(mod_known_info[1].initialCRC, 0x11111111u);
        };

        auto expect_empty_sprites = [&]() {
            EXPECT_EQ(gSpriteList, nullptr);
            EXPECT_EQ(gSpriteCount, 0);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_planted_sprites = [&]() {
            ASSERT_EQ(gSpriteList, planted_sprites.block);
            EXPECT_EQ(planted_sprites.block->frameCount, 17);
            EXPECT_EQ(planted_sprites.block->pSprite, nullptr);
            EXPECT_EQ(planted_sprites.block->pName, planted_sprites.name);
            EXPECT_STREQ(planted_sprites.block->pName, "csretro-keep-name");
            EXPECT_EQ(gSpriteCount, 1);
            EXPECT_EQ(gpSprite, &current_sprite);
            EXPECT_EQ(current_sprite.type, 3);
            EXPECT_EQ(current_sprite.numframes, 2);
            EXPECT_EQ(ghCrosshair, 9);
            EXPECT_EQ(gLoadedSpriteNames.size(), 1u);
            EXPECT_TRUE(gLoadedSpriteNames.contains("csretro-keep-sprite"));
        };

        auto expect_prior = [&](const EngineMiniXashStartup* record) {
            EXPECT_EQ(record->spr_enable_scissor, &SPR_EnableScissor);
            EXPECT_EQ(g_EngineMiniSprEnableScissor, &SPR_EnableScissor);
            EXPECT_EQ(record->spr_enable_scissor_calls, 0);
            EXPECT_EQ(record->spr_draw_generic, &SPR_DrawGeneric);
            EXPECT_EQ(g_EngineMiniSprDrawGeneric, &SPR_DrawGeneric);
            EXPECT_EQ(record->spr_draw_generic_calls, 0);
            EXPECT_EQ(record->spr_draw_holes, &SPR_DrawHoles);
            EXPECT_EQ(g_EngineMiniSprDrawHoles, &SPR_DrawHoles);
            EXPECT_EQ(record->spr_draw_holes_calls, 0);
            EXPECT_EQ(record->spr_draw_additive, &SPR_DrawAdditive);
            EXPECT_EQ(g_EngineMiniSprDrawAdditive, &SPR_DrawAdditive);
            EXPECT_EQ(record->spr_draw_additive_calls, 0);
            EXPECT_EQ(record->spr_draw, &SPR_Draw);
            EXPECT_EQ(g_EngineMiniSprDraw, &SPR_Draw);
            EXPECT_EQ(record->spr_draw_calls, 0);
            EXPECT_EQ(record->spr_load, &SPR_Load);
            EXPECT_EQ(g_EngineMiniSprLoad, &SPR_Load);
            EXPECT_EQ(record->spr_load_calls, 0);
            EXPECT_EQ(record->spr_shutdown_nomodelfree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(g_EngineMiniSprShutdownNoModelFree, &SPR_Shutdown_NoModelFree);
            EXPECT_EQ(record->spr_shutdown_nomodelfree_calls, 0);
            EXPECT_EQ(record->spr_shutdown, &SPR_Shutdown);
            EXPECT_EQ(g_EngineMiniSprShutdown, &SPR_Shutdown);
            EXPECT_EQ(record->spr_shutdown_calls, 0);
            EXPECT_EQ(record->spr_init, &SPR_Init);
            EXPECT_EQ(g_EngineMiniSprInit, &SPR_Init);
            EXPECT_EQ(record->spr_init_calls, 0);
            EXPECT_EQ(record->mod_extradata, &Mod_Extradata);
            EXPECT_EQ(g_EngineMiniModExtradata, &Mod_Extradata);
            EXPECT_EQ(record->mod_extradata_calls, 0);
            EXPECT_EQ(record->mod_forname, &Mod_ForName);
            EXPECT_EQ(g_EngineMiniModForName, &Mod_ForName);
            EXPECT_EQ(record->mod_forname_calls, 0);
            EXPECT_EQ(record->mod_load, &Mod_LoadModel);
            EXPECT_EQ(g_EngineMiniModLoadModel, &Mod_LoadModel);
            EXPECT_EQ(record->mod_load_calls, 0);
            EXPECT_EQ(record->mod_need, &Mod_NeedCRC);
            EXPECT_EQ(g_EngineMiniModNeedCRC, &Mod_NeedCRC);
            EXPECT_EQ(record->mod_need_calls, 0);
            EXPECT_EQ(record->mod_validate, &Mod_ValidateCRC);
            EXPECT_EQ(g_EngineMiniModValidateCRC, &Mod_ValidateCRC);
            EXPECT_EQ(record->mod_validate_calls, 0);
            EXPECT_EQ(record->mod_print, &Mod_Print);
            EXPECT_EQ(g_EngineMiniModPrint, &Mod_Print);
            EXPECT_EQ(record->mod_print_calls, 0);
            EXPECT_EQ(record->mod_find, &Mod_FindName);
            EXPECT_EQ(g_EngineMiniModFindName, &Mod_FindName);
            EXPECT_EQ(record->mod_find_calls, 0);
            EXPECT_EQ(record->mod_clear, &Mod_ClearAll);
            EXPECT_EQ(record->mod_clear_calls, 0);
            EXPECT_EQ(record->host_filter, &Host_FilterTime);
            EXPECT_EQ(record->host_filter_calls, 0);
            EXPECT_EQ(record->host_map, &Host_Map_f);
            EXPECT_EQ(record->host_map_calls, 0);
            EXPECT_EQ(record->clear_state, &CL_ClearState);
            EXPECT_EQ(record->clear_state_calls, 0);
            EXPECT_EQ(record->disconnect, &CL_Disconnect);
            EXPECT_EQ(record->disconnect_calls, 0);
            EXPECT_EQ(record->request_missing, &CL_RequestMissingResources);
            EXPECT_EQ(record->request_missing_calls, 0);
            EXPECT_EQ(record->read_packets, &CL_ReadPackets);
            EXPECT_EQ(record->read_packets_calls, 0);
            EXPECT_EQ(record->start_resource, &CL_StartResourceDownloading);
            EXPECT_EQ(record->start_resource_calls, 0);
            EXPECT_EQ(record->http_stop, &CL_HTTPStop_f);
            EXPECT_EQ(record->http_stop_calls, 0);
            EXPECT_EQ(record->http_update, &CL_HTTPUpdate);
            EXPECT_EQ(record->http_update_calls, 0);
            EXPECT_EQ(record->svc_sendcvarvalue, &CL_Send_CvarValue);
            EXPECT_EQ(record->svc_sendcvarvalue_calls, 0);
            EXPECT_EQ(record->svc_timescale, &CL_Parse_Timescale);
            EXPECT_EQ(record->svc_timescale_calls, 0);
            EXPECT_EQ(record->netchan_copy, &Netchan_CopyFileFragments);
            EXPECT_EQ(record->netchan_copy_calls, 0);
            EXPECT_EQ(record->net_send_packet, &NET_SendPacket);
            EXPECT_EQ(record->net_send_calls, 0);
        };

        std::string printed = CaptureStandardOutput([&]() {
            csr_xash_engine_argv(2, argv);
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        EXPECT_EQ(printed.find("pipe-failed"), std::string::npos);

        const EngineMiniXashStartup* record = EngineMini_XashStartup();
        ASSERT_NE(record->spr_disable_scissor, nullptr);
        EXPECT_EQ(record->spr_disable_scissor, &SPR_DisableScissor);
        EXPECT_EQ(g_EngineMiniSprDisableScissor, &SPR_DisableScissor);
        EXPECT_EQ(record->spr_disable_scissor_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DisableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Set.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_InitGame.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 1);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        ASSERT_NE(engine->client_state, nullptr);
        ASSERT_NE(engine->client_static, nullptr);
        ASSERT_NE(engine->realtime, nullptr);
        ASSERT_NE(engine->host_frametime, nullptr);
        engine->client_state->max_edicts = 42;
        engine->client_state->servercount = 7;
        std::memcpy(engine->client_state->levelname, "csretro-keep", sizeof("csretro-keep"));
        engine->client_static->state = goldsrc::ca_active;
        engine->client_static->signon = 4;
        engine->client_static->challenge = 77;
        EXPECT_EQ(engine->realtime, &host_time);
        EXPECT_EQ(engine->host_frametime, &frame_time);
        EXPECT_DOUBLE_EQ(host_time, 9.25);
        EXPECT_DOUBLE_EQ(frame_time, 0.05);
        const double realtime_planted = host_time;
        const double frametime_planted = frame_time;
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_empty_sprites();

        Dl_info spr_disable_scissor_info{};
        ASSERT_NE(dladdr(reinterpret_cast<void*>(record->spr_disable_scissor), &spr_disable_scissor_info), 0);
        ASSERT_NE(spr_disable_scissor_info.dli_fname, nullptr);
        EXPECT_EQ(std::string(spr_disable_scissor_info.dli_fname).find("hw.so"), std::string::npos);

        std::vector<std::string> sockets_after;
        ASSERT_TRUE(ReadOpenSockets(&sockets_after));
        EXPECT_EQ(sockets_before, sockets_after);
        EXPECT_EQ(RegularFileNames(root), files_before);

        ASSERT_NE(engine->vid, nullptr);
        struct VidGuard
        {
            goldsrc::viddef_t* previous;

            explicit VidGuard(goldsrc::viddef_t* next) :
                previous(p_vid)
            {
                p_vid = next;
            }

            ~VidGuard()
            {
                p_vid = previous;
            }
        } vid_guard(engine->vid);
        p_vid->width = 640;
        p_vid->height = 480;
        EnableScissorTest(11, 22, 33, 44);
        int planted_x = 0;
        int planted_y = 0;
        int planted_width = 0;
        int planted_height = 0;
        bool planted_enabled = false;
        EngineMini_ReadScissorTest(&planted_x, &planted_y, &planted_width, &planted_height, &planted_enabled);
        EXPECT_EQ(planted_x, 11);
        EXPECT_EQ(planted_y, 22);
        EXPECT_EQ(planted_width, 33);
        EXPECT_EQ(planted_height, 44);
        EXPECT_TRUE(planted_enabled);

        planted_sprites.allocate();
        ASSERT_NE(planted_sprites.block, nullptr);
        expect_planted_sprites();

        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        int after_x = -1;
        int after_y = -1;
        int after_width = -1;
        int after_height = -1;
        bool after_enabled = false;
        EngineMini_ReadScissorTest(&after_x, &after_y, &after_width, &after_height, &after_enabled);
        EXPECT_EQ(after_x, planted_x);
        EXPECT_EQ(after_y, planted_y);
        EXPECT_EQ(after_width, planted_width);
        EXPECT_EQ(after_height, planted_height);
        EXPECT_EQ(after_enabled, planted_enabled);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_disable_scissor, &SPR_DisableScissor);
        EXPECT_EQ(g_EngineMiniSprDisableScissor, &SPR_DisableScissor);
        EXPECT_EQ(record->spr_disable_scissor_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DisableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Set.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(record->initializing_calls, 2);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        expect_models();
        expect_planted_sprites();

        g_EngineMiniSprDisableScissor = nullptr;
        printed += CaptureStandardOutput([&]() {
            csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        });
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);

        int null_x = -1;
        int null_y = -1;
        int null_width = -1;
        int null_height = -1;
        bool null_enabled = false;
        EngineMini_ReadScissorTest(&null_x, &null_y, &null_width, &null_height, &null_enabled);
        EXPECT_EQ(null_x, planted_x);
        EXPECT_EQ(null_y, planted_y);
        EXPECT_EQ(null_width, planted_width);
        EXPECT_EQ(null_height, planted_height);
        EXPECT_EQ(null_enabled, planted_enabled);

        record = EngineMini_XashStartup();
        EXPECT_EQ(record->spr_disable_scissor, nullptr);
        EXPECT_EQ(record->spr_disable_scissor_calls, 0);
        expect_prior(record);
        EXPECT_EQ(engine->client_state->max_edicts, 42);
        EXPECT_EQ(engine->client_state->servercount, 7);
        EXPECT_STREQ(engine->client_state->levelname, "csretro-keep");
        EXPECT_EQ(engine->client_static->state, goldsrc::ca_active);
        EXPECT_EQ(engine->client_static->signon, 4);
        EXPECT_EQ(engine->client_static->challenge, 77);
        EXPECT_DOUBLE_EQ(*engine->realtime, realtime_planted);
        EXPECT_DOUBLE_EQ(*engine->host_frametime, frametime_planted);
        EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);
        EXPECT_EQ(engine->SPR_Shutdown.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Shutdown_NoModelFree.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Load.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Draw.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawAdditive.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawHoles.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DrawGeneric.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_EnableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_DisableScissor.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Set.GetHook(), nullptr);
        EXPECT_EQ(engine->SPR_Init.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_Extradata.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_ForName.GetHook(), nullptr);
        EXPECT_EQ(engine->Mod_LoadModel.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_Map_f.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_ClearState.GetHook(), nullptr);
        EXPECT_EQ(engine->CL_Disconnect.GetHook(), nullptr);
        EXPECT_EQ(record->shutdown_calls, 0);
        EXPECT_EQ(record->error_calls, 0);
        EXPECT_EQ(record->init_calls, 0);
        EXPECT_EQ(record->initializing_calls, 3);
        EXPECT_EQ(p_cvar_vars, &cvar_head);
        EXPECT_EQ(cvar_head, &sensitivity);
        EXPECT_EQ(sensitivity.next, &timescale);
        EXPECT_EQ(sensitivity.flags, goldsrc::FCVAR_ARCHIVE);
        EXPECT_FLOAT_EQ(sensitivity.value, 3.0f);
        EXPECT_STREQ(sensitivity.string, "3.0");
        EXPECT_STREQ(sensitivity.name, "sensitivity");
        EXPECT_EQ(sys_timescale, &timescale);
        EXPECT_FLOAT_EQ(timescale.value, 3.5f);
        EXPECT_EQ(timescale.flags, 0);
        EXPECT_STREQ(timescale.string, "3.5");
        EXPECT_EQ(timescale.next, nullptr);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        expect_models();
        expect_planted_sprites();

        std::vector<std::string> sockets_null;
        ASSERT_TRUE(ReadOpenSockets(&sockets_null));
        EXPECT_EQ(sockets_before, sockets_null);
        EXPECT_EQ(RegularFileNames(root), files_before);
        EXPECT_EQ(printed.find("Cached models"), std::string::npos);
        (void)crc_slots;
        (void)sprite_list;
        (void)planted_sprites;
    }

    sys_timescale = nullptr;
    p_cvar_vars = nullptr;
    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}
