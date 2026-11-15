#pragma once

#include "gameui_video.h"
#include "XashKeys.h"
#include <vector>

namespace csr::client
{
    // Original Linux GameUIFuncs v001 ABI, not the six-argument SDK variant.
    // An engine-owned object: callers borrow it and never delete it.
    class NativeGameUiFuncs
    {
    public:
        struct Mode
        {
            int wide;
            int tall;
            int bpp;
        };
        NativeGameUiFuncs(GameUiVideo& video, XashKeyQueries& keys) :
            video_(video),
            keys_(keys)
        {}
        virtual ~NativeGameUiFuncs() = default;
        virtual bool IsKeyDown(const char* action, bool& down);
        virtual const char* Key_NameForKey(int key);
        virtual const char* Key_BindingForKey(int key);
        virtual int GetVGUI2KeyCodeForBind(const char* command);
        virtual void GetVideoModes(Mode** modes, int* count);
        virtual void GetCurrentVideoMode(int* wide, int* tall, int* bpp);
        virtual void GetCurrentRenderer(char* name, int capacity, int* windowed, int* hd, int* addons);
        virtual bool IsConnectedToVACSecureServer();
        virtual int Key_KeyStringToKeyNum(const char* name);

        bool Bind(std::string& error);
        bool IsReady() const;
        void Shutdown();

    private:
        bool BackendsReady() const;
        GameUiVideo& video_;
        XashKeyQueries& keys_;
        std::vector<Mode> modes_;
        bool ready_{};
    };
} // namespace csr::client
