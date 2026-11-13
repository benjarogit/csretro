#include "original_ui.h"
#include "build_registry.h"

#include <dlfcn.h>

namespace csr::client
{
    bool OriginalUi::SurfaceApi::Bind(void* module, std::string& error)
    {
#define LOAD(field, symbol) \
        field = reinterpret_cast<decltype(field)>(dlsym(module, #symbol)); \
        if (!field) { error = "Surface module is missing " #symbol; return false; }
        LOAD(create, csr_surface_create)
        LOAD(destroy, csr_surface_destroy)
        LOAD(object, csr_surface_vgui)
        LOAD(panel, csr_surface_set_panel)
        LOAD(input, csr_surface_set_input)
        LOAD(file, csr_surface_set_file)
        LOAD(popup, csr_surface_set_popup)
        LOAD(embed, csr_surface_set_embedded_panel)
        LOAD(solve, csr_surface_solve)
        LOAD(paint, csr_surface_paint)
        LOAD(begin, csr_surface_begin_frame)
        LOAD(end, csr_surface_end_frame)
        LOAD(move, csr_surface_input_move)
        LOAD(button, csr_surface_input_button)
        LOAD(visible, csr_surface_cursor_visible)
        LOAD(default_cursor, csr_surface_set_default_cursor)
#undef LOAD
        return true;
    }

    OriginalUi::~OriginalUi()
    {
        Shutdown();
        vgui_.reset();
        if (surface_) api_.destroy(surface_);
    }

    bool OriginalUi::Initialize(void* client, const std::filesystem::path& directory,
                               const std::string& client_hash, goldsrc::cl_enginefunc_t* engine,
                               std::string& error)
    {
        if (binding_.IsInitialized())
            return true;
        goldsrc::SCREENINFO screen{};
        screen.iSize = sizeof(screen);
        if (!engine || !engine->pfnGetScreenInfo || !engine->GetMousePosition ||
            !engine->pfnGetScreenInfo(&screen) || screen.iWidth <= 0 || screen.iHeight <= 0 || !files_.Bind(engine))
        {
            error = "Xash has not supplied valid screen, mouse and file callbacks";
            return false;
        }
        engine_ = engine;
        relative_mode_ = reinterpret_cast<decltype(relative_mode_)>(dlsym(client, "SDL_GetRelativeMouseMode"));
        relative_mouse_ = reinterpret_cast<decltype(relative_mouse_)>(dlsym(client, "SDL_GetRelativeMouseState"));
        if (!relative_mode_ || !relative_mouse_)
        {
            error = "Original client SDL2 mouse callbacks are unavailable";
            return false;
        }
        if (surface_ && vgui_)
            return binding_.Initialize(client, vgui_.get(), surface_object_, error);
        const auto vgui_path = directory / "vgui2.valve.so";
        if (!nitroapi::IsRegisteredClientVgui(client_hash, nitroapi::HashFileSha256(vgui_path.string())))
        {
            error = "vgui2.valve.so does not match the registered original client";
            return false;
        }
        vgui_ = Module(dlopen(vgui_path.c_str(), RTLD_NOW | RTLD_LOCAL), &dlclose);
        if (!vgui_)
        {
            const char* reason = dlerror();
            error = reason ? reason : "Cannot load vgui2.valve.so";
            return false;
        }
        if (!panels_.Prepare(client, vgui_.get(), error))
            return false;
        surface_module_ = Module(dlopen((directory / "xash_surface.so").c_str(), RTLD_NOW | RTLD_LOCAL), &dlclose);
        if (!surface_module_)
        {
            const char* reason = dlerror();
            error = reason ? reason : "Cannot load xash_surface.so";
            return false;
        }
        if (!api_.Bind(surface_module_.get(), error))
            return false;
        csr_surface_host_t host{};
        host.screen_wide = screen.iWidth;
        host.screen_tall = screen.iHeight;
        host.panel_abs_pos = [](void* p, int* x, int* y, void* u) { static_cast<OriginalUi*>(u)->panels_.AbsPos(p, x, y); };
        host.panel_insets = [](void* p, int* l, int* t, int* r, int* b, void* u) { static_cast<OriginalUi*>(u)->panels_.Insets(p, l, t, r, b); };
        host.panel_clip_rect = [](void* p, int* l, int* t, int* r, int* b, void* u) { static_cast<OriginalUi*>(u)->panels_.Clip(p, l, t, r, b); };
        host.resolve_font = [](const char* name, int, char* path, int capacity, void* u) { return static_cast<OriginalUi*>(u)->files_.Font(name, path, capacity); };
        host.user = this;
        surface_ = api_.create(&host);
        if (!surface_)
        {
            error = "Cannot create the single product Surface";
            return false;
        }
        auto panel = panels_.PanelFunctions();
        auto input = panels_.InputFunctions();
        csr_surface_file_fns_t file{
            [](const char* path, unsigned char* dest, int capacity, void* u) {
                return static_cast<OriginalUi*>(u)->files_.Read(path, dest, capacity);
            }, this};
        api_.panel(surface_, &panel);
        api_.input(surface_, &input);
        api_.file(surface_, &file);
        api_.popup(surface_, [](void* p, int mouse, int keyboard, void* u) {
            static_cast<OriginalUi*>(u)->panels_.Popup(p, mouse, keyboard);
        }, this);
        api_.default_cursor(surface_, 1);
        surface_object_ = api_.object(surface_);
        return binding_.Initialize(client, vgui_.get(), surface_object_, error);
    }

    bool OriginalUi::AttachRoot()
    {
        if (!binding_.IsInitialized()) return false;
        auto root = panels_.Root();
        if (!root) return false;
        api_.embed(surface_, root);
        return true;
    }

    void OriginalUi::Paint()
    {
        if (!AttachRoot()) return;
        goldsrc::SCREENINFO screen{};
        screen.iSize = sizeof(screen);
        if (!engine_->pfnGetScreenInfo(&screen) || screen.iWidth <= 0 || screen.iHeight <= 0) return;
        api_.begin(surface_, screen.iWidth, screen.iHeight);
        struct EndFrame
        {
            csr_surface_t* surface;
            decltype(&csr_surface_end_frame) end;
            ~EndFrame() { end(surface); }
        } frame{surface_, api_.end};
        auto root = panels_.Root();
        api_.solve(surface_, root, 0);
        api_.paint(surface_, root);
    }

    void OriginalUi::MouseMove()
    {
        MovePointer(false);
    }

    void OriginalUi::Frame()
    {
        MovePointer(true);
    }

    void OriginalUi::MovePointer(bool advance_frame)
    {
        if (!AttachRoot()) return;
        goldsrc::SCREENINFO screen{};
        screen.iSize = sizeof(screen);
        if (!engine_->pfnGetScreenInfo(&screen)) return;
        PointerHooks hooks{
            [](void* u, int* x, int* y) { static_cast<OriginalUi*>(u)->engine_->GetMousePosition(x, y); },
            [](void* u) { return static_cast<OriginalUi*>(u)->relative_mode_(); },
            [](void* u, int* x, int* y) { static_cast<OriginalUi*>(u)->relative_mouse_(x, y); },
            [](void* u) { auto& o = *static_cast<OriginalUi*>(u); return o.binding_.IsInitialized() && o.api_.visible(o.surface_) ? 1 : 0; },
            [](void* u, int x, int y) { auto& o = *static_cast<OriginalUi*>(u); o.api_.move(o.surface_, x, y); },
            nullptr, this};
        if (advance_frame)
            hooks.frame = [](void* u) {
                auto& o = *static_cast<OriginalUi*>(u);
                if (o.binding_.IsInitialized()) o.panels_.RunFrame();
            };
        pointer_.Update(screen.iWidth, screen.iHeight, hooks);
    }

    void OriginalUi::MouseButtons(int buttons)
    {
        if (!AttachRoot()) { buttons_.Clear(nullptr, nullptr); return; }
        buttons_.Update(buttons, [](void* owner, int button, int down) {
            auto& ui = *static_cast<OriginalUi*>(owner);
            ui.api_.button(ui.surface_, button, down);
        }, this);
    }

    void OriginalUi::ClearInput()
    {
        pointer_.Reset();
        buttons_.Clear([](void* owner, int button, int down) {
            auto& ui = *static_cast<OriginalUi*>(owner);
            if (ui.binding_.IsInitialized()) ui.api_.button(ui.surface_, button, down);
        }, this);
    }

    void OriginalUi::Shutdown()
    {
        ClearInput();
        binding_.Shutdown();
    }
}
