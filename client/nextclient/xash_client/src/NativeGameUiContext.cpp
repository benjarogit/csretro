#include "NativeGameUiContext.h"
#include "NativeGameUiSystem.h"
#include <exception>
#include <stdexcept>
#include <utility>

namespace csr::client
{
    struct NativeGameUiContext::State
    {
        Host host;
        // Reverse destruction keeps the shadow table alive through every consumer.
        NativeEngineCommands commands;
        NativeGameUiSystem system;
        NativeSdlInput input;
        NativeGameUiRuntime runtime;
        unsigned active{};
        bool bound{}, commands_bound{}, closing{}, stopping{}, sealed{}, uncertain{};
        std::string error;

        struct Call
        {
            State& state;
            explicit Call(State& value) :
                state(value)
            {
                ++state.active;
            }
            ~Call()
            {
                --state.active;
            }
        };
        bool Open() const
        {
            return bound && !closing && !sealed && !runtime.ReleaseRequested() && !input.ReleaseRequested();
        }
        bool Requested() const
        {
            return closing || runtime.ReleaseRequested() || input.ReleaseRequested();
        }
        void Failure(const char* text)
        {
            closing = true;
            if (error.empty())
            {
                error = text;
            }
        }
        template <class F>
        void Invoke(F&& action)
        {
            if (!Open())
            {
                return;
            }
            Call call(*this);
            try
            {
                action();
            }
            catch (const std::exception& exception)
            {
                Failure(exception.what());
            }
            catch (...)
            {
                Failure("native UI context callback failed");
            }
        }
        template <class F>
        bool Startup(F&& action, std::string& result)
        {
            if (!Open() || active)
            {
                result = "native UI context startup is unavailable or reentrant";
                return false;
            }
            Call call(*this);
            try
            {
                if (action(result) && !Requested())
                {
                    return true;
                }
                Failure(result.empty() ? "native UI context startup requested shutdown" : result.c_str());
            }
            catch (const std::exception& exception)
            {
                Failure(exception.what());
            }
            catch (...)
            {
                Failure("native UI context startup failed");
            }
            result = error;
            return false;
        }
        static bool Remove(const char* name, void* context)
        {
            return static_cast<State*>(context)->commands.Remove(name);
        }
        static void Title(const char* text, void* context)
        {
            auto& state = *static_cast<State*>(context);
            if (!state.sealed && state.host.set_title)
            {
                state.host.set_title(text, state.host.platform_context);
            }
        }
        static void Error(const char* text, void* context)
        {
            auto& state = *static_cast<State*>(context);
            if (!state.sealed && state.host.error)
            {
                state.host.error(text, state.host.platform_context);
            }
        }
        static void Console(bool visible, void* context)
        {
            auto& state = *static_cast<State*>(context);
            state.Invoke([&] { visible ? state.runtime.ShowConsole() : state.runtime.HideConsole(); });
        }
        bool Safe() const
        {
            if (sealed)
            {
                return true;
            }
            return !active && !stopping && !uncertain && (!commands_bound || !commands.HasModuleCallbacks(host.gameui)) &&
                   !input.HasPendingWatch() && !system.HasPendingCommands() && !commands.HasPendingCommands() && runtime.CanUnload();
        }
    };

    NativeGameUiContext::NativeGameUiContext() = default;
    NativeGameUiContext::~NativeGameUiContext() noexcept
    {
        if (!state_)
        {
            return;
        }
        try
        {
            Shutdown();
        }
        catch (...)
        {
            state_->uncertain = true;
        }
        if (!CanUnload())
        {
            // Native command/watch/panel callbacks may still reference this storage.
            state_.release();
        }
    }
    bool NativeGameUiContext::Bind(const Host& host, std::string& error)
    {
        if (state_)
        {
            error = "native UI context already has a lifetime";
            return false;
        }
        if (!host.client || !host.gameui || !host.vgui || !host.filesystem || !host.engine || !host.panels || !host.commands ||
            !host.invoke || !host.clear_matches || host.base_directory.empty() || !host.product_version)
        {
            error = "native UI context requires complete borrowed services";
            return false;
        }
        state_ = std::make_unique<State>();
        auto& state = *state_;
        state.host = host;
        State::Call call(state);
        try
        {
            if (!state.commands.Bind(host.engine, host.commands, host.invoke, host.invoke_context, error))
            {
                throw std::runtime_error(error);
            }
            state.commands_bound = true;
            NativeGameUiSystem::Host system;
            system.engine = state.commands.Engine();
            system.filesystem = host.filesystem;
            system.remove_command = State::Remove;
            system.context = &state;
            system.clear_matches = host.clear_matches;
            system.base_directory = host.base_directory;
            system.product_version = host.product_version;
            system.set_title = State::Title;
            system.show_console = State::Console;
            system.error = State::Error;
            if (!state.system.Bind(system, error))
            {
                throw std::runtime_error(error);
            }
            if (!state.input.Bind(host.client, host.vgui, host.input_hooks, error))
            {
                throw std::runtime_error(error);
            }
            NativeGameUiRuntime::Host runtime;
            runtime.gameui = host.gameui;
            runtime.vgui = host.vgui;
            runtime.engine = state.commands.Engine();
            runtime.system = state.system.Interface();
            runtime.panels = host.panels;
            runtime.hooks = host.ui_hooks;
            if (!state.runtime.Bind(runtime, error))
            {
                throw std::runtime_error(error);
            }
            if (state.closing)
            {
                throw std::runtime_error("native UI context bind was interrupted");
            }
            state.bound = true;
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            state.Failure(exception.what());
        }
        catch (...)
        {
            state.Failure("native UI context binding failed");
        }
        error = state.error;
        // The host invokes Shutdown after this call returns outside the active scope.
        return false;
    }
    bool NativeGameUiContext::Initialize(NativeGameUiRuntime::Factory* factories, int count, std::string& error)
    {
        if (!state_)
        {
            error = "native UI context is not bound";
            return false;
        }
        return state_->Startup([&](std::string& result) { return state_->runtime.Initialize(factories, count, result); }, error);
    }
    bool NativeGameUiContext::Start(std::string& error)
    {
        if (!state_)
        {
            error = "native UI context is not bound";
            return false;
        }
        return state_->Startup([&](std::string& result) { return state_->runtime.Start(result); }, error);
    }
    bool NativeGameUiContext::InitializeConsole(std::string& error)
    {
        if (!state_)
        {
            error = "native UI context is not bound";
            return false;
        }
        return state_->Startup([&](std::string& result) { return state_->runtime.InitializeConsole(result); }, error);
    }
    void NativeGameUiContext::Frame(double delta)
    {
        if (!state_)
        {
            return;
        }
        auto& state = *state_;
        if (state.active)
        {
            return;
        }
        state.Invoke([&] {
            state.input.Drain();
            if (state.Requested())
            {
                return;
            }
            state.system.Frame(delta);
            if (state.Requested())
            {
                return;
            }
            state.runtime.Frame();
        });
    }
    void NativeGameUiContext::Activate()
    {
        if (state_)
        {
            state_->Invoke([&] { state_->runtime.Activate(); });
        }
    }
    void NativeGameUiContext::Hide()
    {
        if (state_)
        {
            state_->Invoke([&] { state_->runtime.Hide(); });
        }
    }
    void NativeGameUiContext::ShowConsole()
    {
        if (state_)
        {
            state_->Invoke([&] { state_->runtime.ShowConsole(); });
        }
    }
    void NativeGameUiContext::HideConsole()
    {
        if (state_)
        {
            state_->Invoke([&] { state_->runtime.HideConsole(); });
        }
    }
    void NativeGameUiContext::ClearInput()
    {
        if (state_)
        {
            state_->Invoke([&] { state_->input.Clear(); });
        }
    }
    void NativeGameUiContext::HandleEvent(const void* event)
    {
        if (state_)
        {
            state_->Invoke([&] { state_->input.HandleEvent(event); });
        }
    }
    void NativeGameUiContext::Shutdown()
    {
        if (!state_ || state_->sealed)
        {
            return;
        }
        auto& state = *state_;
        state.closing = true;
        if (state.active || state.stopping)
        {
            return;
        }
        state.stopping = true;
        auto attempt = [&](auto&& action) {
            try
            {
                action();
            }
            catch (const std::exception& exception)
            {
                state.uncertain = true;
                state.Failure(exception.what());
            }
            catch (...)
            {
                state.uncertain = true;
                state.Failure("native UI context cleanup failed");
            }
        };
        attempt([&] { state.input.Shutdown(); });
        attempt([&] { state.commands.Shutdown(); });
        attempt([&] { state.runtime.Shutdown(); });
        attempt([&] { state.system.Shutdown(); });
        attempt([&] { state.commands.Shutdown(); });
        attempt([&] {
            std::string error;
            if (!state.runtime.RetireControls(error) && state.error.empty())
            {
                state.error = std::move(error);
            }
        });
        state.stopping = false;
    }
    void NativeGameUiContext::EndNativeLifetime() noexcept
    {
        if (!state_ || state_->sealed)
        {
            return;
        }
        auto& state = *state_;
        if (!state.closing || !state.Safe())
        {
            return;
        }
        state.sealed = true;
        state.runtime.EndNativeLifetime();
        state.input.EndNativeLifetime();
        state.system.EndNativeLifetime();
    }
    bool NativeGameUiContext::CanUnload() const
    {
        return !state_ || (state_->closing && state_->Safe());
    }
    bool NativeGameUiContext::IsReady() const
    {
        return state_ && state_->Open() && !state_->Requested() && state_->runtime.IsReady() && state_->system.IsReady() &&
               state_->input.IsReady();
    }
    bool NativeGameUiContext::ReleaseRequested() const
    {
        return state_ && state_->Requested();
    }
    bool NativeGameUiContext::IsActive() const
    {
        bool result{};
        if (state_)
        {
            state_->Invoke([&] { result = state_->runtime.IsActive(); });
        }
        return result;
    }
    bool NativeGameUiContext::IsConsoleVisible() const
    {
        bool result{};
        if (state_)
        {
            state_->Invoke([&] { result = state_->runtime.IsConsoleVisible(); });
        }
        return result;
    }
    bool NativeGameUiContext::HasExclusiveInput() const
    {
        bool result{};
        if (state_)
        {
            state_->Invoke([&] { result = state_->runtime.HasExclusiveInput(); });
        }
        return result;
    }
    void* NativeGameUiContext::BaseUiInterface()
    {
        return state_ && state_->Open() ? state_->runtime.BaseUiInterface() : nullptr;
    }
    NativeGameUiRuntime::Factory NativeGameUiContext::GameFactory() const
    {
        return state_ && state_->Open() ? state_->runtime.GameFactory() : nullptr;
    }
    goldsrc::cl_enginefunc_t* NativeGameUiContext::Engine()
    {
        return state_ && !state_->sealed ? state_->commands.Engine() : nullptr;
    }
    const std::string& NativeGameUiContext::LastError() const
    {
        static const std::string empty;
        if (!state_)
        {
            return empty;
        }
        if (!state_->error.empty())
        {
            return state_->error;
        }
        if (state_->runtime.ReleaseRequested())
        {
            return state_->runtime.LastError();
        }
        return state_->commands.LastError();
    }
} // namespace csr::client
