#include "vgui_binding.h"
#include "elf_symbols.h"

#include <gtest/gtest.h>
#include <array>
#include <dlfcn.h>
#include <memory>
#include <functional>
#include <stdexcept>

namespace
{
    std::function<void()> native_hook;
    void InvokeNativeHook() { if (native_hook) native_hook(); }
    using Module = std::unique_ptr<void, int (*)(void*)>;
    class OriginalVguiBinding : public testing::Test
    {
    protected:
        void SetUp() override
        {
            native_hook = {};
            client.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            vgui.reset(dlopen(CLIENT_VGUI_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(client, nullptr) << dlerror();
            ASSERT_NE(vgui, nullptr) << dlerror();
            auto reset = reinterpret_cast<void (*)()>(dlsym(client.get(), "FixtureClientBindingReset"));
            ASSERT_NE(reset, nullptr);
            reset();
            configure = reinterpret_cast<void (*)(int)>(dlsym(vgui.get(), "FixtureVguiConfigure"));
            ASSERT_NE(configure, nullptr);
            configure(0);
            global = reinterpret_cast<void* (*)(int)>(dlsym(client.get(), "FixtureClientGlobal"));
            ASSERT_NE(global, nullptr);
            for (int i = 0; i < 9; ++i) before[i] = global(i);
        }
        bool Initialize(csr::client::VguiBinding& binding, void* client_module, void* vgui_module,
                        void* surface, std::string& error)
        {
            return binding.BindInterfaces(client_module, vgui_module, surface, error) &&
                   binding.InitializeViewport(error) && binding.StartViewport(error);
        }
        int Count(void* module, const char* name)
        {
            auto fn = reinterpret_cast<int (*)()>(dlsym(module, name));
            return fn ? fn() : -1;
        }
        void* Pointer(void* module, const char* name)
        {
            auto fn = reinterpret_cast<void* (*)()>(dlsym(module, name));
            return fn ? fn() : nullptr;
        }
        void ExpectOriginalGlobals()
        {
            for (int i = 0; i < 9; ++i) EXPECT_EQ(global(i), before[i]);
        }
        Module client{nullptr, &dlclose};
        Module vgui{nullptr, &dlclose};
        void (*configure)(int){};
        void* (*global)(int){};
        std::array<void*, 9> before{};
        int surface_token{};
    };
}

TEST_F(OriginalVguiBinding, BindsOriginalInterfacesAndStartsViewportBeforeRestoringOnShutdown)
{
    const auto factory = reinterpret_cast<csr::client::VguiBinding::Factory>(dlsym(vgui.get(), "CreateInterface"));
    ASSERT_NE(factory, nullptr);
    csr::client::VguiBinding binding;
    std::string error;
    ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error)) << error;
    const char* names[] = {"VGUI_ivgui006", "VGUI_Input004", "VGUI_Panel007", "VGUI_Scheme009", "VGUI_System009"};
    for (int i = 0; i < 5; ++i) EXPECT_EQ(global(i), factory(names[i], nullptr));
    EXPECT_EQ(global(5), &surface_token);
    EXPECT_EQ(global(6), factory("VGUI_Localize003", nullptr));
    EXPECT_EQ(global(7), Pointer(client.get(), "FixtureClientFilesystem"));
    EXPECT_EQ(global(8), factory("KeyValues003", nullptr));
    EXPECT_EQ(Pointer(vgui.get(), "FixtureSeenSurface"), &surface_token);
    EXPECT_EQ(Pointer(vgui.get(), "FixtureSeenFilesystem"), global(7));
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 1);
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 0);
    binding.Shutdown();
    ExpectOriginalGlobals();
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    binding.Shutdown();
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
}

TEST_F(OriginalVguiBinding, SuppliesAllFourFactoriesReadByTheRegisteredNativeViewport)
{
    csr::client::VguiBinding binding;
    std::string error;
    ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error)) << error;
    EXPECT_EQ(Count(client.get(), "FixtureViewportFactoryCount"), 4);
    // The fixture resolves Surface, filesystem, panels and KeyValues through
    // all four entries. A two-entry array cannot initialize/start the viewport.
    EXPECT_EQ(Count(client.get(), "FixtureViewportInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 1);
}

TEST_F(OriginalVguiBinding, MissingInterfaceAndFailedInitializationNeverBindClientGlobals)
{
    for (int mode : {1, 2})
    {
        configure(mode);
        csr::client::VguiBinding binding;
        std::string error;
        EXPECT_FALSE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
        EXPECT_FALSE(error.empty());
        ExpectOriginalGlobals();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), mode == 1 ? 0 : 1);
        EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 0);
    }
    configure(0);
    csr::client::VguiBinding retry;
    std::string error;
    EXPECT_TRUE(Initialize(retry, client.get(), vgui.get(), &surface_token, error)) << error;
}

TEST_F(OriginalVguiBinding, ExclusiveBindingCannotBeReplacedAndCanBeReopened)
{
    csr::client::VguiBinding first, second;
    std::string error;
    ASSERT_TRUE(Initialize(first, client.get(), vgui.get(), &surface_token, error)) << error;
    EXPECT_FALSE(Initialize(second, client.get(), vgui.get(), &surface_token, error));
    second.Shutdown();
    EXPECT_EQ(global(5), &surface_token);
    first.Shutdown();
    ExpectOriginalGlobals();
    ASSERT_TRUE(Initialize(second, client.get(), vgui.get(), &surface_token, error)) << error;
    EXPECT_TRUE(second.IsInitialized());
    EXPECT_EQ(Count(client.get(), "FixtureViewportInitCount"), 2);
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 2);
}

TEST_F(OriginalVguiBinding, MissingModulesFactoriesAndSymbolsFailWithoutInitializing)
{
    for (int missing : {0, 1, 2})
    {
        csr::client::VguiBinding binding;
        std::string error;
        EXPECT_FALSE(Initialize(binding, missing == 0 ? nullptr : client.get(),
                                        missing == 1 ? nullptr : vgui.get(),
                                        missing == 2 ? nullptr : &surface_token, error));
        ExpectOriginalGlobals();
    }
    Module empty(dlopen(CLIENT_NO_EXPORT_FIXTURE, RTLD_NOW | RTLD_LOCAL), &dlclose);
    ASSERT_NE(empty, nullptr) << dlerror();
    for (bool missing_factory : {false, true})
    {
        csr::client::VguiBinding binding;
        std::string error;
        EXPECT_FALSE(Initialize(binding, missing_factory ? client.get() : empty.get(),
                                        missing_factory ? empty.get() : vgui.get(), &surface_token, error));
        EXPECT_FALSE(error.empty());
        ExpectOriginalGlobals();
    }
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), 0);
    EXPECT_EQ(nitroapi::FindSymbolAddress(nullptr, "missing"), 0u);
    EXPECT_EQ(nitroapi::FindSymbolAddress(client.get(), "unregistered_symbol_name"), 0u);
}

TEST_F(OriginalVguiBinding, UninitializedFilesystemOrViewportFailsBeforeAnyUiStarts)
{
    auto available = reinterpret_cast<void (*)(int, int)>(dlsym(client.get(), "FixtureClientAvailability"));
    ASSERT_NE(available, nullptr);
    for (int missing : {0, 1})
    {
        available(missing == 0 ? 0 : 1, missing == 1 ? 0 : 1);
        csr::client::VguiBinding binding;
        std::string error;
        EXPECT_FALSE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
        EXPECT_FALSE(error.empty());
        ExpectOriginalGlobals();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), 0);
        EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
    }
    available(1, 1);
}

TEST_F(OriginalVguiBinding, BindingAndViewportInitializationDoNotStartTheViewport)
{
    csr::client::VguiBinding binding;
    std::string error;
    ASSERT_TRUE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error)) << error;
    EXPECT_FALSE(binding.IsInitialized());
    EXPECT_EQ(global(5), &surface_token);
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportInitCount"), 0);
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
    // A real GameUI.Initialize can run here with the already-bound globals.
    ASSERT_TRUE(binding.InitializeViewport(error)) << error;
    EXPECT_FALSE(binding.IsInitialized());
    EXPECT_EQ(Count(client.get(), "FixtureViewportInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
    // A real GameUI.Start can run here before the viewport starts.
    ASSERT_TRUE(binding.StartViewport(error)) << error;
    EXPECT_TRUE(binding.IsInitialized());
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 1);
}

TEST_F(OriginalVguiBinding, OutOfOrderAndRepeatedPhasesDoNotCallNativeMethods)
{
    csr::client::VguiBinding binding;
    std::string error;
    EXPECT_FALSE(binding.InitializeViewport(error));
    EXPECT_FALSE(binding.StartViewport(error));
    ASSERT_TRUE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
    EXPECT_FALSE(binding.StartViewport(error));
    EXPECT_FALSE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
    ASSERT_TRUE(binding.InitializeViewport(error));
    EXPECT_FALSE(binding.InitializeViewport(error));
    ASSERT_TRUE(binding.StartViewport(error));
    EXPECT_FALSE(binding.StartViewport(error));
    EXPECT_FALSE(binding.InitializeViewport(error));
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportInitCount"), 1);
    EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 1);
    binding.Shutdown();
    EXPECT_FALSE(binding.StartViewport(error));
}

TEST_F(OriginalVguiBinding, ShutdownAtEitherPartialPhaseRestoresGlobalsAndReleasesBinding)
{
    for (bool initialize_viewport : {false, true})
    {
        auto reset = reinterpret_cast<void (*)()>(dlsym(client.get(), "FixtureClientBindingReset"));
        reset();
        configure(0);
        csr::client::VguiBinding binding, other;
        std::string error;
        ASSERT_TRUE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
        if (initialize_viewport) ASSERT_TRUE(binding.InitializeViewport(error));
        EXPECT_FALSE(other.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
        binding.Shutdown();
        ExpectOriginalGlobals();
        EXPECT_FALSE(binding.IsInitialized());
        EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
        binding.Shutdown();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
        EXPECT_TRUE(other.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
    }
}

TEST_F(OriginalVguiBinding, ChangedViewportCannotBeCalledByLaterStartupPhases)
{
    auto available = reinterpret_cast<void (*)(int, int)>(dlsym(client.get(), "FixtureClientAvailability"));
    ASSERT_NE(available, nullptr);
    for (bool initialize_viewport : {false, true})
    {
        csr::client::VguiBinding binding;
        std::string error;
        ASSERT_TRUE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
        if (initialize_viewport) ASSERT_TRUE(binding.InitializeViewport(error));
        available(1, 0);
        EXPECT_FALSE(initialize_viewport ? binding.StartViewport(error) : binding.InitializeViewport(error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
        binding.Shutdown();
        ExpectOriginalGlobals();
        available(1, 1);
    }
}

TEST_F(OriginalVguiBinding, NativeCallbacksCannotAdvancePhasesAndShutdownWaitsForTheirReturn)
{
    auto vgui_hooks = reinterpret_cast<void (*)(void (*)(), void (*)())>(dlsym(vgui.get(), "FixtureVguiHooks"));
    auto viewport_hooks = reinterpret_cast<void (*)(void (*)(), void (*)())>(dlsym(client.get(), "FixtureViewportHooks"));
    ASSERT_NE(vgui_hooks, nullptr);
    ASSERT_NE(viewport_hooks, nullptr);
    for (int stage : {0, 1, 2})
    {
        auto reset = reinterpret_cast<void (*)()>(dlsym(client.get(), "FixtureClientBindingReset"));
        reset();
        configure(0);
        csr::client::VguiBinding binding;
        std::string error;
        native_hook = [&] {
            EXPECT_TRUE(binding.HasBinding());
            EXPECT_FALSE(binding.IsInitialized());
            EXPECT_FALSE(binding.InitializeViewport(error));
            EXPECT_FALSE(binding.StartViewport(error));
            binding.Shutdown();
            EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 0);
        };
        vgui_hooks(stage == 0 ? InvokeNativeHook : nullptr, nullptr);
        viewport_hooks(stage == 1 ? InvokeNativeHook : nullptr, stage == 2 ? InvokeNativeHook : nullptr);
        const bool bound = binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error);
        if (stage == 0) EXPECT_FALSE(bound);
        else
        {
            ASSERT_TRUE(bound);
            const bool initialized = binding.InitializeViewport(error);
            if (stage == 1) EXPECT_FALSE(initialized);
            else { ASSERT_TRUE(initialized); EXPECT_FALSE(binding.StartViewport(error)); }
        }
        native_hook = {};
        EXPECT_FALSE(binding.IsInitialized());
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
        EXPECT_FALSE(binding.HasBinding());
        ExpectOriginalGlobals();
        binding.Shutdown();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    }
}

TEST_F(OriginalVguiBinding, NativeShutdownCannotReenterTeardownOrReleaseExclusiveOwnershipEarly)
{
    auto hooks = reinterpret_cast<void (*)(void (*)(), void (*)())>(dlsym(vgui.get(), "FixtureVguiHooks"));
    ASSERT_NE(hooks, nullptr);
    csr::client::VguiBinding binding, other;
    std::string error;
    ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
    native_hook = [&] {
        EXPECT_TRUE(binding.HasBinding());
        EXPECT_FALSE(binding.IsInitialized());
        binding.Shutdown();
        EXPECT_FALSE(other.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
        EXPECT_EQ(global(5), &surface_token);
    };
    hooks(nullptr, InvokeNativeHook);
    binding.Shutdown();
    native_hook = {};
    hooks(nullptr, nullptr);
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    ExpectOriginalGlobals();
    EXPECT_TRUE(Initialize(other, client.get(), vgui.get(), &surface_token, error));
}

TEST_F(OriginalVguiBinding, ThrowingViewportPhaseRestoresGlobalsAndReleasesOwnership)
{
    auto hooks = reinterpret_cast<void (*)(void (*)(), void (*)())>(dlsym(client.get(), "FixtureViewportHooks"));
    ASSERT_NE(hooks, nullptr);
    for (bool start : {false, true})
    {
        auto reset = reinterpret_cast<void (*)()>(dlsym(client.get(), "FixtureClientBindingReset"));
        reset();
        configure(0);
        csr::client::VguiBinding binding;
        std::string error;
        ASSERT_TRUE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
        if (start) ASSERT_TRUE(binding.InitializeViewport(error));
        native_hook = [] { throw std::runtime_error("synthetic native failure"); };
        hooks(start ? nullptr : InvokeNativeHook, start ? InvokeNativeHook : nullptr);
        EXPECT_THROW(start ? binding.StartViewport(error) : binding.InitializeViewport(error), std::runtime_error);
        native_hook = {};
        hooks(nullptr, nullptr);
        EXPECT_FALSE(binding.IsInitialized());
        ExpectOriginalGlobals();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    }
}

TEST_F(OriginalVguiBinding, ThrowingNativeShutdownRestoresGlobalsAndAllowsAnotherBinding)
{
    auto hooks = reinterpret_cast<void (*)(void (*)(), void (*)())>(dlsym(vgui.get(), "FixtureVguiHooks"));
    ASSERT_NE(hooks, nullptr);
    csr::client::VguiBinding binding, other;
    std::string error;
    ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
    native_hook = [] { throw std::runtime_error("synthetic native shutdown failure"); };
    hooks(nullptr, InvokeNativeHook);
    EXPECT_THROW(binding.Shutdown(), std::runtime_error);
    EXPECT_FALSE(binding.IsInitialized());
    ExpectOriginalGlobals();
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    native_hook = {};
    hooks(nullptr, nullptr);
    EXPECT_TRUE(Initialize(other, client.get(), vgui.get(), &surface_token, error));
}

TEST_F(OriginalVguiBinding, DestructionContainsNativeShutdownExceptions)
{
    auto hooks = reinterpret_cast<void (*)(void (*)(), void (*)())>(dlsym(vgui.get(), "FixtureVguiHooks"));
    ASSERT_NE(hooks, nullptr);
    std::string error;
    {
        csr::client::VguiBinding binding;
        ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
        native_hook = [] { throw std::runtime_error("synthetic destructor shutdown failure"); };
        hooks(nullptr, InvokeNativeHook);
    }
    native_hook = {};
    hooks(nullptr, nullptr);
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    ExpectOriginalGlobals();
    csr::client::VguiBinding other;
    EXPECT_TRUE(Initialize(other, client.get(), vgui.get(), &surface_token, error));
}

TEST_F(OriginalVguiBinding, RuntimeOperationsRequireCompletedStartup)
{
    csr::client::VguiBinding binding;
    int calls{};
    const auto operation = [](void* context) { ++*static_cast<int*>(context); };
    EXPECT_FALSE(binding.RunNativeUi(operation, &calls));
    std::string error;
    ASSERT_TRUE(binding.BindInterfaces(client.get(), vgui.get(), &surface_token, error));
    EXPECT_FALSE(binding.RunNativeUi(operation, &calls));
    ASSERT_TRUE(binding.InitializeViewport(error));
    EXPECT_FALSE(binding.RunNativeUi(operation, &calls));
    ASSERT_TRUE(binding.StartViewport(error));
    EXPECT_FALSE(binding.RunNativeUi(nullptr, &calls));
    EXPECT_TRUE(binding.RunNativeUi(operation, &calls));
    EXPECT_EQ(calls, 1);
}

TEST_F(OriginalVguiBinding, RuntimeShutdownWaitsForTheOutermostNativeOperation)
{
    csr::client::VguiBinding binding;
    std::string error;
    ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
    struct Context
    {
        csr::client::VguiBinding* binding;
        int (*shutdown_count)();
        bool inside{};
    } context{&binding, reinterpret_cast<int (*)()>(dlsym(vgui.get(), "FixtureVguiShutdownCount"))};
    EXPECT_FALSE(binding.RunNativeUi(
        [](void* opaque) {
            Context& context = *static_cast<Context*>(opaque);
            EXPECT_TRUE(context.binding->RunNativeUi(
                [](void* opaque) {
                    Context& context = *static_cast<Context*>(opaque);
                    context.binding->Shutdown();
                    EXPECT_TRUE(context.binding->IsInitialized());
                    EXPECT_TRUE(context.binding->HasBinding());
                },
                &context
            ));
            EXPECT_EQ(context.shutdown_count(), 0);
            int extra_calls{};
            EXPECT_FALSE(context.binding->RunNativeUi([](void* opaque) { ++*static_cast<int*>(opaque); }, &extra_calls));
            EXPECT_EQ(extra_calls, 0);
            context.inside = true;
        },
        &context
    ));
    EXPECT_TRUE(context.inside);
    EXPECT_FALSE(binding.HasBinding());
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    ExpectOriginalGlobals();
}

TEST_F(OriginalVguiBinding, ThrowingRuntimeOperationCompletesRequestedShutdownBeforePropagation)
{
    csr::client::VguiBinding binding;
    std::string error;
    ASSERT_TRUE(Initialize(binding, client.get(), vgui.get(), &surface_token, error));
    EXPECT_THROW(
        binding.RunNativeUi(
            [](void* owner) {
                auto& binding = *static_cast<csr::client::VguiBinding*>(owner);
                binding.Shutdown();
                EXPECT_TRUE(binding.HasBinding());
                throw std::runtime_error("native runtime callback failure");
            },
            &binding
        ),
        std::runtime_error
    );
    EXPECT_FALSE(binding.HasBinding());
    EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 1);
    ExpectOriginalGlobals();
}
