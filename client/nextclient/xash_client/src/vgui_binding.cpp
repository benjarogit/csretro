#include "vgui_binding.h"
#include "native_gameui_funcs.h"
#include "original_vgui_symbols.h"

#include <cstring>
#include <cstdio>
#include <dlfcn.h>

namespace csr::client
{
    VguiBinding* VguiBinding::active_{};

    namespace
    {
        // Facts for the registered original client. These are symbol names,
        // not offsets; the vendored SDK's interfaces must not be substituted.
        struct InterfaceSlot { const char* symbol; const char* name; };
        constexpr InterfaceSlot interfaces[] = {
            {"_ZN5vgui216g_pVGuiInterfaceE", "VGUI_ivgui006"},
            {"_ZN5vgui217g_pInputInterfaceE", "VGUI_Input004"},
            {"_ZN5vgui217g_pPanelInterfaceE", "VGUI_Panel007"},
            {"_ZN5vgui218g_pSchemeInterfaceE", "VGUI_Scheme009"},
            {"_ZN5vgui218g_pSystemInterfaceE", "VGUI_System009"},
            {"_ZN5vgui219g_pSurfaceInterfaceE", "VGUI_Surface026"},
            {"_ZN5vgui220g_pLocalizeInterfaceE", "VGUI_Localize003"},
            {"_ZN5vgui222g_pFileSystemInterfaceE", "VFileSystem009"},
            {"g_pKeyValuesInterface", "KeyValues003"},
        };
    }

    VguiBinding::~VguiBinding()
    {
        try { Shutdown(); }
        catch (...) { std::fprintf(stderr, "CSRETRO original VGUI shutdown failed\n"); }
    }

    void* VguiBinding::HostFactory(const char* name, int* code)
    {
        void* value{};
        if (active_ && name)
        {
            if (std::strcmp(name, "VGUI_Surface026") == 0)
                value = active_->surface_;
            else if (std::strcmp(name, "VFileSystem009") == 0)
                value = active_->filesystem_;
            else if (std::strcmp(name, "VENGINE_GAMEUIFUNCS_VERSION001") == 0 &&
                     active_->gameui_funcs_ && active_->gameui_funcs_->IsReady())
                value = active_->gameui_funcs_;
        }
        if (code)
            *code = value ? 0 : 1;
        return value;
    }

    bool VguiBinding::BindInterfaces(void* client, void* vgui, void* surface, std::string& error, NativeGameUiFuncs* gameui_funcs)
    {
        error.clear();
        if (!client || !vgui || !surface || active_)
        {
            error = "Original VGUI requires both modules, one Surface and an exclusive binding";
            return false;
        }
        auto factory = reinterpret_cast<Factory>(dlsym(vgui, "CreateInterface"));
        if (!factory)
        {
            error = "Original VGUI has no CreateInterface export";
            return false;
        }
        auto files = detail::Symbol<void**>(client, "g_pFileSystem", error);
        auto viewport = detail::Symbol<void**>(client, "gViewPortInterface", error);
        auto init = detail::Symbol<bool (*)(void*, Factory*, int)>(vgui, "_ZN5CVGui4InitEPPFPvPKcPiEi", error);
        shutdown_ = detail::Symbol<void (*)(void*)>(vgui, "_ZN5CVGui8ShutdownEv", error);
        viewport_init_ = detail::Symbol<void (*)(void*, Factory*, int)>(client,
                "_ZN21CounterStrikeViewport10InitializeEPPFPvPKcPiEi", error);
        viewport_start_ = detail::Symbol<void (*)(void*)>(client,
                "_ZN21CounterStrikeViewport5StartEv", error);
        surface_ = surface;
        gameui_funcs_ = gameui_funcs;
        filesystem_ = files ? *files : nullptr;
        for (size_t i = 0; i < slots_.size(); ++i)
        {
            slots_[i].address = detail::Symbol<void**>(client, interfaces[i].symbol, error);
            slots_[i].value = std::strcmp(interfaces[i].name, "VGUI_Surface026") == 0 ? surface_ :
                    std::strcmp(interfaces[i].name, "VFileSystem009") == 0 ? filesystem_ : factory(interfaces[i].name, nullptr);
            if (!slots_[i].value && error.empty())
                error = std::string("Missing original VGUI interface: ") + interfaces[i].name;
        }
        if (!viewport || !*viewport || !error.empty())
        {
            if (error.empty())
                error = "Original client has no initialized viewport";
            return false;
        }
        ivgui_ = slots_[0].value;
        // The registered viewport reads all four incoming slots regardless
        // of count, then prepends its own factory for VGuiControls_Init.
        // Engine/FS services share our host; UI roles share the original VGUI.
        factories_ = {HostFactory, HostFactory, factory, factory};
        const auto selected_viewport = *viewport;
        active_ = this;
        phase_ = Phase::Binding;
        bool ready{};
        try { ready = init(ivgui_, factories_.data(), static_cast<int>(factories_.size())); }
        catch (...)
        {
            phase_ = Phase::Unbound;
            active_ = nullptr;
            shutdown_requested_ = false;
            throw;
        }
        if (!ready)
        {
            active_ = nullptr;
            phase_ = Phase::Unbound;
            shutdown_requested_ = false;
            error = "Original VGUI initialization failed";
            return false;
        }
        phase_ = Phase::Bound;
        if (shutdown_requested_ || *viewport != selected_viewport)
        {
            Shutdown();
            error = "Original VGUI binding was interrupted";
            return false;
        }
        viewport_slot_ = viewport;
        viewport_ = selected_viewport;
        for (auto& slot : slots_)
        {
            slot.original = *slot.address;
            *slot.address = slot.value;
        }
        globals_bound_ = true;
        return true;
    }

    bool VguiBinding::InitializeViewport(std::string& error)
    {
        error.clear();
        if (phase_ != Phase::Bound || active_ != this || shutting_down_)
        {
            error = "Viewport initialization requires bound original VGUI interfaces";
            return false;
        }
        if (*viewport_slot_ != viewport_)
        {
            error = "Original viewport changed after binding";
            return false;
        }
        phase_ = Phase::InitializingViewport;
        try { viewport_init_(viewport_, factories_.data(), static_cast<int>(factories_.size())); }
        catch (...)
        {
            phase_ = Phase::Bound;
            Shutdown();
            throw;
        }
        phase_ = Phase::ViewportInitialized;
        if (shutdown_requested_)
        {
            Shutdown();
            error = "Original viewport initialization was interrupted";
            return false;
        }
        if (*viewport_slot_ != viewport_)
        {
            error = "Original viewport initialization was interrupted";
            return false;
        }
        return true;
    }

    bool VguiBinding::StartViewport(std::string& error)
    {
        error.clear();
        if (phase_ != Phase::ViewportInitialized || active_ != this || shutting_down_)
        {
            error = "Viewport startup requires original viewport initialization";
            return false;
        }
        if (*viewport_slot_ != viewport_)
        {
            error = "Original viewport changed before startup";
            return false;
        }
        phase_ = Phase::StartingViewport;
        try { viewport_start_(viewport_); }
        catch (...)
        {
            phase_ = Phase::ViewportInitialized;
            Shutdown();
            throw;
        }
        phase_ = Phase::ViewportInitialized;
        if (shutdown_requested_)
        {
            Shutdown();
            error = "Original viewport startup was interrupted";
            return false;
        }
        if (*viewport_slot_ != viewport_)
        {
            error = "Original viewport changed during startup";
            return false;
        }
        phase_ = Phase::Started;
        return true;
    }

    void VguiBinding::Shutdown()
    {
        if (shutting_down_) return;
        if (phase_ == Phase::Binding || phase_ == Phase::InitializingViewport || phase_ == Phase::StartingViewport)
        {
            // Native code may still be using VGUI. Defer teardown until it
            // returns, keeping factories/globals live throughout the call.
            shutdown_requested_ = true;
            return;
        }
        shutting_down_ = true;
        const bool bound = phase_ != Phase::Unbound;
        phase_ = Phase::Unbound;
        const auto restore = [this]() noexcept {
            if (globals_bound_)
            {
                for (auto& slot : slots_)
                    *slot.address = slot.original;
                globals_bound_ = false;
            }
            if (active_ == this)
                active_ = nullptr;
            viewport_slot_ = nullptr;
            viewport_ = nullptr;
            gameui_funcs_ = nullptr;
            shutdown_requested_ = false;
            shutting_down_ = false;
        };
        try { if (bound) shutdown_(ivgui_); }
        catch (...)
        {
            restore();
            throw;
        }
        restore();
    }
}
