#include "NativeSdlInput.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <SDL_events.h>
#include <gtest/gtest.h>

namespace
{
    class NativeSdl : public testing::Test
    {
    protected:
        void* module{};
        csr::client::NativeSdlInput input;
        csr::client::NativeSdlInput::Hooks hooks;
        void (*reset)(){};
        void (*configure)(int, int){};
        void (*emit)(const void*){};
        void (*native_hook)(void (*)(void*, int), void*){};
        int (*counter)(int){};
        int (*trace)(int, int){};
        bool wanted{true};
        int resets{};
        int move_x{};
        int move_y{};
        int button_index{-1};
        int button_down{};

        template <class T>
        T Symbol(const char* name)
        {
            return reinterpret_cast<T>(dlsym(module, name));
        }
        void SetUp() override
        {
            module = dlopen(CLIENT_NATIVE_SDL_FIXTURE, RTLD_NOW | RTLD_LOCAL);
            ASSERT_NE(module, nullptr) << dlerror();
            reset = Symbol<decltype(reset)>("FixtureSdlReset");
            configure = Symbol<decltype(configure)>("FixtureSdlConfigure");
            emit = Symbol<decltype(emit)>("FixtureSdlEmit");
            counter = Symbol<decltype(counter)>("FixtureSdlCounter");
            trace = Symbol<decltype(trace)>("FixtureSdlTrace");
            native_hook = Symbol<decltype(native_hook)>("FixtureSdlNativeHook");
            ASSERT_TRUE(reset && configure && emit && counter && trace && native_hook);
            reset();
            hooks.context = this;
            hooks.needs_keyboard = [](void* context) { return static_cast<NativeSdl*>(context)->wanted; };
            hooks.mouse_move = [](void* context, int x, int y) {
                auto& owner = *static_cast<NativeSdl*>(context);
                owner.move_x = x;
                owner.move_y = y;
            };
            hooks.mouse_button = [](void* context, int index, int down) {
                auto& owner = *static_cast<NativeSdl*>(context);
                owner.button_index = index;
                owner.button_down = down;
            };
            hooks.reset_mouse = [](void* context) { ++static_cast<NativeSdl*>(context)->resets; };
        }
        void TearDown() override
        {
            configure(0, 0);
            native_hook(nullptr, nullptr);
            EXPECT_TRUE(input.Shutdown());
            EXPECT_FALSE(input.HasPendingWatch());
            input.EndNativeLifetime();
            dlclose(module);
        }
        bool Bind()
        {
            std::string error;
            bool result = input.Bind(module, module, hooks, error);
            if (!result)
            {
                ADD_FAILURE() << error;
            }
            return result;
        }
        SDL_Event Key(Uint32 type = SDL_KEYDOWN, int scan = SDL_SCANCODE_A, int sym = SDLK_a, bool repeat = false, Uint32 window = 41)
        {
            SDL_Event event{};
            event.type = type;
            event.key.windowID = window;
            event.key.keysym.scancode = static_cast<SDL_Scancode>(scan);
            event.key.keysym.sym = sym;
            event.key.repeat = repeat;
            event.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
            return event;
        }
        SDL_Event Text(const char* value, Uint32 window = 41)
        {
            SDL_Event event{};
            event.type = SDL_TEXTINPUT;
            event.text.windowID = window;
            std::strncpy(event.text.text, value, sizeof(event.text.text));
            return event;
        }
        SDL_Event Focus(Uint8 value)
        {
            SDL_Event event{};
            event.type = SDL_WINDOWEVENT;
            event.window.windowID = 41;
            event.window.event = value;
            return event;
        }
        void Send(const SDL_Event& event)
        {
            emit(&event);
        }
        int Entries()
        {
            return trace(-1, 0);
        }
    };
} // namespace

TEST_F(NativeSdl, RejectsMissingModulesHooksAndExistingRegistration)
{
    std::string error;
    EXPECT_FALSE(input.Bind(nullptr, module, hooks, error));
    auto incomplete = hooks;
    incomplete.needs_keyboard = nullptr;
    EXPECT_FALSE(input.Bind(module, module, incomplete, error));
    ASSERT_TRUE(Bind());
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    EXPECT_EQ(counter(0), 1);
}
TEST_F(NativeSdl, RejectsWrongSdlMajorWithoutRegisteringWatch)
{
    configure(1, 3);
    std::string error;
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    EXPECT_EQ(counter(0), 0);
}
TEST_F(NativeSdl, RejectsMissingExistingWindowAndInputInterface)
{
    configure(2, 0);
    std::string error;
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    configure(2, 1);
    configure(0, 1);
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    EXPECT_EQ(counter(0), 0);
}
TEST_F(NativeSdl, WatcherDefersAllNativeInputToMainThreadDrain)
{
    ASSERT_TRUE(Bind());
    auto event = Key();
    std::thread worker([&] { Send(event); });
    worker.join();
    EXPECT_EQ(Entries(), 0);
    input.Drain();
    ASSERT_EQ(Entries(), 2);
    EXPECT_EQ(trace(0, 0), 1);
    EXPECT_EQ(trace(1, 0), 2);
    EXPECT_EQ(counter(3), 0);
}
TEST_F(NativeSdl, PressRepeatReleaseAreBalancedAndRepeatedPressIsNotInvented)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    Send(Key());
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_A, SDLK_a, true));
    Send(Key(SDL_KEYUP));
    input.Drain();
    ASSERT_EQ(Entries(), 4);
    EXPECT_EQ(trace(0, 0), 1);
    EXPECT_EQ(trace(1, 0), 2);
    EXPECT_EQ(trace(2, 0), 2);
    EXPECT_EQ(trace(3, 0), 3);
    EXPECT_EQ(trace(0, 1), 11);
    Send(Key(SDL_KEYUP));
    input.Drain();
    EXPECT_EQ(Entries(), 4);
}
TEST_F(NativeSdl, InitialRepeatWithoutObservedPressDoesNotInventHeldState)
{
    ASSERT_TRUE(Bind());
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_A, SDLK_a, true));
    input.Drain();
    EXPECT_EQ(Entries(), 0);
}
TEST_F(NativeSdl, IgnoresUnknownKeysAndInvalidPhysicalScancodes)
{
    ASSERT_TRUE(Bind());
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_A, SDLK_UNKNOWN));
    Send(Key(SDL_KEYDOWN, 0));
    Send(Key(SDL_KEYDOWN, SDL_NUM_SCANCODES));
    input.Drain();
    EXPECT_EQ(Entries(), 0);
}
TEST_F(NativeSdl, OnlyActualCurrentWindowIsObserved)
{
    ASSERT_TRUE(Bind());
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_A, SDLK_a, false, 42));
    Send(Text("x", 42));
    input.Drain();
    EXPECT_EQ(Entries(), 0);
    Send(Key());
    input.Drain();
    EXPECT_EQ(Entries(), 2);
}
TEST_F(NativeSdl, RefreshingWindowReleasesOldKeysAndDiscardsOldWindowQueue)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    input.Drain();
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_B, SDLK_b));
    configure(3, 42);
    input.Drain();
    ASSERT_EQ(Entries(), 3);
    EXPECT_EQ(trace(2, 0), 3);
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_B, SDLK_b, false, 42));
    input.Drain();
    EXPECT_EQ(Entries(), 5);
}
TEST_F(NativeSdl, LosingKeyboardOwnershipReleasesHeldKeysEvenWithoutNewEvents)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    input.Drain();
    wanted = false;
    input.Drain();
    ASSERT_EQ(Entries(), 3);
    EXPECT_EQ(trace(2, 0), 3);
    Send(Text("ignored"));
    input.Drain();
    EXPECT_EQ(Entries(), 3);
}
TEST_F(NativeSdl, FocusLossReleasesKeysAndFocusGainAllowsGenuineNewPress)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    input.Drain();
    Send(Focus(SDL_WINDOWEVENT_FOCUS_LOST));
    Send(Key(SDL_KEYDOWN, SDL_SCANCODE_B, SDLK_b));
    input.Drain();
    ASSERT_EQ(Entries(), 3);
    EXPECT_EQ(trace(2, 0), 3);
    Send(Key());
    input.Drain();
    EXPECT_EQ(Entries(), 3);
    Send(Focus(SDL_WINDOWEVENT_FOCUS_GAINED));
    Send(Key());
    input.Drain();
    EXPECT_EQ(Entries(), 5);
}
TEST_F(NativeSdl, Utf8TextUsesUnicodeScalarsIncludingNonBmpWithoutSyntheticKeyText)
{
    ASSERT_TRUE(Bind());
    Send(Text("A\xc3\xa4\xe2\x82\xac\xf0\x9f\x99\x82"));
    input.Drain();
    ASSERT_EQ(Entries(), 4);
    EXPECT_EQ(trace(0, 0), 4);
    EXPECT_EQ(trace(0, 1), 'A');
    EXPECT_EQ(trace(1, 1), 0xe4);
    EXPECT_EQ(trace(2, 1), 0x20ac);
    EXPECT_EQ(trace(3, 1), 0x1f642);
}
TEST_F(NativeSdl, InvalidUtf8DropsWholePayloadInsteadOfEmittingPartialText)
{
    ASSERT_TRUE(Bind());
    const char* invalid[] = {"A\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "\x80"};
    for (const char* value : invalid)
    {
        Send(Text(value));
    }
    input.Drain();
    EXPECT_EQ(Entries(), 0);
}
TEST_F(NativeSdl, UnterminatedTextPayloadIsRejectedWithinTheFixedSdlBuffer)
{
    ASSERT_TRUE(Bind());
    auto event = Text("");
    std::memset(event.text.text, 'a', sizeof(event.text.text));
    Send(event);
    input.Drain();
    EXPECT_EQ(Entries(), 0);
}
TEST_F(NativeSdl, QueueOverflowDropsQueuedInputAndReleasesExistingHeldKeys)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    input.Drain();
    for (int i = 0; i < 300; ++i)
    {
        Send(Text("x"));
    }
    input.Drain();
    ASSERT_EQ(Entries(), 3);
    EXPECT_EQ(trace(2, 0), 3);
    EXPECT_GT(resets, 0);
    Send(Key());
    input.Drain();
    EXPECT_EQ(Entries(), 5);
}
TEST_F(NativeSdl, ExplicitGenuineMouseEventsUseExistingHooksAndNativeWheel)
{
    ASSERT_TRUE(Bind());
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 41;
    event.motion.x = 15;
    event.motion.y = 29;
    input.HandleEvent(&event);
    EXPECT_EQ(move_x, 15);
    EXPECT_EQ(move_y, 29);
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = 41;
    event.button.button = SDL_BUTTON_RIGHT;
    input.HandleEvent(&event);
    EXPECT_EQ(button_index, 1);
    EXPECT_EQ(button_down, 1);
    event.type = SDL_MOUSEBUTTONUP;
    input.HandleEvent(&event);
    EXPECT_EQ(button_down, 0);
    event = {};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.windowID = 41;
    event.wheel.y = -2;
    input.HandleEvent(&event);
    ASSERT_EQ(Entries(), 1);
    EXPECT_EQ(trace(0, 0), 5);
    EXPECT_EQ(trace(0, 1), -2);
}
TEST_F(NativeSdl, WatcherDoesNotDuplicateExistingMouseBridge)
{
    ASSERT_TRUE(Bind());
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.windowID = 41;
    event.motion.x = 90;
    Send(event);
    input.Drain();
    EXPECT_EQ(move_x, 0);
    EXPECT_EQ(Entries(), 0);
}
TEST_F(NativeSdl, ClearRequestedInsideNativePressDefersReleaseAndSuppressesTyping)
{
    ASSERT_TRUE(Bind());
    native_hook(
        [](void* value, int kind) {
            if (kind == 1)
            {
                static_cast<csr::client::NativeSdlInput*>(value)->Clear();
            }
        },
        &input
    );
    Send(Key());
    input.Drain();
    ASSERT_EQ(Entries(), 2);
    EXPECT_EQ(trace(0, 0), 1);
    EXPECT_EQ(trace(1, 0), 3);
    EXPECT_TRUE(input.IsReady());
}
TEST_F(NativeSdl, ShutdownRequestedInsideNativePressKeepsStateUntilDispatchReturns)
{
    ASSERT_TRUE(Bind());
    bool returned = true;
    struct Context
    {
        csr::client::NativeSdlInput* input;
        bool* returned;
    } context{&input, &returned};
    native_hook(
        [](void* value, int kind) {
            if (kind == 1)
            {
                auto* context = static_cast<Context*>(value);
                *context->returned = context->input->Shutdown();
                EXPECT_TRUE(context->input->HasPendingWatch());
            }
        },
        &context
    );
    Send(Key());
    input.Drain();
    EXPECT_FALSE(returned);
    ASSERT_EQ(Entries(), 2);
    EXPECT_EQ(trace(1, 0), 3);
    EXPECT_TRUE(input.ReleaseRequested());
    EXPECT_TRUE(input.Shutdown());
    EXPECT_FALSE(input.HasPendingWatch());
}
TEST_F(NativeSdl, SealInsideNativePressPreventsAllFurtherNativeOperations)
{
    ASSERT_TRUE(Bind());
    native_hook(
        [](void* value, int kind) {
            if (kind == 1)
            {
                static_cast<csr::client::NativeSdlInput*>(value)->EndNativeLifetime();
            }
        },
        &input
    );
    Send(Key());
    Send(Text("never"));
    input.Drain();
    ASSERT_EQ(Entries(), 1);
    input.Clear();
    input.Drain();
    EXPECT_TRUE(input.Shutdown());
    EXPECT_EQ(Entries(), 1);
    EXPECT_EQ(resets, 0);
}
TEST_F(NativeSdl, ForeignThreadShutdownOnlyRequestsMainThreadCleanup)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    input.Drain();
    bool returned = true;
    std::thread worker([&] { returned = input.Shutdown(); });
    worker.join();
    EXPECT_FALSE(returned);
    EXPECT_TRUE(input.HasPendingWatch());
    EXPECT_EQ(Entries(), 2);
    EXPECT_EQ(counter(1), 0);
    EXPECT_TRUE(input.Shutdown());
    ASSERT_EQ(Entries(), 3);
    EXPECT_EQ(trace(2, 0), 3);
    EXPECT_EQ(counter(3), 0);
}
TEST_F(NativeSdl, RemovalWaitsForActualConcurrentSdlDispatchToQuiesce)
{
    ASSERT_TRUE(Bind());
    configure(4, 1);
    auto event = Key();
    std::thread worker([&] { Send(event); });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    while (!counter(4) && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    EXPECT_TRUE(counter(4));
    EXPECT_TRUE(input.Shutdown());
    worker.join();
    EXPECT_EQ(counter(2), 0);
    EXPECT_FALSE(input.HasPendingWatch());
    EXPECT_EQ(Entries(), 0);
}
TEST_F(NativeSdl, FailedWatchRemovalRetainsStateForAnExplicitSuccessfulRetry)
{
    ASSERT_TRUE(Bind());
    configure(0, 3);
    EXPECT_FALSE(input.Shutdown());
    EXPECT_TRUE(input.HasPendingWatch());
    Send(Key());
    EXPECT_EQ(Entries(), 0);
    configure(0, 0);
    EXPECT_TRUE(input.Shutdown());
    EXPECT_FALSE(input.HasPendingWatch());
}
TEST_F(NativeSdl, PartialWatchRegistrationFailureIsActuallyRemovedBeforeBindReturns)
{
    configure(0, 2);
    std::string error;
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    EXPECT_EQ(counter(0), 1);
    EXPECT_EQ(counter(1), 1);
    EXPECT_EQ(counter(2), 0);
    EXPECT_FALSE(input.HasPendingWatch());
}
TEST_F(NativeSdl, DirectEventFromForeignThreadCannotInvokeNativeInput)
{
    ASSERT_TRUE(Bind());
    auto event = Key();
    std::thread worker([&] { input.HandleEvent(&event); });
    worker.join();
    EXPECT_EQ(Entries(), 0);
    EXPECT_EQ(counter(3), 0);
}
TEST_F(NativeSdl, SealedOwnerCannotBeReusedToCallDeletedNativeServices)
{
    ASSERT_TRUE(Bind());
    input.EndNativeLifetime();
    Send(Key());
    input.Drain();
    auto event = Key();
    input.HandleEvent(&event);
    EXPECT_EQ(Entries(), 0);
    EXPECT_TRUE(input.Shutdown());
}

TEST_F(NativeSdl, FocusLostAndRegainedInOneBufferedBatchDoesNotLatchKeyboardOff)
{
    ASSERT_TRUE(Bind());
    Send(Key());
    input.Drain();
    Send(Focus(SDL_WINDOWEVENT_FOCUS_LOST));
    Send(Focus(SDL_WINDOWEVENT_FOCUS_GAINED));
    Send(Key());
    input.Drain();
    ASSERT_EQ(Entries(), 5);
    EXPECT_EQ(trace(2, 0), 3);
    EXPECT_EQ(trace(3, 0), 1);
}

TEST_F(NativeSdl, FocusChangesArePreservedWhileTheUiDoesNotRequestKeyboard)
{
    ASSERT_TRUE(Bind());
    Send(Focus(SDL_WINDOWEVENT_FOCUS_LOST));
    input.Drain();
    wanted = false;
    Send(Focus(SDL_WINDOWEVENT_FOCUS_GAINED));
    input.Drain();
    wanted = true;
    Send(Key());
    input.Drain();
    ASSERT_EQ(Entries(), 2);
    EXPECT_EQ(trace(0, 0), 1);
}

TEST_F(NativeSdl, RejectsOldSdl2WithoutProvenWatcherQuiescenceBeforeRegistration)
{
    std::string error;
    configure(5, 0);
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    configure(5, 6);
    EXPECT_FALSE(input.Bind(module, module, hooks, error));
    EXPECT_EQ(counter(0), 0);
    EXPECT_FALSE(input.HasPendingWatch());
    configure(5, 8);
    EXPECT_TRUE(input.Bind(module, module, hooks, error)) << error;
    EXPECT_EQ(counter(0), 1);
}
