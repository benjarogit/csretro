#include "native_gameui_funcs.h"
#include "vgui_binding.h"
#include "build_registry.h"
#include "GameUiProbe.h"
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <climits>
#include <memory>
#include <string>

namespace
{
    using Module = std::unique_ptr<void, int (*)(void*)>;
    class OriginalGameUiFuncs : public testing::Test
    {
    protected:
        void SetUp() override
        {
            key.reset(dlopen(CLIENT_KEY_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            client.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            vgui.reset(dlopen(CLIENT_VGUI_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            video_module.reset(dlopen(CLIENT_VIDEO_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            consumer.reset(dlopen(CLIENT_LEGACY_GAMEUI_CONSUMER, RTLD_NOW | RTLD_LOCAL));
            ASSERT_TRUE(key && client && vgui && video_module && consumer) << dlerror();
            Sym<void (*)()>(key, "FixtureKeysReset")();
            Sym<void (*)()>(client, "FixtureClientBindingReset")();
            Sym<void (*)(int)>(vgui, "FixtureVguiConfigure")(0);
            configure_video = Sym<void (*)(int)>(video_module, "FixtureVideoConfigure");
            configure_video(0);
            callbacks = *Sym<goldsrc::cl_enginefunc_t* (*)()>(video_module, "FixtureVideoEngine")();
            callbacks.Key_LookupBinding = Sym<goldsrc::cl_enginefunc_t* (*)()>(key, "FixtureKeyCallbacks")()->Key_LookupBinding;
            find = Sym<goldsrc::HUD_KB_FIND_FUNC>(key, "FixtureFindAction");
            set_binding = Sym<void (*)(int, const char*)>(key, "FixtureSetBinding");
            set_action = Sym<void (*)(int)>(key, "FixtureSetActionState");
            consume = Sym<void (*)(void*, const char*, const char*, int, const char*, GameUiProbe*)>(consumer, "FixtureConsumeGameUi");
        }
        template<class T> T Sym(Module& module, const char* name) { return reinterpret_cast<T>(dlsym(module.get(), name)); }
        bool BindKeys(std::string& error)
        {
            return keys.Bind(key.get(), key.get(), vgui.get(), &callbacks, find, 265, error);
        }
        bool BindAll(std::string& error)
        {
            return video.Bind(video_module.get(), &callbacks, error) && BindKeys(error) && funcs.Bind(error);
        }
        int Invalid() { return Sym<int (*)()>(key, "FixtureInvalidKeyQueries")(); }
        int LastSdl() { return Sym<int (*)()>(vgui, "FixtureLastSdlKey")(); }
        Module key{nullptr,&dlclose}, client{nullptr,&dlclose}, vgui{nullptr,&dlclose}, video_module{nullptr,&dlclose}, consumer{nullptr,&dlclose};
        goldsrc::cl_enginefunc_t callbacks{};
        goldsrc::HUD_KB_FIND_FUNC find{};
        void (*configure_video)(int){};
        void (*set_binding)(int,const char*){};
        void (*set_action)(int){};
        void (*consume)(void*,const char*,const char*,int,const char*,GameUiProbe*){};
        csr::client::GameUiVideo video;
        csr::client::XashKeyQueries keys;
        csr::client::NativeGameUiFuncs funcs{video,keys};
    };
}

TEST_F(OriginalGameUiFuncs, SeparatelyCompiledOldAbiConsumerCallsAllNineMethods)
{
    std::string error;
    ASSERT_TRUE(BindAll(error)) << error;
    GameUiProbe result{};
    consume(&funcs, "in_mlook", "+attack", 97, "A", &result);
    EXPECT_EQ(result.exists, 1);
    EXPECT_EQ(result.down, 1);
    EXPECT_STREQ(result.key_name, "a");
    EXPECT_STREQ(result.binding, "+attack");
    EXPECT_EQ(result.vgui_code, 500);
    EXPECT_EQ(LastSdl(), 97);
    EXPECT_EQ(result.key_number, 97);
    EXPECT_EQ(result.count, 2);
    EXPECT_EQ(result.mode_wide, 1920);
    EXPECT_EQ(result.mode_tall, 1080);
    EXPECT_EQ(result.mode_bpp, 32);
    EXPECT_EQ(result.current_wide, 576);
    EXPECT_EQ(result.current_tall, 324);
    EXPECT_EQ(result.current_bpp, 24);
    EXPECT_STREQ(result.renderer, "gl");
    EXPECT_EQ(result.windowed, 1);
    EXPECT_EQ(result.hd, 1);
    EXPECT_EQ(result.addons, 0);
    EXPECT_EQ(result.guard, 0x12345678); // The old renderer has no sixth output.
    EXPECT_EQ(result.vac, 0); // Never claim VAC authentication on Xash.
}

TEST_F(OriginalGameUiFuncs, ActionStateComesFromOriginalClientKbuttonNotPhysicalSdlKeys)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    for (int state : {0,1,2,3,4,7})
    {
        set_action(state);
        bool down = true;
        EXPECT_TRUE(funcs.IsKeyDown("in_mlook", down));
        EXPECT_EQ(down, (state & 1) != 0);
    }
    bool down = true;
    EXPECT_FALSE(funcs.IsKeyDown("unknown", down));
    EXPECT_FALSE(down);
    EXPECT_FALSE(funcs.IsKeyDown(nullptr, down));
}

TEST_F(OriginalGameUiFuncs, LiveBindingChangesAndUnboundKeysAreReadFromEngine)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    EXPECT_STREQ(funcs.Key_BindingForKey(97), "+attack");
    set_binding(97, "+jump");
    EXPECT_STREQ(funcs.Key_BindingForKey(97), "+jump");
    EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind("+attack"), 0);
    EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind("+jump"), 500);
    set_binding(97, nullptr);
    EXPECT_STREQ(funcs.Key_BindingForKey(97), "");
    EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind("+jump"), 0);
}

TEST_F(OriginalGameUiFuncs, KeyToSdlTranslationUsesNativeVguiHelperInsteadOfEngineNumbers)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    set_binding(97, nullptr);
    struct Pair { int engine, sdl; };
    for (const auto pair : {Pair{128,1073741906},Pair{135,1073741882},Pair{134,1073742049},
                           Pair{133,1073742048},Pair{160,1073741919},Pair{172,1073741908},Pair{59,59}})
    {
        set_binding(pair.engine, "test-command");
        EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind("test-command"), 500);
        EXPECT_EQ(LastSdl(), pair.sdl);
        set_binding(pair.engine, nullptr);
    }
    set_binding(200, "mouse-command");
    EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind("mouse-command"), 0);
}

TEST_F(OriginalGameUiFuncs, InvalidLegacyKeysNeverReachEngineArrayAndUnknownNamesStayInvalid)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    for (int invalid : {-1,256,264,265,INT_MIN,INT_MAX})
    {
        EXPECT_STREQ(funcs.Key_NameForKey(invalid), "");
        EXPECT_STREQ(funcs.Key_BindingForKey(invalid), "");
    }
    EXPECT_EQ(Invalid(), 0);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum(nullptr), -1);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum(""), -1);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum("not-a-key"), -1);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum("<UNKNOWN KEYNUM>"), -1);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum("SEMICOLON"), 59);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum("UPARROW"), 128);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum("<"), 60);
    std::string huge(256,'x');
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum(huge.c_str()), -1);
    EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind(huge.c_str()), 0);
}

TEST_F(OriginalGameUiFuncs, InverseNameLookupSurvivesEngineSharedScratchBuffer)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    const char* borrowed = funcs.Key_NameForKey(97);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum(borrowed), 97);
}

TEST_F(OriginalGameUiFuncs, ModeViewIsStableAndCannotCorruptBackendData)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    csr::client::NativeGameUiFuncs::Mode* mode{};
    int count{};
    funcs.GetVideoModes(&mode,&count);
    ASSERT_EQ(count,2);
    auto original = mode;
    mode[0].wide = 77;
    const csr::client::GameUiVideo::Mode* backend{};
    EXPECT_TRUE(video.GetVideoModes(backend,count));
    EXPECT_EQ(backend[0].wide,1920);
    configure_video(21);
    funcs.GetVideoModes(&mode,&count);
    EXPECT_EQ(mode,original);
    int wide{},tall{},bpp{};
    funcs.GetCurrentVideoMode(&wide,&tall,&bpp);
    EXPECT_EQ(wide,800);
    EXPECT_EQ(bpp,16);
}

TEST_F(OriginalGameUiFuncs, FactoryAdvertisesOnlyCompleteLiveProviderAndWithdrawsItOnFailure)
{
    std::string error;
    csr::client::VguiBinding binding;
    int surface{};
    ASSERT_TRUE(binding.BindInterfaces(client.get(),vgui.get(),&surface,error,&funcs));
    auto query = Sym<void* (*)(int*)>(vgui,"FixtureQueryGameUiFuncs");
    ASSERT_NE(query,nullptr);
    int code{};
    EXPECT_EQ(query(&code),nullptr);
    EXPECT_EQ(code,1);
    ASSERT_TRUE(BindAll(error));
    EXPECT_EQ(query(&code),&funcs);
    EXPECT_EQ(code,0);
    configure_video(1);
    EXPECT_EQ(query(&code),nullptr);
    EXPECT_FALSE(funcs.IsReady());
    configure_video(0);
    EXPECT_TRUE(funcs.IsReady());
    funcs.Shutdown();
    EXPECT_EQ(query(&code),nullptr);
    EXPECT_EQ(code,1);
}

TEST_F(OriginalGameUiFuncs, MissingOrWrongModuleFunctionsCannotBecomeNativeProvider)
{
    std::string error;
    EXPECT_FALSE(funcs.Bind(error));
    ASSERT_TRUE(video.Bind(video_module.get(),&callbacks,error));
    EXPECT_FALSE(funcs.Bind(error));
    EXPECT_FALSE(keys.Bind(nullptr,key.get(),vgui.get(),&callbacks,find,265,error));
    EXPECT_FALSE(keys.Bind(key.get(),key.get(),vgui.get(),&callbacks,nullptr,265,error));
    EXPECT_FALSE(keys.Bind(key.get(),key.get(),vgui.get(),&callbacks,find,32,error));
    EXPECT_FALSE(keys.Bind(key.get(),key.get(),vgui.get(),&callbacks,find,99999,error));
    EXPECT_FALSE(keys.Bind(video_module.get(),key.get(),vgui.get(),&callbacks,find,265,error));
    EXPECT_FALSE(keys.Bind(key.get(),video_module.get(),vgui.get(),&callbacks,find,265,error));
    EXPECT_FALSE(keys.Bind(key.get(),key.get(),video_module.get(),&callbacks,find,265,error));
    EXPECT_FALSE(funcs.IsReady());
    EXPECT_TRUE(BindKeys(error));
    EXPECT_TRUE(funcs.Bind(error));
    EXPECT_FALSE(funcs.Bind(error));
    EXPECT_FALSE(BindKeys(error));
}

TEST_F(OriginalGameUiFuncs, ProductionOwnerRejectsUnregisteredFixtureAndNeverStartsEngine)
{
    std::string error;
    csr::client::XashKeys production;
    EXPECT_FALSE(production.Bind(key.get(),vgui.get(),&callbacks,find,error));
    EXPECT_NE(error.find("fingerprint"),std::string::npos);
    EXPECT_FALSE(production.Queries().IsBound());
    EXPECT_EQ(Sym<int (*)()>(key,"FixtureHostMainCalls")(),0);
    EXPECT_FALSE(nitroapi::RegisteredXashKeyCount(""));
    EXPECT_FALSE(nitroapi::RegisteredXashKeyCount(std::string(64,'0')));
    EXPECT_EQ(nitroapi::RegisteredXashKeyCount("d9f3776b156cc906a1016882d3154de8726c71cb4242fe9ed02fa44203af81db"),265);
}

TEST_F(OriginalGameUiFuncs, ShutdownRemovesAllBackendAccessWithoutFakePositiveResults)
{
    std::string error;
    ASSERT_TRUE(BindAll(error));
    funcs.Shutdown();
    keys.Shutdown();
    video.Shutdown();
    bool down=true;
    EXPECT_FALSE(funcs.IsKeyDown("in_mlook",down));
    EXPECT_FALSE(down);
    EXPECT_EQ(funcs.Key_KeyStringToKeyNum("a"),-1);
    EXPECT_STREQ(funcs.Key_BindingForKey(97),"");
    EXPECT_EQ(funcs.GetVGUI2KeyCodeForBind("+attack"),0);
    EXPECT_FALSE(funcs.IsReady());
    EXPECT_TRUE(BindAll(error));
}
