#include "NativeGameUiContext.h"
#include "NativePanelTree.h"
#include <gtest/gtest.h>
#include <array>
#include <cstring>
#include <dlfcn.h>
#include <functional>

namespace
{
    void Print(const char*, ...) {}
    using Context = csr::client::NativeGameUiContext;
    using Factory = csr::client::NativeGameUiRuntime::Factory;
    class NativeGameUiContextTest : public testing::Test
    {
    protected:
        static NativeGameUiContextTest* current;
        void* module{};
        void* command_module{};
        csr::client::NativePanelTree panels;
        csr::client::NativeEngineVGui engine_vgui{panels};
        nitroapi::XashCommandAccess access;
        Context context;
        Context::Host host;
        std::array<Factory, 6> factories{{FactoryCall, FactoryCall, nullptr, nullptr, nullptr, FactoryCall}};
        int token{};
        std::function<void()> callback;
        template <class T>
        T Symbol(const char* name)
        {
            return reinterpret_cast<T>(dlsym(module, name));
        }
        template <class T>
        T CommandSymbol(const char* name)
        {
            return reinterpret_cast<T>(dlsym(command_module, name));
        }
        static void Hook()
        {
            if (current->callback)
            {
                current->callback();
            }
        }
        static void* FactoryCall(const char* name, int*)
        {
            if (!std::strcmp(name, "VEngineVGui001"))
            {
                return &current->engine_vgui;
            }
            if (!std::strcmp(name, "BaseUI001"))
            {
                return current->context.BaseUiInterface();
            }
            return &current->token;
        }
        void SetUp() override
        {
            current = this;
            module = dlopen(CLIENT_NATIVE_GAMEUI_FIXTURE, RTLD_NOW | RTLD_LOCAL);
            ASSERT_NE(module, nullptr) << dlerror();
            command_module = dlopen(CLIENT_NATIVE_COMMAND_FIXTURE, RTLD_NOW | RTLD_LOCAL);
            ASSERT_NE(command_module, nullptr) << dlerror();
            Symbol<void (*)()>("FixtureGameUiReset")();
            Symbol<void (*)()>("FixtureSdlReset")();
            CommandSymbol<void (*)()>("FixtureNativeCommandsReset")();
            std::string error;
            ASSERT_TRUE(panels.Bind(module, 800, 600, error)) << error;
            ASSERT_TRUE(access.Bind(command_module, error)) << error;
            auto* engine = CommandSymbol<goldsrc::cl_enginefunc_t* (*)()>("FixtureNativeCommandsEngine")();
            engine->Con_Printf = Print;
            engine->Con_DPrintf = Print;
            engine->COM_LoadFile = [](const char*, int, int*) -> unsigned char* { return nullptr; };
            engine->COM_FreeFile = [](void*) {};
            engine->CheckParm = [](const char*, char**) { return 0; };
            engine->pfnClientCmd = [](const char*) { return 1; };
            engine->Cmd_Argc = []() { return 0; };
            engine->pfnGetLevelName = []() -> const char* { return "de_dust2"; };
            host.client = host.gameui = host.vgui = module;
            host.filesystem = &token;
            host.engine = engine;
            host.panels = &panels;
            host.commands = &access;
            host.clear_matches = [](void*, bool) {};
            host.base_directory = "fixture";
            host.product_version = 1;
            host.invoke = [](void (*function)(), void*) { function(); };
            host.ui_hooks.key_handler = [](void*, int, int, const char*) { return 0; };
            host.ui_hooks.event_handler = [](void*, void*, void*) {};
            host.ui_hooks.paint = [](void*, int, int, int, int) {};
            host.ui_hooks.reset_input = [](void*) {};
            host.ui_hooks.set_visibility = [](void*, int, bool) {};
            host.ui_hooks.popup_count = [](void*) { return 0; };
            host.ui_hooks.get_popup = [](void*, int) -> uint32_t { return 0; };
            host.input_hooks.needs_keyboard = [](void*) { return true; };
            host.input_hooks.mouse_move = [](void*, int, int) {};
            host.input_hooks.mouse_button = [](void*, int, int) {};
            host.input_hooks.reset_mouse = [](void*) {};
            factories[2] = Symbol<Factory>("CreateInterface");
        }
        void TearDown() override
        {
            callback = {};
            Symbol<void (*)(void (*)())>("FixtureGameUiHook")(nullptr);
            CommandSymbol<void (*)(int, int, int)>("FixtureNativeCommandsMode")(0, 0, 0);
            context.Shutdown();
            EXPECT_TRUE(context.CanUnload()) << context.LastError();
            panels.Shutdown();
            context.EndNativeLifetime();
            access.Shutdown();
            dlclose(command_module);
            dlclose(module);
            current = nullptr;
        }
        bool Bind()
        {
            std::string error;
            const bool result = context.Bind(host, error);
            EXPECT_TRUE(result) << error;
            return result;
        }
        bool Start()
        {
            std::string error;
            if (!context.Initialize(factories.data(), factories.size(), error))
            {
                ADD_FAILURE() << error;
                return false;
            }
            if (!context.Start(error))
            {
                ADD_FAILURE() << error;
                return false;
            }
            if (!context.InitializeConsole(error))
            {
                ADD_FAILURE() << error;
                return false;
            }
            return true;
        }
    };
    NativeGameUiContextTest* NativeGameUiContextTest::current{};
} // namespace
TEST(NativeGameUiContext, EmptyOwnerIsSafeAndCannotStart)
{
    Context context;
    EXPECT_TRUE(context.CanUnload());
    EXPECT_FALSE(context.IsReady());
    std::string error;
    EXPECT_FALSE(context.Start(error));
    Context::Host host;
    EXPECT_FALSE(context.Bind(host, error));
    EXPECT_FALSE(error.empty());
    context.Shutdown();
    context.EndNativeLifetime();
}
TEST_F(NativeGameUiContextTest, ComposesRealShadowSystemInputAndRuntimeLifetime)
{
    ASSERT_TRUE(Bind());
    EXPECT_NE(context.Engine(), host.engine);
    EXPECT_NE(context.BaseUiInterface(), nullptr);
    ASSERT_TRUE(Start());
    EXPECT_TRUE(context.IsReady());
    EXPECT_EQ(Symbol<void* (*)()>("FixtureGameUiEngine")(), context.Engine());
    EXPECT_NE(Symbol<void* (*)()>("FixtureGameUiSystem")(), nullptr);
    context.Activate();
    EXPECT_TRUE(context.IsActive());
    context.ShowConsole();
    EXPECT_TRUE(context.IsConsoleVisible());
    context.Frame(0.016);
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureGameUiCount")(1), 1);
    context.Shutdown();
    EXPECT_TRUE(context.CanUnload()) << context.LastError();
    EXPECT_FALSE(context.IsReady());
    EXPECT_EQ(context.GameFactory(), nullptr);
}
TEST_F(NativeGameUiContextTest, ReentrantShutdownWaitsForActiveStartToReturn)
{
    ASSERT_TRUE(Bind());
    std::string error;
    ASSERT_TRUE(context.Initialize(factories.data(), factories.size(), error)) << error;
    callback = [&] {
        context.Shutdown();
        EXPECT_FALSE(context.CanUnload());
    };
    Symbol<void (*)(void (*)())>("FixtureGameUiHook")(Hook);
    EXPECT_FALSE(context.Start(error));
    EXPECT_TRUE(context.ReleaseRequested());
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureGameUiCount")(0), 0);
    callback = {};
    context.Shutdown();
    EXPECT_TRUE(context.CanUnload()) << context.LastError();
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureGameUiCount")(0), 1);
}
TEST_F(NativeGameUiContextTest, PendingCommandRemovalRetainsOwnersUntilRetry)
{
    ASSERT_TRUE(Bind());
    ASSERT_TRUE(Start());
    ASSERT_EQ(context.Engine()->pfnAddCommand("context_owned", Hook), 1);
    CommandSymbol<void (*)(int, int, int)>("FixtureNativeCommandsMode")(0, 1, 0);
    context.Shutdown();
    EXPECT_FALSE(context.CanUnload());
    context.EndNativeLifetime();
    EXPECT_NE(context.Engine(), nullptr);
    CommandSymbol<void (*)(int, int, int)>("FixtureNativeCommandsMode")(0, 0, 0);
    context.Shutdown();
    EXPECT_TRUE(context.CanUnload()) << context.LastError();
    context.EndNativeLifetime();
    EXPECT_EQ(context.Engine(), nullptr);
    EXPECT_EQ(context.BaseUiInterface(), nullptr);
}
TEST_F(NativeGameUiContextTest, PartialBindFailureStillRemovesInputWatchAndRetiresLease)
{
    Symbol<void (*)(int)>("FixtureGameUiMode")(1);
    std::string error;
    EXPECT_FALSE(context.Bind(host, error));
    EXPECT_TRUE(context.ReleaseRequested());
    EXPECT_FALSE(error.empty());
    context.Shutdown();
    EXPECT_TRUE(context.CanUnload()) << context.LastError();
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureSdlCounter")(0), 1);
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureSdlCounter")(1), 1);
    context.EndNativeLifetime();
    EXPECT_FALSE(context.Bind(host, error));
}
TEST_F(NativeGameUiContextTest, ActiveFrameDefersReentrantShutdownAndStopsFurtherFrames)
{
    ASSERT_TRUE(Bind());
    ASSERT_TRUE(Start());
    callback = [&] {
        context.Shutdown();
        EXPECT_FALSE(context.CanUnload());
    };
    Symbol<void (*)(void (*)())>("FixtureGameUiHook")(Hook);
    context.Frame(0.016);
    EXPECT_TRUE(context.ReleaseRequested());
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureGameUiCount")(0), 0);
    context.Frame(0.016);
    EXPECT_EQ(Symbol<int (*)(int)>("FixtureGameUiCount")(1), 1);
    callback = {};
    context.Shutdown();
    EXPECT_TRUE(context.CanUnload()) << context.LastError();
}

TEST_F(NativeGameUiContextTest, DirectForeignGameUiCommandPinsModuleWithoutRemovingForeignRegistration)
{
    ASSERT_TRUE(Bind());
    ASSERT_TRUE(Start());
    // This real fixture function belongs to the GameUI image, outside our lease.
    const auto native_callback = Symbol<void (*)()>("FixtureGameUiReset");
    ASSERT_EQ(host.engine->pfnAddCommand("foreign_gui", native_callback), 1);
    context.Shutdown();
    EXPECT_FALSE(context.CanUnload());
    EXPECT_EQ(CommandSymbol<void (*(*)(const char*))()>("FixtureNativeCommandsHandler")("foreign_gui"), native_callback);
    context.EndNativeLifetime();
    EXPECT_NE(context.Engine(), nullptr);
    // Only the external owner removes this foreign registration.
    CommandSymbol<void (*)(const char*)>("FixtureNativeCommandsErase")("foreign_gui");
    EXPECT_TRUE(context.CanUnload()) << context.LastError();
    context.EndNativeLifetime();
    EXPECT_EQ(context.Engine(), nullptr);
}
