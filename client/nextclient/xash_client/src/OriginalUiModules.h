#pragma once

#include <memory>
#include <string>
#include <goldsrc/client_api.h>

namespace nitroapi
{
    class XashCommandAccess;
}
namespace csr::client
{
    // Retains the exact modules used by original Initialize; Reset follows all UI cleanup.
    class OriginalUiModules
    {
    public:
#ifdef CSRETRO_NATIVE_MODULE_TESTS
        struct FingerprintChecks
        {
            bool (*gameui)(const std::string&, const std::string&){};
            bool (*filesystem)(const std::string&, const std::string&){};
        };
        explicit OriginalUiModules(const FingerprintChecks& checks);
#endif
        OriginalUiModules();
        ~OriginalUiModules();
        OriginalUiModules(const OriginalUiModules&) = delete;
        OriginalUiModules& operator=(const OriginalUiModules&) = delete;
        bool Preflight(
            goldsrc::cl_enginefunc_t* engine,
            const std::string& client_hash,
            const std::string& engine_hash,
            nitroapi::XashCommandAccess& commands,
            std::string& error
        );
        bool Capture(void* client, std::string& error);
        void* GameUiModule() const;
        void* FileSystemModule() const;
        void* FileSystemInterface() const;
        void* GameUiInterface() const;
        void* GameConsoleInterface() const;
        const std::string& BaseDirectory() const;
        void Reset();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace csr::client
