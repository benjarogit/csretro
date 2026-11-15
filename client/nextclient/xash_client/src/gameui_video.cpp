#include "gameui_video.h"

#include <goldsrc/cvar.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <dlfcn.h>

namespace csr::client
{
    bool GameUiVideo::Bits(const Api& api, uint32_t format, int& bpp)
    {
        uint32_t red{}, green{}, blue{}, alpha{};
        bpp = 0;
        return format && api.masks(format, &bpp, &red, &green, &blue, &alpha) && bpp > 0 && bpp <= 128;
    }

    bool GameUiVideo::Current(const Api& api, goldsrc::cl_enginefunc_t* engine, Mode& mode)
    {
        mode = {};
        if (!engine || !engine->pfnGetScreenInfo) return false;
        void* window = api.window();
        goldsrc::SCREENINFO screen{};
        screen.iSize = sizeof(screen);
        int bpp{};
        if (!window || !engine->pfnGetScreenInfo(&screen) || screen.iWidth <= 0 || screen.iHeight <= 0 ||
            !Bits(api, api.format(window), bpp)) return false;
        // Engine dimensions preserve render scaling; SDL supplies the real
        // window's format, not a made-up 32-bit default or desktop dimensions.
        mode = {screen.iWidth, screen.iHeight, bpp};
        return true;
    }

    bool GameUiVideo::Renderer(const Api& api, goldsrc::cl_enginefunc_t* engine,
                               char* name, int capacity, int* windowed, int* hd, int* addons)
    {
        if (name && capacity > 0) name[0] = '\0';
        if (windowed) *windowed = 0;
        if (hd) *hd = 0;
        if (addons) *addons = 0;
        if (!engine || !engine->pfnGetCvarPointer || capacity < 0 || (name && capacity == 0) || (!name && capacity != 0)) return false;
        void* window = api.window();
        auto renderer = engine->pfnGetCvarPointer("r_refdll_loaded");
        auto hd_models = engine->pfnGetCvarPointer("fs_mount_hd");
        auto addon_models = engine->pfnGetCvarPointer("fs_mount_addon");
        if (!window || !renderer || !renderer->string || !hd_models || !addon_models ||
            !std::isfinite(hd_models->value) || !std::isfinite(addon_models->value)) return false;
        const auto size = strnlen(renderer->string, 256);
        if (!size || size == 256) return false;
        if (name)
        {
            const auto copied = std::min(size, static_cast<size_t>(capacity - 1));
            std::memcpy(name, renderer->string, copied);
            name[copied] = '\0';
        }
        // SDL_WINDOW_FULLSCREEN is bit 0, including fullscreen-desktop. Query
        // the current window instead of guessing between Xash cvar profiles.
        if (windowed) *windowed = (api.flags(window) & 1u) ? 0 : 1;
        if (hd) *hd = hd_models->value != 0.0f;
        if (addons) *addons = addon_models->value != 0.0f;
        return true;
    }

    bool GameUiVideo::Bind(void* client, goldsrc::cl_enginefunc_t* engine, std::string& error)
    {
        error.clear();
        if (IsBound()) { error = "GameUI video services are already bound"; return false; }
        if (!client || !engine || !engine->pfnGetScreenInfo || !engine->pfnGetCvarPointer)
        {
            error = "GameUI video requires a loaded client and real screen/cvar callbacks";
            return false;
        }
        static_assert(offsetof(DisplayMode, driver) == 16);
        static_assert(sizeof(DisplayMode) == (sizeof(void*) == 8 ? 24 : 20));
        static_assert(sizeof(Mode) == 12);
        Api api;
#define LOAD(field, symbol) \
        api.field = reinterpret_cast<decltype(api.field)>(dlsym(client, #symbol)); \
        if (!api.field) { error = "GameUI video is missing " #symbol; return false; }
        LOAD(window, SDL_GL_GetCurrentWindow)
        LOAD(display, SDL_GetWindowDisplayIndex)
        LOAD(format, SDL_GetWindowPixelFormat)
        LOAD(flags, SDL_GetWindowFlags)
        LOAD(count, SDL_GetNumDisplayModes)
        LOAD(mode, SDL_GetDisplayMode)
        LOAD(masks, SDL_PixelFormatEnumToMasks)
#undef LOAD
        Mode current{};
        char renderer[256]{};
        if (!Current(api, engine, current) || !Renderer(api, engine, renderer, sizeof(renderer), nullptr, nullptr, nullptr))
        {
            error = "GameUI video requires a current GL window, valid pixel format and Xash renderer/mount cvars";
            return false;
        }
        const int display = api.display(api.window());
        const int count = display < 0 ? -1 : api.count(display);
        if (count <= 0 || count > 4096)
        {
            error = "GameUI video display mode inventory is unavailable or unbounded";
            return false;
        }
        std::vector<Mode> modes;
        modes.reserve(static_cast<size_t>(count));
        for (int index = 0; index < count; ++index)
        {
            DisplayMode source{};
            int bpp{};
            if (api.mode(display, index, &source) != 0 || source.wide <= 0 || source.tall <= 0 ||
                !Bits(api, source.format, bpp))
            {
                error = "GameUI video received an invalid display mode";
                return false;
            }
            if (std::none_of(modes.begin(), modes.end(), [&](const Mode& existing) {
                    return existing.wide == source.wide && existing.tall == source.tall && existing.bpp == bpp;
                })) modes.push_back({source.wide, source.tall, bpp});
        }
        api_ = api;
        engine_ = engine;
        modes_ = std::move(modes);
        return true;
    }

    void GameUiVideo::Shutdown()
    {
        engine_ = nullptr;
        api_ = {};
        modes_.clear();
    }

    bool GameUiVideo::GetVideoModes(const Mode*& modes, int& count) const
    {
        modes = nullptr;
        count = 0;
        if (!IsBound()) return false;
        modes = modes_.data();
        count = static_cast<int>(modes_.size());
        return true;
    }

    bool GameUiVideo::GetCurrentVideoMode(Mode& mode) const { return Current(api_, engine_, mode); }

    bool GameUiVideo::GetCurrentRenderer(char* name, int capacity, int* windowed, int* hd, int* addons) const
    {
        return Renderer(api_, engine_, name, capacity, windowed, hd, addons);
    }
}
