#include "xash_launcher/xash_launcher.h"

#include <sys/stat.h>

#include <string>

namespace csr
{
namespace
{

bool IsDirectory(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    struct stat info
    {
    };
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool IsRegularFile(const char* path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    struct stat info
    {
    };
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

bool Quote(const char* path, std::string* quoted)
{
    if (path == nullptr || path[0] == '\0' || std::string(path).find('\'') != std::string::npos)
    {
        return false;
    }
    *quoted = std::string("'") + path + "'";
    return true;
}

} // namespace

LauncherPlan PlanXashLauncher(
    const char* content_root,
    const char* host_path,
    const char* surface_path,
    const char* client_path,
    const char* vgui_path,
    const char* gameui_path,
    const char* steam_api_path)
{
    LauncherPlan plan;
    if (!IsDirectory(content_root) || !IsRegularFile(host_path) || !IsRegularFile(surface_path) ||
        !IsRegularFile(client_path) || !IsRegularFile(vgui_path) || !IsRegularFile(gameui_path) ||
        !IsRegularFile(steam_api_path))
    {
        return plan;
    }

    const char* paths[] = {host_path, surface_path, client_path, vgui_path, gameui_path, content_root, steam_api_path};
    std::string quoted[7];
    for (int index = 0; index < 7; ++index)
    {
        if (!Quote(paths[index], &quoted[index]))
        {
            return plan;
        }
    }

    plan.command = quoted[0] + " --surface " + quoted[1] + " --client " + quoted[2] + " --vgui " + quoted[3] + " --gameui " +
        quoted[4] + " --filesystem-root " + quoted[5] + " --steam-api " + quoted[6];
    plan.ready = true;
    return plan;
}

} // namespace csr
