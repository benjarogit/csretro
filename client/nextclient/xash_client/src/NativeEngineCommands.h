#pragma once

#include <memory>
#include <string>

#include <goldsrc/client_api.h>
#include "XashCommandAccess.h"

namespace csr::client
{
    // One engine-thread owner for commands registered through the GameUI table.
    // The authenticated engine/query owner must outlive this lease.
    class NativeEngineCommands
    {
    private:
        struct State;
        std::unique_ptr<State> state_;
        static State* active_;
        static int Register(const char* name, void (*callback)());
        static void Forward();

    public:
        NativeEngineCommands();
        ~NativeEngineCommands();
        NativeEngineCommands(const NativeEngineCommands&) = delete;
        NativeEngineCommands& operator=(const NativeEngineCommands&) = delete;

        bool Bind(
            goldsrc::cl_enginefunc_t* engine,
            nitroapi::XashCommandAccess* access,
            void (*invoke)(void (*)(), void*),
            void* context,
            std::string& error
        );
        goldsrc::cl_enginefunc_t* Engine();
        bool Remove(const char* name);
        bool Shutdown();
        bool Release() noexcept;
        bool HasPendingCommands() const;
        bool HasModuleCallbacks(void* module) const;
        const std::string& LastError() const;
    };
} // namespace csr::client
