#pragma once

#include <goldsrc/client_api.h>
#include <cstdint>
#include <memory>
#include <string>

namespace csr::client
{
    class NativePanelTree;
    class NativeGameUiRuntime
    {
    public:
        using Factory = void* (*)(const char*, int*);
        struct Hooks
        {
            int (*key_handler)(void*, int, int, const char*){};
            void (*event_handler)(void*, void*, void*){};
            void (*paint)(void*, int, int, int, int){};
            void (*reset_input)(void*){};
            void (*set_visibility)(void*, int, bool){};
            int (*popup_count)(void*){};
            uint32_t (*get_popup)(void*, int){};
            void* context{};
        };
        struct Host
        {
            void* gameui{};
            void* vgui{};
            goldsrc::cl_enginefunc_t* engine{};
            void* system{};
            NativePanelTree* panels{};
            Hooks hooks;
        };
        NativeGameUiRuntime();
        ~NativeGameUiRuntime() noexcept;
        NativeGameUiRuntime(const NativeGameUiRuntime&) = delete;
        NativeGameUiRuntime& operator=(const NativeGameUiRuntime&) = delete;
        bool Bind(Host& host, std::string& error);
        void* BaseUiInterface();
        Factory GameFactory() const;
        bool IsActive() const;
        bool IsConsoleVisible() const;
        bool HasExclusiveInput() const;
        bool Initialize(Factory* factories, int count, std::string& error);
        bool Start(std::string& error);
        bool InitializeConsole(std::string& error);
        void Frame();
        void Activate();
        void Hide();
        void ShowConsole();
        void HideConsole();
        void Shutdown();
        bool RetireControls(std::string& error);
        void EndNativeLifetime() noexcept;
        bool IsReady() const;
        bool CanUnload() const;
        bool ReleaseRequested() const;
        const std::string& LastError() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };
} // namespace csr::client
