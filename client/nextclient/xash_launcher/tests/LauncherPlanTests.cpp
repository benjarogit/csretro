#include "xash_launcher/xash_launcher.h"

#include "xash_host/xash_host.h"

#include <gtest/gtest.h>

#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
