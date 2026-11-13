#include "client_callbacks.h"

#include <dlfcn.h>

namespace csr::client
{
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
