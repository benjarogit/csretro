#include "client_callbacks.h"
#include <gtest/gtest.h>
#include <vector>

namespace
{
    struct RuntimeLog
    {
        std::vector<int> events;
        float redraw_time{};
        int intermission{};
        double frame_time{};
        int buttons{};
        int result{17};
    };
    RuntimeLog* log;
    int Redraw(float time, int intermission)
    {
        log->events.push_back(1); log->redraw_time = time; log->intermission = intermission; return log->result;
    }
    void Frame(double time) { log->events.push_back(1); log->frame_time = time; }
    void Mouse(int state) { log->events.push_back(2); log->buttons = state; }
    void Clear() { log->events.push_back(1); }
    void After(void* owner) { static_cast<RuntimeLog*>(owner)->events.push_back(3); }
    void Before(void* owner) { static_cast<RuntimeLog*>(owner)->events.push_back(1); }
    void UiMouse(void* owner, int state)
    {
        auto& data = *static_cast<RuntimeLog*>(owner);
        EXPECT_EQ(state, data.buttons);
        data.events.push_back(3);
    }
}

TEST(RuntimeCallbacks, OriginalRedrawArgumentsAndResultSurviveTheUiPaint)
{
    RuntimeLog data; log = &data;
    EXPECT_EQ(csr::client::RedrawWithVgui(Redraw, 2.5f, 7, After, &data), 17);
    EXPECT_FLOAT_EQ(data.redraw_time, 2.5f); EXPECT_EQ(data.intermission, 7);
    EXPECT_EQ(data.events, (std::vector<int>{1, 3}));
    data.events.clear(); data.result = 0;
    EXPECT_EQ(csr::client::RedrawWithVgui(Redraw, 2.5f, 7, After, &data), 0);
    EXPECT_EQ(data.events, (std::vector<int>{1}));
    data.events.clear();
    EXPECT_EQ(csr::client::RedrawWithVgui(nullptr, 0, 0, After, &data), 0);
    EXPECT_TRUE(data.events.empty());
}

TEST(RuntimeCallbacks, FrameAndInputResetFollowTheOriginalWithoutChangingTheirArguments)
{
    RuntimeLog data; log = &data;
    csr::client::FrameWithVgui(Frame, 1234.125, After, &data);
    EXPECT_DOUBLE_EQ(data.frame_time, 1234.125);
    EXPECT_EQ(data.events, (std::vector<int>{1, 3}));
    data.events.clear();
    csr::client::ResetInputWithVgui(Clear, After, &data);
    EXPECT_EQ(data.events, (std::vector<int>{1, 3}));
    data.events.clear();
    csr::client::FrameWithVgui(nullptr, 1, After, &data);
    csr::client::ResetInputWithVgui(nullptr, After, &data);
    EXPECT_TRUE(data.events.empty());
}

TEST(RuntimeCallbacks, FocusRefreshPrecedesGameMouseHandlingAndUiButtonsFollowIt)
{
    RuntimeLog data; log = &data;
    csr::client::MouseWithVgui(Mouse, 19, Before, UiMouse, &data);
    EXPECT_EQ(data.buttons, 19);
    EXPECT_EQ(data.events, (std::vector<int>{1, 2, 3}));
    data.events.clear();
    csr::client::MouseWithVgui(nullptr, 1, Before, UiMouse, &data);
    EXPECT_TRUE(data.events.empty());
}

namespace
{
    struct Buttons
    {
        csr::client::MouseButtonState state;
        std::vector<int> events;
        bool clear_on_press{};
        bool update_on_release{};
        static void Emit(void* owner, int button, int down)
        {
            auto& self = *static_cast<Buttons*>(owner);
            self.events.push_back(button * 10 + down);
            if (button == 0 && down && self.clear_on_press)
            {
                self.clear_on_press = false;
                self.state.Clear(Emit, &self);
            }
            if (button == 0 && !down && self.update_on_release)
            {
                self.update_on_release = false;
                self.state.Update(1, Emit, &self);
            }
        }
        void Update(int state) { this->state.Update(state, Emit, this); }
        void Clear() { state.Clear(Emit, this); }
    };
}

TEST(MouseButtonState, EmitsOnlyTransitionsAndReleasesHeldButtonsExactlyOnce)
{
    Buttons buttons;
    buttons.Update(1); buttons.Update(1); buttons.Update(3); buttons.Update(2);
    buttons.Clear(); buttons.Clear();
    EXPECT_EQ(buttons.events, (std::vector<int>{1, 11, 0, 10}));
    buttons.events.clear();
    buttons.Update(1 << 7);
    EXPECT_TRUE(buttons.events.empty());
    buttons.Update(1); buttons.state.Clear(nullptr, nullptr); buttons.Update(1);
    EXPECT_EQ(buttons.events, (std::vector<int>{1, 1}));
}

TEST(MouseButtonState, ReentrantClearCancelsTheRemainingPressesAndCannotLeaveAStuckButton)
{
    Buttons buttons;
    buttons.clear_on_press = true;
    buttons.Update(3);
    EXPECT_EQ(buttons.events, (std::vector<int>{1, 0}));
    buttons.events.clear();
    buttons.Update(2); buttons.Clear();
    EXPECT_EQ(buttons.events, (std::vector<int>{11, 10}));
}

TEST(MouseButtonState, ReentrantUpdateDuringReleaseRetainsOnlyTheNewlyHeldButtons)
{
    Buttons buttons;
    buttons.Update(3); buttons.events.clear();
    buttons.update_on_release = true;
    buttons.Clear();
    EXPECT_EQ(buttons.events, (std::vector<int>{0, 1, 10}));
    buttons.events.clear(); buttons.Clear(); buttons.Clear();
    EXPECT_EQ(buttons.events, (std::vector<int>{0}));
}
