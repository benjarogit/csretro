#include "engine.h"

#include <easylogging++.h>
#include <xash_engine/xash_engine.h>

#include "text/text_surface.h"

namespace
{

    template <class Function>
    bool HasRetailHook(const Function& function)
    {
        return function.GetHook() != nullptr;
    }

} // namespace

void* VGUI2_Surface()
{
    nitroapi::EngineData* engine = eng();
    if (engine != nullptr && engine->vgui2_surface != nullptr && *engine->vgui2_surface != nullptr)
    {
        return *engine->vgui2_surface;
    }
    const csr_xash_engine_surface_t* bound = csr_xash_engine_surface();
    return bound != nullptr ? bound->object : nullptr;
}

void VGUI2_Draw_Init()
{
    if (HasRetailHook(eng()->VGUI2_Draw_Init))
    {
        eng()->VGUI2_Draw_Init();
        return;
    }
    csr_xash_engine_draw_init();
}

void VGUI2_ResetCurrentTexture()
{
    if (HasRetailHook(eng()->VGUI2_ResetCurrentTexture))
    {
        eng()->VGUI2_ResetCurrentTexture();
        return;
    }
    csr_xash_engine_reset_texture();
}

void VGUI2_Draw_SetTextColor(int r, int g, int b)
{
    if (HasRetailHook(eng()->VGUI2_Draw_SetTextColor))
    {
        eng()->VGUI2_Draw_SetTextColor(r, g, b);
        return;
    }
    csr_xash_engine_set_text_color(r, g, b);
}

// The engine keeps its own pointer to its surface, and so does stock client.dll, which resolves it by
// name and draws the scoreboard, the buy menu and the rest of the in-game panels through it. Neither
// pointer is ours to redirect, so the text slots of the object they share are replaced instead.
void VGUI2_InstallTextSurface()
{
#ifdef _WIN32
    void* surface = eng()->vgui2_surface ? *eng()->vgui2_surface : nullptr;

    if (!surface)
    {
        LOG(ERROR) << "The engine's vgui2 surface was not found, its text path stays as it is";
        return;
    }

    TextSurface_Install(surface);
#endif
}

// Undo of VGUI2_InstallTextSurface, for the engine shutdown that precedes unloading this module.
void VGUI2_UninstallTextSurface()
{
#ifdef _WIN32
    TextSurface_Uninstall();
#endif
}
