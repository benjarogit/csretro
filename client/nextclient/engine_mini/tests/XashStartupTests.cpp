#include "xash_startup.h"

#include "xash_host/xash_host.h"

#include <xash_engine/xash_engine.h>

#include <goldsrc/limits.h>
#include <goldsrc/server_api.h>

#include <dlfcn.h>

#include <algorithm>
#include <cstdlib>
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
