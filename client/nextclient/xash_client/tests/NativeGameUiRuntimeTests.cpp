#include "NativeGameUiRuntime.h"
#include "NativePanelTree.h"
#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <memory>
#include <stdexcept>

namespace runtime_fixture_consumer
{
    using Factory = void* (*)(const char*, int*);
    class BaseUi
    {
    public:
        virtual ~BaseUi() = default;
        virtual void Initialize(Factory*, int) = 0;
        virtual void Start(goldsrc::cl_enginefunc_t*, int) = 0;
        virtual void Shutdown() = 0;
        virtual int Key(int, int, const char*) = 0;
        virtual void Event(void*, void*) = 0;
        virtual void Paint(int, int, int, int) = 0;
        virtual void Hide() = 0;
        virtual void Activate() = 0;
        virtual void HideConsole() = 0;
        virtual void ShowConsole() = 0;
    };
} // namespace runtime_fixture_consumer
namespace
{
    using Runtime = csr::client::NativeGameUiRuntime;
    Runtime* current{};
    csr::client::NativeEngineVGui* current_engine{};
    Runtime::Factory current_vgui{};
    bool missing_surface{}, missing_service{};
    int token;
    std::function<void()> factory_callback;
    void* EngineFactory(const char* name, int*)
    {
        if (factory_callback)
        {
            factory_callback();
        }
        if (std::strcmp(name, "VEngineVGui001") == 0)
        {
            return current_engine;
        }
        if (std::strcmp(name, "BaseUI001") == 0)
        {
            return current->BaseUiInterface();
        }
        if (std::strcmp(name, "VENGINE_GAMEUIFUNCS_VERSION001") == 0)
        {
            return missing_service ? nullptr : &token;
        }
        if (std::strcmp(name, "VGUI_Surface026") == 0)
        {
            return missing_surface ? nullptr : &token;
        }
        if (std::strcmp(name, "VFileSystem009") == 0)
        {
            return &token;
        }
        return nullptr;
    }
    void* ClientFactory(const char* name, int*)
    {
        return std::strcmp(name, "GameClientExports001") == 0 ? &token : nullptr;
    }
    std::function<void()> callback;
    void Invoke()
    {
        if (callback)
        {
            callback();
        }
    }
    const char* LevelName()
    {
        return "de_dust2";
    }
    class NativeGameUiRuntimeTest : public testing::Test
    {
    protected:
        std::unique_ptr<void, int (*)(void*)> module{nullptr, dlclose};
        csr::client::NativePanelTree panels;
        csr::client::NativeEngineVGui engine{panels};
        Runtime runtime;
        Runtime::Host host;
        goldsrc::cl_enginefunc_t functions{};
        int system{}, key_calls{}, event_calls{}, paint_calls{}, resets{};
        void* last_event{};
        void* last_user{};
        int down{}, key{};
        std::string binding;
        std::array<bool, 3> visibility{{true, true, false}};
        std::array<Runtime::Factory, 6> factories{{EngineFactory, EngineFactory, nullptr, nullptr, nullptr, ClientFactory}};
        template <class T>
        T Function(const char* name)
        {
            return reinterpret_cast<T>(dlsym(module.get(), name));
        }
        void SetUp() override
        {
            callback = {};
            factory_callback = {};
            missing_surface = missing_service = false;
            module.reset(dlopen(CLIENT_NATIVE_GAMEUI_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(module, nullptr) << dlerror();
            Function<void (*)()>("FixtureGameUiReset")();
            std::string error;
            ASSERT_TRUE(panels.Bind(module.get(), 800, 600, error)) << error;
            current = &runtime;
            current_engine = &engine;
            current_vgui = Function<Runtime::Factory>("CreateInterface");
            factories[2] = current_vgui;
            functions.pfnGetLevelName = LevelName;
            host.gameui = host.vgui = module.get();
            host.engine = &functions;
            host.system = &system;
            host.panels = &panels;
            host.hooks.context = this;
            host.hooks.key_handler = [](void* value, int down, int key, const char* text) {
                auto& self = *static_cast<NativeGameUiRuntimeTest*>(value);
                ++self.key_calls;
                self.down = down;
                self.key = key;
                self.binding = text ? text : "";
                return 7;
            };
            host.hooks.event_handler = [](void* value, void* event, void* user) {
                auto& self = *static_cast<NativeGameUiRuntimeTest*>(value);
                ++self.event_calls;
                self.last_event = event;
                self.last_user = user;
            };
            host.hooks.paint = [](void* value, int, int, int, int) { ++static_cast<NativeGameUiRuntimeTest*>(value)->paint_calls; };
            host.hooks.reset_input = [](void* value) { ++static_cast<NativeGameUiRuntimeTest*>(value)->resets; };
            host.hooks.set_visibility = [](void* value, int type, bool visible) {
                static_cast<NativeGameUiRuntimeTest*>(value)->visibility.at(type) = visible;
            };
            host.hooks.popup_count = [](void*) { return 0; };
            host.hooks.get_popup = [](void*, int) -> uint32_t { return 0; };
            ASSERT_TRUE(runtime.Bind(host, error)) << error;
        }
        void TearDown() override
        {
            callback = {};
            factory_callback = {};
            Function<void (*)(void (*)())>("FixtureGameUiHook")(nullptr);
            try
            {
                runtime.Shutdown();
                std::string error;
                runtime.RetireControls(error);
            }
            catch (...)
            {}
            runtime.EndNativeLifetime();
            panels.Shutdown();
            current = nullptr;
            current_engine = nullptr;
            current_vgui = nullptr;
        }
        bool Initialize()
        {
            std::string error;
            return runtime.Initialize(factories.data(), factories.size(), error);
        }
        bool Start()
        {
            std::string error;
            return runtime.Start(error);
        }
        bool Console()
        {
            std::string error;
            return runtime.InitializeConsole(error);
        }
        uint32_t Panel(int kind)
        {
            return Function<uint32_t (*)(int)>("FixtureGameUiPanel")(kind);
        }
        int Count(int kind)
        {
            return Function<int (*)(int)>("FixtureGameUiCount")(kind);
        }
    };
} // namespace
TEST_F(NativeGameUiRuntimeTest, PreservesRealFactoriesEngineSystemAndLifecycleOrder)
{
    EXPECT_FALSE(Start());
    EXPECT_FALSE(Console());
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    ASSERT_TRUE(Console());
    EXPECT_TRUE(runtime.IsReady());
    EXPECT_EQ(Function<void* (*)()>("FixtureGameUiEngine")(), &functions);
    EXPECT_EQ(Function<void* (*)()>("FixtureGameUiSystem")(), &system);
    EXPECT_EQ(Count(2), 1);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(Function<int (*)(int)>("FixtureGameUiOrder")(i), i + 1);
    }
    EXPECT_FALSE(Initialize());
    EXPECT_FALSE(Start());
    EXPECT_FALSE(Console());
    runtime.Frame();
    EXPECT_EQ(Count(1), 1);
}
TEST_F(NativeGameUiRuntimeTest, PreflightsEveryRequiredServiceBeforeNativeInitialization)
{
    missing_surface = true;
    EXPECT_FALSE(Initialize());
    EXPECT_EQ(Count(3), 0);
    missing_surface = false;
    missing_service = true;
    EXPECT_FALSE(Initialize());
    EXPECT_EQ(Count(3), 0);
    EXPECT_TRUE(runtime.CanUnload());
}
TEST_F(NativeGameUiRuntimeTest, BaseUiConsumerUsesExactSlotsAndForwardsActualEventAndKeyArguments)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    ASSERT_TRUE(Console());
    auto* base = static_cast<runtime_fixture_consumer::BaseUi*>(runtime.BaseUiInterface());
    ASSERT_NE(base, nullptr);
    int event{}, user{};
    EXPECT_EQ(base->Key(1, 42, "+attack"), 7);
    base->Event(&event, &user);
    base->Paint(1, 2, 800, 600);
    EXPECT_EQ(key_calls, 1);
    EXPECT_EQ(key, 42);
    EXPECT_EQ(down, 1);
    EXPECT_EQ(binding, "+attack");
    EXPECT_EQ(last_event, &event);
    EXPECT_EQ(last_user, &user);
    EXPECT_EQ(paint_calls, 1);
    base->Activate();
    EXPECT_TRUE(visibility[2]);
    EXPECT_FALSE(visibility[1]);
    base->Hide();
    EXPECT_FALSE(visibility[2]);
    EXPECT_TRUE(visibility[1]);
    base->ShowConsole();
    EXPECT_TRUE(visibility[2]);
    base->HideConsole();
    EXPECT_GE(resets, 4);
}
TEST_F(NativeGameUiRuntimeTest, RetiresOwnedControlsIncludingNonAutoDeleteSurvivorsWithoutDeletingForeignChildren)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    ASSERT_TRUE(Console());
    const auto owned = Panel(0);
    const auto survivor = Panel(3);
    const auto foreign = Function<uint32_t (*)(uint32_t, int)>("FixturePanelBorrow")(owned, 0);
    runtime.Shutdown();
    EXPECT_FALSE(runtime.CanUnload());
    std::string error;
    ASSERT_TRUE(runtime.RetireControls(error)) << error;
    auto alive = Function<int (*)(uint32_t)>("FixtureNativePanelAlive");
    EXPECT_FALSE(alive(owned));
    EXPECT_FALSE(alive(survivor));
    EXPECT_TRUE(alive(foreign));
    EXPECT_EQ(Function<uint32_t (*)(uint32_t)>("FixturePanelParent")(foreign), 0u);
    EXPECT_TRUE(runtime.CanUnload());
    EXPECT_EQ(Count(0), 1);
    EXPECT_TRUE(runtime.RetireControls(error));
    EXPECT_EQ(Count(0), 1);
}
TEST_F(NativeGameUiRuntimeTest, RetiresPartiallyInitializedControlsAfterNativeFailure)
{
    Function<void (*)(int)>("FixtureGameUiMode")(3);
    EXPECT_FALSE(Initialize());
    EXPECT_TRUE(runtime.ReleaseRequested());
    runtime.Shutdown();
    std::string error;
    EXPECT_TRUE(runtime.RetireControls(error)) << error;
    EXPECT_TRUE(runtime.CanUnload());
}
TEST_F(NativeGameUiRuntimeTest, DefersShutdownRequestedInsideNativeFrame)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    callback = [this]() { runtime.Shutdown(); };
    Function<void (*)(void (*)())>("FixtureGameUiHook")(&Invoke);
    runtime.Frame();
    EXPECT_TRUE(runtime.ReleaseRequested());
    EXPECT_EQ(Count(0), 0);
    callback = {};
    runtime.Shutdown();
    EXPECT_EQ(Count(0), 1);
}
TEST_F(NativeGameUiRuntimeTest, FailedNativeShutdownPinsModuleEvenIfControlsAreDeleted)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    Function<void (*)(int)>("FixtureGameUiMode")(5);
    EXPECT_THROW(runtime.Shutdown(), std::runtime_error);
    std::string error;
    EXPECT_FALSE(runtime.RetireControls(error));
    EXPECT_FALSE(runtime.CanUnload());
    runtime.Shutdown();
    EXPECT_EQ(Count(0), 1);
}
TEST_F(NativeGameUiRuntimeTest, UncertainControlDeletionNeverPermitsUnloadOrRetriesThatDeletion)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    runtime.Shutdown();
    Function<void (*)(int)>("FixtureNativePanelThrowDelete")(1);
    std::string error;
    EXPECT_FALSE(runtime.RetireControls(error));
    const auto attempts = Function<int (*)()>("FixtureNativePanelDeleteCalls")();
    EXPECT_FALSE(runtime.RetireControls(error));
    EXPECT_EQ(Function<int (*)()>("FixtureNativePanelDeleteCalls")(), attempts);
    EXPECT_FALSE(runtime.CanUnload());
}
TEST_F(NativeGameUiRuntimeTest, TerminalSealStopsAllNativeAndHostCallbacks)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    ASSERT_TRUE(Console());
    auto* base = static_cast<runtime_fixture_consumer::BaseUi*>(runtime.BaseUiInterface());
    runtime.Shutdown();
    std::string error;
    ASSERT_TRUE(runtime.RetireControls(error));
    runtime.EndNativeLifetime();
    const auto operations = Count(3);
    int event{};
    base->Event(&event, nullptr);
    base->Paint(0, 0, 1, 1);
    EXPECT_EQ(base->Key(1, 2, ""), 0);
    base->Activate();
    base->Hide();
    base->ShowConsole();
    base->HideConsole();
    base->Shutdown();
    runtime.Frame();
    runtime.Shutdown();
    EXPECT_EQ(Count(3), operations);
    EXPECT_EQ(event_calls, 0);
    EXPECT_EQ(paint_calls, 0);
    EXPECT_EQ(runtime.BaseUiInterface(), nullptr);
}

TEST_F(NativeGameUiRuntimeTest, NewUnclassifiedControlsDuringShutdownBlockRetirementBeforeDeletion)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    runtime.Shutdown();
    const auto unexpected = Function<uint32_t (*)(uint32_t, const char*, int)>("FixtureNativePanelControl")(Panel(0), "GameUI", 0);
    std::string error;
    EXPECT_FALSE(runtime.RetireControls(error));
    EXPECT_NE(error.find("Unclassified native control"), std::string::npos);
    EXPECT_FALSE(runtime.CanUnload());
    EXPECT_TRUE(Function<int (*)(uint32_t)>("FixtureNativePanelAlive")(unexpected));
    EXPECT_EQ(Function<int (*)()>("FixtureNativePanelDeleteCalls")(), 0);
}

TEST_F(NativeGameUiRuntimeTest, MissingRealSystemOrHostServicesCannotBindAnotherRuntime)
{
    Runtime other;
    auto incomplete = host;
    incomplete.system = nullptr;
    std::string error;
    EXPECT_FALSE(other.Bind(incomplete, error));
    incomplete = host;
    incomplete.hooks.event_handler = nullptr;
    EXPECT_FALSE(other.Bind(incomplete, error));
    EXPECT_EQ(Count(3), 0);
}

TEST_F(NativeGameUiRuntimeTest, SealedRetainedBaseUiRemainsCallableAfterItsOwnerIsDestroyed)
{
    auto owner = std::make_unique<Runtime>();
    current = owner.get();
    std::string error;
    ASSERT_TRUE(owner->Bind(host, error));
    ASSERT_TRUE(owner->Initialize(factories.data(), factories.size(), error));
    ASSERT_TRUE(owner->Start(error));
    auto* base = static_cast<runtime_fixture_consumer::BaseUi*>(owner->BaseUiInterface());
    Function<void (*)(int)>("FixtureGameUiMode")(5);
    EXPECT_THROW(owner->Shutdown(), std::runtime_error);
    EXPECT_FALSE(owner->RetireControls(error));
    owner->EndNativeLifetime();
    testing::internal::CaptureStderr();
    owner.reset();
    testing::internal::GetCapturedStderr();
    const auto operations = Count(3);
    int event{};
    base->Initialize(factories.data(), factories.size());
    base->Start(&functions, 7);
    base->Shutdown();
    EXPECT_EQ(base->Key(1, 2, ""), 0);
    base->Event(&event, nullptr);
    base->Paint(0, 0, 1, 1);
    base->Hide();
    base->Activate();
    base->HideConsole();
    base->ShowConsole();
    EXPECT_EQ(Count(3), operations);
    current = &runtime;
}


TEST_F(NativeGameUiRuntimeTest, PreflightFactoryCannotReenterInitialization)
{
    bool invoked = false;
    bool nested_result = true;
    factory_callback = [&]() {
        if (!invoked)
        {
            invoked = true;
            std::string error;
            nested_result = runtime.Initialize(factories.data(), factories.size(), error);
        }
    };
    ASSERT_TRUE(Initialize());
    EXPECT_TRUE(invoked);
    EXPECT_FALSE(nested_result);
    EXPECT_EQ(Count(3), 1);
    EXPECT_FALSE(Initialize());
}

TEST_F(NativeGameUiRuntimeTest, NativeStartupCannotReenterStartupThroughBaseUi)
{
    ASSERT_TRUE(Initialize());
    bool invoked = false;
    bool nested_result = true;
    callback = [&]() {
        if (!invoked)
        {
            invoked = true;
            std::string error;
            nested_result = runtime.Start(error);
            auto* base = static_cast<runtime_fixture_consumer::BaseUi*>(runtime.BaseUiInterface());
            base->Start(&functions, 7);
        }
    };
    Function<void (*)(void (*)())>("FixtureGameUiHook")(&Invoke);
    ASSERT_TRUE(Start());
    EXPECT_TRUE(invoked);
    EXPECT_FALSE(nested_result);
    EXPECT_EQ(Count(3), 2);
    EXPECT_TRUE(runtime.IsReady());
}

TEST_F(NativeGameUiRuntimeTest, FailedStartupAttemptCannotInvokeNativeStartupAgain)
{
    ASSERT_TRUE(Initialize());
    Function<void (*)(int)>("FixtureGameUiMode")(4);
    EXPECT_FALSE(Start());
    EXPECT_EQ(Count(3), 2);
    Function<void (*)(int)>("FixtureGameUiMode")(0);
    EXPECT_FALSE(Start());
    EXPECT_EQ(Count(3), 2);
}

TEST_F(NativeGameUiRuntimeTest, ReadsLiveNativeVisibilityAndExclusiveInputOnlyInValidPhases)
{
    EXPECT_EQ(runtime.GameFactory(), Function<Runtime::Factory>("CreateInterface"));
    EXPECT_FALSE(runtime.IsActive());
    EXPECT_FALSE(runtime.HasExclusiveInput());
    EXPECT_FALSE(runtime.IsConsoleVisible());
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), 0);
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    auto visibility = Function<void (*)(int, int, int)>("FixtureGameUiVisibility");
    visibility(1, 1, 0);
    EXPECT_TRUE(runtime.IsActive());
    EXPECT_FALSE(runtime.HasExclusiveInput());
    EXPECT_FALSE(runtime.IsConsoleVisible());
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), 2);
    ASSERT_TRUE(Console());
    visibility(0, 1, 1);
    EXPECT_FALSE(runtime.IsActive());
    EXPECT_TRUE(runtime.IsConsoleVisible());
    EXPECT_TRUE(runtime.HasExclusiveInput());
    runtime.Activate();
    EXPECT_TRUE(runtime.IsActive());
    runtime.Hide();
    EXPECT_FALSE(runtime.IsActive());
    EXPECT_FALSE(runtime.IsConsoleVisible());
    runtime.ShowConsole();
    EXPECT_TRUE(runtime.IsConsoleVisible());
}

TEST_F(NativeGameUiRuntimeTest, StateQueryReentrancyDoesNotRecursivelyInvokeNativeCode)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    Function<void (*)(int, int, int)>("FixtureGameUiVisibility")(1, 0, 0);
    bool nested = true;
    callback = [this, &nested]() { nested = runtime.IsActive(); };
    Function<void (*)(void (*)())>("FixtureGameUiHook")(&Invoke);
    EXPECT_TRUE(runtime.IsActive());
    EXPECT_FALSE(nested);
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), 1);
    EXPECT_FALSE(runtime.ReleaseRequested());
}

TEST_F(NativeGameUiRuntimeTest, ActiveQueryFailureRequestsCleanup)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    Function<void (*)(int)>("FixtureGameUiMode")(6);
    EXPECT_FALSE(runtime.IsActive());
    EXPECT_TRUE(runtime.ReleaseRequested());
    EXPECT_NE(runtime.LastError().find("query failed"), std::string::npos);
    EXPECT_FALSE(runtime.HasExclusiveInput());
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), 1);
}

TEST_F(NativeGameUiRuntimeTest, ConsoleVisibilityQueryFailureRequestsCleanup)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    ASSERT_TRUE(Console());
    Function<void (*)(int)>("FixtureGameUiMode")(6);
    EXPECT_FALSE(runtime.IsConsoleVisible());
    EXPECT_TRUE(runtime.ReleaseRequested());
    EXPECT_NE(runtime.LastError().find("query failed"), std::string::npos);
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), 1);
}

TEST_F(NativeGameUiRuntimeTest, ExclusiveInputQueryFailureRequestsCleanup)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    Function<void (*)(int)>("FixtureGameUiMode")(6);
    EXPECT_FALSE(runtime.HasExclusiveInput());
    EXPECT_TRUE(runtime.ReleaseRequested());
    EXPECT_NE(runtime.LastError().find("query failed"), std::string::npos);
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), 1);
}

TEST_F(NativeGameUiRuntimeTest, TerminalSealWithdrawsFactoryAndStateQueriesWithoutNativeCalls)
{
    ASSERT_TRUE(Initialize());
    ASSERT_TRUE(Start());
    ASSERT_TRUE(Console());
    runtime.Shutdown();
    std::string error;
    ASSERT_TRUE(runtime.RetireControls(error));
    runtime.EndNativeLifetime();
    const auto queries = Function<int (*)()>("FixtureGameUiQueryCount")();
    EXPECT_EQ(runtime.GameFactory(), nullptr);
    EXPECT_FALSE(runtime.IsActive());
    EXPECT_FALSE(runtime.IsConsoleVisible());
    EXPECT_FALSE(runtime.HasExclusiveInput());
    EXPECT_EQ(Function<int (*)()>("FixtureGameUiQueryCount")(), queries);
}
