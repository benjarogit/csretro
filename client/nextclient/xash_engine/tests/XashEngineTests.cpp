#include "xash_engine/xash_engine.h"

#include <dlfcn.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

namespace
{

    int g_init_calls = 0;
    int g_reset_calls = 0;
    int g_color_r = -1;
    int g_color_g = -1;
    int g_color_b = -1;
    void* g_seen = nullptr;

    int g_argv_calls = 0;
    int g_mode_calls = 0;
    int g_init_game_calls = 0;
    int g_shutdown_calls = 0;
    int g_error_calls = 0;
    int g_argc = -1;
    int g_mode_result = -1;
    int g_dedicated = -1;
    int g_init_result = -1;
    void* g_window = nullptr;
    void* g_device = nullptr;
    void* g_context = nullptr;
    std::string g_driver;
    std::string g_mode_cmdline;
    std::string g_init_cmdline;
    std::string g_basedir;
    std::string g_error;
    std::string g_argv0;
    std::string g_argv1;

    void OnInit(void* object)
    {
        ++g_init_calls;
        g_seen = object;
    }

    void OnReset(void* object)
    {
        ++g_reset_calls;
        g_seen = object;
    }

    void OnColor(void* object, int r, int g, int b)
    {
        g_seen = object;
        g_color_r = r;
        g_color_g = g;
        g_color_b = b;
    }

    void OnArgv(int argc, char** argv)
    {
        ++g_argv_calls;
        g_argc = argc;
        if (argv != nullptr && argc > 0 && argv[0] != nullptr)
        {
            g_argv0 = argv[0];
        }
        if (argv != nullptr && argc > 1 && argv[1] != nullptr)
        {
            g_argv1 = argv[1];
        }
    }

    void OnMode(void* window, void* device, void* context, const char* driver, const char* cmdline, int result)
    {
        ++g_mode_calls;
        g_window = window;
        g_device = device;
        g_context = context;
        g_mode_result = result;
        g_driver = driver != nullptr ? driver : "";
        g_mode_cmdline = cmdline != nullptr ? cmdline : "";
    }

    void OnInitGame(char* cmdline, char* basedir, void* window, int dedicated, int result)
    {
        ++g_init_game_calls;
        g_window = window;
        g_dedicated = dedicated;
        g_init_result = result;
        g_init_cmdline = cmdline != nullptr ? cmdline : "";
        g_basedir = basedir != nullptr ? basedir : "";
    }

    void OnShutdown()
    {
        ++g_shutdown_calls;
    }

    void OnError(const char* message)
    {
        ++g_error_calls;
        g_error = message != nullptr ? message : "";
    }

    bool MapsContain(const std::filesystem::path& fragment)
    {
        std::ifstream maps("/proc/self/maps");
        std::string line;

        while (std::getline(maps, line))
        {
            if (line.find(fragment.string()) != std::string::npos)
            {
                return true;
            }
        }

        return false;
    }

    class WorkingDirectory
    {
        std::filesystem::path previous_;

    public:
        explicit WorkingDirectory(const std::filesystem::path& next) :
            previous_(std::filesystem::current_path())
        {
            std::filesystem::current_path(next);
        }

        ~WorkingDirectory()
        {
            std::filesystem::current_path(previous_);
        }
    };

    class XashEngine : public testing::Test
    {
    protected:
        void SetUp() override
        {
            g_init_calls = 0;
            g_reset_calls = 0;
            g_color_r = -1;
            g_color_g = -1;
            g_color_b = -1;
            g_seen = nullptr;
            g_argv_calls = 0;
            g_mode_calls = 0;
            g_init_game_calls = 0;
            g_shutdown_calls = 0;
            g_error_calls = 0;
            g_argc = -1;
            g_mode_result = -1;
            g_dedicated = -1;
            g_init_result = -1;
            g_window = nullptr;
            g_device = nullptr;
            g_context = nullptr;
            g_driver.clear();
            g_mode_cmdline.clear();
            g_init_cmdline.clear();
            g_basedir.clear();
            g_error.clear();
            g_argv0.clear();
            g_argv1.clear();
            csr_xash_engine_clear();
        }
    };

} // namespace

TEST_F(XashEngine, MissingFunctionsAreNotCalled)
{
    int object = 0;
    csr_xash_engine_surface_t surface{};
    surface.object = &object;
    csr_xash_engine_bind(&surface);

    csr_xash_engine_draw_init();
    csr_xash_engine_reset_texture();
    csr_xash_engine_set_text_color(9, 8, 7);

    EXPECT_EQ(g_init_calls, 0);
    EXPECT_EQ(g_reset_calls, 0);
    EXPECT_EQ(g_color_r, -1);
    EXPECT_EQ(csr_xash_engine_surface()->object, &object);
}

TEST_F(XashEngine, BoundFunctionsReceiveTheSurface)
{
    int object = 0;
    csr_xash_engine_surface_t surface{};
    surface.object = &object;
    surface.draw_init = OnInit;
    surface.reset_texture = OnReset;
    surface.set_text_color = OnColor;
    csr_xash_engine_bind(&surface);

    csr_xash_engine_draw_init();
    csr_xash_engine_reset_texture();
    csr_xash_engine_set_text_color(4, 5, 6);

    EXPECT_EQ(g_init_calls, 1);
    EXPECT_EQ(g_reset_calls, 1);
    EXPECT_EQ(g_color_r, 4);
    EXPECT_EQ(g_color_g, 5);
    EXPECT_EQ(g_color_b, 6);
    EXPECT_EQ(g_seen, &object);
}

TEST_F(XashEngine, ANullObjectClearsTheBinding)
{
    int object = 0;
    csr_xash_engine_surface_t surface{};
    surface.object = &object;
    surface.set_text_color = OnColor;
    csr_xash_engine_bind(&surface);
    csr_xash_engine_bind(nullptr);

    csr_xash_engine_set_text_color(1, 2, 3);

    EXPECT_EQ(csr_xash_engine_surface()->object, nullptr);
    EXPECT_EQ(g_color_r, -1);
}

TEST_F(XashEngine, MissingStartupCallbacksAreNotCalled)
{
    char arg0[] = "xash";
    char* argv[] = {arg0, nullptr};
    int window = 0;
    char cmdline[] = "-game cstrike";
    char basedir[] = "valve";

    csr_xash_engine_argv(1, argv);
    csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
    csr_xash_engine_init_game(cmdline, basedir, &window, 0, 1);
    csr_xash_engine_shutdown();
    csr_xash_engine_error("fatal");

    EXPECT_EQ(g_argv_calls, 0);
    EXPECT_EQ(g_mode_calls, 0);
    EXPECT_EQ(g_init_game_calls, 0);
    EXPECT_EQ(g_shutdown_calls, 0);
    EXPECT_EQ(g_error_calls, 0);
}

TEST_F(XashEngine, BoundStartupCallbacksReceiveTheArguments)
{
    int window = 0;
    int device = 0;
    int context = 0;
    char arg0[] = "xash";
    char arg1[] = "-console";
    char* argv[] = {arg0, arg1, nullptr};
    char mode_line[] = "-windowed";
    char init_line[] = "-game cstrike";
    char basedir[] = "valve";

    csr_xash_engine_startup_t startup{};
    startup.argv = OnArgv;
    startup.init_game = OnInitGame;
    startup.shutdown = OnShutdown;
    startup.error = OnError;
    csr_xash_engine_bind_startup(&startup);

    csr_xash_engine_argv(2, argv);
    csr_xash_engine_mode(&window, &device, &context, "gl", mode_line, 1);
    csr_xash_engine_init_game(init_line, basedir, &window, 0, 1);
    csr_xash_engine_shutdown();
    csr_xash_engine_error("fatal");

    EXPECT_EQ(g_argv_calls, 1);
    EXPECT_EQ(g_argc, 2);
    EXPECT_EQ(g_argv0, "xash");
    EXPECT_EQ(g_argv1, "-console");
    EXPECT_EQ(g_mode_calls, 0);
    EXPECT_EQ(g_init_game_calls, 1);
    EXPECT_EQ(g_window, &window);
    EXPECT_EQ(g_dedicated, 0);
    EXPECT_EQ(g_init_result, 1);
    EXPECT_EQ(g_init_cmdline, "-game cstrike");
    EXPECT_EQ(g_basedir, "valve");
    EXPECT_EQ(g_shutdown_calls, 1);
    EXPECT_EQ(g_error_calls, 1);
    EXPECT_EQ(g_error, "fatal");

    startup.mode = OnMode;
    csr_xash_engine_bind_startup(&startup);
    csr_xash_engine_mode(&window, &device, &context, "gl", mode_line, 1);

    EXPECT_EQ(g_mode_calls, 1);
    EXPECT_EQ(g_window, &window);
    EXPECT_EQ(g_device, &device);
    EXPECT_EQ(g_context, &context);
    EXPECT_EQ(g_driver, "gl");
    EXPECT_EQ(g_mode_cmdline, "-windowed");
    EXPECT_EQ(g_mode_result, 1);
}

TEST_F(XashEngine, StartupCallDoesNotOpenHwSo)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-engine-startup";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    csr_xash_engine_startup_t startup{};
    startup.argv = OnArgv;
    startup.mode = OnMode;
    startup.init_game = OnInitGame;
    startup.shutdown = OnShutdown;
    startup.error = OnError;
    csr_xash_engine_bind_startup(&startup);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char* argv[] = {arg0, nullptr};
        int window = 0;
        char cmdline[] = "-game cstrike";
        char basedir[] = "valve";

        csr_xash_engine_argv(1, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", nullptr, 0);
        csr_xash_engine_init_game(cmdline, basedir, &window, 1, 1);
        csr_xash_engine_shutdown();
        csr_xash_engine_error("down");
    }

    EXPECT_EQ(g_argv_calls, 1);
    EXPECT_EQ(g_mode_calls, 1);
    EXPECT_EQ(g_init_game_calls, 1);
    EXPECT_EQ(g_shutdown_calls, 1);
    EXPECT_EQ(g_error_calls, 1);
    EXPECT_FALSE(MapsContain(root / "hw.so"));

    if (previous_path == nullptr)
    {
        ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
    }
    else
    {
        ASSERT_EQ(setenv("LD_LIBRARY_PATH", saved_path.c_str(), 1), 0);
    }
    std::filesystem::remove_all(root);
}
