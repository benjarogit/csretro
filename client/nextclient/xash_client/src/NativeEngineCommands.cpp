#include "NativeEngineCommands.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <dlfcn.h>
#include <link.h>
#include <map>
#include <stdexcept>
#include <vector>

namespace csr::client
{
    namespace
    {
        std::string Key(const char* name)
        {
            std::string result(name);
            for (char& value : result)
            {
                if (value >= 'A' && value <= 'Z')
                {
                    value += 'a' - 'A';
                }
            }
            return result;
        }
        bool Name(const char* name)
        {
            if (!name || !*name || strnlen(name, 128) == 128)
            {
                return false;
            }
            for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p)
            {
                if (*p <= 32 || *p >= 127 || *p == ';' || *p == '"' || *p == '\\')
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    struct NativeEngineCommands::State
    {
        struct Command
        {
            nitroapi::XashCommandAccess::View view;
            void (*target)(){};
            bool removed{};
        };
        goldsrc::cl_enginefunc_t original{};
        goldsrc::cl_enginefunc_t shadow{};
        nitroapi::XashCommandAccess* access{};
        std::map<std::string, Command> commands;
        bool accepting{};
        bool registering{};
        void (*invoke)(void (*)(), void*){};
        void* context{};
        std::string error;

        bool Remove(const char* name)
        {
            auto found = commands.find(Key(name));
            if (found == commands.end())
            {
                return false;
            }
            if (found->second.removed)
            {
                return true;
            }
            nitroapi::XashCommandAccess::View current{};
            if (!access->Inspect(name, current))
            {
                error = "Cannot inspect a registered native command";
                return false;
            }
            if (!current.identity || current.function != found->second.view.function)
            {
                // The owned callback is gone. A replacement belongs to its
                // actual registrar and must never be removed by this lease.
                found->second.removed = true;
                return true;
            }
            if (current.identity != found->second.view.identity)
            {
                error = "A different native command record still references the owned callback";
                return false;
            }
            access->Remove(name);
            nitroapi::XashCommandAccess::View after{};
            if (!access->Inspect(name, after) || (after.identity && after.function == found->second.view.function))
            {
                error = "Native command removal did not retire the owned callback";
                return false;
            }
            found->second.removed = true;
            return true;
        }
    };

    NativeEngineCommands::State* NativeEngineCommands::active_{};
    NativeEngineCommands::NativeEngineCommands() = default;

    NativeEngineCommands::~NativeEngineCommands()
    {
        bool complete{};
        try
        {
            complete = Shutdown();
        }
        catch (...)
        {}
        if (!complete && state_)
        {
            std::fprintf(stderr, "CSRETRO retained native command state after incomplete removal\n");
            static_cast<void>(state_.release());
            return;
        }
        Release();
    }

    bool NativeEngineCommands::Bind(
        goldsrc::cl_enginefunc_t* engine,
        nitroapi::XashCommandAccess* access,
        void (*invoke)(void (*)(), void*),
        void* context,
        std::string& error
    )
    {
        error.clear();
        if (state_ || active_ || !engine || !access || !access->IsBound() || !invoke || !engine->Cmd_Argv || !engine->pfnAddCommand ||
            !engine->GetFirstCmdFunctionHandle || !engine->GetNextCmdFunctionHandle || !engine->GetCmdFunctionName)
        {
            error = "Native commands require an exclusive live engine and authenticated command access";
            return false;
        }
        state_ = std::make_unique<State>();
        state_->original = *engine;
        state_->shadow = *engine;
        state_->shadow.pfnAddCommand = Register;
        state_->access = access;
        state_->accepting = true;
        state_->invoke = invoke;
        state_->context = context;
        active_ = state_.get();
        return true;
    }

    int NativeEngineCommands::Register(const char* name, void (*callback)())
    {
        State* owner = active_;
        if (!owner || !owner->accepting || owner->registering || !Name(name) || !callback)
        {
            return 0;
        }
        owner->registering = true;
        struct EndRegistration
        {
            State& owner;
            ~EndRegistration()
            {
                owner.registering = false;
            }
        } registration{*owner};
        try
        {
            nitroapi::XashCommandAccess::View existing{};
            if (!owner->access->Inspect(name, existing) || existing.identity)
            {
                return 0;
            }
            // Allocate the ownership record before crossing into engine code.
            auto [position, inserted] = owner->commands.try_emplace(Key(name));
            if (!inserted && !position->second.removed)
            {
                return 0;
            }
            position->second = {};
            position->second.target = callback;
            int accepted{};
            try
            {
                accepted = owner->original.pfnAddCommand(name, Forward);
            }
            catch (...)
            {
                owner->error = "Engine command registration threw";
            }
            nitroapi::XashCommandAccess::View current{};
            if (!owner->access->Inspect(name, current))
            {
                // Registration may have succeeded. Keep an unresolved record;
                // no module may unload until an actual query can retire it.
                position->second.view.function = Forward;
                owner->error = "Cannot verify native command registration";
                return 0;
            }
            if (!current.identity || current.function != Forward)
            {
                position->second.removed = true;
                return 0;
            }
            position->second.view = current;
            if (!accepted || !owner->accepting)
            {
                owner->Remove(name);
                return 0;
            }
            return 1;
        }
        catch (...)
        {
            owner->error = "Native command registration failed";
            return 0;
        }
    }

    void NativeEngineCommands::Forward()
    {
        State* owner = active_;
        if (!owner || !owner->accepting)
        {
            return;
        }
        try
        {
            const char* name = owner->original.Cmd_Argv(0);
            if (!Name(name))
            {
                return;
            }
            const auto found = owner->commands.find(Key(name));
            if (found == owner->commands.end() || found->second.removed || !found->second.target)
            {
                return;
            }
            void (*target)() = found->second.target;
            void (*invoke)(void (*)(), void*) = owner->invoke;
            void* context = owner->context;
            // The outer native-call guard may finish teardown and release
            // this state when the target returns. Do not touch owner afterwards.
            invoke(target, context);
        }
        catch (...)
        {
            // Engine command callbacks are a C boundary. Their guarded owner
            // reports failures; exceptions must not escape into the engine.
            std::fprintf(stderr, "CSRETRO guarded native command failed\n");
        }
    }

    goldsrc::cl_enginefunc_t* NativeEngineCommands::Engine()
    {
        return state_ ? &state_->shadow : nullptr;
    }

    bool NativeEngineCommands::Remove(const char* name)
    {
        if (!state_ || state_->registering || !Name(name))
        {
            return false;
        }
        try
        {
            return state_->Remove(name);
        }
        catch (...)
        {
            state_->error = "Native command removal threw";
            return false;
        }
    }

    bool NativeEngineCommands::Shutdown()
    {
        if (!state_)
        {
            return true;
        }
        state_->accepting = false;
        if (state_->registering)
        {
            return false;
        }
        bool complete = true;
        for (const auto& command : state_->commands)
        {
            complete = Remove(command.first.c_str()) && complete;
        }
        return complete;
    }

    bool NativeEngineCommands::Release() noexcept
    {
        if (HasPendingCommands() || (state_ && state_->accepting))
        {
            return false;
        }
        if (state_ && active_ == state_.get())
        {
            active_ = nullptr;
        }
        state_.reset();
        return true;
    }

    bool NativeEngineCommands::HasPendingCommands() const
    {
        if (!state_)
        {
            return false;
        }
        if (state_->registering)
        {
            return true;
        }
        return std::any_of(state_->commands.begin(), state_->commands.end(), [](const auto& item) { return !item.second.removed; });
    }

    bool NativeEngineCommands::HasModuleCallbacks(void* module) const
    {
        if (!state_ || !module)
        {
            return true;
        }
        link_map* map{};
        if (dlinfo(module, RTLD_DI_LINKMAP, &map) || !map)
        {
            return true;
        }
        try
        {
            for (const auto& item : state_->commands)
            {
                if (item.second.removed || !item.second.target)
                {
                    continue;
                }
                Dl_info target_owner{};
                if (!dladdr(reinterpret_cast<void*>(item.second.target), &target_owner) ||
                    target_owner.dli_fbase == reinterpret_cast<void*>(map->l_addr))
                {
                    return true;
                }
            }
            std::vector<unsigned> visited;
            for (unsigned token = state_->original.GetFirstCmdFunctionHandle(); token;
                 token = state_->original.GetNextCmdFunctionHandle(token))
            {
                if (visited.size() >= 8192 || std::find(visited.begin(), visited.end(), token) != visited.end())
                {
                    return true;
                }
                visited.push_back(token);
                const char* name = state_->original.GetCmdFunctionName(token);
                nitroapi::XashCommandAccess::View command{};
                if (!Name(name) || !state_->access->Inspect(name, command) || !command.identity)
                {
                    return true;
                }
                if (!command.function)
                {
                    continue;
                }
                Dl_info owner{};
                if (!dladdr(reinterpret_cast<void*>(command.function), &owner) || owner.dli_fbase == reinterpret_cast<void*>(map->l_addr))
                {
                    return true;
                }
            }
        }
        catch (...)
        {
            return true;
        }
        return false;
    }

    const std::string& NativeEngineCommands::LastError() const
    {
        static const std::string empty;
        return state_ ? state_->error : empty;
    }
} // namespace csr::client
