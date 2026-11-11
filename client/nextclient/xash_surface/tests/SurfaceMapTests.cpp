#include "surface_map.h"

#include <gtest/gtest.h>

namespace
{

    TEST(SurfaceMap, AlphaZeroIsOpaque)
    {
        EXPECT_EQ(xash::VguiAlphaToGl(0), 255);
        EXPECT_EQ(xash::VguiAlphaToGl(255), 0);
        EXPECT_EQ(xash::VguiAlphaToGl(128), 127);
        EXPECT_EQ(xash::VguiAlphaToGl(-4), 255);
        EXPECT_EQ(xash::VguiAlphaToGl(300), 0);
    }

    TEST(SurfaceMap, OffsetMovesTheRect)
    {
        xash::SurfaceFrame frame;
        frame.origin_x = 10;
        frame.origin_y = 6;
        xash::MappedQuad quad;
        ASSERT_TRUE(xash::MapQuad(frame, 0, 0, 4, 2, &quad));
        EXPECT_FLOAT_EQ(quad.x0, 10.0f);
        EXPECT_FLOAT_EQ(quad.y0, 6.0f);
        EXPECT_FLOAT_EQ(quad.x1, 14.0f);
        EXPECT_FLOAT_EQ(quad.y1, 8.0f);
        EXPECT_FLOAT_EQ(quad.u0, 0.0f);
        EXPECT_FLOAT_EQ(quad.u1, 1.0f);
    }

    TEST(SurfaceMap, ClipShrinksUvs)
    {
        xash::SurfaceFrame frame;
        frame.clip = true;
        frame.clip_x0 = 1;
        frame.clip_y0 = 1;
        frame.clip_x1 = 3;
        frame.clip_y1 = 3;
        xash::MappedQuad quad;
        ASSERT_TRUE(xash::MapQuad(frame, 0, 0, 4, 4, &quad));
        EXPECT_FLOAT_EQ(quad.x0, 1.0f);
        EXPECT_FLOAT_EQ(quad.y0, 1.0f);
        EXPECT_FLOAT_EQ(quad.x1, 3.0f);
        EXPECT_FLOAT_EQ(quad.y1, 3.0f);
        EXPECT_FLOAT_EQ(quad.u0, 0.25f);
        EXPECT_FLOAT_EQ(quad.v0, 0.25f);
        EXPECT_FLOAT_EQ(quad.u1, 0.75f);
        EXPECT_FLOAT_EQ(quad.v1, 0.75f);
    }

    TEST(SurfaceMap, ClipRejectsAnEmptyIntersection)
    {
        xash::SurfaceFrame frame;
        frame.origin_x = 10;
        frame.clip = true;
        frame.clip_x0 = 0;
        frame.clip_y0 = 0;
        frame.clip_x1 = 4;
        frame.clip_y1 = 4;
        xash::MappedQuad quad;
        EXPECT_FALSE(xash::MapQuad(frame, 0, 0, 4, 4, &quad));
    }

    TEST(SurfaceMap, SwappedCornersKeepTheTextureUpright)
    {
        xash::SurfaceFrame frame;
        xash::MappedQuad quad;
        ASSERT_TRUE(xash::MapQuad(frame, 4, 2, 0, 0, &quad));
        EXPECT_FLOAT_EQ(quad.x0, 0.0f);
        EXPECT_FLOAT_EQ(quad.y0, 0.0f);
        EXPECT_FLOAT_EQ(quad.x1, 4.0f);
        EXPECT_FLOAT_EQ(quad.y1, 2.0f);
        EXPECT_FLOAT_EQ(quad.u0, 1.0f);
        EXPECT_FLOAT_EQ(quad.u1, 0.0f);
        EXPECT_FLOAT_EQ(quad.v0, 1.0f);
        EXPECT_FLOAT_EQ(quad.v1, 0.0f);
    }

} // namespace
