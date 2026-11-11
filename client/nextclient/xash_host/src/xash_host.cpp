#include "xash_host/xash_host.h"

#include "xash_engine/xash_engine.h"
#include "xash_surface/xash_surface.h"

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
        void* (*create_interface)(const char*, int*) = nullptr;
    };

    SurfaceApi g_surface;
    csr_host_vgui_t g_vgui{};
    csr_host_factory_fn g_list[1 + CSR_HOST_EXTRA_FACTORY_CAP]{};
    int g_count = 0;

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
    g_surface.create_interface = LoadSymbol<decltype(g_surface.create_interface)>(module, "csr_surface_create_interface");
    if (g_surface.create == nullptr || g_surface.destroy == nullptr || g_surface.move == nullptr || g_surface.button == nullptr ||
        g_surface.set_input == nullptr || g_surface.create_interface == nullptr)
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
    g_list[0] = g_surface.create_interface;
    g_count = 1;
    return 1;
}

void csr_host_close(void)
{
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
    g_count = 0;
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

    g_list[0] = g_surface.create_interface;
    for (int index = 0; index < extra_count; ++index)
    {
        g_list[index + 1] = extra[index];
    }
    g_count = extra_count + 1;
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
