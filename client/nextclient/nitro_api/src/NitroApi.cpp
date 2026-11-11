#include "NitroApi.h"

#include <ranges>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <easylogging++.h>

#include <ncl_perf/profiler.h>
#include <nitroapi/handler_helper/Handler.h>

#include "FrameLifecycle.h"
#include "modules/windows/WindowsModule.h"
#include "modules/engine/EngineAddressProviderXash.h"
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
            // Xash3D is this process. The retail engine file is not a module here.
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
            modules_.emplace_back("<xash>", std::move(engine_module));
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
        if (module_path == "<xash>")
        {
            // Do not open, hash, or hook hw.so. Engine calls stay unset until an
            // Xash adapter supplies them. Missing addresses are not hooks.
            LOG(INFO) << "Xash3D engine is this process; hw.so is not opened";

            return;
        }

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

        if (module_path != "<exe>" && module_path != "<xash>" && !module_hook_data.borrowed
            && module_hook_data.module_handle != nullptr)
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
        // The product engine is Xash3D. Retail hw.so and client.so are not read.
        build_version_ = BuildVersion{};
        build_version_.id = "xash";
        build_version_.family = BuildFamily::Xash;

        LOG(INFO) << "Game build identified: xash (Xash3D)";
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetEngineAddressProvider()
    {
        if (build_version_.family == BuildFamily::Xash)
            return std::make_shared<EngineAddressProviderXash>();

        LOG(INFO) << "No address provider for this engine build; engine hooks are disabled";

        return nullptr;
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetClientAddressProvider()
    {
        // Valve's client.so is i386 and is not a module of this process.
        LOG(INFO) << "No client address provider in the Xash process; client hooks are disabled";

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
