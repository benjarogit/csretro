#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace csr::client
{
    // Owns only structural VPanels allocated by the already loaded original VGUI.
    // Controls from other modules remain borrowed, and are detached on shutdown.
    class NativePanelTree
    {
    public:
        NativePanelTree();
        ~NativePanelTree() noexcept;
        NativePanelTree(const NativePanelTree&) = delete;
        NativePanelTree& operator=(const NativePanelTree&) = delete;

        bool Bind(void* vgui, int width, int height, std::string& error);
        // Detach failures preserve live callbacks and permit a later retry.
        // An uncertain throwing FreePanel is never attempted twice.
        void Shutdown();
        // Terminal boundary before CVGui shutdown: retained callbacks may no
        // longer access native services or retry allocator operations.
        void EndNativeLifetime() noexcept;
        void Resize(int width, int height);
        bool IsReady() const;
        bool ReleaseRequested() const;
        uint32_t GetPanel(int type) const;
        void* Root() const;

    private:
        struct State;
        std::unique_ptr<State> state_;
    };

    // Exact old engine interface: its two Itanium destructor entries precede
    // GetPanel and the two obsolete Steam operations. This object is borrowed.
    class NativeEngineVGui
    {
    public:
        explicit NativeEngineVGui(NativePanelTree& tree) :
            tree_(tree)
        {}
        virtual ~NativeEngineVGui() = default;
        virtual uint32_t GetPanel(int type)
        {
            return tree_.GetPanel(type);
        }
        virtual bool SteamRefreshLogin(const char*, bool)
        {
            return false;
        }
        virtual bool SteamProcessCall(bool* finished, void*, void*)
        {
            if (finished)
            {
                *finished = false;
            }
            return false;
        }

    private:
        NativePanelTree& tree_;
    };
} // namespace csr::client
