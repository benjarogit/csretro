#include "xash_launcher/xash_launcher.h"

#include "xash_host/xash_host.h"

#include <gtest/gtest.h>

#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace
{

struct TempRoot
{
    std::string root;

    TempRoot()
    {
        char name[] = "/tmp/csretro-launcher-XXXXXX";
        if (mkdtemp(name) != nullptr)
        {
            root = name;
        }
    }

    ~TempRoot()
    {
        if (!root.empty())
        {
            rmdir(root.c_str());
        }
    }
};

} // namespace

TEST(XashLauncher, MissingRootIsAFailure)
{
    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-launcher-missing-%d", static_cast<int>(getpid()));

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        missing,
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    EXPECT_FALSE(plan.ready);
    EXPECT_TRUE(plan.command.empty());

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    char missing_surface[64];
    std::snprintf(missing_surface, sizeof(missing_surface), "/tmp/csretro-launcher-no-surface-%d", static_cast<int>(getpid()));
    const csr::LauncherPlan missing_module = csr::PlanXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        missing_surface,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    EXPECT_FALSE(missing_module.ready);
    EXPECT_TRUE(missing_module.command.empty());
}

TEST(XashLauncher, PlanNamesTheExistingPieces)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    ASSERT_TRUE(plan.ready);
    EXPECT_NE(plan.command.find(std::string("'") + XASH_HOST_FILE + "' --surface "), std::string::npos) << plan.command;
    EXPECT_NE(plan.command.find(std::string("--surface '") + XASH_SURFACE_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--client '") + XASH_CLIENT_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--vgui '") + XASH_VGUI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--gameui '") + XASH_GAMEUI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--filesystem-root '") + temp.root + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--steam-api '") + XASH_STEAM_API_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--engine '") + XASH_ENGINE_MINI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--nitro-api '") + XASH_NITRO_API_FILE + "'"), std::string::npos);
    EXPECT_EQ(plan.command.find("hw.so"), std::string::npos);
    EXPECT_EQ(plan.command.find("steamapps"), std::string::npos);
    EXPECT_EQ(plan.command.find("RunFrame"), std::string::npos);
}

TEST(XashLauncher, OpenLoadsThePlanAndDoesNotCallRunFrame)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* menu_file = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_file, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), menu_file), std::strlen(text));
    std::fclose(menu_file);

    csr::LauncherProcess session;
    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(),
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        1);
    EXPECT_TRUE(session.open);
    EXPECT_NE(session.steam, nullptr);
    EXPECT_EQ(csr_host_pair_loaded(), 1);
    EXPECT_EQ(csr_host_gameui_loaded(), 1);
    EXPECT_EQ(csr_host_filesystem_mounted(), 1);
    EXPECT_EQ(csr_host_vgui_connected(), 1);
    EXPECT_EQ(csr_host_scheme_loaded(), 0);
    EXPECT_EQ(csr_host_gameui_initialized(), 1);
    EXPECT_GE(csr_host_gameui_child_count(), 1);
    EXPECT_NE(csr_host_vgui_dispatch(), nullptr);
    EXPECT_NE(csr_host_gameui(), nullptr);
    EXPECT_EQ(csr_host_engine_loaded(), 1);
    EXPECT_NE(csr_host_engine_check_parm("-console"), 0);
    int argv_calls = 0;
    int mode_calls = 0;
    int init_calls = 0;
    int shutdown_calls = 0;
    int error_calls = 0;
    ASSERT_EQ(csr_host_engine_startup_calls(&argv_calls, &mode_calls, &init_calls, &shutdown_calls, &error_calls), 1);
    EXPECT_EQ(argv_calls, 1);
    EXPECT_EQ(mode_calls, 1);
    EXPECT_EQ(init_calls, 1);
    EXPECT_EQ(shutdown_calls, 0);
    EXPECT_EQ(error_calls, 0);
    auto steam_running = reinterpret_cast<bool (*)()>(dlsym(session.steam, "SteamAPI_IsSteamRunning"));
    ASSERT_NE(steam_running, nullptr);
    EXPECT_FALSE(steam_running());

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    ASSERT_GE(factories.count, 2);
    int code = 1;
    EXPECT_NE(factories.list[0]("VGUI_Surface026", &code), nullptr);
    EXPECT_NE(factories.list[1]("VFileSystem009", &code), nullptr);

    csr::CloseXashLauncher(&session);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(session.steam, nullptr);
    EXPECT_EQ(csr_host_engine_loaded(), 0);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_gameui_loaded(), 0);
    EXPECT_EQ(csr_host_filesystem_mounted(), 0);
    EXPECT_EQ(csr_host_gameui_initialized(), 0);
    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());

    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-launcher-missing-%d", static_cast<int>(getpid()));
    EXPECT_EQ(
        csr::OpenXashLauncher(
            missing,
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        0);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
}

TEST(XashLauncher, PresentPaintsTheMenuWithoutRunFrame)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* menu_file = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_file, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), menu_file), std::strlen(text));
    std::fclose(menu_file);

    csr::LauncherProcess session;
    EXPECT_EQ(csr::PresentXashLauncher(&session, 640, 480), 0);

    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(),
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        1);
    EXPECT_EQ(csr_host_gameui_started(), 1);
    EXPECT_STREQ(csr_host_last_client_command(), "");

    ASSERT_EQ(csr::PresentXashLauncher(&session, 640, 480), 1);
    EXPECT_STREQ(csr_host_last_client_command(), "setpause");
    EXPECT_EQ(csr::PresentXashLauncher(&session, 0, 480), 0);
    EXPECT_TRUE(session.open);

    csr::CloseXashLauncher(&session);
    EXPECT_EQ(csr::PresentXashLauncher(&session, 640, 480), 0);
    EXPECT_FALSE(session.open);

    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}

namespace
{

bool MapsContain(const std::string& fragment)
{
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line))
    {
        if (line.find(fragment) != std::string::npos)
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

} // namespace

TEST(XashLauncher, DirectFramePaintsGameUiAndAdvancesHostTime)
{
    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_before = realtime;
    const double frametime_before = frametime;
    const double oldrealtime_before = oldrealtime;

    EXPECT_EQ(csr_host_frame(640, 480, 0.125), 0);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_before);
    EXPECT_DOUBLE_EQ(frametime, frametime_before);
    EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* menu_file = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_file, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), menu_file), std::strlen(text));
    std::fclose(menu_file);

    const std::string planted_hw = temp.root + "/hw.so";
    std::filesystem::copy_file(XASH_HOST_FILE, planted_hw);
    EXPECT_FALSE(MapsContain(planted_hw));

    csr::LauncherProcess session;
    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(),
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        1);

    void* engine = dlopen(XASH_ENGINE_MINI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(engine, nullptr);
    using GuardFn = void (*)(int*, int*, int*);
    auto guard = reinterpret_cast<GuardFn>(dlsym(engine, "EngineMini_ReadFrameGuard"));
    ASSERT_NE(guard, nullptr);
    int filter_calls = -1;
    int stored_before = -1;
    int nitro_hooks = -1;
    guard(&filter_calls, &stored_before, &nitro_hooks);
    EXPECT_EQ(filter_calls, 0);
    EXPECT_GT(stored_before, 0);
    EXPECT_EQ(nitro_hooks, 0);

    void* vgui = dlopen(XASH_VGUI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(vgui, nullptr);
    auto run_frame_count = reinterpret_cast<int (*)()>(dlsym(vgui, "csr_vgui_run_frame_count"));
    ASSERT_NE(run_frame_count, nullptr);
    const int run_frames_before = run_frame_count();

    std::vector<std::string> sockets_before;
    ASSERT_TRUE(ReadOpenSockets(&sockets_before));
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_open = realtime;

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    ASSERT_TRUE(plan.ready);
    std::ifstream cmdline("/proc/self/cmdline", std::ios::binary);
    std::string arguments((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());
    EXPECT_EQ(arguments.find("--surface"), std::string::npos);

    ASSERT_EQ(csr_host_frame(640, 480, 0.125), 1);
    EXPECT_STREQ(csr_host_last_client_command(), "setpause");
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_open + 0.125);
    EXPECT_DOUBLE_EQ(frametime, 0.125);
    EXPECT_DOUBLE_EQ(oldrealtime, realtime_open + 0.125);

    int filter_after = -1;
    int stored_after = -1;
    int nitro_after = -1;
    guard(&filter_after, &stored_after, &nitro_after);
    EXPECT_EQ(filter_after, 0);
    EXPECT_EQ(stored_after, stored_before);
    EXPECT_EQ(nitro_after, 0);
    EXPECT_EQ(run_frame_count(), run_frames_before);

    std::vector<std::string> sockets_after;
    ASSERT_TRUE(ReadOpenSockets(&sockets_after));
    EXPECT_EQ(sockets_before, sockets_after);
    EXPECT_FALSE(MapsContain(planted_hw));
    auto steam_running = reinterpret_cast<bool (*)()>(dlsym(session.steam, "SteamAPI_IsSteamRunning"));
    ASSERT_NE(steam_running, nullptr);
    EXPECT_FALSE(steam_running());

    const double realtime_held = realtime;
    const double frametime_held = frametime;
    const double oldrealtime_held = oldrealtime;
    EXPECT_EQ(csr_host_frame(0, 480, 0.125), 0);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_held);
    EXPECT_DOUBLE_EQ(frametime, frametime_held);
    EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_held);
    EXPECT_EQ(run_frame_count(), run_frames_before);
    guard(&filter_after, &stored_after, &nitro_after);
    EXPECT_EQ(filter_after, 0);
    EXPECT_EQ(stored_after, stored_before);
    EXPECT_EQ(nitro_after, 0);

    csr::CloseXashLauncher(&session);
    dlclose(vgui);
    dlclose(engine);
    std::filesystem::remove(planted_hw);
    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}
