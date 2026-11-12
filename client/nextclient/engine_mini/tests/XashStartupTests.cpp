#include "xash_startup.h"

#include <xash_engine/xash_engine.h>

#include <dlfcn.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

int COM_CheckParm(const char* parm);
void COM_InitArgv(int argc, char* argv[]);

namespace
{

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

    class XashStartup : public testing::Test
    {
    protected:
        void TearDown() override
        {
            COM_InitArgv(0, nullptr);
            EngineMini_BindXashStartup(nullptr, false);
        }
    };

} // namespace

TEST_F(XashStartup, AbsentHooksRecordTheSequenceAndInitArgv)
{
    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    const EngineMiniXashStartup* record = EngineMini_XashStartup();
    ASSERT_NE(record, nullptr);
    EXPECT_TRUE(record->engine_is_xash);

    char arg0[] = "xash";
    char arg1[] = "-console";
    char* argv[] = {arg0, arg1, nullptr};
    csr_xash_engine_argv(2, argv);

    record = EngineMini_XashStartup();
    EXPECT_EQ(record->argv_calls, 1);
    EXPECT_EQ(record->argc, 2);
    EXPECT_NE(COM_CheckParm("-console"), 0);

    int window = 0;
    int device = 0;
    int context = 0;
    csr_xash_engine_mode(&window, &device, &context, "gl", "-windowed", 1);

    record = EngineMini_XashStartup();
    EXPECT_EQ(record->mode_calls, 1);
    EXPECT_EQ(record->window, &window);
    EXPECT_EQ(record->device, &device);
    EXPECT_EQ(record->context, &context);
    EXPECT_STREQ(record->driver, "gl");
    EXPECT_STREQ(record->mode_cmdline, "-windowed");
    EXPECT_EQ(record->mode_result, 1);
    EXPECT_EQ(record->initializing_calls, 1);
    EXPECT_EQ(record->shutdown_calls, 0);
    EXPECT_EQ(record->error_calls, 0);
    EXPECT_NE(engine->client_state, nullptr);
    EXPECT_NE(engine->cl_enginefunc, nullptr);
    EXPECT_EQ(engine->Sys_Error.GetHook(), nullptr);
    EXPECT_EQ(engine->CL_Init.GetHook(), nullptr);

    char cmdline[] = "-game cstrike";
    char basedir[] = "valve";
    csr_xash_engine_init_game(cmdline, basedir, &window, 0, 1);
    csr_xash_engine_shutdown();
    csr_xash_engine_error("fatal");

    record = EngineMini_XashStartup();
    EXPECT_EQ(record->init_calls, 1);
    EXPECT_EQ(record->dedicated, 0);
    EXPECT_EQ(record->init_result, 1);
    EXPECT_STREQ(record->init_cmdline, "-game cstrike");
    EXPECT_STREQ(record->basedir, "valve");
    EXPECT_EQ(record->shutdown_calls, 1);
    EXPECT_EQ(record->error_calls, 1);
    EXPECT_STREQ(record->error, "fatal");
}

TEST_F(XashStartup, NullBindingIsNotACall)
{
    EngineMini_BindXashStartup(nullptr, true);

    char arg0[] = "xash";
    char* argv[] = {arg0, nullptr};
    int window = 0;
    char cmdline[] = "line";
    char basedir[] = "valve";

    csr_xash_engine_argv(1, argv);
    csr_xash_engine_mode(&window, nullptr, nullptr, "gl", nullptr, 0);
    csr_xash_engine_init_game(cmdline, basedir, &window, 1, 0);
    csr_xash_engine_shutdown();
    csr_xash_engine_error("fatal");

    const EngineMiniXashStartup* record = EngineMini_XashStartup();
    EXPECT_TRUE(record->engine_is_xash);
    EXPECT_EQ(record->argv_calls, 0);
    EXPECT_EQ(record->mode_calls, 0);
    EXPECT_EQ(record->init_calls, 0);
    EXPECT_EQ(record->shutdown_calls, 0);
    EXPECT_EQ(record->error_calls, 0);
    EXPECT_EQ(record->initializing_calls, 0);
}

TEST_F(XashStartup, WiredCallDoesNotOpenHwSo)
{
    Dl_info info{};
    ASSERT_NE(dladdr(reinterpret_cast<void*>(csr_xash_engine_argv), &info), 0);
    ASSERT_NE(info.dli_fname, nullptr);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "csretro-xash-startup-wire";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::copy_file(info.dli_fname, root / "hw.so");

    const char* previous_path = std::getenv("LD_LIBRARY_PATH");
    const std::string saved_path = previous_path != nullptr ? previous_path : "";
    ASSERT_EQ(setenv("LD_LIBRARY_PATH", root.c_str(), 1), 0);

    auto engine = std::make_unique<nitroapi::EngineData>();
    EngineMini_BindXashStartup(engine.get(), true);

    {
        const WorkingDirectory directory(root);
        char arg0[] = "xash";
        char arg1[] = "-console";
        char* argv[] = {arg0, arg1, nullptr};
        int window = 0;
        char cmdline[] = "-game cstrike";
        char basedir[] = "valve";

        csr_xash_engine_argv(2, argv);
        csr_xash_engine_mode(&window, nullptr, nullptr, "gl", "-windowed", 1);
        csr_xash_engine_init_game(cmdline, basedir, &window, 0, 1);
        EXPECT_EQ(EngineMini_XashStartup()->initializing_calls, 1);
        EXPECT_EQ(EngineMini_XashStartup()->shutdown_calls, 0);
        EXPECT_EQ(EngineMini_XashStartup()->error_calls, 0);
        EXPECT_FALSE(MapsContain(root / "hw.so"));
        csr_xash_engine_shutdown();
        csr_xash_engine_error("fatal");
    }

    EXPECT_EQ(EngineMini_XashStartup()->argv_calls, 1);
    EXPECT_NE(COM_CheckParm("-console"), 0);
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
