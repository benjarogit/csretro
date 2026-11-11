#include "font_atlas.h"

#include <gtest/gtest.h>

namespace
{

    TEST(FontAtlas, ShelfWrapsWhenTheRowIsFull)
    {
        xash::ShelfPage page(16);
        int x = -1;
        int y = -1;
        ASSERT_TRUE(page.Place(10, 4, &x, &y));
        EXPECT_EQ(x, 0);
        EXPECT_EQ(y, 0);
        ASSERT_TRUE(page.Place(10, 6, &x, &y));
        EXPECT_EQ(x, 0);
        EXPECT_EQ(y, 4);
        EXPECT_FALSE(page.Place(16, 8, &x, &y));
    }

    TEST(FontAtlas, PackKeepsCoverageInsideAPaddedCell)
    {
        xash::GlyphAtlas atlas;
        const unsigned char alpha[4] = {0, 255, 128, 64};
        xash::AtlasGlyph glyph;
        ASSERT_TRUE(atlas.Pack(2, 2, alpha, &glyph));
        EXPECT_TRUE(glyph.placed);
        EXPECT_EQ(glyph.page, 0);
        EXPECT_NEAR(glyph.u0, 1.0f / 1024.0f, 0.0001f);
        EXPECT_NEAR(glyph.v1, 3.0f / 1024.0f, 0.0001f);

        const unsigned char* pixels = atlas.page_pixels(0);
        ASSERT_NE(pixels, nullptr);
        const unsigned char* first = pixels + (static_cast<size_t>(1) * 1024 + 1) * 4;
        EXPECT_EQ(first[0], 255);
        EXPECT_EQ(first[3], 0);
        const unsigned char* second = pixels + (static_cast<size_t>(1) * 1024 + 2) * 4;
        EXPECT_EQ(second[3], 255);
        EXPECT_TRUE(atlas.page_dirty(0));
    }

} // namespace
