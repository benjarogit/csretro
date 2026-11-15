#include "NativePanelTree.h"

#include "elf_symbols.h"

#include <array>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <dlfcn.h>
#include <exception>
#include <vector>

namespace csr::client
{
    namespace
    {
        using Handle = uint32_t;
        void* Pointer(Handle value)
        {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(value));
        }

        // The registered native IClientPanel starts directly with GetVPanel;
        // it has no virtual destructor and never exposes an SDK Panel object.
        class ClientPanel
        {
        public:
            virtual Handle GetVPanel() = 0;
            virtual void Think() = 0;
            virtual void PerformApplySchemeSettings() = 0;
            virtual void PaintTraverse(bool force, bool allow) = 0;
            virtual void Repaint() = 0;
            virtual Handle IsWithinTraverse(int x, int y, bool popups) = 0;
            virtual void GetInset(int&, int&, int&, int&) = 0;
            virtual void GetClipRect(int&, int&, int&, int&) = 0;
            virtual void OnChildAdded(Handle) = 0;
            virtual void OnSizeChanged(int, int) = 0;
            virtual void InternalFocusChanged(bool) = 0;
            virtual bool RequestInfo(void*) = 0;
            virtual void RequestFocus(int) = 0;
            virtual bool RequestFocusPrev(Handle) = 0;
            virtual bool RequestFocusNext(Handle) = 0;
            virtual void OnMessage(const void*, Handle) = 0;
            virtual Handle GetCurrentKeyFocus() = 0;
            virtual int GetTabPosition() = 0;
            virtual const char* GetName() = 0;
            virtual const char* GetClassName() = 0;
            virtual Handle GetScheme() = 0;
            virtual bool IsProportional() = 0;
            virtual bool IsAutoDeleteSet() = 0;
            virtual void DeletePanel() = 0;
            virtual void* QueryInterface(int) = 0;
            virtual void* GetPanel() = 0;
            virtual const char* GetModuleName() = 0;
        };
    } // namespace

    struct NativePanelTree::State
    {
        void* panels{};
        void* ivgui{};
        void* schemes{};
        bool ready{};
        bool release{};
        bool stopping{};
        bool sealed{};
        unsigned active{};
        std::exception_ptr uncertain_failure;
        Handle (*allocate)(void*){};
        void (*free)(void*, Handle){};
        void (*initialize)(void*, ClientPanel*){};
        void (*parent)(void*, Handle, Handle){};
        int (*child_count)(void*, Handle){};
        Handle (*child)(void*, Handle, int){};
        bool (*visible)(void*, Handle){};
        bool (*popup)(void*, Handle){};
        bool (*keyboard)(void*, Handle){};
        bool (*mouse)(void*, Handle){};
        void (*set_visible)(void*, Handle, bool){};
        void (*set_keyboard)(void*, Handle, bool){};
        void (*set_mouse)(void*, Handle, bool){};
        void (*position)(void*, Handle, int, int){};
        void (*size)(void*, Handle, int, int){};
        void (*solve)(void*, Handle){};
        void (*clip)(void*, int&, int&, int&, int&){};
        void (*paint)(void*, Handle, bool, bool){};
        void (*repaint)(void*, Handle){};
        Handle (*hit)(void*, Handle, int, int, bool){};
        void (*focus)(void*, Handle, int){};
        bool (*focus_prev)(void*, Handle, Handle){};
        bool (*focus_next)(void*, Handle, Handle){};
        Handle (*current_focus)(void*, Handle){};
        Handle (*default_scheme)(void*){};
        const char* (*message_name)(const void*){};

        struct Guard
        {
            State& state;
            explicit Guard(State& value) :
                state(value)
            {
                ++state.active;
            }
            ~Guard()
            {
                --state.active;
            }
        };

        struct Node final : ClientPanel
        {
            State& owner;
            const char* name;
            Handle handle{};
            bool dirty{true};
            bool free_attempted{};
            Node(State& state, const char* label) :
                owner(state),
                name(label)
            {}
            Handle GetVPanel() override
            {
                return owner.sealed ? 0 : handle;
            }
            void Think() override {}
            void PerformApplySchemeSettings() override
            {
                dirty = true;
            }
            void PaintTraverse(bool force, bool allow) override
            {
                if (owner.sealed)
                {
                    return;
                }
                Guard guard(owner);
                if (!owner.visible(owner.panels, handle))
                {
                    return;
                }
                for (const auto value : owner.Children(handle))
                {
                    if (owner.release)
                    {
                        break;
                    }
                    if (owner.visible(owner.panels, value) && !owner.popup(owner.panels, value))
                    {
                        owner.paint(owner.panels, value, force || dirty, allow);
                    }
                }
                dirty = false;
            }
            void Repaint() override
            {
                if (owner.sealed)
                {
                    return;
                }
                Guard guard(owner);
                dirty = true;
                for (const auto value : owner.Children(handle))
                {
                    owner.repaint(owner.panels, value);
                    if (owner.release)
                    {
                        break;
                    }
                }
            }
            Handle IsWithinTraverse(int x, int y, bool popups) override
            {
                if (owner.sealed)
                {
                    return 0;
                }
                Guard guard(owner);
                int left{}, top{}, right{}, bottom{};
                owner.clip(Pointer(handle), left, top, right, bottom);
                if (owner.sealed || !owner.visible(owner.panels, handle) || !owner.mouse(owner.panels, handle) || x < left || x >= right ||
                    y < top || y >= bottom)
                {
                    return 0;
                }
                const auto children = owner.Children(handle);
                for (auto it = children.rbegin(); it != children.rend() && !owner.release; ++it)
                {
                    if (!owner.visible(owner.panels, *it) || (!popups && owner.popup(owner.panels, *it)))
                    {
                        continue;
                    }
                    const auto result = owner.hit(owner.panels, *it, x, y, popups);
                    if (result)
                    {
                        return result;
                    }
                }
                // Structural containers are transparent to otherwise unhandled input.
                return 0;
            }
            void GetInset(int& left, int& top, int& right, int& bottom) override
            {
                left = top = right = bottom = 0;
            }
            void GetClipRect(int& left, int& top, int& right, int& bottom) override
            {
                if (owner.sealed)
                {
                    left = top = right = bottom = 0;
                    return;
                }
                Guard guard(owner);
                owner.clip(Pointer(handle), left, top, right, bottom);
            }
            void OnChildAdded(Handle) override
            {
                dirty = true;
            }
            void OnSizeChanged(int, int) override
            {
                dirty = true;
            }
            void InternalFocusChanged(bool) override
            {
                dirty = true;
            }
            bool RequestInfo(void*) override
            {
                return false;
            }
            void RequestFocus(int direction) override
            {
                if (owner.sealed)
                {
                    return;
                }
                Guard guard(owner);
                auto children = owner.Focusable(handle);
                if (direction < 0)
                {
                    for (auto it = children.rbegin(); it != children.rend(); ++it)
                    {
                        owner.focus(owner.panels, *it, direction);
                        if (owner.current_focus(owner.panels, *it) || owner.release)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    for (const auto value : children)
                    {
                        owner.focus(owner.panels, value, direction);
                        if (owner.current_focus(owner.panels, value) || owner.release)
                        {
                            break;
                        }
                    }
                }
            }
            bool RequestFocusPrev(Handle existing) override
            {
                return MoveFocus(existing, true);
            }
            bool RequestFocusNext(Handle existing) override
            {
                return MoveFocus(existing, false);
            }
            bool MoveFocus(Handle existing, bool previous)
            {
                if (owner.sealed)
                {
                    return false;
                }
                Guard guard(owner);
                auto children = owner.Focusable(handle);
                if (previous)
                {
                    for (auto it = children.rbegin(); it != children.rend() && !owner.release; ++it)
                    {
                        if (owner.focus_prev(owner.panels, *it, existing))
                        {
                            return true;
                        }
                    }
                }
                else
                {
                    for (const auto value : children)
                    {
                        if (owner.release)
                        {
                            break;
                        }
                        if (owner.focus_next(owner.panels, value, existing))
                        {
                            return true;
                        }
                    }
                }
                return false;
            }
            void OnMessage(const void* message, Handle) override
            {
                if (owner.sealed)
                {
                    return;
                }
                Guard guard(owner);
                const auto* label = message ? owner.message_name(message) : nullptr;
                if (label && std::strcmp(label, "Delete") == 0)
                {
                    owner.release = true;
                }
                else if (label && (std::strcmp(label, "Repaint") == 0 || std::strcmp(label, "InvalidateLayout") == 0))
                {
                    dirty = true;
                }
            }
            Handle GetCurrentKeyFocus() override
            {
                if (owner.sealed)
                {
                    return 0;
                }
                Guard guard(owner);
                for (const auto value : owner.Focusable(handle))
                {
                    if (owner.release)
                    {
                        break;
                    }
                    const auto result = owner.current_focus(owner.panels, value);
                    if (result)
                    {
                        return result;
                    }
                }
                return 0;
            }
            int GetTabPosition() override
            {
                return 0;
            }
            const char* GetName() override
            {
                return name;
            }
            const char* GetClassName() override
            {
                return "CSRETROEngineContainer";
            }
            Handle GetScheme() override
            {
                if (owner.sealed)
                {
                    return 0;
                }
                Guard guard(owner);
                return owner.default_scheme(owner.schemes);
            }
            bool IsProportional() override
            {
                return false;
            }
            bool IsAutoDeleteSet() override
            {
                return false;
            }
            void DeletePanel() override
            {
                owner.release = true;
            }
            void* QueryInterface(int) override
            {
                return nullptr;
            }
            void* GetPanel() override
            {
                return nullptr;
            }
            const char* GetModuleName() override
            {
                return "CSRETROEngine";
            }
        };
        std::array<Node, 3> nodes{{{*this, "CSRETRORoot"}, {*this, "CSRETROClient"}, {*this, "CSRETROGameUI"}}};

        std::vector<Handle> Children(Handle value, bool* valid = nullptr)
        {
            if (sealed)
            {
                if (valid)
                {
                    *valid = false;
                }
                return {};
            }
            if (valid)
            {
                *valid = true;
            }
            const int count = child_count(panels, value);
            if (count < 0 || count > 4096)
            {
                release = true;
                if (valid)
                {
                    *valid = false;
                }
                return {};
            }
            std::vector<Handle> values;
            values.reserve(count);
            for (int index = 0; index < count; ++index)
            {
                const auto candidate = child(panels, value, index);
                if (!candidate || candidate == value)
                {
                    release = true;
                    if (valid)
                    {
                        *valid = false;
                    }
                    return {};
                }
                values.push_back(candidate);
            }
            return values;
        }
        std::vector<Handle> Focusable(Handle value)
        {
            if (sealed)
            {
                return {};
            }
            auto values = Children(value);
            std::vector<Handle> result;
            for (const auto item : values)
            {
                if (sealed)
                {
                    break;
                }
                if (visible(panels, item) && keyboard(panels, item))
                {
                    result.push_back(item);
                }
            }
            return result;
        }
        template <class T>
        bool Resolve(void* module, T& function, const char* name, std::string& error)
        {
            function = reinterpret_cast<T>(nitroapi::FindFunctionAddress(module, name));
            if (!function)
            {
                error = std::string("Missing verified native panel function: ") + name;
            }
            return function != nullptr;
        }
        bool Load(void* module, std::string& error)
        {
            const auto factory = reinterpret_cast<void* (*)(const char*, int*)>(dlsym(module, "CreateInterface"));
            if (!factory)
            {
                error = "Native panels require the loaded VGUI factory";
                return false;
            }
            panels = factory("VGUI_Panel007", nullptr);
            ivgui = factory("VGUI_ivgui006", nullptr);
            schemes = factory("VGUI_Scheme009", nullptr);
            if (!panels || !ivgui || !schemes)
            {
                error = "Native panels require original panel, VGUI and scheme services";
                return false;
            }
#define RESOLVE(member, symbol)                  \
    if (!Resolve(module, member, symbol, error)) \
    {                                            \
        return false;                            \
    }
            RESOLVE(allocate, "_ZN5CVGui10AllocPanelEv")
            RESOLVE(free, "_ZN5CVGui9FreePanelEj")
            RESOLVE(initialize, "_ZN5vgui26VPanel4InitEPNS_12IClientPanelE")
            RESOLVE(parent, "_ZN13VPanelWrapper9SetParentEjj")
            RESOLVE(child_count, "_ZN13VPanelWrapper13GetChildCountEj")
            RESOLVE(child, "_ZN13VPanelWrapper8GetChildEji")
            RESOLVE(visible, "_ZN13VPanelWrapper9IsVisibleEj")
            RESOLVE(popup, "_ZN13VPanelWrapper7IsPopupEj")
            RESOLVE(keyboard, "_ZN13VPanelWrapper22IsKeyBoardInputEnabledEj")
            RESOLVE(mouse, "_ZN13VPanelWrapper19IsMouseInputEnabledEj")
            RESOLVE(set_visible, "_ZN13VPanelWrapper10SetVisibleEjb")
            RESOLVE(set_keyboard, "_ZN13VPanelWrapper23SetKeyBoardInputEnabledEjb")
            RESOLVE(set_mouse, "_ZN13VPanelWrapper20SetMouseInputEnabledEjb")
            RESOLVE(position, "_ZN13VPanelWrapper6SetPosEjii")
            RESOLVE(size, "_ZN13VPanelWrapper7SetSizeEjii")
            RESOLVE(solve, "_ZN13VPanelWrapper5SolveEj")
            RESOLVE(clip, "_ZN5vgui26VPanel11GetClipRectERiS1_S1_S1_")
            RESOLVE(paint, "_ZN13VPanelWrapper13PaintTraverseEjbb")
            RESOLVE(repaint, "_ZN13VPanelWrapper7RepaintEj")
            RESOLVE(hit, "_ZN13VPanelWrapper16IsWithinTraverseEjiib")
            RESOLVE(focus, "_ZN13VPanelWrapper12RequestFocusEji")
            RESOLVE(focus_prev, "_ZN13VPanelWrapper16RequestFocusPrevEjj")
            RESOLVE(focus_next, "_ZN13VPanelWrapper16RequestFocusNextEjj")
            RESOLVE(current_focus, "_ZN13VPanelWrapper18GetCurrentKeyFocusEj")
            RESOLVE(default_scheme, "_ZN14CSchemeManager16GetDefaultSchemeEv")
            RESOLVE(message_name, "_ZNK9KeyValues7GetNameEv")
#undef RESOLVE
            if (!default_scheme(schemes))
            {
                error = "Native panels require a nonzero original default scheme";
                return false;
            }
            return true;
        }
    };

    NativePanelTree::NativePanelTree() = default;
    NativePanelTree::~NativePanelTree() noexcept
    {
        try
        {
            Shutdown();
        }
        catch (const std::exception& exception)
        {
            std::fprintf(stderr, "CSRETRO native panel shutdown failed: %s\n", exception.what());
        }
        catch (...)
        {
            std::fprintf(stderr, "CSRETRO native panel shutdown failed\n");
        }
        if (state_)
        {
            // A throwing void FreePanel cannot reveal whether deletion completed.
            // Keep callbacks valid for any still-live native object; retrying that
            // free or destroying its callback would risk a double free or UAF.
            std::fprintf(stderr, "CSRETRO retained native panel callbacks after incomplete shutdown\n");
            try
            {
                // Intentionally process-lifetime storage: native module exit
                // handlers may still dispatch callbacks during global teardown.
                static auto* retained = new std::vector<std::unique_ptr<State>>();
                retained->push_back(std::move(state_));
            }
            catch (...)
            {
                static_cast<void>(state_.release());
            }
        }
    }

    bool NativePanelTree::Bind(void* vgui, int width, int height, std::string& error)
    {
        error.clear();
        if (state_ || !vgui || width <= 0 || height <= 0 || width > 32767 || height > 32767)
        {
            error = "Native panels require an unbound owner and valid native screen bounds";
            return false;
        }
        auto candidate = std::make_unique<State>();
        if (!candidate->Load(vgui, error))
        {
            return false;
        }
        state_ = std::move(candidate);
        try
        {
            State::Guard guard(*state_);
            for (auto& node : state_->nodes)
            {
                node.handle = state_->allocate(state_->ivgui);
                if (!node.handle)
                {
                    error = "Original VGUI could not allocate a native container";
                    break;
                }
                state_->initialize(Pointer(node.handle), &node);
                state_->set_mouse(state_->panels, node.handle, true);
                state_->set_keyboard(state_->panels, node.handle, true);
                state_->set_visible(state_->panels, node.handle, true);
                state_->position(state_->panels, node.handle, 0, 0);
                state_->size(state_->panels, node.handle, width, height);
                if (state_->release)
                {
                    error = "Native panel initialization was interrupted";
                    break;
                }
            }
            if (error.empty())
            {
                state_->parent(state_->panels, state_->nodes[1].handle, state_->nodes[0].handle);
                state_->parent(state_->panels, state_->nodes[2].handle, state_->nodes[0].handle);
                for (const auto& node : state_->nodes)
                {
                    state_->solve(state_->panels, node.handle);
                }
                if (state_->release)
                {
                    error = "Native panel initialization was interrupted";
                }
            }
        }
        catch (const std::exception& exception)
        {
            error = std::string("Native panel initialization failed: ") + exception.what();
        }
        catch (...)
        {
            error = "Native panel initialization failed";
        }
        if (!error.empty())
        {
            Shutdown();
            return false;
        }
        state_->ready = true;
        return true;
    }

    void NativePanelTree::Shutdown()
    {
        if (!state_ || state_->stopping || state_->sealed)
        {
            return;
        }
        if (state_->active)
        {
            state_->release = true;
            return;
        }
        state_->stopping = true;
        state_->ready = false;
        state_->release = true;
        std::exception_ptr first_failure = state_->uncertain_failure;
        const auto remember = [&first_failure]() {
            if (!first_failure)
            {
                first_failure = std::current_exception();
            }
        };
        // Keep every callback object alive until all native notifications end.
        // A failure in one node must not skip teardown of independent nodes.
        for (auto it = state_->nodes.rbegin(); it != state_->nodes.rend(); ++it)
        {
            if (state_->sealed)
            {
                break;
            }
            if (!it->handle || it->free_attempted)
            {
                continue;
            }
            bool detached = true;
            std::vector<Handle> children;
            try
            {
                children = state_->Children(it->handle, &detached);
                if (!detached)
                {
                    throw std::runtime_error("Invalid native panel children during shutdown");
                }
            }
            catch (...)
            {
                detached = false;
                remember();
            }
            for (const auto borrowed : children)
            {
                try
                {
                    state_->parent(state_->panels, borrowed, 0);
                }
                catch (...)
                {
                    detached = false;
                    remember();
                }
            }
            try
            {
                state_->parent(state_->panels, it->handle, 0);
            }
            catch (...)
            {
                detached = false;
                remember();
            }
            if (!detached)
            {
                continue;
            }
            it->free_attempted = true;
            try
            {
                state_->free(state_->ivgui, it->handle);
                it->handle = 0;
            }
            catch (...)
            {
                state_->uncertain_failure = std::current_exception();
                remember();
            }
        }
        state_->stopping = false;
        bool complete = true;
        for (const auto& node : state_->nodes)
        {
            complete = complete && !node.handle;
        }
        if (complete && !state_->sealed)
        {
            state_.reset();
        }
        if (first_failure)
        {
            std::rethrow_exception(first_failure);
        }
    }

    void NativePanelTree::EndNativeLifetime() noexcept
    {
        if (state_)
        {
            state_->sealed = true;
            state_->ready = false;
            state_->release = true;
        }
    }

    void NativePanelTree::Resize(int width, int height)
    {
        if (!IsReady())
        {
            return;
        }
        if (width <= 0 || height <= 0 || width > 32767 || height > 32767)
        {
            state_->release = true;
            return;
        }
        State::Guard guard(*state_);
        for (const auto& node : state_->nodes)
        {
            state_->size(state_->panels, node.handle, width, height);
            if (state_->sealed)
            {
                break;
            }
            state_->solve(state_->panels, node.handle);
            if (state_->release)
            {
                break;
            }
        }
    }
    void NativePanelTree::SetVisible(int type, bool visible)
    {
        if (!IsReady() || state_->sealed || type < 0 || type >= 3)
        {
            return;
        }
        State::Guard guard(*state_);
        state_->set_visible(state_->panels, state_->nodes[type].handle, visible);
    }

    bool NativePanelTree::IsReady() const
    {
        return state_ && state_->ready && !state_->stopping;
    }
    bool NativePanelTree::ReleaseRequested() const
    {
        return state_ && state_->release;
    }
    uint32_t NativePanelTree::GetPanel(int type) const
    {
        return IsReady() && type >= 0 && type < 3 ? state_->nodes[type].handle : 0;
    }
    void* NativePanelTree::Root() const
    {
        return Pointer(GetPanel(0));
    }
} // namespace csr::client
