#pragma once

#include <memory>
#include <string>
#include <goldsrc/client_api.h>

namespace csr::client
{
    class NativeGameUiSystem
    {
    public:
        struct Host
        {
            goldsrc::cl_enginefunc_t* engine{};
            void* filesystem{};
            bool (*remove_command)(const char*, void*){};
            void* context{};
            void (*clear_matches)(void*, bool){};
            std::string base_directory;
            int product_version{};
            void (*set_title)(const char*, void*){};
            void (*show_console)(bool, void*){};
            void (*error)(const char*, void*){};
        };

        NativeGameUiSystem();
        ~NativeGameUiSystem();
        NativeGameUiSystem(const NativeGameUiSystem&) = delete;
        NativeGameUiSystem& operator=(const NativeGameUiSystem&) = delete;
        bool Bind(const Host& host, std::string& error);
        void* Interface();
        void Frame(double delta);
        void Shutdown();
        void EndNativeLifetime() noexcept;
        bool IsReady() const;
        // Keep this owner, its host and module code alive until removal succeeds.
        bool HasPendingCommands() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace csr::client
