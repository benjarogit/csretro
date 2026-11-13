#include "vgui_binding.h"
#include "elf_symbols.h"

#include <gtest/gtest.h>
#include <array>
#include <dlfcn.h>
#include <memory>

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;
    class OriginalVguiBinding : public testing::Test
    {
    protected:
        void SetUp() override
        {
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
    ASSERT_TRUE(binding.Initialize(client.get(), vgui.get(), &surface_token, error)) << error;
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
    ASSERT_TRUE(binding.Initialize(client.get(), vgui.get(), &surface_token, error)) << error;
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
        EXPECT_FALSE(binding.Initialize(client.get(), vgui.get(), &surface_token, error));
        EXPECT_FALSE(error.empty());
        ExpectOriginalGlobals();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), mode == 1 ? 0 : 1);
        EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiShutdownCount"), 0);
    }
    configure(0);
    csr::client::VguiBinding retry;
    std::string error;
    EXPECT_TRUE(retry.Initialize(client.get(), vgui.get(), &surface_token, error)) << error;
}

TEST_F(OriginalVguiBinding, ExclusiveBindingCannotBeReplacedAndCanBeReopened)
{
    csr::client::VguiBinding first, second;
    std::string error;
    ASSERT_TRUE(first.Initialize(client.get(), vgui.get(), &surface_token, error)) << error;
    EXPECT_FALSE(second.Initialize(client.get(), vgui.get(), &surface_token, error));
    second.Shutdown();
    EXPECT_EQ(global(5), &surface_token);
    first.Shutdown();
    ExpectOriginalGlobals();
    ASSERT_TRUE(second.Initialize(client.get(), vgui.get(), &surface_token, error)) << error;
}

TEST_F(OriginalVguiBinding, MissingModulesFactoriesAndSymbolsFailWithoutInitializing)
{
    for (int missing : {0, 1, 2})
    {
        csr::client::VguiBinding binding;
        std::string error;
        EXPECT_FALSE(binding.Initialize(missing == 0 ? nullptr : client.get(),
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
        EXPECT_FALSE(binding.Initialize(missing_factory ? client.get() : empty.get(),
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
        EXPECT_FALSE(binding.Initialize(client.get(), vgui.get(), &surface_token, error));
        EXPECT_FALSE(error.empty());
        ExpectOriginalGlobals();
        EXPECT_EQ(Count(vgui.get(), "FixtureVguiInitCount"), 0);
        EXPECT_EQ(Count(client.get(), "FixtureViewportStartCount"), 0);
    }
    available(1, 1);
}
