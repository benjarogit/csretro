#include "xash_surface/xash_surface.h"

#include <gtest/gtest.h>
#include <vgui/ISurface.h>

#include <cstring>

namespace
{

    int ReadTga(const char* path, unsigned char* dest, int dest_cap, void* user)
    {
        if (path == nullptr || std::strcmp(path, "cursor.tga") != 0 || user == nullptr)
        {
            return -1;
        }
        if (dest == nullptr || dest_cap == 0)
        {
            return 24;
        }
        if (dest_cap < 24)
        {
            return -1;
        }
        std::memcpy(dest, user, 24);
        return 24;
    }

} // namespace

TEST(SurfaceFile, MissingReaderDoesNotInventATexture)
{
    csr_surface_t* surface = csr_surface_create(nullptr);
    auto* vgui = static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));
    ASSERT_NE(vgui, nullptr);

    vgui->DrawSetTextureFile(1, "cursor.tga", 0, false);
    int wide = 3;
    int tall = 3;
    vgui->DrawGetTextureSize(1, wide, tall);
    EXPECT_EQ(wide, 0);
    EXPECT_EQ(tall, 0);

    csr_surface_destroy(surface);
}

TEST(SurfaceFile, UncompressedTgaBecomesATexture)
{
    unsigned char tga[24] = {};
    tga[2] = 2;
    tga[12] = 2;
    tga[14] = 1;
    tga[16] = 24;
    tga[18] = 0;
    tga[19] = 0;
    tga[20] = 255;
    tga[21] = 255;
    tga[22] = 0;
    tga[23] = 0;

    csr_surface_t* surface = csr_surface_create(nullptr);
    auto* vgui = static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));
    ASSERT_NE(vgui, nullptr);

    csr_surface_file_fns_t file{};
    file.read = ReadTga;
    file.user = tga;
    csr_surface_set_file(surface, &file);

    vgui->DrawSetTextureFile(1, "cursor.tga", 0, false);
    int wide = 0;
    int tall = 0;
    vgui->DrawGetTextureSize(1, wide, tall);
    EXPECT_EQ(wide, 2);
    EXPECT_EQ(tall, 1);

    tga[2] = 10;
    vgui->DrawSetTextureFile(2, "cursor.tga", 0, false);
    wide = 4;
    tall = 4;
    vgui->DrawGetTextureSize(2, wide, tall);
    EXPECT_EQ(wide, 0);
    EXPECT_EQ(tall, 0);

    csr_surface_destroy(surface);
}
