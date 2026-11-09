#include "NitroApi.h"

#include <filesystem>
#include <ranges>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <easylogging++.h>

#include <ncl_perf/profiler.h>
#include <nitroapi/handler_helper/Handler.h>

#include "build_registry.h"
#include "FrameLifecycle.h"
#include "modules/windows/WindowsModule.h"
#include "modules/sdl2/SDL2AddressProvider.h"
#include "modules/sdl2/loaded_sdl_module.h"

#ifndef NITRO_API_STATIC
INITIALIZE_EASYLOGGINGPP
#endif

namespace nitroapi
{
    static nitroapi::NitroApi g_NitroApi;
    NitroApiInterface* GetNitroApi()
    {
        return &g_NitroApi;
    }

    static GlobalExceptionHandlerFunc g_WriteMiniDumpCallback = nullptr;

#ifndef NITRO_API_STATIC
    EXPOSE_SINGLE_INTERFACE_GLOBALVAR(NitroApi, NitroApiInterface, NITROAPI_INTERFACE_VERSION, g_NitroApi);
    static IBaseInterface* CreateFrameLifecycleInterface()
    {
        return reinterpret_cast<IBaseInterface*>(nitroapi::FrameLifecycle_GetInterface());
    }

    EXPOSE_INTERFACE_FN(CreateFrameLifecycleInterface, FrameLifecycleInterface, NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION);

#endif

    NitroApi::~NitroApi()
    {
        g_WriteMiniDumpCallback = nullptr;
    }

    void NitroApi::WriteLog(const char* tag, const char* message)
    {
        el::Loggers::getLogger(tag)->info(message);
    }

    void NitroApi::SetSEHCallback(GlobalExceptionHandlerFunc callback)
    {
        g_WriteMiniDumpCallback = callback;
    }

    GlobalExceptionHandlerFunc NitroApi::GetSEHCallback()
    {
        return g_WriteMiniDumpCallback;
    }

    bool NitroApi::Initialize(ICommandLine* command_line, IFileSystem* file_system, IRegistry* registry)
    {
        if (is_initialized_)
        {
            return true;
        }

        LOG(INFO) << "---------- Initialize | Begin ----------";

        hook_storage_ = std::make_shared<HookStorage>();

        auto windows_module = std::make_unique<WindowsModule>(hook_storage_, nullptr);
        windows_data_ = windows_module->GetWindowsData();
        modules_.emplace_back("<exe>", std::move(windows_module));

        DetectBuildIdentity();

        std::shared_ptr<AddressProviderBase> engine_provider = GetEngineAddressProvider();

        if (engine_provider != nullptr)
        {
            std::string engine_path = IsWindows() ? "hw.dll" : "hw.so";
            auto engine_module = std::make_unique<EngineModule>(
                hook_storage_,
                engine_provider,
                this,
                command_line,
                file_system,
                registry,
                FrameLifecycle_GetDefaultService()
            );
            engine_data_ = engine_module->GetEngineData();
            modules_.emplace_back(engine_path, std::move(engine_module));
        }
        else
        {
            LOG(INFO) << "Can't get engine address provider, engine hooks will not work";
        }

        std::shared_ptr<AddressProviderBase> client_provider = GetClientAddressProvider();

        if (client_provider != nullptr)
        {
            std::string client_path = IsWindows() ? "cstrike\\cl_dlls\\client.dll" : "cstrike/cl_dlls/client.so";
            auto client_module = std::make_unique<ClientModule>(hook_storage_, client_provider);
            client_data_ = client_module->GetClientData();
            modules_.emplace_back(client_path, std::move(client_module));
        }
        else
        {
            LOG(INFO) << "Can't get client address provider, client hooks will not work";
        }

        for (auto& [path, module] : modules_)
        {
            InvokeLibraryLoaded(path, module);
        }

        nitro_utils::SysModule engine_handle = nullptr;

        for (const auto& [path, module] : modules_)
        {
            if (path == "hw.dll" || path == "hw.so")
            {
                engine_handle = module.module_handle;
                break;
            }
        }

        nitro_utils::SysModule sdl_handle = nitroapi::sdl2::FindLoadedModule(engine_handle);

        if (sdl_handle)
        {
            auto sdl2_module = std::make_unique<SDL2Module>(hook_storage_, std::make_shared<SDL2AddressProvider>());
            sdl2_data_ = sdl2_module->GetSDL2Data();
            sdl2_module->OnLibraryLoaded(sdl_handle);
            modules_.emplace_back("<loaded SDL2>", std::move(sdl2_module));
            ModuleHookData& module = std::get<1>(modules_.back());
            module.module_handle = sdl_handle;
            module.borrowed = true;
        }

#if USE_PROFILER
        engine_data_->Host_FilterTime += [](float delta, int result) {
            if (result)
            {
                NCL_PROFILE_VALUE("Simulation delta ms", double(delta) * 1000.0);
            }
        };
#endif

        LOG(INFO) << "---------- Initialize | End ----------";

        is_initialized_ = true;

        return true;
    }

    bool NitroApi::UnInitialize()
    {
        if (!is_initialized_)
        {
            return true;
        }

        is_initialized_ = false;

        nitroapi::FrameLifecycle_ClearObservers();

        LOG(INFO) << "---------- UnInitialize | Begin ----------";

        for (auto& [path, module] : modules_ | std::views::reverse)
        {
            InvokeLibraryUnloading(path, module);
        }

        modules_.clear();
        hook_storage_.reset();

        LOG(INFO) << "---------- UnInitialize | End ----------";

        return true;
    }

    void NitroApi::InvokeLibraryLoaded(const std::string& module_path, ModuleHookData& module_hook_data)
    {
        nitro_utils::SysModule hModule;

        if (module_path == "<exe>")
        {
            hModule = nitro_utils::GetSysModule(nullptr);
        }
        else
        {
            hModule = nitro_utils::LoadSysModule(module_path.c_str());
        }

        if (hModule == nullptr)
        {
            LOG(INFO) << "Module loading " << module_path << " failed (nitro_utils::GetSysModule/nitro_utils::LoadSysModule returns null)";

            return;
        }

        LOG(INFO) << "Module loading " << module_path << " | Begin";

        module_hook_data.module_handle = hModule;
        module_hook_data.module->OnLibraryLoaded(hModule);

        LOG(INFO) << "Module loading " << module_path << " | End";
    }

    void NitroApi::InvokeLibraryUnloading(const std::string& module_path, ModuleHookData& module_hook_data)
    {
        LOG(INFO) << "Module unloading " << module_path << " | Begin";

        module_hook_data.module->OnLibraryUnload();

        if (module_path != "<exe>" && !module_hook_data.borrowed)
        {
            nitro_utils::UnloadSysModule(module_hook_data.module_handle);
        }

        module_hook_data.module_handle = nullptr;

        LOG(INFO) << "Module unloading " << module_path << " | End";
    }

    void NitroApi::GetVersion(char* buffer, int size)
    {
        if (buffer != nullptr)
        {
            V_strncpy(buffer, NITROAPI_INTERFACE_VERSION ", " __DATE__ " " __TIME__, size);
        }
    }

    void NitroApi::DetectBuildIdentity()
    {
        const std::string platform = IsWindows() ? "windows" : "linux";
        const std::string engine_path = IsWindows() ? "hw.dll" : "hw.so";
        const std::string client_path = IsWindows() ? "cstrike\\cl_dlls\\client.dll" : "cstrike/cl_dlls/client.so";

        const std::string engine_sha256 = HashFileSha256(engine_path);

        if (engine_sha256.empty())
        {
            LOG(INFO) << "Can't read " << engine_path << " to identify the game build";

            return;
        }

        const std::string client_sha256 = HashFileSha256(client_path);

        if (client_sha256.empty())
        {
            LOG(INFO) << "Can't read " << client_path << " to identify the game build";

            return;
        }

        std::optional<BuildVersion> identity = IdentifyBuild({platform, engine_sha256, client_sha256});

        if (!identity.has_value())
        {
            build_version_ = BuildVersion{};
            build_version_.engine_sha256 = engine_sha256;
            build_version_.client_sha256 = client_sha256;

            LOG(INFO) << "Unknown game build: " << engine_path << " sha256=" << engine_sha256
                      << ", " << client_path << " sha256=" << client_sha256;

            return;
        }

        build_version_ = std::move(identity.value());

        LOG(INFO) << "Game build identified: " << build_version_.id
                  << " (Steam build " << build_version_.build_number << ")";
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetEngineAddressProvider()
    {
        if (!build_version_.IsSupported())
        {
            LOG(INFO) << "No address provider for an unidentified engine build; engine hooks are disabled";

            return nullptr;
        }

        // The Default Public provider is built on the three-stage resolver in
        // G07b and filled with Public offsets in G07c. Until then no legacy
        // provider may stand in for it.
        LOG(INFO) << "No address provider implemented for build " << build_version_.id;

        return nullptr;
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetClientAddressProvider()
    {
        if (!build_version_.IsSupported())
        {
            LOG(INFO) << "No address provider for an unidentified client build; client hooks are disabled";

            return nullptr;
        }

        LOG(INFO) << "No address provider implemented for build " << build_version_.id;

        return nullptr;
    }

#ifdef _WIN32
    LONG WINAPI GlobalExceptionHandlerWin32(EXCEPTION_POINTERS* exception_pointers)
    {
        if (g_WriteMiniDumpCallback != nullptr)
        {
            g_WriteMiniDumpCallback(exception_pointers);
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif
} // namespace nitroapi
