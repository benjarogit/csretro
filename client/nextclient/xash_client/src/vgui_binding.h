#pragma once

#include <array>
#include <string>

namespace csr::client
{
    // Original VGUI uses different versioned interfaces from the vendored UI.
    // Keep these opaque and call resolved original methods; never cast them to
    // the vendored IPanel, IScheme or IInput classes.
    class NativeGameUiFuncs;
    class NativePanelTree;
    class NativeEngineVGui;
    class VguiBinding
    {
    public:
        using Factory = void* (*)(const char*, int*);
        struct PanelServices
        {
            NativePanelTree* tree{};
            NativeEngineVGui* engine{};
            void (*detach_surface)(void*){};
            void (*release_services)(void*) noexcept {};
            void* owner{};
            bool (*can_release)(void*){};
            void (*end_native_lifetime)(void*) noexcept {};
            void (*stop_client)(void*){};
        };
        VguiBinding() = default;
        ~VguiBinding();
        VguiBinding(const VguiBinding&) = delete;
        VguiBinding& operator=(const VguiBinding&) = delete;

        // Both modules and the Surface are borrowed and must outlive this owner.
        // Keep the native phases separate so GameUI can initialize before the
        // viewport and start between viewport initialization and startup.
        bool BindInterfaces(
            void* client,
            void* vgui,
            void* surface,
            std::string& error,
            NativeGameUiFuncs* gameui_funcs = nullptr,
            const PanelServices* panels = nullptr
        );
        bool InitializeEnginePanels(int width, int height, std::string& error);
        bool InitializeViewport(std::string& error);
        bool StartViewport(std::string& error);
        bool RunNativeUi(void (*operation)(void*), void* context);
        void Shutdown();
        bool RunStartupUi(void (*operation)(void*), void* context);
        bool RunKeyInput(void (*operation)(void*), void* context);
        bool CanDispatchNativeUi() const
        {
            return phase_ == Phase::Started && !shutting_down_ && !shutdown_requested_;
        }
        bool SetBaseUi(void* base_ui);
        Factory EngineFactory() const
        {
            return HostFactory;
        }
        void* FileSystem() const
        {
            return filesystem_;
        }
        bool HasBinding() const { return phase_ != Phase::Unbound || shutting_down_; }
        bool IsInitialized() const { return phase_ == Phase::Started; }

    private:
        static void* HostFactory(const char* name, int* code);
        static VguiBinding* active_;
        struct Slot { void** address{}; void* original{}; void* value{}; };
        std::array<Slot, 9> slots_{};
        std::array<Factory, 4> factories_{};
        void* surface_{};
        void* base_ui_{};
        NativeGameUiFuncs* gameui_funcs_{};
        PanelServices engine_panels_{};
        void* vgui_module_{};
        void (*viewport_parent_)(void*, unsigned){};
        void* filesystem_{};
        void* ivgui_{};
        void (*shutdown_)(void*){};
        enum class Phase
        {
            Unbound,
            Binding,
            Bound,
            InitializingEnginePanels,
            InitializingViewport,
            ViewportInitialized,
            StartingViewport,
            Started
        };
        Phase phase_{Phase::Unbound};
        void** viewport_slot_{};
        void* viewport_{};
        void (*viewport_init_)(void*, Factory*, int){};
        void (*viewport_start_)(void*){};
        bool shutting_down_{};
        bool shutdown_requested_{};
        bool globals_bound_{};
        unsigned native_call_depth_{};
    };
}
