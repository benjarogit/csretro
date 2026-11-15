#pragma once

#include "NativeEngineCommands.h"
#include "NativeGameUiRuntime.h"
#include "NativeSdlInput.h"
#include <memory>
#include <string>

namespace csr::client
{
    // All operations run synchronously inside the host's native UI lifetime guard.
    // The borrowed modules, panel tree and command access outlive this owner.
    class NativeGameUiContext
    {
    public:
        struct Host
        {
            void* client{};
            void* gameui{};
            void* vgui{};
            void* filesystem{};
            goldsrc::cl_enginefunc_t* engine{};
            NativePanelTree* panels{};
            nitroapi::XashCommandAccess* commands{};
            NativeGameUiRuntime::Hooks ui_hooks;
            NativeSdlInput::Hooks input_hooks;
            void (*clear_matches)(void*, bool){};
            std::string base_directory;
            int product_version{};
            void (*invoke)(void (*)(), void*){};
            void* invoke_context{};
            void (*set_title)(const char*, void*){};
            void (*error)(const char*, void*){};
            void* platform_context{};
        };

        NativeGameUiContext();
        ~NativeGameUiContext() noexcept;
        NativeGameUiContext(const NativeGameUiContext&) = delete;
        NativeGameUiContext& operator=(const NativeGameUiContext&) = delete;
        bool Bind(const Host&, std::string& error);
        bool Initialize(NativeGameUiRuntime::Factory*, int count, std::string& error);
        bool Start(std::string& error);
        bool InitializeConsole(std::string& error);
        void Frame(double delta);
        void Activate();
        void Hide();
        void ShowConsole();
        void HideConsole();
        void HandleEvent(const void* event);
        void ClearInput();
        void Shutdown();
        void EndNativeLifetime() noexcept;
        bool CanUnload() const;
        bool IsReady() const;
        bool ReleaseRequested() const;
        bool IsActive() const;
        bool IsConsoleVisible() const;
        bool HasExclusiveInput() const;
        void* BaseUiInterface();
        NativeGameUiRuntime::Factory GameFactory() const;
        goldsrc::cl_enginefunc_t* Engine();
        const std::string& LastError() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace csr::client
