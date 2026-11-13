#pragma once

#include <array>
#include <string>

namespace csr::client
{
    // Original VGUI uses different versioned interfaces from the vendored UI.
    // Keep these opaque and call resolved original methods; never cast them to
    // the vendored IPanel, IScheme or IInput classes.
    class VguiBinding
    {
    public:
        using Factory = void* (*)(const char*, int*);
        VguiBinding() = default;
        ~VguiBinding();
        VguiBinding(const VguiBinding&) = delete;
        VguiBinding& operator=(const VguiBinding&) = delete;

        // Both modules and the Surface are borrowed and must outlive this owner.
        bool Initialize(void* client, void* vgui, void* surface, std::string& error);
        void Shutdown();
        bool IsInitialized() const { return initialized_; }

    private:
        static void* HostFactory(const char* name, int* code);
        static VguiBinding* active_;
        struct Slot { void** address{}; void* original{}; void* value{}; };
        std::array<Slot, 9> slots_{};
        std::array<Factory, 2> factories_{};
        void* surface_{};
        void* filesystem_{};
        void* ivgui_{};
        void (*shutdown_)(void*){};
        bool initialized_{};
        bool globals_bound_{};
    };
}
