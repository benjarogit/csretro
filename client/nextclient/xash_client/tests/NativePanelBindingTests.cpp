#include "NativePanelTree.h"
#include "GameUiProbe.h"
#include "vgui_binding.h"

#include <gtest/gtest.h>
#include <dlfcn.h>
#include <functional>
#include <memory>
#include <stdexcept>

namespace
{
    std::function<void()> panel_hook;

    void InvokePanelHook()
    {
        if (panel_hook)
        {
            panel_hook();
        }
    }

    class NativePanelBinding : public testing::Test
    {
    protected:
        using Module = std::unique_ptr<void, int (*)(void*)>;
        Module client{nullptr, dlclose};
        Module vgui{nullptr, dlclose};
        csr::client::NativePanelTree tree;
        csr::client::NativeEngineVGui engine{tree};
        csr::client::VguiBinding binding;
        int surface{};
        int detach_count{};
        int release_count{};
        bool throw_detach{};

        template <class T>
        T Function(Module& module, const char* name)
        {
            return reinterpret_cast<T>(dlsym(module.get(), name));
        }

        int Count(int kind)
        {
            return Function<int (*)(int)>(vgui, "FixturePanelCount")(kind);
        }

        int NativeShutdowns()
        {
            return Function<int (*)()>(vgui, "FixtureNativePanelShutdownCount")();
        }

        void SetUp() override
        {
            panel_hook = {};
            client.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            vgui.reset(dlopen(CLIENT_NATIVE_PANEL_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_TRUE(client && vgui) << dlerror();
            Function<void (*)()>(client, "FixtureClientBindingReset")();
            Function<void (*)()>(vgui, "FixturePanelReset")();
        }

        void TearDown() override
        {
            panel_hook = {};
            Function<void (*)(void (*)())>(vgui, "FixtureNativePanelAllocHook")(nullptr);
            Function<void (*)(void (*)())>(client, "FixtureViewportParentHook")(nullptr);
            binding.Shutdown();
            tree.Shutdown();
        }

        bool Bind(std::string& error)
        {
            const csr::client::VguiBinding::PanelServices services{
                &tree,
                &engine,
                [](void* owner) {
                    auto& fixture = *static_cast<NativePanelBinding*>(owner);
                    EXPECT_EQ(fixture.NativeShutdowns(), 0);
                    ++fixture.detach_count;
                    if (fixture.throw_detach)
                    {
                        throw std::runtime_error("surface detach failure");
                    }
                },
                [](void* owner) noexcept {
                    auto& fixture = *static_cast<NativePanelBinding*>(owner);
                    EXPECT_FALSE(fixture.tree.IsReady());
                    EXPECT_EQ(fixture.NativeShutdowns(), 1);
                    ++fixture.release_count;
                },
                this
            };
            return binding.BindInterfaces(client.get(), vgui.get(), &surface, error, nullptr, &services);
        }

        bool Start(std::string& error)
        {
            return Bind(error) && binding.InitializeEnginePanels(800, 600, error) && binding.InitializeViewport(error) &&
                   binding.StartViewport(error);
        }
    };
} // namespace

TEST_F(NativePanelBinding, PublishesOnlyReadyContainersAndParentsTheCompleteViewportAfterStart)
{
    std::string error;
    ASSERT_TRUE(Bind(error)) << error;
    auto query = Function<void* (*)(const char*)>(vgui, "FixtureNativePanelQueryEngine");
    EXPECT_EQ(query("VEngineVGui001"), nullptr);
    EXPECT_FALSE(binding.InitializeViewport(error));
    ASSERT_TRUE(binding.InitializeEnginePanels(800, 600, error)) << error;
    EXPECT_EQ(query("VEngineVGui001"), &engine);
    EXPECT_EQ(Count(0), 3);
    EXPECT_NE(tree.GetPanel(0), tree.GetPanel(1));
    EXPECT_NE(tree.GetPanel(1), tree.GetPanel(2));
    ASSERT_TRUE(binding.InitializeViewport(error)) << error;
    EXPECT_EQ(Function<int (*)()>(client, "FixtureViewportParentCount")(), 0);
    panel_hook = [&] {
        EXPECT_EQ(Function<int (*)()>(client, "FixtureViewportStartCount")(), 1);
        EXPECT_EQ(Function<unsigned (*)()>(client, "FixtureViewportParent")(), tree.GetPanel(1));
    };
    Function<void (*)(void (*)())>(client, "FixtureViewportParentHook")(InvokePanelHook);
    ASSERT_TRUE(binding.StartViewport(error)) << error;
    EXPECT_EQ(Function<int (*)()>(client, "FixtureViewportParentCount")(), 1);
    binding.Shutdown();
    EXPECT_EQ(detach_count, 1);
    EXPECT_EQ(release_count, 1);
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(Count(2), 0);
    EXPECT_EQ(query("VEngineVGui001"), nullptr);
}

TEST_F(NativePanelBinding, ReentrantAllocationShutdownKeepsGlobalsAndPanelsUntilNativeBindReturns)
{
    std::string error;
    ASSERT_TRUE(Bind(error)) << error;
    panel_hook = [&] {
        binding.Shutdown();
        EXPECT_TRUE(binding.HasBinding());
        EXPECT_EQ(NativeShutdowns(), 0);
        EXPECT_EQ(Count(1), 0);
    };
    Function<void (*)(void (*)())>(vgui, "FixtureNativePanelAllocHook")(InvokePanelHook);
    EXPECT_FALSE(binding.InitializeEnginePanels(800, 600, error));
    EXPECT_FALSE(binding.HasBinding());
    EXPECT_EQ(Count(1), Count(0));
    EXPECT_EQ(NativeShutdowns(), 1);
    EXPECT_EQ(detach_count, 1);
    EXPECT_EQ(release_count, 1);
}

TEST_F(NativePanelBinding, ReentrantNativeParentingShutdownDefersAllOwnedPanelDeletion)
{
    std::string error;
    ASSERT_TRUE(Bind(error)) << error;
    ASSERT_TRUE(binding.InitializeEnginePanels(800, 600, error)) << error;
    ASSERT_TRUE(binding.InitializeViewport(error)) << error;
    panel_hook = [&] {
        binding.Shutdown();
        EXPECT_TRUE(tree.IsReady());
        EXPECT_EQ(Count(1), 0);
        EXPECT_EQ(NativeShutdowns(), 0);
    };
    Function<void (*)(void (*)())>(client, "FixtureViewportParentHook")(InvokePanelHook);
    EXPECT_FALSE(binding.StartViewport(error));
    EXPECT_FALSE(tree.IsReady());
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(NativeShutdowns(), 1);
}

TEST_F(NativePanelBinding, RuntimeShutdownDefersContainerFreeAndReleasesServicesAfterNativeShutdown)
{
    std::string error;
    ASSERT_TRUE(Start(error)) << error;
    panel_hook = [&] {
        binding.Shutdown();
        EXPECT_TRUE(tree.IsReady());
        EXPECT_EQ(Count(1), 0);
        EXPECT_EQ(NativeShutdowns(), 0);
        EXPECT_EQ(release_count, 0);
    };
    EXPECT_FALSE(binding.RunNativeUi([](void*) { InvokePanelHook(); }, nullptr));
    EXPECT_FALSE(tree.IsReady());
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(detach_count, 1);
    EXPECT_EQ(release_count, 1);
}

TEST_F(NativePanelBinding, MissingNativePanelEntryPointsFailBeforeViewportInitialization)
{
    Module incomplete(dlopen(CLIENT_VGUI_FIXTURE, RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(incomplete, nullptr);
    Function<void (*)(int)>(incomplete, "FixtureVguiConfigure")(0);
    const csr::client::VguiBinding::PanelServices services{&tree, &engine, [](void*) {}, nullptr, nullptr};
    std::string error;
    ASSERT_TRUE(binding.BindInterfaces(client.get(), incomplete.get(), &surface, error, nullptr, &services));
    EXPECT_FALSE(binding.InitializeEnginePanels(800, 600, error));
    EXPECT_FALSE(binding.HasBinding());
    EXPECT_EQ(Function<int (*)()>(client, "FixtureViewportInitCount")(), 0);
    EXPECT_EQ(Count(0), 0);
}

TEST_F(NativePanelBinding, SurfaceDetachFailureStillCleansPanelsAndCallsNativeShutdown)
{
    std::string error;
    ASSERT_TRUE(Start(error)) << error;
    throw_detach = true;
    EXPECT_THROW(binding.Shutdown(), std::runtime_error);
    EXPECT_FALSE(binding.HasBinding());
    EXPECT_FALSE(tree.IsReady());
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(NativeShutdowns(), 1);
    EXPECT_EQ(detach_count, 1);
    EXPECT_EQ(release_count, 1);
}

TEST_F(NativePanelBinding, SeparateLegacyConsumerChecksAllEngineInterfaceMethodsAndOpaqueOutputs)
{
    std::string error;
    ASSERT_TRUE(Start(error)) << error;
    Module consumer(dlopen(CLIENT_LEGACY_GAMEUI_CONSUMER, RTLD_NOW | RTLD_LOCAL), dlclose);
    ASSERT_NE(consumer, nullptr);
    auto consume = Function<void (*)(void*, EngineVguiProbe*)>(consumer, "FixtureConsumeEngineVGui");
    ASSERT_NE(consume, nullptr);
    EngineVguiProbe result{};
    consume(&engine, &result);
    EXPECT_EQ(result.root, tree.GetPanel(0));
    EXPECT_EQ(result.client, tree.GetPanel(1));
    EXPECT_EQ(result.gameui, tree.GetPanel(2));
    EXPECT_EQ(result.unknown, 0u);
    EXPECT_EQ(result.login, 0);
    EXPECT_EQ(result.poll, 0);
    EXPECT_EQ(result.finished, 0);
    EXPECT_EQ(result.progress_guard, 0x1234abcdu);
    EXPECT_EQ(result.error_guard, 0xfeed9876u);
}

TEST_F(NativePanelBinding, FailedCleanupIsSealedBeforeCvGuiInvalidatesItsNativeAllocator)
{
    auto retained = std::make_unique<csr::client::NativePanelTree>();
    csr::client::NativeEngineVGui retained_engine(*retained);
    csr::client::VguiBinding retained_binding;
    const csr::client::VguiBinding::PanelServices services{retained.get(), &retained_engine, [](void*) {}, nullptr, nullptr};
    std::string error;
    ASSERT_TRUE(retained_binding.BindInterfaces(client.get(), vgui.get(), &surface, error, nullptr, &services));
    ASSERT_TRUE(retained_binding.InitializeEnginePanels(800, 600, error)) << error;
    Function<void (*)(int)>(vgui, "FixturePanelThrowParent")(1);
    Function<void (*)(int)>(vgui, "FixturePanelInvalidateOnShutdown")(1);
    EXPECT_THROW(retained_binding.Shutdown(), std::runtime_error);
    EXPECT_FALSE(retained_binding.HasBinding());
    EXPECT_FALSE(retained->IsReady());
    EXPECT_EQ(NativeShutdowns(), 1);
    EXPECT_EQ(Function<int (*)()>(vgui, "FixtureNativePanelProbeInvalidated")(), 1);
    const int parent_calls = Function<int (*)()>(vgui, "FixturePanelParentCalls")();
    const int free_calls = Function<int (*)()>(vgui, "FixturePanelFreeCalls")();
    retained.reset();
    EXPECT_EQ(Function<int (*)()>(vgui, "FixturePanelParentCalls")(), parent_calls);
    EXPECT_EQ(Function<int (*)()>(vgui, "FixturePanelFreeCalls")(), free_calls);
    EXPECT_EQ(Function<int (*)()>(vgui, "FixturePanelPostShutdownCalls")(), 0);
}
