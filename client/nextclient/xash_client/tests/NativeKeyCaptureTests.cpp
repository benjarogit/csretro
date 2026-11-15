#include "NativeKeyCapture.h"
#include "vgui_binding.h"
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
    class NativeKeyCaptureTest : public testing::Test
    {
    protected:
        struct EventRecord
        {
            int down;
            int key;
            const char* binding;
            bool repeat;
        };
        static NativeKeyCaptureTest* current;
        csr::client::NativeKeyCapture keys;
        std::vector<EventRecord> original_calls, captured;
        std::function<void()> on_capture, on_original;
        bool visible{};
        int result{37};
        char binding[16]{"+attack"};
        static int Original(int down, int key, const char* text)
        {
            current->original_calls.push_back({down, key, text, false});
            if (current->on_original)
            {
                current->on_original();
            }
            return current->result;
        }
        static bool Capture(void* owner, int down, int key, const char* text, bool repeat)
        {
            auto& self = *static_cast<NativeKeyCaptureTest*>(owner);
            self.captured.push_back({down, key, text, repeat});
            if (self.on_capture)
            {
                self.on_capture();
            }
            return self.visible;
        }
        int Event(int down, int key = 42, const char* text = nullptr)
        {
            return keys.Event(Original, down, key, text ? text : binding, Capture, this);
        }
        void SetUp() override
        {
            current = this;
        }
    };
    NativeKeyCaptureTest* NativeKeyCaptureTest::current{};
} // namespace
TEST_F(NativeKeyCaptureTest, ForwardsExactOriginalArgumentsAndResultsWhenNotCaptured)
{
    EXPECT_EQ(Event(7), 37);
    result = -9;
    EXPECT_EQ(Event(0), -9);
    ASSERT_EQ(original_calls.size(), 2u);
    EXPECT_EQ(original_calls[0].down, 7);
    EXPECT_EQ(original_calls[0].key, 42);
    EXPECT_EQ(original_calls[0].binding, binding);
    EXPECT_EQ(original_calls[1].down, 0);
    EXPECT_EQ(original_calls[1].binding, binding);
    ASSERT_EQ(captured.size(), 1u);
    EXPECT_FALSE(captured[0].repeat);
}
TEST_F(NativeKeyCaptureTest, UiOwnedPressAndReleaseStayConsumedAfterUiCloses)
{
    visible = true;
    EXPECT_EQ(Event(1), 0);
    visible = false;
    EXPECT_EQ(Event(1), 0);
    EXPECT_EQ(Event(0), 0);
    EXPECT_TRUE(original_calls.empty());
    ASSERT_EQ(captured.size(), 2u);
    EXPECT_TRUE(captured[1].repeat);
    EXPECT_EQ(Event(1), 37);
    EXPECT_FALSE(captured.back().repeat);
}
TEST_F(NativeKeyCaptureTest, GameOwnedRepeatAndReleaseStayOriginalWhenUiOpens)
{
    EXPECT_EQ(Event(1), 37);
    visible = true;
    result = -18;
    char repeat_binding[]{"+forward"};
    EXPECT_EQ(Event(9, 42, repeat_binding), -18);
    EXPECT_EQ(Event(0), -18);
    ASSERT_EQ(original_calls.size(), 3u);
    EXPECT_EQ(original_calls[1].down, 9);
    EXPECT_EQ(original_calls[1].binding, repeat_binding);
    EXPECT_EQ(original_calls.back().down, 0);
    ASSERT_EQ(captured.size(), 1u);
    EXPECT_FALSE(captured[0].repeat);
}
TEST_F(NativeKeyCaptureTest, RepeatsExposeSingleFirstDownForMenuOrConsoleToggle)
{
    int toggles{};
    on_capture = [&] {
        if (!captured.back().repeat)
        {
            ++toggles;
            visible = !visible;
        }
    };
    EXPECT_EQ(Event(1), 0);
    EXPECT_EQ(Event(1), 0);
    EXPECT_EQ(Event(1), 0);
    EXPECT_EQ(Event(0), 0);
    EXPECT_EQ(toggles, 1);
    EXPECT_EQ(Event(1), 37);
    EXPECT_EQ(toggles, 2);
}
TEST_F(NativeKeyCaptureTest, UnknownKeysPassWithoutUiCapture)
{
    visible = true;
    for (int key : {-1, 0, 265, std::numeric_limits<int>::max()})
    {
        EXPECT_EQ(Event(1, key), 37);
        EXPECT_EQ(Event(0, key), 37);
    }
    EXPECT_TRUE(captured.empty());
    EXPECT_EQ(original_calls.size(), 8u);
    EXPECT_EQ(keys.Event(Original, 1, 0, nullptr, Capture, this), 37);
    EXPECT_EQ(original_calls.back().binding, nullptr);
}
TEST_F(NativeKeyCaptureTest, MissingCallbacksAndUnmatchedReleaseAreSafe)
{
    EXPECT_EQ(keys.Event(nullptr, 1, 42, binding, nullptr, nullptr), 0);
    EXPECT_EQ(keys.Event(nullptr, 0, 42, binding, nullptr, nullptr), 0);
    visible = true;
    EXPECT_EQ(Event(0, 43), 37);
    EXPECT_TRUE(captured.empty());
}
TEST_F(NativeKeyCaptureTest, NestedPressCannotBypassCaptureAndItsReleaseStaysConsumed)
{
    on_capture = [&] { EXPECT_EQ(Event(1, 43), 0); };
    EXPECT_EQ(Event(1), 37);
    on_capture = {};
    EXPECT_EQ(Event(0, 43), 0);
    ASSERT_EQ(original_calls.size(), 1u);
    ASSERT_EQ(captured.size(), 1u);
    EXPECT_EQ(Event(0), 37);
}
TEST_F(NativeKeyCaptureTest, OriginalCallbackReentryAlsoFailsClosed)
{
    on_original = [&] { EXPECT_EQ(Event(1, 43), 0); };
    EXPECT_EQ(Event(1), 37);
    on_original = {};
    EXPECT_EQ(Event(0, 43), 0);
    EXPECT_EQ(original_calls.size(), 1u);
    EXPECT_EQ(captured.size(), 1u);
}
TEST_F(NativeKeyCaptureTest, CaptureExceptionProtectsFreshReleaseAndRestoresGuard)
{
    on_capture = [] { throw std::runtime_error("capture failed"); };
    EXPECT_THROW(Event(1), std::runtime_error);
    on_capture = {};
    EXPECT_EQ(Event(0), 0);
    EXPECT_TRUE(original_calls.empty());
    EXPECT_EQ(Event(1), 37);
    EXPECT_FALSE(captured.back().repeat);
}
TEST_F(NativeKeyCaptureTest, GameOwnedRepeatNeverInvokesCaptureAfterUiTransition)
{
    EXPECT_EQ(Event(1), 37);
    visible = true;
    on_capture = [] { throw std::runtime_error("capture must not run"); };
    EXPECT_EQ(Event(1), 37);
    EXPECT_EQ(Event(0), 37);
    EXPECT_EQ(original_calls.size(), 3u);
    EXPECT_EQ(captured.size(), 1u);
}
TEST_F(NativeKeyCaptureTest, NativeKeyDomainIncludesInternationalKeys)
{
    visible = true;
    for (int key : {1, 255, 256, 264})
    {
        EXPECT_EQ(Event(1, key), 0);
        EXPECT_EQ(Event(0, key), 0);
    }
    EXPECT_EQ(captured.size(), 4u);
    EXPECT_TRUE(original_calls.empty());
}
TEST_F(NativeKeyCaptureTest, OriginalExceptionRestoresGuardAndKeepsGameRelease)
{
    on_original = [] { throw std::runtime_error("original callback failed"); };
    EXPECT_THROW(Event(1), std::runtime_error);
    on_original = {};
    visible = true;
    EXPECT_EQ(Event(0), 37);
    EXPECT_EQ(original_calls.size(), 2u);
    EXPECT_EQ(captured.size(), 1u);
}


TEST_F(NativeKeyCaptureTest, ActualGameKeyReleaseAllowsFreshUiCapture)
{
    EXPECT_EQ(Event(1), 37);
    // Engine focus clearing delivers real key releases before the mouse callback.
    EXPECT_EQ(Event(0), 37);
    visible = true;
    EXPECT_EQ(Event(1), 0);
    ASSERT_EQ(captured.size(), 2u);
    EXPECT_FALSE(captured.back().repeat);
    EXPECT_EQ(Event(0), 0);
    EXPECT_EQ(original_calls.size(), 2u);
}
TEST_F(NativeKeyCaptureTest, ActualUiKeyReleaseAllowsFreshGamePress)
{
    visible = true;
    EXPECT_EQ(Event(1), 0);
    EXPECT_EQ(Event(0), 0);
    visible = false;
    EXPECT_EQ(Event(1), 37);
    EXPECT_FALSE(captured.back().repeat);
    EXPECT_EQ(Event(0), 37);
    EXPECT_EQ(original_calls.size(), 2u);
}
TEST_F(NativeKeyCaptureTest, ActualFocusReleaseClearsEachPressOwnerIndependently)
{
    EXPECT_EQ(Event(1, 42), 37);
    visible = true;
    EXPECT_EQ(Event(1, 43), 0);
    EXPECT_EQ(Event(0, 42), 37);
    EXPECT_EQ(Event(0, 43), 0);
    EXPECT_EQ(Event(1, 42), 0);
    EXPECT_FALSE(captured.back().repeat);
    EXPECT_EQ(Event(1, 43), 0);
    EXPECT_FALSE(captured.back().repeat);
    EXPECT_EQ(original_calls.size(), 2u);
}
TEST_F(NativeKeyCaptureTest, OriginalReleaseReentryCannotLeakNewGameplayPress)
{
    EXPECT_EQ(Event(1), 37);
    on_original = [&] { EXPECT_EQ(Event(1), 0); };
    EXPECT_EQ(Event(0), 37);
    on_original = {};
    EXPECT_EQ(Event(0), 0);
    EXPECT_EQ(original_calls.size(), 2u);
    EXPECT_EQ(Event(1), 37);
    EXPECT_FALSE(captured.back().repeat);
}
TEST_F(NativeKeyCaptureTest, GameplayReleaseInsideAnotherKeyCaptureForwardsExactlyOnce)
{
    EXPECT_EQ(Event(1, 'w'), 37);
    visible = true;
    char release_binding[]{"+forward"};
    on_capture = [&] { EXPECT_EQ(Event(0, 'w', release_binding), 37); };
    EXPECT_EQ(Event(1, 27), 0);
    ASSERT_EQ(original_calls.size(), 2u);
    EXPECT_EQ(original_calls.back().down, 0);
    EXPECT_EQ(original_calls.back().key, 'w');
    EXPECT_EQ(original_calls.back().binding, release_binding);
    on_capture = {};
    EXPECT_EQ(Event(1, 'w'), 0);
    EXPECT_FALSE(captured.back().repeat);
}
TEST_F(NativeKeyCaptureTest, NestedUiReleaseClearsOwnershipWithoutForwarding)
{
    visible = true;
    EXPECT_EQ(Event(1, 43), 0);
    on_capture = [&] { EXPECT_EQ(Event(0, 43), 0); };
    EXPECT_EQ(Event(1, 42), 0);
    on_capture = {};
    EXPECT_EQ(Event(1, 43), 0);
    EXPECT_FALSE(captured.back().repeat);
    EXPECT_TRUE(original_calls.empty());
}
TEST_F(NativeKeyCaptureTest, UnknownNestedReleasePreservesOriginalArgumentsAndResult)
{
    visible = true;
    on_capture = [&] { EXPECT_EQ(keys.Event(Original, 0, -9, nullptr, Capture, this), 37); };
    EXPECT_EQ(Event(1), 0);
    ASSERT_EQ(original_calls.size(), 1u);
    EXPECT_EQ(original_calls[0].key, -9);
    EXPECT_EQ(original_calls[0].down, 0);
    EXPECT_EQ(original_calls[0].binding, nullptr);
}
TEST_F(NativeKeyCaptureTest, SameKeyReleaseRecursionStopsButDifferentReleaseCanForward)
{
    EXPECT_EQ(Event(1, 42), 37);
    EXPECT_EQ(Event(1, 43), 37);
    on_original = [&] {
        const int key = original_calls.back().key;
        EXPECT_EQ(Event(0, key), 0);
        if (key == 42)
        {
            EXPECT_EQ(Event(0, 43), 37);
        }
    };
    EXPECT_EQ(Event(0, 42), 37);
    ASSERT_EQ(original_calls.size(), 4u);
    EXPECT_EQ(original_calls[2].key, 42);
    EXPECT_EQ(original_calls[3].key, 43);
}
TEST_F(NativeKeyCaptureTest, ThrowingNestedReleaseRestoresBothReentryGuards)
{
    EXPECT_EQ(Event(1, 43), 37);
    on_original = [] { throw std::runtime_error("release failed"); };
    on_capture = [&] { Event(0, 43); };
    EXPECT_THROW(Event(1, 42), std::runtime_error);
    on_original = {};
    on_capture = {};
    EXPECT_EQ(Event(0, 43), 37);
    EXPECT_EQ(Event(0, 42), 0);
    EXPECT_EQ(Event(1, 43), 37);
    EXPECT_FALSE(captured.back().repeat);
}
TEST_F(NativeKeyCaptureTest, FreshPressReleasedDuringCaptureNeverForwardsLateGameplayDown)
{
    on_capture = [&] { EXPECT_EQ(Event(0), 0); };
    EXPECT_EQ(Event(1), 0);
    EXPECT_TRUE(original_calls.empty());
    on_capture = {};
    EXPECT_EQ(Event(1), 37);
    EXPECT_FALSE(captured.back().repeat);
}

TEST_F(NativeKeyCaptureTest, InternationalUiPressRepeatAndReleaseRemainCaptured)
{
    for (int key : {256, 264})
    {
        visible = true;
        EXPECT_EQ(Event(1, key), 0);
        EXPECT_FALSE(captured.back().repeat);
        visible = false;
        EXPECT_EQ(Event(1, key), 0);
        EXPECT_TRUE(captured.back().repeat);
        EXPECT_EQ(Event(0, key), 0);
    }
    EXPECT_TRUE(original_calls.empty());
}
TEST_F(NativeKeyCaptureTest, InternationalGamePressRetainsExactRepeatAndRelease)
{
    for (int key : {256, 264})
    {
        visible = false;
        EXPECT_EQ(Event(1, key), 37);
        visible = true;
        result = -11;
        EXPECT_EQ(Event(7, key), -11);
        EXPECT_EQ(Event(0, key), -11);
        result = 37;
    }
    EXPECT_EQ(original_calls.size(), 6u);
    EXPECT_EQ(captured.size(), 2u);
}

namespace
{
    class GuardedNativeKeyCaptureTest : public NativeKeyCaptureTest
    {
    protected:
        void* client{};
        void* vgui{};
        csr::client::VguiBinding lifetime;
        int surface{};
        void SetUp() override
        {
            NativeKeyCaptureTest::SetUp();
            client = dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL);
            vgui = dlopen(CLIENT_VGUI_FIXTURE, RTLD_NOW | RTLD_LOCAL);
            ASSERT_NE(client, nullptr) << dlerror();
            ASSERT_NE(vgui, nullptr) << dlerror();
            reinterpret_cast<void (*)()>(dlsym(client, "FixtureClientBindingReset"))();
            reinterpret_cast<void (*)(int)>(dlsym(vgui, "FixtureVguiConfigure"))(0);
            std::string error;
            ASSERT_TRUE(lifetime.BindInterfaces(client, vgui, &surface, error)) << error;
            ASSERT_TRUE(lifetime.InitializeViewport(error)) << error;
            ASSERT_TRUE(lifetime.StartViewport(error)) << error;
        }
        void TearDown() override
        {
            on_capture = {};
            on_original = {};
            lifetime.Shutdown();
            dlclose(vgui);
            dlclose(client);
        }
        int ShutdownCount()
        {
            return reinterpret_cast<int (*)()>(dlsym(vgui, "FixtureVguiShutdownCount"))();
        }
        int Guarded(int down, int key = 42)
        {
            return keys.EventGuarded(lifetime, Original, down, key, binding, Capture, this);
        }
        void Outer(std::function<void()> operation)
        {
            lifetime.RunNativeUi([](void* value) { (*static_cast<std::function<void()>*>(value))(); }, &operation);
        }
    };
} // namespace
TEST_F(GuardedNativeKeyCaptureTest, PendingShutdownStillForwardsHeldGameplayReleaseBeforeTeardown)
{
    EXPECT_EQ(Guarded(1), 37);
    Outer([&] {
        lifetime.Shutdown();
        EXPECT_EQ(Guarded(0), 37);
        EXPECT_EQ(ShutdownCount(), 0);
        EXPECT_EQ(original_calls.size(), 2u);
        EXPECT_EQ(Guarded(1, 43), 0);
        EXPECT_EQ(captured.size(), 1u);
    });
    EXPECT_EQ(ShutdownCount(), 1);
    EXPECT_EQ(Guarded(0, 43), 0);
}
TEST_F(GuardedNativeKeyCaptureTest, PendingShutdownConsumesHeldUiReleaseAndUnboundReleaseStaysOwned)
{
    visible = true;
    EXPECT_EQ(Guarded(1), 0);
    EXPECT_EQ(Guarded(1, 43), 0);
    Outer([&] {
        lifetime.Shutdown();
        EXPECT_EQ(Guarded(0), 0);
        EXPECT_EQ(ShutdownCount(), 0);
    });
    EXPECT_FALSE(lifetime.HasBinding());
    EXPECT_EQ(Guarded(0, 43), 0);
    EXPECT_TRUE(original_calls.empty());
}
TEST_F(GuardedNativeKeyCaptureTest, CompletedOperationRetainsItsResultWhenGuardFinishesTeardown)
{
    on_original = [&] { lifetime.Shutdown(); };
    EXPECT_EQ(Guarded(1), 37);
    EXPECT_EQ(original_calls.size(), 1u);
    EXPECT_EQ(ShutdownCount(), 1);
    EXPECT_FALSE(lifetime.HasBinding());
    on_original = {};
    EXPECT_EQ(Guarded(0), 37);
    EXPECT_EQ(original_calls.size(), 2u);
}

TEST_F(NativeKeyCaptureTest, NestedGameplayRepeatPreservesOriginalStreamDuringAnotherCapture)
{
    EXPECT_EQ(Event(1, 'w'), 37);
    visible = true;
    char repeated[]{"+forward"};
    on_capture = [&] { EXPECT_EQ(Event(2, 'w', repeated), 37); };
    EXPECT_EQ(Event(1, 27), 0);
    ASSERT_EQ(original_calls.size(), 2u);
    EXPECT_EQ(original_calls.back().down, 2);
    EXPECT_EQ(original_calls.back().key, 'w');
    EXPECT_EQ(original_calls.back().binding, repeated);
    EXPECT_EQ(captured.size(), 2u);
}
TEST_F(NativeKeyCaptureTest, SameKeyOriginalDownRecursionStopsWhileOtherOwnedKeyForwards)
{
    EXPECT_EQ(Event(1, 43), 37);
    on_original = [&] {
        const int key = original_calls.back().key;
        EXPECT_EQ(Event(1, key), 0);
        if (key == 42)
        {
            EXPECT_EQ(Event(5, 43), 37);
        }
    };
    EXPECT_EQ(Event(1, 42), 37);
    ASSERT_EQ(original_calls.size(), 3u);
    EXPECT_EQ(original_calls[1].key, 42);
    EXPECT_EQ(original_calls[2].key, 43);
    EXPECT_EQ(original_calls[2].down, 5);
}
TEST_F(NativeKeyCaptureTest, InvalidNestedDownPassesUnchangedAndSameKeyRecursionStops)
{
    visible = true;
    on_original = [&] { EXPECT_EQ(Event(9, -1), 0); };
    on_capture = [&] { EXPECT_EQ(Event(3, -1), 37); };
    EXPECT_EQ(Event(1), 0);
    ASSERT_EQ(original_calls.size(), 1u);
    EXPECT_EQ(original_calls[0].down, 3);
    EXPECT_EQ(original_calls[0].key, -1);
    EXPECT_EQ(original_calls[0].binding, binding);
    EXPECT_EQ(captured.size(), 1u);
}
