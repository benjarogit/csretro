#include "xash_engine/xash_engine.h"

namespace
{

    csr_xash_engine_surface_t g_surface{};
    csr_xash_engine_startup_t g_startup{};

    void Call(void (*function)(void*), void* object)
    {
        if (function == nullptr || object == nullptr)
        {
            return;
        }
        function(object);
    }

    void ClearSurface()
    {
        g_surface = {};
    }

} // namespace

void csr_xash_engine_bind(const csr_xash_engine_surface_t* surface)
{
    if (surface == nullptr || surface->object == nullptr)
    {
        ClearSurface();
        return;
    }
    g_surface = *surface;
}

void csr_xash_engine_clear(void)
{
    ClearSurface();
    g_startup = {};
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

void csr_xash_engine_bind_startup(const csr_xash_engine_startup_t* startup)
{
    if (startup == nullptr)
    {
        g_startup = {};
        return;
    }
    g_startup = *startup;
}

void csr_xash_engine_argv(int argc, char** argv)
{
    if (g_startup.argv == nullptr)
    {
        return;
    }
    g_startup.argv(argc, argv);
}

void csr_xash_engine_mode(void* window, void* device, void* context, const char* driver, const char* cmdline, int result)
{
    if (g_startup.mode == nullptr)
    {
        return;
    }
    g_startup.mode(window, device, context, driver, cmdline, result);
}

void csr_xash_engine_init_game(char* cmdline, char* basedir, void* window, int dedicated, int result)
{
    if (g_startup.init_game == nullptr)
    {
        return;
    }
    g_startup.init_game(cmdline, basedir, window, dedicated, result);
}

void csr_xash_engine_shutdown(void)
{
    if (g_startup.shutdown == nullptr)
    {
        return;
    }
    g_startup.shutdown();
}

void csr_xash_engine_error(const char* message)
{
    if (g_startup.error == nullptr)
    {
        return;
    }
    g_startup.error(message);
}

double oldrealtime;

namespace
{

    double g_realtime = 0.0;
    double g_frametime = 0.0;

} // namespace

double* csr_xash_engine_realtime_ptr(void)
{
    return &g_realtime;
}

double* csr_xash_engine_frametime_ptr(void)
{
    return &g_frametime;
}

void csr_xash_engine_advance_host_time(double delta)
{
    g_realtime += delta;
    g_frametime = delta;
    oldrealtime = g_realtime;
}

void csr_xash_engine_read_host_time(double* realtime, double* frametime, double* oldrealtime_out)
{
    if (realtime != nullptr)
    {
        *realtime = g_realtime;
    }
    if (frametime != nullptr)
    {
        *frametime = g_frametime;
    }
    if (oldrealtime_out != nullptr)
    {
        *oldrealtime_out = oldrealtime;
    }
}
