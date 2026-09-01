#include "NitroApi.h"

#include <ranges>
#include <filesystem>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <easylogging++.h>

#include "nitroapi/handler_helper/Handler.h"
#include "modules/windows/WindowsModule.h"

#ifdef NITROAPI_USE_PROFILER
#include "profiler.h"
bool g_ProfilerInsideFrame = false;
#endif

#ifndef NITRO_API_STATIC
INITIALIZE_EASYLOGGINGPP
#endif

namespace nitroapi
{
    static nitroapi::NitroApi g_NitroApi;
    NitroApiInterface* GetNitroApi() { return &g_NitroApi; }

    static GlobalExceptionHandlerFunc g_WriteMiniDumpCallback = nullptr;

#ifndef NITRO_API_STATIC
    EXPOSE_SINGLE_INTERFACE_GLOBALVAR(NitroApi, NitroApiInterface, NITROAPI_INTERFACE_VERSION, g_NitroApi);
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

    bool NitroApi::Initialize(ICommandLine *command_line, IFileSystem* file_system, IRegistry *registry)
    {
        if (is_initialized_)
            return true;

        LOG(INFO) << "---------- Initialize | Begin ----------";

        hook_storage_ = std::make_shared<HookStorage>();

        auto windows_module = std::make_unique<WindowsModule>(hook_storage_, nullptr);
        windows_data_ = windows_module->GetWindowsData();
        modules_.emplace_back("<exe>", std::move(windows_module));

        RetrieveEngineBuildVersion();

        // Phase 2: kein Steam-hw.dll-/client.dll-/sdl2.dll-Hook. Provider bleiben nullptr.
        if (GetEngineAddressProvider() == nullptr)
            LOG(INFO) << "No engine address provider (Steam/8684 cut); engine hooks disabled";
        if (GetClientAddressProvider() == nullptr)
            LOG(INFO) << "No client address provider (Steam/8684 cut); client hooks disabled";

        for (auto& [path, module]: modules_)
            InvokeLibraryLoaded(path, module);

#ifdef NITROAPI_USE_PROFILER
        SetupProfiler();
#endif

        LOG(INFO) << "---------- Initialize | End ----------";

        is_initialized_ = true;
        return true;
    }

    bool NitroApi::UnInitialize()
    {
        if (!is_initialized_)
            return true;

        is_initialized_ = false;

        LOG(INFO) << "---------- UnInitialize | Begin ----------";

        for (auto& [path, module]: modules_ | std::views::reverse)
            InvokeLibraryUnloading(path, module);

        modules_.clear();
        hook_storage_.reset();

        LOG(INFO) << "---------- UnInitialize | End ----------";

        return true;
    }

    void NitroApi::InvokeLibraryLoaded(const std::string& module_path, ModuleHookData& module_hook_data)
    {
        nitro_utils::SysModule hModule;

        if (module_path == "<exe>")
            hModule = nitro_utils::GetSysModule(nullptr);
        else
            hModule = nitro_utils::LoadSysModule(module_path.c_str());

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

        if (module_path != "<exe>")
            nitro_utils::UnloadSysModule(module_hook_data.module_handle);

        module_hook_data.module_handle = nullptr;

        LOG(INFO) << "Module unloading " << module_path << " | End";
    }

    void NitroApi::GetVersion(char* buffer, int size)
    {
        if (buffer != nullptr)
            V_strncpy(buffer, NITROAPI_INTERFACE_VERSION ", " __DATE__ " " __TIME__, size);
    }

#ifdef NITROAPI_USE_PROFILER
    void NitroApi::SetupProfiler()
    {
        static bool filter_result;

        engine_data_->Host_FilterTime += [](float delta, int result) {
            filter_result = result;

            if (!filter_result)
                return;

            g_ProfilerInsideFrame = true;

            OPTICK_FRAME("Frame");
            OPTICK_TAG("Delta", std::to_string(delta * 1000).c_str());

            OPTICK_EVENT("Host_FrameInternal");
        };

        engine_data_->Host_FrameInternal += [](float delta) {
            if (!filter_result)
                return;

            g_ProfilerInsideFrame = false;

            ::Optick::EndFrame();
			::Optick::Update();
        };
    }
#endif

    void NitroApi::RetrieveEngineBuildVersion()
    {
        // Phase 2: Steam-hw.dll-Größe 8684 wird nicht mehr erkannt.
        LOG(INFO) << "Steam engine build detection disabled";
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetEngineAddressProvider()
    {
        return nullptr;
    }

    std::shared_ptr<AddressProviderBase> NitroApi::GetClientAddressProvider()
    {
        return nullptr;
    }

#ifdef _WIN32
    LONG WINAPI GlobalExceptionHandlerWin32(EXCEPTION_POINTERS* exception_pointers)
    {
        if (g_WriteMiniDumpCallback != nullptr)
            g_WriteMiniDumpCallback(exception_pointers);

        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif
}