#include "xash_surface/xash_surface.h"

#include <gtest/gtest.h>
#include <vgui/ISurface.h>

#include <string>
#include <vector>

namespace
{

    struct InputLog
    {
        std::vector<std::string> events;
    };

    void CursorMoved(int x, int y, void* user)
    {
        static_cast<InputLog*>(user)->events.push_back("move " + std::to_string(x) + " " + std::to_string(y));
    }

    void MousePressed(int code, void* user)
    {
        static_cast<InputLog*>(user)->events.push_back("press " + std::to_string(code));
    }

    void MouseReleased(int code, void* user)
    {
        static_cast<InputLog*>(user)->events.push_back("release " + std::to_string(code));
    }

    void Dispatch(void* user)
    {
        static_cast<InputLog*>(user)->events.push_back("dispatch");
    }

    TEST(SurfaceInput, StoredInputDoesNotDispatchWithoutTheTable)
    {
        csr_surface_t* surface = csr_surface_create(nullptr);
        vgui2::ISurface* vgui = static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));
        csr_surface_input_move(surface, 239, 201);
        csr_surface_input_button(surface, 0, 1);
        int x = 0;
        int y = 0;
        vgui->SurfaceGetCursorPos(x, y);
        EXPECT_EQ(x, 239);
        EXPECT_EQ(y, 201);
        vgui->RunFrame();
        csr_surface_destroy(surface);
    }

    TEST(SurfaceInput, MovePressAndReleaseEachDispatchOnce)
    {
        InputLog log;
        csr_surface_input_fns_t fns{};
        fns.cursor_moved = CursorMoved;
        fns.mouse_pressed = MousePressed;
        fns.mouse_released = MouseReleased;
        fns.dispatch = Dispatch;
        fns.user = &log;

        csr_surface_t* surface = csr_surface_create(nullptr);
        csr_surface_set_input(surface, &fns);
        vgui2::ISurface* vgui = static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));

        csr_surface_input_move(surface, 239, 201);
        csr_surface_input_button(surface, 0, 1);
        csr_surface_input_button(surface, 0, 0);
        vgui->RunFrame();
        vgui->SurfaceSetCursorPos(4, 5);

        ASSERT_EQ(log.events.size(), 6u);
        EXPECT_EQ(log.events[0], "move 239 201");
        EXPECT_EQ(log.events[1], "dispatch");
        EXPECT_EQ(log.events[2], "press 0");
        EXPECT_EQ(log.events[3], "dispatch");
        EXPECT_EQ(log.events[4], "release 0");
        EXPECT_EQ(log.events[5], "dispatch");

        int x = 0;
        int y = 0;
        vgui->SurfaceGetCursorPos(x, y);
        EXPECT_EQ(x, 4);
        EXPECT_EQ(y, 5);
        csr_surface_destroy(surface);
    }

} // namespace
