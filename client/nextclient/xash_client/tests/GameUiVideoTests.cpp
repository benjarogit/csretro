#include "gameui_video.h"
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <memory>
#include <cstring>

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;
    class NativeGameUiVideo : public testing::Test
    {
    protected:
        void SetUp() override
        {
            module.reset(dlopen(CLIENT_VIDEO_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_NE(module, nullptr) << dlerror();
            configure = reinterpret_cast<void (*)(int)>(dlsym(module.get(), "FixtureVideoConfigure"));
            auto read = reinterpret_cast<goldsrc::cl_enginefunc_t* (*)()>(dlsym(module.get(), "FixtureVideoEngine"));
            ASSERT_NE(configure, nullptr);
            ASSERT_NE(read, nullptr);
            configure(0);
            engine = read();
        }
        Module module{nullptr, &dlclose};
        void (*configure)(int){};
        goldsrc::cl_enginefunc_t* engine{};
    };
}

TEST_F(NativeGameUiVideo, ReadsActualEngineDimensionsPixelFormatAndDistinctDisplayModes)
{
    csr::client::GameUiVideo video;
    std::string error;
    ASSERT_TRUE(video.Bind(module.get(), engine, error)) << error;
    csr::client::GameUiVideo::Mode current{};
    ASSERT_TRUE(video.GetCurrentVideoMode(current));
    EXPECT_EQ(current.wide, 576);
    EXPECT_EQ(current.tall, 324);
    EXPECT_EQ(current.bpp, 24);
    const csr::client::GameUiVideo::Mode* modes{};
    int count{};
    ASSERT_TRUE(video.GetVideoModes(modes, count));
    ASSERT_EQ(count, 2); // Refresh-rate variants collapse, real sizes/bpp remain.
    EXPECT_EQ(modes[0].wide, 1920);
    EXPECT_EQ(modes[0].tall, 1080);
    EXPECT_EQ(modes[0].bpp, 32);
    EXPECT_EQ(modes[1].wide, 1280);
    EXPECT_EQ(modes[1].bpp, 16);
}

TEST_F(NativeGameUiVideo, FiveArgumentRendererReadsLiveFlagsWithoutWritingASixthOutput)
{
    csr::client::GameUiVideo video;
    std::string error;
    ASSERT_TRUE(video.Bind(module.get(), engine, error));
    struct { char name[3]; char guard; } text{{}, 'x'};
    struct { int windowed; int hd; int addon; int guard; } flags{-1,-1,-1,0x12345678};
    EXPECT_TRUE(video.GetCurrentRenderer(text.name, sizeof(text.name), &flags.windowed, &flags.hd, &flags.addon));
    EXPECT_STREQ(text.name, "gl");
    EXPECT_EQ(text.guard, 'x');
    EXPECT_EQ(flags.windowed, 1);
    EXPECT_EQ(flags.hd, 1);
    EXPECT_EQ(flags.addon, 0);
    EXPECT_EQ(flags.guard, 0x12345678);
    configure(21);
    char name[64]{};
    EXPECT_TRUE(video.GetCurrentRenderer(name, sizeof(name), &flags.windowed, &flags.hd, &flags.addon));
    EXPECT_STREQ(name, "other-renderer");
    EXPECT_EQ(flags.windowed, 0);
    EXPECT_EQ(flags.hd, 0);
    EXPECT_EQ(flags.addon, 1);
    EXPECT_EQ(flags.guard, 0x12345678);
}

TEST_F(NativeGameUiVideo, SnapshotStorageRemainsStableUntilShutdownAndCurrentModeStaysLive)
{
    csr::client::GameUiVideo video;
    std::string error;
    ASSERT_TRUE(video.Bind(module.get(), engine, error));
    const csr::client::GameUiVideo::Mode* original{};
    int count{};
    ASSERT_TRUE(video.GetVideoModes(original, count));
    EXPECT_FALSE(video.Bind(module.get(), engine, error));
    configure(21);
    csr::client::GameUiVideo::Mode current{};
    EXPECT_TRUE(video.GetCurrentVideoMode(current));
    EXPECT_EQ(current.wide, 800);
    EXPECT_EQ(current.tall, 450);
    EXPECT_EQ(current.bpp, 16);
    const csr::client::GameUiVideo::Mode* again{};
    EXPECT_TRUE(video.GetVideoModes(again, count));
    EXPECT_EQ(again, original);
    EXPECT_EQ(again[0].wide, 1920);
    video.Shutdown();
    EXPECT_FALSE(video.GetVideoModes(again, count));
    EXPECT_EQ(again, nullptr);
    EXPECT_EQ(count, 0);
    EXPECT_FALSE(video.GetCurrentVideoMode(current));
    EXPECT_EQ(current.wide, 0);
    EXPECT_TRUE(video.Bind(module.get(), engine, error));
}

TEST_F(NativeGameUiVideo, MissingOrInvalidLiveDataCannotProduceFakeDefaultModesOrRenderer)
{
    for (int scenario : {1,2,3,4,5,6,7,8,9,10,11,12,13,15,16,17,18,19})
    {
        SCOPED_TRACE(scenario);
        configure(scenario);
        csr::client::GameUiVideo video;
        std::string error;
        EXPECT_FALSE(video.Bind(module.get(), engine, error));
        EXPECT_FALSE(error.empty());
        EXPECT_FALSE(video.IsBound());
        const csr::client::GameUiVideo::Mode* modes = reinterpret_cast<const csr::client::GameUiVideo::Mode*>(1);
        int count = 42;
        EXPECT_FALSE(video.GetVideoModes(modes, count));
        EXPECT_EQ(modes, nullptr);
        EXPECT_EQ(count, 0);
    }
}

TEST_F(NativeGameUiVideo, LossOfWindowOrCvarsAfterBindingFailsQueriesRatherThanReusingOldValues)
{
    csr::client::GameUiVideo video;
    std::string error;
    ASSERT_TRUE(video.Bind(module.get(), engine, error));
    for (int scenario : {1,6,7,8,9,15,19})
    {
        configure(scenario);
        char renderer[32] = "stale";
        int windowed=42,hd=42,addon=42;
        EXPECT_FALSE(video.GetCurrentRenderer(renderer, sizeof(renderer), &windowed, &hd, &addon));
        EXPECT_STREQ(renderer, "");
        EXPECT_EQ(windowed, 0);
        EXPECT_EQ(hd, 0);
        EXPECT_EQ(addon, 0);
    }
    configure(1);
    csr::client::GameUiVideo::Mode mode{42,42,42};
    EXPECT_FALSE(video.GetCurrentVideoMode(mode));
    EXPECT_EQ(mode.wide, 0);
    EXPECT_EQ(mode.tall, 0);
    EXPECT_EQ(mode.bpp, 0);
}

TEST_F(NativeGameUiVideo, RendererBoundsAndOptionalOutputsDoNotOverwriteMemory)
{
    csr::client::GameUiVideo video;
    std::string error;
    ASSERT_TRUE(video.Bind(module.get(), engine, error));
    char tiny[2] = {'x','y'};
    EXPECT_TRUE(video.GetCurrentRenderer(tiny, 1, nullptr, nullptr, nullptr));
    EXPECT_EQ(tiny[0], '\0');
    EXPECT_EQ(tiny[1], 'y');
    EXPECT_FALSE(video.GetCurrentRenderer(tiny, 0, nullptr, nullptr, nullptr));
    EXPECT_FALSE(video.GetCurrentRenderer(tiny, -1, nullptr, nullptr, nullptr));
    EXPECT_FALSE(video.GetCurrentRenderer(nullptr, 1, nullptr, nullptr, nullptr));
    EXPECT_TRUE(video.GetCurrentRenderer(nullptr, 0, nullptr, nullptr, nullptr));
    video.Shutdown();
    EXPECT_FALSE(video.GetCurrentRenderer(tiny, 1, nullptr, nullptr, nullptr));
}

TEST_F(NativeGameUiVideo, MissingCallbacksAndBorrowedSdlSymbolsFailBeforePublication)
{
    csr::client::GameUiVideo video;
    std::string error;
    EXPECT_FALSE(video.Bind(nullptr, engine, error));
    EXPECT_FALSE(video.Bind(module.get(), nullptr, error));
    auto callbacks = *engine;
    callbacks.pfnGetCvarPointer = nullptr;
    EXPECT_FALSE(video.Bind(module.get(), &callbacks, error));
    callbacks = *engine;
    callbacks.pfnGetScreenInfo = nullptr;
    EXPECT_FALSE(video.Bind(module.get(), &callbacks, error));
    for (auto path : {CLIENT_NO_EXPORT_FIXTURE, CLIENT_VIDEO_NO_MASKS_FIXTURE})
    {
        Module missing(dlopen(path, RTLD_NOW | RTLD_LOCAL), &dlclose);
        ASSERT_NE(missing, nullptr) << dlerror();
        EXPECT_FALSE(video.Bind(missing.get(), engine, error));
        EXPECT_NE(error.find("GameUI video is missing SDL_"), std::string::npos);
    }
    EXPECT_TRUE(video.Bind(module.get(), engine, error));
}

TEST_F(NativeGameUiVideo, LostEngineCallbacksAfterBindingDoNotCallNullOrReturnStaleData)
{
    csr::client::GameUiVideo video;
    std::string error;
    ASSERT_TRUE(video.Bind(module.get(), engine, error));
    engine->pfnGetScreenInfo = nullptr;
    csr::client::GameUiVideo::Mode mode{1,2,3};
    EXPECT_FALSE(video.GetCurrentVideoMode(mode));
    EXPECT_EQ(mode.wide, 0);
    engine->pfnGetCvarPointer = nullptr;
    char name[32] = "stale";
    int windowed=42,hd=42,addon=42;
    EXPECT_FALSE(video.GetCurrentRenderer(name, sizeof(name), &windowed, &hd, &addon));
    EXPECT_STREQ(name, "");
    EXPECT_EQ(windowed, 0);
    EXPECT_EQ(hd, 0);
    EXPECT_EQ(addon, 0);
}
