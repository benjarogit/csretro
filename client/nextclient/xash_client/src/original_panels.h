#pragma once

#include <string>
#include <xash_surface/xash_surface.h>

namespace csr::client
{
    // Borrowed original VGUI objects and methods. Native panel handles retain
    // their original unsigned-int ABI; no vendored IPanel vtable is used.
    class OriginalPanels
    {
    public:
        bool Prepare(void* client, void* vgui, std::string& error);
        void* Root() const;
        void AbsPos(void* panel, int* x, int* y) const;
        void Insets(void* panel, int* l, int* t, int* r, int* b) const;
        void Clip(void* panel, int* x0, int* y0, int* x1, int* y1) const;
        void Popup(void* panel, int mouse, int keyboard) const;
        void RunFrame() const;
        csr_surface_panel_fns_t PanelFunctions();
        csr_surface_input_fns_t InputFunctions();

    private:
        void* panels_{};
        void* input_{};
        void* ivgui_{};
        void** viewport_{};
        unsigned (*root_)(void*){};
        void (*abs_)(void*, int&, int&){};
        void (*insets_)(void*, int&, int&, int&, int&){};
        void (*clip_)(void*, int&, int&, int&, int&){};
        bool (*visible_)(void*, unsigned){};
        bool (*popup_)(void*, unsigned){};
        int (*count_)(void*, unsigned){};
        unsigned (*child_)(void*, unsigned, int){};
        void (*solve_)(void*, unsigned){};
        void (*scheme_)(void*, unsigned){};
        void (*paint_)(void*, unsigned, bool, bool){};
        void (*popup_enable_)(void*, unsigned, bool){};
        void (*mouse_enable_)(void*, unsigned, bool){};
        void (*keyboard_enable_)(void*, unsigned, bool){};
        void (*cursor_)(void*, int, int){};
        void (*focus_)(void*, int, int){};
        void (*press_)(void*, int){};
        void (*release_)(void*, int){};
        void (*dispatch_)(void*){};
        void (*frame_)(void*){};
    };
}
