#include "xash_launcher/xash_launcher.h"

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
