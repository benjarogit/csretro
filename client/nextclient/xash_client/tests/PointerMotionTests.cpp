#include "pointer_motion.h"
#include <gtest/gtest.h>
#include <climits>
#include <vector>

namespace
{
    struct Pointer
    {
        csr::client::PointerMotion motion;
        int absolute_x{239}, absolute_y{201};
        int x{}, y{}, dx{}, dy{};
        int relative{1}, visible{};
        int frame_visible{-1};
        int reads{};
        std::vector<int> events;
        csr::client::PointerHooks Hooks()
        {
            return {
                [](void* u, int* x, int* y) { auto& p = *static_cast<Pointer*>(u); *x = p.absolute_x; *y = p.absolute_y; },
                [](void* u) { return static_cast<Pointer*>(u)->relative; },
                [](void* u, int* x, int* y) { auto& p = *static_cast<Pointer*>(u); ++p.reads; p.events.push_back(3); *x = p.dx; *y = p.dy; },
                [](void* u) { return static_cast<Pointer*>(u)->visible; },
                [](void* u, int x, int y) { auto& p = *static_cast<Pointer*>(u); p.x = x; p.y = y; p.events.push_back(1); },
                [](void* u) { auto& p = *static_cast<Pointer*>(u); p.events.push_back(2); if (p.frame_visible >= 0) p.visible = p.frame_visible; },
                this};
        }
        void Update(int wide = 640, int tall = 480) { motion.Update(wide, tall, Hooks()); }
    };
}

TEST(PointerMotion, HiddenUiNeverConsumesTheOriginalGamesRelativeMouseDeltas)
{
    Pointer pointer;
    pointer.dx = 13; pointer.dy = 9;
    pointer.Update();
    EXPECT_EQ(pointer.reads, 0);
    EXPECT_EQ(pointer.x, 239); EXPECT_EQ(pointer.y, 201);
    EXPECT_EQ(pointer.events, (std::vector<int>{1, 2}));
    pointer.visible = 1; pointer.relative = 0;
    pointer.Update();
    EXPECT_EQ(pointer.reads, 0);
}

TEST(PointerMotion, VisibleRelativeUiUsesVirtualCoordinatesEvenWhenTheWindowPositionStaysLocked)
{
    Pointer pointer;
    pointer.visible = 1; pointer.dx = 5; pointer.dy = -3;
    pointer.Update();
    EXPECT_EQ(pointer.x, 244); EXPECT_EQ(pointer.y, 198);
    pointer.dx = 8; pointer.dy = 4;
    pointer.Update();
    EXPECT_EQ(pointer.x, 252); EXPECT_EQ(pointer.y, 202);
    EXPECT_EQ(pointer.reads, 2);
    EXPECT_EQ(pointer.events, (std::vector<int>{1, 2, 3, 1, 1, 2, 3, 1}));
}

TEST(PointerMotion, SynchronizesPositionBeforeVguiFrameAndObservesItsNewVisibilityBeforeReadingDeltas)
{
    Pointer pointer;
    pointer.visible = 1; pointer.frame_visible = 0;
    auto hooks = pointer.Hooks();
    hooks.frame = [](void* owner) {
        auto& p = *static_cast<Pointer*>(owner);
        EXPECT_EQ(p.x, 239); EXPECT_EQ(p.y, 201);
        p.visible = 0;
    };
    pointer.motion.Update(640, 480, hooks);
    EXPECT_EQ(pointer.reads, 0);
    pointer.frame_visible = 1; pointer.dx = 2;
    pointer.Update();
    EXPECT_EQ(pointer.reads, 1);
    EXPECT_EQ(pointer.x, 241);
}

TEST(PointerMotion, ClipsDeltasAndResizeWithoutOverflowAndResetRestoresAbsolutePosition)
{
    Pointer pointer;
    pointer.visible = 1; pointer.dx = INT_MAX; pointer.dy = INT_MIN;
    pointer.Update();
    EXPECT_EQ(pointer.x, 639); EXPECT_EQ(pointer.y, 0);
    pointer.dx = pointer.dy = 0;
    pointer.Update(24, 24);
    EXPECT_EQ(pointer.x, 23); EXPECT_EQ(pointer.y, 0);
    pointer.absolute_x = 7; pointer.absolute_y = 9;
    pointer.motion.Reset(); pointer.Update();
    EXPECT_EQ(pointer.x, 7); EXPECT_EQ(pointer.y, 9);
    const int before = pointer.reads;
    pointer.Update(0, 0);
    EXPECT_EQ(pointer.reads, before);
}
