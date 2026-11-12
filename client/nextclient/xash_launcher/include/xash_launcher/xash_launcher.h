#pragma once

#include <string>

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

// Opens the host the way OpenXashLauncher does, then calls csr_host_run.
// The window stays mapped until the recorded client command is quit, and
// the call then destroys that window. PresentXashLauncher is the one-frame
// present and is not this call. Does not exec the planned command, does not
// call RunFrame, and does not start Steam. Returns 0 when the session is
// missing, the size is empty, the open fails, or a frame fails. This call
// does not play Counter-Strike.
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
    double delta);

void CloseXashLauncher(LauncherProcess* session);

} // namespace csr
