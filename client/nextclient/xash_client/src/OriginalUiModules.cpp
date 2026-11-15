#include "OriginalUiModules.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <dlfcn.h>
#include <link.h>
#include "XashCommandAccess.h"
#include "build_registry.h"
#include "elf_symbols.h"

namespace csr::client
{
    namespace
    {
        using Factory = void* (*)(const char*, int*);
        using Module = std::unique_ptr<void, int (*)(void*)>;

        std::string Canonical(const char* name)
        {
            if (!name || !*name)
            {
                throw std::runtime_error("Native UI module path is empty");
            }
            auto path = std::filesystem::canonical(name);
            if (!path.is_absolute() || !std::filesystem::is_regular_file(path))
            {
                throw std::runtime_error("Native UI module is not an existing absolute regular file");
            }
            return path.string();
        }

        std::string Expanded(goldsrc::cl_enginefunc_t* engine, const char* name)
        {
            std::array<char, 4096> buffer{};
            if (!engine->COM_ExpandFilename(name, buffer.data(), static_cast<int>(buffer.size())) ||
                strnlen(buffer.data(), buffer.size()) == buffer.size())
            {
                throw std::runtime_error("Engine cannot resolve a required original UI module");
            }
            // Relative results would reinterpret the engine path using the caller's working directory.
            if (!std::filesystem::path(buffer.data()).is_absolute())
            {
                throw std::runtime_error("Engine UI module resolution must return an absolute path");
            }
            return Canonical(buffer.data());
        }

        std::string LoadedPath(void* module)
        {
            link_map* map{};
            if (!module || dlinfo(module, RTLD_DI_LINKMAP, &map) != 0 || !map)
            {
                throw std::runtime_error("Cannot identify retained native UI module mapping");
            }
            return Canonical(map->l_name);
        }

        Module Retain(const std::string& path)
        {
            Module module(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD), &dlclose);
            if (!module)
            {
                throw std::runtime_error("Required UI module is not already loaded");
            }
            if (LoadedPath(module.get()) != path)
            {
                throw std::runtime_error("Retained UI module does not match the resolved path");
            }
            return module;
        }

        Factory ModuleFactory(void* module)
        {
            auto factory = reinterpret_cast<Factory>(nitroapi::FindFunctionAddress(module, "CreateInterface"));
            if (!factory)
            {
                throw std::runtime_error("Retained UI module has no verified CreateInterface");
            }
            return factory;
        }

        std::string FileSystemRoot(void* module)
        {
            uintptr_t address = nitroapi::FindSymbolAddress(module, "fs_rootdir");
            if (!address)
            {
                throw std::runtime_error("Verified filesystem has no native root directory");
            }
            std::array<char, 1024> root{};
            std::memcpy(root.data(), reinterpret_cast<const void*>(address), root.size());
            size_t size = strnlen(root.data(), root.size());
            if (!size || size == root.size() || !std::filesystem::path(root.data()).is_absolute())
            {
                throw std::runtime_error("Native filesystem root must be bounded, nonempty and absolute");
            }
            auto path = std::filesystem::canonical(root.data());
            if (!std::filesystem::is_directory(path))
            {
                throw std::runtime_error("Native filesystem root is not an existing directory");
            }
            return path.string();
        }

        void* ReadGlobal(void* module, const char* name)
        {
            uintptr_t address = nitroapi::FindSymbolAddress(module, name);
            if (!address)
            {
                throw std::runtime_error(std::string("Missing original UI global: ") + name);
            }
            void* value{};
            std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
            return value;
        }
    } // namespace

    struct OriginalUiModules::Impl
    {
        bool (*gameui_check)(const std::string&, const std::string&) = nitroapi::IsRegisteredClientGameUi;
        bool (*filesystem_check)(const std::string&, const std::string&) = nitroapi::IsRegisteredXashFileSystem;
        goldsrc::cl_enginefunc_t* engine{};
        nitroapi::XashCommandAccess* commands{};
        std::string client_hash;
        std::string engine_hash;
        std::string gameui_path;
        std::string filesystem_path;
        std::string gameui_hash;
        std::string filesystem_hash;
        std::string base_directory;
        Module gameui{nullptr, &dlclose};
        Module filesystem{nullptr, &dlclose};
        void* files{};
        void* game{};
        void* console{};
        bool attempted{};
        bool prepared{};
        bool captured{};

        bool VerifyHashes() const
        {
            const auto game_hash = nitroapi::HashFileSha256(gameui_path);
            const auto fs_hash = nitroapi::HashFileSha256(filesystem_path);
            return !game_hash.empty() && !fs_hash.empty() && game_hash == gameui_hash && fs_hash == filesystem_hash &&
                   gameui_check(client_hash, game_hash) && filesystem_check(engine_hash, fs_hash);
        }
    };

    OriginalUiModules::OriginalUiModules() :
        impl_(std::make_unique<Impl>())
    {}
    OriginalUiModules::~OriginalUiModules() = default;
#ifdef CSRETRO_NATIVE_MODULE_TESTS
    OriginalUiModules::OriginalUiModules(const FingerprintChecks& checks) :
        OriginalUiModules()
    {
        impl_->gameui_check = checks.gameui;
        impl_->filesystem_check = checks.filesystem;
    }
#endif

    bool OriginalUiModules::Preflight(
        goldsrc::cl_enginefunc_t* engine,
        const std::string& client_hash,
        const std::string& engine_hash,
        nitroapi::XashCommandAccess& commands,
        std::string& error
    )
    {
        error.clear();
        auto& state = *impl_;
        if (state.attempted || !engine || !engine->COM_ExpandFilename || client_hash.empty() || engine_hash.empty() ||
            !commands.IsBound() || !state.gameui_check || !state.filesystem_check)
        {
            error = "Original UI preflight requires unused ownership, live engine paths, identity and command access";
            return false;
        }
        state.attempted = true;
        state.engine = engine;
        state.commands = &commands;
        state.client_hash = client_hash;
        state.engine_hash = engine_hash;
        try
        {
            state.filesystem_path = Expanded(engine, "filesystem_stdio.so");
            state.gameui_path = Expanded(engine, "cl_dlls/gameui.so");
            state.gameui_hash = nitroapi::HashFileSha256(state.gameui_path);
            state.filesystem_hash = nitroapi::HashFileSha256(state.filesystem_path);
            if (!state.VerifyHashes())
            {
                throw std::runtime_error("Original GameUI or Xash filesystem does not match its registered pair");
            }
            state.filesystem = Retain(state.filesystem_path);
            if (!commands.IsFileSystemOwner(state.filesystem.get()))
            {
                throw std::runtime_error("Resolved filesystem is not the actual live Xash filesystem owner");
            }
            state.base_directory = FileSystemRoot(state.filesystem.get());
            state.files = ModuleFactory(state.filesystem.get())("VFileSystem009", nullptr);
            if (!state.files || !state.VerifyHashes())
            {
                throw std::runtime_error("Original UI filesystem interface or immutable preflight identity is invalid");
            }
            state.prepared = true;
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
        }
        catch (...)
        {
            error = "Original UI preflight failed in a native service";
        }
        return false;
    }

    bool OriginalUiModules::Capture(void* client, std::string& error)
    {
        error.clear();
        auto& state = *impl_;
        if (!state.prepared || state.captured || !client)
        {
            error = "Original UI capture requires completed preflight and one successful original Initialize";
            return false;
        }
        try
        {
            if (Expanded(state.engine, "filesystem_stdio.so") != state.filesystem_path ||
                Expanded(state.engine, "cl_dlls/gameui.so") != state.gameui_path || !state.VerifyHashes() ||
                !state.commands->IsFileSystemOwner(state.filesystem.get()) ||
                FileSystemRoot(state.filesystem.get()) != state.base_directory ||
                nitroapi::HashFileSha256(LoadedPath(client)) != state.client_hash)
            {
                throw std::runtime_error("Original UI paths, fingerprints, client or native filesystem state changed after preflight");
            }
            // Read the native handles as opaque values. Never call dlinfo on a client-supplied unknown handle.
            void* actual_gameui = ReadGlobal(client, "g_hGameUIModule");
            void* actual_filesystem = ReadGlobal(client, "g_hFileSystemModule");
            if (!actual_gameui || actual_filesystem != state.filesystem.get())
            {
                throw std::runtime_error("Original Initialize did not use the retained filesystem and a live GameUI module");
            }
            state.gameui = Retain(state.gameui_path);
            if (state.gameui.get() != actual_gameui)
            {
                throw std::runtime_error("Original GameUI handle differs from the retained preflight module");
            }
            void* actual_game = ReadGlobal(client, "g_pGameUI");
            void* actual_console = ReadGlobal(client, "g_pGameConsole");
            void* actual_files = ReadGlobal(client, "g_pFileSystem");
            auto factory = ModuleFactory(state.gameui.get());
            void* game = factory("GameUI007", nullptr);
            void* console = factory("GameConsole003", nullptr);
            if (!game || !console || game != actual_game || console != actual_console || actual_files != state.files ||
                ModuleFactory(state.filesystem.get())("VFileSystem009", nullptr) != state.files)
            {
                throw std::runtime_error("Original UI globals do not match their actual retained factory objects");
            }
            if (ReadGlobal(state.gameui.get(), "_ZL11staticPanel") || ReadGlobal(state.gameui.get(), "g_pTaskbar"))
            {
                throw std::runtime_error("Original GameUI is already initialized; fresh base panel and taskbar are required");
            }
            if (!state.VerifyHashes() || !state.commands->IsFileSystemOwner(state.filesystem.get()))
            {
                throw std::runtime_error("Original UI module identity changed during capture");
            }
            state.game = game;
            state.console = console;
            state.captured = true;
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
        }
        catch (...)
        {
            error = "Original UI module capture failed in a native service";
        }
        return false;
    }

    void* OriginalUiModules::GameUiModule() const
    {
        return impl_->captured ? impl_->gameui.get() : nullptr;
    }
    void* OriginalUiModules::FileSystemModule() const
    {
        return impl_->prepared ? impl_->filesystem.get() : nullptr;
    }
    void* OriginalUiModules::FileSystemInterface() const
    {
        return impl_->prepared ? impl_->files : nullptr;
    }
    void* OriginalUiModules::GameUiInterface() const
    {
        return impl_->captured ? impl_->game : nullptr;
    }
    void* OriginalUiModules::GameConsoleInterface() const
    {
        return impl_->captured ? impl_->console : nullptr;
    }
    const std::string& OriginalUiModules::BaseDirectory() const
    {
        static const std::string empty;
        return impl_->prepared ? impl_->base_directory : empty;
    }
    void OriginalUiModules::Reset()
    {
        auto gameui_check = impl_->gameui_check;
        auto filesystem_check = impl_->filesystem_check;
        impl_ = std::make_unique<Impl>();
        impl_->gameui_check = gameui_check;
        impl_->filesystem_check = filesystem_check;
    }
} // namespace csr::client
