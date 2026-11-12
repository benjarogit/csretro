#include "xash_launcher/xash_launcher.h"

#include "xash_host/xash_host.h"

#include <gtest/gtest.h>

#include <vgui/IPanel.h>
#include <vgui/ISurface.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <dlfcn.h>

#include <algorithm>
#include <atomic>
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
#include <thread>
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

    bool Start(std::string* error, const char* screen = "640x480x24")
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
            execlp("Xvfb", "Xvfb", name.c_str(), "-nolisten", "tcp", "-screen", "0", screen, "+extension", "GLX", nullptr);
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

namespace
{

int g_fixture_x_error = 0;

int OnFixtureXError(Display*, XErrorEvent* event)
{
    g_fixture_x_error = event->error_code;
    return 0;
}

std::atomic<int> g_pump_moves{0};
std::atomic<int> g_pump_x{-1};
std::atomic<int> g_pump_y{-1};
std::atomic<int> g_pump_presses{0};
std::atomic<int> g_pump_code{-1};

void PumpCursorMoved(void*, int x, int y)
{
    g_pump_x.store(x);
    g_pump_y.store(y);
    g_pump_moves.fetch_add(1);
}

void PumpMousePressed(void*, int code)
{
    g_pump_code.store(code);
    g_pump_presses.fetch_add(1);
}

void PumpMouseReleased(void*, int)
{
}

// After an EGL display is terminated, a second session in that process calls
// CVGui::AllocPanel through a null PLT. Each proof here execs a fresh process.
bool RunInFreshProcess(const char* token, const char* filter)
{
    const char* isolated = std::getenv("CSR_HOST_ISOLATED");
    if (isolated != nullptr && std::strcmp(isolated, token) == 0)
    {
        return true;
    }
    const pid_t pid = fork();
    EXPECT_GE(pid, 0);
    if (pid < 0)
    {
        return false;
    }
    if (pid == 0)
    {
        if (setenv("CSR_HOST_ISOLATED", token, 1) != 0)
        {
            _exit(127);
        }
        const std::string argument = std::string("--gtest_filter=") + filter;
        execl("/proc/self/exe", "xash_launcher-tests", argument.c_str(), nullptr);
        _exit(127);
    }
    int status = 0;
    EXPECT_EQ(waitpid(pid, &status, 0), pid);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
    return false;
}

struct FrameSession
{
    TempRoot temp;
    std::string menu;
    std::string planted_hw;
    csr::LauncherProcess session{};

    bool Open()
    {
        if (temp.root.empty())
        {
            return false;
        }
        if (mkdir((temp.root + "/Resource").c_str(), 0755) != 0)
        {
            return false;
        }
        menu = temp.root + "/Resource/GameMenu.res";
        FILE* menu_file = std::fopen(menu.c_str(), "wb");
        if (menu_file == nullptr)
        {
            return false;
        }
        const char* text = "\"GameMenu\"\n{\n}\n";
        const bool wrote = std::fwrite(text, 1, std::strlen(text), menu_file) == std::strlen(text);
        std::fclose(menu_file);
        if (!wrote)
        {
            return false;
        }
        planted_hw = temp.root + "/hw.so";
        std::error_code copy_error;
        std::filesystem::copy_file(XASH_HOST_FILE, planted_hw, copy_error);
        if (copy_error)
        {
            return false;
        }
        return csr::OpenXashLauncher(
                   temp.root.c_str(),
                   XASH_HOST_FILE,
                   XASH_SURFACE_FILE,
                   XASH_CLIENT_FILE,
                   XASH_VGUI_FILE,
                   XASH_GAMEUI_FILE,
                   XASH_STEAM_API_FILE,
                   XASH_ENGINE_MINI_FILE,
                   XASH_NITRO_API_FILE,
                   &session) == 1;
    }

    ~FrameSession()
    {
        csr::CloseXashLauncher(&session);
        if (!planted_hw.empty())
        {
            std::filesystem::remove(planted_hw);
        }
        if (!menu.empty())
        {
            std::remove(menu.c_str());
        }
        if (!temp.root.empty())
        {
            rmdir((temp.root + "/Resource").c_str());
        }
    }
};

} // namespace

TEST(XashLauncher, WindowSurfaceMatchesWhenRootVisualDiffers)
{
    if (!RunInFreshProcess("visual", "XashLauncher.WindowSurfaceMatchesWhenRootVisualDiffers"))
    {
        return;
    }
    VirtualDisplay display;
    FrameSession session;
    ASSERT_TRUE(session.Open());
    std::string display_error;
    // 16-bit root. The first alpha-8 EGL config on this server is a 32-bit
    // visual, so the parent visual and the EGL visual are not the same.
    ASSERT_TRUE(display.Start(&display_error, "640x480x16")) << display_error;

    Display* view = XOpenDisplay(nullptr);
    ASSERT_NE(view, nullptr);
    const int screen = DefaultScreen(view);
    const Window root = RootWindow(view, screen);
    XWindowAttributes root_attributes{};
    ASSERT_NE(XGetWindowAttributes(view, root, &root_attributes), 0);
    const int root_depth = root_attributes.depth;
    const VisualID root_visual = XVisualIDFromVisual(root_attributes.visual);

    auto* get_platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    ASSERT_NE(get_platform, nullptr);
    EGLDisplay egl_display = get_platform(EGL_PLATFORM_X11_KHR, view, nullptr);
    ASSERT_NE(egl_display, EGL_NO_DISPLAY);
    ASSERT_EQ(eglInitialize(egl_display, nullptr, nullptr), static_cast<EGLBoolean>(EGL_TRUE));
    ASSERT_EQ(eglBindAPI(EGL_OPENGL_API), static_cast<EGLBoolean>(EGL_TRUE));
    const EGLint strict[] = {
        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_BIT,
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint config_count = 0;
    ASSERT_EQ(eglChooseConfig(egl_display, strict, &config, 1, &config_count), static_cast<EGLBoolean>(EGL_TRUE));
    ASSERT_GE(config_count, 1);
    EGLint visual_id = 0;
    ASSERT_EQ(eglGetConfigAttrib(egl_display, config, EGL_NATIVE_VISUAL_ID, &visual_id), static_cast<EGLBoolean>(EGL_TRUE));
    XVisualInfo request{};
    request.visualid = static_cast<VisualID>(visual_id);
    int visual_count = 0;
    XVisualInfo* visual = XGetVisualInfo(view, VisualIDMask, &request, &visual_count);
    ASSERT_NE(visual, nullptr);
    ASSERT_GE(visual_count, 1);
    ASSERT_NE(visual->depth, root_depth) << "fixture did not separate the parent visual from the EGL visual";
    ASSERT_NE(visual->visualid, root_visual);

    g_fixture_x_error = 0;
    XErrorHandler previous = XSetErrorHandler(OnFixtureXError);
    XSetWindowAttributes attributes{};
    attributes.colormap = XCreateColormap(view, root, visual->visual, AllocNone);
    attributes.event_mask = StructureNotifyMask;
    Window mismatched = XCreateWindow(
        view,
        root,
        0,
        0,
        64,
        64,
        0,
        visual->depth,
        InputOutput,
        visual->visual,
        CWColormap | CWEventMask,
        &attributes
    );
    XSync(view, False);
    const int create_error = g_fixture_x_error;
    if (mismatched != 0)
    {
        XDestroyWindow(view, mismatched);
        XSync(view, False);
    }
    if (attributes.colormap != 0)
    {
        XFreeColormap(view, attributes.colormap);
    }
    XSetErrorHandler(previous);
    XFree(visual);
    eglTerminate(egl_display);
    XCloseDisplay(view);
    // Without a border pixel this is BadMatch. Mesa does not return
    // EGL_BAD_CONFIG for the parent visual on this server; NVIDIA does
    // when that parent visual is paired with this deeper config.
    EXPECT_EQ(create_error, BadMatch);

    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_open = realtime;
    ASSERT_EQ(csr_host_frame(640, 480, 0.125), 1) << csr_host_present_error();
    void* presented_display = nullptr;
    void* presented_surface = nullptr;
    unsigned long window = 0;
    int swaps = 0;
    ASSERT_EQ(csr_host_present_query(&presented_display, &presented_surface, &window, &swaps), 1);
    EXPECT_NE(presented_display, nullptr);
    EXPECT_NE(presented_surface, nullptr);
    EXPECT_NE(window, 0u);
    EXPECT_EQ(swaps, 1);
    EXPECT_EQ(csr_host_present_error(), nullptr);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_open + 0.125);
}

TEST(XashLauncher, PumpRepeatsFramesAndForwardsX11Input)
{
    if (!RunInFreshProcess("pump", "XashLauncher.PumpRepeatsFramesAndForwardsX11Input"))
    {
        return;
    }
    g_pump_moves.store(0);
    g_pump_x.store(-1);
    g_pump_y.store(-1);
    g_pump_presses.store(0);
    g_pump_code.store(-1);

    VirtualDisplay display;
    FrameSession session;
    ASSERT_TRUE(session.Open());

    void* vgui = dlopen(XASH_VGUI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(vgui, nullptr);
    auto run_frame_count = reinterpret_cast<int (*)()>(dlsym(vgui, "csr_vgui_run_frame_count"));
    ASSERT_NE(run_frame_count, nullptr);
    const int run_frames_before = run_frame_count();

    csr_host_vgui_t input{};
    input.cursor_moved = PumpCursorMoved;
    input.mouse_pressed = PumpMousePressed;
    input.mouse_released = PumpMouseReleased;
    ASSERT_EQ(csr_host_bind_input(&input), 1);

    std::string display_error;
    ASSERT_TRUE(display.Start(&display_error)) << display_error;

    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_open = realtime;

    std::atomic<unsigned long> seen_window{0};
    std::atomic<bool> injected{false};
    std::thread injector([&seen_window, &injected] {
        Display* view = XOpenDisplay(nullptr);
        if (view == nullptr)
        {
            return;
        }
        const Window root = RootWindow(view, DefaultScreen(view));
        for (int attempt = 0; attempt < 400 && !injected.load(); ++attempt)
        {
            Window root_return = 0;
            Window parent = 0;
            Window* children = nullptr;
            unsigned int count = 0;
            if (XQueryTree(view, root, &root_return, &parent, &children, &count) != 0 && count > 0 && children != nullptr)
            {
                const Window window = children[count - 1];
                XFree(children);
                XEvent motion{};
                motion.xmotion.type = MotionNotify;
                motion.xmotion.display = view;
                motion.xmotion.window = window;
                motion.xmotion.root = root;
                motion.xmotion.x = 20;
                motion.xmotion.y = 30;
                motion.xmotion.x_root = 20;
                motion.xmotion.y_root = 30;
                motion.xmotion.same_screen = True;
                XSendEvent(view, window, False, PointerMotionMask, &motion);
                XEvent button{};
                button.xbutton.type = ButtonPress;
                button.xbutton.display = view;
                button.xbutton.window = window;
                button.xbutton.root = root;
                button.xbutton.x = 20;
                button.xbutton.y = 30;
                button.xbutton.x_root = 20;
                button.xbutton.y_root = 30;
                button.xbutton.button = Button1;
                button.xbutton.same_screen = True;
                XSendEvent(view, window, False, ButtonPressMask, &button);
                XFlush(view);
                seen_window.store(static_cast<unsigned long>(window));
                injected.store(true);
                break;
            }
            if (children != nullptr)
            {
                XFree(children);
            }
            timespec delay{};
            delay.tv_nsec = 5 * 1000 * 1000;
            nanosleep(&delay, nullptr);
        }
        XCloseDisplay(view);
    });

    constexpr int kFrames = 3;
    constexpr double kDelta = 0.125;
    ASSERT_EQ(csr_host_pump(640, 480, kFrames, kDelta), 1) << csr_host_present_error();
    injector.join();
    ASSERT_TRUE(injected.load());
    EXPECT_EQ(g_pump_moves.load(), 1);
    EXPECT_EQ(g_pump_x.load(), 20);
    EXPECT_EQ(g_pump_y.load(), 30);
    EXPECT_EQ(g_pump_presses.load(), 1);
    EXPECT_EQ(g_pump_code.load(), 0);

    unsigned long window = 1;
    int swaps = 0;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, &window, &swaps), 0);
    EXPECT_EQ(window, 0u);
    EXPECT_EQ(swaps, kFrames);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_open + (kFrames * kDelta));
    EXPECT_DOUBLE_EQ(frametime, kDelta);
    EXPECT_DOUBLE_EQ(oldrealtime, realtime_open + (kFrames * kDelta));
    EXPECT_EQ(run_frame_count(), run_frames_before);

    Display* view = XOpenDisplay(nullptr);
    ASSERT_NE(view, nullptr);
    g_fixture_x_error = 0;
    XErrorHandler previous = XSetErrorHandler(OnFixtureXError);
    XWindowAttributes gone{};
    const int attributes = XGetWindowAttributes(view, static_cast<Window>(seen_window.load()), &gone);
    XSync(view, False);
    XSetErrorHandler(previous);
    EXPECT_EQ(attributes, 0);
    EXPECT_EQ(g_fixture_x_error, BadWindow);
    XCloseDisplay(view);
    dlclose(vgui);
}

namespace
{

bool WriteTextFile(const std::string& path, const char* text)
{
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr || text == nullptr)
    {
        if (file != nullptr)
        {
            std::fclose(file);
        }
        return false;
    }
    const std::size_t length = std::strlen(text);
    const bool wrote = std::fwrite(text, 1, length, file) == length;
    std::fclose(file);
    return wrote;
}

vgui2::VPANEL FindPanelNamed(vgui2::IPanel* panels, vgui2::VPANEL root, const char* name)
{
    if (panels == nullptr || root == 0 || name == nullptr)
    {
        return 0;
    }
    const char* panel_name = panels->GetName(root);
    if (panel_name != nullptr && std::strcmp(panel_name, name) == 0)
    {
        return root;
    }
    const int count = panels->GetChildCount(root);
    for (int index = 0; index < count; ++index)
    {
        const vgui2::VPANEL found = FindPanelNamed(panels, panels->GetChild(root, index), name);
        if (found != 0)
        {
            return found;
        }
    }
    return 0;
}

std::vector<std::string> ReadChildPids()
{
    const std::string path = "/proc/" + std::to_string(getpid()) + "/task/" + std::to_string(getpid()) + "/children";
    std::ifstream children(path);
    std::vector<std::string> pids;
    std::string pid;
    while (children >> pid)
    {
        pids.push_back(pid);
    }
    return pids;
}

} // namespace

TEST(XashLauncher, RunKeepsTheWindowUntilQuit)
{
    if (!RunInFreshProcess("run", "XashLauncher.RunKeepsTheWindowUntilQuit"))
    {
        return;
    }

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource/schemes").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    const std::string scheme = temp.root + "/resource/schemes/TrackerScheme_ClassicPlus.res";
    const char* menu_text =
        "\"GameMenu\"\n"
        "{\n"
        "\t\"1\"\n"
        "\t{\n"
        "\t\t\"label\" \"Quit\"\n"
        "\t\t\"command\" \"QuitNoConfirm\"\n"
        "\t\t\"name\" \"QuitGame\"\n"
        "\t}\n"
        "}\n";
    const char* scheme_text =
        "\"Scheme\"\n"
        "{\n"
        "\t\"Colors\"\n"
        "\t{\n"
        "\t\t\"Blank\" \"0 0 0 0\"\n"
        "\t}\n"
        "\t\"BaseSettings\"\n"
        "\t{\n"
        "\t\t\"FgColor\" \"Blank\"\n"
        "\t}\n"
        "}\n";
    ASSERT_TRUE(WriteTextFile(menu, menu_text));
    ASSERT_TRUE(WriteTextFile(scheme, scheme_text));

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

    VirtualDisplay display;
    std::string display_error;
    ASSERT_TRUE(display.Start(&display_error)) << display_error;

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

    ASSERT_EQ(csr_host_activate_gameui(), 1);
    ASSERT_EQ(csr_host_paint_gameui(640, 480), 1);
    auto* surface = static_cast<vgui2::ISurface*>(csr_host_create_interface("VGUI_Surface026", nullptr));
    auto create = reinterpret_cast<void* (*)(const char*, int*)>(dlsym(vgui, "CreateInterface"));
    ASSERT_NE(surface, nullptr);
    ASSERT_NE(create, nullptr);
    int code = 1;
    auto* panels = static_cast<vgui2::IPanel*>(create("VGUI_Panel007", &code));
    ASSERT_NE(panels, nullptr);
    const vgui2::VPANEL item = FindPanelNamed(panels, surface->GetEmbeddedPanel(), "QuitGame");
    ASSERT_NE(item, 0);
    int origin_x = 0;
    int origin_y = 0;
    int wide = 0;
    int tall = 0;
    panels->GetAbsPos(item, origin_x, origin_y);
    panels->GetSize(item, wide, tall);
    ASSERT_GT(wide, 0);
    ASSERT_GT(tall, 0);
    const int click_x = origin_x + wide / 2;
    const int click_y = origin_y + tall / 2;

    std::vector<std::string> sockets_before;
    ASSERT_TRUE(ReadOpenSockets(&sockets_before));
    const std::vector<std::string> children_before = ReadChildPids();
    ASSERT_EQ(children_before.size(), 1u);
    EXPECT_EQ(children_before[0], std::to_string(display.pid));

    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_open = realtime;

    std::atomic<unsigned long> seen_window{0};
    std::atomic<int> saw_viewable{0};
    std::thread injector([&seen_window, &saw_viewable, click_x, click_y] {
        Display* view = XOpenDisplay(nullptr);
        if (view == nullptr)
        {
            return;
        }
        const Window root = RootWindow(view, DefaultScreen(view));
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            unsigned long window = 0;
            int swaps = 0;
            csr_host_present_query(nullptr, nullptr, &window, &swaps);
            if (swaps >= 2 && window != 0)
            {
                XWindowAttributes attributes{};
                if (XGetWindowAttributes(view, static_cast<Window>(window), &attributes) != 0 && attributes.map_state == IsViewable)
                {
                    saw_viewable.store(1);
                    seen_window.store(window);
                    const Window target = static_cast<Window>(window);
                    XEvent motion{};
                    motion.xmotion.type = MotionNotify;
                    motion.xmotion.display = view;
                    motion.xmotion.window = target;
                    motion.xmotion.root = root;
                    motion.xmotion.x = click_x;
                    motion.xmotion.y = click_y;
                    motion.xmotion.x_root = click_x;
                    motion.xmotion.y_root = click_y;
                    motion.xmotion.same_screen = True;
                    XSendEvent(view, target, False, PointerMotionMask, &motion);
                    XEvent press{};
                    press.xbutton.type = ButtonPress;
                    press.xbutton.display = view;
                    press.xbutton.window = target;
                    press.xbutton.root = root;
                    press.xbutton.x = click_x;
                    press.xbutton.y = click_y;
                    press.xbutton.x_root = click_x;
                    press.xbutton.y_root = click_y;
                    press.xbutton.button = Button1;
                    press.xbutton.same_screen = True;
                    XSendEvent(view, target, False, ButtonPressMask, &press);
                    XEvent release{};
                    release.xbutton.type = ButtonRelease;
                    release.xbutton.display = view;
                    release.xbutton.window = target;
                    release.xbutton.root = root;
                    release.xbutton.x = click_x;
                    release.xbutton.y = click_y;
                    release.xbutton.x_root = click_x;
                    release.xbutton.y_root = click_y;
                    release.xbutton.button = Button1;
                    release.xbutton.same_screen = True;
                    XSendEvent(view, target, False, ButtonReleaseMask, &release);
                    XFlush(view);
                    break;
                }
            }
            timespec delay{};
            delay.tv_nsec = 5 * 1000 * 1000;
            nanosleep(&delay, nullptr);
        }
        XCloseDisplay(view);
    });

    constexpr double kDelta = 0.125;
    ASSERT_EQ(csr_host_run(640, 480, kDelta), 1) << csr_host_present_error();
    injector.join();
    ASSERT_EQ(saw_viewable.load(), 1);
    ASSERT_NE(seen_window.load(), 0u);

    int moves = 0;
    int move_x = -1;
    int move_y = -1;
    int presses = 0;
    int button_code = -1;
    csr_host_input_observed(&moves, &move_x, &move_y, &presses, &button_code);
    EXPECT_GE(moves, 1);
    EXPECT_EQ(move_x, click_x);
    EXPECT_EQ(move_y, click_y);
    EXPECT_GE(presses, 1);
    EXPECT_EQ(button_code, 0);
    EXPECT_STREQ(csr_host_last_client_command(), "quit\n");

    unsigned long window = 1;
    int swaps = 0;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, &window, &swaps), 0);
    EXPECT_EQ(window, 0u);
    EXPECT_GE(swaps, 2);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_open + (swaps * kDelta));
    EXPECT_DOUBLE_EQ(frametime, kDelta);
    EXPECT_DOUBLE_EQ(oldrealtime, realtime_open + (swaps * kDelta));
    EXPECT_EQ(run_frame_count(), run_frames_before);

    int filter_after = -1;
    int stored_after = -1;
    int nitro_after = -1;
    guard(&filter_after, &stored_after, &nitro_after);
    EXPECT_EQ(filter_after, 0);
    EXPECT_EQ(stored_after, stored_before);
    EXPECT_EQ(nitro_after, 0);
    EXPECT_FALSE(MapsContain(planted_hw));
    auto steam_running = reinterpret_cast<bool (*)()>(dlsym(session.steam, "SteamAPI_IsSteamRunning"));
    ASSERT_NE(steam_running, nullptr);
    EXPECT_FALSE(steam_running());
    EXPECT_EQ(ReadChildPids(), children_before);

    std::vector<std::string> sockets_after;
    ASSERT_TRUE(ReadOpenSockets(&sockets_after));
    std::vector<std::string> added;
    std::set_difference(
        sockets_after.begin(),
        sockets_after.end(),
        sockets_before.begin(),
        sockets_before.end(),
        std::back_inserter(added)
    );
    EXPECT_TRUE(added.empty());

    std::ifstream cmdline("/proc/self/cmdline", std::ios::binary);
    std::string arguments((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());
    EXPECT_EQ(arguments.find("--surface"), std::string::npos);

    Display* view = XOpenDisplay(nullptr);
    ASSERT_NE(view, nullptr);
    g_fixture_x_error = 0;
    XErrorHandler previous = XSetErrorHandler(OnFixtureXError);
    XWindowAttributes gone{};
    const int attributes = XGetWindowAttributes(view, static_cast<Window>(seen_window.load()), &gone);
    XSync(view, False);
    XSetErrorHandler(previous);
    EXPECT_EQ(attributes, 0);
    EXPECT_EQ(g_fixture_x_error, BadWindow);
    XCloseDisplay(view);

    csr::CloseXashLauncher(&session);
    dlclose(vgui);
    dlclose(engine);
    std::filesystem::remove(planted_hw);
    std::remove(menu.c_str());
    std::remove(scheme.c_str());
    rmdir((temp.root + "/resource/schemes").c_str());
    rmdir((temp.root + "/resource").c_str());
    rmdir((temp.root + "/Resource").c_str());
}

TEST(XashLauncher, RunEntryReturnsWithoutAHost)
{
    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    int swaps = -1;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, nullptr, &swaps), 0);

    csr::LauncherProcess session;
    EXPECT_EQ(
        csr::RunXashLauncher(
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            &session,
            640,
            480,
            0.125),
        0);
    EXPECT_FALSE(session.open);

    char missing[64];
    std::snprintf(missing, sizeof(missing), "/tmp/csretro-launcher-missing-%d", static_cast<int>(getpid()));
    EXPECT_EQ(
        csr::RunXashLauncher(
            missing,
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session,
            0,
            480,
            0.125),
        0);
    EXPECT_FALSE(session.open);
    EXPECT_EQ(
        csr::RunXashLauncher(
            missing,
            XASH_HOST_FILE,
            XASH_SURFACE_FILE,
            XASH_CLIENT_FILE,
            XASH_VGUI_FILE,
            XASH_GAMEUI_FILE,
            XASH_STEAM_API_FILE,
            XASH_ENGINE_MINI_FILE,
            XASH_NITRO_API_FILE,
            &session,
            640,
            480,
            0.125),
        0);
    EXPECT_FALSE(session.open);

    double realtime_after = 0.0;
    double frametime_after = 0.0;
    double oldrealtime_after = 0.0;
    csr_host_read_host_time(&realtime_after, &frametime_after, &oldrealtime_after);
    EXPECT_DOUBLE_EQ(realtime_after, realtime);
    EXPECT_DOUBLE_EQ(frametime_after, frametime);
    EXPECT_DOUBLE_EQ(oldrealtime_after, oldrealtime);
    int swaps_after = -1;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, nullptr, &swaps_after), 0);
    EXPECT_EQ(swaps_after, swaps);
    EXPECT_STREQ(csr_host_last_client_command(), "");
}

extern int csretro_human_main(int argc, char** argv);

TEST(XashLauncher, HumanEntryCallsRunXashLauncher)
{
    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    int swaps = -1;
    unsigned long window = 1;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, &window, &swaps), 0);
    const std::vector<std::string> children_before = ReadChildPids();

    // This is the process entry. main in the human executable is this function.
    // No content root, so RunXashLauncher returns before a window exists.
    EXPECT_EQ(csretro_human_main(0, nullptr), 1);

    double realtime_after = 0.0;
    double frametime_after = 0.0;
    double oldrealtime_after = 0.0;
    csr_host_read_host_time(&realtime_after, &frametime_after, &oldrealtime_after);
    EXPECT_DOUBLE_EQ(realtime_after, realtime);
    EXPECT_DOUBLE_EQ(frametime_after, frametime);
    EXPECT_DOUBLE_EQ(oldrealtime_after, oldrealtime);
    int swaps_after = -1;
    unsigned long window_after = 1;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, &window_after, &swaps_after), 0);
    EXPECT_EQ(swaps_after, swaps);
    EXPECT_EQ(window_after, window);
    EXPECT_STREQ(csr_host_last_client_command(), "");
    EXPECT_EQ(ReadChildPids(), children_before);

    std::ifstream cmdline("/proc/self/cmdline", std::ios::binary);
    std::string arguments((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());
    EXPECT_EQ(arguments.find("--surface"), std::string::npos);
}

TEST(XashLauncher, EntryRunsTheHostUntilQuit)
{
    if (!RunInFreshProcess("entry", "XashLauncher.EntryRunsTheHostUntilQuit"))
    {
        return;
    }

    TempRoot temp;
    ASSERT_FALSE(temp.root.empty());
    ASSERT_EQ(mkdir((temp.root + "/Resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource").c_str(), 0755), 0);
    ASSERT_EQ(mkdir((temp.root + "/resource/schemes").c_str(), 0755), 0);
    const std::string menu = temp.root + "/Resource/GameMenu.res";
    const std::string scheme = temp.root + "/resource/schemes/TrackerScheme_ClassicPlus.res";
    const char* menu_text =
        "\"GameMenu\"\n"
        "{\n"
        "\t\"1\"\n"
        "\t{\n"
        "\t\t\"label\" \"Quit\"\n"
        "\t\t\"command\" \"QuitNoConfirm\"\n"
        "\t\t\"name\" \"QuitGame\"\n"
        "\t}\n"
        "}\n";
    const char* scheme_text =
        "\"Scheme\"\n"
        "{\n"
        "\t\"Colors\"\n"
        "\t{\n"
        "\t\t\"Blank\" \"0 0 0 0\"\n"
        "\t}\n"
        "\t\"BaseSettings\"\n"
        "\t{\n"
        "\t\t\"FgColor\" \"Blank\"\n"
        "\t}\n"
        "}\n";
    ASSERT_TRUE(WriteTextFile(menu, menu_text));
    ASSERT_TRUE(WriteTextFile(scheme, scheme_text));

    const std::string planted_hw = temp.root + "/hw.so";
    std::filesystem::copy_file(XASH_HOST_FILE, planted_hw);
    EXPECT_FALSE(MapsContain(planted_hw));

    VirtualDisplay display;
    std::string display_error;
    ASSERT_TRUE(display.Start(&display_error)) << display_error;

    // The entry opens the host itself. A child measures QuitGame first so this
    // process can click that item without a second EGL session.
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);
    const pid_t probe = fork();
    ASSERT_GE(probe, 0);
    if (probe == 0)
    {
        close(fds[0]);
        csr::LauncherProcess probed;
        if (csr::OpenXashLauncher(
                temp.root.c_str(),
                XASH_HOST_FILE,
                XASH_SURFACE_FILE,
                XASH_CLIENT_FILE,
                XASH_VGUI_FILE,
                XASH_GAMEUI_FILE,
                XASH_STEAM_API_FILE,
                XASH_ENGINE_MINI_FILE,
                XASH_NITRO_API_FILE,
                &probed) != 1)
        {
            _exit(2);
        }
        void* engine = dlopen(XASH_ENGINE_MINI_FILE, RTLD_NOW | RTLD_NOLOAD);
        if (engine == nullptr)
        {
            _exit(3);
        }
        using GuardFn = void (*)(int*, int*, int*);
        auto guard = reinterpret_cast<GuardFn>(dlsym(engine, "EngineMini_ReadFrameGuard"));
        if (guard == nullptr)
        {
            _exit(4);
        }
        int filter_calls = -1;
        int stored = -1;
        int nitro = -1;
        guard(&filter_calls, &stored, &nitro);
        void* vgui = dlopen(XASH_VGUI_FILE, RTLD_NOW | RTLD_NOLOAD);
        if (vgui == nullptr)
        {
            _exit(5);
        }
        auto run_frame_count = reinterpret_cast<int (*)()>(dlsym(vgui, "csr_vgui_run_frame_count"));
        if (run_frame_count == nullptr)
        {
            _exit(6);
        }
        const int run_frames = run_frame_count();
        if (csr_host_activate_gameui() != 1 || csr_host_paint_gameui(640, 480) != 1)
        {
            _exit(7);
        }
        auto* surface = static_cast<vgui2::ISurface*>(csr_host_create_interface("VGUI_Surface026", nullptr));
        auto create = reinterpret_cast<void* (*)(const char*, int*)>(dlsym(vgui, "CreateInterface"));
        if (surface == nullptr || create == nullptr)
        {
            _exit(8);
        }
        int code = 1;
        auto* panels = static_cast<vgui2::IPanel*>(create("VGUI_Panel007", &code));
        if (panels == nullptr)
        {
            _exit(9);
        }
        const vgui2::VPANEL item = FindPanelNamed(panels, surface->GetEmbeddedPanel(), "QuitGame");
        if (item == 0)
        {
            _exit(10);
        }
        int origin_x = 0;
        int origin_y = 0;
        int item_wide = 0;
        int item_tall = 0;
        panels->GetAbsPos(item, origin_x, origin_y);
        panels->GetSize(item, item_wide, item_tall);
        if (item_wide <= 0 || item_tall <= 0)
        {
            _exit(11);
        }
        const int click_x = origin_x + item_wide / 2;
        const int click_y = origin_y + item_tall / 2;
        csr::CloseXashLauncher(&probed);
        dlclose(vgui);
        dlclose(engine);
        char line[128];
        const int length = std::snprintf(
            line,
            sizeof(line),
            "%d %d %d %d %d %d\n",
            click_x,
            click_y,
            filter_calls,
            stored,
            nitro,
            run_frames);
        if (length <= 0 || static_cast<size_t>(length) >= sizeof(line) ||
            write(fds[1], line, static_cast<size_t>(length)) != length)
        {
            _exit(12);
        }
        close(fds[1]);
        _exit(0);
    }
    close(fds[1]);
    std::string payload;
    char buffer[64];
    while (true)
    {
        const ssize_t got = read(fds[0], buffer, sizeof(buffer));
        if (got == 0)
        {
            break;
        }
        ASSERT_GT(got, 0);
        payload.append(buffer, buffer + got);
    }
    close(fds[0]);
    int probe_status = 0;
    ASSERT_EQ(waitpid(probe, &probe_status, 0), probe);
    ASSERT_TRUE(WIFEXITED(probe_status));
    ASSERT_EQ(WEXITSTATUS(probe_status), 0) << payload;
    int click_x = 0;
    int click_y = 0;
    int filter_before = -1;
    int stored_before = -1;
    int nitro_before = -1;
    int run_frames_before = -1;
    ASSERT_EQ(
        std::sscanf(
            payload.c_str(),
            "%d %d %d %d %d %d",
            &click_x,
            &click_y,
            &filter_before,
            &stored_before,
            &nitro_before,
            &run_frames_before),
        6);
    EXPECT_EQ(filter_before, 0);
    EXPECT_GT(stored_before, 0);
    EXPECT_EQ(nitro_before, 0);
    ASSERT_GE(click_x, 0);
    ASSERT_LT(click_x, 640);
    ASSERT_GE(click_y, 0);
    ASSERT_LT(click_y, 480);

    std::vector<std::string> sockets_before;
    ASSERT_TRUE(ReadOpenSockets(&sockets_before));
    const std::vector<std::string> children_before = ReadChildPids();
    ASSERT_EQ(children_before.size(), 1u);
    EXPECT_EQ(children_before[0], std::to_string(display.pid));

    double realtime = 0.0;
    double frametime = 0.0;
    double oldrealtime = 0.0;
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    const double realtime_open = realtime;

    std::atomic<unsigned long> seen_window{0};
    std::atomic<int> saw_viewable{0};
    std::thread injector([&seen_window, &saw_viewable, click_x, click_y] {
        Display* view = XOpenDisplay(nullptr);
        if (view == nullptr)
        {
            return;
        }
        const Window root = RootWindow(view, DefaultScreen(view));
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            unsigned long window = 0;
            int swaps = 0;
            csr_host_present_query(nullptr, nullptr, &window, &swaps);
            if (swaps >= 2 && window != 0)
            {
                XWindowAttributes attributes{};
                if (XGetWindowAttributes(view, static_cast<Window>(window), &attributes) != 0 &&
                    attributes.map_state == IsViewable)
                {
                    saw_viewable.store(1);
                    seen_window.store(window);
                    const Window target = static_cast<Window>(window);
                    XEvent motion{};
                    motion.xmotion.type = MotionNotify;
                    motion.xmotion.display = view;
                    motion.xmotion.window = target;
                    motion.xmotion.root = root;
                    motion.xmotion.x = click_x;
                    motion.xmotion.y = click_y;
                    motion.xmotion.x_root = click_x;
                    motion.xmotion.y_root = click_y;
                    motion.xmotion.same_screen = True;
                    XSendEvent(view, target, False, PointerMotionMask, &motion);
                    XEvent press{};
                    press.xbutton.type = ButtonPress;
                    press.xbutton.display = view;
                    press.xbutton.window = target;
                    press.xbutton.root = root;
                    press.xbutton.x = click_x;
                    press.xbutton.y = click_y;
                    press.xbutton.x_root = click_x;
                    press.xbutton.y_root = click_y;
                    press.xbutton.button = Button1;
                    press.xbutton.same_screen = True;
                    XSendEvent(view, target, False, ButtonPressMask, &press);
                    XEvent release{};
                    release.xbutton.type = ButtonRelease;
                    release.xbutton.display = view;
                    release.xbutton.window = target;
                    release.xbutton.root = root;
                    release.xbutton.x = click_x;
                    release.xbutton.y = click_y;
                    release.xbutton.x_root = click_x;
                    release.xbutton.y_root = click_y;
                    release.xbutton.button = Button1;
                    release.xbutton.same_screen = True;
                    XSendEvent(view, target, False, ButtonReleaseMask, &release);
                    XFlush(view);
                    break;
                }
            }
            timespec delay{};
            delay.tv_nsec = 5 * 1000 * 1000;
            nanosleep(&delay, nullptr);
        }
        XCloseDisplay(view);
    });

    std::atomic<int> finished{0};
    const pid_t xvfb_pid = display.pid;
    std::thread watchdog([&finished, xvfb_pid] {
        for (int step = 0; step < 150; ++step)
        {
            if (finished.load() != 0)
            {
                return;
            }
            timespec delay{};
            delay.tv_nsec = 100 * 1000 * 1000;
            nanosleep(&delay, nullptr);
        }
        if (xvfb_pid > 0)
        {
            kill(xvfb_pid, SIGTERM);
        }
        _exit(86);
    });

    csr::LauncherProcess session;
    constexpr double kDelta = 0.125;
    const int ran = csr::RunXashLauncher(
        temp.root.c_str(),
        XASH_HOST_FILE,
        XASH_SURFACE_FILE,
        XASH_CLIENT_FILE,
        XASH_VGUI_FILE,
        XASH_GAMEUI_FILE,
        XASH_STEAM_API_FILE,
        XASH_ENGINE_MINI_FILE,
        XASH_NITRO_API_FILE,
        &session,
        640,
        480,
        kDelta);
    finished.store(1);
    watchdog.join();
    injector.join();
    ASSERT_EQ(ran, 1) << csr_host_present_error();
    ASSERT_EQ(saw_viewable.load(), 1);
    ASSERT_NE(seen_window.load(), 0u);
    EXPECT_TRUE(session.open);

    int moves = 0;
    int move_x = -1;
    int move_y = -1;
    int presses = 0;
    int button_code = -1;
    csr_host_input_observed(&moves, &move_x, &move_y, &presses, &button_code);
    EXPECT_GE(moves, 1);
    EXPECT_EQ(move_x, click_x);
    EXPECT_EQ(move_y, click_y);
    EXPECT_GE(presses, 1);
    EXPECT_EQ(button_code, 0);
    EXPECT_STREQ(csr_host_last_client_command(), "quit\n");

    unsigned long window = 1;
    int swaps = 0;
    EXPECT_EQ(csr_host_present_query(nullptr, nullptr, &window, &swaps), 0);
    EXPECT_EQ(window, 0u);
    EXPECT_GE(swaps, 2);
    csr_host_read_host_time(&realtime, &frametime, &oldrealtime);
    EXPECT_DOUBLE_EQ(realtime, realtime_open + (swaps * kDelta));
    EXPECT_DOUBLE_EQ(frametime, kDelta);
    EXPECT_DOUBLE_EQ(oldrealtime, realtime_open + (swaps * kDelta));
    std::cerr << "entry-run swaps=" << swaps << " host=" << realtime << " move=" << move_x << "," << move_y
              << " button=" << button_code << "\n";

    void* engine = dlopen(XASH_ENGINE_MINI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(engine, nullptr);
    using GuardFn = void (*)(int*, int*, int*);
    auto guard = reinterpret_cast<GuardFn>(dlsym(engine, "EngineMini_ReadFrameGuard"));
    ASSERT_NE(guard, nullptr);
    int filter_after = -1;
    int stored_after = -1;
    int nitro_after = -1;
    guard(&filter_after, &stored_after, &nitro_after);
    EXPECT_EQ(filter_after, 0);
    EXPECT_EQ(stored_after, stored_before);
    EXPECT_EQ(nitro_after, 0);

    void* vgui = dlopen(XASH_VGUI_FILE, RTLD_NOW | RTLD_NOLOAD);
    ASSERT_NE(vgui, nullptr);
    auto run_frame_count = reinterpret_cast<int (*)()>(dlsym(vgui, "csr_vgui_run_frame_count"));
    ASSERT_NE(run_frame_count, nullptr);
    EXPECT_EQ(run_frame_count(), run_frames_before);

    EXPECT_FALSE(MapsContain(planted_hw));
    auto steam_running = reinterpret_cast<bool (*)()>(dlsym(session.steam, "SteamAPI_IsSteamRunning"));
    ASSERT_NE(steam_running, nullptr);
    EXPECT_FALSE(steam_running());
    EXPECT_EQ(ReadChildPids(), children_before);

    std::vector<std::string> sockets_after;
    ASSERT_TRUE(ReadOpenSockets(&sockets_after));
    std::vector<std::string> added;
    std::set_difference(
        sockets_after.begin(),
        sockets_after.end(),
        sockets_before.begin(),
        sockets_before.end(),
        std::back_inserter(added));
    EXPECT_TRUE(added.empty());

    std::ifstream cmdline("/proc/self/cmdline", std::ios::binary);
    std::string arguments((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());
    EXPECT_EQ(arguments.find("--surface"), std::string::npos);

    Display* view = XOpenDisplay(nullptr);
    ASSERT_NE(view, nullptr);
    g_fixture_x_error = 0;
    XErrorHandler previous = XSetErrorHandler(OnFixtureXError);
    XWindowAttributes gone{};
    const int attributes = XGetWindowAttributes(view, static_cast<Window>(seen_window.load()), &gone);
    XSync(view, False);
    XSetErrorHandler(previous);
    EXPECT_EQ(attributes, 0);
    EXPECT_EQ(g_fixture_x_error, BadWindow);
    XCloseDisplay(view);

    csr::CloseXashLauncher(&session);
    dlclose(vgui);
    dlclose(engine);
    const pid_t xvfb = display.pid;
    display.pid = -1;
    if (xvfb > 0)
    {
        kill(xvfb, SIGTERM);
        waitpid(xvfb, nullptr, 0);
    }
    EXPECT_TRUE(ReadChildPids().empty());

    std::filesystem::remove(planted_hw);
    std::remove(menu.c_str());
    std::remove(scheme.c_str());
    rmdir((temp.root + "/resource/schemes").c_str());
    rmdir((temp.root + "/resource").c_str());
    rmdir((temp.root + "/Resource").c_str());
}
