#pragma once

#include <string>

namespace goldsrc
{
struct client_state_t;
}

// Plans the Xash launcher command from the pieces that already exist.
// Does not exec, does not start the game, and does not start Steam.
// A missing content root is a failure and is not replaced with another path.

namespace csr
{

struct LauncherPlan
{
    bool ready = false;
    std::string command;
};

LauncherPlan PlanXashLauncher(
    const char* content_root,
    const char* host_path,
    const char* surface_path,
    const char* client_path,
    const char* vgui_path,
    const char* gameui_path,
    const char* steam_api_path,
    const char* engine_mini_path,
    const char* nitro_api_path);

// Opens that plan in this process and calls IGameUI::Start. Does not exec the
// command, does not start the game or Steam, and does not call RunFrame. A
// failed open leaves the host closed. steam is the libsteam_api.so handle
// while open is true.
struct LauncherProcess
{
    bool open = false;
    void* steam = nullptr;
};

int OpenXashLauncher(
    const char* content_root,
    const char* host_path,
    const char* surface_path,
    const char* client_path,
    const char* vgui_path,
    const char* gameui_path,
    const char* steam_api_path,
    const char* engine_mini_path,
    const char* nitro_api_path,
    LauncherProcess* session);

// One frame of the opened session: ActivateGameUI, then paint at wide x tall.
// Does not call csr_host_run, does not exec the planned command, does not
// call RunFrame, and does not start Steam. Returns 0 when the session is
// closed or the paint does not reach a panel. Recorded commands are not
// executed.
int PresentXashLauncher(LauncherProcess* session, int wide, int tall);

// Reads cstrike/maps/<map_name>.bsp under content_root. A version 30 header
// whose lumps sit inside the file records map_name in state->levelname, the
// checksum of the bytes read in state->mapCRC, and a brush model in
// state->worldmodel. The model name is map_name. A non-empty entities lump
// is copied onto that model. Vertex, edge, surfedge and face lumps are copied
// too. One disk face becomes one surface. Texinfo and textures are not
// copied. Lighting bytes are not lightmap surfaces, and GL_BuildLightmaps
// is not called. A missing file, a short file, or another version leaves
// state unchanged and returns 0. Host_Map_f is not this call.
// Mod_LoadBrushModel is not this call.
int LoadXashMap(const char* content_root, const char* map_name, goldsrc::client_state_t* state);

// Loads the map and, only when that load builds a world model, points
// r_worldentity->model at it and draws that model once. A failed load
// returns 0, does not write that field, and does not draw. eng()->R_NewMap
// and eng()->R_DrawWorld are not installed and not called. The draw does
// not open a window. The engine module has to already be loaded.
int LoadAndBindXashMap(const char* content_root, const char* map_name, const char* engine_mini_path);

// Opens the host the way OpenXashLauncher does. A map name loads that file
// onto the client state, builds its world model, binds r_worldentity->model
// to that model, and draws that model once before csr_host_run. An empty
// map name skips the load. A failed load closes the session and does not
// enter the quit loop. The draw does not open a window. csr_host_run does
// not call it.
// The window stays mapped until the recorded client command is quit, and
// the call then destroys that window. PresentXashLauncher is the one-frame
// present and is not this call. Does not exec the planned command, does not
// call RunFrame, and does not start Steam. Returns 0 when the session is
// missing, the size is empty, the open fails, the map load fails, or a
// frame fails. This call does not play Counter-Strike.
int RunXashLauncher(
    const char* content_root,
    const char* host_path,
    const char* surface_path,
    const char* client_path,
    const char* vgui_path,
    const char* gameui_path,
    const char* steam_api_path,
    const char* engine_mini_path,
    const char* nitro_api_path,
    LauncherProcess* session,
    int wide,
    int tall,
    double delta,
    const char* map_name = nullptr);

void CloseXashLauncher(LauncherProcess* session);

} // namespace csr
