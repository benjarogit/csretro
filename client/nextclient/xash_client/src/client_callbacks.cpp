#include "client_callbacks.h"

#include <dlfcn.h>

namespace csr::client
{
    int RedrawWithVgui(goldsrc::HUD_REDRAW_FUNC original, float time, int intermission, UiAction after, void* owner)
    {
        if (!original) return 0;
        const int result = original(time, intermission);
        if (result && after) after(owner);
        return result;
    }

    void FrameWithVgui(goldsrc::HUD_FRAME_FUNC original, double time, UiAction after, void* owner)
    {
        if (!original) return;
        original(time);
        if (after) after(owner);
    }

    void MouseWithVgui(goldsrc::HUD_IN_MOUSEEVENT_FUNC original, int buttons, UiAction before, UiMouse after, void* owner)
    {
        if (!original) return;
        if (before) before(owner);
        original(buttons);
        if (after) after(owner, buttons);
    }

    void ResetInputWithVgui(goldsrc::HUD_IN_CLEARSTATES_FUNC original, UiAction after, void* owner)
    {
        if (!original) return;
        original();
        if (after) after(owner);
    }

    void MouseButtonState::Update(int buttons, Emit emit, void* owner)
    {
        const auto revision = ++revision_;
        const auto next = static_cast<unsigned>(buttons);
        for (int button = 0; button < button_count; ++button)
        {
            const unsigned bit = 1u << button;
            const bool down = (next & bit) != 0;
            if (down == ((held_ & bit) != 0)) continue;
            if (down) held_ |= bit; else held_ &= ~bit;
            if (emit) emit(owner, button, down);
            if (revision_ != revision) return;
        }
    }

    void MouseButtonState::Clear(Emit emit, void* owner)
    {
        const auto revision = ++revision_;
        for (int button = 0; button < button_count; ++button)
        {
            const unsigned bit = 1u << button;
            if (!(held_ & bit)) continue;
            held_ &= ~bit;
            if (emit) emit(owner, button, 0);
            if (revision_ != revision) return;
        }
    }

    int InitializeWithVgui(goldsrc::INITIALIZE_FUNC original, goldsrc::cl_enginefunc_t* engine,
                           int version, BindVgui bind, void* owner)
    {
        if (!original || !bind)
            return 0;
        const int result = original(engine, version);
        if (!result)
            return result;
        return bind(owner, engine) ? result : 0;
    }

    bool ReadCallbacks(void* module, goldsrc::cldll_func_t& out, std::string& error)
    {
        out = {};
        error.clear();
        if (!module)
        {
            error = "Original client module is not loaded";
            return false;
        }
        using FillCallbacks = void (*)(goldsrc::cldll_func_t*);
        auto fill = reinterpret_cast<FillCallbacks>(dlsym(module, "F"));
        if (!fill)
        {
            error = "Original client has no F callback export";
            return false;
        }

        goldsrc::cldll_func_t candidate{};
        fill(&candidate);

        // These callbacks are required by the Xash game loader. Optional tail
        // entries remain exactly as supplied by the original library.
#define REQUIRE_CALLBACK(field) \
        if (!candidate.field) { error = "Original client is missing " #field; return false; }
        REQUIRE_CALLBACK(pInitFunc)
        REQUIRE_CALLBACK(pHudInitFunc)
        REQUIRE_CALLBACK(pHudVidInitFunc)
        REQUIRE_CALLBACK(pHudRedrawFunc)
        REQUIRE_CALLBACK(pHudUpdateClientDataFunc)
        REQUIRE_CALLBACK(pHudResetFunc)
        REQUIRE_CALLBACK(pClientMove)
        REQUIRE_CALLBACK(pClientMoveInit)
        REQUIRE_CALLBACK(pClientTextureType)
        REQUIRE_CALLBACK(pIN_ActivateMouse)
        REQUIRE_CALLBACK(pIN_DeactivateMouse)
        REQUIRE_CALLBACK(pIN_MouseEvent)
        REQUIRE_CALLBACK(pIN_ClearStates)
        REQUIRE_CALLBACK(pIN_Accumulate)
        REQUIRE_CALLBACK(pCL_CreateMove)
        REQUIRE_CALLBACK(pCL_IsThirdPerson)
        REQUIRE_CALLBACK(pCL_GetCameraOffsets)
        REQUIRE_CALLBACK(pFindKey)
        REQUIRE_CALLBACK(pCamThink)
        REQUIRE_CALLBACK(pCalcRefdef)
        REQUIRE_CALLBACK(pAddEntity)
        REQUIRE_CALLBACK(pCreateEntities)
        REQUIRE_CALLBACK(pDrawNormalTriangles)
        REQUIRE_CALLBACK(pDrawTransparentTriangles)
        REQUIRE_CALLBACK(pStudioEvent)
        REQUIRE_CALLBACK(pPostRunCmd)
        REQUIRE_CALLBACK(pShutdown)
        REQUIRE_CALLBACK(pTxferLocalOverrides)
        REQUIRE_CALLBACK(pProcessPlayerState)
        REQUIRE_CALLBACK(pTxferPredictionData)
        REQUIRE_CALLBACK(pReadDemoBuffer)
        REQUIRE_CALLBACK(pConnectionlessPacket)
        REQUIRE_CALLBACK(pGetHullBounds)
        REQUIRE_CALLBACK(pHudFrame)
        REQUIRE_CALLBACK(pKeyEvent)
        REQUIRE_CALLBACK(pTempEntUpdate)
        REQUIRE_CALLBACK(pGetUserEntity)
#undef REQUIRE_CALLBACK

        out = candidate;
        return true;
    }
}
