#include "xash_launcher/xash_launcher.h"

#include "xash_host/xash_host.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
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
        missing, XASH_HOST_FILE, XASH_SURFACE_FILE, XASH_CLIENT_FILE, XASH_VGUI_FILE, XASH_GAMEUI_FILE, XASH_STEAM_API_FILE);
    EXPECT_FALSE(plan.ready);
    EXPECT_TRUE(plan.command.empty());

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    char missing_surface[64];
    std::snprintf(missing_surface, sizeof(missing_surface), "/tmp/csretro-launcher-no-surface-%d", static_cast<int>(getpid()));
    const csr::LauncherPlan missing_module = csr::PlanXashLauncher(
        temp.root.c_str(), XASH_HOST_FILE, missing_surface, XASH_CLIENT_FILE, XASH_VGUI_FILE, XASH_GAMEUI_FILE, XASH_STEAM_API_FILE);
    EXPECT_FALSE(missing_module.ready);
    EXPECT_TRUE(missing_module.command.empty());
}

TEST(XashLauncher, PlanNamesTheExistingPieces)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        temp.root.c_str(), XASH_HOST_FILE, XASH_SURFACE_FILE, XASH_CLIENT_FILE, XASH_VGUI_FILE, XASH_GAMEUI_FILE, XASH_STEAM_API_FILE);
    ASSERT_TRUE(plan.ready);
    EXPECT_NE(plan.command.find(std::string("'") + XASH_HOST_FILE + "' --surface "), std::string::npos) << plan.command;
    EXPECT_NE(plan.command.find(std::string("--surface '") + XASH_SURFACE_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--client '") + XASH_CLIENT_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--vgui '") + XASH_VGUI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--gameui '") + XASH_GAMEUI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--filesystem-root '") + temp.root + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--steam-api '") + XASH_STEAM_API_FILE + "'"), std::string::npos);
    EXPECT_EQ(plan.command.find("hw.so"), std::string::npos);
    EXPECT_EQ(plan.command.find("steamapps"), std::string::npos);
    EXPECT_EQ(plan.command.find("RunFrame"), std::string::npos);
}

TEST(XashLauncher, OpenLoadsThePlanAndDoesNotCallRunFrame)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());

    csr::LauncherProcess session;
    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(), XASH_HOST_FILE, XASH_SURFACE_FILE, XASH_CLIENT_FILE, XASH_VGUI_FILE, XASH_GAMEUI_FILE, XASH_STEAM_API_FILE,
            &session),
        1);
    EXPECT_TRUE(session.open);
    EXPECT_NE(session.steam, nullptr);
    EXPECT_EQ(csr_host_pair_loaded(), 1);
    EXPECT_EQ(csr_host_gameui_loaded(), 1);
    EXPECT_EQ(csr_host_filesystem_mounted(), 1);
    EXPECT_EQ(csr_host_vgui_connected(), 1);
    EXPECT_NE(csr_host_vgui_dispatch(), nullptr);
    EXPECT_NE(csr_host_gameui(), nullptr);

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    ASSERT_GE(factories.count, 2);
    int code = 1;
    EXPECT_NE(factories.list[0]("VGUI_Surface026", &code), nullptr);
    EXPECT_NE(factories.list[1]("VFileSystem009", &code), nullptr);

    csr::CloseXashLauncher(&session);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(session.steam, nullptr);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_gameui_loaded(), 0);
    EXPECT_EQ(csr_host_filesystem_mounted(), 0);

    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-launcher-missing-%d", static_cast<int>(getpid()));
    EXPECT_EQ(
        csr::OpenXashLauncher(
            missing, XASH_HOST_FILE, XASH_SURFACE_FILE, XASH_CLIENT_FILE, XASH_VGUI_FILE, XASH_GAMEUI_FILE, XASH_STEAM_API_FILE, &session),
        0);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
}
