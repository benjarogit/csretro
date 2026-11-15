#include "NativeSdlInput.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <dlfcn.h>
#include <SDL_events.h>
#include <SDL_version.h>
#include <SDL_video.h>
#include "elf_symbols.h"

static_assert(sizeof(SDL_Event) == 56, "Original Linux SDL2 event ABI");
static_assert(sizeof(wchar_t) == 4, "Original Linux VGUI wide-character ABI");

namespace csr::client
{
    namespace
    {
        constexpr size_t kQueueCapacity = 256;
        constexpr int kLastNativeKey = 106;

        Uint32 EventWindow(const SDL_Event& event)
        {
            switch (event.type)
            {
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    return event.key.windowID;
                case SDL_TEXTINPUT:
                    return event.text.windowID;
                case SDL_WINDOWEVENT:
                    return event.window.windowID;
                case SDL_MOUSEMOTION:
                    return event.motion.windowID;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    return event.button.windowID;
                case SDL_MOUSEWHEEL:
                    return event.wheel.windowID;
                default:
                    return 0;
            }
        }

        bool Watched(const SDL_Event& event)
        {
            return event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT ||
                   (event.type == SDL_WINDOWEVENT &&
                    (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED));
        }

        bool Decode(const char* text, std::vector<wchar_t>& output)
        {
            size_t size = strnlen(text, SDL_TEXTINPUTEVENT_TEXT_SIZE);
            if (size == SDL_TEXTINPUTEVENT_TEXT_SIZE)
            {
                return false;
            }
            for (size_t i = 0; i < size;)
            {
                unsigned char lead = static_cast<unsigned char>(text[i++]);
                unsigned value{};
                unsigned extra{};
                unsigned minimum{};
                if (lead < 0x80)
                {
                    value = lead;
                }
                else if (lead >= 0xc2 && lead <= 0xdf)
                {
                    value = lead & 0x1f;
                    extra = 1;
                    minimum = 0x80;
                }
                else if (lead >= 0xe0 && lead <= 0xef)
                {
                    value = lead & 0x0f;
                    extra = 2;
                    minimum = 0x800;
                }
                else if (lead >= 0xf0 && lead <= 0xf4)
                {
                    value = lead & 7;
                    extra = 3;
                    minimum = 0x10000;
                }
                else
                {
                    return false;
                }
                if (i + extra > size)
                {
                    return false;
                }
                for (unsigned j = 0; j < extra; ++j)
                {
                    unsigned char byte = static_cast<unsigned char>(text[i++]);
                    if ((byte & 0xc0) != 0x80)
                    {
                        return false;
                    }
                    value = (value << 6) | (byte & 0x3f);
                }
                if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
                {
                    return false;
                }
                output.push_back(static_cast<wchar_t>(value));
            }
            return true;
        }
    } // namespace

    struct NativeSdlInput::State
    {
        Hooks hooks;
        std::thread::id thread;
        void* input{};
        void (*pressed)(void*, int){};
        void (*typed)(void*, int){};
        void (*released)(void*, int){};
        void (*character)(void*, wchar_t){};
        void (*wheel)(void*, int){};
        int (*translate)(int){};
        void (*version)(SDL_version*){};
        SDL_Window* (*current_window)(){};
        Uint32 (*window_id)(SDL_Window*){};
        void (*add_watch)(SDL_EventFilter, void*){};
        void (*del_watch)(SDL_EventFilter, void*){};
        std::mutex queue_mutex;
        std::condition_variable idle;
        std::array<SDL_Event, kQueueCapacity> queue{};
        size_t head{};
        size_t count{};
        bool overflow{};
        std::array<int, SDL_NUM_SCANCODES> held{};
        std::atomic<Uint32> window{};
        std::atomic<unsigned> watchers{};
        std::atomic<unsigned> dispatches{};
        std::atomic<unsigned> held_count{};
        std::atomic<bool> stop{true};
        std::atomic<bool> release_requested{};
        std::atomic<bool> clear_requested{};
        std::atomic<bool> native_alive{true};
        std::atomic<bool> registered{};
        std::shared_ptr<State> watch_lifetime;
        bool focused{true};
        bool clearing{};

        bool Main() const
        {
            return thread == std::this_thread::get_id();
        }
        bool Live() const
        {
            return native_alive.load() && !stop.load() && !release_requested.load() && !clear_requested.load();
        }

        struct Dispatch
        {
            State& state;
            explicit Dispatch(State& value) :
                state(value)
            {
                ++state.dispatches;
            }
            ~Dispatch()
            {
                if (--state.dispatches == 0 && state.clear_requested && !state.clearing)
                {
                    state.ReleaseHeld();
                }
            }
        };

        static int SDLCALL Watch(void* context, SDL_Event* event) noexcept
        {
            auto& state = *static_cast<State*>(context);
            ++state.watchers;
            try
            {
                if (!state.stop && event && Watched(*event) && EventWindow(*event) == state.window.load() && state.window.load())
                {
                    std::lock_guard<std::mutex> lock(state.queue_mutex);
                    if (!state.stop && !state.overflow)
                    {
                        if (state.count == kQueueCapacity)
                        {
                            state.overflow = true;
                            state.count = 0;
                            state.head = 0;
                        }
                        else
                        {
                            state.queue[(state.head + state.count) % kQueueCapacity] = *event;
                            ++state.count;
                        }
                    }
                }
            }
            catch (...)
            {
                state.stop = true;
                state.release_requested = true;
                state.clear_requested = true;
            }
            state.idle.notify_all();
            --state.watchers;
            return 1;
        }

        void ReleaseHeld() noexcept
        {
            if (clearing || !Main())
            {
                return;
            }
            clearing = true;
            clear_requested = false;
            ++dispatches;
            for (int& key : held)
            {
                int value = key;
                if (!value)
                {
                    continue;
                }
                key = 0;
                --held_count;
                if (native_alive)
                {
                    try
                    {
                        released(input, value);
                    }
                    catch (...)
                    {
                        key = value;
                        ++held_count;
                        clear_requested = true;
                        stop = true;
                        release_requested = true;
                    }
                }
            }
            if (native_alive)
            {
                try
                {
                    hooks.reset_mouse(hooks.context);
                }
                catch (...)
                {
                    stop = true;
                    release_requested = true;
                    clear_requested = true;
                }
            }
            --dispatches;
            clearing = false;
        }

        void ClearQueue()
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            count = 0;
            head = 0;
            overflow = false;
        }

        void RequestClear()
        {
            if (Main() && clearing)
            {
                return;
            }
            clear_requested = true;
            if (Main())
            {
                ClearQueue();
                if (!dispatches && !clearing)
                {
                    ReleaseHeld();
                }
            }
        }

        bool Unregister()
        {
            stop = true;
            if (!Main())
            {
                return false;
            }
            if (registered)
            {
                try
                {
                    // SDL2 dispatch and DelEventWatch share the recursive watcher mutex.
                    // Our watcher never invokes the host, so removal cannot be reentered there.
                    del_watch(Watch, this);
                    registered = false;
                }
                catch (...)
                {
                    return false;
                }
            }
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (!idle.wait_for(lock, std::chrono::milliseconds(50), [this] { return watchers.load() == 0; }))
            {
                return false;
            }
            lock.unlock();
            watch_lifetime.reset();
            return true;
        }

        void RefreshWindow()
        {
            SDL_Window* actual = current_window();
            Uint32 id = actual ? window_id(actual) : 0;
            if (id != window.load())
            {
                RequestClear();
                ReleaseHeld();
                window = id;
                focused = true;
            }
        }

        void Apply(const SDL_Event& event)
        {
            if (!Live() || !window || EventWindow(event) != window.load())
            {
                return;
            }
            if (event.type == SDL_WINDOWEVENT)
            {
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                {
                    focused = false;
                    RequestClear();
                    ReleaseHeld();
                }
                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                {
                    focused = true;
                }
                return;
            }
            bool wanted = hooks.needs_keyboard(hooks.context);
            if (!Live())
            {
                return;
            }
            if (!wanted || !focused)
            {
                ReleaseHeld();
                return;
            }
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            {
                int scan = static_cast<int>(event.key.keysym.scancode);
                if (scan <= 0 || scan >= SDL_NUM_SCANCODES)
                {
                    return;
                }
                if (event.type == SDL_KEYUP)
                {
                    int code = held[scan];
                    if (code)
                    {
                        held[scan] = 0;
                        --held_count;
                        if (native_alive)
                        {
                            try
                            {
                                released(input, code);
                            }
                            catch (...)
                            {
                                held[scan] = code;
                                ++held_count;
                                throw;
                            }
                        }
                    }
                    return;
                }
                int code = held[scan];
                if (code && !event.key.repeat)
                {
                    return;
                }
                if (!code)
                {
                    code = translate(event.key.keysym.sym);
                    if (!Live() || code <= 0 || code > kLastNativeKey)
                    {
                        return;
                    }
                    // A repeat whose initial press was not observed cannot invent a press.
                    if (event.key.repeat)
                    {
                        return;
                    }
                    held[scan] = code;
                    ++held_count;
                    pressed(input, code);
                }
                if (Live() && held[scan])
                {
                    typed(input, code);
                }
            }
            else if (event.type == SDL_TEXTINPUT)
            {
                std::vector<wchar_t> characters;
                if (!Decode(event.text.text, characters))
                {
                    return;
                }
                for (wchar_t code : characters)
                {
                    if (!Live() || !focused)
                    {
                        break;
                    }
                    character(input, code);
                }
            }
            else if (event.type == SDL_MOUSEMOTION)
            {
                hooks.mouse_move(hooks.context, event.motion.x, event.motion.y);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP)
            {
                int index = -1;
                switch (event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                        index = 0;
                        break;
                    case SDL_BUTTON_RIGHT:
                        index = 1;
                        break;
                    case SDL_BUTTON_MIDDLE:
                        index = 2;
                        break;
                    case SDL_BUTTON_X1:
                        index = 3;
                        break;
                    case SDL_BUTTON_X2:
                        index = 4;
                        break;
                }
                if (index >= 0)
                {
                    hooks.mouse_button(hooks.context, index, event.type == SDL_MOUSEBUTTONDOWN);
                }
            }
            else if (event.type == SDL_MOUSEWHEEL)
            {
                wheel(input, event.wheel.y);
            }
        }

        bool Pending() const
        {
            return registered || watchers || dispatches || (clear_requested && native_alive) || (release_requested && held_count);
        }
    };

    NativeSdlInput::NativeSdlInput() = default;
    NativeSdlInput::~NativeSdlInput()
    {
        Shutdown();
        EndNativeLifetime();
    }

    bool NativeSdlInput::Bind(void* client, void* vgui, const Hooks& hooks, std::string& error)
    {
        error.clear();
        if (!client || !vgui || !hooks.needs_keyboard || !hooks.mouse_move || !hooks.mouse_button || !hooks.reset_mouse ||
            (state_ && (state_->Live() || state_->Pending())))
        {
            error = "SDL input requires an inactive owner, borrowed modules and real input hooks";
            return false;
        }
        auto state = std::make_shared<State>();
        state->hooks = hooks;
        state->thread = std::this_thread::get_id();
        auto factory = reinterpret_cast<void* (*)(const char*, int*)>(nitroapi::FindFunctionAddress(vgui, "CreateInterface"));
        state->version = reinterpret_cast<decltype(state->version)>(dlsym(client, "SDL_GetVersion"));
        state->current_window = reinterpret_cast<decltype(state->current_window)>(dlsym(client, "SDL_GL_GetCurrentWindow"));
        state->window_id = reinterpret_cast<decltype(state->window_id)>(dlsym(client, "SDL_GetWindowID"));
        state->add_watch = reinterpret_cast<decltype(state->add_watch)>(dlsym(client, "SDL_AddEventWatch"));
        state->del_watch = reinterpret_cast<decltype(state->del_watch)>(dlsym(client, "SDL_DelEventWatch"));
        auto resolve = [vgui](const char* name) { return nitroapi::FindFunctionAddress(vgui, name); };
        state->pressed = reinterpret_cast<decltype(state->pressed)>(resolve("_ZN11CInputWin3222InternalKeyCodePressedEN5vgui27KeyCodeE"));
        state->typed = reinterpret_cast<decltype(state->typed)>(resolve("_ZN11CInputWin3220InternalKeyCodeTypedEN5vgui27KeyCodeE"));
        state->released =
            reinterpret_cast<decltype(state->released)>(resolve("_ZN11CInputWin3223InternalKeyCodeReleasedEN5vgui27KeyCodeE"));
        state->character = reinterpret_cast<decltype(state->character)>(resolve("_ZN11CInputWin3216InternalKeyTypedEw"));
        state->wheel = reinterpret_cast<decltype(state->wheel)>(resolve("_ZN11CInputWin3220InternalMouseWheeledEi"));
        state->translate = reinterpret_cast<decltype(state->translate)>(resolve("KeyCode_VirtualKeyToVGUI"));
        if (!factory || !state->version || !state->current_window || !state->window_id || !state->add_watch || !state->del_watch ||
            !state->pressed || !state->typed || !state->released || !state->character || !state->wheel || !state->translate)
        {
            error = "SDL input is missing actual SDL2 or verified original VGUI functions";
            return false;
        }
        Dl_info sdl_owner{};
        if (!dladdr(reinterpret_cast<void*>(state->version), &sdl_owner) || !sdl_owner.dli_fbase)
        {
            error = "SDL input cannot identify the borrowed SDL2 owner";
            return false;
        }
        const std::array<void*, 4> functions = {
            reinterpret_cast<void*>(state->current_window),
            reinterpret_cast<void*>(state->window_id),
            reinterpret_cast<void*>(state->add_watch),
            reinterpret_cast<void*>(state->del_watch)
        };
        for (void* function : functions)
        {
            Dl_info owner{};
            if (!dladdr(function, &owner) || owner.dli_fbase != sdl_owner.dli_fbase)
            {
                error = "SDL input functions must belong to the same already-loaded SDL2 object";
                return false;
            }
        }
        SDL_version version{};
        state->version(&version);
        SDL_Window* window = state->current_window();
        state->window = window ? state->window_id(window) : 0;
        state->input = factory("VGUI_InputInternal001", nullptr);
        if (version.major != 2 || (version.minor == 0 && version.patch < 8) || !state->window || !state->input)
        {
            error = "SDL input requires SDL2 >= 2.0.8 watcher synchronization, its existing current window and native InputInternal";
            return false;
        }
        state_ = state;
        state->stop = false;
        state->watch_lifetime = state;
        state->registered = true;
        try
        {
            state->add_watch(State::Watch, state.get());
        }
        catch (...)
        {
            state->release_requested = true;
            state->stop = true;
            state->clear_requested = true;
            error = "SDL input watch registration failed; removal must be confirmed before unload";
            Shutdown();
            return false;
        }
        return true;
    }

    void NativeSdlInput::Drain()
    {
        auto state = state_;
        if (!state || !state->Main())
        {
            return;
        }
        State::Dispatch scope(*state);
        try
        {
            if (!state->Live())
            {
                state->RequestClear();
                return;
            }
            state->RefreshWindow();
            if (!state->Live())
            {
                return;
            }
            if (!state->hooks.needs_keyboard(state->hooks.context) && state->native_alive)
            {
                state->ReleaseHeld();
            }
            if (!state->Live())
            {
                return;
            }
            std::array<SDL_Event, kQueueCapacity> events;
            size_t count{};
            bool overflow{};
            {
                std::lock_guard<std::mutex> lock(state->queue_mutex);
                overflow = state->overflow;
                if (!overflow)
                {
                    count = state->count;
                    for (size_t i = 0; i < count; ++i)
                    {
                        events[i] = state->queue[(state->head + i) % kQueueCapacity];
                    }
                }
                state->head = 0;
                state->count = 0;
                state->overflow = false;
            }
            if (overflow)
            {
                state->RequestClear();
                return;
            }
            for (size_t i = 0; i < count && state->Live(); ++i)
            {
                state->Apply(events[i]);
            }
        }
        catch (...)
        {
            state->stop = true;
            state->release_requested = true;
            state->RequestClear();
        }
    }

    void NativeSdlInput::HandleEvent(const void* value)
    {
        auto state = state_;
        if (!state || !value || !state->Main() || !state->Live())
        {
            return;
        }
        State::Dispatch scope(*state);
        SDL_Event event{};
        std::memcpy(&event, value, sizeof(event));
        try
        {
            state->RefreshWindow();
            state->Apply(event);
        }
        catch (...)
        {
            state->stop = true;
            state->release_requested = true;
            state->RequestClear();
        }
    }

    void NativeSdlInput::Clear()
    {
        auto state = state_;
        if (state)
        {
            state->RequestClear();
        }
    }
    bool NativeSdlInput::Shutdown()
    {
        auto state = state_;
        if (!state)
        {
            return true;
        }
        state->release_requested = true;
        state->stop = true;
        state->RequestClear();
        bool removed = state->Unregister();
        return removed && !state->Pending();
    }
    void NativeSdlInput::EndNativeLifetime() noexcept
    {
        auto state = state_;
        if (!state)
        {
            return;
        }
        state->native_alive = false;
        state->release_requested = true;
        state->stop = true;
        state->clear_requested = true;
        if (state->Main() && !state->dispatches)
        {
            state->ReleaseHeld();
        }
    }
    bool NativeSdlInput::IsReady() const
    {
        return state_ && state_->Live();
    }
    bool NativeSdlInput::ReleaseRequested() const
    {
        return state_ && state_->release_requested;
    }
    bool NativeSdlInput::HasPendingWatch() const
    {
        return state_ && state_->Pending();
    }
} // namespace csr::client
