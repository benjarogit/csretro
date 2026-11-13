#pragma once

#include "engine_files.h"
#include "client_callbacks.h"
#include "original_panels.h"
#include "vgui_binding.h"
#include "pointer_motion.h"
#include <filesystem>
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
                        std::string& error);
        void Shutdown();
        void Paint();
        void Frame();
        void MouseMove();
        void MouseButtons(int buttons);
        void ClearInput();

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
            bool Bind(void* module, std::string& error);
        } api_;
        bool AttachRoot();
        void MovePointer(bool advance_frame);
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
        OriginalPanels panels_;
        VguiBinding binding_;
    };
}
