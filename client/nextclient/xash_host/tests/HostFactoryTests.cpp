#include "xash_host/xash_host.h"

#include "xash_engine/xash_engine.h"

#include <gtest/gtest.h>
#include <vgui/ISurface.h>

#include <string>
#include <vector>

namespace
{

    int g_extra_calls = 0;

    void* ExtraFactory(const char* name, int* code)
    {
        ++g_extra_calls;
        if (code != nullptr)
        {
            *code = 0;
        }
        return const_cast<char*>(name);
    }

    struct InputLog
    {
        std::vector<std::string> events;
    };

    void CursorMoved(void* input, int x, int y)
    {
        static_cast<InputLog*>(input)->events.push_back("move " + std::to_string(x) + " " + std::to_string(y));
    }

    void MousePressed(void* input, int code)
    {
        static_cast<InputLog*>(input)->events.push_back("press " + std::to_string(code));
    }

    void MouseReleased(void* input, int code)
    {
        static_cast<InputLog*>(input)->events.push_back("release " + std::to_string(code));
    }

    void Dispatch(void* vgui)
    {
        static_cast<InputLog*>(vgui)->events.push_back("dispatch");
    }

    class HostFactory : public testing::Test
    {
    protected:
        void TearDown() override
        {
            csr_host_close();
        }
    };

} // namespace

TEST_F(HostFactory, AMissingSurfacePublishesNothing)
{
    EXPECT_EQ(csr_host_open("/no/such/xash_surface.so"), 0);
    csr_host_factories_t factories{};
    EXPECT_EQ(csr_host_factory_list(nullptr, 0, &factories), 0);
    EXPECT_EQ(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);
}

TEST_F(HostFactory, SurfaceFactoryIsFirstAndExtrasAreNotAskedForIt)
{
    g_extra_calls = 0;
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);

    csr_host_factory_fn extra = ExtraFactory;
    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(&extra, 1, &factories), 1);
    ASSERT_EQ(factories.count, 2);
    ASSERT_NE(factories.list, nullptr);

    int code = -1;
    void* surface = factories.list[0]("VGUI_Surface026", &code);
    EXPECT_NE(surface, nullptr);
    EXPECT_EQ(code, 0);
    EXPECT_EQ(factories.list[0]("FileSystem", &code), nullptr);
    EXPECT_EQ(g_extra_calls, 0);

    g_extra_calls = 0;
    EXPECT_EQ(csr_host_create_interface("VGUI_Surface026", nullptr), surface);
    EXPECT_EQ(csr_xash_engine_surface()->object, surface);
    csr_xash_engine_set_text_color(1, 2, 3);
    csr_xash_engine_reset_texture();
    csr_xash_engine_draw_init();
    EXPECT_EQ(g_extra_calls, 0);
    void* filesystem = csr_host_create_interface("FileSystem", nullptr);
    ASSERT_NE(filesystem, nullptr);
    EXPECT_STREQ(static_cast<const char*>(filesystem), "FileSystem");
    EXPECT_EQ(g_extra_calls, 1);
}

TEST_F(HostFactory, MouseEventsDispatchOnceAndCursorWarpsDoNot)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    InputLog log;
    csr_host_vgui_t vgui{};
    vgui.input = &log;
    vgui.vgui = &log;
    vgui.cursor_moved = CursorMoved;
    vgui.mouse_pressed = MousePressed;
    vgui.mouse_released = MouseReleased;
    vgui.dispatch = Dispatch;
    ASSERT_EQ(csr_host_bind_input(&vgui), 1);

    csr_host_input_move(239, 201);
    csr_host_input_button(0, 1);
    csr_host_input_button(0, 0);

    int code = 0;
    auto* surface = static_cast<vgui2::ISurface*>(csr_host_create_interface("VGUI_Surface026", &code));
    ASSERT_NE(surface, nullptr);
    surface->SurfaceSetCursorPos(4, 5);

    ASSERT_EQ(log.events.size(), 6u);
    EXPECT_EQ(log.events[0], "move 239 201");
    EXPECT_EQ(log.events[1], "dispatch");
    EXPECT_EQ(log.events[2], "press 0");
    EXPECT_EQ(log.events[3], "dispatch");
    EXPECT_EQ(log.events[4], "release 0");
    EXPECT_EQ(log.events[5], "dispatch");
    int x = 0;
    int y = 0;
    surface->SurfaceGetCursorPos(x, y);
    EXPECT_EQ(x, 4);
    EXPECT_EQ(y, 5);
}

TEST_F(HostFactory, AMissingPairLibraryLeavesTheSurfaceFactory)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    EXPECT_EQ(csr_host_load_pair("/no/such/client_mini.dll", XASH_VGUI_FILE), 0);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_vgui_dispatch(), nullptr);
    EXPECT_EQ(csr_host_scheme(), nullptr);
    EXPECT_NE(csr_host_create_interface("VGUI_Surface026", nullptr), nullptr);

    EXPECT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, "/no/such/vgui2.so"), 0);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_scheme(), nullptr);

    EXPECT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_SURFACE_FILE), 0);
    EXPECT_EQ(csr_host_vgui_dispatch(), nullptr);
}

TEST_F(HostFactory, ThePairPublishesTheSurfaceAndTheOneScheme)
{
    ASSERT_EQ(csr_host_open(XASH_SURFACE_FILE), 1);
    ASSERT_EQ(csr_host_load_pair(XASH_CLIENT_FILE, XASH_VGUI_FILE), 1);
    EXPECT_EQ(csr_host_pair_loaded(), 1);

    void* surface = csr_host_create_interface("VGUI_Surface026", nullptr);
    void* scheme = csr_host_scheme();
    ASSERT_NE(surface, nullptr);
    ASSERT_NE(scheme, nullptr);
    EXPECT_NE(scheme, surface);
    ASSERT_NE(csr_host_vgui_dispatch(), nullptr);

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    EXPECT_EQ(factories.count, 1);
    EXPECT_EQ(factories.list[0]("VGUI_Surface026", nullptr), surface);
}
