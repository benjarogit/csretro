#include "xash_keys.h"
#include "build_registry.h"
#include "elf_symbols.h"
#include <goldsrc/usercmd.h>

#include <cctype>
#include <cstring>
#include <dlfcn.h>
#include <string_view>

namespace csr::client
{
    namespace
    {
        bool Text(const char* value, std::string_view& view)
        {
            if (!value) return false;
            const size_t size = strnlen(value, 256);
            if (!size || size == 256) return false;
            view = {value, size};
            return true;
        }
        bool Equal(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i)
                if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
            return true;
        }
        const char* SdlName(std::string_view name)
        {
            struct Alias { const char* engine; const char* sdl; };
            static constexpr Alias aliases[] = {
                {"UPARROW", "Up"}, {"DOWNARROW", "Down"}, {"LEFTARROW", "Left"}, {"RIGHTARROW", "Right"},
                {"ALT", "Left Alt"}, {"CTRL", "Left Ctrl"}, {"SHIFT", "Left Shift"},
                {"SCROLLOCK", "ScrollLock"}, {"INS", "Insert"}, {"DEL", "Delete"},
                {"PGUP", "PageUp"}, {"PGDN", "PageDown"}, {"SEMICOLON", ";"},
                {"KP_HOME", "Keypad 7"}, {"KP_UPARROW", "Keypad 8"}, {"KP_PGUP", "Keypad 9"},
                {"KP_LEFTARROW", "Keypad 4"}, {"KP_5", "Keypad 5"}, {"KP_RIGHTARROW", "Keypad 6"},
                {"KP_END", "Keypad 1"}, {"KP_DOWNARROW", "Keypad 2"}, {"KP_PGDN", "Keypad 3"},
                {"KP_INS", "Keypad 0"}, {"KP_DEL", "Keypad ."}, {"KP_SLASH", "Keypad /"},
                {"KP_MINUS", "Keypad -"}, {"KP_PLUS", "Keypad +"}, {"KP_ENTER", "Keypad Enter"},
                {"KP_MULTIPLY", "Keypad *"}, {"KP_MUL", "Keypad *"}
            };
            for (const auto& alias : aliases) if (Equal(name, alias.engine)) return alias.sdl;
            return nullptr;
        }
    }

    bool XashKeyQueries::Bind(void* engine, void* client, void* vgui, goldsrc::cl_enginefunc_t* callbacks,
                             goldsrc::HUD_KB_FIND_FUNC find, int engine_key_count, std::string& error)
    {
        error.clear();
        if (IsBound() || !engine || !client || !vgui || !callbacks || !callbacks->Key_LookupBinding || !find ||
            engine_key_count < legacy_keys || engine_key_count > 4096)
        {
            error = "Native key queries require verified modules, full legacy key domain and real lookup/action callbacks";
            return false;
        }
        auto name = reinterpret_cast<decltype(name_)>(nitroapi::FindFunctionAddress(engine, "Key_KeynumToString"));
        auto binding = reinterpret_cast<decltype(binding_)>(nitroapi::FindFunctionAddress(engine, "Key_GetBinding"));
        auto vgui_key = reinterpret_cast<decltype(vgui_key_)>(nitroapi::FindFunctionAddress(vgui, "KeyCode_VirtualKeyToVGUI"));
        auto sdl_key = reinterpret_cast<decltype(sdl_key_)>(dlsym(client, "SDL_GetKeyFromName"));
        if (!name || !binding || !vgui_key || !sdl_key)
        {
            error = "Native key queries are missing verified Xash/VGUI functions or the borrowed SDL2 translator";
            return false;
        }
        name_ = name;
        binding_ = binding;
        vgui_key_ = vgui_key;
        sdl_key_ = sdl_key;
        find_ = find;
        callbacks_ = callbacks;
        return true;
    }

    void XashKeyQueries::Shutdown()
    {
        callbacks_ = nullptr;
        find_ = nullptr;
        name_ = binding_ = nullptr;
        sdl_key_ = nullptr;
        vgui_key_ = nullptr;
    }

    bool XashKeyQueries::IsActionDown(const char* action, bool& down) const
    {
        down = false;
        std::string_view text;
        if (!IsBound() || !Text(action, text)) return false;
        auto key = find_(action);
        if (!key) return false;
        down = (key->state & 1) != 0;
        return true;
    }
    const char* XashKeyQueries::Name(int key) const
    {
        if (!IsBound() || key < 0 || key >= legacy_keys) return "";
        auto value = name_(key);
        return value ? value : "";
    }
    const char* XashKeyQueries::Binding(int key) const
    {
        if (!IsBound() || key < 0 || key >= legacy_keys) return "";
        auto value = binding_(key);
        return value ? value : ""; // A genuinely unbound key, not a substitute service.
    }
    int XashKeyQueries::Number(const char* name) const
    {
        std::string_view text;
        if (!IsBound() || !Text(name, text)) return -1;
        const std::string stable_input(text); // Engine names may use one shared scratch buffer.
        text = stable_input;
        if (Equal(text, "SEMICOLON")) text = ";";
        if (text.size() == 1) return std::tolower(static_cast<unsigned char>(text[0]));
        for (int key = 0; key < legacy_keys; ++key)
        {
            std::string_view candidate;
            if (Text(Name(key), candidate) && (candidate.size() == 1 || candidate.front() != '<') && Equal(candidate, text)) return key;
        }
        return -1;
    }
    int XashKeyQueries::VguiCodeForBinding(const char* command) const
    {
        std::string_view input;
        if (!IsBound() || !callbacks_->Key_LookupBinding || !Text(command, input)) return 0;
        auto name = callbacks_->Key_LookupBinding(command);
        std::string_view text;
        if (!Text(name, text)) return 0;
        int key{};
        if (text.size() == 1 && static_cast<unsigned char>(text[0]) >= 32 && static_cast<unsigned char>(text[0]) < 127)
            key = std::tolower(static_cast<unsigned char>(text[0]));
        else
        {
            const char* alias = SdlName(text);
            key = sdl_key_(alias ? alias : name);
        }
        return key ? vgui_key_(key) : 0;
    }

    bool XashKeys::Bind(void* client, void* vgui, goldsrc::cl_enginefunc_t* callbacks,
                        goldsrc::HUD_KB_FIND_FUNC find, std::string& error)
    {
        error.clear();
        if (engine_ || !callbacks || !callbacks->pfnGetScreenInfo)
        {
            error = "Xash key source is already bound or lacks the engine owner callback";
            return false;
        }
        Dl_info owner{};
        if (!dladdr(reinterpret_cast<void*>(callbacks->pfnGetScreenInfo), &owner) || !owner.dli_fname)
        {
            error = "Cannot identify the loaded Xash key-service owner";
            return false;
        }
        auto count = nitroapi::RegisteredXashKeyCount(nitroapi::HashFileSha256(owner.dli_fname));
        if (!count)
        {
            error = "Loaded engine does not match the registered Xash key-service fingerprint";
            return false;
        }
        Module engine(dlopen(owner.dli_fname, RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
        auto host = engine ? nitroapi::FindFunctionAddress(engine.get(), "Host_Main") : 0;
        Dl_info host_owner{};
        if (!host || !dladdr(reinterpret_cast<void*>(host), &host_owner) || host_owner.dli_fbase != owner.dli_fbase)
        {
            error = "Verified Xash entry does not belong to the already-loaded callback owner";
            return false;
        }
        if (!queries_.Bind(engine.get(), client, vgui, callbacks, find, *count, error)) return false;
        engine_ = std::move(engine);
        return true;
    }
    void XashKeys::Shutdown()
    {
        queries_.Shutdown();
        engine_.reset();
    }
}
