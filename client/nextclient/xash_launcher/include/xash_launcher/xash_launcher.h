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
    const char* steam_api_path);

// Opens that plan in this process. Does not exec the command, does not start
// the game or Steam, and does not call RunFrame. A failed open leaves the
// host closed. steam is the libsteam_api.so handle while open is true.
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
    LauncherProcess* session);

void CloseXashLauncher(LauncherProcess* session);

} // namespace csr
