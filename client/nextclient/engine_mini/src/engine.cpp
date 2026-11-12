#include "engine.h"

#include <cstdio>

#include <IGameConsole.h>
#include <Registry.h>
#include <easylogging++.h>
#include <graphics/gl_draw.h>
#include <cvars/cvar_defaults.h>
#include <next_engine_mini/engine_mini.h>
#include <nitro_utils/PtrValidator.h>
#include <nitro_utils/poor_reflection_utils.h>
#include <nitro_utils/string_utils.h>
#include <service/matchmaking/MatchmakingService.h>
#include <service/matchmaking/MatchmakingSteamComp.h>
#include <service/discord/DiscordPresence.h>
#include <taskcoro/TaskCoro.h>
#include <taskcoro/impl/TaskCoroImpl.h>
#include <tier2/tier2.h>
#include <goldsrc/client_api.h>
#include <goldsrc/limits.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/cmd.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/engine/sound.h>
#include <goldsrc/engine/video.h>
#include <goldsrc/entity.h>
#include <goldsrc/keys.h>
#include <goldsrc/server_api.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/types.h>
#include <goldsrc/view.h>
#include "graphics/render/render_device.h"
#include "xash_startup.h"

#include <xash_engine/xash_engine.h>

#include "common/common.h"
#include "common/net_chan.h"
#include "common/net_ws.h"
#include "common/model.h"
#include "common/zone.h"
#include "common/host.h"
#include "common/sys_dll.h"
#include "graphics/gl_local.h"
#include "graphics/detailtexture.h"
#include "graphics/color_scheme.h"
#include "graphics/text/text_render.h"
#include "graphics/text/text_surface.h"
#include "graphics/vgui2.h"
#include "graphics/preview/preview.h"
#include "graphics/preview/preview_model.h"
#include "graphics/render/StudioIntercept.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/render/RendererLifecycle.h"
#include "client/client.h"
#include "client/cl_main.h"
#include "client/download.h"
#include "client/spriteapi.h"
#include "client/ncl_entity/cl_ncl_entity_sync.h"
#include "client/ncl_entity/cl_ncl_entity_sync_overlay.h"
#include "client/ncl_entity/WeaponSyncSystem.h"
#include "client/ncl_entity/MaterialSyncSystem.h"
#include "client/cl_private_resources.h"
#include "client/cl_parsefn.h"
#include "client/cl_game.h"
#include "client/cl_string_registry.h"
#include "graphics/render/material/material_edit.h"
#include "graphics/render/render_state.h"
#include "graphics/render/render_capture.h"
#include "graphics/render/material/ServerMaterials.h"
#include "binding/jsapi/jsapi.h"
#include "common/cvar.h"
#include "console/console.h"
#include "console/protector.h"
#include "cstrike_hack.h"
#include "client/cl_sound.h"
#include "client/ncl_entity/PlayerSyncSystem.h"
#include "performance/performance_integration.h"
#include "performance/PerformanceMonitor.h"
#include <ncl_perf/profiler.h>

#include <cstring>

#ifndef _WIN32
#include <dlfcn.h>
#endif

NextClientVersion g_NextClientVersion;

nitroapi::NitroApiInterface* g_NitroApi;
std::shared_ptr<nitro_utils::ConfigProviderInterface> g_SettingGuard;
std::shared_ptr<nitro_utils::ConfigProviderInterface> g_UserConfig;
IRegistry* registry;
IFileSystem* g_pFileSystem;
IFileSystemNext* g_pFileSystemNext;
AnalyticsInterface* g_Analytics;
IGameUI* g_pGameUi;
IGameConsole* g_GameConsole;
IGameConsoleNext* g_GameConsoleNext;
vgui2::ILocalize* g_pLocalize;

goldsrc::cl_enginefunc_t gEngfuncs;
goldsrc::enginefuncs_t g_engfuncs;
goldsrc::modfuncs_t g_modfuncs;
goldsrc::sizebuf_t* net_message;
goldsrc::netadr_t* net_from;
int* pMsg_readcount;
double* realtime;
double* host_frametime;
goldsrc::qboolean* gfExtendedError;
char* gszDisconnectReason;
char* gDownloadFile;
goldsrc::client_state_t* cl;
client_stateex_t client_stateex;
goldsrc::cmd_source_t* cmd_source;
goldsrc::cldll_func_t cl_funcs;
goldsrc::client_static_t* cls;
goldsrc::server_static_t* g_psvs;
goldsrc::server_t* g_psv;
goldsrc::qboolean* g_bMajorMapChange;
goldsrc::CareerStateType* p_g_careerState;
goldsrc::model_t** loadmodel;
int* cl_playerindex;
goldsrc::cl_entity_t** p_cl_entities;
bool* p_noclip_anglehack;
goldsrc::cache_user_t** p_loadcache;
unsigned char** p_loadbuf;
int* p_loadsize;
goldsrc::qboolean* p_host_initialized;
goldsrc::cl_enginefunc_dst_t* p_g_engdstAddrs;
goldsrc::viddef_t* p_vid;
int* p_currenttexture;
goldsrc::cl_entity_t** p_currententity;
float* p_r_blend;
vec3_t* p_r_entorigin;
goldsrc::qboolean* p_g_bUserFogOn;
int* p_numTransObjs;
goldsrc::transObjRef** p_transObjects;
goldsrc::qboolean* p_mtexenabled;
goldsrc::cl_entity_t* p_r_worldentity;
goldsrc::qboolean* p_mirror;
int* p_c_brush_polys;
int* p_c_alias_polys;
int* p_cl_numvisedicts;
goldsrc::cl_entity_t** p_cl_visedicts;
goldsrc::qboolean* p_isFogEnabled;
goldsrc::refdef_t* r_refdef;
float* p_g_SND_VoiceOverdrive;
int* p_cszrawsentences;
char* (*p_rgpszrawsentence)[goldsrc::CVOXFILESENTENCEMAX];
float* p_scr_fov_value;
goldsrc::dlight_t (*p_cl_elights)[kMaxElights];
float* p_scr_con_current;
goldsrc::keydest_t* p_key_dest;
goldsrc::sfx_t** p_known_sfx;
int* p_num_sfx;
int* p_gHostSpawnCount;
goldsrc::netadr_t* p_net_local_adr;
int* p_ip_sockets;
int* p_ipx_sockets;
goldsrc::netadr_t* p_g_GameServerAddress;
float* p_g_LastScreenUpdateTime;
int* p_maxTransObjs;

goldsrc::cvar_t* fs_startup_timings;
goldsrc::cvar_t* fs_lazy_precache;
goldsrc::cvar_t* fs_precache_timings;
goldsrc::cvar_t* fs_perf_warnings;
goldsrc::cvar_t* cl_download_ingame;
goldsrc::cvar_t* cl_shownet;
goldsrc::cvar_t* cl_timeout;
goldsrc::cvar_t* scr_downloading;
goldsrc::cvar_t* host_framerate;
goldsrc::cvar_t* sys_ticrate;
goldsrc::cvar_t* gl_vsync;
goldsrc::cvar_t* snd_show;
goldsrc::cvar_t* chase_active;
goldsrc::cvar_t* fps_override;
goldsrc::cvar_t* fps_max;
goldsrc::cvar_t* sys_timescale;
goldsrc::cvar_t* developer;
goldsrc::cvar_t* sv_cheats;
goldsrc::cvar_t* gl_dither_cvar;
goldsrc::cvar_t* gl_spriteblend;
goldsrc::cvar_t* r_drawentities;
goldsrc::cvar_t* r_norefresh;
goldsrc::cvar_t* r_speeds;
goldsrc::cvar_t* viewmodel_fov;

bool g_bIsDedicatedServer;
goldsrc::r_studio_interface_t* pStudioAPI;

static std::unique_ptr<service::matchmaking::MatchmakingSteamComp> g_pMatchmakingServers;

static std::shared_ptr<taskcoro::TaskCoroImpl> g_pTaskCoroImpl;
static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsubs;

nitroapi::NitroApiInterface* napi() { return g_NitroApi; }
nitroapi::EngineData* eng() { return g_NitroApi->GetEngineData(); }
nitroapi::ClientData* client() { return g_NitroApi->GetClientData(); }
nitroapi::SDL2Data* sdl2() { return g_NitroApi->GetSDL2Data(); }

NclmVerificatorInterface* g_NclmVerificator;

static void EngineMiniStopWorkers()
{
    g_pMatchmakingServers = nullptr;
    taskcoro::TaskCoro::UnInitialize();
    g_pTaskCoroImpl = nullptr;
}

static void EngineMiniUninitialize()
{
    Preview_UnbindEngine();
    performance::runtime::Shutdown();
    CL_DeleteHttpDownloadManager();
    KV_UninitializeKeyValuesSystem();

#ifdef _WIN32
    JSAPI_Shutdown();
#endif

    // MultiSourceQuery's thread only exits once its owner is destroyed, and
    // TaskCoro::UnInitialize waits for every thread it runs
    EngineMiniStopWorkers();

    g_NitroApi = nullptr;
    g_SettingGuard = nullptr;
    g_UserConfig = nullptr;
    g_pFileSystem = nullptr;
    g_pFileSystemNext = nullptr;
    g_pGameUi = nullptr;
    g_GameConsole = nullptr;
    g_GameConsoleNext = nullptr;
    g_pLocalize = nullptr;

    Q_memset(&gEngfuncs, 0, sizeof(gEngfuncs));
    Q_memset(&g_engfuncs, 0, sizeof(g_engfuncs));
    net_message = nullptr;
    net_from = nullptr;
    pMsg_readcount = nullptr;
    realtime = nullptr;
    host_frametime = nullptr;
    gfExtendedError = nullptr;
    gszDisconnectReason = nullptr;
    gDownloadFile = nullptr;
    cl = nullptr;
    cmd_source = nullptr;
    Q_memset(&cl_funcs, 0, sizeof(cl_funcs));
    cls = nullptr;
    g_psvs = nullptr;
    g_psv = nullptr;
    g_bMajorMapChange = nullptr;
    p_g_careerState = nullptr;
    loadmodel = nullptr;
    cl_playerindex = nullptr;
    p_loadcache = nullptr;
    p_loadbuf = nullptr;
    p_loadsize = nullptr;
    p_host_initialized = nullptr;
    p_g_engdstAddrs = nullptr;
    p_vid = nullptr;
    p_currenttexture = nullptr;
    p_currententity = nullptr;
    p_r_blend = nullptr;
    p_r_entorigin = nullptr;
    p_g_bUserFogOn = nullptr;
    p_numTransObjs = nullptr;
    p_transObjects = nullptr;
    p_mtexenabled = nullptr;
    p_r_worldentity = nullptr;
    p_mirror = nullptr;
    p_c_brush_polys = nullptr;
    p_c_alias_polys = nullptr;
    p_cl_numvisedicts = nullptr;
    p_cl_visedicts = nullptr;
    p_isFogEnabled = nullptr;
    r_refdef = nullptr;
    p_g_SND_VoiceOverdrive = nullptr;
    p_cszrawsentences = nullptr;
    p_rgpszrawsentence = nullptr;
    p_cl_elights = nullptr;
    p_scr_fov_value = nullptr;
    p_scr_con_current = nullptr;
    p_key_dest = nullptr;
    p_known_sfx = nullptr;
    p_num_sfx = nullptr;
    p_gHostSpawnCount = nullptr;
    p_net_local_adr = nullptr;
    p_ip_sockets = nullptr;
    p_ipx_sockets = nullptr;
    p_g_GameServerAddress = nullptr;
    p_g_LastScreenUpdateTime = nullptr;
    p_maxTransObjs = nullptr;

    fs_startup_timings = nullptr;
    fs_lazy_precache = nullptr;
    fs_precache_timings = nullptr;
    cl_download_ingame = nullptr;
    cl_shownet = nullptr;
    cl_timeout = nullptr;
    scr_downloading = nullptr;
    host_framerate = nullptr;
    sys_ticrate = nullptr;
    gl_vsync = nullptr;
    fps_override = nullptr;
    fps_max = nullptr;
    sys_timescale = nullptr;
    developer = nullptr;
    sv_cheats = nullptr;
    gl_spriteblend = nullptr;
    r_drawentities = nullptr;
    r_norefresh = nullptr;
    r_speeds = nullptr;

    p_cl_entities = nullptr;
    p_noclip_anglehack = nullptr;

    p_cmd_argc = nullptr;
    p_cmd_argv = nullptr;
    p_cvar_vars = nullptr;
}

static void EngineMiniInitialize(nitroapi::NitroApiInterface* nitro_api, NextClientVersion next_client_version, AnalyticsInterface* analytics)
{
    ConfigureEngineElppLogger();

    g_pTaskCoroImpl = std::make_shared<taskcoro::TaskCoroImpl>(std::this_thread::get_id());
    taskcoro::TaskCoro::Initialize(g_pTaskCoroImpl);

    g_NitroApi = nitro_api;
    performance::runtime::Bind(nitro_api);
    g_NextClientVersion = next_client_version;
    g_Analytics = analytics;

    g_pMatchmakingServers = std::make_unique<service::matchmaking::MatchmakingSteamComp>();
    
    registry = new CRegistry("Software\\Valve\\Half-Life\\Settings");
    registry->Init();
}

static void OnGameUninitializing()
{
    render::GetLifecycle().BeginShutdown();

    if (g_Analytics)
        g_Analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " OnGameUninitializing");
    
    // Notify the server of disconnect when the game is closed via the window's X button.
    if (cls && (cls->state == goldsrc::ca_connected || cls->state == goldsrc::ca_active || cls->state == goldsrc::ca_uninitialized ||
                cls->state == goldsrc::ca_connecting))
        CL_Disconnect();

    if (g_pTaskCoroImpl)
        g_pTaskCoroImpl->ShutdownUpdateExecutor();

    render::GetLifecycle().ReleaseLiveContext();
    performance::runtime::Shutdown();

    DiscordPresence_Shutdown();
    PROTECTOR_Shutdown();
    CL_CvarsSandboxShutdown();
    CL_StringRegistryShutdown();
    CL_NclEntitySyncOverlayShutdown();
    CL_NclEntitySyncShutdown();

    for (auto &unsubscriber : g_Unsubs)
        unsubscriber->Unsubscribe();
    g_Unsubs.clear();
}

namespace
{
    bool XashStartupIsActive();
    bool SupplyXashEngineTable();
    void RecordXashNetSendPacket();
    void RecordXashNetchanCopyFileFragments();
    void RecordXashSvcTimeScale();
    void RecordXashSvcSendCvarValue();
    void RecordXashHttpUpdate();
    void RecordXashHttpStop();
    void RecordXashStartResourceDownloading();
    void RecordXashReadPackets();
    void RecordXashRequestMissingResources();
    void RecordXashDisconnect();
    void RecordXashClearState();
    void RecordXashHostMap();
    void RecordXashHostFilterTime();
    void RecordXashModClearAll();
    void RecordXashModFindName();
    void RecordXashModPrint();
    void RecordXashModValidateCRC();
    void RecordXashModNeedCRC();
    void RecordXashModLoadModel();
    void RecordXashModForName();
    void RecordXashModExtradata();
    void RecordXashSprInit();
    void RecordXashSprShutdown();
    void RecordXashSprShutdownNoModelFree();
    void RecordXashSprLoad();
    void RecordXashSprDraw();
    void RecordXashSprDrawAdditive();
    void RecordXashSprDrawHoles();
    void RecordXashSprDrawGeneric();
    void RecordXashSprEnableScissor();
    void RecordXashSprDisableScissor();
    void RecordXashSprSet();
    void RecordXashSprWidth();
    void RecordXashSprHeight();
    void RecordXashSprFrames();
    void RecordXashSprGetModelPointer();
    void RecordXashRStudioReloadSkin();
    void RecordXashSetCrosshair();
    void RecordXashDrawCrosshair();
    void RecordXashRRenderView();
    void RecordXashGLInit();
    void RecordXashGLConfig();
    void RecordXashGLSelectTexture();
    void RecordXashCLConnectClient();
    void RecordXashCLClearClientState();
    void RecordXashGLLoadTexture();
    void RecordXashGLLoadTexture2();
    void RecordXashGLUnloadTextures();
    void RecordXashGLUnloadTexture();
    void RecordXashGLBind();
    void RecordXashDrawInit();
    void RecordXashDrawShutdown();
    void RecordXashDrawPicFromWad();
    void RecordXashUpdatePalette();
}

static void OnGameInitializing(void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result)
{
    // The Xash table is storage in this process. Null function pointers in it
    // are not calls, and a missing pointer is not Sys_Error. Con_Init runs
    // once that table is stored. SetCStrikeFlags runs only when pfnGetGameDir
    // is set. NET_SendPacket is then the process function when that hook is
    // absent. This init stores the pointer and does not send.
    // Netchan_CopyFileFragments is then the process function when that hook
    // is absent. This init stores the pointer and does not copy a fragment.
    // SVC_TimeScale is then the process function when that hook is absent.
    // This init stores the pointer and does not change the timescale.
    // SVC_SendCvarValue is then the process function when that hook is absent.
    // This init stores the pointer and does not send a cvar.
    // CL_HTTPUpdate is then the process function when that hook is absent.
    // This init stores the pointer and does not update HTTP.
    // CL_HTTPStop_f is then the process function when that hook is absent.
    // This init stores the pointer and does not stop a download.
    // CL_StartResourceDownloading is then the process function when that hook
    // is absent. This init stores the pointer and does not start a download.
    // CL_ReadPackets is then the process function when that hook is absent.
    // This init stores the pointer and does not read a packet.
    // CL_RequestMissingResources is then the process function when that hook
    // is absent. This init stores the pointer and does not request a resource.
    // CL_Disconnect is then the process function when that hook is absent.
    // This init stores the pointer and does not disconnect. The retail body
    // reads client state and can transmit, so a real call does not run.
    // CL_ClearState is then the process function when that hook is absent.
    // This init stores the pointer and does not clear client state. The
    // process body forwards into the retail clear, so a real call does not run.
    // Host_Map_f is then the process function when that hook is absent.
    // This init stores the pointer and does not load a map. The process body
    // disconnects and starts a map, so a real call does not run.
    // Host_FilterTime is then the process function when that hook is absent.
    // This init stores the pointer and does not filter time. The process body
    // advances host time and the frame interval, so a real call does not run.
    // Mod_ClearAll is then the process function when that hook is absent.
    // This init stores the pointer and does not clear models. The process body
    // marks known models unreferenced and drops sprite cache, so a real call
    // does not run.
    // Mod_FindName is then the process function when that hook is absent.
    // This init stores the pointer and does not look up a model. The process
    // body searches the known list and can allocate a slot, so a real call
    // does not run.
    // Mod_Print is then the process function when that hook is absent.
    // This init stores the pointer and does not print the model list. The
    // process body writes that list through Con_Printf, so a real call does
    // not run.
    // Mod_ValidateCRC is then the process function when that hook is absent.
    // This init stores the pointer and does not validate a CRC. The process
    // body looks up a model and can reject a stored CRC, so a real call does
    // not run.
    // Mod_NeedCRC is then the process function when that hook is absent.
    // This init stores the pointer and does not mark a CRC. The process body
    // looks up a model and writes the need-CRC flag, so a real call does not
    // run.
    // Mod_LoadModel is then the process function when that hook is absent.
    // This init stores the pointer and does not load a model. The process body
    // reads a model file and can write a CRC and needload, so a real call does
    // not run.
    // Mod_ForName is then the process function when that hook is absent.
    // This init stores the pointer and does not look up a model. The process
    // body searches the known list and can load a model, so a real call does
    // not run.
    // Mod_Extradata is then the process function when that hook is absent.
    // This init stores the pointer and does not read extradata. The process
    // body can Sys_Error on a brush model and can load a model, so a real call
    // does not run.
    // SPR_Init is then the process function when that hook is absent.
    // This init stores the pointer and does not allocate the sprite list. The
    // process body allocates that list when it is empty, so a real call does
    // not run.
    // SPR_Shutdown is then the process function when that hook is absent.
    // This init stores the pointer and does not free the sprite list. The
    // process body frees that list when it is allocated and can unload sprite
    // textures, so a real call does not run.
    // SPR_Shutdown_NoModelFree is then the process function when that hook is absent.
    // This init stores the pointer and does not free the sprite list or the sprite names.
    // The process body frees that list and those names when the list is allocated and
    // does not unload sprite textures, so a real call does not run.
    // SPR_Load is then the process function when that hook is absent.
    // This init stores the pointer and does not load a sprite or allocate a sprite name.
    // The process body loads a named sprite when the list is allocated, can allocate a
    // sprite name, can call Mod_ForName, and can Sys_Error when the list is full, so a
    // real call does not run.
    // SPR_Draw is then the process function when that hook is absent.
    // This init stores the pointer and does not draw a sprite. The process body draws
    // the current sprite frame when a sprite is current and the point is inside the
    // video image, and can call Draw_SpriteFrame, so a real call does not run.
    // SPR_DrawAdditive is then the process function when that hook is absent.
    // This init stores the pointer and does not draw a sprite. The process body draws
    // the current sprite additively when a sprite is current and the point is inside
    // the video image, and can call Draw_SpriteFrameAdditive, so a real call does not
    // run.
    // SPR_DrawHoles is then the process function when that hook is absent.
    // This init stores the pointer and does not draw a sprite. The process body draws
    // the current sprite with holes when a sprite is current and the point is inside
    // the video image, and can call Draw_SpriteFrameHoles, so a real call does not
    // run.
    // SPR_DrawGeneric is then the process function when that hook is absent.
    // This init stores the pointer and does not draw a sprite. The process body draws
    // the current sprite with the caller blend when a sprite is current and the point
    // is inside the video image, and can call Draw_SpriteFrameGeneric, so a real call
    // does not run.
    // SPR_EnableScissor is then the process function when that hook is absent.
    // This init stores the pointer and does not set a scissor rectangle. The process
    // body enables a scissor rectangle and can call EnableScissorTest, so a real call
    // does not run.
    // SPR_DisableScissor is then the process function when that hook is absent.
    // This init stores the pointer and does not clear a scissor rectangle. The process
    // body disables the scissor and can call DisableScissorTest, so a real call does
    // not run.
    // SPR_Set is then the process function when that hook is absent.
    // This init stores the pointer and does not set the current sprite or its color.
    // The process body looks up a sprite and can call qglColor4f, so a real call does
    // not run.
    // SPR_Width is then the process function when that hook is absent.
    // This init stores the pointer and does not read a sprite width. The process body
    // looks up a sprite frame and can return its width, so a real call does not run.
    // SPR_Height is then the process function when that hook is absent.
    // This init stores the pointer and does not read a sprite height. The process body
    // looks up a sprite frame and can return its height, so a real call does not run.
    // SPR_Frames is then the process function when that hook is absent.
    // This init stores the pointer and does not read a sprite frame count. The process
    // body looks up a sprite and can return its frame count, so a real call does not run.
    // SPR_GetModelPointer is then the process function when that hook is absent.
    // This init stores the pointer and does not resolve a model pointer. The process body
    // looks up a sprite and can return its model pointer, so a real call does not run.
    // R_StudioReloadSkin is then the process function when that hook is absent.
    // This init stores the pointer and does not reload a studio skin. The process body
    // can load a model texture and write skin fields, so a real call does not run.
    // SetCrosshair is then the process function when that hook is absent.
    // This init stores the pointer and does not set the crosshair. The process body
    // can write the crosshair sprite, color and rectangle, so a real call does not run.
    // DrawCrosshair is then the process function when that hook is absent.
    // This init stores the pointer and does not draw the crosshair. The process body
    // can draw the crosshair sprite, so a real call does not run.
    // R_RenderView is then the process function when that hook is absent.
    // This init stores the pointer and does not render a view. The process body
    // requires a GL context and draws the scene, so a real call does not run.
    // GL_Init is then the process function when that hook is absent.
    // This init stores the pointer and does not initialize GL. The process body
    // requires a GL context and queries GL strings, so a real call does not run.
    // GL_Config is then the process function when that hook is absent.
    // This init stores the pointer and does not configure GL. The process body
    // writes GL cvar pointers and can insert vendor cfg text, so a real call
    // does not run.
    // GL_SelectTexture is then the process function when that hook is absent.
    // This init stores the pointer and does not select a texture unit. The
    // process body requires a GL context and changes the active texture unit,
    // so a real call does not run.
    // CL_ConnectClient is then the process function when that hook is absent.
    // This init stores the pointer and does not connect the client. The
    // process body can open a network path and change connection state, so a
    // real call does not run.
    // CL_ClearClientState is then the process function when that hook is absent.
    // This init stores the pointer and does not clear client state. The
    // process body can clear client and connection state, so a real call
    // does not run.
    // GL_LoadTexture is then the process function when that hook is absent.
    // This init stores the pointer and does not load a texture. The process
    // body requires a GL context or a texture file and allocates texture
    // memory, so a real call does not run.
    // GL_LoadTexture2 is then the process function when that hook is absent.
    // This init stores the pointer and does not load a texture. The process
    // body requires a GL context or a texture file, applies a filter and
    // allocates texture memory, so a real call does not run.
    // GL_UnloadTextures is then the process function when that hook is absent.
    // This init stores the pointer and does not unload textures. The process
    // body frees texture session state, so a real call does not run.
    // GL_UnloadTexture is then the process function when that hook is absent.
    // This init stores the pointer and does not unload a texture. The process
    // body frees a matching texture by identifier, so a real call does not run.
    // GL_Bind is then the process function when that hook is absent.
    // This init stores the pointer and does not bind a texture. The process
    // body requires a GL context and changes GL texture state, so a real call
    // does not run.
    // Draw_Init is then the process function when that hook is absent.
    // This init stores the pointer and does not initialize draw state. The
    // process body initializes draw state and loads draw resources, so a real
    // call does not run.
    // Draw_Shutdown is then the process function when that hook is absent.
    // This init stores the pointer and does not shut down draw state. The
    // process body releases draw state and frees draw resources, so a real
    // call does not run.
    // Draw_PicFromWad is then the process function when that hook is absent.
    // This init stores the pointer and does not load a WAD picture. The
    // process body loads a WAD picture, so a real call does not run.
    // V_UpdatePalette is then the process function when that hook is absent.
    // This init stores the pointer and does not update the palette. The
    // process body updates the palette and can write gamma tables, so a real
    // call does not run.
    // The next retail hook is V_CheckGamma. This path returns before it.
    if (XashStartupIsActive())
    {
        if (SupplyXashEngineTable())
        {
            Con_Init();
            if (g_engfuncs.pfnGetGameDir != nullptr)
            {
                SetCStrikeFlags();
            }
            RecordXashNetSendPacket();
            RecordXashNetchanCopyFileFragments();
            RecordXashSvcTimeScale();
            RecordXashSvcSendCvarValue();
            RecordXashHttpUpdate();
            RecordXashHttpStop();
            RecordXashStartResourceDownloading();
            RecordXashReadPackets();
            RecordXashRequestMissingResources();
            RecordXashDisconnect();
            RecordXashClearState();
            RecordXashHostMap();
            RecordXashHostFilterTime();
            RecordXashModClearAll();
            RecordXashModFindName();
            RecordXashModPrint();
            RecordXashModValidateCRC();
            RecordXashModNeedCRC();
            RecordXashModLoadModel();
            RecordXashModForName();
            RecordXashModExtradata();
            RecordXashSprInit();
            RecordXashSprShutdown();
            RecordXashSprShutdownNoModelFree();
            RecordXashSprLoad();
            RecordXashSprDraw();
            RecordXashSprDrawAdditive();
            RecordXashSprDrawHoles();
            RecordXashSprDrawGeneric();
            RecordXashSprEnableScissor();
            RecordXashSprDisableScissor();
            RecordXashSprSet();
            RecordXashSprWidth();
            RecordXashSprHeight();
            RecordXashSprFrames();
            RecordXashSprGetModelPointer();
            RecordXashRStudioReloadSkin();
            RecordXashSetCrosshair();
            RecordXashDrawCrosshair();
            RecordXashRRenderView();
            RecordXashGLInit();
            RecordXashGLConfig();
            RecordXashGLSelectTexture();
            RecordXashCLConnectClient();
            RecordXashCLClearClientState();
            RecordXashGLLoadTexture();
            RecordXashGLLoadTexture2();
            RecordXashGLUnloadTextures();
            RecordXashGLUnloadTexture();
            RecordXashGLBind();
            RecordXashDrawInit();
            RecordXashDrawShutdown();
            RecordXashDrawPicFromWad();
            RecordXashUpdatePalette();
        }
        return;
    }

    if (g_Analytics)
        g_Analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " OnGameInitializing");

    nitro_utils::PtrValidator v;

    v.Assign(p_currenttexture, GET_VARIABLE_NAME(p_currenttexture), eng()->currenttexture);
    v.Assign(cl, GET_VARIABLE_NAME(cl), eng()->client_state);
    v.Assign(cls, GET_VARIABLE_NAME(cls), eng()->client_static);

#ifdef _WIN32
    CreateInterfaceFn gameui_factory = Sys_GetFactory("gameui.dll");
#else
    CreateInterfaceFn gameui_factory = Sys_GetFactory("gameui.so");
#endif
    v.Assign(g_pGameUi, GET_VARIABLE_NAME(g_pGameUi), (IGameUI*)InitializeInterface(GAMEUI_INTERFACE_VERSION_GS, &gameui_factory, 1));
    v.Assign(g_GameConsoleNext, GET_VARIABLE_NAME(g_GameConsoleNext), (IGameConsoleNext*)InitializeInterface(GAMECONSOLE_NEXT_INTERFACE_VERSION, &gameui_factory, 1));
    v.Assign(g_GameConsole, GET_VARIABLE_NAME(g_GameConsole), (IGameConsole*)InitializeInterface(GAMECONSOLE_INTERFACE_VERSION_GS, &gameui_factory, 1));

    CreateInterfaceFn filesystem_factory = Sys_GetFactory("FileSystem_Proxy.dll");
    v.Assign(g_pFileSystem, GET_VARIABLE_NAME(g_pFileSystem), (IFileSystem*)filesystem_factory(FILESYSTEM_INTERFACE_VERSION, NULL));
    v.Assign(g_pFileSystemNext, GET_VARIABLE_NAME(g_pFileSystemNext), (IFileSystemNext*)filesystem_factory(FILESYSTEM_NEXT_INTERFACE_VERSION, NULL));

    g_pFileSystem->AddSearchPathNoWrite("\\cstrike_downloads_private", "GAME");

    ColorScheme_ApplyAlias();

    if (v.Validate(eng()->cl_enginefunc, GET_VARIABLE_NAME(gEngfuncs)))
    {
        std::memcpy(&gEngfuncs, eng()->cl_enginefunc, sizeof(gEngfuncs));
        performance::runtime::BindPresentation();
    }

    if (v.Validate(eng()->enginefuncs, GET_VARIABLE_NAME(g_engfuncs)))
        std::memcpy(&g_engfuncs, eng()->enginefuncs, sizeof(g_engfuncs));

    if (v.Validate(eng()->modfuncs, GET_VARIABLE_NAME(g_modfuncs)))
        std::memcpy(&g_modfuncs, eng()->modfuncs, sizeof(g_modfuncs));

    if (v.Validate(eng()->cldll_func, GET_VARIABLE_NAME(cl_funcs)))
        std::memcpy(&cl_funcs, eng()->cldll_func, sizeof(cl_funcs));

    v.Assign(g_SettingGuard, GET_VARIABLE_NAME(g_SettingGuard), std::make_shared<nitro_utils::FileConfigProvider>("setting_guard.ini"));
    v.Assign(g_UserConfig, GET_VARIABLE_NAME(g_UserConfig), std::make_shared<nitro_utils::FileConfigProvider>("user_game_config.ini"));
    v.Assign(net_message, GET_VARIABLE_NAME(net_message), eng()->net_message);
    v.Assign(net_from, GET_VARIABLE_NAME(net_from), eng()->net_from);
    v.Assign(pMsg_readcount, GET_VARIABLE_NAME(pMsg_readcount), eng()->msg_readcount);
    v.Assign(realtime, GET_VARIABLE_NAME(realtime), eng()->realtime);
    v.Assign(host_frametime, GET_VARIABLE_NAME(host_frametime), eng()->host_frametime);
    v.Assign(gfExtendedError, GET_VARIABLE_NAME(gfExtendedError), eng()->gfExtendedError);
    v.Assign(gszDisconnectReason, GET_VARIABLE_NAME(gszDisconnectReason), eng()->gszDisconnectReason);
    v.Assign(gDownloadFile, GET_VARIABLE_NAME(gDownloadFile), eng()->gDownloadFile);
    v.Assign(cmd_source, GET_VARIABLE_NAME(cmd_source), eng()->cmd_source);
    v.Assign(g_psvs, GET_VARIABLE_NAME(g_psvs), eng()->server_static);
    v.Assign(g_psv, GET_VARIABLE_NAME(g_psvs), eng()->server);
    v.Assign(g_bMajorMapChange, GET_VARIABLE_NAME(g_bMajorMapChange), eng()->bMajorMapChange);
    v.Assign(p_g_careerState, GET_VARIABLE_NAME(g_careerState), eng()->careerState);
    v.Assign(loadmodel, GET_VARIABLE_NAME(loadmodel), eng()->loadmodel);
    v.Assign(cl_playerindex, GET_VARIABLE_NAME(cl_playerindex), eng()->cl_playerindex);
    v.Assign(p_loadcache, GET_VARIABLE_NAME(pLoadcache), eng()->loadcache);
    v.Assign(p_loadbuf, GET_VARIABLE_NAME(pLoadbuf), eng()->loadbuf);
    v.Assign(p_loadsize, GET_VARIABLE_NAME(pLoadsize), eng()->loadsize);
    v.Assign(p_host_initialized, GET_VARIABLE_NAME(p_host_initialized), eng()->host_initialized);
    v.Assign(p_g_engdstAddrs, GET_VARIABLE_NAME(p_g_engdstAddrs), eng()->g_engdstAddrs);
    v.Assign(p_vid, GET_VARIABLE_NAME(p_vid), eng()->vid);
    v.Assign(p_currententity, GET_VARIABLE_NAME(p_currententity), eng()->currententity);
    v.Assign(p_r_blend, GET_VARIABLE_NAME(p_r_blend), eng()->r_blend);
    v.Assign(p_r_entorigin, GET_VARIABLE_NAME(p_r_entorigin), eng()->r_entorigin);
    v.Assign(p_g_bUserFogOn, GET_VARIABLE_NAME(p_g_bUserFogOn), eng()->g_bUserFogOn);
    v.Assign(p_numTransObjs, GET_VARIABLE_NAME(p_numTransObjs), eng()->numTransObjs);
    v.Assign(p_transObjects, GET_VARIABLE_NAME(p_transObjects), eng()->transObjects);
    v.Assign(p_mtexenabled, GET_VARIABLE_NAME(p_mtexenabled), eng()->mtexenabled);
    v.Assign(p_r_worldentity, GET_VARIABLE_NAME(p_r_worldentity), eng()->r_worldentity);
    v.Assign(p_mirror, GET_VARIABLE_NAME(p_mirror), eng()->mirror);
    v.Assign(p_c_brush_polys, GET_VARIABLE_NAME(p_c_brush_polys), eng()->c_brush_polys);
    v.Assign(p_c_alias_polys, GET_VARIABLE_NAME(p_c_alias_polys), eng()->c_alias_polys);
    v.Assign(p_cl_numvisedicts, GET_VARIABLE_NAME(p_cl_numvisedicts), eng()->cl_numvisedicts);
    v.Assign(p_cl_visedicts, GET_VARIABLE_NAME(p_cl_visedicts), eng()->cl_visedicts);
    v.Assign(p_isFogEnabled, GET_VARIABLE_NAME(p_isFogEnabled), eng()->isFogEnabled);
    v.Assign(r_refdef, GET_VARIABLE_NAME(p_r_refdef), eng()->r_refdef);
    v.Assign(p_scr_fov_value, GET_VARIABLE_NAME(p_scr_fov_value), eng()->scr_fov_value);
    v.Assign(p_g_SND_VoiceOverdrive, GET_VARIABLE_NAME(p_g_SND_VoiceOverdrive), eng()->g_SND_VoiceOverdrive);
    v.Assign(p_cszrawsentences, GET_VARIABLE_NAME(p_cszrawsentences), eng()->cszrawsentences);
    v.Assign(p_rgpszrawsentence, GET_VARIABLE_NAME(p_rgpszrawsentence), eng()->rgpszrawsentence);
    v.Assign(p_cl_elights, GET_VARIABLE_NAME(p_cl_elights), eng()->cl_elights);
    v.Assign(p_scr_con_current, GET_VARIABLE_NAME(p_scr_con_current), eng()->scr_con_current);
    v.Assign(scr_downloading, GET_VARIABLE_NAME(scr_downloading), eng()->scr_downloading);
    v.Assign(p_cl_entities, GET_VARIABLE_NAME(p_cl_entities), eng()->cl_entities);
    v.Assign(p_noclip_anglehack, GET_VARIABLE_NAME(p_noclip_anglehack), eng()->noclip_anglehack);
    v.Assign(developer, GET_VARIABLE_NAME(developer), eng()->developer);
    v.Assign(p_cmd_argc, GET_VARIABLE_NAME(p_cmd_argc), eng()->cmd_argc);
    v.Assign(p_cmd_argv, GET_VARIABLE_NAME(p_cmd_argv), eng()->cmd_argv);
    v.Assign(p_cvar_vars, GET_VARIABLE_NAME(p_cvar_vars), eng()->cvar_vars);
    v.Assign(p_key_dest, GET_VARIABLE_NAME(p_key_dest), eng()->key_dest);
    v.Assign(p_known_sfx, GET_VARIABLE_NAME(p_known_sfx), eng()->known_sfx);
    v.Assign(p_num_sfx, GET_VARIABLE_NAME(p_num_sfx), eng()->num_sfx);
    v.Assign(p_gHostSpawnCount, GET_VARIABLE_NAME(p_gHostSpawnCount), eng()->gHostSpawnCount);
    v.Assign(p_net_local_adr, GET_VARIABLE_NAME(p_net_local_adr), eng()->net_local_adr);
    v.Assign(p_ip_sockets, GET_VARIABLE_NAME(p_ip_sockets), eng()->ip_sockets);
#ifdef _WIN32
    v.Assign(p_ipx_sockets, GET_VARIABLE_NAME(p_ipx_sockets), eng()->ipx_sockets);
#else
    // The Linux engine has no IPX support at all, so these sockets are never open
    static int no_ipx_sockets[goldsrc::NS_MAX]{};
    p_ipx_sockets = no_ipx_sockets;
#endif
    v.Assign(p_g_GameServerAddress, GET_VARIABLE_NAME(p_g_GameServerAddress), eng()->g_GameServerAddress);
    v.Assign(p_g_LastScreenUpdateTime, GET_VARIABLE_NAME(p_g_LastScreenUpdateTime), eng()->g_LastScreenUpdateTime);
    v.Assign(p_maxTransObjs, GET_VARIABLE_NAME(p_maxTransObjs), eng()->maxTransObjs);

    if (v.HasNullPtr())
    {
        std::string error = std::format(BREADCRUMBS_TAG " OnGameInitializing: PtrValidator failed: {}", nitro_utils::join(v.GetNullPtrNames().cbegin(), v.GetNullPtrNames().cend(), ", "));
        nitroapi::NitroApiInterface* nitro_api = g_NitroApi;

        EngineMiniUninitialize();

        nitro_api->GetEngineData()->Sys_Error.InvokeChained(error.c_str());
        return;
    }

    Con_Init();
    SetCStrikeFlags();

    //
    // Hooks that completely replace engine functions
    //
    g_Unsubs.emplace_back(eng()->NET_SendPacket              |= [](goldsrc::netsrc_t sock, int length, void* data, goldsrc::netadr_t to, const auto& next) { NET_SendPacket(sock, length, data, to); });
    g_Unsubs.emplace_back(eng()->Netchan_CopyFileFragments   |= [](goldsrc::netchan_t* chan, const auto& next)                                  { return Netchan_CopyFileFragments(chan); } );
    g_Unsubs.emplace_back(eng()->SVC_TimeScale               |= [](const auto& next)                                                   { CL_Parse_Timescale(); });
    g_Unsubs.emplace_back(eng()->SVC_SendCvarValue           |= [](const auto& next)                                                   { CL_Send_CvarValue(); });
    g_Unsubs.emplace_back(eng()->CL_HTTPUpdate               |= [](const auto& next)                                                   { CL_HTTPUpdate(); } );
    g_Unsubs.emplace_back(eng()->CL_HTTPStop_f               |= [](const auto& next)                                                   { CL_HTTPStop_f(); } );
    g_Unsubs.emplace_back(eng()->CL_StartResourceDownloading |= [](const char* msg, int custom, const auto& next)                      { CL_StartResourceDownloading(msg, custom); });
    g_Unsubs.emplace_back(eng()->CL_ReadPackets              |= [](const auto& next)                                                   { CL_ReadPackets(); });
    g_Unsubs.emplace_back(eng()->CL_RequestMissingResources  |= [](const auto& next)                                                   { return CL_RequestMissingResources(); });
    g_Unsubs.emplace_back(eng()->CL_Disconnect               |= [](const auto& next)                                                   { return CL_Disconnect(); });
    g_Unsubs.emplace_back(eng()->CL_ClearState |= [](goldsrc::qboolean quiet, const auto& next) {
        render::GetLifecycle().OnClientStateClearing();
        next->Invoke(quiet);
    });
    g_Unsubs.emplace_back(eng()->Host_Map_f                  |= [](const auto& next)                                                   { Host_Map_f(); });
    g_Unsubs.emplace_back(eng()->Host_FilterTime             |= [](float delay, const auto& next)                                      { return Host_FilterTime(delay); });
    g_Unsubs.emplace_back(eng()->Mod_ClearAll                |= [](const auto& next)                                                   { Mod_ClearAll(); });
    g_Unsubs.emplace_back(eng()->Mod_FindName |=
                          [](goldsrc::qboolean trackCRC, const char* name, const auto& next) { return Mod_FindName(trackCRC, name); });
    g_Unsubs.emplace_back(eng()->Mod_Print                   |= [](const auto& next)                                                   { Mod_Print(); });
    g_Unsubs.emplace_back(eng()->Mod_ValidateCRC             |= [](const char* name, goldsrc::CRC32_t crc, const auto& next)                    { return Mod_ValidateCRC(name, crc); });
    g_Unsubs.emplace_back(eng()->Mod_NeedCRC                 |= [](const char* name, goldsrc::qboolean needCRC, const auto& next)               { Mod_NeedCRC(name, needCRC); });
    g_Unsubs.emplace_back(eng()->Mod_LoadModel               |= [](goldsrc::model_t* mod, goldsrc::qboolean crash, goldsrc::qboolean trackCRC, const auto& next)  { return Mod_LoadModel(mod, crash, trackCRC); });
#ifndef _WIN32
    // GCC split hw.so's Mod_LoadModel into a Mod_LoadModel.part.2 clone that these two jump
    // into directly, skipping the hooked entry and handing our mod_known entries to the engine's loader
    g_Unsubs.emplace_back(eng()->Mod_ForName                 |= [](const char* name, goldsrc::qboolean crash, goldsrc::qboolean trackCRC, const auto& next) { return Mod_ForName(name, crash, trackCRC); });
    g_Unsubs.emplace_back(eng()->Mod_Extradata               |= [](goldsrc::model_t* mod, const auto& next)                                     { return Mod_Extradata(mod); });
#endif
    g_Unsubs.emplace_back(eng()->SPR_Init                    |= [](const auto& next)                                                   { return SPR_Init(); });
    g_Unsubs.emplace_back(eng()->SPR_Shutdown                |= [](const auto& next)                                                   { return SPR_Shutdown(); });
    g_Unsubs.emplace_back(eng()->SPR_Shutdown_NoModelFree    |= [](const auto& next)                                                   { return SPR_Shutdown_NoModelFree(); });
    g_Unsubs.emplace_back(eng()->SPR_Load                    |= [](const char* str, const auto& next)                                  { return SPR_Load(str); });
    g_Unsubs.emplace_back(eng()->SPR_Draw |=
                          [](int frame, int x, int y, const goldsrc::wrect_t* prc, const auto& next) { SPR_Draw(frame, x, y, prc); });
    g_Unsubs.emplace_back(eng()->SPR_DrawAdditive |= [](int frame, int x, int y, const goldsrc::wrect_t* prc, const auto& next) {
        SPR_DrawAdditive(frame, x, y, prc);
    });
    g_Unsubs.emplace_back(eng()->SPR_DrawHoles |=
                          [](int frame, int x, int y, const goldsrc::wrect_t* prc, const auto& next) { SPR_DrawHoles(frame, x, y, prc); });
    g_Unsubs.emplace_back(
        eng()->SPR_DrawGeneric |=
        [](int frame, int x, int y, const goldsrc::wrect_t* prc, int src, int dest, int width, int height, const auto& next) {
            SPR_DrawGeneric(frame, x, y, prc, src, dest, width, height);
        });
    g_Unsubs.emplace_back(eng()->SPR_EnableScissor           |= [](int x, int y, int width, int height, const auto& next)              { SPR_EnableScissor(x, y, width, height); });
    g_Unsubs.emplace_back(eng()->SPR_DisableScissor          |= [](const auto& next)                                                   { SPR_DisableScissor(); });
    g_Unsubs.emplace_back(eng()->SPR_Set |=
                          [](goldsrc::HSPRITE_t hsprite, int r, int g, int b, const auto& next) { SPR_Set(hsprite, r, g, b); });
    g_Unsubs.emplace_back(eng()->SPR_Width |=
                          [](goldsrc::HSPRITE_t hsprite, int frame, const auto& next) { return SPR_Width(hsprite, frame); });
    g_Unsubs.emplace_back(eng()->SPR_Height |=
                          [](goldsrc::HSPRITE_t hsprite, int frame, const auto& next) { return SPR_Height(hsprite, frame); });
    g_Unsubs.emplace_back(eng()->SPR_Frames |= [](goldsrc::HSPRITE_t hsprite, const auto& next) { return SPR_Frames(hsprite); });
    g_Unsubs.emplace_back(eng()->SPR_GetModelPointer |=
                          [](goldsrc::HSPRITE_t hsprite, const auto& next) { return SPR_GetModelPointer(hsprite); });
    g_Unsubs.emplace_back(eng()->R_StudioReloadSkin |= [](goldsrc::model_t* pModel, int index, goldsrc::skin_t* pskin, const auto& next) {
        return R_StudioReloadSkin(pModel, index, pskin);
    });
    g_Unsubs.emplace_back(eng()->SetCrosshair |= [](goldsrc::HSPRITE_t hspr, goldsrc::wrect_t rc, int r, int g, int b, const auto& next) {
        SetCrosshair(hspr, rc, r, g, b);
    });
    g_Unsubs.emplace_back(eng()->DrawCrosshair               |= [](int x, int y, const auto& next)                                     { DrawCrosshair(x, y); });
    g_Unsubs.emplace_back(eng()->R_RenderView                |= [](const auto& next)                                                   { R_RenderView(); });
    g_Unsubs.emplace_back(eng()->GL_Init                     |= [](const auto& next)                                                   { GL_Init(); next->Invoke(); });
    g_Unsubs.emplace_back(eng()->GL_Config                   |= [](const auto& next)                                                   { GL_Config(); });
    g_Unsubs.emplace_back(eng()->GL_SelectTexture            |= [](GLenum target, const auto& next)                                    { GL_SelectTexture(target); });
    g_Unsubs.emplace_back(eng()->CL_ConnectClient            |= [](const auto& next)                                                   { CL_ConnectClient(); });
    g_Unsubs.emplace_back(eng()->CL_ClearClientState         |= [](const auto& next)                                                   { CL_ClearClientState(); });
    g_Unsubs.emplace_back(eng()->GL_LoadTexture              |= [](const char* identifier, int textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal, const auto& next)  { return GL_LoadTexture(identifier, (GL_TEXTURETYPE)textureType, width, height, data, mipmap, iType, pPal); });
    g_Unsubs.emplace_back(eng()->GL_LoadTexture2             |= [](const char* identifier, int textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal, int filter, const auto& next)  { return GL_LoadTexture2(identifier, (GL_TEXTURETYPE)textureType, width, height, data, mipmap, iType, pPal, filter); });
    g_Unsubs.emplace_back(eng()->GL_UnloadTextures           |= [](const auto& next)                                                   { GL_UnloadTextures(); });
    g_Unsubs.emplace_back(eng()->GL_UnloadTexture            |= [](const char* identifier, const auto& next)                           { GL_UnloadTexture(identifier); });
    g_Unsubs.emplace_back(eng()->GL_Bind                     |= [](int texnum, const auto& next)                                       { GL_Bind(texnum); });
    g_Unsubs.emplace_back(eng()->Draw_Init                   |= [](const auto& next)                                                   { Draw_Init(); });
#ifdef _WIN32
    // The engine resolves its surface here, and schemes create their fonts right after, so this is
    // the last moment at which the text slots can be replaced before the first font is made.
    g_Unsubs.emplace_back(eng()->VGui_InitInterfacesList      += [](const char*, void*, int, bool)                                      { VGUI2_InstallTextSurface(); });
#endif
    g_Unsubs.emplace_back(eng()->Draw_Shutdown |= [](const auto& next) { Draw_Shutdown(); });
    g_Unsubs.emplace_back(eng()->Draw_PicFromWad             |= [](const char* name, const auto& next)                                 { return Draw_PicFromWad(name); });
    g_Unsubs.emplace_back(eng()->V_UpdatePalette             |= [](const auto& next)                                                   { V_UpdatePalette(); });
    g_Unsubs.emplace_back(eng()->V_CheckGamma                |= [](const auto& next)                                                   { return V_CheckGamma(); });
    g_Unsubs.emplace_back(eng()->BuildGammaTable |= [](float g, const auto& next) {
        render::Studio_Flush(render::StudioFlushReason::Resource);
        next->Invoke(g);
        BuildGammaTable(g);
    });
    g_Unsubs.emplace_back(eng()->DT_SetRenderState           |= [](int diffuseId, const auto& next)                                    { return DT_SetRenderState(diffuseId); });
    g_Unsubs.emplace_back(eng()->DT_LoadDetailTexture        |= [](const char *diffuseName, int diffuseId, const auto& next)           { DT_LoadDetailTexture(diffuseName, diffuseId); });
    g_Unsubs.emplace_back(eng()->R_ForceCVars                |= [](goldsrc::qboolean mp, const auto& next)                                      { R_ForceCVars(mp); });
#ifndef _WIN32
    // R_Clear and R_SetupFrame call this split-out body directly, and only in multiplayer
    g_Unsubs.emplace_back(eng()->R_ForceCVars_part           |= [](const auto& next)                                                   { R_ForceCVars(true); });
#endif

    //
    // The rest of the hooks and subscribers
    //
    g_Unsubs.emplace_back(eng()->SVC_StuffText |= [](const auto& next) {
        int read_count = *pMsg_readcount;
        std::string cmd = MSG_ReadString();
        *pMsg_readcount = read_count;

        PrivateRes_ParseDownloadPath(cmd);
        next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->SVC_ResourceLocation |= [](const auto &next) {
        int read_count = *pMsg_readcount;
        std::string url = MSG_ReadString();
        *pMsg_readcount = read_count;

        CL_HTTPSetDownloadUrl(url);
        next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->CL_HTTPCancel_f += []() {
        CL_HTTPCancel_f();
    });

    g_Unsubs.emplace_back(eng()->Cbuf_AddText += [](const char *text, goldsrc::sizebuf_t *buf, char* result) {
        // for cases when client downloading files through dlfile
        // give him a chance to use http download again
        // called directly: pfnClientCmd adds a command and its "\n" in two Cbuf_AddText calls and
        // this runs in between, so a queued "httpstop" glued onto the console's "disconnect"
        if (std::strncmp(text, "disconnect", 10) == 0)
            CL_HTTPStop_f();
    });

    g_Unsubs.emplace_back(eng()->Sys_InitGame += [](char *pOrgCmdLine, char *pBaseDir, void *pwnd, int bIsDedicated, bool ret) {
        g_bIsDedicatedServer = bIsDedicated;
    });

    g_Unsubs.emplace_back(eng()->Cvar_Command |= [](const auto& next) -> goldsrc::qboolean {
        const auto cvar_name = g_engfuncs.pfnCmd_Argv(0);
        const auto cvar = g_engfuncs.pfnCVarGetPointer(cvar_name);

        if (cvar != nullptr && cvar->flags & goldsrc::FCVAR_CHEAT)
        {
            if (!Host_IsSinglePlayerGame() && sv_cheats->value == 0 && !cls->spectator && !cls->demoplayback)
            {
                Con_Printf("Can't use cheat cvar %s with disabled cheats\n", cvar_name);
                return TRUE;
            }
        }

        return next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->AppendTEntity += [](goldsrc::cl_entity_t* ent) {
        AppendTEntity_Subscriber(ent);
    });

    g_Unsubs.emplace_back(eng()->Con_MessageMode_f += []() {
        if (*p_key_dest == goldsrc::key_message)
        {
            gEngfuncs.pfnServerCmd("client_chat_open");
        }
    });

    g_Unsubs.emplace_back(eng()->Con_MessageMode2_f += []() {
        if (*p_key_dest == goldsrc::key_message)
        {
            gEngfuncs.pfnServerCmd("client_chat_team_open");
        }
    });

    g_Unsubs.emplace_back(eng()->Key_Message += [](int key) {
        if (*p_key_dest != goldsrc::key_message)
        {
            gEngfuncs.pfnServerCmd("client_chat_close");
        }
    });

    g_Unsubs.emplace_back(client()->HUD_GetStudioModelInterface |= [](int version, goldsrc::r_studio_interface_t** ppinterface, goldsrc::engine_studio_api_t* pstudio, const auto& next) {
        // The client copies the table when it answers, so the patch has to be in before the call.
        render::Studio_PatchApi(pstudio);
        PreviewModel_PatchStudioApi(pstudio);

        return next->Invoke(version, ppinterface, pstudio);
    });

    g_Unsubs.emplace_back(client()->HUD_GetStudioModelInterface += [](int version, goldsrc::r_studio_interface_t **ppinterface, goldsrc::engine_studio_api_t *pstudio, int result) {
        pStudioAPI = *ppinterface;
        PreviewModel_BindStudioInterface(pStudioAPI);
    });

    g_Unsubs.emplace_back(
        client()->HUD_StudioEvent |= [](const goldsrc::mstudioevent_s* event, const goldsrc::cl_entity_t* entity, const auto& next) {
            render::StudioQueuePause pause;
            next->Invoke(event, entity);
        }
    );

    g_Unsubs.emplace_back(eng()->Host_FrameInternal += [](float delta) {
        if (g_pTaskCoroImpl)
        {
            g_pTaskCoroImpl->Update();
        }

        DiscordPresence_Frame();
    });

    g_Unsubs.emplace_back(eng()->S_StartDynamicSound |= S_StartDynamicSoundHook);
    
    GL_SetMode_Subscriber(mainwindow, pmaindc, pbaseRC, pszDriver, pszCmdLine, result);
    if (result)
    {
        performance::runtime::ContextCreated();
    }
}

static void OnGameInitialized()
{
    render::Renderer_InitializeMaterialFiles(gEngfuncs.pfnGetGameDirectory());
    nitro_utils::PtrValidator v;

#ifdef _WIN32
    CreateInterfaceFn vgui2_factory = Sys_GetFactory("vgui2.dll");
#else
    CreateInterfaceFn vgui2_factory = Sys_GetFactory("vgui2.so");
#endif
    g_pLocalize = v.Validate((vgui2::ILocalize*)InitializeInterface(VGUI_LOCALIZE_INTERFACE_VERSION, &vgui2_factory, 1), GET_VARIABLE_NAME(localize));
    g_pLocalize->AddFile(g_pFileSystem, "resource/nextclient_%language%.txt");

    KV_InitKeyValuesSystem2(vgui2_factory);

    v.Assign(fs_startup_timings, GET_VARIABLE_NAME(fs_startup_timings), g_engfuncs.pfnCVarGetPointer("fs_startup_timings"));
    v.Assign(fs_lazy_precache, GET_VARIABLE_NAME(fs_lazy_precache), g_engfuncs.pfnCVarGetPointer("fs_lazy_precache"));
    v.Assign(fs_precache_timings, GET_VARIABLE_NAME(fs_precache_timings), g_engfuncs.pfnCVarGetPointer("fs_precache_timings"));
    v.Assign(fs_perf_warnings, GET_VARIABLE_NAME(fs_perf_warnings), g_engfuncs.pfnCVarGetPointer("fs_perf_warnings"));
    v.Assign(cl_download_ingame, GET_VARIABLE_NAME(cl_download_ingame), g_engfuncs.pfnCVarGetPointer("cl_download_ingame"));
    v.Assign(cl_shownet, GET_VARIABLE_NAME(cl_shownet), g_engfuncs.pfnCVarGetPointer("cl_shownet"));
    v.Assign(cl_timeout, GET_VARIABLE_NAME(cl_timeout), g_engfuncs.pfnCVarGetPointer("cl_timeout"));
    v.Assign(host_framerate, GET_VARIABLE_NAME(host_framerate), g_engfuncs.pfnCVarGetPointer("host_framerate"));
    v.Assign(sys_ticrate, GET_VARIABLE_NAME(sys_ticrate), g_engfuncs.pfnCVarGetPointer("sys_ticrate"));
    v.Assign(fps_override, GET_VARIABLE_NAME(fps_override), g_engfuncs.pfnCVarGetPointer("fps_override"));
    v.Assign(fps_max, GET_VARIABLE_NAME(fps_max), g_engfuncs.pfnCVarGetPointer("fps_max"));
    v.Assign(sv_cheats, GET_VARIABLE_NAME(sv_cheats), g_engfuncs.pfnCVarGetPointer("sv_cheats"));
    v.Assign(r_drawentities, GET_VARIABLE_NAME(r_drawentities), g_engfuncs.pfnCVarGetPointer("r_drawentities"));
    v.Assign(r_norefresh, GET_VARIABLE_NAME(r_norefresh), g_engfuncs.pfnCVarGetPointer("r_norefresh"));
    v.Assign(r_speeds, GET_VARIABLE_NAME(r_speeds), g_engfuncs.pfnCVarGetPointer("r_speeds"));
    v.Assign(gl_dither_cvar, GET_VARIABLE_NAME(gl_dither), g_engfuncs.pfnCVarGetPointer("gl_dither"));
    v.Assign(gl_spriteblend, GET_VARIABLE_NAME(gl_spriteblend), g_engfuncs.pfnCVarGetPointer("gl_spriteblend"));
    v.Assign(gl_vsync, GET_VARIABLE_NAME(gl_vsync), g_engfuncs.pfnCVarGetPointer("gl_vsync"));
    v.Assign(snd_show, GET_VARIABLE_NAME(snd_show), g_engfuncs.pfnCVarGetPointer("snd_show"));
    v.Assign(chase_active, GET_VARIABLE_NAME(chase_active), g_engfuncs.pfnCVarGetPointer("chase_active"));
    
    v.Assign(sys_timescale, GET_VARIABLE_NAME(sys_timescale), eng()->sys_timescale);
    sys_timescale->flags |= goldsrc::FCVAR_CHEAT;
    Cvar_RegisterVariable(sys_timescale);

    if (v.HasNullPtr())
    {
        std::string error = std::format(BREADCRUMBS_TAG " OnGameInitialize: PtrValidator failed: {}", nitro_utils::join(v.GetNullPtrNames().cbegin(), v.GetNullPtrNames().cend(), ", "));
        nitroapi::NitroApiInterface* nitro_api = g_NitroApi;

        EngineMiniUninitialize();

        nitro_api->GetEngineData()->Sys_Error.InvokeChained(error.c_str());
        return;
    }

    performance::runtime::PresentationInitialized();
    viewmodel_fov = gEngfuncs.pfnRegisterVariable(cvars::kViewmodelFov.name, cvars::kViewmodelFov.value, goldsrc::FCVAR_ARCHIVE);

    Preview_Init();

    CL_CreateHttpDownloadManager(g_pGameUi, g_pLocalize, g_SettingGuard);
#ifdef _WIN32
    JSAPI_Init();
#endif
    CL_CvarsSandboxInit();
    CL_StringRegistryInit();
    CL_NclEntitySyncInit();
    CL_NclEntitySyncRegisterType(static_cast<uint8_t>(ncl_entity::EntityTypeId::Player), std::make_unique<PlayerSyncSystem>());
    CL_NclEntitySyncRegisterType(static_cast<uint8_t>(ncl_entity::EntityTypeId::Weapon), std::make_unique<WeaponSyncSystem>());
    CL_NclEntitySyncRegisterType(
        static_cast<uint8_t>(ncl_entity::EntityTypeId::Material),
        std::make_unique<MaterialSyncSystem>(render::GetServerMaterials(), CL_GetStringRegistry())
    );
    CL_NclEntitySyncOverlayInit();
    PROTECTOR_Init(g_SettingGuard);
    DiscordPresence_Init();
}

EngineMiniNetSendPacketFn g_EngineMiniNetSendPacket = NET_SendPacket;
EngineMiniNetchanCopyFileFragmentsFn g_EngineMiniNetchanCopyFileFragments = Netchan_CopyFileFragments;
EngineMiniSvcTimeScaleFn g_EngineMiniSvcTimeScale = CL_Parse_Timescale;
EngineMiniSvcSendCvarValueFn g_EngineMiniSvcSendCvarValue = CL_Send_CvarValue;
EngineMiniHttpUpdateFn g_EngineMiniHttpUpdate = CL_HTTPUpdate;
EngineMiniHttpStopFn g_EngineMiniHttpStop = CL_HTTPStop_f;
EngineMiniStartResourceDownloadingFn g_EngineMiniStartResourceDownloading = CL_StartResourceDownloading;
EngineMiniReadPacketsFn g_EngineMiniReadPackets = CL_ReadPackets;
EngineMiniRequestMissingResourcesFn g_EngineMiniRequestMissingResources = CL_RequestMissingResources;
EngineMiniDisconnectFn g_EngineMiniDisconnect = CL_Disconnect;
EngineMiniClearStateFn g_EngineMiniClearState = CL_ClearState;
EngineMiniHostMapFn g_EngineMiniHostMap = Host_Map_f;
EngineMiniHostFilterTimeFn g_EngineMiniHostFilterTime = Host_FilterTime;
EngineMiniModClearAllFn g_EngineMiniModClearAll = Mod_ClearAll;
EngineMiniModFindNameFn g_EngineMiniModFindName = Mod_FindName;
EngineMiniModPrintFn g_EngineMiniModPrint = Mod_Print;
EngineMiniModValidateCRCFn g_EngineMiniModValidateCRC = Mod_ValidateCRC;
EngineMiniModNeedCRCFn g_EngineMiniModNeedCRC = Mod_NeedCRC;
EngineMiniModLoadModelFn g_EngineMiniModLoadModel = Mod_LoadModel;
EngineMiniModForNameFn g_EngineMiniModForName = Mod_ForName;
EngineMiniModExtradataFn g_EngineMiniModExtradata = Mod_Extradata;
EngineMiniSprInitFn g_EngineMiniSprInit = SPR_Init;
EngineMiniSprShutdownFn g_EngineMiniSprShutdown = SPR_Shutdown;
EngineMiniSprShutdownNoModelFreeFn g_EngineMiniSprShutdownNoModelFree = SPR_Shutdown_NoModelFree;
EngineMiniSprLoadFn g_EngineMiniSprLoad = SPR_Load;
EngineMiniSprDrawFn g_EngineMiniSprDraw = SPR_Draw;
EngineMiniSprDrawAdditiveFn g_EngineMiniSprDrawAdditive = SPR_DrawAdditive;
EngineMiniSprDrawHolesFn g_EngineMiniSprDrawHoles = SPR_DrawHoles;
EngineMiniSprDrawGenericFn g_EngineMiniSprDrawGeneric = SPR_DrawGeneric;
EngineMiniSprEnableScissorFn g_EngineMiniSprEnableScissor = SPR_EnableScissor;
EngineMiniSprDisableScissorFn g_EngineMiniSprDisableScissor = SPR_DisableScissor;
EngineMiniSprSetFn g_EngineMiniSprSet = SPR_Set;
EngineMiniSprWidthFn g_EngineMiniSprWidth = SPR_Width;
EngineMiniSprHeightFn g_EngineMiniSprHeight = SPR_Height;
EngineMiniSprFramesFn g_EngineMiniSprFrames = SPR_Frames;
EngineMiniSprGetModelPointerFn g_EngineMiniSprGetModelPointer = SPR_GetModelPointer;
EngineMiniRStudioReloadSkinFn g_EngineMiniRStudioReloadSkin = R_StudioReloadSkin;
EngineMiniSetCrosshairFn g_EngineMiniSetCrosshair = SetCrosshair;
EngineMiniDrawCrosshairFn g_EngineMiniDrawCrosshair = DrawCrosshair;
EngineMiniRRenderViewFn g_EngineMiniRRenderView = R_RenderView;
EngineMiniGLInitFn g_EngineMiniGLInit = GL_Init;
EngineMiniGLConfigFn g_EngineMiniGLConfig = GL_Config;
EngineMiniGLSelectTextureFn g_EngineMiniGLSelectTexture =
    reinterpret_cast<EngineMiniGLSelectTextureFn>(GL_SelectTexture);
EngineMiniCLConnectClientFn g_EngineMiniCLConnectClient = CL_ConnectClient;
EngineMiniCLClearClientStateFn g_EngineMiniCLClearClientState = CL_ClearClientState;
EngineMiniGLLoadTextureFn g_EngineMiniGLLoadTexture =
    reinterpret_cast<EngineMiniGLLoadTextureFn>(GL_LoadTexture);
EngineMiniGLLoadTexture2Fn g_EngineMiniGLLoadTexture2 =
    reinterpret_cast<EngineMiniGLLoadTexture2Fn>(GL_LoadTexture2);
EngineMiniGLUnloadTexturesFn g_EngineMiniGLUnloadTextures = GL_UnloadTextures;
EngineMiniGLUnloadTextureFn g_EngineMiniGLUnloadTexture = GL_UnloadTexture;
EngineMiniGLBindFn g_EngineMiniGLBind = GL_Bind;
EngineMiniDrawInitFn g_EngineMiniDrawInit = Draw_Init;
EngineMiniDrawShutdownFn g_EngineMiniDrawShutdown = Draw_Shutdown;
EngineMiniDrawPicFromWadFn g_EngineMiniDrawPicFromWad = Draw_PicFromWad;
EngineMiniUpdatePaletteFn g_EngineMiniUpdatePalette = V_UpdatePalette;

namespace
{

    EngineMiniXashStartup g_xash_startup{};
    nitroapi::EngineData* g_xash_engine = nullptr;

    void RecordXashNetSendPacket()
    {
        // The retail site installs a replacement. It does not send during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.net_send_packet = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->NET_SendPacket.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniNetSendPacket == nullptr)
        {
            return;
        }
        g_xash_startup.net_send_packet = g_EngineMiniNetSendPacket;
    }

    void RecordXashNetchanCopyFileFragments()
    {
        // The retail site installs a replacement. It does not copy during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.netchan_copy = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Netchan_CopyFileFragments.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniNetchanCopyFileFragments == nullptr)
        {
            return;
        }
        g_xash_startup.netchan_copy = g_EngineMiniNetchanCopyFileFragments;
    }

    void RecordXashSvcTimeScale()
    {
        // The retail site installs a replacement. It does not parse during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.svc_timescale = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SVC_TimeScale.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSvcTimeScale == nullptr)
        {
            return;
        }
        g_xash_startup.svc_timescale = g_EngineMiniSvcTimeScale;
    }

    void RecordXashSvcSendCvarValue()
    {
        // The retail site installs a replacement. It does not send a cvar during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.svc_sendcvarvalue = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SVC_SendCvarValue.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSvcSendCvarValue == nullptr)
        {
            return;
        }
        g_xash_startup.svc_sendcvarvalue = g_EngineMiniSvcSendCvarValue;
    }

    void RecordXashHttpUpdate()
    {
        // The retail site installs a replacement. It does not update HTTP during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.http_update = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_HTTPUpdate.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniHttpUpdate == nullptr)
        {
            return;
        }
        g_xash_startup.http_update = g_EngineMiniHttpUpdate;
    }

    void RecordXashHttpStop()
    {
        // The retail site installs a replacement. It does not stop a download during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.http_stop = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_HTTPStop_f.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniHttpStop == nullptr)
        {
            return;
        }
        g_xash_startup.http_stop = g_EngineMiniHttpStop;
    }

    void RecordXashStartResourceDownloading()
    {
        // The retail site installs a replacement. It does not start a download during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.start_resource = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_StartResourceDownloading.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniStartResourceDownloading == nullptr)
        {
            return;
        }
        g_xash_startup.start_resource = g_EngineMiniStartResourceDownloading;
    }

    void RecordXashReadPackets()
    {
        // The retail site installs a replacement. It does not read packets during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.read_packets = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_ReadPackets.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniReadPackets == nullptr)
        {
            return;
        }
        g_xash_startup.read_packets = g_EngineMiniReadPackets;
    }

    void RecordXashRequestMissingResources()
    {
        // The retail site installs a replacement. It does not request resources during init.
        // A null function pointer is not a call. A null hook is not a call.
        g_xash_startup.request_missing = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_RequestMissingResources.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniRequestMissingResources == nullptr)
        {
            return;
        }
        g_xash_startup.request_missing = g_EngineMiniRequestMissingResources;
    }

    void RecordXashDisconnect()
    {
        // The retail site installs a replacement. It does not disconnect during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The retail body reads client state and can transmit, so this path does not call it.
        g_xash_startup.disconnect = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_Disconnect.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniDisconnect == nullptr)
        {
            return;
        }
        g_xash_startup.disconnect = g_EngineMiniDisconnect;
    }

    void RecordXashClearState()
    {
        // The retail site runs the original clear after a lifecycle notice.
        // It does not clear during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body forwards into that clear, so this path does not call it.
        g_xash_startup.clear_state = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_ClearState.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniClearState == nullptr)
        {
            return;
        }
        g_xash_startup.clear_state = g_EngineMiniClearState;
    }

    void RecordXashHostMap()
    {
        // The retail site installs a replacement. It does not change the map during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body loads a map and can disconnect, so this path does not call it.
        g_xash_startup.host_map = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Host_Map_f.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniHostMap == nullptr)
        {
            return;
        }
        g_xash_startup.host_map = g_EngineMiniHostMap;
    }

    void RecordXashHostFilterTime()
    {
        // The retail site installs a replacement. It does not filter time during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body advances host time and the frame interval, so this path does not call it.
        g_xash_startup.host_filter = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Host_FilterTime.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniHostFilterTime == nullptr)
        {
            return;
        }
        g_xash_startup.host_filter = g_EngineMiniHostFilterTime;
    }

    void RecordXashModClearAll()
    {
        // The retail site installs a replacement. It does not clear models during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body marks known models unreferenced and drops sprite cache, so this path does not call it.
        g_xash_startup.mod_clear = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_ClearAll.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModClearAll == nullptr)
        {
            return;
        }
        g_xash_startup.mod_clear = g_EngineMiniModClearAll;
    }

    void RecordXashModFindName()
    {
        // The retail site installs a replacement. It does not find a model during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body searches the known list and can allocate a slot, so this path does not call it.
        g_xash_startup.mod_find = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_FindName.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModFindName == nullptr)
        {
            return;
        }
        g_xash_startup.mod_find = g_EngineMiniModFindName;
    }

    void RecordXashModPrint()
    {
        // The retail site installs a replacement. It does not print during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body prints the cached model list, so this path does not call it.
        g_xash_startup.mod_print = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_Print.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModPrint == nullptr)
        {
            return;
        }
        g_xash_startup.mod_print = g_EngineMiniModPrint;
    }

    void RecordXashModValidateCRC()
    {
        // The retail site installs a replacement. It does not validate a CRC during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a model and can reject a stored CRC, so this path does not call it.
        g_xash_startup.mod_validate = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_ValidateCRC.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModValidateCRC == nullptr)
        {
            return;
        }
        g_xash_startup.mod_validate = g_EngineMiniModValidateCRC;
    }

    void RecordXashModNeedCRC()
    {
        // The retail site installs a replacement. It does not mark a CRC during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a model and writes the need-CRC flag, so this path does not call it.
        g_xash_startup.mod_need = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_NeedCRC.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModNeedCRC == nullptr)
        {
            return;
        }
        g_xash_startup.mod_need = g_EngineMiniModNeedCRC;
    }

    void RecordXashModLoadModel()
    {
        // The retail site installs a replacement. It does not load a model during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body reads a model file and can write a CRC and needload, so this path does not call it.
        g_xash_startup.mod_load = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_LoadModel.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModLoadModel == nullptr)
        {
            return;
        }
        g_xash_startup.mod_load = g_EngineMiniModLoadModel;
    }

    void RecordXashModForName()
    {
        // The retail site installs a replacement. It does not look up a model during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body searches the known list and can load a model, so this path does not call it.
        g_xash_startup.mod_forname = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_ForName.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModForName == nullptr)
        {
            return;
        }
        g_xash_startup.mod_forname = g_EngineMiniModForName;
    }

    void RecordXashModExtradata()
    {
        // The retail site installs a replacement. It does not read extradata during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body can Sys_Error on a brush model and can load a model, so this path does not call it.
        g_xash_startup.mod_extradata = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Mod_Extradata.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniModExtradata == nullptr)
        {
            return;
        }
        g_xash_startup.mod_extradata = g_EngineMiniModExtradata;
    }

    void RecordXashSprInit()
    {
        // The retail site installs a replacement. It does not allocate the sprite list during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body allocates the sprite list when it is empty, so this path does not call it.
        g_xash_startup.spr_init = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Init.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprInit == nullptr)
        {
            return;
        }
        g_xash_startup.spr_init = g_EngineMiniSprInit;
    }

    void RecordXashSprShutdown()
    {
        // The retail site installs a replacement. It does not free the sprite list during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body frees the sprite list when it is allocated and can unload sprite textures, so this path does not call it.
        g_xash_startup.spr_shutdown = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Shutdown.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprShutdown == nullptr)
        {
            return;
        }
        g_xash_startup.spr_shutdown = g_EngineMiniSprShutdown;
    }

    void RecordXashSprShutdownNoModelFree()
    {
        // The retail site installs a replacement. It does not free the sprite list during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body frees the sprite list and sprite names when the list is allocated, so this path does not call it.
        g_xash_startup.spr_shutdown_nomodelfree = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Shutdown_NoModelFree.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprShutdownNoModelFree == nullptr)
        {
            return;
        }
        g_xash_startup.spr_shutdown_nomodelfree = g_EngineMiniSprShutdownNoModelFree;
    }

    void RecordXashSprLoad()
    {
        // The retail site installs a replacement. It does not load a sprite during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body loads a named sprite when the list is allocated, can allocate a
        // name, can call Mod_ForName, and can Sys_Error when the list is full, so this path
        // does not call it.
        g_xash_startup.spr_load = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Load.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprLoad == nullptr)
        {
            return;
        }
        g_xash_startup.spr_load = g_EngineMiniSprLoad;
    }

    void RecordXashSprDraw()
    {
        // The retail site installs a replacement. It does not draw a sprite during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body draws the current sprite frame when a sprite is current and the
        // point is inside the video image, and can call Draw_SpriteFrame, so this path does
        // not call it.
        g_xash_startup.spr_draw = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Draw.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprDraw == nullptr)
        {
            return;
        }
        g_xash_startup.spr_draw = g_EngineMiniSprDraw;
    }

    void RecordXashSprDrawAdditive()
    {
        // The retail site installs a replacement. It does not draw a sprite during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body draws the current sprite additively when a sprite is current
        // and the point is inside the video image, and can call Draw_SpriteFrameAdditive,
        // so this path does not call it.
        g_xash_startup.spr_draw_additive = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_DrawAdditive.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprDrawAdditive == nullptr)
        {
            return;
        }
        g_xash_startup.spr_draw_additive = g_EngineMiniSprDrawAdditive;
    }

    void RecordXashSprDrawHoles()
    {
        // The retail site installs a replacement. It does not draw a sprite during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body draws the current sprite with holes when a sprite is current
        // and the point is inside the video image, and can call Draw_SpriteFrameHoles,
        // so this path does not call it.
        g_xash_startup.spr_draw_holes = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_DrawHoles.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprDrawHoles == nullptr)
        {
            return;
        }
        g_xash_startup.spr_draw_holes = g_EngineMiniSprDrawHoles;
    }

    void RecordXashSprDrawGeneric()
    {
        // The retail site installs a replacement. It does not draw a sprite during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body draws the current sprite with the caller blend when a sprite
        // is current and the point is inside the video image, and can call
        // Draw_SpriteFrameGeneric, so this path does not call it.
        g_xash_startup.spr_draw_generic = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_DrawGeneric.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprDrawGeneric == nullptr)
        {
            return;
        }
        g_xash_startup.spr_draw_generic = g_EngineMiniSprDrawGeneric;
    }

    void RecordXashSprEnableScissor()
    {
        // The retail site installs a replacement. It does not set a scissor during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body enables a scissor rectangle and can call EnableScissorTest,
        // so this path does not call it.
        g_xash_startup.spr_enable_scissor = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_EnableScissor.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprEnableScissor == nullptr)
        {
            return;
        }
        g_xash_startup.spr_enable_scissor = g_EngineMiniSprEnableScissor;
    }

    void RecordXashSprDisableScissor()
    {
        // The retail site installs a replacement. It does not clear a scissor during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body disables the scissor and can call DisableScissorTest,
        // so this path does not call it.
        g_xash_startup.spr_disable_scissor = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_DisableScissor.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprDisableScissor == nullptr)
        {
            return;
        }
        g_xash_startup.spr_disable_scissor = g_EngineMiniSprDisableScissor;
    }

    void RecordXashSprSet()
    {
        // The retail site installs a replacement. It does not set a sprite during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a sprite and can call qglColor4f,
        // so this path does not call it.
        g_xash_startup.spr_set = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Set.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprSet == nullptr)
        {
            return;
        }
        g_xash_startup.spr_set = g_EngineMiniSprSet;
    }

    void RecordXashSprWidth()
    {
        // The retail site installs a replacement. It does not read a width during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a sprite frame and can return its width,
        // so this path does not call it.
        g_xash_startup.spr_width = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Width.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprWidth == nullptr)
        {
            return;
        }
        g_xash_startup.spr_width = g_EngineMiniSprWidth;
    }

    void RecordXashSprHeight()
    {
        // The retail site installs a replacement. It does not read a height during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a sprite frame and can return its height,
        // so this path does not call it.
        g_xash_startup.spr_height = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Height.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprHeight == nullptr)
        {
            return;
        }
        g_xash_startup.spr_height = g_EngineMiniSprHeight;
    }

    void RecordXashSprFrames()
    {
        // The retail site installs a replacement. It does not read a frame count during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a sprite and can return its frame count,
        // so this path does not call it.
        g_xash_startup.spr_frames = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_Frames.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprFrames == nullptr)
        {
            return;
        }
        g_xash_startup.spr_frames = g_EngineMiniSprFrames;
    }

    void RecordXashSprGetModelPointer()
    {
        // The retail site installs a replacement. It does not resolve a model pointer during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body looks up a sprite and can return its model pointer,
        // so this path does not call it.
        g_xash_startup.spr_get_model_pointer = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SPR_GetModelPointer.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSprGetModelPointer == nullptr)
        {
            return;
        }
        g_xash_startup.spr_get_model_pointer = g_EngineMiniSprGetModelPointer;
    }

    void RecordXashRStudioReloadSkin()
    {
        // The retail site installs a replacement. It does not reload a studio skin during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body can load a model texture and write skin fields,
        // so this path does not call it.
        g_xash_startup.studio_reload_skin = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->R_StudioReloadSkin.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniRStudioReloadSkin == nullptr)
        {
            return;
        }
        g_xash_startup.studio_reload_skin = g_EngineMiniRStudioReloadSkin;
    }

    void RecordXashSetCrosshair()
    {
        // The retail site installs a replacement. It does not set the crosshair during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body can write the crosshair sprite, color and rectangle,
        // so this path does not call it.
        g_xash_startup.set_crosshair = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->SetCrosshair.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniSetCrosshair == nullptr)
        {
            return;
        }
        g_xash_startup.set_crosshair = g_EngineMiniSetCrosshair;
    }

    void RecordXashDrawCrosshair()
    {
        // The retail site installs a replacement. It does not draw the crosshair during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body can draw the crosshair sprite,
        // so this path does not call it.
        g_xash_startup.draw_crosshair = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->DrawCrosshair.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniDrawCrosshair == nullptr)
        {
            return;
        }
        g_xash_startup.draw_crosshair = g_EngineMiniDrawCrosshair;
    }

    void RecordXashRRenderView()
    {
        // The retail site installs a replacement. It does not render a view during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body requires a GL context and draws the scene,
        // so this path does not call it.
        g_xash_startup.render_view = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->R_RenderView.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniRRenderView == nullptr)
        {
            return;
        }
        g_xash_startup.render_view = g_EngineMiniRRenderView;
    }

    void RecordXashGLInit()
    {
        // The retail site installs a replacement. It does not initialize GL during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body requires a GL context and queries GL strings,
        // so this path does not call it.
        g_xash_startup.gl_init = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_Init.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLInit == nullptr)
        {
            return;
        }
        g_xash_startup.gl_init = g_EngineMiniGLInit;
    }

    void RecordXashGLConfig()
    {
        // The retail site installs a replacement. It does not configure GL during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body writes GL cvar pointers and can insert vendor cfg text,
        // so this path does not call it.
        g_xash_startup.gl_config = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_Config.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLConfig == nullptr)
        {
            return;
        }
        g_xash_startup.gl_config = g_EngineMiniGLConfig;
    }

    void RecordXashGLSelectTexture()
    {
        // The retail site installs a replacement. It does not select a texture unit during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body requires a GL context and changes the active texture unit,
        // so this path does not call it.
        g_xash_startup.gl_select_texture = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_SelectTexture.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLSelectTexture == nullptr)
        {
            return;
        }
        g_xash_startup.gl_select_texture = g_EngineMiniGLSelectTexture;
    }

    void RecordXashCLConnectClient()
    {
        // The retail site installs a replacement. It does not connect the client during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body can open a network path and change connection state,
        // so this path does not call it.
        g_xash_startup.connect_client = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_ConnectClient.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniCLConnectClient == nullptr)
        {
            return;
        }
        g_xash_startup.connect_client = g_EngineMiniCLConnectClient;
    }

    void RecordXashCLClearClientState()
    {
        // The retail site installs a replacement. It does not clear client state during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body can clear client and connection state,
        // so this path does not call it.
        g_xash_startup.clear_client_state = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->CL_ClearClientState.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniCLClearClientState == nullptr)
        {
            return;
        }
        g_xash_startup.clear_client_state = g_EngineMiniCLClearClientState;
    }

    void RecordXashGLLoadTexture()
    {
        // The retail site installs a replacement. It does not load a texture during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body requires a GL context or a texture file and allocates
        // texture memory, so this path does not call it.
        g_xash_startup.load_texture = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_LoadTexture.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLLoadTexture == nullptr)
        {
            return;
        }
        g_xash_startup.load_texture = g_EngineMiniGLLoadTexture;
    }

    void RecordXashGLLoadTexture2()
    {
        // The retail site installs a replacement. It does not load a texture during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body requires a GL context or a texture file, applies a
        // filter and allocates texture memory, so this path does not call it.
        g_xash_startup.load_texture2 = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_LoadTexture2.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLLoadTexture2 == nullptr)
        {
            return;
        }
        g_xash_startup.load_texture2 = g_EngineMiniGLLoadTexture2;
    }

    void RecordXashGLUnloadTextures()
    {
        // The retail site installs a replacement. It does not unload textures during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body frees texture session state, so this path does not call it.
        g_xash_startup.unload_textures = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_UnloadTextures.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLUnloadTextures == nullptr)
        {
            return;
        }
        g_xash_startup.unload_textures = g_EngineMiniGLUnloadTextures;
    }

    void RecordXashGLUnloadTexture()
    {
        // The retail site installs a replacement. It does not unload a texture during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body frees a matching texture by identifier, so this path does not call it.
        g_xash_startup.unload_texture = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_UnloadTexture.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLUnloadTexture == nullptr)
        {
            return;
        }
        g_xash_startup.unload_texture = g_EngineMiniGLUnloadTexture;
    }

    void RecordXashGLBind()
    {
        // The retail site installs a replacement. It does not bind a texture during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body requires a GL context and changes GL texture state, so this path
        // does not call it.
        g_xash_startup.bind = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->GL_Bind.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniGLBind == nullptr)
        {
            return;
        }
        g_xash_startup.bind = g_EngineMiniGLBind;
    }

    void RecordXashDrawInit()
    {
        // The retail site installs a replacement. It does not initialize draw state during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body initializes draw state and loads draw resources, so this path
        // does not call it.
        g_xash_startup.draw_init = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Draw_Init.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniDrawInit == nullptr)
        {
            return;
        }
        g_xash_startup.draw_init = g_EngineMiniDrawInit;
    }

    void RecordXashDrawShutdown()
    {
        // The retail site installs a replacement. It does not shut down draw state during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body releases draw state and frees draw resources, so this path
        // does not call it.
        g_xash_startup.draw_shutdown = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Draw_Shutdown.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniDrawShutdown == nullptr)
        {
            return;
        }
        g_xash_startup.draw_shutdown = g_EngineMiniDrawShutdown;
    }

    void RecordXashDrawPicFromWad()
    {
        // The retail site installs a replacement. It does not load a WAD picture during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body loads a WAD picture, so this path does not call it.
        g_xash_startup.draw_pic_from_wad = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->Draw_PicFromWad.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniDrawPicFromWad == nullptr)
        {
            return;
        }
        g_xash_startup.draw_pic_from_wad = g_EngineMiniDrawPicFromWad;
    }

    void RecordXashUpdatePalette()
    {
        // The retail site installs a replacement. It does not update the palette during init.
        // A null function pointer is not a call. A null hook is not a call.
        // The process body updates the palette and can write gamma tables, so this path
        // does not call it.
        g_xash_startup.update_palette = nullptr;
        if (g_xash_engine == nullptr || g_xash_engine->V_UpdatePalette.GetHook() != nullptr)
        {
            return;
        }
        if (g_EngineMiniUpdatePalette == nullptr)
        {
            return;
        }
        g_xash_startup.update_palette = g_EngineMiniUpdatePalette;
    }

    bool XashStartupIsActive()
    {
        return g_xash_startup.engine_is_xash;
    }

    template<class T>
    void FillXashPointer(T*& field)
    {
        // One object per type. Same-typed fields share it. This path only
        // needs the pointers to be non-null; it does not read the values.
        if (field == nullptr)
        {
            static T storage{};
            field = &storage;
        }
    }

    bool CopyMountedGameDirectory(char* buffer, int capacity)
    {
        if (buffer == nullptr || capacity <= 1)
        {
            return false;
        }
        buffer[0] = '\0';
#ifndef _WIN32
        using DirectoryFn = int (*)(char*, int);
        auto directory = reinterpret_cast<DirectoryFn>(dlsym(RTLD_DEFAULT, "csr_host_filesystem_directory"));
        if (directory == nullptr)
        {
            return false;
        }
        char path[4096];
        if (directory(path, static_cast<int>(sizeof(path))) == 0 || path[0] == '\0')
        {
            return false;
        }
        const char* slash = std::strrchr(path, '/');
        const char* leaf = path;
        if (slash != nullptr)
        {
            if (slash[1] == '\0')
            {
                return false;
            }
            leaf = slash + 1;
        }
        if (leaf[0] == '\0')
        {
            return false;
        }
        std::snprintf(buffer, static_cast<size_t>(capacity), "%s", leaf);
        return buffer[0] != '\0';
#else
        return false;
#endif
    }

    void XashGetGameDir(char* buffer)
    {
        if (buffer == nullptr)
        {
            return;
        }
        if (!CopyMountedGameDirectory(buffer, goldsrc::MAX_OSPATH))
        {
            buffer[0] = '\0';
        }
    }

    bool SupplyXashEngineTable()
    {
        g_engfuncs.pfnGetGameDir = nullptr;
        nitroapi::EngineData* engine = g_xash_engine;
        if (engine == nullptr)
        {
            return false;
        }

        // The frame writes these three fields directly. A planted pointer stays.
        if (engine->realtime == nullptr)
        {
            engine->realtime = csr_xash_engine_realtime_ptr();
        }
        if (engine->host_frametime == nullptr)
        {
            engine->host_frametime = csr_xash_engine_frametime_ptr();
        }
        if (realtime == nullptr)
        {
            realtime = engine->realtime;
        }
        if (host_frametime == nullptr)
        {
            host_frametime = engine->host_frametime;
        }

        FillXashPointer(engine->currenttexture);
        FillXashPointer(engine->client_state);
        FillXashPointer(engine->client_static);
        FillXashPointer(engine->cl_enginefunc);
        FillXashPointer(engine->enginefuncs);
        FillXashPointer(engine->modfuncs);
        FillXashPointer(engine->cldll_func);
        FillXashPointer(engine->net_message);
        FillXashPointer(engine->net_from);
        FillXashPointer(engine->msg_readcount);
        FillXashPointer(engine->realtime);
        FillXashPointer(engine->host_frametime);
        FillXashPointer(engine->gfExtendedError);
        FillXashPointer(engine->gszDisconnectReason);
        FillXashPointer(engine->gDownloadFile);
        FillXashPointer(engine->cmd_source);
        FillXashPointer(engine->server_static);
        FillXashPointer(engine->server);
        FillXashPointer(engine->bMajorMapChange);
        FillXashPointer(engine->careerState);
        FillXashPointer(engine->loadmodel);
        FillXashPointer(engine->cl_playerindex);
        FillXashPointer(engine->loadcache);
        FillXashPointer(engine->loadbuf);
        FillXashPointer(engine->loadsize);
        FillXashPointer(engine->host_initialized);
        FillXashPointer(engine->g_engdstAddrs);
        FillXashPointer(engine->vid);
        FillXashPointer(engine->currententity);
        FillXashPointer(engine->r_blend);
        FillXashPointer(engine->r_entorigin);
        FillXashPointer(engine->g_bUserFogOn);
        FillXashPointer(engine->numTransObjs);
        FillXashPointer(engine->transObjects);
        FillXashPointer(engine->mtexenabled);
        FillXashPointer(engine->r_worldentity);
        FillXashPointer(engine->mirror);
        FillXashPointer(engine->c_brush_polys);
        FillXashPointer(engine->c_alias_polys);
        FillXashPointer(engine->cl_numvisedicts);
        FillXashPointer(engine->cl_visedicts);
        FillXashPointer(engine->isFogEnabled);
        FillXashPointer(engine->r_refdef);
        FillXashPointer(engine->scr_fov_value);
        FillXashPointer(engine->g_SND_VoiceOverdrive);
        FillXashPointer(engine->cszrawsentences);
        FillXashPointer(engine->rgpszrawsentence);
        FillXashPointer(engine->cl_elights);
        FillXashPointer(engine->scr_con_current);
        FillXashPointer(engine->scr_downloading);
        FillXashPointer(engine->cl_entities);
        FillXashPointer(engine->noclip_anglehack);
        FillXashPointer(engine->developer);
        FillXashPointer(engine->cmd_argc);
        FillXashPointer(engine->cmd_argv);
        FillXashPointer(engine->cvar_vars);
        FillXashPointer(engine->key_dest);
        FillXashPointer(engine->known_sfx);
        FillXashPointer(engine->num_sfx);
        FillXashPointer(engine->gHostSpawnCount);
        FillXashPointer(engine->net_local_adr);
        FillXashPointer(engine->ip_sockets);
        FillXashPointer(engine->g_GameServerAddress);
        FillXashPointer(engine->g_LastScreenUpdateTime);
        FillXashPointer(engine->maxTransObjs);

        const void* required[] = {
            engine->currenttexture,
            engine->client_state,
            engine->client_static,
            engine->cl_enginefunc,
            engine->enginefuncs,
            engine->modfuncs,
            engine->cldll_func,
            engine->net_message,
            engine->realtime,
            engine->host_frametime,
            engine->server_static,
            engine->server,
            engine->vid,
            engine->r_refdef,
            engine->cl_entities,
            engine->ip_sockets,
        };
        for (const void* pointer : required)
        {
            if (pointer == nullptr)
            {
                return false;
            }
        }

        // Other function pointers in the table stay null. pfnGetGameDir is the
        // mounted directory name from this process. An empty mount leaves it
        // null. This return does not call it, does not call Sys_Error, and
        // does not run the retail hooks.
        char mounted_game[goldsrc::MAX_OSPATH];
        if (engine->enginefuncs != nullptr && CopyMountedGameDirectory(mounted_game, goldsrc::MAX_OSPATH))
        {
            engine->enginefuncs->pfnGetGameDir = XashGetGameDir;
            g_engfuncs.pfnGetGameDir = XashGetGameDir;
        }
        else if (engine->enginefuncs != nullptr)
        {
            engine->enginefuncs->pfnGetGameDir = nullptr;
        }
        ++g_xash_startup.initializing_calls;
        return true;
    }

    void CopyField(char* dest, size_t size, const char* src)
    {
        if (size == 0)
        {
            return;
        }
        if (src == nullptr)
        {
            dest[0] = '\0';
            return;
        }
        std::snprintf(dest, size, "%s", src);
    }

    void XashAcceptArgv(int argc, char** argv)
    {
        ++g_xash_startup.argv_calls;
        g_xash_startup.argc = argc;
        // Argv parsing does not read the retail pointer table.
        COM_InitArgv(argc, argv);
    }

    void XashAcceptMode(void* window, void* device, void* context, const char* driver, const char* cmdline, int result)
    {
        ++g_xash_startup.mode_calls;
        g_xash_startup.window = window;
        g_xash_startup.device = device;
        g_xash_startup.context = context;
        g_xash_startup.mode_result = result;
        CopyField(g_xash_startup.driver, sizeof(g_xash_startup.driver), driver);
        CopyField(g_xash_startup.mode_cmdline, sizeof(g_xash_startup.mode_cmdline), cmdline);
        if (g_xash_startup.engine_is_xash)
        {
            OnGameInitializing(window, reinterpret_cast<HDC*>(device), reinterpret_cast<HGLRC*>(context), driver, cmdline, result != 0);
        }
    }

    void XashAcceptInitGame(char* cmdline, char* basedir, void* window, int dedicated, int result)
    {
        ++g_xash_startup.init_calls;
        g_xash_startup.window = window;
        g_xash_startup.dedicated = dedicated;
        g_xash_startup.init_result = result;
        CopyField(g_xash_startup.init_cmdline, sizeof(g_xash_startup.init_cmdline), cmdline);
        CopyField(g_xash_startup.basedir, sizeof(g_xash_startup.basedir), basedir);
    }

    void XashAcceptShutdown()
    {
        ++g_xash_startup.shutdown_calls;
    }

    void XashAcceptError(const char* message)
    {
        ++g_xash_startup.error_calls;
        CopyField(g_xash_startup.error, sizeof(g_xash_startup.error), message);
    }

} // namespace

void EngineMini_BindXashStartup(nitroapi::EngineData* engine, bool engine_is_xash)
{
    g_xash_engine = engine;
    g_xash_startup = {};
    g_xash_startup.engine_is_xash = engine_is_xash;
    g_EngineMiniNetSendPacket = NET_SendPacket;
    g_EngineMiniNetchanCopyFileFragments = Netchan_CopyFileFragments;
    g_EngineMiniSvcTimeScale = CL_Parse_Timescale;
    g_EngineMiniSvcSendCvarValue = CL_Send_CvarValue;
    g_EngineMiniHttpUpdate = CL_HTTPUpdate;
    g_EngineMiniHttpStop = CL_HTTPStop_f;
    g_EngineMiniStartResourceDownloading = CL_StartResourceDownloading;
    g_EngineMiniReadPackets = CL_ReadPackets;
    g_EngineMiniRequestMissingResources = CL_RequestMissingResources;
    g_EngineMiniDisconnect = CL_Disconnect;
    g_EngineMiniClearState = CL_ClearState;
    g_EngineMiniHostMap = Host_Map_f;
    g_EngineMiniHostFilterTime = Host_FilterTime;
    g_EngineMiniModClearAll = Mod_ClearAll;
    g_EngineMiniModFindName = Mod_FindName;
    g_EngineMiniModPrint = Mod_Print;
    g_EngineMiniModValidateCRC = Mod_ValidateCRC;
    g_EngineMiniModNeedCRC = Mod_NeedCRC;
    g_EngineMiniModLoadModel = Mod_LoadModel;
    g_EngineMiniModForName = Mod_ForName;
    g_EngineMiniModExtradata = Mod_Extradata;
    g_EngineMiniSprInit = SPR_Init;
    g_EngineMiniSprShutdown = SPR_Shutdown;
    g_EngineMiniSprShutdownNoModelFree = SPR_Shutdown_NoModelFree;
    g_EngineMiniSprLoad = SPR_Load;
    g_EngineMiniSprDraw = SPR_Draw;
    g_EngineMiniSprDrawAdditive = SPR_DrawAdditive;
    g_EngineMiniSprDrawHoles = SPR_DrawHoles;
    g_EngineMiniSprDrawGeneric = SPR_DrawGeneric;
    g_EngineMiniSprEnableScissor = SPR_EnableScissor;
    g_EngineMiniSprDisableScissor = SPR_DisableScissor;
    g_EngineMiniSprSet = SPR_Set;
    g_EngineMiniSprWidth = SPR_Width;
    g_EngineMiniSprHeight = SPR_Height;
    g_EngineMiniSprFrames = SPR_Frames;
    g_EngineMiniSprGetModelPointer = SPR_GetModelPointer;
    g_EngineMiniRStudioReloadSkin = R_StudioReloadSkin;
    g_EngineMiniSetCrosshair = SetCrosshair;
    g_EngineMiniDrawCrosshair = DrawCrosshair;
    g_EngineMiniRRenderView = R_RenderView;
    g_EngineMiniGLInit = GL_Init;
    g_EngineMiniGLConfig = GL_Config;
    g_EngineMiniGLSelectTexture = reinterpret_cast<EngineMiniGLSelectTextureFn>(GL_SelectTexture);
    g_EngineMiniCLConnectClient = CL_ConnectClient;
    g_EngineMiniCLClearClientState = CL_ClearClientState;
    g_EngineMiniGLLoadTexture = reinterpret_cast<EngineMiniGLLoadTextureFn>(GL_LoadTexture);
    g_EngineMiniGLLoadTexture2 = reinterpret_cast<EngineMiniGLLoadTexture2Fn>(GL_LoadTexture2);
    g_EngineMiniGLUnloadTextures = GL_UnloadTextures;
    g_EngineMiniGLUnloadTexture = GL_UnloadTexture;
    g_EngineMiniGLBind = GL_Bind;
    g_EngineMiniDrawInit = Draw_Init;
    g_EngineMiniDrawShutdown = Draw_Shutdown;
    g_EngineMiniDrawPicFromWad = Draw_PicFromWad;
    g_EngineMiniUpdatePalette = V_UpdatePalette;

    csr_xash_engine_startup_t startup{};
    if (engine != nullptr)
    {
        if (engine->COM_InitArgv.GetHook() == nullptr)
        {
            startup.argv = XashAcceptArgv;
        }
        if (engine->GL_SetMode.GetHook() == nullptr)
        {
            startup.mode = XashAcceptMode;
        }
        if (engine->Sys_InitGame.GetHook() == nullptr)
        {
            startup.init_game = XashAcceptInitGame;
        }
        if (engine->Host_Shutdown.GetHook() == nullptr)
        {
            startup.shutdown = XashAcceptShutdown;
        }
        if (engine->Sys_Error.GetHook() == nullptr)
        {
            startup.error = XashAcceptError;
        }
    }
    csr_xash_engine_bind_startup(&startup);
}

const EngineMiniXashStartup* EngineMini_XashStartup()
{
    return &g_xash_startup;
}

extern "C" void EngineMini_ReadXashStartup(
    int* argv_calls,
    int* mode_calls,
    int* init_calls,
    int* shutdown_calls,
    int* error_calls)
{
    if (argv_calls != nullptr)
    {
        *argv_calls = g_xash_startup.argv_calls;
    }
    if (mode_calls != nullptr)
    {
        *mode_calls = g_xash_startup.mode_calls;
    }
    if (init_calls != nullptr)
    {
        *init_calls = g_xash_startup.init_calls;
    }
    if (shutdown_calls != nullptr)
    {
        *shutdown_calls = g_xash_startup.shutdown_calls;
    }
    if (error_calls != nullptr)
    {
        *error_calls = g_xash_startup.error_calls;
    }
}

extern "C" int EngineMini_CheckParm(const char* parm)
{
    if (parm == nullptr)
    {
        return 0;
    }
    return COM_CheckParm(parm);
}

void EngineMini_NoteHostMapCall()
{
    ++g_xash_startup.host_map_calls;
}

void EngineMini_NoteHostFilterCall()
{
    ++g_xash_startup.host_filter_calls;
}

extern "C" int EngineMini_HostMapCalls(void)
{
    return g_xash_startup.host_map_calls;
}

extern "C" goldsrc::client_state_t* EngineMini_ClientState(void)
{
    if (g_xash_engine == nullptr)
    {
        return nullptr;
    }
    return g_xash_engine->client_state;
}

extern "C" int EngineMini_BindRendererWorld(void)
{
    if (g_xash_engine == nullptr || g_xash_engine->client_state == nullptr)
    {
        return 0;
    }
    goldsrc::model_t* world = g_xash_engine->client_state->worldmodel;
    // Assigns the loaded model. A null world leaves the previous pointer.
    // Does not call R_NewMap or R_DrawWorld.
    if (world == nullptr || g_xash_engine->r_worldentity == nullptr)
    {
        return 0;
    }
    p_r_worldentity = g_xash_engine->r_worldentity;
    p_r_worldentity->model = world;
    return 1;
}

extern "C" goldsrc::cl_entity_t* EngineMini_RendererWorldEntity(void)
{
    return p_r_worldentity;
}

extern "C" int EngineMini_RNewMapHooked(void)
{
    if (g_xash_engine == nullptr)
    {
        return 0;
    }
    return g_xash_engine->R_NewMap.GetHook() != nullptr ? 1 : 0;
}

extern "C" int EngineMini_RDrawWorldHooked(void)
{
    if (g_xash_engine == nullptr)
    {
        return 0;
    }
    return g_xash_engine->R_DrawWorld.GetHook() != nullptr ? 1 : 0;
}

static int g_world_draw_calls = 0;
static int g_world_draw_vertex_count = 0;
static float g_world_draw_vertices[64 * 3]{};

extern "C" int EngineMini_RendererWorldBound(void)
{
    return p_r_worldentity != nullptr && p_r_worldentity->model != nullptr ? 1 : 0;
}

extern "C" int EngineMini_DrawRendererWorld(void)
{
    ++g_world_draw_calls;
    g_world_draw_vertex_count = 0;
    if (p_r_worldentity == nullptr || p_r_worldentity->model == nullptr)
    {
        return 0;
    }
    const goldsrc::model_t* model = p_r_worldentity->model;
    if (model->numsurfaces < 1 || model->surfaces == nullptr || model->vertexes == nullptr || model->edges == nullptr ||
        model->surfedges == nullptr || model->numvertexes < 1 || model->numedges < 1 || model->numsurfedges < 1)
    {
        return 0;
    }
    for (int face = 0; face < model->numsurfaces; ++face)
    {
        const goldsrc::msurface_t& surface = model->surfaces[face];
        if (surface.numedges <= 0 || surface.firstedge < 0 || surface.firstedge >= model->numsurfedges)
        {
            continue;
        }
        if (surface.numedges > model->numsurfedges - surface.firstedge)
        {
            continue;
        }
        for (int edge_index = 0; edge_index < surface.numedges; ++edge_index)
        {
            if (g_world_draw_vertex_count >= 64)
            {
                break;
            }
            const int surfedge = model->surfedges[surface.firstedge + edge_index];
            const int edge_id = surfedge < 0 ? -surfedge : surfedge;
            if (edge_id < 0 || edge_id >= model->numedges)
            {
                continue;
            }
            const goldsrc::medge_t& edge = model->edges[edge_id];
            const unsigned int vertex_id = surfedge < 0 ? edge.v[1] : edge.v[0];
            if (vertex_id >= static_cast<unsigned int>(model->numvertexes))
            {
                continue;
            }
            const float* position = model->vertexes[vertex_id].position;
            float* dest = g_world_draw_vertices + g_world_draw_vertex_count * 3;
            dest[0] = position[0];
            dest[1] = position[1];
            dest[2] = position[2];
            ++g_world_draw_vertex_count;
        }
    }
    return g_world_draw_vertex_count;
}

extern "C" int EngineMini_WorldDrawCalls(void)
{
    return g_world_draw_calls;
}

extern "C" int EngineMini_WorldDrawVertexCount(void)
{
    return g_world_draw_vertex_count;
}

extern "C" const float* EngineMini_WorldDrawVertices(void)
{
    return g_world_draw_vertices;
}

extern "C" void EngineMini_ReadFrameGuard(int* host_filter_calls, int* stored_functions, int* nitro_hooks)
{
    if (host_filter_calls != nullptr)
    {
        *host_filter_calls = g_xash_startup.host_filter_calls;
    }
    if (stored_functions != nullptr)
    {
        int count = 0;
        if (g_xash_startup.net_send_packet != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.netchan_copy != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.svc_timescale != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.svc_sendcvarvalue != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.http_update != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.http_stop != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.start_resource != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.read_packets != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.request_missing != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.disconnect != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.clear_state != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.host_map != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.host_filter != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_clear != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_find != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_print != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_validate != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_need != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_load != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_forname != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.mod_extradata != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_init != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_shutdown != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_shutdown_nomodelfree != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_load != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_draw != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_draw_additive != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_draw_holes != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_draw_generic != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_enable_scissor != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_disable_scissor != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_set != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_width != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_height != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_frames != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.spr_get_model_pointer != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.studio_reload_skin != nullptr)
        {
            ++count;
        }
        if (g_xash_startup.set_crosshair != nullptr)
        {
            ++count;
        }
        *stored_functions = count;
    }
    if (nitro_hooks != nullptr)
    {
        int count = 0;
        if (g_xash_engine != nullptr)
        {
            auto count_hook = [&count](const auto& function) {
                if (function.GetHook() != nullptr)
                {
                    ++count;
                }
            };
            count_hook(g_xash_engine->NET_SendPacket);
            count_hook(g_xash_engine->Netchan_CopyFileFragments);
            count_hook(g_xash_engine->SVC_TimeScale);
            count_hook(g_xash_engine->SVC_SendCvarValue);
            count_hook(g_xash_engine->CL_HTTPUpdate);
            count_hook(g_xash_engine->CL_HTTPStop_f);
            count_hook(g_xash_engine->CL_StartResourceDownloading);
            count_hook(g_xash_engine->CL_ReadPackets);
            count_hook(g_xash_engine->CL_RequestMissingResources);
            count_hook(g_xash_engine->CL_Disconnect);
            count_hook(g_xash_engine->CL_ClearState);
            count_hook(g_xash_engine->Host_Map_f);
            count_hook(g_xash_engine->Host_FilterTime);
            count_hook(g_xash_engine->Mod_ClearAll);
            count_hook(g_xash_engine->Mod_FindName);
            count_hook(g_xash_engine->Mod_Print);
            count_hook(g_xash_engine->Mod_ValidateCRC);
            count_hook(g_xash_engine->Mod_NeedCRC);
            count_hook(g_xash_engine->Mod_LoadModel);
            count_hook(g_xash_engine->Mod_ForName);
            count_hook(g_xash_engine->Mod_Extradata);
            count_hook(g_xash_engine->SPR_Init);
            count_hook(g_xash_engine->SPR_Shutdown);
            count_hook(g_xash_engine->SPR_Shutdown_NoModelFree);
            count_hook(g_xash_engine->SPR_Load);
            count_hook(g_xash_engine->SPR_Draw);
            count_hook(g_xash_engine->SPR_DrawAdditive);
            count_hook(g_xash_engine->SPR_DrawHoles);
            count_hook(g_xash_engine->R_NewMap);
            count_hook(g_xash_engine->R_DrawWorld);
        }
        *nitro_hooks = count;
    }
}

class EngineMini : public EngineMiniInterface
{
    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubs_{};
    bool is_uninitializing_{};

public:
    void Init(nitroapi::NitroApiInterface* nitro_api, NextClientVersion client_version, AnalyticsInterface* analytics) override
    {
        if (analytics)
            analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " EngineMini::Init");

        EngineMiniInitialize(nitro_api, client_version, analytics);

        nitroapi::EngineData* engine_data = nitro_api->GetEngineData();
        const nitroapi::BuildVersion* build = nitro_api->GetBuildVersion();
        const bool engine_is_xash = build != nullptr && build->family == nitroapi::BuildFamily::Xash;

        if (engine_data->COM_InitArgv.GetHook() != nullptr)
        {
            unsubs_.emplace_back(engine_data->COM_InitArgv |= [](int argc, char** argv, const auto& next) {
                COM_InitArgv(argc, argv);
                next->Invoke(argc, argv);
            });
        }

        if (engine_data->GL_SetMode.GetHook() != nullptr)
        {
            unsubs_.emplace_back(engine_data->GL_SetMode += [](void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result) {
                OnGameInitializing(mainwindow, pmaindc, pbaseRC, pszDriver, pszCmdLine, result);
            });
        }

        if (engine_data->Sys_InitGame.GetHook() != nullptr)
        {
            unsubs_.emplace_back(engine_data->Sys_InitGame += [](char* lpOrgCmdLine, char* pBaseDir, void* pwnd, int bIsDedicated, bool ret) {
                OnGameInitialized();
            });
        }

        if (engine_data->Host_Shutdown.GetHook() != nullptr)
        {
            unsubs_.emplace_back(engine_data->Host_Shutdown |= [this](const auto& next) {
                if (!is_uninitializing_)
                {
                    is_uninitializing_ = true;
                    OnGameUninitializing();
                }

                next->Invoke();

                SPR_ResetState();
                Mod_ResetState();
            });
        }

        if (engine_data->Sys_Error.GetHook() != nullptr)
        {
            unsubs_.emplace_back(engine_data->Sys_Error |= [](const char* error, const auto& next) {
                render::GetLifecycle().OnFatalShutdown();
                CL_DeleteHttpDownloadManager();
                EngineMiniStopWorkers();
                ncl_perf::Profiling_Stop();
                next->Invoke(error);
            });
        }

        EngineMini_BindXashStartup(engine_data, engine_is_xash);
    }

    void Uninitialize() override
    {
        EngineMini_BindXashStartup(nullptr, false);

        if (g_Analytics)
            g_Analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " EngineMini::Uninitialize");

        for (auto &unsubscriber : unsubs_)
            unsubscriber->Unsubscribe();
        unsubs_.clear();

        EngineMiniUninitialize();
    }

    void GetVersion(char* buffer, int size) override
    {
        if (buffer != nullptr)
            snprintf(buffer, size, "%s", ENGINE_MINI_INTERFACE_VERSION ", " __DATE__ " " __TIME__);
    }

    HttpDownloadManagerInterface* GetHttpDownloadManager() override
    {
        return CL_GetHttpDownloadManager();
    }

    bool AddDownloadLogger(DownloadFileLoggerInterface* logger) override
    {
        return CL_AddDownloadFileLogger(logger);
    }

    bool RemoveDownloadLogger(DownloadFileLoggerInterface* logger) override
    {
        return CL_RemoveDonwloadFileLogger(logger);
    }

    bool AddCmdLogger(CommandLoggerInterface* logger) override
    {
        return PROTECTOR_AddCmdLogger(logger);
    }

    bool RemoveCmdLogger(CommandLoggerInterface* logger) override
    {
        return PROTECTOR_RemoveCmdLogger(logger);
    }

    MatchmakingServersInterface* GetSteamMatchmakingServers() override
    {
        return g_pMatchmakingServers.get();
    }
    
    void SetNclmVerificator(NclmVerificatorInterface* handler) override
    {
        g_NclmVerificator = handler;
    }

    TextRenderInterface* GetTextRender() override
    {
#ifdef _WIN32
        return TextRender_Get();
#else
        return nullptr;
#endif
    }

    render::MaterialEditInterface* GetMaterialEdit() override
    {
        return render::MaterialEdit_Get();
    }
};

EXPOSE_SINGLE_INTERFACE(EngineMini, EngineMiniInterface, ENGINE_MINI_INTERFACE_VERSION);

namespace performance
{
    static IBaseInterface* CreatePerformanceMonitorInterface()
    {
        return reinterpret_cast<IBaseInterface*>(&performance::GetMonitor());
    }
    EXPOSE_INTERFACE_FN(CreatePerformanceMonitorInterface, PerformanceMonitorInterface, PERFORMANCE_MONITOR_INTERFACE_VERSION);
} // namespace performance
