#include "native_gameui_funcs.h"

namespace csr::client
{
    bool NativeGameUiFuncs::BackendsReady() const
    {
        GameUiVideo::Mode mode{};
        return video_.IsBound() && keys_.IsBound() && video_.GetCurrentVideoMode(mode) &&
               video_.GetCurrentRenderer(nullptr, 0, nullptr, nullptr, nullptr);
    }
    bool NativeGameUiFuncs::Bind(std::string& error)
    {
        error.clear();
        const GameUiVideo::Mode* source{};
        int count{};
        if (ready_ || !BackendsReady() || !video_.GetVideoModes(source, count) || count <= 0)
        {
            error = "Original GameUIFuncs requires complete live key/video services";
            return false;
        }
        std::vector<Mode> modes;
        modes.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) modes.push_back({source[i].wide, source[i].tall, source[i].bpp});
        // The legacy ABI requests a mutable array. Marshal a separate view so
        // its caller cannot corrupt the read-only video backend's snapshot.
        modes_ = std::move(modes);
        ready_ = true;
        return true;
    }
    bool NativeGameUiFuncs::IsReady() const { return ready_ && BackendsReady(); }
    void NativeGameUiFuncs::Shutdown() { ready_ = false; modes_.clear(); }
    bool NativeGameUiFuncs::IsKeyDown(const char* action, bool& down)
    {
        down = false;
        return IsReady() && keys_.IsActionDown(action, down);
    }
    const char* NativeGameUiFuncs::Key_NameForKey(int key) { return IsReady() ? keys_.Name(key) : ""; }
    const char* NativeGameUiFuncs::Key_BindingForKey(int key) { return IsReady() ? keys_.Binding(key) : ""; }
    int NativeGameUiFuncs::GetVGUI2KeyCodeForBind(const char* command) { return IsReady() ? keys_.VguiCodeForBinding(command) : 0; }
    void NativeGameUiFuncs::GetVideoModes(Mode** modes, int* count)
    {
        if (modes) *modes = nullptr;
        if (count) *count = 0;
        if (!IsReady()) return;
        if (modes) *modes = modes_.data();
        if (count) *count = static_cast<int>(modes_.size());
    }
    void NativeGameUiFuncs::GetCurrentVideoMode(int* wide, int* tall, int* bpp)
    {
        GameUiVideo::Mode mode{};
        if (IsReady()) video_.GetCurrentVideoMode(mode);
        if (wide) *wide = mode.wide;
        if (tall) *tall = mode.tall;
        if (bpp) *bpp = mode.bpp;
    }
    void NativeGameUiFuncs::GetCurrentRenderer(char* name, int capacity, int* windowed, int* hd, int* addons)
    {
        if (name && capacity > 0) *name = '\0';
        if (windowed) *windowed = 0;
        if (hd) *hd = 0;
        if (addons) *addons = 0;
        if (IsReady()) video_.GetCurrentRenderer(name, capacity, windowed, hd, addons);
    }
    bool NativeGameUiFuncs::IsConnectedToVACSecureServer()
    {
        // Xash has no VAC-authenticated transport. Never claim a secure session;
        // the harness also forbids modified clients on VAC-secured servers.
        return false;
    }
    int NativeGameUiFuncs::Key_KeyStringToKeyNum(const char* name) { return IsReady() ? keys_.Number(name) : -1; }
}
