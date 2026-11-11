#include "xash_surface/xash_surface.h"

#include <GL/gl.h>
#include <gtest/gtest.h>
#include <vgui/ISurface.h>

#include <cstdlib>
#include <string>
#include <vector>

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace
{

#if defined(__linux__)
    struct EglProbe
    {
        using EglBoolean = unsigned int;
        using EglEnum = unsigned int;
        using EglDisplay = void*;
        using EglConfig = void*;
        using EglSurface = void*;
        using EglContext = void*;
        using EglProc = void (*)();
        using GetProc = EglProc (*)(const char*);
        using GetPlatformDisplay = EglDisplay (*)(EglEnum, void*, const int*);
        using Initialize = EglBoolean (*)(EglDisplay, int*, int*);
        using BindApi = EglBoolean (*)(EglEnum);
        using ChooseConfig = EglBoolean (*)(EglDisplay, const int*, EglConfig*, int, int*);
        using CreateSurface = EglSurface (*)(EglDisplay, EglConfig, const int*);
        using CreateContext = EglContext (*)(EglDisplay, EglConfig, EglContext, const int*);
        using MakeCurrent = EglBoolean (*)(EglDisplay, EglSurface, EglSurface, EglContext);
        using DestroyContext = EglBoolean (*)(EglDisplay, EglContext);
        using DestroySurface = EglBoolean (*)(EglDisplay, EglSurface);
        using Terminate = EglBoolean (*)(EglDisplay);
        using GetError = int (*)();

        void* library{};
        EglDisplay display{};
        EglSurface surface{};
        EglContext context{};
        MakeCurrent make_current{};
        DestroyContext destroy_context{};
        DestroySurface destroy_surface{};
        Terminate terminate{};
        bool initialized{};

        bool Open(std::string& error, int wide, int tall)
        {
            setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
            library = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
            if (library == nullptr)
            {
                error = "libEGL.so.1 is not available";
                return false;
            }
            auto symbol = [&](const char* name) -> void* { return dlsym(library, name); };
            auto get_proc = reinterpret_cast<GetProc>(symbol("eglGetProcAddress"));
            auto initialize = reinterpret_cast<Initialize>(symbol("eglInitialize"));
            auto bind_api = reinterpret_cast<BindApi>(symbol("eglBindAPI"));
            auto choose_config = reinterpret_cast<ChooseConfig>(symbol("eglChooseConfig"));
            auto create_surface = reinterpret_cast<CreateSurface>(symbol("eglCreatePbufferSurface"));
            auto create_context = reinterpret_cast<CreateContext>(symbol("eglCreateContext"));
            make_current = reinterpret_cast<MakeCurrent>(symbol("eglMakeCurrent"));
            destroy_context = reinterpret_cast<DestroyContext>(symbol("eglDestroyContext"));
            destroy_surface = reinterpret_cast<DestroySurface>(symbol("eglDestroySurface"));
            terminate = reinterpret_cast<Terminate>(symbol("eglTerminate"));
            auto get_error = reinterpret_cast<GetError>(symbol("eglGetError"));
            if (get_proc == nullptr || initialize == nullptr || make_current == nullptr || get_error == nullptr)
            {
                error = "libEGL.so.1 is missing the pbuffer entry points";
                return false;
            }
            auto get_display = reinterpret_cast<GetPlatformDisplay>(get_proc("eglGetPlatformDisplayEXT"));
            if (get_display == nullptr)
            {
                error = "EGL has no surfaceless platform display";
                return false;
            }
            constexpr EglEnum kSurfaceless = 0x31DD;
            constexpr EglEnum kOpenGlApi = 0x30A2;
            constexpr int kNone = 0x3038;
            constexpr int kSurfaceType = 0x3033;
            constexpr int kPbufferBit = 0x0001;
            constexpr int kRenderable = 0x3040;
            constexpr int kOpenGlBit = 0x0008;
            constexpr int kRed = 0x3024;
            constexpr int kGreen = 0x3023;
            constexpr int kBlue = 0x3022;
            constexpr int kAlpha = 0x3021;
            constexpr int kWidth = 0x3057;
            constexpr int kHeight = 0x3056;
            constexpr int kMajor = 0x3098;
            constexpr int kMinor = 0x30FB;
            display = get_display(kSurfaceless, nullptr, nullptr);
            if (display == nullptr || initialize(display, nullptr, nullptr) == 0)
            {
                error = "eglInitialize failed, EGL error " + std::to_string(get_error());
                return false;
            }
            initialized = true;
            if (bind_api(kOpenGlApi) == 0)
            {
                error = "eglBindAPI(OpenGL) failed";
                return false;
            }
            const int config_attributes[] = {
                kSurfaceType, kPbufferBit, kRenderable, kOpenGlBit, kRed, 8, kGreen, 8, kBlue, 8, kAlpha, 8, kNone
            };
            EglConfig config = nullptr;
            int count = 0;
            if (choose_config(display, config_attributes, &config, 1, &count) == 0 || count == 0)
            {
                error = "EGL has no RGBA8 pbuffer config";
                return false;
            }
            const int surface_attributes[] = {kWidth, wide, kHeight, tall, kNone};
            surface = create_surface(display, config, surface_attributes);
            const int context_attributes[] = {kMajor, 2, kMinor, 1, kNone};
            context = create_context(display, config, nullptr, context_attributes);
            if (surface == nullptr || context == nullptr || make_current(display, surface, surface, context) == 0)
            {
                error = "EGL pbuffer context failed, EGL error " + std::to_string(get_error());
                return false;
            }
            return true;
        }

        ~EglProbe()
        {
            if (make_current != nullptr && display != nullptr)
            {
                make_current(display, nullptr, nullptr, nullptr);
            }
            if (context != nullptr)
            {
                destroy_context(display, context);
            }
            if (surface != nullptr)
            {
                destroy_surface(display, surface);
            }
            if (initialized)
            {
                terminate(display);
            }
            if (library != nullptr)
            {
                dlclose(library);
            }
        }
    };
#endif

    struct Panel
    {
        int x;
        int y;
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

    void ClipRect(void* panel, int* x0, int* y0, int* x1, int* y1, void*)
    {
        const auto* value = static_cast<const Panel*>(panel);
        *x0 = value->clip_x0;
        *y0 = value->clip_y0;
        *x1 = value->clip_x1;
        *y1 = value->clip_y1;
    }

    const unsigned char* Pixel(const std::vector<unsigned char>& pixels, int wide, int tall, int x, int y)
    {
        const int row = tall - 1 - y;
        return pixels.data() + (static_cast<size_t>(row) * static_cast<size_t>(wide) + static_cast<size_t>(x)) * 4;
    }

} // namespace

TEST(SurfaceGl, FillTextAndTextureLandAndRestoreState)
{
#if !defined(__linux__)
    GTEST_SKIP() << "the pbuffer probe is Linux-only";
#else
    constexpr int kWide = 64;
    constexpr int kTall = 64;
    EglProbe egl;
    std::string error;
    ASSERT_TRUE(egl.Open(error, kWide, kTall)) << error;

    Panel panel{};
    panel.x = 8;
    panel.y = 4;
    panel.clip_x0 = 0;
    panel.clip_y0 = 0;
    panel.clip_x1 = kWide;
    panel.clip_y1 = kTall;
    csr_surface_host_t host{};
    host.panel_abs_pos = AbsPos;
    host.panel_clip_rect = ClipRect;
    csr_surface_t* surface = csr_surface_create(&host);
    auto* vgui = static_cast<vgui2::ISurface*>(csr_surface_vgui(surface));

    glDisable(GL_TEXTURE_2D);
    glColor4ub(9, 8, 7, 6);
    const int marker_mode = GL_MODELVIEW;
    glMatrixMode(marker_mode);
    int viewport_before[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport_before);

    glClearColor(0.0f, 0.0f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    csr_surface_begin_frame(surface, kWide, kTall);

    vgui->DrawSetColor(255, 0, 0, 0);
    vgui->DrawFilledRect(1, 1, 5, 5);

    vgui->DrawSetColor(0, 255, 0, 255);
    vgui->DrawFilledRect(20, 20, 24, 24);

    const unsigned char rgba[16] = {255, 255, 0, 255, 0, 0, 255, 255, 0, 0, 255, 255, 255, 255, 0, 255};
    const int texture = vgui->CreateNewTextureID(false);
    vgui->DrawSetTextureRGBA(texture, rgba, 2, 2, 0, true);
    vgui->DrawSetTexture(texture);
    vgui->DrawSetColor(255, 255, 255, 0);
    vgui->DrawTexturedRect(30, 2, 34, 6);

    const auto key = static_cast<vgui2::VPANEL>(reinterpret_cast<uintptr_t>(&panel));
    vgui->PushMakeCurrent(key, false);
    vgui->DrawSetColor(255, 128, 0, 0);
    vgui->DrawFilledRect(0, 0, 3, 3);
    vgui->PopMakeCurrent(key);

    panel.x = 40;
    panel.y = 40;
    panel.clip_x0 = 40;
    panel.clip_y0 = 40;
    panel.clip_x1 = 42;
    panel.clip_y1 = 42;
    vgui->PushMakeCurrent(key, false);
    vgui->DrawSetColor(255, 255, 255, 0);
    vgui->DrawFilledRect(0, 0, 8, 8);
    vgui->PopMakeCurrent(key);

    const vgui2::HFont font = vgui->CreateFont();
    ASSERT_TRUE(vgui->AddGlyphSetToFont(font, "Tahoma", 20, 0, 0, 0, 0, 0, 0));
    vgui->DrawSetTextFont(font);
    vgui->DrawSetTextColor(0, 255, 0, 0);
    vgui->DrawSetTextPos(0, 40);
    vgui->DrawPrintText(L"A", 1);
    const int letter_wide = vgui->GetCharacterWidth(font, L'A');
    const int letter_tall = vgui->GetFontTall(font);

    csr_surface_end_frame(surface);
    EXPECT_EQ(csr_surface_gl_error(surface), 0);

    std::vector<unsigned char> pixels(static_cast<size_t>(kWide) * static_cast<size_t>(kTall) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, kWide, kTall, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));

    const unsigned char* opaque = Pixel(pixels, kWide, kTall, 2, 2);
    EXPECT_EQ(opaque[0], 255);
    EXPECT_EQ(opaque[1], 0);
    EXPECT_EQ(opaque[2], 0);

    const unsigned char* clear = Pixel(pixels, kWide, kTall, 21, 21);
    EXPECT_LT(clear[0], 20);
    EXPECT_GT(clear[2], 20);

    const unsigned char* texel = Pixel(pixels, kWide, kTall, 30, 2);
    EXPECT_EQ(texel[0], 255);
    EXPECT_EQ(texel[1], 255);
    EXPECT_EQ(texel[2], 0);

    const unsigned char* moved = Pixel(pixels, kWide, kTall, 8, 4);
    EXPECT_EQ(moved[0], 255);
    EXPECT_EQ(moved[1], 128);
    EXPECT_EQ(moved[2], 0);

    const unsigned char* clipped = Pixel(pixels, kWide, kTall, 44, 44);
    EXPECT_LT(clipped[0], 20);
    EXPECT_GT(clipped[2], 20);
    const unsigned char* kept = Pixel(pixels, kWide, kTall, 40, 40);
    EXPECT_EQ(kept[0], 255);
    EXPECT_EQ(kept[1], 255);
    EXPECT_EQ(kept[2], 255);

    int ink = 0;
    int box = 0;
    for (int y = 40; y < 40 + letter_tall && y < kTall; ++y)
    {
        for (int x = 0; x < letter_wide && x < kWide; ++x)
        {
            const unsigned char* sample = Pixel(pixels, kWide, kTall, x, y);
            ++box;
            if (sample[1] > sample[0] + 40 && sample[1] > 40)
            {
                ++ink;
            }
        }
    }
    EXPECT_GT(box, 0);
    EXPECT_GT(ink, 8);
    EXPECT_LT(ink, box);

    float color[4] = {};
    glGetFloatv(GL_CURRENT_COLOR, color);
    EXPECT_NEAR(color[0], 9.0f / 255.0f, 0.02f);
    EXPECT_NEAR(color[1], 8.0f / 255.0f, 0.02f);
    EXPECT_NEAR(color[2], 7.0f / 255.0f, 0.02f);
    EXPECT_FALSE(glIsEnabled(GL_TEXTURE_2D));
    int mode = 0;
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    EXPECT_EQ(mode, marker_mode);
    int viewport_after[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport_after);
    EXPECT_EQ(viewport_after[0], viewport_before[0]);
    EXPECT_EQ(viewport_after[1], viewport_before[1]);
    EXPECT_EQ(viewport_after[2], viewport_before[2]);
    EXPECT_EQ(viewport_after[3], viewport_before[3]);

    csr_surface_destroy(surface);
#endif
}
