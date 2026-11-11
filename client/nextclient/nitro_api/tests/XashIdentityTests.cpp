#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "NitroApi.h"

namespace
{
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

    bool MapsContain(const std::filesystem::path& fragment)
    {
        std::ifstream maps("/proc/self/maps");
        std::string line;

        while (std::getline(maps, line))
        {
            if (line.find(fragment.string()) != std::string::npos)
                return true;
        }

        return false;
    }
}

TEST(XashIdentity, InitializeDoesNotOpenOrHashTheRetailEngine)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-identity";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "cstrike" / "cl_dlls");
    std::filesystem::copy_file(RESOLVER_TEST_MODULE_PATH, root / "hw.so");
    std::filesystem::copy_file(RESOLVER_TEST_MODULE_PATH, root / "cstrike" / "cl_dlls" / "client.so");

    nitroapi::NitroApiInterface* api = nullptr;

    {
        const WorkingDirectory directory(root);
        api = nitroapi::GetNitroApi();
        ASSERT_TRUE(api->Initialize(nullptr, nullptr, nullptr));

        const nitroapi::BuildVersion* build = api->GetBuildVersion();
        ASSERT_NE(build, nullptr);
        EXPECT_EQ(build->id, "xash");
        EXPECT_EQ(build->family, nitroapi::BuildFamily::Xash);
        EXPECT_TRUE(build->engine_sha256.empty());
        EXPECT_TRUE(build->client_sha256.empty());
        EXPECT_FALSE(MapsContain(root));

        nitroapi::EngineData* engine = api->GetEngineData();
        ASSERT_NE(engine, nullptr);
        EXPECT_EQ(engine->Sys_FloatTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Host_FilterTime.GetHook(), nullptr);
        EXPECT_EQ(engine->Sys_FloatTime.InvokeChained(), 0.0);
        EXPECT_EQ(engine->Host_FilterTime(1.0f), 0);
        engine->SZ_Clear.InvokeChained(nullptr);

        // engine_mini::Init only subscribes. The chain must accept that with no target.
        const auto subscription = engine->Host_FilterTime |= [](float delta, auto* next) {
            return next->Invoke(delta);
        };
        EXPECT_NE(subscription, nullptr);
        EXPECT_EQ(engine->Host_FilterTime.InvokeChained(1.0f), 0);
    }

    ASSERT_NE(api, nullptr);
    EXPECT_TRUE(api->UnInitialize());
    std::filesystem::remove_all(root);
}
