#pragma once

#include <goldsrc/client_api.h>
#include <cstdint>
#include <string>
#include <vector>

namespace csr::client
{
    // Read-only native GameUI video data, not the newer SDK's IGameUIFuncs.
    // The verified client module and engine table are borrowed until Shutdown.
    // No SDL initialization, window creation, renderer change or library load.
    class GameUiVideo
    {
    public:
        struct Mode { int wide; int tall; int bpp; };
        GameUiVideo() = default;
        GameUiVideo(const GameUiVideo&) = delete;
        GameUiVideo& operator=(const GameUiVideo&) = delete;
        bool Bind(void* client, goldsrc::cl_enginefunc_t* engine, std::string& error);
        void Shutdown();
        bool IsBound() const { return engine_ != nullptr; }
        bool GetVideoModes(const Mode*& modes, int& count) const;
        bool GetCurrentVideoMode(Mode& mode) const;
        // Five explicit arguments match the original renderer data contract.
        // This backend reports missing data as false; no native vtable is exposed.
        bool GetCurrentRenderer(char* name, int capacity, int* windowed, int* hd, int* addons) const;

    private:
        // Public SDL2 C ABI layout; kept separate from any VGUI SDK interface.
        struct DisplayMode { uint32_t format; int wide; int tall; int refresh; void* driver; };
        struct Api
        {
            void* (*window)(){};
            int (*display)(void*){};
            uint32_t (*format)(void*){};
            uint32_t (*flags)(void*){};
            int (*count)(int){};
            int (*mode)(int, int, DisplayMode*){};
            int (*masks)(uint32_t, int*, uint32_t*, uint32_t*, uint32_t*, uint32_t*){};
        } api_;
        static bool Bits(const Api& api, uint32_t format, int& bpp);
        static bool Current(const Api& api, goldsrc::cl_enginefunc_t* engine, Mode& mode);
        static bool Renderer(const Api& api, goldsrc::cl_enginefunc_t* engine,
                             char* name, int capacity, int* windowed, int* hd, int* addons);
        goldsrc::cl_enginefunc_t* engine_{};
        std::vector<Mode> modes_;
    };
}
