#include "NativeEngineCommands.h"

#include <gtest/gtest.h>
#include <dlfcn.h>
#include <functional>
#include <filesystem>
#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace
{
    using Callback = void (*)();
    std::function<void()> hook;
    int calls{};
    void Target()
    {
        ++calls;
    }
    void ThrowingTarget()
    {
        throw std::runtime_error("synthetic command target failure");
    }
    void InvokeHook()
    {
        if (hook)
        {
            hook();
        }
    }

    class NativeEngineCommandsTest : public testing::Test
    {
    protected:
        std::unique_ptr<void, int (*)(void*)> module{nullptr, dlclose};
        nitroapi::XashCommandAccess access;
        csr::client::NativeEngineCommands lease;
        goldsrc::cl_enginefunc_t* engine{};
        template <class T>
        T Function(const char* name)
        {
            return reinterpret_cast<T>(dlsym(module.get(), name));
        }
        void SetUp() override
        {
            hook = {};
            calls = 0;
            module.reset(dlopen(CLIENT_NATIVE_COMMAND_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(module, nullptr) << dlerror();
            Function<void (*)()>("FixtureNativeCommandsReset")();
            engine = Function<goldsrc::cl_enginefunc_t* (*)()>("FixtureNativeCommandsEngine")();
            std::string error;
            ASSERT_TRUE(access.Bind(module.get(), error)) << error;
        }
        void TearDown() override
        {
            hook = {};
            Function<void (*)(Callback)>("FixtureNativeCommandsAddHook")(nullptr);
            Mode(0, 0, 0);
            // Tests restore any injected ambiguous record before fixture lifetime ends.
            lease.Shutdown();
            EXPECT_FALSE(lease.HasPendingCommands());
            EXPECT_TRUE(lease.Release());
        }
        void Mode(int add, int remove, int bad)
        {
            Function<void (*)(int, int, int)>("FixtureNativeCommandsMode")(add, remove, bad);
        }
        int Count(int kind)
        {
            return Function<int (*)(int)>("FixtureNativeCommandsCount")(kind);
        }
        bool Bind()
        {
            std::string error;
            return lease.Bind(engine, &access, [](Callback callback, void*) { callback(); }, nullptr, error);
        }
        int Add(const char* name = "test_command", Callback target = Target)
        {
            return lease.Engine()->pfnAddCommand(name, target);
        }
        void Invoke(const char* name)
        {
            Function<void (*)(const char*)>("FixtureNativeCommandsInvoke")(name);
        }
        Callback Handler(const char* name)
        {
            return Function<Callback (*)(const char*)>("FixtureNativeCommandsHandler")(name);
        }
        void Foreign(const char* name, Callback callback, bool reuse = true)
        {
            Function<void (*)(const char*, Callback, int)>("FixtureNativeCommandsForeign")(name, callback, reuse);
        }
    };
} // namespace

TEST_F(NativeEngineCommandsTest, UsesActualLookupAndGuardedForwardingWithoutChangingTheOriginalTable)
{
    ASSERT_TRUE(Bind());
    EXPECT_NE(lease.Engine(), engine);
    EXPECT_NE(lease.Engine()->pfnAddCommand, engine->pfnAddCommand);
    EXPECT_EQ(Add(), 1);
    EXPECT_NE(Handler("test_command"), Target);
    Invoke("TEST_COMMAND");
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(lease.Remove("TEST_COMMAND"));
    EXPECT_EQ(Handler("test_command"), nullptr);
    EXPECT_EQ(Count(1), 1);
    EXPECT_TRUE(lease.Remove("test_command"));
    EXPECT_EQ(Count(1), 1);
}

TEST_F(NativeEngineCommandsTest, NeverRegistersOverOrRemovesForeignCaseVariantCommands)
{
    const auto foreign = Function<Callback>("FixtureForeignNativeCommand");
    Foreign("CONDUMP", foreign);
    ASSERT_TRUE(Bind());
    EXPECT_EQ(Add("condump"), 0);
    EXPECT_FALSE(lease.Remove("CoNdUmP"));
    EXPECT_TRUE(lease.Shutdown());
    EXPECT_EQ(Handler("CONDUMP"), foreign);
    EXPECT_EQ(Count(0), 0);
    EXPECT_EQ(Count(1), 0);
}

TEST_F(NativeEngineCommandsTest, ReplacementAtTheSameRecordAddressDoesNotBelongToTheOldLease)
{
    ASSERT_TRUE(Bind());
    ASSERT_EQ(Add(), 1);
    const auto foreign = Function<Callback>("FixtureForeignNativeCommand");
    Foreign("test_command", foreign, true);
    EXPECT_TRUE(lease.Remove("test_command"));
    EXPECT_EQ(Handler("test_command"), foreign);
    EXPECT_EQ(Count(1), 0);
}

TEST_F(NativeEngineCommandsTest, UnknownNewRecordReferencingTheForwarderBlocksRemovalAndUnload)
{
    ASSERT_TRUE(Bind());
    ASSERT_EQ(Add(), 1);
    const auto forwarder = Handler("test_command");
    Foreign("test_command", forwarder, false);
    EXPECT_FALSE(lease.Remove("test_command"));
    EXPECT_FALSE(lease.Shutdown());
    EXPECT_TRUE(lease.HasPendingCommands());
    EXPECT_FALSE(lease.Release());
    EXPECT_EQ(Count(1), 0);
    Invoke("test_command");
    EXPECT_EQ(calls, 0);
    // Actual engine removal is independent evidence that the callback is gone.
    Function<void (*)(const char*)>("FixtureNativeCommandsErase")("test_command");
    EXPECT_TRUE(lease.Shutdown());
}

TEST_F(NativeEngineCommandsTest, RegistrationFailuresRollbackOnlyActuallyInstalledOwnedCallbacks)
{
    ASSERT_TRUE(Bind());
    Mode(1, 0, 0);
    EXPECT_EQ(Add(), 0);
    EXPECT_FALSE(lease.HasPendingCommands());
    EXPECT_EQ(Count(1), 0);
    Mode(2, 0, 0);
    EXPECT_EQ(Add(), 0);
    EXPECT_EQ(Count(1), 1);
    EXPECT_FALSE(lease.HasPendingCommands());
    Mode(3, 0, 0);
    EXPECT_EQ(Add(), 0);
    EXPECT_EQ(Count(1), 2);
    EXPECT_FALSE(lease.HasPendingCommands());
}

TEST_F(NativeEngineCommandsTest, FailedAndThrowingRemovalRemainPendingUntilActualAbsenceIsVerified)
{
    ASSERT_TRUE(Bind());
    ASSERT_EQ(Add(), 1);
    Mode(0, 1, 0);
    EXPECT_FALSE(lease.Shutdown());
    EXPECT_TRUE(lease.HasPendingCommands());
    Mode(0, 2, 0);
    EXPECT_FALSE(lease.Shutdown());
    EXPECT_TRUE(lease.HasPendingCommands());
    Mode(0, 3, 0);
    EXPECT_FALSE(lease.Shutdown());
    const int attempts = Count(1);
    EXPECT_TRUE(lease.HasPendingCommands());
    EXPECT_TRUE(lease.Shutdown());
    EXPECT_EQ(Count(1), attempts);
}

TEST_F(NativeEngineCommandsTest, ShutdownDuringRegistrationWaitsForTheActualResultAndRejectsNestedRegistration)
{
    ASSERT_TRUE(Bind());
    hook = [&] {
        EXPECT_EQ(Add("nested"), 0);
        EXPECT_FALSE(lease.Shutdown());
        EXPECT_TRUE(lease.HasPendingCommands());
    };
    Function<void (*)(Callback)>("FixtureNativeCommandsAddHook")(InvokeHook);
    EXPECT_EQ(Add(), 0);
    EXPECT_EQ(Count(0), 1);
    EXPECT_EQ(Count(1), 1);
    EXPECT_FALSE(lease.HasPendingCommands());
}

TEST_F(NativeEngineCommandsTest, GuardsCanRemoveTheCurrentCallbackAndReleaseTheirOwnerAfterItReturns)
{
    std::string error;
    struct Context
    {
        csr::client::NativeEngineCommands* lease;
        int invocations{};
    } context{&lease};
    ASSERT_TRUE(lease.Bind(
        engine,
        &access,
        [](Callback target, void* opaque) {
            auto& context = *static_cast<Context*>(opaque);
            ++context.invocations;
            target();
            EXPECT_TRUE(context.lease->Shutdown());
            EXPECT_TRUE(context.lease->Release());
        },
        &context,
        error
    )) << error;
    ASSERT_EQ(Add(), 1);
    Invoke("test_command");
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(context.invocations, 1);
    EXPECT_FALSE(lease.HasPendingCommands());
    EXPECT_EQ(lease.Engine(), nullptr);
}

TEST_F(NativeEngineCommandsTest, ExceptionsCannotCrossTheEngineCommandBoundary)
{
    ASSERT_TRUE(Bind());
    ASSERT_EQ(Add("throws", ThrowingTarget), 1);
    EXPECT_NO_THROW(Invoke("throws"));
    EXPECT_TRUE(lease.Shutdown());
}

TEST_F(NativeEngineCommandsTest, MissingDependenciesAndInvalidNamesCannotCreateRegistrations)
{
    csr::client::NativeEngineCommands invalid;
    std::string error;
    EXPECT_FALSE(invalid.Bind(nullptr, &access, [](Callback, void*) {}, nullptr, error));
    EXPECT_FALSE(invalid.Bind(engine, nullptr, [](Callback, void*) {}, nullptr, error));
    EXPECT_FALSE(invalid.Bind(engine, &access, nullptr, nullptr, error));
    ASSERT_TRUE(Bind());
    EXPECT_EQ(Add(""), 0);
    EXPECT_EQ(Add("two names"), 0);
    EXPECT_EQ(Add("cmd;quit"), 0);
    EXPECT_EQ(Add("valid", nullptr), 0);
    EXPECT_EQ(Count(0), 0);
}

TEST_F(NativeEngineCommandsTest, RemovalCannotConsumeAnUncommittedRegistrationRecord)
{
    ASSERT_TRUE(Bind());
    hook = [&] {
        EXPECT_FALSE(lease.Remove("test_command"));
        EXPECT_FALSE(lease.Release());
        EXPECT_EQ(Count(1), 0);
    };
    Function<void (*)(Callback)>("FixtureNativeCommandsAddHook")(InvokeHook);
    ASSERT_EQ(Add(), 1);
    EXPECT_TRUE(lease.HasPendingCommands());
    Invoke("test_command");
    EXPECT_EQ(calls, 1);
}

TEST_F(NativeEngineCommandsTest, AStoppedLeaseCanBeReleasedAndReboundWithoutPublishingAnOldTable)
{
    ASSERT_TRUE(Bind());
    EXPECT_FALSE(lease.Release());
    ASSERT_EQ(Add(), 1);
    EXPECT_TRUE(lease.Shutdown());
    EXPECT_EQ(Add("after_stop"), 0);
    EXPECT_TRUE(lease.Release());
    EXPECT_EQ(lease.Engine(), nullptr);
    ASSERT_TRUE(Bind());
    EXPECT_EQ(Add("fresh"), 1);
    Invoke("fresh");
    EXPECT_EQ(calls, 1);
}

TEST_F(NativeEngineCommandsTest, UntrackedCallbacksIntoAModuleKeepItsUnloadGateClosed)
{
    ASSERT_TRUE(Bind());
    const auto foreign = Function<Callback>("FixtureForeignNativeCommand");
    ASSERT_EQ(engine->pfnAddCommand("untracked", foreign), 1);
    EXPECT_TRUE(lease.HasModuleCallbacks(module.get()));
    EXPECT_TRUE(lease.Shutdown());
    EXPECT_TRUE(lease.HasModuleCallbacks(module.get()));
    EXPECT_EQ(Count(1), 0);
    EXPECT_TRUE(lease.HasModuleCallbacks(nullptr));
}

TEST_F(NativeEngineCommandsTest, NativeQueriesRejectInvalidRecordsAndMissingFunctionProviders)
{
    const auto foreign = Function<Callback>("FixtureForeignNativeCommand");
    Foreign("MiXeD", foreign);
    nitroapi::XashCommandAccess::View view{};
    ASSERT_TRUE(access.Inspect("mixed", view));
    EXPECT_EQ(view.function, foreign);
    Mode(0, 0, 1);
    EXPECT_FALSE(access.Inspect("MIXED", view));
    EXPECT_EQ(view.identity, nullptr);
    ASSERT_TRUE(Bind());
    EXPECT_EQ(Add("mixed"), 0);
    EXPECT_EQ(Count(0), 0);
    Mode(0, 0, 0);
    nitroapi::XashCommandAccess missing;
    std::unique_ptr<void, int (*)(void*)> incomplete(dlopen(CLIENT_NO_EXPORT_FIXTURE, RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(incomplete, nullptr);
    std::string error;
    EXPECT_FALSE(missing.Bind(incomplete.get(), error));
    EXPECT_FALSE(missing.IsBound());
}

TEST_F(NativeEngineCommandsTest, FileSystemMustBeTheEngineInstalledFactoryRatherThanIdenticalBytes)
{
    using Factory = void* (*)(const char*, int*);
    const auto set = Function<void (*)(Factory)>("FixtureNativeFileSystemFactory");
    ASSERT_NE(set, nullptr);
    set(nullptr);
    EXPECT_FALSE(access.IsFileSystemOwner(module.get()));
    set(Function<Factory>("CreateInterface"));
    EXPECT_TRUE(access.IsFileSystemOwner(module.get()));
    EXPECT_FALSE(access.IsFileSystemOwner(nullptr));
    char path[] = "/tmp/csretro-native-fs-XXXXXX";
    ASSERT_NE(mkdtemp(path), nullptr);
    struct Cleanup
    {
        const char* path;
        ~Cleanup()
        {
            std::filesystem::remove_all(path);
        }
    } cleanup{path};
    const auto copy = std::filesystem::path(path) / "filesystem-copy.so";
    std::filesystem::copy_file(CLIENT_NATIVE_COMMAND_FIXTURE, copy);
    std::unique_ptr<void, int (*)(void*)> duplicate(dlopen(copy.c_str(), RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(duplicate, nullptr) << dlerror();
    EXPECT_FALSE(access.IsFileSystemOwner(duplicate.get()));
    set(nullptr);
}
