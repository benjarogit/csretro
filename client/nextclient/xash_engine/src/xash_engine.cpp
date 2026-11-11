#include "xash_engine/xash_engine.h"

namespace
{

    csr_xash_engine_surface_t g_surface{};

    void Call(void (*function)(void*), void* object)
    {
        if (function == nullptr || object == nullptr)
        {
            return;
        }
        function(object);
    }

} // namespace

void csr_xash_engine_bind(const csr_xash_engine_surface_t* surface)
{
    if (surface == nullptr || surface->object == nullptr)
    {
        csr_xash_engine_clear();
        return;
    }
    g_surface = *surface;
}

void csr_xash_engine_clear(void)
{
    g_surface = {};
}

const csr_xash_engine_surface_t* csr_xash_engine_surface(void)
{
    return &g_surface;
}

void csr_xash_engine_draw_init(void)
{
    Call(g_surface.draw_init, g_surface.object);
}

void csr_xash_engine_reset_texture(void)
{
    Call(g_surface.reset_texture, g_surface.object);
}

void csr_xash_engine_set_text_color(int r, int g, int b)
{
    if (g_surface.set_text_color == nullptr || g_surface.object == nullptr)
    {
        return;
    }
    g_surface.set_text_color(g_surface.object, r, g, b);
}
