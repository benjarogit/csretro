#include "client_callbacks.h"
#include "build_registry.h"

#include <gtest/gtest.h>
#include <goldsrc/pmove.h>
#include <goldsrc/usercmd.h>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <memory>

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;

    class ClientCallbacks : public testing::Test
    {
    protected:
        void SetUp() override
        {
            module.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(module, nullptr) << dlerror();
            configure = reinterpret_cast<void (*)(int)>(dlsym(module.get(), "FixtureConfigure"));
            ASSERT_NE(configure, nullptr);
            configure(0);
        }
        int Count(const char* symbol)
        {
            auto count = reinterpret_cast<int (*)()>(dlsym(module.get(), symbol));
            return count ? count() : -1;
        }
        Module module{nullptr, &dlclose};
        void (*configure)(int){};
    };
}

TEST_F(ClientCallbacks, RetainsTheWholeOriginalTableWithoutCallingGameCallbacks)
{
    goldsrc::cldll_func_t original{}, forwarded{};
    auto fill = reinterpret_cast<void (*)(goldsrc::cldll_func_t*)>(dlsym(module.get(), "F"));
    ASSERT_NE(fill, nullptr);
    fill(&original);
    std::string error;
    ASSERT_TRUE(csr::client::ReadCallbacks(module.get(), forwarded, error)) << error;
    EXPECT_EQ(std::memcmp(&original, &forwarded, sizeof(original)), 0);
    EXPECT_EQ(Count("FixtureHudInitCount"), 0);
    EXPECT_EQ(Count("FixtureShutdownCount"), 0);
}

TEST_F(ClientCallbacks, ForwardsMovementInputInitializationAndReturnValues)
{
    goldsrc::cldll_func_t table{};
    std::string error;
    ASSERT_TRUE(csr::client::ReadCallbacks(module.get(), table, error)) << error;
    EXPECT_EQ(table.pInitFunc(nullptr, 7), 7);
    int engine_token{};
    EXPECT_EQ(table.pInitFunc(reinterpret_cast<goldsrc::cl_enginefunc_t*>(&engine_token), 7), 1007);
    EXPECT_EQ(table.pHudRedrawFunc(1.25f, 9), 134);

    auto move = std::make_unique<goldsrc::playermove_t>();
    move->frametime = .25f;
    move->velocity[0] = 10.f;
    table.pClientMove(move.get(), 0);
    EXPECT_FLOAT_EQ(move->velocity[0], 20.f);
    EXPECT_EQ(move->server, 0);
    table.pClientMove(move.get(), 1);
    EXPECT_FLOAT_EQ(move->velocity[0], 40.f);
    EXPECT_EQ(move->server, 1);
    goldsrc::usercmd_t command{};
    command.forwardmove = 5.f;
    table.pCL_CreateMove(.5f, &command, 3);
    EXPECT_FLOAT_EQ(command.forwardmove, 6.5f);

    table.pHudInitFunc();
    table.pShutdown();
    EXPECT_EQ(Count("FixtureHudInitCount"), 1);
    EXPECT_EQ(Count("FixtureShutdownCount"), 1);
}

TEST_F(ClientCallbacks, RejectsIncompleteTablesWithoutExposingPartialCallbacks)
{
    for (int missing : {1, 2, 3})
    {
        configure(missing);
        goldsrc::cldll_func_t table{};
        table.pHudInitFunc = [] {};
        std::string error;
        EXPECT_FALSE(csr::client::ReadCallbacks(module.get(), table, error));
        EXPECT_NE(error.find("missing"), std::string::npos);
        goldsrc::cldll_func_t empty{};
        EXPECT_EQ(std::memcmp(&table, &empty, sizeof(table)), 0);
        EXPECT_EQ(Count("FixtureHudInitCount"), 0);
    }
}

TEST_F(ClientCallbacks, OptionalCallbacksStayNullAndRepeatedReadsDoNotInitialize)
{
    configure(4);
    for (int i = 0; i < 2; ++i)
    {
        goldsrc::cldll_func_t table{};
        std::string error;
        ASSERT_TRUE(csr::client::ReadCallbacks(module.get(), table, error)) << error;
        EXPECT_EQ(table.pVoiceStatus, nullptr);
        EXPECT_EQ(table.pClientFactory, nullptr);
        EXPECT_NE(table.pClientMove, nullptr);
    }
    EXPECT_EQ(Count("FixtureHudInitCount"), 0);
}

TEST(ClientCallbacksFailure, MissingModuleAndExportReturnNoTable)
{
    Module module(dlopen(CLIENT_NO_EXPORT_FIXTURE, RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(module, nullptr) << dlerror();
    for (void* handle : {static_cast<void*>(nullptr), module.get()})
    {
        goldsrc::cldll_func_t table{};
        std::string error;
        EXPECT_FALSE(csr::client::ReadCallbacks(handle, table, error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(table.pInitFunc, nullptr);
    }
}

TEST(ClientCallbacksFailure, ShippingBridgeRejectsMissingOriginalAndAllowsNullDestination)
{
    Module module(dlopen(CLIENT_BRIDGE, RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(module, nullptr) << dlerror();
    auto fill = reinterpret_cast<void (*)(goldsrc::cldll_func_t*)>(dlsym(module.get(), "F"));
    ASSERT_NE(fill, nullptr);
    fill(nullptr);
    goldsrc::cldll_func_t table{};
    table.pHudInitFunc = [] {};
    fill(&table);
    goldsrc::cldll_func_t empty{};
    EXPECT_EQ(std::memcmp(&table, &empty, sizeof(table)), 0);
}

TEST(ClientIdentity, RecognizesOnlyTheRegisteredGameWithoutAnEngineFingerprint)
{
    const std::string known = "2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a";
    EXPECT_TRUE(nitroapi::IsRegisteredClient("linux", known));
    EXPECT_TRUE(nitroapi::IsRegisteredClient("LINUX", "2E799EF31931C0F4372FF31AD30F0AAB529B16461ED3B0928DFBD958183B2E5A"));
    EXPECT_FALSE(nitroapi::IsRegisteredClient("windows", known));
    EXPECT_FALSE(nitroapi::IsRegisteredClient("linux", std::string(64, 'a')));
    EXPECT_FALSE(nitroapi::IsRegisteredClient("linux", ""));
    EXPECT_FALSE(nitroapi::IsRegisteredClient("", known));
    EXPECT_FALSE(nitroapi::IdentifyBuild({"linux", "", known}).has_value());
    EXPECT_FALSE(nitroapi::IsRegisteredClient("linux", nitroapi::HashFileSha256(CLIENT_CALLBACK_FIXTURE)));
}

TEST(ClientIdentity, RequiresTheMatchingOriginalVguiCompanion)
{
    const std::string client = "2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a";
    const std::string vgui = "f0291c95ec420d65a8f174ccb0e144be831d101b9d2650bb29c6da6bd96bcd3f";
    EXPECT_TRUE(nitroapi::IsRegisteredClientVgui(client, vgui));
    EXPECT_FALSE(nitroapi::IsRegisteredClientVgui(client, std::string(64, 'a')));
    EXPECT_FALSE(nitroapi::IsRegisteredClientVgui(std::string(64, 'a'), vgui));
    EXPECT_FALSE(nitroapi::IsRegisteredClientVgui("", vgui));
    EXPECT_FALSE(nitroapi::IsRegisteredClientVgui(client, ""));
}

TEST(ClientCallbacksFailure, ShippingBridgeRejectsUnknownLibraryBeforeRunningItsConstructor)
{
    // The x64 bridge rejects the i386-only production contract immediately.
    // The same fixture on i386 also exercises the file hash before dlopen.
    char directory[] = "/tmp/csretro-client-bridge-XXXXXX";
    ASSERT_NE(mkdtemp(directory), nullptr);
    const std::filesystem::path root(directory);
    struct Cleanup
    {
        std::filesystem::path root;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(root, error); }
    } cleanup{root};
    const auto bridge = root / "csretro_client.so";
    std::filesystem::copy_file(CLIENT_BRIDGE, bridge);
    std::filesystem::copy_file(CLIENT_TRAP_FIXTURE, root / "client.valve.so");
    Module module(dlopen(bridge.c_str(), RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(module, nullptr) << dlerror();
    auto fill = reinterpret_cast<void (*)(goldsrc::cldll_func_t*)>(dlsym(module.get(), "F"));
    ASSERT_NE(fill, nullptr);
    goldsrc::cldll_func_t table{};
    fill(&table);
    EXPECT_EQ(table.pInitFunc, nullptr);
    EXPECT_EQ(table.pClientMove, nullptr);
}

TEST_F(ClientCallbacks, OriginalInitializationPrecedesVguiAndPreservesItsArgumentsAndResult)
{
    goldsrc::cldll_func_t table{};
    std::string error;
    ASSERT_TRUE(csr::client::ReadCallbacks(module.get(), table, error)) << error;
    struct Context
    {
        void* module;
        goldsrc::cl_enginefunc_t* expected_engine;
        int calls{};
    };
    int token{};
    auto* engine = reinterpret_cast<goldsrc::cl_enginefunc_t*>(&token);
    Context context{module.get(), engine};
    const auto bind = [](void* owner, goldsrc::cl_enginefunc_t* engine) -> bool {
        auto& context = *static_cast<Context*>(owner);
        ++context.calls;
        auto count = reinterpret_cast<int (*)()>(dlsym(context.module, "FixtureInitializeCount"));
        auto last = reinterpret_cast<void* (*)()>(dlsym(context.module, "FixtureLastEngine"));
        EXPECT_EQ(count(), 1);
        EXPECT_EQ(last(), context.expected_engine);
        EXPECT_EQ(engine, context.expected_engine);
        return true;
    };
    EXPECT_EQ(csr::client::InitializeWithVgui(table.pInitFunc, engine, 7, bind, &context), 1007);
    EXPECT_EQ(context.calls, 1);
}

TEST_F(ClientCallbacks, FailedInitializationSkipsVguiAndFailedVguiFailsTheEngineHandshake)
{
    goldsrc::cldll_func_t table{};
    std::string error;
    ASSERT_TRUE(csr::client::ReadCallbacks(module.get(), table, error)) << error;
    int calls{};
    auto bind = [](void* owner, goldsrc::cl_enginefunc_t*) {
        ++*static_cast<int*>(owner);
        return false;
    };
    configure(5);
    EXPECT_EQ(csr::client::InitializeWithVgui(table.pInitFunc, nullptr, 7, bind, &calls), 0);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(Count("FixtureInitializeCount"), 1);
    configure(0);
    EXPECT_EQ(csr::client::InitializeWithVgui(table.pInitFunc, nullptr, 7, bind, &calls), 0);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(Count("FixtureInitializeCount"), 1);
    EXPECT_EQ(csr::client::InitializeWithVgui(nullptr, nullptr, 7, bind, &calls), 0);
    EXPECT_EQ(calls, 1);
}
