#include "xash_surface/xash_surface.h"

#include <Color.h>
#include <gtest/gtest.h>
#include <vgui/ISurface.h>

extern "C" IBaseInterface* CreateInterface(const char* name, int* code);

namespace
{

    struct Panel
    {
        int x;
        int y;
        int left;
        int top;
        int clip_x0;
        int clip_y0;
        int clip_x1;
        int clip_y1;
    };

    void AbsPos(void* panel, int* x, int* y, void*)
    {
        const auto* value = static_cast<const Panel*>(panel);
        *x = value->x;
        *y = value->y;
    }

    void Insets(void* panel, int* left, int* top, int*, int*, void*)
    {
        const auto* value = static_cast<const Panel*>(panel);
        *left = value->left;
        *top = value->top;
    }

    vgui2::ISurface* AsSurface(csr_surface_t* surface)
    {
        return static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));
    }

    TEST(SurfaceApi, FactoryExposesOnlyTheLiveSurface)
    {
        int code = 0;
        EXPECT_EQ(CreateInterface("VGUI_Surface026", &code), nullptr);
        EXPECT_EQ(code, IFACE_FAILED);

        csr_surface_t* surface = csr_surface_create(nullptr);
        IBaseInterface* found = CreateInterface("VGUI_Surface026", &code);
        EXPECT_EQ(code, IFACE_OK);
        EXPECT_EQ(found, csr_surface_vgui(surface));
        EXPECT_EQ(CreateInterface("VGUI_Surface030", &code), nullptr);
        EXPECT_EQ(code, IFACE_FAILED);

        csr_surface_destroy(surface);
        EXPECT_EQ(CreateInterface("VGUI_Surface026", &code), nullptr);
        EXPECT_EQ(code, IFACE_FAILED);
    }

    TEST(SurfaceApi, ScreenCursorAndFontMetricsDoNotNeedGl)
    {
        csr_surface_host_t host{};
        host.screen_wide = 800;
        host.screen_tall = 600;
        csr_surface_t* surface = csr_surface_create(&host);
        vgui2::ISurface* vgui = AsSurface(surface);

        int wide = 0;
        int tall = 0;
        vgui->GetScreenSize(wide, tall);
        EXPECT_EQ(wide, 800);
        EXPECT_EQ(tall, 600);
        csr_surface_begin_frame(surface, 1024, 768);
        vgui->GetScreenSize(wide, tall);
        EXPECT_EQ(wide, 1024);
        EXPECT_EQ(tall, 768);

        vgui->GetProportionalBase(wide, tall);
        EXPECT_EQ(wide, 640);
        EXPECT_EQ(tall, 480);

        csr_surface_input_move(surface, 239, 201);
        csr_surface_input_button(surface, 0, 1);
        int x = 0;
        int y = 0;
        vgui->SurfaceGetCursorPos(x, y);
        EXPECT_EQ(x, 239);
        EXPECT_EQ(y, 201);
        EXPECT_TRUE(vgui->HasCursorPosFunctions());
        EXPECT_TRUE(vgui->IsWithin(10, 10));
        EXPECT_FALSE(vgui->IsWithin(-1, 10));
        EXPECT_FALSE(vgui->IsWithin(1024, 10));

        const vgui2::HFont font = vgui->CreateFont();
        EXPECT_NE(font, 0u);
        EXPECT_TRUE(vgui->AddGlyphSetToFont(font, "Tahoma", 26, 800, 0, 0, 0, 0, 0));
        EXPECT_TRUE(vgui->AddGlyphSetToFont(font, "Arial", 40, 700, 0, 0, 0, 0, 0));
        EXPECT_EQ(vgui->GetFontTall(font), 26);
        EXPECT_EQ(vgui->GetCharacterWidth(font, L'A'), 13);
        int a = -1;
        int b = -1;
        int c = -1;
        vgui->GetCharABCwide(font, L'A', a, b, c);
        EXPECT_EQ(a, 0);
        EXPECT_EQ(b, 13);
        EXPECT_EQ(c, 0);
        vgui->GetTextSize(font, L"AB", wide, tall);
        EXPECT_EQ(wide, 26);
        EXPECT_EQ(tall, 26);
        EXPECT_EQ(vgui->GetFontAscent(font, L'A'), 19);

        vgui->DrawSetTextFont(font);
        vgui->DrawSetTextPos(4, 5);
        vgui->DrawGetTextPos(x, y);
        EXPECT_EQ(x, 4);
        EXPECT_EQ(y, 5);
        vgui->DrawPrintText(L"A", 1);
        vgui->DrawGetTextPos(x, y);
        EXPECT_EQ(x, 17);
        EXPECT_EQ(y, 5);

        Panel panel{96, 84, 2, 3, 90, 80, 200, 180};
        host.panel_abs_pos = AbsPos;
        host.panel_insets = Insets;
        // The host struct was copied at creation. Rebuild so the callbacks are live.
        csr_surface_destroy(surface);
        host.screen_wide = 1024;
        host.screen_tall = 768;
        surface = csr_surface_create(&host);
        vgui = AsSurface(surface);
        const auto key = static_cast<vgui2::VPANEL>(reinterpret_cast<uintptr_t>(&panel));
        vgui->PushMakeCurrent(key, true);
        vgui->PushMakeCurrent(key, false);
        vgui->PopMakeCurrent(key);
        vgui->PopMakeCurrent(key);
        vgui->PopMakeCurrent(key);

        csr_surface_end_frame(surface);
        EXPECT_EQ(csr_surface_gl_error(surface), 0);
        csr_surface_destroy(surface);
        csr_surface_destroy(nullptr);
    }

    TEST(SurfaceApi, MissingFontAndTextureStayEmpty)
    {
        csr_surface_t* surface = csr_surface_create(nullptr);
        vgui2::ISurface* vgui = AsSurface(surface);
        EXPECT_EQ(vgui->GetFontTall(1), 0);
        EXPECT_FALSE(vgui->AddGlyphSetToFont(1, "Tahoma", 12, 0, 0, 0, 0, 0, 0));
        int wide = 7;
        int tall = 7;
        vgui->DrawGetTextureSize(3, wide, tall);
        EXPECT_EQ(wide, 0);
        EXPECT_EQ(tall, 0);
        EXPECT_FALSE(vgui->IsTextureIDValid(3));
        EXPECT_EQ(vgui->CreateNewTextureID(false), 1);
        const unsigned char pixel[4] = {1, 2, 3, 4};
        vgui->DrawSetTextureRGBA(1, pixel, 1, 1, 0, true);
        vgui->DrawGetTextureSize(1, wide, tall);
        EXPECT_EQ(wide, 1);
        EXPECT_EQ(tall, 1);
        EXPECT_TRUE(vgui->IsTextureIDValid(1));
        EXPECT_TRUE(vgui->DeleteTextureByID(1));
        EXPECT_FALSE(vgui->IsTextureIDValid(1));
        csr_surface_destroy(surface);
    }

} // namespace
