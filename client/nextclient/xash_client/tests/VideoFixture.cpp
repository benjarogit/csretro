// Synthetic public engine/SDL2 data only; no SDL or game is initialized.
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <cstring>
#include <limits>

namespace
{
    // Independent SDL2 wire declaration, not the production private type.
    struct SdlMode { uint32_t format; int w; int h; int hz; void* driverdata; };
    int scenario{};
    int window_token{};
    char gl[] = "gl";
    char other[] = "other-renderer";
    char empty[] = "";
    char long_name[257]{};
    goldsrc::cvar_t renderer{"r_refdll_loaded", gl, 0, 0, nullptr};
    goldsrc::cvar_t hd{"fs_mount_hd", empty, 0, 1, nullptr};
    goldsrc::cvar_t addon{"fs_mount_addon", empty, 0, 0, nullptr};
    int Screen(goldsrc::SCREENINFO* screen)
    {
        if (!screen || screen->iSize != sizeof(*screen)) return 0;
        screen->iWidth = scenario == 10 ? 0 : scenario == 21 ? 800 : 576;
        screen->iHeight = scenario == 21 ? 450 : 324;
        return 1;
    }
    goldsrc::cvar_t* Cvar(const char* name)
    {
        if (std::strcmp(name, "r_refdll_loaded") == 0) return scenario == 6 ? nullptr : &renderer;
        if (std::strcmp(name, "fs_mount_hd") == 0) return scenario == 8 ? nullptr : &hd;
        if (std::strcmp(name, "fs_mount_addon") == 0) return scenario == 9 ? nullptr : &addon;
        return nullptr;
    }
    goldsrc::cl_enginefunc_t engine{};
}
extern "C" void FixtureVideoConfigure(int value)
{
    scenario = value;
    std::memset(long_name, 'x', 256);
    long_name[256] = '\0';
    renderer.string = value == 7 ? empty : value == 19 ? long_name : value == 21 ? other : gl;
    hd.value = value == 15 ? std::numeric_limits<float>::quiet_NaN() : value == 21 ? 0.0f : 1.0f;
    addon.value = value == 21 ? 1.0f : 0.0f;
    engine = {};
    engine.pfnGetScreenInfo = Screen;
    engine.pfnGetCvarPointer = Cvar;
}
extern "C" goldsrc::cl_enginefunc_t* FixtureVideoEngine() { return &engine; }
extern "C" void* SDL_GL_GetCurrentWindow() { return scenario == 1 ? nullptr : &window_token; }
extern "C" int SDL_GetWindowDisplayIndex(void* window)
{
    return window == &window_token && scenario != 2 ? 2 : -1;
}
extern "C" uint32_t SDL_GetWindowPixelFormat(void* window)
{
    return window != &window_token || scenario == 13 ? 0 : scenario == 21 ? 16 : 24;
}
extern "C" uint32_t SDL_GetWindowFlags(void* window)
{
    return window == &window_token && (scenario == 20 || scenario == 21) ? 0x1001u : 0;
}
extern "C" int SDL_GetNumDisplayModes(int display)
{
    if (display != 2 || scenario == 18) return -1;
    return scenario == 3 ? 0 : scenario == 11 ? 5000 : 3;
}
extern "C" int SDL_GetDisplayMode(int display, int index, SdlMode* mode)
{
    if (display != 2 || !mode || index < 0 || index >= 3 || (scenario == 4 && index == 1)) return -1;
    *mode = index == 2 ? SdlMode{16, 1280, 720, 60, nullptr} : SdlMode{32, 1920, 1080, index ? 144 : 60, nullptr};
    if (scenario == 12) mode->w = -1;
    if (scenario == 16 && index == 1) mode->format = 0;
    return 0;
}
#ifndef CSRETRO_FIXTURE_NO_MASKS
extern "C" int SDL_PixelFormatEnumToMasks(uint32_t format, int* bpp, uint32_t* r, uint32_t* g, uint32_t* b, uint32_t* a)
{
    if (!format || scenario == 5) return 0;
    *bpp = scenario == 17 ? 129 : static_cast<int>(format);
    *r = *g = *b = *a = 0;
    return 1;
}
#endif
