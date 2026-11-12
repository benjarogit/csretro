#include "xash_launcher/xash_launcher.h"

#include "xash_host/xash_host.h"

#include <gtest/gtest.h>

#include <EGL/egl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

struct TempRoot
{
    std::string root;

    TempRoot()
    {
        char name[] = "/tmp/csretro-launcher-XXXXXX";
        if (mkdtemp(name) != nullptr)
        {
            root = name;
        }
    }

    ~TempRoot()
    {
        if (!root.empty())
        {
            rmdir(root.c_str());
        }
    }
};

} // namespace

TEST(XashLauncher, MissingRootIsAFailure)
{
    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-launcher-missing-%d", static_cast<int>(getpid()));

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        missing,
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    EXPECT_FALSE(plan.ready);
    EXPECT_TRUE(plan.command.empty());

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    char missing_surface[64];
    std::snprintf(missing_surface, sizeof(missing_surface), "/tmp/csretro-launcher-no-surface-%d", static_cast<int>(getpid()));
    const csr::LauncherPlan missing_module = csr::PlanXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        missing_surface,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    EXPECT_FALSE(missing_module.ready);
    EXPECT_TRUE(missing_module.command.empty());
}

TEST(XashLauncher, PlanNamesTheExistingPieces)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    ASSERT_TRUE(plan.ready);
    EXPECT_NE(plan.command.find(std::string("'") + XASH_HOST_FILE + "' --surface "), std::string::npos) << plan.command;
    EXPECT_NE(plan.command.find(std::string("--surface '") + XASH_SURFACE_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--client '") + XASH_CLIENT_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--vgui '") + XASH_VGUI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--gameui '") + XASH_GAMEUI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--filesystem-root '") + temp.root + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--steam-api '") + XASH_STEAM_API_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--engine '") + XASH_ENGINE_MINI_FILE + "'"), std::string::npos);
    EXPECT_NE(plan.command.find(std::string("--nitro-api '") + XASH_NITRO_API_FILE + "'"), std::string::npos);
    EXPECT_EQ(plan.command.find("hw.so"), std::string::npos);
    EXPECT_EQ(plan.command.find("steamapps"), std::string::npos);
    EXPECT_EQ(plan.command.find("RunFrame"), std::string::npos);
}

TEST(XashLauncher, OpenLoadsThePlanAndDoesNotCallRunFrame)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* menu_file = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_file, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), menu_file), std::strlen(text));
    std::fclose(menu_file);

    csr::LauncherProcess session;
    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(),
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        1);
    EXPECT_TRUE(session.open);
    EXPECT_NE(session.steam, nullptr);
    EXPECT_EQ(csr_host_pair_loaded(), 1);
    EXPECT_EQ(csr_host_gameui_loaded(), 1);
    EXPECT_EQ(csr_host_filesystem_mounted(), 1);
    EXPECT_EQ(csr_host_vgui_connected(), 1);
    EXPECT_EQ(csr_host_scheme_loaded(), 0);
    EXPECT_EQ(csr_host_gameui_initialized(), 1);
    EXPECT_GE(csr_host_gameui_child_count(), 1);
    EXPECT_NE(csr_host_vgui_dispatch(), nullptr);
    EXPECT_NE(csr_host_gameui(), nullptr);
    EXPECT_EQ(csr_host_engine_loaded(), 1);
    EXPECT_NE(csr_host_engine_check_parm("-console"), 0);
    int argv_calls = 0;
    int mode_calls = 0;
    int init_calls = 0;
    int shutdown_calls = 0;
    int error_calls = 0;
    ASSERT_EQ(csr_host_engine_startup_calls(&argv_calls, &mode_calls, &init_calls, &shutdown_calls, &error_calls), 1);
    EXPECT_EQ(argv_calls, 1);
    EXPECT_EQ(mode_calls, 1);
    EXPECT_EQ(init_calls, 1);
    EXPECT_EQ(shutdown_calls, 0);
    EXPECT_EQ(error_calls, 0);
    auto steam_running = reinterpret_cast<bool (*)()>(dlsym(session.steam, "SteamAPI_IsSteamRunning"));
    ASSERT_NE(steam_running, nullptr);
    EXPECT_FALSE(steam_running());

    csr_host_factories_t factories{};
    ASSERT_EQ(csr_host_factory_list(nullptr, 0, &factories), 1);
    ASSERT_GE(factories.count, 2);
    int code = 1;
    EXPECT_NE(factories.list[0]("VGUI_Surface026", &code), nullptr);
    EXPECT_NE(factories.list[1]("VFileSystem009", &code), nullptr);

    csr::CloseXashLauncher(&session);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(session.steam, nullptr);
    EXPECT_EQ(csr_host_engine_loaded(), 0);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
    EXPECT_EQ(csr_host_gameui_loaded(), 0);
    EXPECT_EQ(csr_host_filesystem_mounted(), 0);
    EXPECT_EQ(csr_host_gameui_initialized(), 0);
    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());

    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-launcher-missing-%d", static_cast<int>(getpid()));
    EXPECT_EQ(
        csr::OpenXashLauncher(
            missing,
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        0);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(csr_host_pair_loaded(), 0);
}

TEST(XashLauncher, PresentPaintsTheMenuWithoutRunFrame)
{
    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* menu_file = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_file, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), menu_file), std::strlen(text));
    std::fclose(menu_file);

    csr::LauncherProcess session;
    EXPECT_EQ(csr::PresentXashLauncher(&session, 640, 480), 0);

    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(),
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        1);
    EXPECT_EQ(csr_host_gameui_started(), 1);
    EXPECT_STREQ(csr_host_last_client_command(), "");

    ASSERT_EQ(csr::PresentXashLauncher(&session, 640, 480), 1);
    EXPECT_STREQ(csr_host_last_client_command(), "setpause");
    EXPECT_EQ(csr::PresentXashLauncher(&session, 0, 480), 0);
    EXPECT_TRUE(session.open);

    csr::CloseXashLauncher(&session);
    EXPECT_EQ(csr::PresentXashLauncher(&session, 640, 480), 0);
    EXPECT_FALSE(session.open);

    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}

namespace
{

bool MapsContain(const std::string& fragment)
{
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line))
    {
        if (line.find(fragment) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

bool ReadOpenSockets(std::vector<std::string>* links)
{
    links->clear();
    std::error_code error;
    std::filesystem::directory_iterator it("/proc/self/fd", error);
    if (error)
    {
        return false;
    }
    const std::filesystem::directory_iterator end;
    for (; !error && it != end; it.increment(error))
    {
        std::error_code link_error;
        const std::filesystem::path target = std::filesystem::read_symlink(it->path(), link_error);
        if (!link_error && target.string().rfind("socket:", 0) == 0)
        {
            links->push_back(target.string());
        }
    }
    if (error)
    {
        return false;
    }
    std::sort(links->begin(), links->end());
    return true;
}

bool PeerSocketPath(const std::string& link, std::string* path)
{
    std::error_code error;
    const std::filesystem::directory_iterator end;
    for (std::filesystem::directory_iterator it("/proc/self/fd", error); !error && it != end; it.increment(error))
    {
        std::error_code link_error;
        const std::filesystem::path target = std::filesystem::read_symlink(it->path(), link_error);
        if (link_error || target.string() != link)
        {
            continue;
        }
        const int fd = std::atoi(it->path().filename().string().c_str());
        sockaddr_un peer = {};
        socklen_t length = sizeof(peer);
        if (getpeername(fd, reinterpret_cast<sockaddr*>(&peer), &length) != 0)
        {
            return false;
        }
        const socklen_t path_offset = offsetof(sockaddr_un, sun_path);
        if (length <= path_offset)
        {
            return false;
        }
        if (peer.sun_path[0] == '\0')
        {
            path->assign(peer.sun_path + 1, static_cast<size_t>(length - path_offset - 1));
        }
        else
        {
            path->assign(peer.sun_path);
        }
        return true;
    }
    return false;
}

// The frame opens whatever DISPLAY names. This server is that display for the
// test, at the same size as the frame. It is not the game process.
struct VirtualDisplay
{
    pid_t pid = -1;
    std::string number;
    std::string previous_display;
    bool changed_display = false;
    std::string previous_software;
    bool had_software = false;
    bool changed_software = false;

    bool Start(std::string* error)
    {
        int chosen = -1;
        for (int candidate = 20; candidate < 60; ++candidate)
        {
            const std::string probe_name = ":" + std::to_string(candidate);
            Display* probe = XOpenDisplay(probe_name.c_str());
            if (probe != nullptr)
            {
                XCloseDisplay(probe);
                continue;
            }
            chosen = candidate;
            break;
        }
        if (chosen < 0)
        {
            *error = "Xvfb";
            return false;
        }
        number = std::to_string(chosen);
        const std::string name = ":" + number;
        pid = fork();
        if (pid < 0)
        {
            *error = "fork";
            return false;
        }
        if (pid == 0)
        {
            unsetenv("LD_LIBRARY_PATH");
            execlp("Xvfb", "Xvfb", name.c_str(), "-nolisten", "tcp", "-screen", "0", "640x480x24", "+extension", "GLX", nullptr);
            _exit(127);
        }
        bool ready = false;
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            Display* opened = XOpenDisplay(name.c_str());
            if (opened != nullptr)
            {
                XCloseDisplay(opened);
                ready = true;
                break;
            }
            timespec delay = {};
            delay.tv_nsec = 20 * 1000 * 1000;
            nanosleep(&delay, nullptr);
        }
        if (!ready)
        {
            *error = "Xvfb";
            return false;
        }
        const char* old_display = std::getenv("DISPLAY");
        previous_display = old_display == nullptr ? "" : old_display;
        if (setenv("DISPLAY", name.c_str(), 1) != 0)
        {
            *error = "setenv DISPLAY";
            return false;
        }
        changed_display = true;
        const char* software = std::getenv("LIBGL_ALWAYS_SOFTWARE");
        had_software = software != nullptr;
        previous_software = had_software ? software : "";
        if (setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1) != 0)
        {
            *error = "setenv LIBGL_ALWAYS_SOFTWARE";
            return false;
        }
        changed_software = true;
        return true;
    }

    ~VirtualDisplay()
    {
        if (changed_display)
        {
            if (previous_display.empty())
            {
                unsetenv("DISPLAY");
            }
            else
            {
                setenv("DISPLAY", previous_display.c_str(), 1);
            }
        }
        if (changed_software)
        {
            if (!had_software)
            {
                unsetenv("LIBGL_ALWAYS_SOFTWARE");
            }
            else
            {
                setenv("LIBGL_ALWAYS_SOFTWARE", previous_software.c_str(), 1);
            }
        }
        if (pid > 0)
        {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
        }
    }
};

} // namespace

TEST(XashLauncher, DirectFramePaintsGameUiAndAdvancesHostTime)
{
    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_before = realtime;
    const double frametime_before = frametime;
    const double oldrealtime_before = oldrealtime;

    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(csr_host_frame(640, 480, 0.125), 0);
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, nullptr, nullptr), 0);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_before);
    EXPECT_DOUBLE_EQ(frametime, frametime_before);
    EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_before);

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    FILE* menu_file = std::fopen(menu.c_str(), "wb");
    ASSERT_NE(menu_file, nullptr);
    const char* text = "\"GameMenu\"\n{\n}\n";
    ASSERT_EQ(std::fwrite(text, 1, std::strlen(text), menu_file), std::strlen(text));
    std::fclose(menu_file);

    const std::string planted_hw = temp.root + "/hw.so";
    std::filesystem::copy_file(XASH_HOST_FILE, planted_hw);
    EXPECT_FALSE(MapsContain(planted_hw));

    csr::LauncherProcess session;
    ASSERT_EQ(
        csr::OpenXashLauncher(
            temp.root.c_str(),
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session),
        1);

    void* engine = dlopen(XASH_ENGINE_MINI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(engine, nullptr);
    using GuardFn = void (*)(int*, int*, int*);
    auto guard = reinterpret_cast<GuardFn>(dlsym(engine, "EngineMini_ReadFrameGuard"));
    ASSERT_NE(guard, nullptr);
    int filter_calls = -1;
    int stored_before = -1;
    int nitro_hooks = -1;
    guard(&filter_calls, &stored_before, &nitro_hooks);
    EXPECT_EQ(filter_calls, 0);
    EXPECT_GT(stored_before, 0);
    EXPECT_EQ(nitro_hooks, 0);

    void* vgui = dlopen(XASH_VGUI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(vgui, nullptr);
    auto run_frame_count = reinterpret_cast<int (*)()>(dlsym(vgui, "csr_vgui_run_frame_count"));
    ASSERT_NE(run_frame_count, nullptr);
    const int run_frames_before = run_frame_count();

    std::vector<std::string> sockets_before;
    ASSERT_TRUE(ReadOpenSockets(&sockets_before));
    VirtualDisplay display;
    std::string display_error;
    ASSERT_TRUE(display.Start(&display_error)) << display_error;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_open = realtime;

    const csr::LauncherPlan plan = csr::PlanXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE);
    ASSERT_TRUE(plan.ready);
    std::ifstream cmdline("/proc/self/cmdline", std::ios::binary);
    std::string arguments((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());
    EXPECT_EQ(arguments.find("--surface"), std::string::npos);

    ASSERT_EQ(csr_host_frame(640, 480, 0.125), 1) << csr_host_present_error();
    void* egl_display = nullptr;
    void* egl_surface = nullptr;
    unsigned long window = 0;
    int swaps = 0;
    ASSERT_EQ(csr_host_present_query(&egl_display, &egl_surface, &window, &swaps), 1);
    EXPECT_NE(egl_display, nullptr);
    EXPECT_NE(egl_surface, nullptr);
    EXPECT_NE(window, 0u);
    EXPECT_EQ(swaps, 1);
    EXPECT_EQ(eglGetCurrentDisplay(), egl_display);
    EXPECT_EQ(eglGetCurrentSurface(EGL_DRAW), egl_surface);
    EGLint surface_wide = 0;
    EGLint surface_tall = 0;
    EXPECT_EQ(
        eglQuerySurface(static_cast<EGLDisplay>(egl_display), static_cast<EGLSurface>(egl_surface), EGL_WIDTH, &surface_wide),
        static_cast<EGLBoolean>(EGL_TRUE)
    );
    EXPECT_EQ(
        eglQuerySurface(static_cast<EGLDisplay>(egl_display), static_cast<EGLSurface>(egl_surface), EGL_HEIGHT, &surface_tall),
        static_cast<EGLBoolean>(EGL_TRUE)
    );
    EXPECT_EQ(surface_wide, 640);
    EXPECT_EQ(surface_tall, 480);
    Display* view = XOpenDisplay(nullptr);
    ASSERT_NE(view, nullptr) << "XOpenDisplay";
    XWindowAttributes attributes = {};
    ASSERT_NE(XGetWindowAttributes(view, static_cast<Window>(window), &attributes), 0);
    EXPECT_EQ(attributes.width, 640);
    EXPECT_EQ(attributes.height, 480);
    EXPECT_EQ(attributes.map_state, IsViewable);
    XImage* image = XGetImage(
        view,
        static_cast<Window>(window),
        0,
        0,
        static_cast<unsigned int>(attributes.width),
        static_cast<unsigned int>(attributes.height),
        AllPlanes,
        ZPixmap
    );
    ASSERT_NE(image, nullptr) << "XGetImage";
    const int sample_y = attributes.height - 1;
    const unsigned long corner = XGetPixel(image, 1, sample_y);
    bool presented = false;
    for (int y = sample_y - 7; y <= sample_y && !presented; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            if (XGetPixel(image, x, y) == 0xff0000)
            {
                presented = true;
            }
        }
    }
    XDestroyImage(image);
    XCloseDisplay(view);
    const char* shown = std::getenv("DISPLAY");
    EXPECT_TRUE(presented) << "DISPLAY=" << (shown == nullptr ? "" : shown) << " corner=0x" << std::hex << corner;
    EXPECT_STREQ(csr_host_last_client_command(), "setpause");
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_open + 0.125);
    EXPECT_DOUBLE_EQ(frametime, 0.125);
    EXPECT_DOUBLE_EQ(oldrealtime, realtime_open + 0.125);

    int filter_after = -1;
    int stored_after = -1;
    int nitro_after = -1;
    guard(&filter_after, &stored_after, &nitro_after);
    EXPECT_EQ(filter_after, 0);
    EXPECT_EQ(stored_after, stored_before);
    EXPECT_EQ(nitro_after, 0);
    EXPECT_EQ(run_frame_count(), run_frames_before);

    std::vector<std::string> sockets_after;
    ASSERT_TRUE(ReadOpenSockets(&sockets_after));
    std::vector<std::string> added;
    std::set_difference(sockets_after.begin(), sockets_after.end(), sockets_before.begin(), sockets_before.end(), std::back_inserter(added));
    ASSERT_FALSE(added.empty());
    const std::string x11_path = "/tmp/.X11-unix/X" + display.number;
    for (const std::string& link : added)
    {
        std::string path;
        ASSERT_TRUE(PeerSocketPath(link, &path)) << link;
        EXPECT_NE(path.find(x11_path), std::string::npos) << path;
    }
    EXPECT_FALSE(MapsContain(planted_hw));
    auto steam_running = reinterpret_cast<bool (*)()>(dlsym(session.steam, "SteamAPI_IsSteamRunning"));
    ASSERT_NE(steam_running, nullptr);
    EXPECT_FALSE(steam_running());

    const double realtime_held = realtime;
    const double frametime_held = frametime;
    const double oldrealtime_held = oldrealtime;
    EXPECT_EQ(csr_host_frame(0, 480, 0.125), 0);
    int swaps_held = 0;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, nullptr, &swaps_held), 1);
    EXPECT_EQ(swaps_held, 1);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_held);
    EXPECT_DOUBLE_EQ(frametime, frametime_held);
    EXPECT_DOUBLE_EQ(oldrealtime, oldrealtime_held);
    EXPECT_EQ(run_frame_count(), run_frames_before);
    guard(&filter_after, &stored_after, &nitro_after);
    EXPECT_EQ(filter_after, 0);
    EXPECT_EQ(stored_after, stored_before);
    EXPECT_EQ(nitro_after, 0);

    csr::CloseXashLauncher(&session);
    dlclose(vgui);
    dlclose(engine);
    std::filesystem::remove(planted_hw);
    std::remove(menu.c_str());
    rmdir((temp.root + "/Resource").c_str());
}
