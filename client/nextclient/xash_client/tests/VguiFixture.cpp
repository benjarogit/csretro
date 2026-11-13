// Versioned opaque interfaces for the original-VGUI binding tests, not UI code.
#include "vgui_binding.h"
#include <cstring>

using Factory = csr::client::VguiBinding::Factory;
namespace
{
    int mode{};
    int init_count{};
    int shutdown_count{};
    int tokens[6]{};
    void* seen_surface{};
    void* seen_filesystem{};
}
class __attribute__((visibility("hidden"))) CVGui
{
public:
    bool Init(Factory* factories, int count);
    void Shutdown();
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

extern "C" void FixtureVguiConfigure(int value)
{
    mode = value;
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
    void* value{};
    if (std::strcmp(name, "VGUI_ivgui006") == 0) value = &vgui;
    const char* names[] = {"VGUI_Input004", "VGUI_Panel007", "VGUI_Scheme009", "VGUI_System009", "VGUI_Localize003", "KeyValues003"};
    for (int i = 0; i < 6; ++i)
        if (std::strcmp(name, names[i]) == 0) value = &tokens[i];
    if (mode == 1 && std::strcmp(name, "KeyValues003") == 0) value = nullptr;
    if (code) *code = value ? 0 : 1;
    return value;
}
