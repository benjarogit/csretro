// Versioned opaque interfaces for the original-VGUI binding tests, not UI code.
#include "vgui_binding.h"
#include <cstring>
#include <cstdint>
#include <vector>

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
    if (count != 2 || !factories)
        return false;
    seen_surface = factories[0]("VGUI_Surface026", nullptr);
    seen_filesystem = factories[0]("VFileSystem009", nullptr);
    return seen_surface && seen_filesystem && mode != 2;
}
void CVGui::Shutdown() { ++shutdown_count; }
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
    int GetChildCount(unsigned);
    unsigned GetChild(unsigned, int);
    void Solve(unsigned);
    void PerformApplySchemeSettings(unsigned);
    void PaintTraverse(unsigned, bool, bool);
    void SetPopup(unsigned, bool);
    void SetMouseInputEnabled(unsigned, bool);
    void SetKeyBoardInputEnabled(unsigned, bool);
};
bool VPanelWrapper::IsVisible(unsigned p) { return p != 18; }
bool VPanelWrapper::IsPopup(unsigned p) { return p == 17; }
int VPanelWrapper::GetChildCount(unsigned p) { return p == 16 ? 2 : 0; }
unsigned VPanelWrapper::GetChild(unsigned p, int i) { return p == 16 && i >= 0 && i < 2 ? 17 + i : 0; }
void VPanelWrapper::Solve(unsigned p) { events.push_back(100 + p); }
void VPanelWrapper::PerformApplySchemeSettings(unsigned p) { events.push_back(200 + p); }
void VPanelWrapper::PaintTraverse(unsigned p, bool r, bool f) { events.push_back(300 + p); events.push_back(r); events.push_back(f); }
void VPanelWrapper::SetPopup(unsigned p, bool value) { events.push_back(1000 + p); events.push_back(value); }
void VPanelWrapper::SetMouseInputEnabled(unsigned p, bool value) { events.push_back(2000 + p); events.push_back(value); }
void VPanelWrapper::SetKeyBoardInputEnabled(unsigned p, bool value) { events.push_back(3000 + p); events.push_back(value); }
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
