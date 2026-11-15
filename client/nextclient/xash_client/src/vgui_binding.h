#pragma once

#include <array>
#include <string>

namespace csr::client
{
    // Original VGUI uses different versioned interfaces from the vendored UI.
    // Keep these opaque and call resolved original methods; never cast them to
    // the vendored IPanel, IScheme or IInput classes.
    class NativeGameUiFuncs;
    class VguiBinding
    {
    public:
        using Factory = void* (*)(const char*, int*);
        VguiBinding() = default;
        ~VguiBinding();
        VguiBinding(const VguiBinding&) = delete;
        VguiBinding& operator=(const VguiBinding&) = delete;

        // Both modules and the Surface are borrowed and must outlive this owner.
        // Keep the native phases separate so GameUI can initialize before the
        // viewport and start between viewport initialization and startup.
        bool BindInterfaces(void* client, void* vgui, void* surface, std::string& error, NativeGameUiFuncs* gameui_funcs = nullptr);
        bool InitializeViewport(std::string& error);
        bool StartViewport(std::string& error);
        void Shutdown();
        bool HasBinding() const { return phase_ != Phase::Unbound || shutting_down_; }
        bool IsInitialized() const { return phase_ == Phase::Started; }

    private:
        static void* HostFactory(const char* name, int* code);
        static VguiBinding* active_;
        struct Slot { void** address{}; void* original{}; void* value{}; };
        std::array<Slot, 9> slots_{};
        std::array<Factory, 4> factories_{};
        void* surface_{};
        NativeGameUiFuncs* gameui_funcs_{};
        void* filesystem_{};
        void* ivgui_{};
        void (*shutdown_)(void*){};
        enum class Phase { Unbound, Binding, Bound, InitializingViewport, ViewportInitialized, StartingViewport, Started };
        Phase phase_{Phase::Unbound};
        void** viewport_slot_{};
        void* viewport_{};
        void (*viewport_init_)(void*, Factory*, int){};
        void (*viewport_start_)(void*){};
        bool shutting_down_{};
        bool shutdown_requested_{};
        bool globals_bound_{};
    };
}
