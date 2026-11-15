#include "OriginalUiModules.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include "XashCommandAccess.h"
#include "build_registry.h"
#include "elf_symbols.h"

namespace
{
    class OriginalModules : public testing::Test
    {
    protected:
        using Module = std::unique_ptr<void, int (*)(void*)>;
        using Factory = void* (*)(const char*, int*);
        static OriginalModules* current;
        Module client{nullptr, &dlclose};
        Module filesystem{nullptr, &dlclose};
        Module gameui{nullptr, &dlclose};
        nitroapi::XashCommandAccess commands;
        std::unique_ptr<csr::client::OriginalUiModules> owner;
        goldsrc::cl_enginefunc_t engine{};
        std::string client_hash;
        std::string filesystem_hash;
        std::string gameui_hash;
        std::string filesystem_path;
        std::string gameui_path;
        std::string root;
        std::string error;
        bool accept_gameui{true};
        bool accept_filesystem{true};
        int expand_mode{};
        std::vector<std::filesystem::path> temporary;
        void (*set_globals)(void*, void*, void*, void*, void*){};
        void (*set_fs_factory)(Factory){};
        void (*set_fs_root)(const char*){};
        void (*set_taskbar)(void*){};
        Factory fs_factory{};
        Factory game_factory{};
        void* game{};
        void* console{};
        void* files{};
        int token{};

        template <class T>
        T Symbol(Module& module, const char* name)
        {
            return reinterpret_cast<T>(dlsym(module.get(), name));
        }
        static bool GamePair(const std::string& client, const std::string& game)
        {
            return current->accept_gameui && client == current->client_hash && game == current->gameui_hash;
        }
        static bool FilePair(const std::string& engine, const std::string& filesystem)
        {
            return current->accept_filesystem && engine == "synthetic-engine" && filesystem == current->filesystem_hash;
        }
        static int Expand(const char* name, char* dest, int capacity)
        {
            if (current->expand_mode == 1)
            {
                return 0;
            }
            if (current->expand_mode == 2)
            {
                std::snprintf(dest, capacity, "relative/module.so");
                return 1;
            }
            if (current->expand_mode == 3)
            {
                std::memset(dest, 'x', capacity);
                return 1;
            }
            const auto& path = std::strcmp(name, "filesystem_stdio.so") == 0 ? current->filesystem_path : current->gameui_path;
            std::snprintf(dest, capacity, "%s", path.c_str());
            return 1;
        }
        void SetUp() override
        {
            current = this;
            client.reset(dlopen(CLIENT_CALLBACK_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            filesystem.reset(dlopen(CLIENT_NATIVE_COMMAND_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            gameui.reset(dlopen(CLIENT_NATIVE_GAMEUI_FIXTURE, RTLD_NOW | RTLD_LOCAL));
            ASSERT_TRUE(client && filesystem && gameui) << dlerror();
            Symbol<void (*)()>(client, "FixtureClientBindingReset")();
            Symbol<void (*)()>(gameui, "FixtureGameUiReset")();
            set_globals = Symbol<decltype(set_globals)>(client, "FixtureClientUiModules");
            set_fs_factory = Symbol<decltype(set_fs_factory)>(filesystem, "FixtureNativeFileSystemFactory");
            set_fs_root = Symbol<decltype(set_fs_root)>(filesystem, "FixtureNativeFileSystemRoot");
            set_taskbar = Symbol<decltype(set_taskbar)>(gameui, "FixtureGameUiTaskbar");
            fs_factory = Symbol<Factory>(filesystem, "CreateInterface");
            game_factory = Symbol<Factory>(gameui, "CreateInterface");
            ASSERT_TRUE(set_globals && set_fs_factory && set_fs_root && set_taskbar && fs_factory && game_factory);
            fs_factory = reinterpret_cast<Factory>(nitroapi::FindFunctionAddress(filesystem.get(), "CreateInterface"));
            ASSERT_NE(fs_factory, nullptr);
            set_fs_factory(fs_factory);
            root = std::filesystem::canonical(std::filesystem::temp_directory_path()).string();
            set_fs_root(root.c_str());
            ASSERT_TRUE(commands.Bind(filesystem.get(), error)) << error;
            filesystem_path = std::filesystem::canonical(CLIENT_NATIVE_COMMAND_FIXTURE).string();
            gameui_path = std::filesystem::canonical(CLIENT_NATIVE_GAMEUI_FIXTURE).string();
            client_hash = nitroapi::HashFileSha256(CLIENT_CALLBACK_FIXTURE);
            filesystem_hash = nitroapi::HashFileSha256(filesystem_path);
            gameui_hash = nitroapi::HashFileSha256(gameui_path);
            engine.COM_ExpandFilename = Expand;
            owner = std::make_unique<csr::client::OriginalUiModules>(csr::client::OriginalUiModules::FingerprintChecks{GamePair, FilePair});
            game = game_factory("GameUI007", nullptr);
            console = game_factory("GameConsole003", nullptr);
            files = fs_factory("VFileSystem009", nullptr);
            ASSERT_TRUE(game && console && files);
            Publish();
        }
        void TearDown() override
        {
            if (owner)
            {
                owner->Reset();
            }
            if (client)
            {
                set_globals(nullptr, nullptr, nullptr, nullptr, nullptr);
            }
            commands.Shutdown();
            gameui.reset();
            filesystem.reset();
            client.reset();
            for (const auto& path : temporary)
            {
                std::filesystem::remove_all(path);
            }
            current = nullptr;
        }
        void Publish(void* game_override = nullptr, void* console_override = nullptr)
        {
            set_globals(
                gameui.get(), game_override ? game_override : game, console_override ? console_override : console, filesystem.get(), files
            );
        }
        bool Preflight()
        {
            return owner->Preflight(&engine, client_hash, "synthetic-engine", commands, error);
        }
        std::filesystem::path Copy(const std::filesystem::path& source)
        {
            std::string pattern = (std::filesystem::temp_directory_path() / "csretro-ui-modules-XXXXXX").string();
            char* directory = mkdtemp(pattern.data());
            if (!directory)
            {
                throw std::runtime_error("Cannot create synthetic module test directory");
            }
            temporary.emplace_back(directory);
            auto target = temporary.back() / source.filename();
            std::filesystem::copy_file(source, target);
            return target;
        }
    };
    OriginalModules* OriginalModules::current{};
} // namespace

TEST_F(OriginalModules, ShippingDefaultPolicyRejectsSyntheticFingerprints)
{
    csr::client::OriginalUiModules production;
    EXPECT_FALSE(production.Preflight(&engine, client_hash, "synthetic-engine", commands, error));
    EXPECT_EQ(production.FileSystemModule(), nullptr);
    EXPECT_EQ(production.GameUiModule(), nullptr);
    EXPECT_FALSE(error.empty());
}
TEST_F(OriginalModules, TestInjectionCannotBypassMissingOperationsOrEnginePrerequisites)
{
    csr::client::OriginalUiModules incomplete(csr::client::OriginalUiModules::FingerprintChecks{});
    EXPECT_FALSE(incomplete.Preflight(&engine, client_hash, "synthetic-engine", commands, error));
    EXPECT_FALSE(owner->Preflight(nullptr, client_hash, "synthetic-engine", commands, error));
    auto copy = engine;
    copy.COM_ExpandFilename = nullptr;
    EXPECT_FALSE(owner->Preflight(&copy, client_hash, "synthetic-engine", commands, error));
}
TEST_F(OriginalModules, ExactRetainedModulesAndNativeFactoryObjectsAreCaptured)
{
    ASSERT_TRUE(Preflight()) << error;
    EXPECT_EQ(owner->FileSystemModule(), filesystem.get());
    EXPECT_EQ(owner->FileSystemInterface(), files);
    EXPECT_EQ(owner->GameUiModule(), nullptr);
    ASSERT_TRUE(owner->Capture(client.get(), error)) << error;
    EXPECT_EQ(owner->GameUiModule(), gameui.get());
    EXPECT_EQ(owner->GameUiInterface(), game);
    EXPECT_EQ(owner->GameConsoleInterface(), console);
    EXPECT_FALSE(owner->Capture(client.get(), error));
}
TEST_F(OriginalModules, RefusesCaptureBeforePreflightAndRepeatedPreflight)
{
    EXPECT_FALSE(owner->Capture(client.get(), error));
    ASSERT_TRUE(Preflight()) << error;
    EXPECT_FALSE(Preflight());
}
TEST_F(OriginalModules, RejectsUnknownGamePairBeforeAnyNativeModuleIsPublished)
{
    accept_gameui = false;
    EXPECT_FALSE(Preflight());
    EXPECT_EQ(owner->FileSystemModule(), nullptr);
    EXPECT_EQ(owner->GameUiModule(), nullptr);
}
TEST_F(OriginalModules, RejectsUnknownFilesystemPair)
{
    accept_filesystem = false;
    EXPECT_FALSE(Preflight());
    EXPECT_EQ(owner->FileSystemModule(), nullptr);
}
TEST_F(OriginalModules, RejectsUnresolvedRelativeAndUnterminatedEnginePaths)
{
    for (int mode : {1, 2, 3})
    {
        owner->Reset();
        expand_mode = mode;
        EXPECT_FALSE(Preflight()) << mode;
        EXPECT_EQ(owner->FileSystemModule(), nullptr);
    }
}
TEST_F(OriginalModules, FilesystemMustAlreadyBeLoadedNoNewLoadIsAttempted)
{
    filesystem_path = Copy(CLIENT_NATIVE_COMMAND_FIXTURE).string();
    EXPECT_FALSE(Preflight());
    Module probe(dlopen(filesystem_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
    EXPECT_FALSE(probe);
}
TEST_F(OriginalModules, EqualFilesystemBytesInAnotherMappingDoNotOwnEngineState)
{
    filesystem_path = Copy(CLIENT_NATIVE_COMMAND_FIXTURE).string();
    Module duplicate(dlopen(filesystem_path.c_str(), RTLD_NOW | RTLD_LOCAL), &dlclose);
    ASSERT_TRUE(duplicate) << dlerror();
    EXPECT_FALSE(Preflight());
    EXPECT_NE(error.find("actual live Xash"), std::string::npos);
}
TEST_F(OriginalModules, FilesystemOwnerCannotChangeAfterPreflight)
{
    ASSERT_TRUE(Preflight()) << error;
    set_fs_factory(nullptr);
    EXPECT_FALSE(owner->Capture(client.get(), error));
    EXPECT_EQ(owner->GameUiInterface(), nullptr);
}
TEST_F(OriginalModules, ResolutionCannotChangeBetweenPreflightAndCapture)
{
    ASSERT_TRUE(Preflight()) << error;
    gameui_path = CLIENT_CALLBACK_FIXTURE;
    EXPECT_FALSE(owner->Capture(client.get(), error));
    EXPECT_EQ(owner->GameUiModule(), nullptr);
}
TEST_F(OriginalModules, ClientModuleMustMatchThePreflightClientHash)
{
    ASSERT_TRUE(Preflight()) << error;
    EXPECT_FALSE(owner->Capture(filesystem.get(), error));
    EXPECT_EQ(owner->GameUiInterface(), nullptr);
}
TEST_F(OriginalModules, UnknownOpaqueClientHandlesAreComparedWithoutDereferencingThem)
{
    ASSERT_TRUE(Preflight()) << error;
    set_globals(reinterpret_cast<void*>(1), game, console, filesystem.get(), files);
    EXPECT_FALSE(owner->Capture(client.get(), error));
    EXPECT_EQ(owner->GameUiInterface(), nullptr);
}
TEST_F(OriginalModules, OriginalInitializeMustUseTheRetainedFilesystemHandle)
{
    ASSERT_TRUE(Preflight()) << error;
    set_globals(gameui.get(), game, console, reinterpret_cast<void*>(1), files);
    EXPECT_FALSE(owner->Capture(client.get(), error));
}
TEST_F(OriginalModules, OriginalGameAndConsoleGlobalsMustMatchTheirFactoryObjects)
{
    ASSERT_TRUE(Preflight()) << error;
    Publish(&token, nullptr);
    EXPECT_FALSE(owner->Capture(client.get(), error));
    Publish(nullptr, &token);
    EXPECT_FALSE(owner->Capture(client.get(), error));
    EXPECT_EQ(owner->GameUiInterface(), nullptr);
}
TEST_F(OriginalModules, OriginalFilesystemGlobalMustMatchTheEngineOwnedFactoryObject)
{
    ASSERT_TRUE(Preflight()) << error;
    set_globals(gameui.get(), game, console, filesystem.get(), &token);
    EXPECT_FALSE(owner->Capture(client.get(), error));
}
TEST_F(OriginalModules, ExistingBasePanelOrTaskbarRejectsNonFreshGameUi)
{
    ASSERT_TRUE(Preflight()) << error;
    auto* base = reinterpret_cast<void**>(nitroapi::FindSymbolAddress(gameui.get(), "_ZL11staticPanel"));
    ASSERT_NE(base, nullptr);
    *base = &token;
    EXPECT_FALSE(owner->Capture(client.get(), error));
    *base = nullptr;
    set_taskbar(&token);
    EXPECT_FALSE(owner->Capture(client.get(), error));
    set_taskbar(nullptr);
}
TEST_F(OriginalModules, CapturedGameUiIsRetainedAfterTheClientReleasesItsReference)
{
    ASSERT_TRUE(Preflight()) << error;
    ASSERT_TRUE(owner->Capture(client.get(), error)) << error;
    gameui.reset();
    {
        Module probe(dlopen(gameui_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
        ASSERT_TRUE(probe);
        EXPECT_EQ(probe.get(), owner->GameUiModule());
    }
    owner->Reset();
    Module probe(dlopen(gameui_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
    EXPECT_FALSE(probe);
}
TEST_F(OriginalModules, GameUiPreflightDoesNotLoadOrRetainTheModuleBeforeInitialize)
{
    ASSERT_TRUE(Preflight()) << error;
    gameui.reset();
    Module probe(dlopen(gameui_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
    ASSERT_FALSE(probe);
    EXPECT_FALSE(owner->Capture(client.get(), error));
}
TEST_F(OriginalModules, FilesystemIsRetainedAcrossOriginalInitializeUntilReset)
{
    ASSERT_TRUE(Preflight()) << error;
    void* expected = filesystem.get();
    filesystem.reset();
    {
        Module probe(dlopen(filesystem_path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
        ASSERT_TRUE(probe);
        EXPECT_EQ(probe.get(), expected);
    }
    EXPECT_EQ(owner->FileSystemModule(), expected);
}
TEST_F(OriginalModules, BaseDirectoryComesFromNativeFilesystemStateIndependentOfCwd)
{
    auto old = std::filesystem::current_path();
    struct Restore
    {
        std::filesystem::path path;
        ~Restore()
        {
            std::filesystem::current_path(path);
        }
    } restore{old};
    auto new_directory = Copy(CLIENT_CALLBACK_FIXTURE).parent_path();
    std::filesystem::current_path(new_directory);
    ASSERT_TRUE(Preflight()) << error;
    EXPECT_EQ(owner->BaseDirectory(), root);
    EXPECT_NE(owner->BaseDirectory(), std::filesystem::current_path().string());
    ASSERT_TRUE(owner->Capture(client.get(), error)) << error;
    EXPECT_EQ(owner->BaseDirectory(), root);
}
TEST_F(OriginalModules, RejectsEmptyRelativeMissingAndUnterminatedFilesystemRoots)
{
    auto* native = reinterpret_cast<char*>(nitroapi::FindSymbolAddress(filesystem.get(), "fs_rootdir"));
    ASSERT_NE(native, nullptr);
    for (const char* value : {"", "relative-root", "/nonexistent-csretro-fixture-root"})
    {
        owner->Reset();
        set_fs_root(value);
        EXPECT_FALSE(Preflight()) << value;
    }
    owner->Reset();
    std::memset(native, 'x', 1024);
    EXPECT_FALSE(Preflight());
    set_fs_root(root.c_str());
}
TEST_F(OriginalModules, FilesystemRootMustRemainTheSameAcrossOriginalInitialization)
{
    ASSERT_TRUE(Preflight()) << error;
    auto directory = Copy(CLIENT_CALLBACK_FIXTURE).parent_path();
    set_fs_root(directory.c_str());
    EXPECT_FALSE(owner->Capture(client.get(), error));
}
TEST_F(OriginalModules, ResetDropsPublishedObjectsAndPermitsOneNewPreflight)
{
    ASSERT_TRUE(Preflight()) << error;
    ASSERT_TRUE(owner->Capture(client.get(), error)) << error;
    owner->Reset();
    EXPECT_EQ(owner->GameUiModule(), nullptr);
    EXPECT_EQ(owner->FileSystemModule(), nullptr);
    EXPECT_EQ(owner->GameUiInterface(), nullptr);
    EXPECT_EQ(owner->GameConsoleInterface(), nullptr);
    ASSERT_TRUE(Preflight()) << error;
}
