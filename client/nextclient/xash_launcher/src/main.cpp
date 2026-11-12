#include "xash_launcher/xash_launcher.h"

#include <unistd.h>

#include <filesystem>
#include <string>

namespace
{

std::string ExecutableDirectory()
{
    char buffer[4096];
    const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(buffer) - 1)
    {
        return {};
    }
    buffer[length] = '\0';
    return std::filesystem::path(buffer).parent_path().string();
}

std::string Beside(const std::string& directory, const char* name)
{
    if (directory.empty())
    {
        return {};
    }
    return (std::filesystem::path(directory) / name).string();
}

} // namespace

// The human executable keeps this symbol as main. The launcher tests compile
// this file under another name and call it: a content root makes this call
// block until quit. The planned command is not executed.
#ifndef CSRETRO_ENTRY_NAME
#define CSRETRO_ENTRY_NAME main
#endif

int CSRETRO_ENTRY_NAME(int argc, char** argv)
{
    const char* content_root = nullptr;
    if (argc > 1 && argv != nullptr && argv[1] != nullptr && argv[1][0] != '\0')
    {
        content_root = argv[1];
    }
    const char* map_name = nullptr;
    if (argc > 2 && argv != nullptr && argv[2] != nullptr && argv[2][0] != '\0')
    {
        map_name = argv[2];
    }

    const std::string directory = ExecutableDirectory();
    const std::string host = Beside(directory, "xash_host.so");
    const std::string surface = Beside(directory, "xash_surface.so");
    const std::string client = Beside(directory, "client_mini.dll");
    const std::string vgui = Beside(directory, "vgui2.so");
    const std::string gameui = Beside(directory, "cstrike/cl_dlls/gameui.so");
    const std::string steam = Beside(directory, "libsteam_api.so");
    const std::string engine = Beside(directory, "next_engine_mini.dll");
    const std::string nitro = Beside(directory, "nitro_api2.dll");

    csr::LauncherProcess session{};
    const int ran = csr::RunXashLauncher(
        content_root,
        host.c_str(),
        surface.c_str(),
        client.c_str(),
        vgui.c_str(),
        gameui.c_str(),
        steam.c_str(),
        engine.c_str(),
        nitro.c_str(),
        &session,
        640,
        480,
        0.125,
        map_name);
    csr::CloseXashLauncher(&session);
    return ran == 1 ? 0 : 1;
}
