// Versioned opaque interfaces for the original-VGUI binding tests, not UI code.
#include "vgui_binding.h"
#include <cstring>
#include <cstdint>
#include <vector>
#include <unordered_map>

using Factory = csr::client::VguiBinding::Factory;
namespace
{
    int mode{};
    int init_count{};
    int shutdown_count{};
    int tokens[6]{};
    void* seen_surface{};
    void* seen_filesystem{};
    std::vector<int> events;
    int last_sdl_key{};
    Factory engine_factory{};
    void (*init_hook)();
    void (*shutdown_hook)();
    struct KeyboardPanel
    {
        unsigned parent;
        bool visible;
        bool keyboard;
        bool popup;
    };
    std::unordered_map<unsigned, KeyboardPanel> keyboard_panels;
    int keyboard_queries{};
    KeyboardPanel PanelState(unsigned panel)
    {
        auto found = keyboard_panels.find(panel);
        if (found != keyboard_panels.end())
        {
            return found->second;
        }
        return {panel == 17 || panel == 18 ? 16u : 0u, panel != 18, panel == 16 || panel == 17, panel == 17};
    }
}
class __attribute__((visibility("hidden"))) CVGui
{
public:
    bool Init(Factory* factories, int count);
    void Shutdown();
    void DispatchMessages();
    void RunFrame();
};
bool CVGui::Init(Factory* factories, int count)
{
    ++init_count;
    engine_factory = count > 0 && factories ? factories[0] : nullptr;
    if (count <= 0 || !factories)
        return false;
    for (int i = 0; i < count; ++i)
    {
        if (!factories[i]) return false;
        if (!seen_surface) seen_surface = factories[i]("VGUI_Surface026", nullptr);
        if (!seen_filesystem) seen_filesystem = factories[i]("VFileSystem009", nullptr);
    }
    if (init_hook) init_hook();
    return seen_surface && seen_filesystem && mode != 2;
}
void CVGui::Shutdown() { ++shutdown_count; if (shutdown_hook) shutdown_hook(); }
void CVGui::DispatchMessages() { events.push_back(900); }
void CVGui::RunFrame() { events.push_back(950); }

namespace vgui2
{
    enum MouseCode { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_4, MOUSE_5 };
    class __attribute__((visibility("hidden"))) VPanel
    {
    public:
        void GetAbsPos(int&, int&);
        void GetInset(int&, int&, int&, int&);
        void GetClipRect(int&, int&, int&, int&);
    };
    void VPanel::GetAbsPos(int& x, int& y) { x = 32; y = 48; }
    void VPanel::GetInset(int& l, int& t, int& r, int& b) { l = 1; t = 2; r = 3; b = 4; }
    void VPanel::GetClipRect(int& x0, int& y0, int& x1, int& y1) { x0 = 32; y0 = 48; x1 = 640; y1 = 480; }
}
class __attribute__((visibility("hidden"))) VPanelWrapper
{
public:
    bool IsVisible(unsigned);
    bool IsPopup(unsigned);
    bool IsKeyBoardInputEnabled(unsigned);
    unsigned GetParent(unsigned);
    int GetChildCount(unsigned);
    unsigned GetChild(unsigned, int);
    void Solve(unsigned);
    void PerformApplySchemeSettings(unsigned);
    void PaintTraverse(unsigned, bool, bool);
    void SetPopup(unsigned, bool);
    void SetMouseInputEnabled(unsigned, bool);
    void SetKeyBoardInputEnabled(unsigned, bool);
};
bool VPanelWrapper::IsVisible(unsigned p)
{
    ++keyboard_queries;
    return PanelState(p).visible;
}
bool VPanelWrapper::IsPopup(unsigned p)
{
    ++keyboard_queries;
    return PanelState(p).popup;
}
bool VPanelWrapper::IsKeyBoardInputEnabled(unsigned p)
{
    ++keyboard_queries;
    return PanelState(p).keyboard;
}
unsigned VPanelWrapper::GetParent(unsigned p)
{
    ++keyboard_queries;
    return PanelState(p).parent;
}
int VPanelWrapper::GetChildCount(unsigned p) { return p == 16 ? 2 : 0; }
unsigned VPanelWrapper::GetChild(unsigned p, int i) { return p == 16 && i >= 0 && i < 2 ? 17 + i : 0; }
void VPanelWrapper::Solve(unsigned p) { events.push_back(100 + p); }
void VPanelWrapper::PerformApplySchemeSettings(unsigned p) { events.push_back(200 + p); }
void VPanelWrapper::PaintTraverse(unsigned p, bool r, bool f) { events.push_back(300 + p); events.push_back(r); events.push_back(f); }
void VPanelWrapper::SetPopup(unsigned p, bool value)
{
    auto state = PanelState(p);
    state.popup = value;
    keyboard_panels[p] = state;
    events.push_back(1000 + p);
    events.push_back(value);
}
void VPanelWrapper::SetMouseInputEnabled(unsigned p, bool value) { events.push_back(2000 + p); events.push_back(value); }
void VPanelWrapper::SetKeyBoardInputEnabled(unsigned p, bool value)
{
    auto state = PanelState(p);
    state.keyboard = value;
    keyboard_panels[p] = state;
    events.push_back(3000 + p);
    events.push_back(value);
}
class __attribute__((visibility("hidden"))) CInputWin32
{
public:
    void InternalCursorMoved(int, int);
    void UpdateMouseFocus(int, int);
    void InternalMousePressed(vgui2::MouseCode);
    void InternalMouseReleased(vgui2::MouseCode);
};
void CInputWin32::InternalCursorMoved(int x, int y) { events.push_back(600); events.push_back(x); events.push_back(y); }
void CInputWin32::UpdateMouseFocus(int x, int y) { events.push_back(650); events.push_back(x); events.push_back(y); }
void CInputWin32::InternalMousePressed(vgui2::MouseCode code) { events.push_back(700 + code); }
void CInputWin32::InternalMouseReleased(vgui2::MouseCode code) { events.push_back(800 + code); }

extern "C" void FixtureVguiConfigure(int value)
{
    mode = value;
    keyboard_panels.clear();
    keyboard_queries = 0;
    last_sdl_key = 0;
    engine_factory = nullptr;
    init_hook = shutdown_hook = nullptr;
    events.clear();
    init_count = shutdown_count = 0;
    seen_surface = seen_filesystem = nullptr;
}
extern "C" int FixtureVguiInitCount() { return init_count; }
extern "C" int FixtureVguiShutdownCount() { return shutdown_count; }
extern "C" void* FixtureSeenSurface() { return seen_surface; }
extern "C" void* FixtureSeenFilesystem() { return seen_filesystem; }
extern "C" void* CreateInterface(const char* name, int* code)
{
    static CVGui vgui;
    static VPanelWrapper panels;
    static CInputWin32 input;
    void* value{};
    if (std::strcmp(name, "VGUI_ivgui006") == 0) value = &vgui;
    const char* names[] = {"VGUI_Input004", "VGUI_Panel007", "VGUI_Scheme009", "VGUI_System009", "VGUI_Localize003", "KeyValues003"};
    for (int i = 0; i < 6; ++i)
        if (std::strcmp(name, names[i]) == 0) value = &tokens[i];
    if (std::strcmp(name, "VGUI_Panel007") == 0) value = &panels;
    if (std::strcmp(name, "VGUI_InputInternal001") == 0) value = mode == 3 ? nullptr : &input;
    if (mode == 1 && std::strcmp(name, "KeyValues003") == 0) value = nullptr;
    if (code) *code = value ? 0 : 1;
    return value;
}

extern "C" int FixturePanelEventCount() { return static_cast<int>(events.size()); }
extern "C" int FixturePanelEvent(int index)
{
    return index >= 0 && index < static_cast<int>(events.size()) ? events[index] : -1;
}

extern "C" void FixtureVguiHooks(void (*init)(), void (*shutdown)())
{
    init_hook = init;
    shutdown_hook = shutdown;
}

extern "C" __attribute__((visibility("hidden"))) int KeyCode_VirtualKeyToVGUI(int key)
{
    last_sdl_key = key;
    return key ? 500 : 0;
}
extern "C" int FixtureLastSdlKey() { return last_sdl_key; }
extern "C" void* FixtureQueryGameUiFuncs(int* code)
{
    if (!engine_factory) { if (code) *code = 1; return nullptr; }
    return engine_factory("VENGINE_GAMEUIFUNCS_VERSION001", code);
}

extern "C" void FixtureKeyboardPanel(unsigned handle, unsigned parent, int visible, int keyboard, int popup)
{
    keyboard_panels[handle] = {parent, visible != 0, keyboard != 0, popup != 0};
}
extern "C" int FixtureKeyboardQueryCount()
{
    return keyboard_queries;
}
