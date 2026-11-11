#include "xash_engine/xash_engine.h"

#include <gtest/gtest.h>

namespace
{

    int g_init_calls = 0;
    int g_reset_calls = 0;
    int g_color_r = -1;
    int g_color_g = -1;
    int g_color_b = -1;
    void* g_seen = nullptr;

    void OnInit(void* object)
    {
        ++g_init_calls;
        g_seen = object;
    }

    void OnReset(void* object)
    {
        ++g_reset_calls;
        g_seen = object;
    }

    void OnColor(void* object, int r, int g, int b)
    {
        g_seen = object;
        g_color_r = r;
        g_color_g = g;
        g_color_b = b;
    }

    class XashEngine : public testing::Test
    {
    protected:
        void SetUp() override
        {
            g_init_calls = 0;
            g_reset_calls = 0;
            g_color_r = -1;
            g_color_g = -1;
            g_color_b = -1;
            g_seen = nullptr;
            csr_xash_engine_clear();
        }
    };

} // namespace

TEST_F(XashEngine, MissingFunctionsAreNotCalled)
{
    int object = 0;
    csr_xash_engine_surface_t surface{};
    surface.object = &object;
    csr_xash_engine_bind(&surface);

    csr_xash_engine_draw_init();
    csr_xash_engine_reset_texture();
    csr_xash_engine_set_text_color(9, 8, 7);

    EXPECT_EQ(g_init_calls, 0);
    EXPECT_EQ(g_reset_calls, 0);
    EXPECT_EQ(g_color_r, -1);
    EXPECT_EQ(csr_xash_engine_surface()->object, &object);
}

TEST_F(XashEngine, BoundFunctionsReceiveTheSurface)
{
    int object = 0;
    csr_xash_engine_surface_t surface{};
    surface.object = &object;
    surface.draw_init = OnInit;
    surface.reset_texture = OnReset;
    surface.set_text_color = OnColor;
    csr_xash_engine_bind(&surface);

    csr_xash_engine_draw_init();
    csr_xash_engine_reset_texture();
    csr_xash_engine_set_text_color(4, 5, 6);

    EXPECT_EQ(g_init_calls, 1);
    EXPECT_EQ(g_reset_calls, 1);
    EXPECT_EQ(g_color_r, 4);
    EXPECT_EQ(g_color_g, 5);
    EXPECT_EQ(g_color_b, 6);
    EXPECT_EQ(g_seen, &object);
}

TEST_F(XashEngine, ANullObjectClearsTheBinding)
{
    int object = 0;
    csr_xash_engine_surface_t surface{};
    surface.object = &object;
    surface.set_text_color = OnColor;
    csr_xash_engine_bind(&surface);
    csr_xash_engine_bind(nullptr);

    csr_xash_engine_set_text_color(1, 2, 3);

    EXPECT_EQ(csr_xash_engine_surface()->object, nullptr);
    EXPECT_EQ(g_color_r, -1);
}
