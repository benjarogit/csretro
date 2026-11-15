// Synthetic SDL2/VGUI input services; no window, display or original module is opened.
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <SDL_events.h>
#include <SDL_version.h>
#include <SDL_video.h>

struct SDL_Window
{
    Uint32 id;
};
namespace
{
    struct Entry
    {
        int kind;
        int value;
    };
    std::recursive_mutex watch_mutex;
    std::mutex trace_mutex;
    SDL_EventFilter watcher{};
    void* userdata{};
    SDL_Window window{41};
    bool has_window{true};
    int major{2};
    int patch{10};
    int config{};
    int adds{};
    int deletes{};
    int gui_wrong_thread{};
    std::thread::id main_thread;
    std::vector<Entry> trace;
    void (*native_hook)(void*, int){};
    void* native_context{};
    std::atomic<bool> emitting{};
    std::atomic<bool> delay_emit{};
    void Record(int kind, int value)
    {
        {
            std::lock_guard<std::mutex> lock(trace_mutex);
            if (main_thread != std::this_thread::get_id())
            {
                ++gui_wrong_thread;
            }
            trace.push_back({kind, value});
        }
        if (native_hook)
        {
            native_hook(native_context, kind);
        }
    }
} // namespace
namespace vgui2
{
    enum KeyCode : int
    {
        KEY_NONE = 0
    };
} // namespace vgui2
class CInputWin32
{
public:
    void InternalKeyCodePressed(vgui2::KeyCode);
    void InternalKeyCodeTyped(vgui2::KeyCode);
    void InternalKeyCodeReleased(vgui2::KeyCode);
    void InternalKeyTyped(wchar_t);
    void InternalMouseWheeled(int);
};
void CInputWin32::InternalKeyCodePressed(vgui2::KeyCode key)
{
    Record(1, key);
}
void CInputWin32::InternalKeyCodeTyped(vgui2::KeyCode key)
{
    Record(2, key);
}
void CInputWin32::InternalKeyCodeReleased(vgui2::KeyCode key)
{
    Record(3, key);
}
void CInputWin32::InternalKeyTyped(wchar_t key)
{
    Record(4, static_cast<int>(key));
}
void CInputWin32::InternalMouseWheeled(int delta)
{
    Record(5, delta);
}
extern "C" int KeyCode_VirtualKeyToVGUI(int key)
{
    return key == SDLK_a ? 11 : key == SDLK_b ? 12 : key == SDLK_LEFT ? 88 : 0;
}
extern "C" void* CreateInterface(const char* name, int*)
{
    static CInputWin32 input;
    return config != 1 && std::strcmp(name, "VGUI_InputInternal001") == 0 ? &input : nullptr;
}
extern "C" void SDL_GetVersion(SDL_version* value)
{
    value->major = static_cast<Uint8>(major);
    value->minor = 0;
    value->patch = static_cast<Uint8>(patch);
}
extern "C" SDL_Window* SDL_GL_GetCurrentWindow()
{
    return has_window ? &window : nullptr;
}
extern "C" Uint32 SDL_GetWindowID(SDL_Window* value)
{
    return value ? value->id : 0;
}
extern "C" void SDL_AddEventWatch(SDL_EventFilter callback, void* context)
{
    std::lock_guard<std::recursive_mutex> lock(watch_mutex);
    watcher = callback;
    userdata = context;
    ++adds;
    if (config == 2)
    {
        throw std::runtime_error("fixture AddEventWatch failed after registration");
    }
}
extern "C" void SDL_DelEventWatch(SDL_EventFilter callback, void* context)
{
    std::lock_guard<std::recursive_mutex> lock(watch_mutex);
    ++deletes;
    if (config == 3)
    {
        throw std::runtime_error("fixture DelEventWatch failed");
    }
    if (watcher == callback && userdata == context)
    {
        watcher = nullptr;
        userdata = nullptr;
    }
}
extern "C" void FixtureSdlReset()
{
    std::lock_guard<std::recursive_mutex> lock(watch_mutex);
    watcher = nullptr;
    userdata = nullptr;
    config = 0;
    major = 2;
    patch = 10;
    window.id = 41;
    has_window = true;
    adds = deletes = gui_wrong_thread = 0;
    native_hook = nullptr;
    native_context = nullptr;
    main_thread = std::this_thread::get_id();
    emitting = false;
    delay_emit = false;
    std::lock_guard<std::mutex> trace_lock(trace_mutex);
    trace.clear();
}
extern "C" void FixtureSdlConfigure(int setting, int value)
{
    if (setting == 0)
    {
        config = value;
    }
    if (setting == 1)
    {
        major = value;
    }
    if (setting == 2)
    {
        has_window = value;
    }
    if (setting == 3)
    {
        window.id = static_cast<Uint32>(value);
    }
    if (setting == 4)
    {
        delay_emit = value != 0;
    }
    if (setting == 5)
    {
        patch = value;
    }
}
extern "C" void FixtureSdlNativeHook(void (*hook)(void*, int), void* context)
{
    native_hook = hook;
    native_context = context;
}
extern "C" int FixtureSdlCounter(int field)
{
    if (field == 0)
    {
        return adds;
    }
    if (field == 1)
    {
        return deletes;
    }
    if (field == 2)
    {
        return watcher != nullptr;
    }
    if (field == 3)
    {
        return gui_wrong_thread;
    }
    return emitting;
}
extern "C" void FixtureSdlEmit(const void* value)
{
    SDL_Event event{};
    std::memcpy(&event, value, sizeof(event));
    std::lock_guard<std::recursive_mutex> lock(watch_mutex);
    emitting = true;
    if (delay_emit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    if (watcher)
    {
        watcher(userdata, &event);
    }
    emitting = false;
}
extern "C" int FixtureSdlTrace(int index, int field)
{
    std::lock_guard<std::mutex> lock(trace_mutex);
    if (index < 0)
    {
        return static_cast<int>(trace.size());
    }
    if (static_cast<size_t>(index) >= trace.size())
    {
        return -1;
    }
    return field ? trace[index].value : trace[index].kind;
}
