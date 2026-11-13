// Synthetic callback implementations for ABI forwarding tests, not CS gameplay.
#include <goldsrc/client_api.h>
#include <goldsrc/pmove.h>
#include <goldsrc/usercmd.h>
#include <type_traits>

namespace
{
    int missing{};
    int hud_init_count{};
    int shutdown_count{};
    template <typename T> struct Default;
    template <typename R, typename... Args> struct Default<R (*)(Args...)>
    {
        static R Call(Args...) { if constexpr (!std::is_void_v<R>) return R{}; }
    };
    int Initialize(goldsrc::cl_enginefunc_t* engine, int version)
    {
        return version + (engine ? 1000 : 0);
    }
    void HudInit() { ++hud_init_count; }
    void Shutdown() { ++shutdown_count; }
    int Redraw(float time, int intermission) { return int(time * 100) + intermission; }
    void Move(goldsrc::playermove_t* move, goldsrc::qboolean server)
    {
        move->velocity[0] += move->frametime * (server ? 80.f : 40.f);
        move->server = server;
    }
    void CreateMove(float time, goldsrc::usercmd_t* command, int active)
    {
        command->forwardmove += time * active;
    }
}
extern "C" void FixtureConfigure(int value)
{
    missing = value;
    hud_init_count = shutdown_count = 0;
}
extern "C" int FixtureHudInitCount() { return hud_init_count; }
extern "C" int FixtureShutdownCount() { return shutdown_count; }
extern "C" void F(goldsrc::cldll_func_t* out)
{
    *out = {};
    out->pInitFunc = &Default<decltype(out->pInitFunc)>::Call;
    out->pHudInitFunc = &Default<decltype(out->pHudInitFunc)>::Call;
    out->pHudVidInitFunc = &Default<decltype(out->pHudVidInitFunc)>::Call;
    out->pHudRedrawFunc = &Default<decltype(out->pHudRedrawFunc)>::Call;
    out->pHudUpdateClientDataFunc = &Default<decltype(out->pHudUpdateClientDataFunc)>::Call;
    out->pHudResetFunc = &Default<decltype(out->pHudResetFunc)>::Call;
    out->pClientMove = &Default<decltype(out->pClientMove)>::Call;
    out->pClientMoveInit = &Default<decltype(out->pClientMoveInit)>::Call;
    out->pClientTextureType = &Default<decltype(out->pClientTextureType)>::Call;
    out->pIN_ActivateMouse = &Default<decltype(out->pIN_ActivateMouse)>::Call;
    out->pIN_DeactivateMouse = &Default<decltype(out->pIN_DeactivateMouse)>::Call;
    out->pIN_MouseEvent = &Default<decltype(out->pIN_MouseEvent)>::Call;
    out->pIN_ClearStates = &Default<decltype(out->pIN_ClearStates)>::Call;
    out->pIN_Accumulate = &Default<decltype(out->pIN_Accumulate)>::Call;
    out->pCL_CreateMove = &Default<decltype(out->pCL_CreateMove)>::Call;
    out->pCL_IsThirdPerson = &Default<decltype(out->pCL_IsThirdPerson)>::Call;
    out->pCL_GetCameraOffsets = &Default<decltype(out->pCL_GetCameraOffsets)>::Call;
    out->pFindKey = &Default<decltype(out->pFindKey)>::Call;
    out->pCamThink = &Default<decltype(out->pCamThink)>::Call;
    out->pCalcRefdef = &Default<decltype(out->pCalcRefdef)>::Call;
    out->pAddEntity = &Default<decltype(out->pAddEntity)>::Call;
    out->pCreateEntities = &Default<decltype(out->pCreateEntities)>::Call;
    out->pDrawNormalTriangles = &Default<decltype(out->pDrawNormalTriangles)>::Call;
    out->pDrawTransparentTriangles = &Default<decltype(out->pDrawTransparentTriangles)>::Call;
    out->pStudioEvent = &Default<decltype(out->pStudioEvent)>::Call;
    out->pPostRunCmd = &Default<decltype(out->pPostRunCmd)>::Call;
    out->pShutdown = &Default<decltype(out->pShutdown)>::Call;
    out->pTxferLocalOverrides = &Default<decltype(out->pTxferLocalOverrides)>::Call;
    out->pProcessPlayerState = &Default<decltype(out->pProcessPlayerState)>::Call;
    out->pTxferPredictionData = &Default<decltype(out->pTxferPredictionData)>::Call;
    out->pReadDemoBuffer = &Default<decltype(out->pReadDemoBuffer)>::Call;
    out->pConnectionlessPacket = &Default<decltype(out->pConnectionlessPacket)>::Call;
    out->pGetHullBounds = &Default<decltype(out->pGetHullBounds)>::Call;
    out->pHudFrame = &Default<decltype(out->pHudFrame)>::Call;
    out->pKeyEvent = &Default<decltype(out->pKeyEvent)>::Call;
    out->pTempEntUpdate = &Default<decltype(out->pTempEntUpdate)>::Call;
    out->pGetUserEntity = &Default<decltype(out->pGetUserEntity)>::Call;
    out->pVoiceStatus = &Default<decltype(out->pVoiceStatus)>::Call;
    out->pDirectorMessage = &Default<decltype(out->pDirectorMessage)>::Call;
    out->pStudioInterface = &Default<decltype(out->pStudioInterface)>::Call;
    out->pChatInputPosition = &Default<decltype(out->pChatInputPosition)>::Call;
    out->pGetPlayerTeam = &Default<decltype(out->pGetPlayerTeam)>::Call;
    out->pClientFactory = &Default<decltype(out->pClientFactory)>::Call;

    out->pInitFunc = missing == 1 ? nullptr : Initialize;
    out->pHudInitFunc = HudInit;
    out->pHudRedrawFunc = Redraw;
    out->pClientMove = missing == 2 ? nullptr : Move;
    out->pCL_CreateMove = CreateMove;
    out->pShutdown = missing == 3 ? nullptr : Shutdown;
    if (missing == 4)
    {
        out->pVoiceStatus = nullptr;
        out->pDirectorMessage = nullptr;
        out->pStudioInterface = nullptr;
        out->pChatInputPosition = nullptr;
        out->pGetPlayerTeam = nullptr;
        out->pClientFactory = nullptr;
    }
}
