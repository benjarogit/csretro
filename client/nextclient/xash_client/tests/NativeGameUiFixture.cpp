// Synthetic GameUI/console implementation; shares the native VGUI fixture.
#include <goldsrc/client_api.h>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cstring>

extern "C" void* NativePanelCreateInterface(const char*, int*);
extern "C" void* NativeSdlCreateInterface(const char*, int*);
extern "C" {
void* g_pTaskbar = nullptr;
}
extern "C" void FixtureGameUiTaskbar(void* value)
{
    g_pTaskbar = value;
}
extern "C" uint32_t FixtureNativePanelControl(uint32_t, const char*, int);
extern "C" int FixtureNativePanelAlive(uint32_t);
extern "C" void FixturePanelReset();
extern "C" void NativeParent(void*, uint32_t, uint32_t) asm("_ZN13VPanelWrapper9SetParentEjj");
extern "C" __attribute__((visibility("hidden"))) void* NativeGameUiBase asm("_ZL11staticPanel");
void* NativeGameUiBase{};
namespace native_gameui_fixture
{
    class Engine
    {
    public:
        virtual ~Engine() = default;
        virtual uint32_t GetPanel(int) = 0;
        virtual bool Login(const char*, bool) = 0;
        virtual bool Process(bool*, void*, void*) = 0;
    };
} // namespace native_gameui_fixture
namespace
{
    using Factory = void* (*)(const char*, int*);
    struct Control
    {
        uint32_t handle;
    };
    int game_token{}, console_token{}, mode{}, client_export_queries{}, shutdown_calls{}, frame_calls{};
    bool started{}, console_started{}, game_visible{}, console_visible{}, exclusive_input{};
    int query_calls{};
    goldsrc::cl_enginefunc_t* engine_table{};
    void* system_pointer{};
    uint32_t container{}, console_panel{}, taskbar{}, survivor{};
    std::vector<int> order;
    std::map<uint32_t, std::unique_ptr<Control>> controls;
    void (*callback)(){};
    Control* NewControl(uint32_t parent, bool auto_delete = true)
    {
        const auto handle = FixtureNativePanelControl(parent, "GameUI", auto_delete);
        auto control = std::make_unique<Control>();
        control->handle = handle;
        auto* pointer = control.get();
        controls.emplace(handle, std::move(control));
        return pointer;
    }
} // namespace
extern "C" void FixtureGameUiReset()
{
    FixturePanelReset();
    controls.clear();
    NativeGameUiBase = nullptr;
    g_pTaskbar = nullptr;
    mode = client_export_queries = shutdown_calls = frame_calls = 0;
    started = console_started = game_visible = console_visible = exclusive_input = false;
    query_calls = 0;
    engine_table = nullptr;
    system_pointer = nullptr;
    container = console_panel = taskbar = survivor = 0;
    order.clear();
    callback = nullptr;
}
extern "C" void FixtureGameUiMode(int value)
{
    mode = value;
}
extern "C" void FixtureGameUiHook(void (*value)())
{
    callback = value;
}
extern "C" int FixtureGameUiCount(int kind)
{
    if (kind == 0)
    {
        return shutdown_calls;
    }
    if (kind == 1)
    {
        return frame_calls;
    }
    if (kind == 2)
    {
        return client_export_queries;
    }
    return static_cast<int>(order.size());
}
extern "C" int FixtureGameUiOrder(int index)
{
    return order.at(index);
}
extern "C" uint32_t FixtureGameUiPanel(int kind)
{
    if (kind == 0)
    {
        return NativeGameUiBase ? static_cast<Control*>(NativeGameUiBase)->handle : 0;
    }
    if (kind == 1)
    {
        return taskbar;
    }
    if (kind == 2)
    {
        return console_panel;
    }
    return survivor;
}
extern "C" void* FixtureGameUiEngine()
{
    return engine_table;
}
extern "C" void* FixtureGameUiSystem()
{
    return system_pointer;
}
extern "C" void* CreateInterface(const char* name, int* status)
{
    if (std::strcmp(name, "GameUI007") == 0)
    {
        return mode == 1 ? nullptr : &game_token;
    }
    if (std::strcmp(name, "GameConsole003") == 0)
    {
        return mode == 2 ? nullptr : &console_token;
    }
    if (std::strcmp(name, "VGUI_InputInternal001") == 0)
    {
        return NativeSdlCreateInterface(name, status);
    }
    return NativePanelCreateInterface(name, status);
}
#define GAME(result, name, args, symbol)                                           \
    extern "C" __attribute__((visibility("hidden"))) result name args asm(symbol); \
    extern "C" result name args
GAME(void, GameInitialize, (void*, Factory* factories, int count), "_ZN7CGameUI10InitializeEPPFPvPKcPiEi")
{
    order.push_back(1);
    if (count < 4 || !factories[0] || !factories[1] || !factories[2])
    {
        throw std::runtime_error("invalid factories");
    }
    auto* native_engine = static_cast<native_gameui_fixture::Engine*>(factories[0]("VEngineVGui001", nullptr));
    if (!native_engine || !factories[0]("BaseUI001", nullptr))
    {
        throw std::runtime_error("missing engine services");
    }
    if (count > 5 && factories[5])
    {
        if (factories[5]("GameClientExports001", nullptr))
        {
            ++client_export_queries;
        }
    }
    container = native_engine->GetPanel(2);
    NativeGameUiBase = NewControl(container);
    if (callback)
    {
        callback();
    }
    if (mode == 3)
    {
        throw std::runtime_error("synthetic initialization failure");
    }
}
GAME(
    void,
    GameStart,
    (void*, goldsrc::cl_enginefunc_t* engine, int version, void* system),
    "_ZN7CGameUI5StartEP16cl_enginefuncs_siP11IBaseSystem"
)
{
    if (!NativeGameUiBase || !engine || !system || version != 7)
    {
        throw std::runtime_error("invalid GameUI startup services");
    }
    order.push_back(2);
    engine_table = engine;
    system_pointer = system;
    taskbar = NewControl(static_cast<Control*>(NativeGameUiBase)->handle)->handle;
    survivor = NewControl(taskbar, false)->handle;
    if (callback)
    {
        callback();
    }
    if (mode == 4)
    {
        throw std::runtime_error("synthetic startup failure");
    }
    started = true;
}
GAME(void, GameShutdown, (void*), "_ZN7CGameUI8ShutdownEv")
{
    order.push_back(7);
    ++shutdown_calls;
    if (callback)
    {
        callback();
    }
    if (mode == 5)
    {
        throw std::runtime_error("synthetic shutdown failure");
    }
}
GAME(int, GameActivate, (void*), "_ZN7CGameUI14ActivateGameUIEv")
{
    order.push_back(4);
    game_visible = true;
    if (callback)
    {
        callback();
    }
    return 1;
}
GAME(void, GameHide, (void*), "_ZN7CGameUI10HideGameUIEv")
{
    order.push_back(5);
    game_visible = false;
}
GAME(void, GameFrame, (void*), "_ZN7CGameUI8RunFrameEv")
{
    if (!started)
    {
        throw std::runtime_error("frame before startup");
    }
    ++frame_calls;
    if (callback)
    {
        callback();
    }
}
GAME(bool, GameActive, (void*), "_ZN7CGameUI14IsGameUIActiveEv")
{
    ++query_calls;
    if (mode == 6)
    {
        throw std::runtime_error("synthetic state query failure");
    }
    if (callback)
    {
        callback();
    }
    return game_visible;
}
GAME(void, ConsoleInitialize, (void*), "_ZN12CGameConsole10InitializeEv")
{
    if (!started || !engine_table)
    {
        throw std::runtime_error("console before GameUI startup");
    }
    order.push_back(3);
    console_panel = NewControl(taskbar)->handle;
    console_started = true;
    if (callback)
    {
        callback();
    }
}
GAME(void, ConsoleActivate, (void*), "_ZN12CGameConsole8ActivateEv")
{
    if (!console_started)
    {
        throw std::runtime_error("console not initialized");
    }
    order.push_back(6);
    console_visible = true;
}
GAME(void, ConsoleHide, (void*), "_ZN12CGameConsole4HideEv")
{
    order.push_back(8);
    console_visible = false;
}
GAME(void, ConsoleParent, (void*, int parent), "_ZN12CGameConsole9SetParentEi")
{
    auto* panels = NativePanelCreateInterface("VGUI_Panel007", nullptr);
    NativeParent(panels, console_panel, static_cast<uint32_t>(parent));
}
GAME(const char*, GameModuleName, (), "_ZN5vgui221GetControlsModuleNameEv")
{
    return "GameUI";
}
GAME(uint32_t, GamePanelHandle, (void* panel), "_ZN5vgui25Panel9GetVPanelEv")
{
    return static_cast<Control*>(panel)->handle;
}


extern "C" void FixtureGameUiVisibility(int active, int console, int exclusive)
{
    game_visible = active != 0;
    console_visible = console != 0;
    exclusive_input = exclusive != 0;
}
extern "C" int FixtureGameUiQueryCount()
{
    return query_calls;
}
GAME(int, GameExclusiveInput, (void*), "_ZN7CGameUI17HasExclusiveInputEv")
{
    ++query_calls;
    if (mode == 6)
    {
        throw std::runtime_error("synthetic exclusive-input query failure");
    }
    if (callback)
    {
        callback();
    }
    return exclusive_input ? 1 : 0;
}
GAME(bool, ConsoleVisible, (void*), "_ZN12CGameConsole16IsConsoleVisibleEv")
{
    ++query_calls;
    if (mode == 6)
    {
        throw std::runtime_error("synthetic console query failure");
    }
    if (callback)
    {
        callback();
    }
    return console_visible;
}
