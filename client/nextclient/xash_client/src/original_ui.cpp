#include "original_ui.h"
#include "build_registry.h"
#include "elf_symbols.h"
#include <cmath>
#include <array>
#include <cstring>
#include <stdexcept>

#include <dlfcn.h>
#include <cstdio>

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
        LOAD(keyboard, csr_surface_set_keyboard_query)
        LOAD(popup_count, csr_surface_popup_count)
        LOAD(popup_at, csr_surface_popup_at)
#undef LOAD
        return true;
    }

    OriginalUi::~OriginalUi()
    {
        try { Shutdown(); }
        catch (...) { std::fprintf(stderr, "CSRETRO original UI shutdown failed\n"); }
        engine_panels_.reset();
        modules_.Reset();
        vgui_.reset();
        if (surface_) api_.destroy(surface_);
    }

    bool OriginalUi::Prepare(goldsrc::cl_enginefunc_t* engine, const std::string& client_hash, std::string& error)
    {
        if (binding_.HasBinding() || !context_.CanUnload())
        {
            error = "Prior native GameUI lifetime has not been retired";
            return false;
        }
        return keys_.PrepareEngine(engine, error) &&
               modules_.Preflight(engine, client_hash, keys_.EngineFingerprint(), keys_.Commands(), error);
    }

    bool OriginalUi::CanDestroy() const
    {
        return !binding_.HasBinding() && context_.CanUnload();
    }

    bool OriginalUi::Initialize(void* client, const std::filesystem::path& directory,
                               const std::string& client_hash, goldsrc::cl_enginefunc_t* engine,
                               goldsrc::HUD_KB_FIND_FUNC find_key, std::string& error)
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
        if (!modules_.Capture(client, error))
        {
            return false;
        }
        engine_ = engine;
        sdl_window_ = reinterpret_cast<decltype(sdl_window_)>(dlsym(client, "SDL_GL_GetCurrentWindow"));
        sdl_title_ = reinterpret_cast<decltype(sdl_title_)>(dlsym(client, "SDL_SetWindowTitle"));
        relative_mode_ = reinterpret_cast<decltype(relative_mode_)>(dlsym(client, "SDL_GetRelativeMouseMode"));
        relative_mouse_ = reinterpret_cast<decltype(relative_mouse_)>(dlsym(client, "SDL_GetRelativeMouseState"));
        if (!relative_mode_ || !relative_mouse_)
        {
            error = "Original client SDL2 mouse callbacks are unavailable";
            return false;
        }
        if (!video_.Bind(client, engine, error)) return false;
        struct VideoAttempt
        {
            GameUiVideo& video;
            XashKeys& keys;
            NativeGameUiFuncs& funcs;
            VguiBinding& binding;
            ~VideoAttempt()
            {
                if (!binding.HasBinding()) { funcs.Shutdown(); keys.Shutdown(); video.Shutdown(); }
            }
        } video_attempt{video_, keys_, gameui_funcs_, binding_};
        if (surface_ && vgui_)
            return StartBinding(client, find_key, error);
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
        GameUiVideo::Mode mode{};
        if (!video_.GetCurrentVideoMode(mode))
        {
            error = "GameUI video mode disappeared before Surface creation";
            return false;
        }
        csr_surface_host_t host{};
        host.screen_wide = mode.wide;
        host.screen_tall = mode.tall;
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
        return StartBinding(client, find_key, error);
    }

    bool OriginalUi::StartBinding(void* client, goldsrc::HUD_KB_FIND_FUNC find_key, std::string& error)
    {
        if (!keys_.Bind(client, vgui_.get(), engine_, find_key, error) || !gameui_funcs_.Bind(error)) return false;
        const VguiBinding::PanelServices services{
            engine_panels_.get(),
            &engine_vgui_,
            [](void* owner) {
                OriginalUi& ui = *static_cast<OriginalUi*>(owner);
                ui.api_.keyboard(ui.surface_, nullptr, nullptr);
                ui.context_.Shutdown();
                ui.api_.embed(ui.surface_, nullptr);
            },
            [](void* owner) noexcept {
                OriginalUi& ui = *static_cast<OriginalUi*>(owner);
                ui.gameui_funcs_.Shutdown();
                ui.keys_.Shutdown();
                ui.video_.Shutdown();
            },
            this,
            [](void* owner) { return static_cast<OriginalUi*>(owner)->context_.CanUnload(); },
            [](void* owner) noexcept { static_cast<OriginalUi*>(owner)->context_.EndNativeLifetime(); },
            [](void* owner) {
                auto& ui = *static_cast<OriginalUi*>(owner);
                if (ui.original_shutdown_)
                {
                    ui.original_shutdown_();
                }
            }
        };
        if (!binding_.BindInterfaces(client, vgui_.get(), surface_object_, error, &gameui_funcs_, &services))
        {
            return false;
        }
        GameUiVideo::Mode mode{};
        if (!video_.GetCurrentVideoMode(mode) || !binding_.InitializeEnginePanels(mode.wide, mode.tall, error))
        {
            binding_.Shutdown();
            return false;
        }
        if (!StartGameUi(client, error))
        {
            binding_.Shutdown();
            return false;
        }
        if (!binding_.InitializeViewport(error))
        {
            binding_.Shutdown();
            return false;
        }
        struct StartRequest
        {
            OriginalUi* ui;
            std::string* error;
            bool result{};
        } request{this, &error};
        if (!binding_.RunStartupUi(
                [](void* opaque) {
                    auto& request = *static_cast<StartRequest*>(opaque);
                    request.result = request.ui->context_.Start(*request.error);
                },
                &request
            ) ||
            !request.result || !binding_.StartViewport(error))
        {
            binding_.Shutdown();
            return false;
        }
        request.result = false;
        if (!binding_.RunStartupUi(
                [](void* opaque) {
                    auto& request = *static_cast<StartRequest*>(opaque);
                    request.result = request.ui->context_.InitializeConsole(*request.error);
                    if (request.result)
                    {
                        request.ui->context_.Activate();
                    }
                },
                &request
            ) ||
            !request.result || context_.ReleaseRequested())
        {
            binding_.Shutdown();
            if (error.empty())
            {
                error = "Native GameUI console startup was interrupted";
            }
            return false;
        }
        api_.keyboard(surface_, [](void* opaque) { return static_cast<OriginalUi*>(opaque)->NeedsKeyboard() ? 1 : 0; }, this);
        have_frame_ = false;
        return true;
    }

    bool OriginalUi::StartGameUi(void* client, std::string& error)
    {
        struct Request
        {
            OriginalUi* ui;
            void* client;
            std::string* error;
            bool result{};
        } request{this, client, &error};
        return binding_.RunStartupUi(
                   [](void* opaque) {
                       auto& request = *static_cast<Request*>(opaque);
                       auto& ui = *request.ui;
                       NativeGameUiContext::Host host{};
                       host.client = request.client;
                       host.gameui = ui.modules_.GameUiModule();
                       host.vgui = ui.vgui_.get();
                       host.filesystem = ui.binding_.FileSystem();
                       host.engine = ui.engine_;
                       host.panels = ui.engine_panels_.get();
                       host.commands = &ui.keys_.Commands();
                       host.clear_matches = reinterpret_cast<decltype(host.clear_matches)>(
                           nitroapi::FindFunctionAddress(host.gameui, "_ZN10ObjectList5ClearEb")
                       );
                       host.base_directory = ui.modules_.BaseDirectory();
                       host.product_version = CSRETRO_SYSTEM_VERSION;
                       host.invoke_context = &ui;
                       host.invoke = [](void (*target)(), void* opaque) {
                           struct Call
                           {
                               void (*target)();
                           } call{target};
                           static_cast<OriginalUi*>(opaque)->binding_.RunStartupUi(
                               [](void* opaque) { static_cast<Call*>(opaque)->target(); }, &call
                           );
                       };
                       host.platform_context = &ui;
                       host.set_title = [](const char* title, void* opaque) { static_cast<OriginalUi*>(opaque)->SetTitle(title); };
                       host.error = [](const char*, void* opaque) { static_cast<OriginalUi*>(opaque)->Shutdown(); };
                       host.ui_hooks.context = &ui;
                       host.ui_hooks.key_handler = [](void* opaque, int down, int key, const char* binding) {
                           return static_cast<OriginalUi*>(opaque)->HandleKey(down, key, binding);
                       };
                       host.ui_hooks.event_handler = [](void* opaque, void* event, void*) {
                           static_cast<OriginalUi*>(opaque)->context_.HandleEvent(event);
                       };
                       host.ui_hooks.paint = [](void* opaque, int x, int y, int right, int bottom) {
                           auto& ui = *static_cast<OriginalUi*>(opaque);
                           GameUiVideo::Mode mode{};
                           if (!ui.video_.GetCurrentVideoMode(mode) || x != 0 || y != 0 || right != mode.wide || bottom != mode.tall)
                           {
                               throw std::runtime_error("Native BaseUI paint requires the actual full-window viewport");
                           }
                           ui.Paint();
                       };
                       host.ui_hooks.reset_input = [](void* opaque) { static_cast<OriginalUi*>(opaque)->ClearInput(); };
                       host.ui_hooks.set_visibility = [](void* opaque, int type, bool visible) {
                           static_cast<OriginalUi*>(opaque)->engine_panels_->SetVisible(type, visible);
                       };
                       host.ui_hooks.popup_count = [](void* opaque) {
                           auto& ui = *static_cast<OriginalUi*>(opaque);
                           return ui.api_.popup_count(ui.surface_);
                       };
                       host.ui_hooks.get_popup = [](void* opaque, int index) {
                           auto& ui = *static_cast<OriginalUi*>(opaque);
                           return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ui.api_.popup_at(ui.surface_, index)));
                       };
                       host.input_hooks.context = &ui;
                       host.input_hooks.needs_keyboard = [](void* opaque) { return static_cast<OriginalUi*>(opaque)->NeedsKeyboard(); };
                       host.input_hooks.mouse_move = [](void* opaque, int x, int y) {
                           auto& ui = *static_cast<OriginalUi*>(opaque);
                           ui.api_.move(ui.surface_, x, y);
                       };
                       host.input_hooks.mouse_button = [](void* opaque, int button, int down) {
                           auto& ui = *static_cast<OriginalUi*>(opaque);
                           ui.api_.button(ui.surface_, button, down);
                       };
                       host.input_hooks.reset_mouse = [](void* opaque) { static_cast<OriginalUi*>(opaque)->ClearMouseBound(); };
                       if (!ui.context_.Bind(host, *request.error) || !ui.binding_.SetBaseUi(ui.context_.BaseUiInterface()))
                       {
                           if (request.error->empty())
                           {
                               *request.error = "Cannot publish the native BaseUI owner";
                           }
                           return;
                       }
                       const auto original_factory =
                           reinterpret_cast<NativeGameUiRuntime::Factory>(nitroapi::FindFunctionAddress(request.client, "CreateInterface"));
                       const auto vgui_factory =
                           reinterpret_cast<NativeGameUiRuntime::Factory>(nitroapi::FindFunctionAddress(ui.vgui_.get(), "CreateInterface"));
                       if (!original_factory || !vgui_factory)
                       {
                           *request.error = "Native GameUI requires the original client and VGUI factories";
                           return;
                       }
                       // This registered implementation reads roles 0/1/2 and optional 5.
                       // Every entry is callable; optional requests stay with the real module.
                       std::array<NativeGameUiRuntime::Factory, 6> factories{
                           ui.binding_.EngineFactory(),
                           vgui_factory,
                           ui.binding_.EngineFactory(),
                           ui.context_.GameFactory(),
                           ui.context_.GameFactory(),
                           original_factory
                       };
                       request.result = ui.context_.Initialize(factories.data(), factories.size(), *request.error);
                   },
                   &request
               ) &&
               request.result;
    }

    bool OriginalUi::NeedsKeyboard() const
    {
        return context_.IsReady() &&
               (context_.IsActive() || context_.IsConsoleVisible() || context_.HasExclusiveInput() || (surface_ && api_.visible(surface_)));
    }

    int OriginalUi::HandleKey(int down, int key, const char* binding)
    {
        if (!context_.IsReady())
        {
            return 0;
        }
        if (down && binding && std::strcmp(binding, "toggleconsole") == 0)
        {
            if (context_.IsConsoleVisible())
            {
                context_.HideConsole();
            }
            else
            {
                context_.ShowConsole();
            }
            return 1;
        }
        if (down && key == 27)
        {
            if (context_.IsConsoleVisible())
            {
                context_.HideConsole();
            }
            else if (context_.IsActive())
            {
                const char* level = engine_->pfnGetLevelName ? engine_->pfnGetLevelName() : nullptr;
                if (level && *level && !context_.HasExclusiveInput())
                {
                    context_.Hide();
                }
            }
            else
            {
                context_.Activate();
            }
            return 1;
        }
        return NeedsKeyboard() ? 1 : 0;
    }

    void OriginalUi::SetTitle(const char* title)
    {
        if (!title || !sdl_window_ || !sdl_title_)
        {
            return;
        }
        if (void* window = sdl_window_())
        {
            sdl_title_(window, title);
        }
    }

    void OriginalUi::ClearMouseBound()
    {
        pointer_.Reset();
        buttons_.Clear(
            [](void* opaque, int button, int down) {
                auto& ui = *static_cast<OriginalUi*>(opaque);
                ui.api_.button(ui.surface_, button, down);
            },
            this
        );
    }

    bool OriginalUi::AttachRoot()
    {
        if (!binding_.IsInitialized()) return false;
        void* root = engine_panels_->Root();
        if (!root || !engine_panels_->IsReady())
        {
            return false;
        }
        api_.embed(surface_, root);
        return true;
    }

    void OriginalUi::Paint()
    {
        binding_.RunNativeUi([](void* owner) { static_cast<OriginalUi*>(owner)->PaintBound(); }, this);
    }

    void OriginalUi::PaintBound()
    {
        if (engine_panels_->ReleaseRequested())
        {
            Shutdown();
            return;
        }
        if (!AttachRoot()) return;
        GameUiVideo::Mode mode{};
        if (!video_.GetCurrentVideoMode(mode)) return;
        engine_panels_->Resize(mode.wide, mode.tall);
        if (!engine_panels_->IsReady() || engine_panels_->ReleaseRequested())
        {
            Shutdown();
            return;
        }
        api_.begin(surface_, mode.wide, mode.tall);
        struct EndFrame
        {
            csr_surface_t* surface;
            decltype(&csr_surface_end_frame) end;
            ~EndFrame() { end(surface); }
        } frame{surface_, api_.end};
        void* root = engine_panels_->Root();
        api_.solve(surface_, root, 0);
        api_.paint(surface_, root);
        if (engine_panels_->ReleaseRequested())
        {
            Shutdown();
        }
    }

    void OriginalUi::MouseMove()
    {
        MovePointer(false);
    }

    void OriginalUi::Frame(double time)
    {
        struct Request
        {
            OriginalUi* ui;
            double time;
        } request{this, time};
        binding_.RunNativeUi(
            [](void* opaque) {
                auto& request = *static_cast<Request*>(opaque);
                auto& ui = *request.ui;
                double delta{};
                if (std::isfinite(request.time))
                {
                    if (ui.have_frame_ && request.time >= ui.last_frame_)
                    {
                        delta = request.time - ui.last_frame_;
                    }
                    ui.last_frame_ = request.time;
                    ui.have_frame_ = true;
                }
                ui.context_.Frame(delta);
                if (ui.context_.ReleaseRequested())
                {
                    ui.Shutdown();
                    return;
                }
                ui.MovePointerBound(true);
            },
            &request
        );
    }

    void OriginalUi::MovePointer(bool advance_frame)
    {
        struct Request
        {
            OriginalUi* ui;
            bool advance;
        } request{this, advance_frame};
        binding_.RunNativeUi(
            [](void* context) {
                Request& request = *static_cast<Request*>(context);
                request.ui->MovePointerBound(request.advance);
            },
            &request
        );
    }

    void OriginalUi::MovePointerBound(bool advance_frame)
    {
        if (engine_panels_->ReleaseRequested())
        {
            Shutdown();
            return;
        }
        if (!AttachRoot()) return;
        GameUiVideo::Mode mode{};
        if (!video_.GetCurrentVideoMode(mode)) return;
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
                if (o.binding_.IsInitialized())
                {
                    o.panels_.RunFrame();
                    if (o.engine_panels_->ReleaseRequested())
                    {
                        o.Shutdown();
                    }
                }
            };
        pointer_.Update(mode.wide, mode.tall, hooks);
    }

    void OriginalUi::MouseButtons(int buttons)
    {
        struct Request
        {
            OriginalUi* ui;
            int buttons;
        } request{this, buttons};
        if (!binding_.RunNativeUi(
                [](void* context) {
                    Request& request = *static_cast<Request*>(context);
                    OriginalUi& ui = *request.ui;
                    if (!ui.AttachRoot())
                    {
                        ui.buttons_.Clear(nullptr, nullptr);
                        return;
                    }
                    ui.buttons_.Update(
                        request.buttons,
                        [](void* owner, int button, int down) {
                            OriginalUi& ui = *static_cast<OriginalUi*>(owner);
                            ui.api_.button(ui.surface_, button, down);
                        },
                        &ui
                    );
                },
                &request
            ))
        {
            buttons_.Clear(nullptr, nullptr);
        }
    }

    void OriginalUi::ClearInput()
    {
        pointer_.Reset();
        if (!binding_.RunNativeUi(
                [](void* owner) {
                    OriginalUi& ui = *static_cast<OriginalUi*>(owner);
                    ui.context_.ClearInput();
                    ui.ClearMouseBound();
                },
                this
            ))
        {
            buttons_.Clear(nullptr, nullptr);
        }
    }

    void OriginalUi::Shutdown()
    {
        ClearInput();
        try { binding_.Shutdown(); }
        catch (...)
        {
            if (!binding_.HasBinding()) { gameui_funcs_.Shutdown(); keys_.Shutdown(); video_.Shutdown(); }
            throw;
        }
        if (!binding_.HasBinding()) { gameui_funcs_.Shutdown(); keys_.Shutdown(); video_.Shutdown(); }
    }
}
