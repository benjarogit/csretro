#include "original_panels.h"
#include "original_vgui_symbols.h"
#include "vgui_binding.h"

#include <cstdint>
#include <dlfcn.h>

namespace csr::client
{
    namespace
    {
        unsigned Handle(void* panel) { return static_cast<unsigned>(reinterpret_cast<uintptr_t>(panel)); }
        void* Panel(unsigned handle) { return reinterpret_cast<void*>(static_cast<uintptr_t>(handle)); }
        OriginalPanels& Owner(void* user) { return *static_cast<OriginalPanels*>(user); }
    }

    bool OriginalPanels::Prepare(void* client, void* vgui, std::string& error)
    {
        *this = {};
        error.clear();
        if (!client || !vgui)
        {
            error = "Original panel binding requires both borrowed modules";
            return false;
        }
        auto factory = reinterpret_cast<VguiBinding::Factory>(dlsym(vgui, "CreateInterface"));
        if (!factory)
        {
            error = "Original panel binding requires the VGUI factory";
            return false;
        }
        panels_ = factory("VGUI_Panel007", nullptr);
        input_ = factory("VGUI_InputInternal001", nullptr);
        ivgui_ = factory("VGUI_ivgui006", nullptr);
        if (!panels_ || !input_ || !ivgui_)
        {
            error = "Original panel/input/VGUI interfaces are missing";
            return false;
        }
        using detail::Symbol;
        viewport_ = Symbol<void**>(client, "gViewPortInterface", error);
        // CounterStrikeViewport constructs the TeamFortressViewport base;
        // its inherited accessor returns the real root VPANEL.
        root_ = Symbol<decltype(root_)>(client, "_ZN20TeamFortressViewport16GetViewPortPanelEv", error);
        abs_ = Symbol<decltype(abs_)>(vgui, "_ZN5vgui26VPanel9GetAbsPosERiS1_", error);
        insets_ = Symbol<decltype(insets_)>(vgui, "_ZN5vgui26VPanel8GetInsetERiS1_S1_S1_", error);
        clip_ = Symbol<decltype(clip_)>(vgui, "_ZN5vgui26VPanel11GetClipRectERiS1_S1_S1_", error);
        visible_ = Symbol<decltype(visible_)>(vgui, "_ZN13VPanelWrapper9IsVisibleEj", error);
        popup_ = Symbol<decltype(popup_)>(vgui, "_ZN13VPanelWrapper7IsPopupEj", error);
        count_ = Symbol<decltype(count_)>(vgui, "_ZN13VPanelWrapper13GetChildCountEj", error);
        child_ = Symbol<decltype(child_)>(vgui, "_ZN13VPanelWrapper8GetChildEji", error);
        solve_ = Symbol<decltype(solve_)>(vgui, "_ZN13VPanelWrapper5SolveEj", error);
        scheme_ = Symbol<decltype(scheme_)>(vgui, "_ZN13VPanelWrapper26PerformApplySchemeSettingsEj", error);
        paint_ = Symbol<decltype(paint_)>(vgui, "_ZN13VPanelWrapper13PaintTraverseEjbb", error);
        popup_enable_ = Symbol<decltype(popup_enable_)>(vgui, "_ZN13VPanelWrapper8SetPopupEjb", error);
        mouse_enable_ = Symbol<decltype(mouse_enable_)>(vgui, "_ZN13VPanelWrapper20SetMouseInputEnabledEjb", error);
        keyboard_enable_ = Symbol<decltype(keyboard_enable_)>(vgui, "_ZN13VPanelWrapper23SetKeyBoardInputEnabledEjb", error);
        cursor_ = Symbol<decltype(cursor_)>(vgui, "_ZN11CInputWin3219InternalCursorMovedEii", error);
        focus_ = Symbol<decltype(focus_)>(vgui, "_ZN11CInputWin3216UpdateMouseFocusEii", error);
        press_ = Symbol<decltype(press_)>(vgui, "_ZN11CInputWin3220InternalMousePressedEN5vgui29MouseCodeE", error);
        release_ = Symbol<decltype(release_)>(vgui, "_ZN11CInputWin3221InternalMouseReleasedEN5vgui29MouseCodeE", error);
        dispatch_ = Symbol<decltype(dispatch_)>(vgui, "_ZN5CVGui16DispatchMessagesEv", error);
        frame_ = Symbol<decltype(frame_)>(vgui, "_ZN5CVGui8RunFrameEv", error);
        return error.empty();
    }

    void* OriginalPanels::Root() const { return viewport_ && *viewport_ && root_ ? Panel(root_(*viewport_)) : nullptr; }
    void OriginalPanels::AbsPos(void* panel, int* x, int* y) const { abs_(panel, *x, *y); }
    void OriginalPanels::Insets(void* panel, int* l, int* t, int* r, int* b) const { insets_(panel, *l, *t, *r, *b); }
    void OriginalPanels::Clip(void* panel, int* x0, int* y0, int* x1, int* y1) const { clip_(panel, *x0, *y0, *x1, *y1); }
    void OriginalPanels::Popup(void* panel, int mouse, int keyboard) const
    {
        popup_enable_(panels_, Handle(panel), true);
        mouse_enable_(panels_, Handle(panel), mouse != 0);
        keyboard_enable_(panels_, Handle(panel), keyboard != 0);
    }

    void OriginalPanels::RunFrame() const { frame_(ivgui_); }

    csr_surface_panel_fns_t OriginalPanels::PanelFunctions()
    {
        return {
            [](void* p, void* u) { auto& o = Owner(u); return o.visible_(o.panels_, Handle(p)) ? 1 : 0; },
            [](void* p, void* u) { auto& o = Owner(u); return o.popup_(o.panels_, Handle(p)) ? 1 : 0; },
            [](void* p, void* u) { auto& o = Owner(u); return o.count_(o.panels_, Handle(p)); },
            [](void* p, int i, void* u) { auto& o = Owner(u); return Panel(o.child_(o.panels_, Handle(p), i)); },
            [](void* p, void* u) { auto& o = Owner(u); o.solve_(o.panels_, Handle(p)); },
            [](void* p, void* u) { auto& o = Owner(u); o.scheme_(o.panels_, Handle(p)); },
            [](void* p, int r, int f, void* u) { auto& o = Owner(u); o.paint_(o.panels_, Handle(p), r != 0, f != 0); },
            this,
        };
    }

    csr_surface_input_fns_t OriginalPanels::InputFunctions()
    {
        return {
            [](int x, int y, void* u) {
                auto& o = Owner(u);
                o.cursor_(o.input_, x, y);
                // InternalCursorMoved skips unchanged positions. Panels can
                // hide beneath a stationary cursor, so refresh focus too.
                o.focus_(o.input_, x, y);
            },
            [](int code, void* u) { auto& o = Owner(u); o.press_(o.input_, code); },
            [](int code, void* u) { auto& o = Owner(u); o.release_(o.input_, code); },
            [](void* u) { auto& o = Owner(u); o.dispatch_(o.ivgui_); },
            this,
        };
    }
}
