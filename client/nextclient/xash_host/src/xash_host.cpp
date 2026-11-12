#include "xash_host/xash_host.h"

#include "host_filesystem.h"
#include "xash_surface/xash_surface.h"
#include "xash_engine/xash_engine.h"

#include <vgui/IScheme.h>
#include <vgui/IVGui.h>

#include <dlfcn.h>

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

    struct GameUiApi
    {
        void* module = nullptr;
        void* gameui = nullptr;
        void* console = nullptr;
    };

    GameUiApi g_gameui;

    void CloseGameUi()
    {
        g_gameui.gameui = nullptr;
        g_gameui.console = nullptr;
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
    g_surface.create_interface = LoadSymbol<decltype(g_surface.create_interface)>(module, "csr_surface_create_interface");
    if (g_surface.create == nullptr || g_surface.destroy == nullptr || g_surface.move == nullptr || g_surface.button == nullptr ||
        g_surface.set_input == nullptr || g_surface.set_file == nullptr || g_surface.create_interface == nullptr)
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

void csr_host_close(void)
{
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
