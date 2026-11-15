// Synthetic old VGUI ABI. No original library or private object layout is used.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace native_panel_fixture
{
    using H = uint32_t;
    class Callback
    {
    public:
        virtual H GetVPanel() = 0;
        virtual void Think() = 0;
        virtual void Scheme() = 0;
        virtual void Paint(bool, bool) = 0;
        virtual void Repaint() = 0;
        virtual H Hit(int, int, bool) = 0;
        virtual void Inset(int&, int&, int&, int&) = 0;
        virtual void Clip(int&, int&, int&, int&) = 0;
        virtual void Child(H) = 0;
        virtual void Size(int, int) = 0;
        virtual void FocusChanged(bool) = 0;
        virtual bool Info(void*) = 0;
        virtual void Focus(int) = 0;
        virtual bool Prev(H) = 0;
        virtual bool Next(H) = 0;
        virtual void Message(const void*, H) = 0;
        virtual H CurrentFocus() = 0;
        virtual int Tab() = 0;
        virtual const char* Name() = 0;
        virtual const char* ClassName() = 0;
        virtual H DefaultScheme() = 0;
        virtual bool Proportional() = 0;
        virtual bool AutoDelete() = 0;
        virtual void Delete() = 0;
        virtual void* Query(int) = 0;
        virtual void* Panel() = 0;
        virtual const char* Module() = 0;
    };
} // namespace native_panel_fixture

namespace
{
    using H = native_panel_fixture::H;
    using Callback = native_panel_fixture::Callback;
    struct Record
    {
        Callback* callback{};
        std::vector<H> children;
        H parent{};
        bool visible{true}, popup{}, keyboard{true}, mouse{true}, foreign{};
        int x{}, y{}, width{800}, height{600};
        int paints{}, repaints{};
    };
    std::map<H, Record> records;
    H next_handle = 100;
    H focused{};
    int allocations{}, frees{}, fail_allocation{}, configuration{}, invalid_free{}, fail_interface{};
    int panel_service{}, gui_service{}, scheme_service{};
    H actual_scheme = 37;
    using Factory = void* (*)(const char*, int*);
    Factory engine_factory{};
    int shutdown_count{};
    int extra_services[7]{};
    bool native_alive{true};
    bool invalidate_on_shutdown{};
    int post_shutdown_calls{};
    std::vector<Callback*> invalidated_callbacks;
    void RequireNative()
    {
        if (!native_alive)
        {
            ++post_shutdown_calls;
            throw std::runtime_error("native panel allocator already ended");
        }
    }
    int parent_calls{}, free_calls{}, throw_parent{}, throw_free{};
    void (*init_hook)(){};
    void (*shutdown_hook)(){};
    void (*allocate_hook)(){};
    void (*hook)(){};
    Record& At(H value)
    {
        RequireNative();
        return records.at(value);
    }
    H Number(void* pointer)
    {
        return static_cast<H>(reinterpret_cast<uintptr_t>(pointer));
    }
    H Allocate()
    {
        RequireNative();
        ++allocations;
        if (allocate_hook)
        {
            allocate_hook();
        }
        if (allocations == fail_allocation)
        {
            return 0;
        }
        const auto value = next_handle++;
        records.emplace(value, Record{});
        return value;
    }
    void SetParent(H value, H parent)
    {
        auto& node = At(value);
        if (node.parent)
        {
            auto& children = At(node.parent).children;
            children.erase(std::remove(children.begin(), children.end(), value), children.end());
        }
        node.parent = parent;
        if (parent)
        {
            At(parent).children.push_back(value);
            if (At(parent).callback)
            {
                At(parent).callback->Child(value);
            }
        }
    }
    void Bounds(H value, int& left, int& top, int& right, int& bottom)
    {
        auto& node = At(value);
        left = node.x;
        top = node.y;
        right = left + node.width;
        bottom = top + node.height;
        if (node.parent)
        {
            int pl{}, pt{}, pr{}, pb{};
            Bounds(node.parent, pl, pt, pr, pb);
            left += pl;
            top += pt;
            right = std::min(right + pl, pr);
            bottom = std::min(bottom + pt, pb);
        }
    }
} // namespace

extern "C" void FixtureNativePanelInvalidate();

extern "C" void FixturePanelReset()
{
    native_alive = true;
    invalidate_on_shutdown = false;
    post_shutdown_calls = 0;
    invalidated_callbacks.clear();
    records.clear();
    next_handle = 100;
    focused = 0;
    allocations = frees = fail_allocation = configuration = invalid_free = fail_interface = 0;
    actual_scheme = 37;
    hook = nullptr;
    parent_calls = free_calls = throw_parent = throw_free = 0;
    engine_factory = nullptr;
    shutdown_count = 0;
    init_hook = shutdown_hook = allocate_hook = nullptr;
}
extern "C" void FixturePanelFailAllocation(int value)
{
    fail_allocation = value;
}
extern "C" void FixturePanelFailInterface(int value)
{
    fail_interface = value;
}
extern "C" void FixturePanelScheme(H value)
{
    actual_scheme = value;
}
extern "C" void FixturePanelHook(void (*value)())
{
    hook = value;
}
extern "C" int FixturePanelCount(int kind)
{
    if (kind == 0)
    {
        return allocations;
    }
    if (kind == 1)
    {
        return frees;
    }
    if (kind == 2)
    {
        return invalid_free;
    }
    return static_cast<int>(records.size());
}
extern "C" H FixturePanelBorrow(H parent, int popup)
{
    const auto value = Allocate();
    At(value).foreign = true;
    At(value).popup = popup != 0;
    SetParent(value, parent);
    return value;
}
extern "C" H FixturePanelParent(H value)
{
    return At(value).parent;
}
extern "C" int FixturePanelPaints(H value)
{
    return At(value).paints;
}
extern "C" int FixturePanelRepaints(H value)
{
    return At(value).repaints;
}
extern "C" void FixturePanelFlags(H value, int visible, int keyboard, int mouse)
{
    At(value).visible = visible != 0;
    At(value).keyboard = keyboard != 0;
    At(value).mouse = mouse != 0;
}
extern "C" void FixturePanelPaint(H value)
{
    At(value).callback->Paint(false, true);
}
extern "C" H FixturePanelHit(H value, int x, int y, int popups)
{
    return At(value).callback->Hit(x, y, popups != 0);
}
extern "C" void FixturePanelFocus(H value, int direction)
{
    At(value).callback->Focus(direction);
}
extern "C" H FixturePanelCurrentFocus(H value)
{
    return At(value).callback->CurrentFocus();
}
extern "C" int FixturePanelMoveFocus(H value, H existing, int previous)
{
    return previous ? At(value).callback->Prev(existing) : At(value).callback->Next(existing);
}
extern "C" void FixturePanelMessage(H value, const char* name)
{
    At(value).callback->Message(name, 0);
}
extern "C" void FixturePanelDelete(H value)
{
    At(value).callback->Delete();
}
extern "C" int FixturePanelProtocol(H value)
{
    auto* cb = At(value).callback;
    int l = 9, t = 9, r = 9, b = 9;
    cb->Think();
    cb->Scheme();
    cb->Repaint();
    cb->Inset(l, t, r, b);
    if (l || t || r || b)
    {
        return 0;
    }
    cb->Clip(l, t, r, b);
    if (l != 0 || t != 0 || r != At(value).width || b != At(value).height)
    {
        return 0;
    }
    cb->Child(0);
    cb->Size(At(value).width, At(value).height);
    cb->FocusChanged(false);
    cb->Message("Unrecognized", 99);
    return cb->GetVPanel() == value && !cb->Info(nullptr) && cb->Tab() == 0 && cb->Name() &&
           std::strcmp(cb->ClassName(), "CSRETROEngineContainer") == 0 && cb->DefaultScheme() == actual_scheme && !cb->Proportional() &&
           !cb->AutoDelete() && !cb->Query(0) && !cb->Panel() && std::strcmp(cb->Module(), "CSRETROEngine") == 0;
}

extern "C" void* CreateInterface(const char* name, int*)
{
    if (std::strcmp(name, "VGUI_Panel007") == 0)
    {
        return fail_interface == 1 ? nullptr : &panel_service;
    }
    if (std::strcmp(name, "VGUI_ivgui006") == 0)
    {
        return fail_interface == 2 ? nullptr : &gui_service;
    }
    if (std::strcmp(name, "VGUI_Scheme009") == 0)
    {
        return fail_interface == 3 ? nullptr : &scheme_service;
    }
    const char* names[] = {"KeyValues003", "VGUI_Input004", "VGUI_InputInternal001", "VGUI_System009", "VGUI_Localize003"};
    for (int i = 0; i < 5; ++i)
    {
        if (std::strcmp(name, names[i]) == 0)
        {
            return &extra_services[i];
        }
    }
    return nullptr;
}

#define NATIVE(result, name, args, symbol)                                         \
    extern "C" __attribute__((visibility("hidden"))) result name args asm(symbol); \
    extern "C" result name args

NATIVE(H, NativeAllocate, (void*), "_ZN5CVGui10AllocPanelEv")
{
    return Allocate();
}
NATIVE(void, NativeFree, (void*, H value), "_ZN5CVGui9FreePanelEj")
{
    ++free_calls;
    if (free_calls == throw_free)
    {
        throw std::runtime_error("synthetic FreePanel failure");
    }
    auto& node = At(value);
    if (node.foreign || node.parent || !node.children.empty())
    {
        ++invalid_free;
    }
    if (hook)
    {
        hook();
    }
    records.erase(value);
    ++frees;
}
NATIVE(void, NativeInit, (void* panel, Callback* callback), "_ZN5vgui26VPanel4InitEPNS_12IClientPanelE")
{
    At(Number(panel)).callback = callback;
}
NATIVE(void, NativeParent, (void*, H value, H parent), "_ZN13VPanelWrapper9SetParentEjj")
{
    ++parent_calls;
    if (parent_calls == throw_parent)
    {
        throw std::runtime_error("synthetic SetParent failure");
    }
    SetParent(value, parent);
}
NATIVE(int, NativeChildren, (void*, H value), "_ZN13VPanelWrapper13GetChildCountEj")
{
    return At(value).children.size();
}
NATIVE(H, NativeChild, (void*, H value, int index), "_ZN13VPanelWrapper8GetChildEji")
{
    return At(value).children.at(index);
}
NATIVE(bool, NativeVisible, (void*, H value), "_ZN13VPanelWrapper9IsVisibleEj")
{
    return At(value).visible;
}
NATIVE(bool, NativePopup, (void*, H value), "_ZN13VPanelWrapper7IsPopupEj")
{
    return At(value).popup;
}
NATIVE(bool, NativeKeyboard, (void*, H value), "_ZN13VPanelWrapper22IsKeyBoardInputEnabledEj")
{
    return At(value).keyboard;
}
NATIVE(bool, NativeMouse, (void*, H value), "_ZN13VPanelWrapper19IsMouseInputEnabledEj")
{
    return At(value).mouse;
}
NATIVE(void, NativeSetVisible, (void*, H value, bool state), "_ZN13VPanelWrapper10SetVisibleEjb")
{
    At(value).visible = state;
}
NATIVE(void, NativeSetKeyboard, (void*, H value, bool state), "_ZN13VPanelWrapper23SetKeyBoardInputEnabledEjb")
{
    At(value).keyboard = state;
}
NATIVE(void, NativeSetMouse, (void*, H value, bool state), "_ZN13VPanelWrapper20SetMouseInputEnabledEjb")
{
    At(value).mouse = state;
}
NATIVE(void, NativePosition, (void*, H value, int x, int y), "_ZN13VPanelWrapper6SetPosEjii")
{
    At(value).x = x;
    At(value).y = y;
}
NATIVE(void, NativeSize, (void*, H value, int width, int height), "_ZN13VPanelWrapper7SetSizeEjii")
{
    At(value).width = width;
    At(value).height = height;
    if (At(value).callback)
    {
        At(value).callback->Size(width, height);
    }
    ++configuration;
    if (hook)
    {
        hook();
    }
}
NATIVE(void, NativeSolve, (void*, H), "_ZN13VPanelWrapper5SolveEj")
{
    RequireNative();
}
NATIVE(void, NativeClip, (void* value, int& left, int& top, int& right, int& bottom), "_ZN5vgui26VPanel11GetClipRectERiS1_S1_S1_")
{
    Bounds(Number(value), left, top, right, bottom);
}
NATIVE(void, NativePaint, (void*, H value, bool force, bool allow), "_ZN13VPanelWrapper13PaintTraverseEjbb")
{
    if (At(value).foreign)
    {
        ++At(value).paints;
    }
    else
    {
        At(value).callback->Paint(force, allow);
    }
    if (hook)
    {
        hook();
    }
}
NATIVE(void, NativeRepaint, (void*, H value), "_ZN13VPanelWrapper7RepaintEj")
{
    if (At(value).foreign)
    {
        ++At(value).repaints;
    }
    else
    {
        At(value).callback->Repaint();
    }
}
NATIVE(H, NativeHit, (void*, H value, int x, int y, bool popups), "_ZN13VPanelWrapper16IsWithinTraverseEjiib")
{
    if (!At(value).foreign)
    {
        return At(value).callback->Hit(x, y, popups);
    }
    int l{}, t{}, r{}, b{};
    Bounds(value, l, t, r, b);
    return At(value).mouse && x >= l && x < r && y >= t && y < b ? value : 0;
}
NATIVE(void, NativeFocus, (void*, H value, int direction), "_ZN13VPanelWrapper12RequestFocusEji")
{
    if (At(value).foreign)
    {
        focused = value;
    }
    else
    {
        At(value).callback->Focus(direction);
    }
}
NATIVE(bool, NativePrev, (void*, H value, H existing), "_ZN13VPanelWrapper16RequestFocusPrevEjj")
{
    if (!At(value).foreign)
    {
        return At(value).callback->Prev(existing);
    }
    if (value == existing)
    {
        return false;
    }
    focused = value;
    return true;
}
NATIVE(bool, NativeNext, (void*, H value, H existing), "_ZN13VPanelWrapper16RequestFocusNextEjj")
{
    if (!At(value).foreign)
    {
        return At(value).callback->Next(existing);
    }
    if (value == existing)
    {
        return false;
    }
    focused = value;
    return true;
}
NATIVE(H, NativeCurrentFocus, (void*, H value), "_ZN13VPanelWrapper18GetCurrentKeyFocusEj")
{
    return At(value).foreign ? (focused == value ? value : 0) : At(value).callback->CurrentFocus();
}
NATIVE(H, NativeDefaultScheme, (void*), "_ZN14CSchemeManager16GetDefaultSchemeEv")
{
    RequireNative();
    return actual_scheme;
}
NATIVE(const char*, NativeMessageName, (const void* message), "_ZNK9KeyValues7GetNameEv")
{
    RequireNative();
    return static_cast<const char*>(message);
}


extern "C" void* FixtureNativePanelQueryEngine(const char* name)
{
    return engine_factory ? engine_factory(name, nullptr) : nullptr;
}
extern "C" int FixtureNativePanelShutdownCount()
{
    return shutdown_count;
}
extern "C" void FixtureNativePanelInitHook(void (*value)())
{
    init_hook = value;
}
extern "C" void FixtureNativePanelShutdownHook(void (*value)())
{
    shutdown_hook = value;
}
extern "C" void FixtureNativePanelAllocHook(void (*value)())
{
    allocate_hook = value;
}
NATIVE(bool, NativeGuiInit, (void*, Factory* factories, int count), "_ZN5CVGui4InitEPPFPvPKcPiEi")
{
    engine_factory = factories && count ? factories[0] : nullptr;
    if (!engine_factory || !engine_factory("VGUI_Surface026", nullptr) || !engine_factory("VFileSystem009", nullptr))
    {
        return false;
    }
    if (init_hook)
    {
        init_hook();
    }
    return true;
}
NATIVE(void, NativeGuiShutdown, (void*), "_ZN5CVGui8ShutdownEv")
{
    ++shutdown_count;
    if (invalidate_on_shutdown)
    {
        FixtureNativePanelInvalidate();
    }
    if (shutdown_hook)
    {
        shutdown_hook();
    }
}

extern "C" void FixturePanelThrowParent(int relative)
{
    throw_parent = parent_calls + relative;
}
extern "C" void FixturePanelThrowFree(int relative)
{
    throw_free = free_calls + relative;
}
extern "C" int FixturePanelFreeCalls()
{
    return free_calls;
}


extern "C" void FixtureNativePanelInvalidate()
{
    for (const auto& entry : records)
    {
        if (entry.second.callback)
        {
            invalidated_callbacks.push_back(entry.second.callback);
        }
    }
    records.clear();
    native_alive = false;
}
extern "C" int FixtureNativePanelPostShutdownCalls()
{
    return post_shutdown_calls;
}
extern "C" void FixtureNativePanelInvalidateOnShutdown(int enabled)
{
    invalidate_on_shutdown = enabled != 0;
}
extern "C" int FixtureNativePanelProbeInvalidated()
{
    for (auto* cb : invalidated_callbacks)
    {
        int l = 7, t = 7, r = 7, b = 7;
        if (cb->GetVPanel())
        {
            return 0;
        }
        cb->Think();
        cb->Scheme();
        cb->Paint(true, true);
        cb->Repaint();
        if (cb->Hit(2, 3, true))
        {
            return 0;
        }
        cb->Inset(l, t, r, b);
        cb->Clip(l, t, r, b);
        if (l || t || r || b)
        {
            return 0;
        }
        cb->Child(0);
        cb->Size(1, 1);
        cb->FocusChanged(false);
        if (cb->Info(nullptr))
        {
            return 0;
        }
        cb->Focus(1);
        if (cb->Prev(0) || cb->Next(0))
        {
            return 0;
        }
        cb->Message("Delete", 0);
        if (cb->CurrentFocus() || cb->DefaultScheme())
        {
            return 0;
        }
        if (!cb->Name() || !cb->ClassName() || !cb->Module())
        {
            return 0;
        }
        cb->Tab();
        cb->Proportional();
        cb->AutoDelete();
        cb->Delete();
        if (cb->Query(0) || cb->Panel())
        {
            return 0;
        }
    }
    return 1;
}

extern "C" void FixturePanelInvalidateOnShutdown(int enabled)
{
    FixtureNativePanelInvalidateOnShutdown(enabled);
}
extern "C" int FixturePanelPostShutdownCalls()
{
    return FixtureNativePanelPostShutdownCalls();
}
extern "C" int FixturePanelParentCalls()
{
    return parent_calls;
}
