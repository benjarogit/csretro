#pragma once

#include "engine_files.h"
#include "gameui_video.h"
#include "NativeGameUiFuncs.h"
#include "NativePanelTree.h"
#include "client_callbacks.h"
#include "original_panels.h"
#include "vgui_binding.h"
#include "pointer_motion.h"
#include "NativeGameUiContext.h"
#include "NativeKeyCapture.h"
#include "OriginalUiModules.h"
#include <filesystem>
#include <functional>
#include <memory>

namespace csr::client
{
    // Owns the original VGUI and product Surface. The game module is borrowed
    // and must remain loaded until Shutdown has restored its interface globals.
    class OriginalUi
    {
    public:
        ~OriginalUi();
        bool Initialize(void* client, const std::filesystem::path& directory,
                        const std::string& client_hash, goldsrc::cl_enginefunc_t* engine,
                        goldsrc::HUD_KB_FIND_FUNC find_key, std::string& error);
        bool Prepare(goldsrc::cl_enginefunc_t* engine, const std::string& client_hash, std::string& error);
        bool CanDestroy() const;
        bool HasBinding() const
        {
            return binding_.HasBinding();
        }
        void SetOriginalShutdown(std::function<void()> callback)
        {
            original_shutdown_ = std::move(callback);
        }
        void Shutdown();
        void Paint();
        void Frame(double time);
        void MouseMove();
        void MouseButtons(int buttons);
        void ClearInput();
        int KeyEvent(NativeKeyCapture& capture, goldsrc::HUD_KEY_EVENT_FUNC original, int down, int key, const char* binding);

    private:
        struct SurfaceApi
        {
            decltype(&csr_surface_create) create{};
            decltype(&csr_surface_destroy) destroy{};
            decltype(&csr_surface_vgui) object{};
            decltype(&csr_surface_set_panel) panel{};
            decltype(&csr_surface_set_input) input{};
            decltype(&csr_surface_set_file) file{};
            decltype(&csr_surface_set_popup) popup{};
            decltype(&csr_surface_set_embedded_panel) embed{};
            decltype(&csr_surface_solve) solve{};
            decltype(&csr_surface_paint) paint{};
            decltype(&csr_surface_begin_frame) begin{};
            decltype(&csr_surface_end_frame) end{};
            decltype(&csr_surface_input_move) move{};
            decltype(&csr_surface_input_button) button{};
            decltype(&csr_surface_cursor_visible) visible{};
            decltype(&csr_surface_set_default_cursor) default_cursor{};
            decltype(&csr_surface_set_keyboard_query) keyboard{};
            decltype(&csr_surface_popup_count) popup_count{};
            decltype(&csr_surface_popup_at) popup_at{};
            bool Bind(void* module, std::string& error);
        } api_;
        bool StartBinding(void* client, goldsrc::HUD_KB_FIND_FUNC find_key, std::string& error);
        bool AttachRoot();
        bool StartGameUi(void* client, std::string& error);
        bool NeedsKeyboard() const;
        void ClearMouseBound();
        int HandleKey(int down, int key, const char* binding, bool repeat = false);
        void SetTitle(const char* title);
        void MovePointer(bool advance_frame);
        void PaintBound();
        void MovePointerBound(bool advance_frame);
        using Module = std::unique_ptr<void, int (*)(void*)>;
        Module surface_module_{nullptr, nullptr};
        Module vgui_{nullptr, nullptr};
        csr_surface_t* surface_{};
        void* surface_object_{};
        goldsrc::cl_enginefunc_t* engine_{};
        MouseButtonState buttons_;
        PointerMotion pointer_;
        int (*relative_mode_)(){};
        unsigned (*relative_mouse_)(int*, int*){};
        EngineFiles files_;
        GameUiVideo video_;
        XashKeys keys_;
        NativeGameUiFuncs gameui_funcs_{video_, keys_.Queries()};
        OriginalPanels panels_;
        std::unique_ptr<NativePanelTree> engine_panels_ = std::make_unique<NativePanelTree>();
        NativeEngineVGui engine_vgui_{*engine_panels_};
        VguiBinding binding_;
        std::function<void()> original_shutdown_;
        OriginalUiModules modules_;
        NativeGameUiContext context_;
        double last_frame_{};
        bool have_frame_{};
        void* (*sdl_window_)(){};
        void (*sdl_title_)(void*, const char*){};
    };
}
