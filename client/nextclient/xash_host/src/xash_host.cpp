#include "xash_host/xash_host.h"

#include "host_filesystem.h"
#include "xash_surface/xash_surface.h"
#include "xash_engine/xash_engine.h"

#include <tier1/interface.h>

#include <next_engine_mini/engine_mini.h>

#include <goldsrc/client_api.h>

#include <FileSystem.h>

#include <IGameUIFuncs.h>

#include <vgui/IClientPanel.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>

#include <dlfcn.h>

#include <cstdint>
#include <cstring>

namespace
{

    struct SurfaceApi
    {
        void* module = nullptr;
        csr_surface_t* surface = nullptr;
        csr_surface_t* (*create)(const csr_surface_host_t*) = nullptr;
        void (*destroy)(csr_surface_t*) = nullptr;
        void (*move)(csr_surface_t*, int, int) = nullptr;
        void (*button)(csr_surface_t*, int, int) = nullptr;
        void (*set_input)(csr_surface_t*, const csr_surface_input_fns_t*) = nullptr;
        void (*set_file)(csr_surface_t*, const csr_surface_file_fns_t*) = nullptr;
        void (*set_panel)(csr_surface_t*, const csr_surface_panel_fns_t*) = nullptr;
        void (*begin_frame)(csr_surface_t*, int, int) = nullptr;
        int (*push_count)(const csr_surface_t*) = nullptr;
        int (*textured_quad_count)(const csr_surface_t*) = nullptr;
        void* (*create_interface)(const char*, int*) = nullptr;
    };

    SurfaceApi g_surface;
    csr_host_vgui_t g_vgui{};
    csr_host_factory_fn g_list[2 + CSR_HOST_EXTRA_FACTORY_CAP]{};
    csr_host_factory_fn g_extra[CSR_HOST_EXTRA_FACTORY_CAP]{};
    int g_extra_count = 0;
    int g_count = 0;

    struct PairApi
    {
        void* client = nullptr;
        void* vgui = nullptr;
        void* (*create)(const char*, int*) = nullptr;
        void (*dispatch)() = nullptr;
        void* scheme = nullptr;
    };

    PairApi g_pair;
    int g_vgui_connected = 0;
    int g_scheme_loaded = 0;
    int g_gameui_initialized = 0;
    int g_gameui_started = 0;
    int g_engine_commands = 0;
    char g_last_client_command[128] = {};
    goldsrc::cl_enginefunc_t g_engine_funcs{};

    int AddCommand(const char* name, void (*)())
    {
        if (name == nullptr || name[0] == '\0')
        {
            return 0;
        }
        ++g_engine_commands;
        return 1;
    }

    float CvarFloat(const char*)
    {
        return 0.f;
    }

    char* CvarString(const char*)
    {
        static char empty[] = "";
        return empty;
    }

    const char* GameDirectory()
    {
        return "";
    }

    const char* LevelName()
    {
        return "";
    }

    const char* LocalInfo(const char*)
    {
        return "";
    }

    const char* PlayerInfo(int, const char*)
    {
        return "";
    }

    void SetPlayerInfo(const char*, const char*)
    {
    }

    int ClientCmd(const char* command)
    {
        g_last_client_command[0] = '\0';
        if (command != nullptr)
        {
            std::size_t index = 0;
            for (; command[index] != '\0' && index + 1 < sizeof(g_last_client_command); ++index)
            {
                g_last_client_command[index] = command[index];
            }
            g_last_client_command[index] = '\0';
        }
        return 0;
    }

    void CvarSet(const char*, const char*)
    {
    }

    void CvarSetValue(const char*, float)
    {
    }

    double AbsoluteTime()
    {
        return 0.0;
    }

    int MaxClients()
    {
        return 0;
    }

    std::int32_t RandomLong(std::int32_t, std::int32_t)
    {
        return 0;
    }

    char* ParseEngineFile(const char* data, const char* token)
    {
        int code = 1;
        auto* files = static_cast<IFileSystem*>(csr_host_filesystem_factory("VFileSystem009", &code));
        if (files == nullptr || token == nullptr)
        {
            return nullptr;
        }
        bool quoted = false;
        return files->ParseFile(const_cast<char*>(data), const_cast<char*>(token), &quoted);
    }

    void FillEngineFuncs()
    {
        g_engine_funcs = {};
        g_engine_funcs.pfnAddCommand = AddCommand;
        g_engine_funcs.pfnGetCvarFloat = CvarFloat;
        g_engine_funcs.pfnGetCvarString = CvarString;
        g_engine_funcs.pfnGetCvarPointer = [](const char*) -> goldsrc::cvar_t* { return nullptr; };
        g_engine_funcs.pfnGetGameDirectory = GameDirectory;
        g_engine_funcs.pfnGetLevelName = LevelName;
        g_engine_funcs.LocalPlayerInfo_ValueForKey = LocalInfo;
        g_engine_funcs.PlayerInfo_ValueForKey = PlayerInfo;
        g_engine_funcs.PlayerInfo_SetValueForKey = SetPlayerInfo;
        g_engine_funcs.pfnClientCmd = ClientCmd;
        g_engine_funcs.Cvar_Set = CvarSet;
        g_engine_funcs.Cvar_SetValue = CvarSetValue;
        g_engine_funcs.GetAbsoluteTime = AbsoluteTime;
        g_engine_funcs.GetMaxClients = MaxClients;
        g_engine_funcs.pfnRandomLong = RandomLong;
        g_engine_funcs.COM_ParseFile = ParseEngineFile;
    }
    vgui2::IVGui* g_ivgui = nullptr;
    vgui2::VPANEL g_gameui_root = 0;

    // The engine root is a VPanel plus this client. SetParent asks the client
    // whether the parent is proportional. A panel with no client crashes there.
    class RootClient final : public vgui2::IClientPanel
    {
    public:
        vgui2::VPANEL GetVPanel() override { return panel_; }
        void Think() override {}
        void PerformApplySchemeSettings() override {}
        void PaintTraverse(bool repaint, bool allow_force) override
        {
            if (g_pair.create == nullptr || panel_ == 0)
            {
                return;
            }
            int code = 1;
            auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
            if (panels == nullptr)
            {
                return;
            }
            const int count = panels->GetChildCount(panel_);
            for (int index = 0; index < count; ++index)
            {
                const vgui2::VPANEL child = panels->GetChild(panel_, index);
                if (child != 0 && panels->IsVisible(child))
                {
                    panels->PaintTraverse(child, repaint, allow_force);
                }
            }
        }
        void Repaint() override {}
        vgui2::VPANEL IsWithinTraverse(int x, int y, bool traverse_popups) override
        {
            if (g_pair.create == nullptr || panel_ == 0)
            {
                return 0;
            }
            int code = 1;
            auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
            if (panels == nullptr || !panels->IsVisible(panel_) || !panels->IsMouseInputEnabled(panel_))
            {
                return 0;
            }
            const int count = panels->GetChildCount(panel_);
            for (int index = count - 1; index >= 0; --index)
            {
                const vgui2::VPANEL child = panels->GetChild(panel_, index);
                if (child == 0 || (!traverse_popups && panels->IsPopup(child)))
                {
                    continue;
                }
                const vgui2::VPANEL hit = panels->IsWithinTraverse(child, x, y, traverse_popups);
                if (hit != 0)
                {
                    return hit;
                }
            }
            int origin_x = 0;
            int origin_y = 0;
            int wide = 0;
            int tall = 0;
            panels->GetAbsPos(panel_, origin_x, origin_y);
            panels->GetSize(panel_, wide, tall);
            if (x >= origin_x && y >= origin_y && x < origin_x + wide && y < origin_y + tall)
            {
                return panel_;
            }
            return 0;
        }
        void GetInset(int& top, int& left, int& right, int& bottom) override
        {
            top = left = right = bottom = 0;
        }
        void GetClipRect(int& x0, int& y0, int& x1, int& y1) override
        {
            x0 = y0 = x1 = y1 = 0;
            if (g_pair.create == nullptr || panel_ == 0)
            {
                return;
            }
            int code = 1;
            auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
            if (panels == nullptr)
            {
                return;
            }
            int wide = 0;
            int tall = 0;
            panels->GetSize(panel_, wide, tall);
            x1 = wide;
            y1 = tall;
        }
        void OnChildAdded(vgui2::VPANEL) override {}
        void OnSizeChanged(int, int) override {}
        void InternalFocusChanged(bool) override {}
        bool RequestInfo(KeyValues*) override { return false; }
        void RequestFocus(int) override {}
        bool RequestFocusPrev(vgui2::VPANEL) override { return false; }
        bool RequestFocusNext(vgui2::VPANEL) override { return false; }
        void OnMessage(const KeyValues*, vgui2::VPANEL) override {}
        vgui2::VPANEL GetCurrentKeyFocus() override { return 0; }
        int GetTabPosition() override { return 0; }
        const char* GetName() override { return "GameUIRoot"; }
        const char* GetClassName() override { return "HostPanel"; }
        vgui2::HScheme GetScheme() override { return 0; }
        bool IsProportional() override { return false; }
        bool IsAutoDeleteSet() override { return false; }
        void DeletePanel() override {}
        void* QueryInterface(vgui2::EInterfaceID) override { return nullptr; }
        vgui2::Panel* GetPanel() override { return nullptr; }
        const char* GetModuleName() override { return "xash_host"; }

        vgui2::VPANEL panel_ = 0;
    };

    RootClient g_root_client;

    // Same slots as IEngineVGui: two destructor entries, then GetPanel.
    class HostEngineVGui
    {
    public:
        virtual ~HostEngineVGui() = default;

        virtual vgui2::VPANEL GetPanel(int type)
        {
            return type == 2 ? g_gameui_root : 0;
        }
    };

    HostEngineVGui g_engine_vgui;

    // No keys are bound and no VAC server is connected. One mode keeps the
    // video page from walking an empty list into a null pointer.
    class HostGameUiFuncs final : public IGameUIFuncs
    {
    public:
        bool IsKeyDown(const char*, bool& down) override
        {
            down = false;
            return false;
        }

        const char* Key_NameForKey(EngineKey) override
        {
            return "";
        }

        const char* Key_BindingForKey(EngineKey) override
        {
            return "";
        }

        vgui2::KeyCode GetVGUI2KeyCodeForBind(const char*) override
        {
            return vgui2::KEY_NONE;
        }

        void GetVideoModes(vmode_t** modes, int* count) override
        {
            static vmode_t mode{640, 480, 32};
            if (modes != nullptr)
            {
                *modes = &mode;
            }
            if (count != nullptr)
            {
                *count = 1;
            }
        }

        void GetCurrentVideoMode(int* wide, int* tall, int* bpp) override
        {
            if (wide != nullptr)
            {
                *wide = 640;
            }
            if (tall != nullptr)
            {
                *tall = 480;
            }
            if (bpp != nullptr)
            {
                *bpp = 32;
            }
        }

        void GetCurrentRenderer(char* name, int name_len, int* windowed, int* hd, int* addons, int* level) override
        {
            if (name != nullptr && name_len > 0)
            {
                name[0] = '\0';
            }
            if (windowed != nullptr)
            {
                *windowed = 1;
            }
            if (hd != nullptr)
            {
                *hd = 0;
            }
            if (addons != nullptr)
            {
                *addons = 0;
            }
            if (level != nullptr)
            {
                *level = 0;
            }
        }

        bool IsConnectedToVACSecureServer() override
        {
            return false;
        }

        EngineKey Key_KeyStringToKeyNum(const char*) override
        {
            return -1;
        }
    };

    HostGameUiFuncs g_gameui_funcs;

    void* EngineVGuiFactory(const char* name, int* code)
    {
        void* found = nullptr;
        if (name != nullptr && std::strcmp(name, "VEngineVGui001") == 0)
        {
            found = &g_engine_vgui;
        }
        else if (name != nullptr && std::strcmp(name, IGAMEUIFUNCS_NAME) == 0)
        {
            found = &g_gameui_funcs;
        }
        if (found != nullptr)
        {
            if (code != nullptr)
            {
                *code = 0;
            }
            return found;
        }
        if (code != nullptr)
        {
            *code = 1;
        }
        return nullptr;
    }

    struct GameUiApi
    {
        void* module = nullptr;
        void* gameui = nullptr;
        void* console = nullptr;
    };

    GameUiApi g_gameui;

    struct EngineApi
    {
        void* nitro_module = nullptr;
        void* engine_module = nullptr;
        void* nitro = nullptr;
        EngineMiniInterface* engine = nullptr;
        int (*check_parm)(const char*) = nullptr;
        void (*read_startup)(int*, int*, int*, int*, int*) = nullptr;
        bool nitro_ready = false;
    };

    // Itanium: two destructor slots, then NitroApiInterface methods.
    // Initialize is the fourth method, UnInitialize the fifth.
    bool NitroInitialize(void* nitro)
    {
        auto** vtable = *reinterpret_cast<void***>(nitro);
        auto initialize = reinterpret_cast<bool (*)(void*, void*, void*, void*)>(vtable[5]);
        return initialize(nitro, nullptr, nullptr, nullptr);
    }

    void NitroUninitialize(void* nitro)
    {
        auto** vtable = *reinterpret_cast<void***>(nitro);
        auto uninitialize = reinterpret_cast<bool (*)(void*)>(vtable[6]);
        uninitialize(nitro);
    }

    EngineApi g_engine_api;
    int g_engine_loaded = 0;
    char g_engine_arg0[] = "xash";
    char g_engine_arg1[] = "-console";
    char g_engine_mode_line[] = "-windowed";
    char g_engine_init_line[] = "-game cstrike";
    int g_engine_window = 0;

    void CloseEngine()
    {
        if (g_engine_api.engine != nullptr)
        {
            g_engine_api.engine->Uninitialize();
        }
        if (g_engine_api.nitro_ready && g_engine_api.nitro != nullptr)
        {
            NitroUninitialize(g_engine_api.nitro);
        }
        g_engine_api.engine = nullptr;
        g_engine_api.nitro = nullptr;
        g_engine_api.nitro_ready = false;
        g_engine_api.check_parm = nullptr;
        g_engine_api.read_startup = nullptr;
        g_engine_loaded = 0;
        if (g_engine_api.engine_module != nullptr)
        {
            dlclose(g_engine_api.engine_module);
            g_engine_api.engine_module = nullptr;
        }
        if (g_engine_api.nitro_module != nullptr)
        {
            dlclose(g_engine_api.nitro_module);
            g_engine_api.nitro_module = nullptr;
        }
    }

    void CloseGameUi()
    {
        g_gameui.gameui = nullptr;
        g_gameui.console = nullptr;
        g_gameui_initialized = 0;
        g_gameui_started = 0;
        g_engine_commands = 0;
        g_last_client_command[0] = '\0';
        g_engine_funcs = {};
        if (g_gameui.module != nullptr)
        {
            dlclose(g_gameui.module);
        }
        g_gameui = {};
    }

    void ClosePair()
    {
        g_pair.dispatch = nullptr;
        g_pair.scheme = nullptr;
        g_pair.create = nullptr;
        g_vgui_connected = 0;
        g_scheme_loaded = 0;
        g_gameui_initialized = 0;
        g_gameui_started = 0;
        g_ivgui = nullptr;
        g_gameui_root = 0;
        g_root_client.panel_ = 0;
        // Drop the pair input bind before closing the modules that own it.
        if (g_surface.surface != nullptr && g_surface.set_input != nullptr)
        {
            g_surface.set_input(g_surface.surface, nullptr);
        }
        g_vgui = {};
        if (g_pair.vgui != nullptr)
        {
            dlclose(g_pair.vgui);
        }
        if (g_pair.client != nullptr)
        {
            dlclose(g_pair.client);
        }
        g_pair = {};
    }

    void OnCursorMoved(int x, int y, void* /*user*/)
    {
        if (g_vgui.cursor_moved != nullptr)
        {
            g_vgui.cursor_moved(g_vgui.input, x, y);
        }
    }

    void OnMousePressed(int code, void* /*user*/)
    {
        if (g_vgui.mouse_pressed != nullptr)
        {
            g_vgui.mouse_pressed(g_vgui.input, code);
        }
    }

    void OnMouseReleased(int code, void* /*user*/)
    {
        if (g_vgui.mouse_released != nullptr)
        {
            g_vgui.mouse_released(g_vgui.input, code);
        }
    }

    void OnDispatch(void* /*user*/)
    {
        if (g_vgui.dispatch != nullptr)
        {
            g_vgui.dispatch(g_vgui.vgui);
        }
    }

    void RealCursorMoved(void* input, int x, int y)
    {
        if (input == nullptr)
        {
            return;
        }
        static_cast<vgui2::IInputInternal*>(input)->InternalCursorMoved(x, y);
    }

    void RealMousePressed(void* input, int code)
    {
        if (input == nullptr)
        {
            return;
        }
        static_cast<vgui2::IInputInternal*>(input)->InternalMousePressed(static_cast<vgui2::MouseCode>(code));
    }

    void RealMouseReleased(void* input, int code)
    {
        if (input == nullptr)
        {
            return;
        }
        static_cast<vgui2::IInputInternal*>(input)->InternalMouseReleased(static_cast<vgui2::MouseCode>(code));
    }

    void RealDispatch(void*)
    {
        if (g_pair.dispatch != nullptr)
        {
            g_pair.dispatch();
        }
    }

    // The pair's input object. IInputInternal::RunFrame is not this call.
    int BindRealInput()
    {
        if (g_pair.create == nullptr || g_pair.dispatch == nullptr || g_surface.surface == nullptr || g_surface.set_input == nullptr)
        {
            return 0;
        }
        int code = 1;
        void* input = g_pair.create("VGUI_InputInternal001", &code);
        if (input == nullptr)
        {
            return 0;
        }
        g_vgui = {};
        g_vgui.input = input;
        g_vgui.cursor_moved = RealCursorMoved;
        g_vgui.mouse_pressed = RealMousePressed;
        g_vgui.mouse_released = RealMouseReleased;
        g_vgui.dispatch = RealDispatch;
        csr_surface_input_fns_t fns{};
        fns.cursor_moved = OnCursorMoved;
        fns.mouse_pressed = OnMousePressed;
        fns.mouse_released = OnMouseReleased;
        fns.dispatch = OnDispatch;
        g_surface.set_input(g_surface.surface, &fns);
        return 1;
    }

    template <typename T>
    T LoadSymbol(void* module, const char* name)
    {
        return reinterpret_cast<T>(dlsym(module, name));
    }

    void RebuildList()
    {
        std::memset(g_list, 0, sizeof(g_list));
        if (g_surface.create_interface == nullptr)
        {
            g_count = 0;
            return;
        }

        int index = 0;
        g_list[index++] = g_surface.create_interface;
        if (csr_host_filesystem_mounted() != 0)
        {
            g_list[index++] = csr_host_filesystem_factory;
        }
        for (int extra = 0; extra < g_extra_count; ++extra)
        {
            g_list[index++] = g_extra[extra];
        }
        g_count = index;
    }

    vgui2::VPANEL AsVPanel(void* panel)
    {
        return static_cast<vgui2::VPANEL>(reinterpret_cast<uintptr_t>(panel));
    }

    vgui2::IPanel* Panels()
    {
        if (g_pair.create == nullptr)
        {
            return nullptr;
        }
        int code = 1;
        return static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
    }

    int PanelVisible(void* panel, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return 0;
        }
        return panels->IsVisible(AsVPanel(panel)) ? 1 : 0;
    }

    int PanelPopup(void* panel, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return 0;
        }
        return panels->IsPopup(AsVPanel(panel)) ? 1 : 0;
    }

    int PanelChildCount(void* panel, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return 0;
        }
        return panels->GetChildCount(AsVPanel(panel));
    }

    void* PanelChildAt(void* panel, int index, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return nullptr;
        }
        const vgui2::VPANEL child = panels->GetChild(AsVPanel(panel), index);
        return reinterpret_cast<void*>(static_cast<uintptr_t>(child));
    }

    void PanelSolve(void* panel, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return;
        }
        panels->Solve(AsVPanel(panel));
    }

    void PanelScheme(void* panel, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return;
        }
        panels->PerformApplySchemeSettings(AsVPanel(panel));
    }

    void PanelPaint(void* panel, int repaint, int allow_force, void*)
    {
        vgui2::IPanel* panels = Panels();
        if (panels == nullptr || panel == nullptr)
        {
            return;
        }
        panels->PaintTraverse(AsVPanel(panel), repaint != 0, allow_force != 0);
    }

    void InstallPanelFns()
    {
        if (g_surface.surface == nullptr || g_surface.set_panel == nullptr)
        {
            return;
        }
        csr_surface_panel_fns_t fns{};
        fns.is_visible = PanelVisible;
        fns.is_popup = PanelPopup;
        fns.child_count = PanelChildCount;
        fns.child_at = PanelChildAt;
        fns.solve = PanelSolve;
        fns.apply_scheme = PanelScheme;
        fns.paint = PanelPaint;
        g_surface.set_panel(g_surface.surface, &fns);
    }

} // namespace

int csr_host_open(const char* path)
{
    csr_host_close();
    if (path == nullptr || path[0] == '\0')
    {
        return 0;
    }

    // RTLD_LOCAL keeps this CreateInterface from replacing the engine's.
    void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr)
    {
        return 0;
    }

    g_surface.module = module;
    g_surface.create = LoadSymbol<decltype(g_surface.create)>(module, "csr_surface_create");
    g_surface.destroy = LoadSymbol<decltype(g_surface.destroy)>(module, "csr_surface_destroy");
    g_surface.move = LoadSymbol<decltype(g_surface.move)>(module, "csr_surface_input_move");
    g_surface.button = LoadSymbol<decltype(g_surface.button)>(module, "csr_surface_input_button");
    g_surface.set_input = LoadSymbol<decltype(g_surface.set_input)>(module, "csr_surface_set_input");
    g_surface.set_file = LoadSymbol<decltype(g_surface.set_file)>(module, "csr_surface_set_file");
    g_surface.set_panel = LoadSymbol<decltype(g_surface.set_panel)>(module, "csr_surface_set_panel");
    g_surface.begin_frame = LoadSymbol<decltype(g_surface.begin_frame)>(module, "csr_surface_begin_frame");
    g_surface.push_count = LoadSymbol<decltype(g_surface.push_count)>(module, "csr_surface_push_count");
    g_surface.textured_quad_count =
        LoadSymbol<decltype(g_surface.textured_quad_count)>(module, "csr_surface_textured_quad_count");
    g_surface.create_interface = LoadSymbol<decltype(g_surface.create_interface)>(module, "csr_surface_create_interface");
    if (g_surface.create == nullptr || g_surface.destroy == nullptr || g_surface.move == nullptr || g_surface.button == nullptr ||
        g_surface.set_input == nullptr || g_surface.set_file == nullptr || g_surface.set_panel == nullptr ||
        g_surface.begin_frame == nullptr || g_surface.push_count == nullptr || g_surface.textured_quad_count == nullptr ||
        g_surface.create_interface == nullptr)
    {
        csr_host_close();
        return 0;
    }

    g_surface.surface = g_surface.create(nullptr);
    using HooksFn = void (*)(csr_surface_t*, csr_xash_engine_surface_t*);
    HooksFn hooks = LoadSymbol<HooksFn>(module, "csr_surface_engine_hooks");
    if (hooks != nullptr)
    {
        csr_xash_engine_surface_t engine_surface{};
        hooks(g_surface.surface, &engine_surface);
        csr_xash_engine_bind(&engine_surface);
    }
    RebuildList();
    return 1;
}

int csr_host_load_pair(const char* client_path, const char* vgui_path)
{
    ClosePair();
    if (g_surface.create_interface == nullptr || client_path == nullptr || client_path[0] == '\0' || vgui_path == nullptr ||
        vgui_path[0] == '\0')
    {
        return 0;
    }

    void* client = dlopen(client_path, RTLD_NOW | RTLD_LOCAL);
    if (client == nullptr)
    {
        return 0;
    }
    void* vgui = dlopen(vgui_path, RTLD_NOW | RTLD_LOCAL);
    if (vgui == nullptr)
    {
        dlclose(client);
        return 0;
    }

    auto dispatch = LoadSymbol<void (*)()>(vgui, CSR_VGUI_DISPATCH_SYMBOL);
    auto create = LoadSymbol<void* (*)(const char*, int*)>(vgui, "CreateInterface");
    auto client_create = LoadSymbol<void* (*)(const char*, int*)>(client, "CreateInterface");
    if (dispatch == nullptr || create == nullptr || client_create == nullptr)
    {
        dlclose(vgui);
        dlclose(client);
        return 0;
    }

    int code = 1;
    void* scheme = create("VGUI_Scheme009", &code);
    if (scheme == nullptr)
    {
        dlclose(vgui);
        dlclose(client);
        return 0;
    }

    g_pair.client = client;
    g_pair.vgui = vgui;
    g_pair.create = create;
    g_pair.dispatch = dispatch;
    g_pair.scheme = scheme;
    if (BindRealInput() == 0)
    {
        ClosePair();
        return 0;
    }
    return 1;
}

int csr_host_pair_loaded(void)
{
    return g_pair.client != nullptr && g_pair.vgui != nullptr ? 1 : 0;
}

void (*csr_host_vgui_dispatch(void))(void)
{
    return g_pair.dispatch;
}

void* csr_host_scheme(void)
{
    return g_pair.scheme;
}

int csr_host_load_gameui(const char* path)
{
    CloseGameUi();
    if (g_surface.create_interface == nullptr || path == nullptr || path[0] == '\0')
    {
        return 0;
    }

    void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr)
    {
        return 0;
    }

    auto create = LoadSymbol<void* (*)(const char*, int*)>(module, "CreateInterface");
    if (create == nullptr)
    {
        dlclose(module);
        return 0;
    }

    int code = 1;
    void* gameui = create("GameUI007", &code);
    void* console = create("GameConsole003", &code);
    if (gameui == nullptr || console == nullptr)
    {
        dlclose(module);
        return 0;
    }

    g_gameui.module = module;
    g_gameui.gameui = gameui;
    g_gameui.console = console;
    return 1;
}

int csr_host_gameui_loaded(void)
{
    return g_gameui.module != nullptr ? 1 : 0;
}

void* csr_host_gameui(void)
{
    return g_gameui.gameui;
}

void* csr_host_game_console(void)
{
    return g_gameui.console;
}

int csr_host_mount_filesystem(const char* root)
{
    if (g_surface.create_interface == nullptr)
    {
        return 0;
    }
    if (csr_host_filesystem_set_root(root) == 0)
    {
        return 0;
    }
    csr_surface_file_fns_t file{};
    file.read = [](const char* path, unsigned char* dest, int dest_cap, void*) {
        return csr_host_filesystem_read(path, dest, dest_cap);
    };
    if (g_surface.set_file != nullptr)
    {
        g_surface.set_file(g_surface.surface, &file);
    }
    RebuildList();
    return 1;
}

int csr_host_connect_vgui(void)
{
    if (g_vgui_connected != 0)
    {
        return 1;
    }
    if (g_pair.create == nullptr || g_surface.create_interface == nullptr || csr_host_filesystem_mounted() == 0)
    {
        return 0;
    }

    int code = 1;
    if (g_surface.create_interface("VGUI_Surface026", &code) == nullptr ||
        csr_host_filesystem_factory("VFileSystem009", &code) == nullptr ||
        g_pair.create("VGUI_Localize003", &code) == nullptr)
    {
        return 0;
    }

    auto* ivgui = static_cast<vgui2::IVGui*>(g_pair.create("VGUI_ivgui006", &code));
    if (ivgui == nullptr)
    {
        return 0;
    }

    // Init stores the surface, the filesystem and localize. RunFrame is not this call.
    CreateInterfaceFn list[3] = {
        reinterpret_cast<CreateInterfaceFn>(g_surface.create_interface),
        reinterpret_cast<CreateInterfaceFn>(csr_host_filesystem_factory),
        reinterpret_cast<CreateInterfaceFn>(g_pair.create),
    };
    if (!ivgui->Init(list, 3))
    {
        return 0;
    }
    g_ivgui = ivgui;
    g_vgui_connected = 1;
    return 1;
}

int csr_host_vgui_connected(void)
{
    return g_vgui_connected;
}

int csr_host_load_scheme(const char* path, const char* tag)
{
    if (g_scheme_loaded != 0)
    {
        return 1;
    }
    if (g_vgui_connected == 0 || g_pair.scheme == nullptr || path == nullptr || path[0] == '\0' || tag == nullptr ||
        tag[0] == '\0')
    {
        return 0;
    }
    if (csr_host_filesystem_read(path, nullptr, 0) <= 0)
    {
        return 0;
    }

    auto* manager = static_cast<vgui2::ISchemeManager*>(g_pair.scheme);
    const vgui2::HScheme handle = manager->LoadSchemeFromFile(path, tag);
    if (handle == 0)
    {
        return 0;
    }
    g_scheme_loaded = 1;
    return 1;
}

int csr_host_scheme_loaded(void)
{
    return g_scheme_loaded;
}

int csr_host_init_gameui(void)
{
    if (g_gameui_initialized != 0)
    {
        return 1;
    }
    if (g_vgui_connected == 0 || g_ivgui == nullptr || g_gameui.gameui == nullptr || g_pair.create == nullptr)
    {
        return 0;
    }

    int code = 1;
    if (g_pair.create("VGUI_SurfaceNext001", &code) == nullptr || g_pair.create("VGUI_InputInternal001", &code) == nullptr ||
        g_pair.create("VGUI_Panel007", &code) == nullptr || g_pair.create("VGUI_SchemeNext001", &code) == nullptr ||
        g_pair.create("VGUI_System009", &code) == nullptr || g_pair.create("KeyValues003", &code) == nullptr)
    {
        return 0;
    }

    if (g_gameui_root == 0)
    {
        g_gameui_root = g_ivgui->AllocPanel();
        g_root_client.panel_ = g_gameui_root;
        auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
        if (g_gameui_root == 0 || panels == nullptr)
        {
            g_gameui_root = 0;
            return 0;
        }
        panels->Init(g_gameui_root, &g_root_client);
    }

    CreateInterfaceFn list[4] = {
        reinterpret_cast<CreateInterfaceFn>(g_surface.create_interface),
        reinterpret_cast<CreateInterfaceFn>(csr_host_filesystem_factory),
        reinterpret_cast<CreateInterfaceFn>(g_pair.create),
        reinterpret_cast<CreateInterfaceFn>(EngineVGuiFactory),
    };
    // Itanium puts the complete and deleting destructor in the first two slots.
    // Initialize is next. IsGameUIActive follows the ten methods declared before it.
    // A typed call in this file would bind the empty base instead of CGameUI.
    auto** gameui_vtable = *reinterpret_cast<void***>(g_gameui.gameui);
    auto initialize = reinterpret_cast<void (*)(void*, CreateInterfaceFn*, int)>(gameui_vtable[2]);
    auto active = reinterpret_cast<bool (*)(void*)>(gameui_vtable[12]);
    initialize(g_gameui.gameui, list, 4);
    if (!active(g_gameui.gameui) || csr_host_gameui_child_count() < 1)
    {
        return 0;
    }
    g_gameui_initialized = 1;
    return 1;
}

int csr_host_gameui_child_count(void)
{
    if (g_gameui_root == 0 || g_pair.create == nullptr)
    {
        return 0;
    }
    int code = 1;
    auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
    if (panels == nullptr)
    {
        return 0;
    }
    return panels->GetChildCount(g_gameui_root);
}

int csr_host_gameui_base_child_count(void)
{
    if (g_gameui_root == 0 || g_pair.create == nullptr)
    {
        return 0;
    }
    int code = 1;
    auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
    if (panels == nullptr || panels->GetChildCount(g_gameui_root) < 1)
    {
        return 0;
    }
    const vgui2::VPANEL base = panels->GetChild(g_gameui_root, 0);
    return panels->GetChildCount(base);
}

int csr_host_gameui_initialized(void)
{
    return g_gameui_initialized;
}

int csr_host_start_gameui(void)
{
    if (g_gameui_started != 0)
    {
        return 1;
    }
    if (g_gameui_initialized == 0 || g_gameui.gameui == nullptr)
    {
        return 0;
    }

    // The table stays in this process. It does not open hw.so and it does not
    // run the commands it records. RunFrame is not this call.
    FillEngineFuncs();
    auto** gameui_vtable = *reinterpret_cast<void***>(g_gameui.gameui);
    auto start = reinterpret_cast<void (*)(void*, goldsrc::cl_enginefunc_t*, int, void*)>(gameui_vtable[3]);
    start(g_gameui.gameui, &g_engine_funcs, 0, nullptr);
    g_gameui_started = 1;
    return 1;
}

int csr_host_gameui_started(void)
{
    return g_gameui_started;
}

int csr_host_load_engine(const char* engine_path, const char* nitro_path, const char* basedir)
{
    CloseEngine();
    if (g_surface.create_interface == nullptr || engine_path == nullptr || engine_path[0] == '\0' || nitro_path == nullptr ||
        nitro_path[0] == '\0')
    {
        return 0;
    }

    void* nitro_module = dlopen(nitro_path, RTLD_NOW | RTLD_LOCAL);
    if (nitro_module == nullptr)
    {
        return 0;
    }
    auto nitro_create = LoadSymbol<void* (*)(const char*, int*)>(nitro_module, "CreateInterface");
    if (nitro_create == nullptr)
    {
        dlclose(nitro_module);
        return 0;
    }
    int code = 1;
    void* nitro = nitro_create("NITRO_API_001", &code);
    if (nitro == nullptr || !NitroInitialize(nitro))
    {
        dlclose(nitro_module);
        return 0;
    }

    void* engine_module = dlopen(engine_path, RTLD_NOW | RTLD_LOCAL);
    if (engine_module == nullptr)
    {
        NitroUninitialize(nitro);
        dlclose(nitro_module);
        return 0;
    }
    auto engine_create = LoadSymbol<void* (*)(const char*, int*)>(engine_module, "CreateInterface");
    auto check_parm = LoadSymbol<int (*)(const char*)>(engine_module, "EngineMini_CheckParm");
    auto read_startup = LoadSymbol<void (*)(int*, int*, int*, int*, int*)>(engine_module, "EngineMini_ReadXashStartup");
    if (engine_create == nullptr || check_parm == nullptr || read_startup == nullptr)
    {
        dlclose(engine_module);
        NitroUninitialize(nitro);
        dlclose(nitro_module);
        return 0;
    }
    code = 1;
    auto* engine = static_cast<EngineMiniInterface*>(engine_create(ENGINE_MINI_INTERFACE_VERSION, &code));
    if (engine == nullptr)
    {
        dlclose(engine_module);
        NitroUninitialize(nitro);
        dlclose(nitro_module);
        return 0;
    }

    NextClientVersion version{};
    engine->Init(nitro, version, nullptr);

    g_engine_api.nitro_module = nitro_module;
    g_engine_api.engine_module = engine_module;
    g_engine_api.nitro = nitro;
    g_engine_api.nitro_ready = true;
    g_engine_api.engine = engine;
    g_engine_api.check_parm = check_parm;
    g_engine_api.read_startup = read_startup;
    g_engine_loaded = 1;

    // Pointers stay in this process for COM_CheckParm. Shutdown and the fatal
    // error stay bound and are not called here. OnGameInitializing reads the
    // Xash table, runs Con_Init, then SetCStrikeFlags when pfnGetGameDir is
    // set from the mounted directory. A null pointer is not a call, and
    // Sys_Error is not called.
    char* argv[] = {g_engine_arg0, g_engine_arg1};
    csr_xash_engine_argv(2, argv);
    csr_xash_engine_mode(&g_engine_window, nullptr, nullptr, "gl", g_engine_mode_line, 1);
    csr_xash_engine_init_game(g_engine_init_line, const_cast<char*>(basedir), &g_engine_window, 0, 1);
    return 1;
}

int csr_host_engine_loaded(void)
{
    return g_engine_loaded;
}

int csr_host_engine_check_parm(const char* parm)
{
    if (g_engine_api.check_parm == nullptr)
    {
        return 0;
    }
    return g_engine_api.check_parm(parm);
}

int csr_host_engine_startup_calls(int* argv_calls, int* mode_calls, int* init_calls, int* shutdown_calls, int* error_calls)
{
    if (g_engine_api.read_startup == nullptr)
    {
        return 0;
    }
    g_engine_api.read_startup(argv_calls, mode_calls, init_calls, shutdown_calls, error_calls);
    return 1;
}

int csr_host_engine_command_count(void)
{
    return g_engine_commands;
}

const char* csr_host_last_client_command(void)
{
    return g_last_client_command;
}

int csr_host_activate_gameui(void)
{
    if (g_gameui_started == 0 || g_gameui.gameui == nullptr)
    {
        return 0;
    }
    // Slot 5 is ActivateGameUI. RunFrame is slot 8 and is not this call.
    auto** gameui_vtable = *reinterpret_cast<void***>(g_gameui.gameui);
    auto activate = reinterpret_cast<int (*)(void*)>(gameui_vtable[5]);
    return activate(g_gameui.gameui);
}

int csr_host_paint_gameui(int wide, int tall)
{
    if (g_gameui_initialized == 0 || g_gameui_root == 0 || wide <= 0 || tall <= 0 || g_surface.surface == nullptr ||
        g_surface.begin_frame == nullptr || g_surface.push_count == nullptr || g_surface.set_panel == nullptr ||
        g_surface.create_interface == nullptr || g_pair.create == nullptr)
    {
        return 0;
    }

    InstallPanelFns();
    g_surface.begin_frame(g_surface.surface, wide, tall);

    int code = 1;
    auto* panels = static_cast<vgui2::IPanel*>(g_pair.create("VGUI_Panel007", &code));
    auto* surface = static_cast<vgui2::ISurface*>(g_surface.create_interface("VGUI_Surface026", &code));
    if (panels == nullptr || surface == nullptr)
    {
        return 0;
    }

    panels->SetSize(g_gameui_root, wide, tall);
    // The base panel was laid out while the root was still 0x0. Size the
    // non-popup children here. PerformLayout is not this call: it runs the
    // startup mp3 command.
    const int children = panels->GetChildCount(g_gameui_root);
    for (int index = 0; index < children; ++index)
    {
        const vgui2::VPANEL child = panels->GetChild(g_gameui_root, index);
        if (child != 0 && !panels->IsPopup(child))
        {
            panels->SetSize(child, wide, tall);
        }
    }
    surface->SetEmbeddedPanel(g_gameui_root);
    void* input = g_pair.create("VGUI_InputInternal001", &code);
    if (input != nullptr)
    {
        static_cast<vgui2::IInputInternal*>(input)->AssociatePanelWithInputContext(
            DEFAULT_INPUT_CONTEXT, g_gameui_root);
    }
    const int before = g_surface.push_count(g_surface.surface);
    // GameUI loads this file during init and points panels at that scheme.
    // The placeholder scheme has no color data, so a missing file is not applied.
    // Panels keep the scheme flag until this solve. RunFrame is not that call.
    const bool scheme_ready =
        csr_host_filesystem_read("resource/schemes/TrackerScheme_ClassicPlus.res", nullptr, 0) > 0;
    surface->SolveTraverse(g_gameui_root, scheme_ready);
    surface->PaintTraverse(g_gameui_root);
    return g_surface.push_count(g_surface.surface) > before ? 1 : 0;
}

int csr_host_textured_quad_count(void)
{
    if (g_surface.surface == nullptr || g_surface.textured_quad_count == nullptr)
    {
        return 0;
    }
    return g_surface.textured_quad_count(g_surface.surface);
}

int csr_host_frame(int wide, int tall, double delta)
{
    if (wide <= 0 || tall <= 0)
    {
        return 0;
    }
    // Same present path as PresentXashLauncher: show the menu, then paint.
    if (csr_host_activate_gameui() == 0 || csr_host_paint_gameui(wide, tall) == 0)
    {
        return 0;
    }
    csr_xash_engine_advance_host_time(delta);
    return 1;
}

void csr_host_read_host_time(double* realtime, double* frametime, double* oldrealtime)
{
    csr_xash_engine_read_host_time(realtime, frametime, oldrealtime);
}

void csr_host_close(void)
{
    CloseEngine();
    CloseGameUi();
    ClosePair();
    csr_host_filesystem_clear();
    csr_xash_engine_clear();
    if (g_surface.surface != nullptr && g_surface.destroy != nullptr)
    {
        g_surface.destroy(g_surface.surface);
    }
    if (g_surface.module != nullptr)
    {
        dlclose(g_surface.module);
    }
    g_surface = {};
    g_vgui = {};
    g_extra_count = 0;
    g_count = 0;
    std::memset(g_extra, 0, sizeof(g_extra));
    std::memset(g_list, 0, sizeof(g_list));
}

int csr_host_factory_list(csr_host_factory_fn* extra, int extra_count, csr_host_factories_t* out)
{
    if (g_surface.create_interface == nullptr || out == nullptr || extra_count < 0 || extra_count > CSR_HOST_EXTRA_FACTORY_CAP)
    {
        return 0;
    }
    if (extra_count > 0 && extra == nullptr)
    {
        return 0;
    }

    g_extra_count = extra_count;
    std::memset(g_extra, 0, sizeof(g_extra));
    for (int index = 0; index < extra_count; ++index)
    {
        g_extra[index] = extra[index];
    }
    RebuildList();
    out->list = g_list;
    out->count = g_count;
    return 1;
}

void* csr_host_create_interface(const char* name, int* code)
{
    for (int index = 0; index < g_count; ++index)
    {
        if (g_list[index] == nullptr)
        {
            continue;
        }
        int one = 0;
        void* found = g_list[index](name, &one);
        if (found != nullptr)
        {
            if (code != nullptr)
            {
                *code = one;
            }
            return found;
        }
    }
    if (code != nullptr)
    {
        *code = 1;
    }
    return nullptr;
}

int csr_host_bind_input(const csr_host_vgui_t* vgui)
{
    if (g_surface.surface == nullptr || g_surface.set_input == nullptr || vgui == nullptr)
    {
        return 0;
    }

    g_vgui = *vgui;
    csr_surface_input_fns_t fns{};
    fns.cursor_moved = OnCursorMoved;
    fns.mouse_pressed = OnMousePressed;
    fns.mouse_released = OnMouseReleased;
    fns.dispatch = OnDispatch;
    fns.user = nullptr;
    g_surface.set_input(g_surface.surface, &fns);
    return 1;
}

void csr_host_input_move(int x, int y)
{
    if (g_surface.move != nullptr && g_surface.surface != nullptr)
    {
        g_surface.move(g_surface.surface, x, y);
    }
}

void csr_host_input_button(int button, int down)
{
    if (g_surface.button != nullptr && g_surface.surface != nullptr)
    {
        g_surface.button(g_surface.surface, button, down);
    }
}
