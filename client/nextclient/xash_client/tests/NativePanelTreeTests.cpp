#include "NativePanelTree.h"

#include <gtest/gtest.h>
#include <dlfcn.h>
#include <functional>
#include <memory>

namespace
{
    std::function<void()> hook;
    void InvokeHook()
    {
        if (hook)
        {
            hook();
        }
    }
    class NativePanelTreeTest : public testing::Test
    {
    protected:
        std::unique_ptr<void, int (*)(void*)> module{nullptr, dlclose};
        csr::client::NativePanelTree tree;
        template <class T>
        T Function(const char* name)
        {
            return reinterpret_cast<T>(dlsym(module.get(), name));
        }
        void SetUp() override
        {
            hook = {};
            module.reset(dlopen(CLIENT_NATIVE_PANEL_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(module, nullptr) << dlerror();
            auto reset = Function<void (*)()>("FixturePanelReset");
            ASSERT_NE(reset, nullptr);
            reset();
        }
        void TearDown() override
        {
            hook = {};
            Function<void (*)(void (*)())>("FixturePanelHook")(nullptr);
            tree.Shutdown();
        }
        bool Bind()
        {
            std::string error;
            return tree.Bind(module.get(), 800, 600, error);
        }
        int Count(int kind)
        {
            return Function<int (*)(int)>("FixturePanelCount")(kind);
        }
        uint32_t Borrow(uint32_t parent, bool popup = false)
        {
            return Function<uint32_t (*)(uint32_t, int)>("FixturePanelBorrow")(parent, popup);
        }
        uint32_t Parent(uint32_t handle)
        {
            return Function<uint32_t (*)(uint32_t)>("FixturePanelParent")(handle);
        }
        void Flags(uint32_t value, bool visible, bool keyboard, bool mouse)
        {
            Function<void (*)(uint32_t, int, int, int)>("FixturePanelFlags")(value, visible, keyboard, mouse);
        }
    };
} // namespace

TEST_F(NativePanelTreeTest, OwnsThreeRealDistinctHandlesAndKeepsForeignPanelsBorrowed)
{
    ASSERT_TRUE(Bind());
    EXPECT_TRUE(tree.IsReady());
    EXPECT_NE(tree.GetPanel(0), 0u);
    EXPECT_NE(tree.GetPanel(0), tree.GetPanel(1));
    EXPECT_NE(tree.GetPanel(1), tree.GetPanel(2));
    EXPECT_EQ(Parent(tree.GetPanel(1)), tree.GetPanel(0));
    EXPECT_EQ(Parent(tree.GetPanel(2)), tree.GetPanel(0));
    EXPECT_EQ(tree.GetPanel(-1), 0u);
    EXPECT_EQ(tree.GetPanel(3), 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(tree.Root()), tree.GetPanel(0));
    const auto borrowed = Borrow(tree.GetPanel(1));
    tree.Shutdown();
    EXPECT_EQ(Parent(borrowed), 0u);
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(Count(2), 0);
    EXPECT_EQ(Count(3), 1);
    EXPECT_FALSE(tree.IsReady());
    tree.Shutdown();
    EXPECT_EQ(Count(1), 3);
}

TEST_F(NativePanelTreeTest, MatchesAllNativeCallbackSlotsAndReadsTheLiveScheme)
{
    ASSERT_TRUE(Bind());
    auto protocol = Function<int (*)(uint32_t)>("FixturePanelProtocol");
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(protocol(tree.GetPanel(i)), 1);
    }
    Function<void (*)(uint32_t)>("FixturePanelScheme")(91);
    EXPECT_EQ(protocol(tree.GetPanel(0)), 1);
    tree.Resize(1024, 768);
    EXPECT_EQ(protocol(tree.GetPanel(0)), 1);
}

TEST_F(NativePanelTreeTest, PaintsVisibleChildrenOnceAndLeavesPopupsForTheSurfacePass)
{
    ASSERT_TRUE(Bind());
    const auto first = Borrow(tree.GetPanel(1));
    const auto popup = Borrow(tree.GetPanel(1), true);
    const auto hidden = Borrow(tree.GetPanel(2));
    Flags(hidden, false, true, true);
    Function<void (*)(uint32_t)>("FixturePanelPaint")(tree.GetPanel(0));
    auto paints = Function<int (*)(uint32_t)>("FixturePanelPaints");
    EXPECT_EQ(paints(first), 1);
    EXPECT_EQ(paints(popup), 0);
    EXPECT_EQ(paints(hidden), 0);
    EXPECT_FALSE(tree.ReleaseRequested());
}

TEST_F(NativePanelTreeTest, HitTestsInReverseOrderWithClippingVisibilityAndPopupRules)
{
    ASSERT_TRUE(Bind());
    const auto first = Borrow(tree.GetPanel(1));
    const auto last = Borrow(tree.GetPanel(1));
    const auto popup = Borrow(tree.GetPanel(1), true);
    auto hit = Function<uint32_t (*)(uint32_t, int, int, int)>("FixturePanelHit");
    EXPECT_EQ(hit(tree.GetPanel(0), 2, 3, false), last);
    EXPECT_EQ(hit(tree.GetPanel(0), 2, 3, true), popup);
    Flags(last, false, true, true);
    EXPECT_EQ(hit(tree.GetPanel(0), 2, 3, false), first);
    EXPECT_EQ(hit(tree.GetPanel(0), 800, 3, false), 0u);
    EXPECT_EQ(hit(tree.GetPanel(0), -1, 3, false), 0u);
    Flags(first, true, true, false);
    EXPECT_EQ(hit(tree.GetPanel(0), 2, 3, false), 0u);
}

TEST_F(NativePanelTreeTest, RoutesFocusOnlyToVisibleKeyboardEnabledChildren)
{
    ASSERT_TRUE(Bind());
    const auto first = Borrow(tree.GetPanel(1));
    const auto last = Borrow(tree.GetPanel(1));
    const auto disabled = Borrow(tree.GetPanel(2));
    Flags(disabled, true, false, true);
    auto focus = Function<void (*)(uint32_t, int)>("FixturePanelFocus");
    auto current = Function<uint32_t (*)(uint32_t)>("FixturePanelCurrentFocus");
    auto move = Function<int (*)(uint32_t, uint32_t, int)>("FixturePanelMoveFocus");
    focus(tree.GetPanel(0), 1);
    EXPECT_EQ(current(tree.GetPanel(0)), first);
    focus(tree.GetPanel(0), -1);
    EXPECT_EQ(current(tree.GetPanel(0)), last);
    EXPECT_EQ(move(tree.GetPanel(0), first, false), 1);
    EXPECT_EQ(current(tree.GetPanel(0)), last);
    EXPECT_EQ(move(tree.GetPanel(0), last, true), 1);
    EXPECT_EQ(current(tree.GetPanel(0)), first);
}

TEST_F(NativePanelTreeTest, RequestsOwnerCleanupWithoutFreeingDuringMessageDispatch)
{
    ASSERT_TRUE(Bind());
    Function<void (*)(uint32_t, const char*)>("FixturePanelMessage")(tree.GetPanel(0), "Delete");
    EXPECT_TRUE(tree.ReleaseRequested());
    EXPECT_TRUE(tree.IsReady());
    EXPECT_EQ(Count(1), 0);
    tree.Shutdown();
    EXPECT_EQ(Count(1), 3);
}

TEST_F(NativePanelTreeTest, DefersReentrantShutdownDuringResizeAndNativeFree)
{
    ASSERT_TRUE(Bind());
    hook = [this]() { tree.Shutdown(); };
    Function<void (*)(void (*)())>("FixturePanelHook")(&InvokeHook);
    tree.Resize(1024, 768);
    EXPECT_TRUE(tree.ReleaseRequested());
    EXPECT_EQ(Count(1), 0);
    tree.Shutdown();
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(Count(2), 0);
}

TEST_F(NativePanelTreeTest, RollsBackEachPartialAllocationFailure)
{
    for (int failed = 1; failed <= 3; ++failed)
    {
        Function<void (*)()>("FixturePanelReset")();
        Function<void (*)(int)>("FixturePanelFailAllocation")(failed);
        EXPECT_FALSE(Bind());
        EXPECT_FALSE(tree.IsReady());
        EXPECT_EQ(Count(1), failed - 1);
        EXPECT_EQ(Count(2), 0);
        EXPECT_EQ(Count(3), 0);
    }
}

TEST_F(NativePanelTreeTest, RejectsMissingInterfacesBadBoundsAndMissingRealSchemeBeforeAllocation)
{
    for (int missing = 1; missing <= 3; ++missing)
    {
        Function<void (*)(int)>("FixturePanelFailInterface")(missing);
        EXPECT_FALSE(Bind());
        EXPECT_EQ(Count(0), 0);
    }
    Function<void (*)(int)>("FixturePanelFailInterface")(0);
    Function<void (*)(uint32_t)>("FixturePanelScheme")(0);
    EXPECT_FALSE(Bind());
    EXPECT_EQ(Count(0), 0);
    std::string error;
    EXPECT_FALSE(tree.Bind(module.get(), 32768, 600, error));
    EXPECT_FALSE(tree.Bind(nullptr, 800, 600, error));
}

TEST_F(NativePanelTreeTest, RejectsASecondBindAndAllowsFreshBindingAfterShutdown)
{
    ASSERT_TRUE(Bind());
    EXPECT_FALSE(Bind());
    EXPECT_EQ(Count(0), 3);
    tree.Shutdown();
    EXPECT_TRUE(Bind());
}

TEST_F(NativePanelTreeTest, EngineInterfaceUsesNativeHandlesAndNeverAcknowledgesSteamAuthentication)
{
    ASSERT_TRUE(Bind());
    csr::client::NativeEngineVGui engine(tree);
    auto& borrowed = engine;
    EXPECT_EQ(borrowed.GetPanel(2), tree.GetPanel(2));
    EXPECT_FALSE(borrowed.SteamRefreshLogin("unused", true));
    bool finished = true;
    EXPECT_FALSE(borrowed.SteamProcessCall(&finished, nullptr, nullptr));
    EXPECT_FALSE(finished);
}

TEST_F(NativePanelTreeTest, RollsBackReentrantShutdownDuringNativeAllocation)
{
    hook = [this]() { tree.Shutdown(); };
    Function<void (*)(void (*)())>("FixtureNativePanelAllocHook")(&InvokeHook);
    EXPECT_FALSE(Bind());
    EXPECT_FALSE(tree.IsReady());
    EXPECT_EQ(Count(0), 1);
    EXPECT_EQ(Count(1), 1);
    EXPECT_EQ(Count(2), 0);
    Function<void (*)(void (*)())>("FixtureNativePanelAllocHook")(nullptr);
}

TEST_F(NativePanelTreeTest, RollsBackNativeConfigurationExceptions)
{
    bool thrown = false;
    hook = [&thrown]() {
        if (!thrown)
        {
            thrown = true;
            throw std::runtime_error("synthetic native configuration failure");
        }
    };
    Function<void (*)(void (*)())>("FixturePanelHook")(&InvokeHook);
    EXPECT_FALSE(Bind());
    EXPECT_TRUE(thrown);
    EXPECT_FALSE(tree.IsReady());
    EXPECT_EQ(Count(0), 1);
    EXPECT_EQ(Count(1), 1);
    EXPECT_EQ(Count(2), 0);
}

TEST_F(NativePanelTreeTest, KeepsBorrowedAndOwnedPanelsAliveUntilPaintReturns)
{
    ASSERT_TRUE(Bind());
    const auto borrowed = Borrow(tree.GetPanel(1));
    hook = [this]() { tree.Shutdown(); };
    Function<void (*)(void (*)())>("FixturePanelHook")(&InvokeHook);
    Function<void (*)(uint32_t)>("FixturePanelPaint")(tree.GetPanel(0));
    EXPECT_TRUE(tree.ReleaseRequested());
    EXPECT_TRUE(tree.IsReady());
    EXPECT_EQ(Count(1), 0);
    tree.Shutdown();
    EXPECT_EQ(Parent(borrowed), 0u);
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(Count(2), 0);
}

TEST_F(NativePanelTreeTest, DetachFailureDoesNotSkipIndependentNodesAndCanBeRetried)
{
    ASSERT_TRUE(Bind());
    Function<void (*)(int)>("FixturePanelThrowParent")(1);
    EXPECT_THROW(tree.Shutdown(), std::runtime_error);
    EXPECT_FALSE(tree.IsReady());
    EXPECT_TRUE(tree.ReleaseRequested());
    EXPECT_EQ(Count(1), 2);
    EXPECT_EQ(Count(2), 0);
    tree.Shutdown();
    EXPECT_EQ(Count(1), 3);
    EXPECT_EQ(Count(3), 0);
    EXPECT_TRUE(Bind());
}

TEST_F(NativePanelTreeTest, ContainsUncertainFreeWithoutRetryingOrDestroyingLiveCallbacks)
{
    auto owner = std::make_unique<csr::client::NativePanelTree>();
    std::string error;
    ASSERT_TRUE(owner->Bind(module.get(), 800, 600, error));
    const auto uncertain = owner->GetPanel(2);
    Function<void (*)(int)>("FixturePanelThrowFree")(1);
    EXPECT_THROW(owner->Shutdown(), std::runtime_error);
    EXPECT_FALSE(owner->IsReady());
    EXPECT_TRUE(owner->ReleaseRequested());
    EXPECT_EQ(Count(1), 2);
    EXPECT_EQ(Count(2), 0);
    EXPECT_EQ(Function<int (*)()>("FixturePanelFreeCalls")(), 3);
    EXPECT_THROW(owner->Shutdown(), std::runtime_error);
    EXPECT_EQ(Function<int (*)()>("FixturePanelFreeCalls")(), 3);
    testing::internal::CaptureStderr();
    owner.reset();
    EXPECT_NE(testing::internal::GetCapturedStderr().find("retained native panel callbacks"), std::string::npos);
    EXPECT_EQ(Function<int (*)(uint32_t)>("FixturePanelProtocol")(uncertain), 1);
}

TEST_F(NativePanelTreeTest, TerminalSealPreventsAllocatorRetriesAndAllCallbackNativeAccess)
{
    auto owner = std::make_unique<csr::client::NativePanelTree>();
    std::string error;
    ASSERT_TRUE(owner->Bind(module.get(), 800, 600, error));
    Function<void (*)(int)>("FixturePanelThrowParent")(1);
    EXPECT_THROW(owner->Shutdown(), std::runtime_error);
    owner->EndNativeLifetime();
    Function<void (*)()>("FixtureNativePanelInvalidate")();
    const auto parent_calls = Function<int (*)()>("FixturePanelParentCalls")();
    const auto free_calls = Function<int (*)()>("FixturePanelFreeCalls")();
    EXPECT_EQ(Function<int (*)()>("FixtureNativePanelProbeInvalidated")(), 1);
    owner->Resize(1024, 768);
    owner->Shutdown();
    owner->EndNativeLifetime();
    EXPECT_FALSE(owner->IsReady());
    EXPECT_TRUE(owner->ReleaseRequested());
    EXPECT_EQ(owner->GetPanel(0), 0u);
    EXPECT_EQ(owner->Root(), nullptr);
    EXPECT_FALSE(owner->Bind(module.get(), 800, 600, error));
    testing::internal::CaptureStderr();
    owner.reset();
    EXPECT_NE(testing::internal::GetCapturedStderr().find("retained native panel callbacks"), std::string::npos);
    EXPECT_EQ(Function<int (*)()>("FixtureNativePanelProbeInvalidated")(), 1);
    EXPECT_EQ(Function<int (*)()>("FixturePanelPostShutdownCalls")(), 0);
    EXPECT_EQ(Function<int (*)()>("FixturePanelParentCalls")(), parent_calls);
    EXPECT_EQ(Function<int (*)()>("FixturePanelFreeCalls")(), free_calls);
}

TEST_F(NativePanelTreeTest, SealDuringResizeStopsBeforeAnyFurtherNativeOperation)
{
    auto owner = std::make_unique<csr::client::NativePanelTree>();
    std::string error;
    ASSERT_TRUE(owner->Bind(module.get(), 800, 600, error));
    hook = [&owner, this]() {
        owner->EndNativeLifetime();
        Function<void (*)()>("FixtureNativePanelInvalidate")();
    };
    Function<void (*)(void (*)())>("FixturePanelHook")(&InvokeHook);
    owner->Resize(1024, 768);
    hook = {};
    EXPECT_FALSE(owner->IsReady());
    EXPECT_EQ(Function<int (*)()>("FixturePanelPostShutdownCalls")(), 0);
    testing::internal::CaptureStderr();
    owner.reset();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(Function<int (*)()>("FixtureNativePanelProbeInvalidated")(), 1);
}
