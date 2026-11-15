#include "NativeGameUiRuntime.h"
#include "NativePanelTree.h"
#include "elf_symbols.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <set>
#include <stdexcept>
#include <vector>

namespace csr::client
{
    struct NativeGameUiRuntime::State
    {
        NativeGameUiRuntime& owner;
        Host host;
        void* game{};
        void* console{};
        void* panels{};
        void* gui{};
        void** base{};
        Factory game_factory{};
        bool initialized{}, init_attempted{}, native_init_attempted{}, start_attempted{}, started{}, console_started{}, console_attempted{};
        bool shutdown_attempted{}, shutdown_success{}, retired{true}, uncertain{}, sealed{}, release{};
        unsigned active{};
        bool querying{};
        std::string error;
        std::string module_name;
        std::vector<uint32_t> owned;
        std::set<uint32_t> delete_attempted;
        std::set<uint32_t> observed;
        void (*initialize)(void*, Factory*, int){};
        void (*start)(void*, goldsrc::cl_enginefunc_t*, int, void*){};
        void (*shutdown)(void*){};
        int (*activate)(void*){};
        void (*hide)(void*){};
        void (*frame)(void*){};
        bool (*is_active)(void*){};
        int (*exclusive_input)(void*){};
        bool (*console_visible)(void*){};
        void (*console_initialize)(void*){};
        void (*console_activate)(void*){};
        void (*console_hide)(void*){};
        void (*console_parent)(void*, int){};
        const char* (*controls_name)(){};
        uint32_t (*get_vpanel)(void*){};
        uint32_t (*safe_handle)(void*, uint32_t){};
        uint32_t (*live_panel)(void*, uint32_t){};
        const char* (*panel_module)(void*, uint32_t){};
        int (*child_count)(void*, uint32_t){};
        uint32_t (*child)(void*, uint32_t, int){};
        uint32_t (*parent)(void*, uint32_t){};
        void (*set_parent)(void*, uint32_t, uint32_t){};
        void (*delete_panel)(void*, uint32_t){};
        void (*dispatch)(void*){};
        struct Guard
        {
            State& value;
            explicit Guard(State& state) :
                value(state)
            {
                ++value.active;
            }
            ~Guard()
            {
                --value.active;
            }
        };
        // Exact registered BaseUI001 prefix: two destructor entries, ten methods.
        struct BaseUi
        {
            State& state;
            explicit BaseUi(State& value) :
                state(value)
            {}
            virtual ~BaseUi() = default;
            virtual void Initialize(Factory* factories, int count)
            {
                if (!state.sealed)
                {
                    std::string error;
                    state.owner.Initialize(factories, count, error);
                }
            }
            virtual void Start(goldsrc::cl_enginefunc_t* engine, int version)
            {
                if (state.sealed)
                {
                    return;
                }
                if (engine != state.host.engine || version != 7)
                {
                    state.Fail("BaseUI engine table/version mismatch");
                    return;
                }
                std::string error;
                state.owner.Start(error);
            }
            virtual void Shutdown()
            {
                if (!state.sealed)
                {
                    state.owner.Shutdown();
                }
            }
            virtual int Key_Event(int down, int key, const char* binding)
            {
                if (state.sealed || !state.owner.IsReady())
                {
                    return 0;
                }
                Guard guard(state);
                try
                {
                    return state.host.hooks.key_handler(state.host.hooks.context, down, key, binding);
                }
                catch (...)
                {
                    state.Fail("Native BaseUI key handler failed");
                    return 0;
                }
            }
            virtual void CallEngineSurfaceAppHandler(void* event, void* user)
            {
                if (state.sealed || !state.owner.IsReady() || !event)
                {
                    return;
                }
                Guard guard(state);
                try
                {
                    state.host.hooks.event_handler(state.host.hooks.context, event, user);
                }
                catch (...)
                {
                    state.Fail("Native BaseUI event handler failed");
                }
            }
            virtual void Paint(int x, int y, int right, int bottom)
            {
                if (state.sealed || !state.owner.IsReady())
                {
                    return;
                }
                Guard guard(state);
                try
                {
                    state.host.hooks.paint(state.host.hooks.context, x, y, right, bottom);
                }
                catch (...)
                {
                    state.Fail("Native BaseUI paint handler failed");
                }
            }
            virtual void HideGameUI()
            {
                if (!state.sealed)
                {
                    state.owner.Hide();
                }
            }
            virtual void ActivateGameUI()
            {
                if (!state.sealed)
                {
                    state.owner.Activate();
                }
            }
            virtual void HideConsole()
            {
                if (!state.sealed)
                {
                    state.owner.HideConsole();
                }
            }
            virtual void ShowConsole()
            {
                if (!state.sealed)
                {
                    state.owner.ShowConsole();
                }
            }
        } base_ui{*this};
        State(NativeGameUiRuntime& runtime, Host& services) :
            owner(runtime),
            host(services)
        {}
        void Fail(const std::string& reason)
        {
            if (error.empty())
            {
                error = reason;
            }
            release = true;
        }
        template <class T>
        bool Query(T function, void* object, const char* reason)
        {
            if (!started || sealed || release || shutdown_attempted || querying)
            {
                return false;
            }
            struct QueryGuard
            {
                bool& flag;
                explicit QueryGuard(bool& value) :
                    flag(value)
                {
                    flag = true;
                }
                ~QueryGuard()
                {
                    flag = false;
                }
            } query_guard(querying);
            Guard guard(*this);
            try
            {
                const bool result = function(object) != 0;
                return !sealed && !release && result;
            }
            catch (...)
            {
                Fail(reason);
                return false;
            }
        }
        template <class T>
        bool Resolve(void* module, T& target, const char* name)
        {
            target = reinterpret_cast<T>(nitroapi::FindFunctionAddress(module, name));
            if (!target)
            {
                Fail(std::string("Missing verified native GameUI function: ") + name);
            }
            return target != nullptr;
        }
        bool Load()
        {
#define GET(module, member, symbol)       \
    if (!Resolve(module, member, symbol)) \
    {                                     \
        return false;                     \
    }
            GET(host.gameui, game_factory, "CreateInterface")
            game = game_factory("GameUI007", nullptr);
            console = game_factory("GameConsole003", nullptr);
            auto factory = reinterpret_cast<Factory>(nitroapi::FindFunctionAddress(host.vgui, "CreateInterface"));
            if (!factory || !game || !console)
            {
                Fail("Native GameUI/console/VGUI interfaces are missing");
                return false;
            }
            panels = factory("VGUI_Panel007", nullptr);
            gui = factory("VGUI_ivgui006", nullptr);
            if (!panels || !gui)
            {
                Fail("Native GameUI requires original panel/VGUI services");
                return false;
            }
            base = reinterpret_cast<void**>(nitroapi::FindSymbolAddress(host.gameui, "_ZL11staticPanel"));
            if (!base || *base)
            {
                Fail("Native GameUI requires a fresh uninitialized module");
                return false;
            }
            GET(host.gameui, initialize, "_ZN7CGameUI10InitializeEPPFPvPKcPiEi")
            GET(host.gameui, start, "_ZN7CGameUI5StartEP16cl_enginefuncs_siP11IBaseSystem")
            GET(host.gameui, shutdown, "_ZN7CGameUI8ShutdownEv")
            GET(host.gameui, activate, "_ZN7CGameUI14ActivateGameUIEv")
            GET(host.gameui, hide, "_ZN7CGameUI10HideGameUIEv")
            GET(host.gameui, frame, "_ZN7CGameUI8RunFrameEv")
            GET(host.gameui, is_active, "_ZN7CGameUI14IsGameUIActiveEv")
            GET(host.gameui, exclusive_input, "_ZN7CGameUI17HasExclusiveInputEv")
            GET(host.gameui, console_visible, "_ZN12CGameConsole16IsConsoleVisibleEv")
            GET(host.gameui, console_initialize, "_ZN12CGameConsole10InitializeEv")
            GET(host.gameui, console_activate, "_ZN12CGameConsole8ActivateEv")
            GET(host.gameui, console_hide, "_ZN12CGameConsole4HideEv")
            GET(host.gameui, console_parent, "_ZN12CGameConsole9SetParentEi")
            GET(host.gameui, controls_name, "_ZN5vgui221GetControlsModuleNameEv")
            GET(host.gameui, get_vpanel, "_ZN5vgui25Panel9GetVPanelEv")
            GET(host.vgui, safe_handle, "_ZN5CVGui13PanelToHandleEj")
            GET(host.vgui, live_panel, "_ZN5CVGui13HandleToPanelEm")
            GET(host.vgui, panel_module, "_ZN13VPanelWrapper13GetModuleNameEj")
            GET(host.vgui, child_count, "_ZN13VPanelWrapper13GetChildCountEj")
            GET(host.vgui, child, "_ZN13VPanelWrapper8GetChildEji")
            GET(host.vgui, parent, "_ZN13VPanelWrapper9GetParentEj")
            GET(host.vgui, set_parent, "_ZN13VPanelWrapper9SetParentEjj")
            GET(host.vgui, delete_panel, "_ZN13VPanelWrapper11DeletePanelEj")
            GET(host.vgui, dispatch, "_ZN5CVGui16DispatchMessagesEv")
#undef GET
            return true;
        }
        std::vector<uint32_t> Children(uint32_t value)
        {
            const int count = child_count(panels, value);
            if (count < 0 || count > 4096)
            {
                throw std::runtime_error("Native GameUI child count is invalid");
            }
            std::vector<uint32_t> result;
            for (int i = 0; i < count; ++i)
            {
                const auto item = child(panels, value, i);
                if (!item || item == value)
                {
                    throw std::runtime_error("Native GameUI child handle is invalid");
                }
                result.push_back(item);
            }
            return result;
        }
        // Called while platform modules are still loaded. Cache safe handles,
        // not raw Panel pointers, and protect foreign controls before shutdown.
        void Snapshot(bool detach_foreign)
        {
            if (sealed)
            {
                throw std::runtime_error("Native GameUI lifetime has ended");
            }
            const auto* label = controls_name();
            if (!label || !*label)
            {
                throw std::runtime_error("Native GameUI module name is missing");
            }
            module_name = label;
            std::vector<std::pair<uint32_t, unsigned>> pending;
            const auto container = host.panels->GetPanel(2);
            if (container)
            {
                pending.push_back({container, 0});
            }
            if (*base)
            {
                pending.push_back({get_vpanel(*base), 0});
            }
            const int count = host.hooks.popup_count(host.hooks.context);
            if (count < 0 || count > 4096)
            {
                throw std::runtime_error("Native popup count is invalid");
            }
            for (int i = 0; i < count; ++i)
            {
                const auto popup = host.hooks.get_popup(host.hooks.context, i);
                if (popup)
                {
                    pending.push_back({popup, 0});
                }
            }
            std::set<uint32_t> visited;
            std::vector<uint32_t> foreign;
            while (!pending.empty())
            {
                const auto item = pending.back();
                pending.pop_back();
                if (!item.first || !visited.insert(item.first).second)
                {
                    continue;
                }
                if (item.second > 128 || visited.size() > 16384)
                {
                    throw std::runtime_error("Native GameUI tree exceeds traversal bounds");
                }
                const auto registered = safe_handle(gui, item.first);
                if (!registered || live_panel(gui, registered) != item.first)
                {
                    throw std::runtime_error("Native panel safe handle is invalid");
                }
                observed.insert(registered);
                const auto* name = panel_module(panels, item.first);
                if (!name)
                {
                    throw std::runtime_error("Native panel module name is missing");
                }
                if (module_name == name)
                {
                    const auto safe = safe_handle(gui, item.first);
                    if (!safe || live_panel(gui, safe) != item.first)
                    {
                        throw std::runtime_error("Native GameUI safe handle is invalid");
                    }
                    if (std::find(owned.begin(), owned.end(), safe) == owned.end())
                    {
                        owned.push_back(safe);
                    }
                }
                else if (detach_foreign)
                {
                    const auto ancestor = parent(panels, item.first);
                    if (ancestor)
                    {
                        const auto* ancestor_module = panel_module(panels, ancestor);
                        if (ancestor_module && module_name == ancestor_module)
                        {
                            foreign.push_back(item.first);
                        }
                    }
                }
                for (const auto descendant : Children(item.first))
                {
                    pending.push_back({descendant, item.second + 1});
                }
            }
            for (const auto value : foreign)
            {
                set_parent(panels, value, 0);
            }
        }
    };

    NativeGameUiRuntime::NativeGameUiRuntime() = default;
    NativeGameUiRuntime::~NativeGameUiRuntime() noexcept
    {
        try
        {
            Shutdown();
            std::string error;
            RetireControls(error);
        }
        catch (...)
        {
            std::fprintf(stderr, "CSRETRO native GameUI teardown failed\n");
        }
        EndNativeLifetime();
        if (state_ && !CanUnload())
        {
            std::fprintf(stderr, "CSRETRO retained native GameUI callbacks after incomplete teardown\n");
            try
            {
                static auto* retained = new std::vector<std::unique_ptr<State>>();
                retained->push_back(std::move(state_));
            }
            catch (...)
            {
                static_cast<void>(state_.release());
            }
        }
    }
    bool NativeGameUiRuntime::Bind(Host& host, std::string& error)
    {
        error.clear();
        const auto& hooks = host.hooks;
        if (state_ || !host.gameui || !host.vgui || !host.engine || !host.system || !host.panels || !host.panels->IsReady() ||
            !hooks.key_handler || !hooks.event_handler || !hooks.paint || !hooks.reset_input || !hooks.set_visibility ||
            !hooks.popup_count || !hooks.get_popup)
        {
            error = "Native GameUI requires fresh modules, live engine/system, panels and complete real host services";
            return false;
        }
        auto state = std::make_unique<State>(*this, host);
        try
        {
            if (!state->Load())
            {
                error = state->error;
                return false;
            }
        }
        catch (const std::exception& failure)
        {
            error = failure.what();
            return false;
        }
        state_ = std::move(state);
        return true;
    }
    void* NativeGameUiRuntime::BaseUiInterface()
    {
        return state_ && !state_->sealed ? &state_->base_ui : nullptr;
    }
    NativeGameUiRuntime::Factory NativeGameUiRuntime::GameFactory() const
    {
        return state_ && !state_->sealed && !state_->release ? state_->game_factory : nullptr;
    }
    bool NativeGameUiRuntime::IsActive() const
    {
        return state_ && state_->Query(state_->is_active, state_->game, "Native GameUI active-state query failed");
    }
    bool NativeGameUiRuntime::IsConsoleVisible() const
    {
        return state_ && state_->console_started &&
               state_->Query(state_->console_visible, state_->console, "Native console visibility query failed");
    }
    bool NativeGameUiRuntime::HasExclusiveInput() const
    {
        return state_ && state_->Query(state_->exclusive_input, state_->game, "Native GameUI exclusive-input query failed");
    }

    bool NativeGameUiRuntime::Initialize(Factory* factories, int count, std::string& error)
    {
        error.clear();
        if (!state_ || state_->sealed || state_->active || state_->init_attempted || state_->release || !factories || count < 4 ||
            count > 6)
        {
            error = "Native GameUI initialization requires a fresh binding and four to six factories";
            return false;
        }
        auto& state = *state_;
        // Latch before a factory callback can reenter this initialization.
        state.init_attempted = true;
        State::Guard guard(state);
        try
        {
            std::vector<Factory> providers{state.game_factory, factories[0], factories[1], factories[2]};
            if (count > 5)
            {
                providers.push_back(factories[5]);
            }
            const char* required[] = {
                "VGUI_ivgui006",
                "VGUI_Input004",
                "VGUI_Panel007",
                "VGUI_Surface026",
                "VGUI_Scheme009",
                "VGUI_System009",
                "VGUI_Localize003",
                "VFileSystem009",
                "KeyValues003"
            };
            for (const auto* name : required)
            {
                bool found = false;
                for (size_t i = 0; i < providers.size() && i < static_cast<size_t>(count); ++i)
                {
                    if (providers[i] && providers[i](name, nullptr))
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    throw std::runtime_error(std::string("Missing native GameUI initialization interface: ") + name);
                }
            }
            const char* engine_services[] = {"VEngineVGui001", "VENGINE_GAMEUIFUNCS_VERSION001", "VGUI_Surface026", "BaseUI001"};
            if (!factories[0])
            {
                throw std::runtime_error("Native GameUI engine factory is missing");
            }
            for (const auto* name : engine_services)
            {
                if (!factories[0](name, nullptr))
                {
                    throw std::runtime_error(std::string("Missing native GameUI engine service: ") + name);
                }
            }
            if (factories[0]("BaseUI001", nullptr) != &state.base_ui)
            {
                throw std::runtime_error("Native GameUI engine factory does not expose this BaseUI owner");
            }
            state.native_init_attempted = true;
            state.retired = false;
            state.initialize(state.game, factories, count);
            if (state.release || state.sealed)
            {
                throw std::runtime_error("Native GameUI initialization was interrupted");
            }
            state.Snapshot(false);
            state.initialized = true;
            return true;
        }
        catch (const std::exception& failure)
        {
            state.Fail(failure.what());
            error = state.error;
            return false;
        }
        catch (...)
        {
            state.Fail("Native GameUI initialization failed");
            error = state.error;
            return false;
        }
    }
    bool NativeGameUiRuntime::Start(std::string& error)
    {
        error.clear();
        if (!state_ || !state_->initialized || state_->active || state_->start_attempted || state_->release || state_->sealed ||
            state_->shutdown_attempted)
        {
            error = "Native GameUI startup requires one successful initialization";
            return false;
        }
        state_->start_attempted = true;
        State::Guard guard(*state_);
        try
        {
            state_->start(state_->game, state_->host.engine, 7, state_->host.system);
            if (state_->release || state_->sealed)
            {
                throw std::runtime_error("Native GameUI startup was interrupted");
            }
            state_->Snapshot(false);
            state_->started = true;
            return true;
        }
        catch (const std::exception& failure)
        {
            state_->Fail(failure.what());
            error = state_->error;
            return false;
        }
        catch (...)
        {
            state_->Fail("Native GameUI startup failed");
            error = state_->error;
            return false;
        }
    }
    bool NativeGameUiRuntime::InitializeConsole(std::string& error)
    {
        error.clear();
        if (!IsReady() || state_->active || state_->console_attempted)
        {
            error = "Native console requires a started GameUI and one initialization";
            return false;
        }
        State::Guard guard(*state_);
        state_->console_attempted = true;
        try
        {
            state_->console_initialize(state_->console);
            if (state_->release || state_->sealed)
            {
                throw std::runtime_error("Native console initialization was interrupted");
            }
            state_->console_parent(state_->console, static_cast<int>(state_->host.panels->GetPanel(2)));
            state_->Snapshot(false);
            state_->console_started = true;
            return true;
        }
        catch (const std::exception& failure)
        {
            state_->Fail(failure.what());
            error = state_->error;
            return false;
        }
        catch (...)
        {
            state_->Fail("Native console initialization failed");
            error = state_->error;
            return false;
        }
    }
    void NativeGameUiRuntime::Frame()
    {
        if (!IsReady())
        {
            return;
        }
        State::Guard guard(*state_);
        try
        {
            state_->frame(state_->game);
            if (!state_->sealed && !state_->release)
            {
                state_->Snapshot(false);
            }
        }
        catch (...)
        {
            state_->Fail("Native GameUI frame failed");
        }
    }
    void NativeGameUiRuntime::Activate()
    {
        if (!IsReady())
        {
            return;
        }
        State::Guard guard(*state_);
        try
        {
            state_->activate(state_->game);
            if (state_->sealed || state_->release)
            {
                return;
            }
            auto& hooks = state_->host.hooks;
            hooks.set_visibility(hooks.context, 2, true);
            if (state_->sealed)
            {
                return;
            }
            hooks.set_visibility(hooks.context, 1, false);
            if (state_->sealed)
            {
                return;
            }
            hooks.reset_input(hooks.context);
        }
        catch (...)
        {
            state_->Fail("Native GameUI activation failed");
        }
    }
    void NativeGameUiRuntime::Hide()
    {
        if (!IsReady())
        {
            return;
        }
        State::Guard guard(*state_);
        try
        {
            state_->hide(state_->game);
            if (state_->sealed || state_->release)
            {
                return;
            }
            if (state_->console_started)
            {
                state_->console_hide(state_->console);
            }
            if (state_->sealed)
            {
                return;
            }
            const auto* level = state_->host.engine->pfnGetLevelName ? state_->host.engine->pfnGetLevelName() : nullptr;
            auto& hooks = state_->host.hooks;
            if (level && *level)
            {
                hooks.set_visibility(hooks.context, 2, false);
                hooks.set_visibility(hooks.context, 1, true);
            }
            hooks.reset_input(hooks.context);
        }
        catch (...)
        {
            state_->Fail("Native GameUI hiding failed");
        }
    }
    void NativeGameUiRuntime::ShowConsole()
    {
        if (!IsReady() || !state_->console_started)
        {
            return;
        }
        State::Guard guard(*state_);
        try
        {
            auto& hooks = state_->host.hooks;
            hooks.set_visibility(hooks.context, 2, true);
            if (state_->sealed)
            {
                return;
            }
            state_->console_activate(state_->console);
            if (state_->sealed)
            {
                return;
            }
            hooks.reset_input(hooks.context);
        }
        catch (...)
        {
            state_->Fail("Native console activation failed");
        }
    }
    void NativeGameUiRuntime::HideConsole()
    {
        if (!IsReady() || !state_->console_started)
        {
            return;
        }
        State::Guard guard(*state_);
        try
        {
            state_->console_hide(state_->console);
            if (state_->sealed)
            {
                return;
            }
            state_->host.hooks.reset_input(state_->host.hooks.context);
        }
        catch (...)
        {
            state_->Fail("Native console hiding failed");
        }
    }
    void NativeGameUiRuntime::Shutdown()
    {
        if (!state_ || state_->sealed || state_->shutdown_attempted || !state_->native_init_attempted)
        {
            return;
        }
        if (state_->active)
        {
            state_->release = true;
            return;
        }
        auto& state = *state_;
        State::Guard guard(state);
        state.release = true;
        state.shutdown_attempted = true;
        std::exception_ptr failure;
        try
        {
            state.Snapshot(true);
        }
        catch (...)
        {
            state.uncertain = true;
            failure = std::current_exception();
        }
        try
        {
            if (state.sealed)
            {
                throw std::runtime_error("Native GameUI lifetime ended during shutdown preparation");
            }
            state.shutdown(state.game);
            state.shutdown_success = true;
        }
        catch (...)
        {
            state.uncertain = true;
            if (!failure)
            {
                failure = std::current_exception();
            }
        }
        if (failure)
        {
            state.Fail("Native GameUI shutdown was incomplete");
            std::rethrow_exception(failure);
        }
    }
    bool NativeGameUiRuntime::RetireControls(std::string& error)
    {
        error.clear();
        if (!state_ || state_->retired)
        {
            return true;
        }
        auto& state = *state_;
        if (state.sealed || state.active || !state.shutdown_attempted)
        {
            error = "Native controls require completed shutdown before their lifetime ends";
            return false;
        }
        State::Guard guard(state);
        // Shutdown may unload foreign modules. Verify handle topology without
        // invoking callbacks from newly appeared, unclassified controls.
        try
        {
            std::vector<uint32_t> pending;
            const auto container = state.host.panels->GetPanel(2);
            if (container)
            {
                pending.push_back(container);
            }
            for (const auto safe : state.owned)
            {
                const auto value = state.live_panel(state.gui, safe);
                if (value)
                {
                    pending.push_back(value);
                }
            }
            const int popups = state.host.hooks.popup_count(state.host.hooks.context);
            if (popups < 0 || popups > 4096)
            {
                throw std::runtime_error("Native popup count changed to an invalid value");
            }
            for (int i = 0; i < popups; ++i)
            {
                const auto value = state.host.hooks.get_popup(state.host.hooks.context, i);
                if (value)
                {
                    pending.push_back(value);
                }
            }
            std::set<uint32_t> visited;
            while (!pending.empty())
            {
                const auto value = pending.back();
                pending.pop_back();
                if (!visited.insert(value).second)
                {
                    continue;
                }
                if (visited.size() > 16384)
                {
                    throw std::runtime_error("Native retirement traversal exceeds bounds");
                }
                const auto safe = state.safe_handle(state.gui, value);
                if (!safe || !state.observed.count(safe))
                {
                    throw std::runtime_error("Unclassified native control appeared during shutdown");
                }
                for (const auto descendant : state.Children(value))
                {
                    pending.push_back(descendant);
                }
            }
        }
        catch (const std::exception& failure)
        {
            state.Fail(failure.what());
            state.uncertain = true;
            error = state.error;
            return false;
        }
        catch (...)
        {
            state.Fail("Native retirement topology verification failed");
            state.uncertain = true;
            error = state.error;
            return false;
        }

        bool complete = true;
        // Delete ancestors first: native control destructors may explicitly own
        // descendants in addition to the ordinary auto-delete hierarchy.
        for (size_t round = 0; round <= state.owned.size(); ++round)
        {
            std::vector<std::pair<uint32_t, uint32_t>> alive;
            std::set<uint32_t> handles;
            try
            {
                for (const auto safe : state.owned)
                {
                    const auto panel = state.live_panel(state.gui, safe);
                    if (panel)
                    {
                        alive.push_back({safe, panel});
                        handles.insert(panel);
                    }
                }
            }
            catch (...)
            {
                state.Fail("Native GameUI retirement handle lookup failed");
                state.uncertain = true;
                complete = false;
                break;
            }
            if (alive.empty())
            {
                break;
            }
            bool progress = false;
            for (const auto& item : alive)
            {
                try
                {
                    if (!state.live_panel(state.gui, item.first))
                    {
                        continue;
                    }
                    auto ancestor = state.parent(state.panels, item.second);
                    unsigned depth = 0;
                    bool covered = false;
                    while (ancestor)
                    {
                        if (++depth > 128)
                        {
                            throw std::runtime_error("Native control parent chain is invalid");
                        }
                        if (handles.count(ancestor))
                        {
                            covered = true;
                            break;
                        }
                        ancestor = state.parent(state.panels, ancestor);
                    }
                    if (covered)
                    {
                        continue;
                    }
                    const auto* module = state.panel_module(state.panels, item.second);
                    if (!module || state.module_name != module)
                    {
                        throw std::runtime_error("Native GameUI ownership changed before retirement");
                    }
                    if (!state.delete_attempted.insert(item.first).second)
                    {
                        throw std::runtime_error("A native control deletion has uncertain completion");
                    }
                    state.delete_panel(state.panels, item.second);
                    progress = true;
                }
                catch (const std::exception& failure)
                {
                    state.Fail(failure.what());
                    state.uncertain = true;
                    complete = false;
                }
                catch (...)
                {
                    state.Fail("Native GameUI control deletion failed");
                    state.uncertain = true;
                    complete = false;
                }
            }
            if (!progress)
            {
                complete = false;
                break;
            }
        }
        try
        {
            state.dispatch(state.gui);
            for (const auto safe : state.owned)
            {
                if (state.live_panel(state.gui, safe))
                {
                    complete = false;
                }
            }
        }
        catch (...)
        {
            state.Fail("Native control retirement verification failed");
            complete = false;
            state.uncertain = true;
        }
        state.retired = complete && !state.uncertain;
        if (!state.retired)
        {
            error = state.error.empty() ? "Native GameUI controls could not be retired" : state.error;
            return false;
        }
        return true;
    }
    void NativeGameUiRuntime::EndNativeLifetime() noexcept
    {
        if (state_)
        {
            state_->sealed = true;
            state_->release = true;
        }
    }
    bool NativeGameUiRuntime::IsReady() const
    {
        return state_ && state_->started && !state_->release && !state_->sealed && !state_->shutdown_attempted;
    }
    bool NativeGameUiRuntime::CanUnload() const
    {
        return !state_ ||
               (!state_->active && !state_->uncertain && state_->retired && (!state_->native_init_attempted || state_->shutdown_success));
    }
    bool NativeGameUiRuntime::ReleaseRequested() const
    {
        return state_ && state_->release;
    }
    const std::string& NativeGameUiRuntime::LastError() const
    {
        static const std::string empty;
        return state_ ? state_->error : empty;
    }
} // namespace csr::client
