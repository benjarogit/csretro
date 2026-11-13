#include "vgui_binding.h"
#include "elf_symbols.h"

#include <cstring>
#include <dlfcn.h>

namespace csr::client
{
    VguiBinding* VguiBinding::active_{};

    namespace
    {
        template <class T>
        T Symbol(void* module, const char* name, std::string& error)
        {
            auto address = nitroapi::FindSymbolAddress(module, name);
            if (!address && error.empty())
                error = std::string("Missing original VGUI binding symbol: ") + name;
            return reinterpret_cast<T>(address);
        }

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

    VguiBinding::~VguiBinding() { Shutdown(); }

    void* VguiBinding::HostFactory(const char* name, int* code)
    {
        void* value{};
        if (active_ && name)
        {
            if (std::strcmp(name, "VGUI_Surface026") == 0)
                value = active_->surface_;
            else if (std::strcmp(name, "VFileSystem009") == 0)
                value = active_->filesystem_;
        }
        if (code)
            *code = value ? 0 : 1;
        return value;
    }

    bool VguiBinding::Initialize(void* client, void* vgui, void* surface, std::string& error)
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
        auto files = Symbol<void**>(client, "g_pFileSystem", error);
        auto viewport = Symbol<void**>(client, "gViewPortInterface", error);
        auto init = Symbol<bool (*)(void*, Factory*, int)>(vgui, "_ZN5CVGui4InitEPPFPvPKcPiEi", error);
        shutdown_ = Symbol<void (*)(void*)>(vgui, "_ZN5CVGui8ShutdownEv", error);
        auto viewport_init = Symbol<void (*)(void*, Factory*, int)>(client,
                "_ZN21CounterStrikeViewport10InitializeEPPFPvPKcPiEi", error);
        auto viewport_start = Symbol<void (*)(void*)>(client,
                "_ZN21CounterStrikeViewport5StartEv", error);
        surface_ = surface;
        filesystem_ = files ? *files : nullptr;
        for (size_t i = 0; i < slots_.size(); ++i)
        {
            slots_[i].address = Symbol<void**>(client, interfaces[i].symbol, error);
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
        factories_ = {HostFactory, factory};
        active_ = this;
        if (!init(ivgui_, factories_.data(), static_cast<int>(factories_.size())))
        {
            active_ = nullptr;
            error = "Original VGUI initialization failed";
            return false;
        }
        initialized_ = true;
        for (auto& slot : slots_)
        {
            slot.original = *slot.address;
            *slot.address = slot.value;
        }
        globals_bound_ = true;
        viewport_init(*viewport, factories_.data(), static_cast<int>(factories_.size()));
        viewport_start(*viewport);
        return true;
    }

    void VguiBinding::Shutdown()
    {
        if (initialized_)
        {
            initialized_ = false;
            shutdown_(ivgui_);
        }
        if (globals_bound_)
        {
            for (auto& slot : slots_)
                *slot.address = slot.original;
            globals_bound_ = false;
        }
        if (active_ == this)
            active_ = nullptr;
    }
}
