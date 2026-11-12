#include "host_present.h"

#include "xash_host/xash_host.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{

    struct Present
    {
        Display* display = nullptr;
        Window window = 0;
        Colormap colormap = 0;
        EGLDisplay egl_display = EGL_NO_DISPLAY;
        EGLSurface egl_surface = EGL_NO_SURFACE;
        EGLContext egl_context = EGL_NO_CONTEXT;
        int wide = 0;
        int tall = 0;
        int swaps = 0;
    };

    Present g_present;
    char g_error[80] = {};
    int g_x_error = 0;

    int OnXError(Display*, XErrorEvent* event)
    {
        g_x_error = event->error_code;
        return 0;
    }

    void SetError(const char* call, int code)
    {
        if (code == 0)
        {
            std::snprintf(g_error, sizeof(g_error), "%s", call);
            return;
        }
        std::snprintf(g_error, sizeof(g_error), "%s 0x%x", call, code);
    }

    void Release(bool keep_error)
    {
        char saved[sizeof(g_error)];
        std::memcpy(saved, g_error, sizeof(saved));
        if (g_present.egl_display != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(g_present.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (g_present.egl_surface != EGL_NO_SURFACE)
            {
                eglDestroySurface(g_present.egl_display, g_present.egl_surface);
            }
            if (g_present.egl_context != EGL_NO_CONTEXT)
            {
                eglDestroyContext(g_present.egl_display, g_present.egl_context);
            }
            eglTerminate(g_present.egl_display);
        }
        if (g_present.display != nullptr)
        {
            if (g_present.window != 0)
            {
                XDestroyWindow(g_present.display, g_present.window);
            }
            if (g_present.colormap != 0)
            {
                XFreeColormap(g_present.display, g_present.colormap);
            }
            XCloseDisplay(g_present.display);
        }
        g_present = {};
        if (keep_error)
        {
            std::memcpy(g_error, saved, sizeof(g_error));
        }
        else
        {
            g_error[0] = '\0';
        }
    }

    void ClearBackBuffer(int wide, int tall)
    {
        glViewport(0, 0, wide, tall);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    bool MakeCurrent()
    {
        if (eglMakeCurrent(g_present.egl_display, g_present.egl_surface, g_present.egl_surface, g_present.egl_context) == EGL_FALSE)
        {
            SetError("eglMakeCurrent", eglGetError());
            return false;
        }
        return true;
    }

} // namespace

int csr_host_present_begin(int wide, int tall)
{
    if (wide <= 0 || tall <= 0)
    {
        SetError("csr_host_present_begin", 0);
        return 0;
    }
    if (g_present.egl_surface != EGL_NO_SURFACE)
    {
        if (g_present.wide != wide || g_present.tall != tall)
        {
            XResizeWindow(g_present.display, g_present.window, static_cast<unsigned int>(wide), static_cast<unsigned int>(tall));
            XFlush(g_present.display);
            g_present.wide = wide;
            g_present.tall = tall;
        }
        if (!MakeCurrent())
        {
            return 0;
        }
        ClearBackBuffer(wide, tall);
        g_error[0] = '\0';
        return 1;
    }

    g_error[0] = '\0';
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        SetError("XOpenDisplay", 0);
        return 0;
    }
    g_present.display = display;

    const int screen = DefaultScreen(display);
    const Window root = RootWindow(display, screen);
    auto* get_platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    if (get_platform != nullptr)
    {
        egl_display = get_platform(EGL_PLATFORM_X11_KHR, display, nullptr);
    }
    if (egl_display == EGL_NO_DISPLAY)
    {
        egl_display = eglGetDisplay(display);
    }
    if (egl_display == EGL_NO_DISPLAY)
    {
        SetError("eglGetPlatformDisplayEXT", eglGetError());
        Release(true);
        return 0;
    }
    g_present.egl_display = egl_display;
    if (eglInitialize(egl_display, nullptr, nullptr) == EGL_FALSE)
    {
        SetError("eglInitialize", eglGetError());
        Release(true);
        return 0;
    }
    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    {
        SetError("eglBindAPI", eglGetError());
        Release(true);
        return 0;
    }

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_NONE
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (eglChooseConfig(egl_display, config_attributes, &config, 1, &count) == EGL_FALSE || count < 1)
    {
        SetError("eglChooseConfig", eglGetError());
        Release(true);
        return 0;
    }

    // The window visual has to be the EGL config's visual. A parent visual
    // makes eglCreateWindowSurface return EGL_BAD_CONFIG.
    EGLint visual_id = 0;
    if (eglGetConfigAttrib(egl_display, config, EGL_NATIVE_VISUAL_ID, &visual_id) == EGL_FALSE)
    {
        SetError("eglGetConfigAttrib", eglGetError());
        Release(true);
        return 0;
    }
    XVisualInfo request = {};
    request.visualid = static_cast<VisualID>(visual_id);
    int visual_count = 0;
    XVisualInfo* visual = XGetVisualInfo(display, VisualIDMask, &request, &visual_count);
    if (visual == nullptr || visual_count < 1)
    {
        SetError("XGetVisualInfo", 0);
        Release(true);
        return 0;
    }

    g_x_error = 0;
    XErrorHandler previous_handler = XSetErrorHandler(OnXError);
    Colormap colormap = XCreateColormap(display, root, visual->visual, AllocNone);
    XSetWindowAttributes attributes = {};
    attributes.colormap = colormap;
    attributes.event_mask = StructureNotifyMask;
    Window window = XCreateWindow(
        display,
        root,
        0,
        0,
        static_cast<unsigned int>(wide),
        static_cast<unsigned int>(tall),
        0,
        visual->depth,
        InputOutput,
        visual->visual,
        CWColormap | CWEventMask,
        &attributes
    );
    XFree(visual);
    if (window != 0 && g_x_error == 0)
    {
        XMapWindow(display, window);
        XFlush(display);
    }
    XSync(display, False);
    XSetErrorHandler(previous_handler);
    if (window == 0 || g_x_error != 0)
    {
        SetError("XCreateWindow", g_x_error);
        if (window != 0)
        {
            XDestroyWindow(display, window);
        }
        if (colormap != 0)
        {
            XFreeColormap(display, colormap);
        }
        Release(true);
        return 0;
    }
    g_present.window = window;
    g_present.colormap = colormap;

    bool viewable = false;
    for (int attempt = 0; attempt < 50; ++attempt)
    {
        XWindowAttributes mapped = {};
        if (XGetWindowAttributes(display, window, &mapped) != 0 && mapped.map_state == IsViewable)
        {
            viewable = true;
            break;
        }
        timespec delay = {};
        delay.tv_nsec = 20 * 1000 * 1000;
        nanosleep(&delay, nullptr);
    }
    if (!viewable)
    {
        SetError("XMapWindow", 0);
        Release(true);
        return 0;
    }

    EGLSurface surface = eglCreateWindowSurface(egl_display, config, static_cast<EGLNativeWindowType>(window), nullptr);
    if (surface == EGL_NO_SURFACE)
    {
        SetError("eglCreateWindowSurface", eglGetError());
        Release(true);
        return 0;
    }
    g_present.egl_surface = surface;
    const EGLint context_attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
    EGLContext context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attributes);
    if (context == EGL_NO_CONTEXT)
    {
        SetError("eglCreateContext", eglGetError());
        Release(true);
        return 0;
    }
    g_present.egl_context = context;
    g_present.wide = wide;
    g_present.tall = tall;
    if (!MakeCurrent())
    {
        Release(true);
        return 0;
    }
    ClearBackBuffer(wide, tall);
    g_error[0] = '\0';
    return 1;
}

int csr_host_present_swap(void)
{
    if (g_present.egl_surface == EGL_NO_SURFACE)
    {
        SetError("eglSwapBuffers", 0);
        return 0;
    }
    if (!MakeCurrent())
    {
        return 0;
    }
    // Last pixels in the back buffer. The X11 window stays empty until the swap.
    glDrawBuffer(GL_BACK);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 8, 8);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    if (eglSwapBuffers(g_present.egl_display, g_present.egl_surface) == EGL_FALSE)
    {
        SetError("eglSwapBuffers", eglGetError());
        return 0;
    }
    ++g_present.swaps;
    g_error[0] = '\0';
    return 1;
}

void csr_host_present_shutdown(void)
{
    Release(false);
}

int csr_host_present_query(void** egl_display, void** egl_surface, unsigned long* window, int* swaps)
{
    if (egl_display != nullptr)
    {
        *egl_display = g_present.egl_display == EGL_NO_DISPLAY ? nullptr : g_present.egl_display;
    }
    if (egl_surface != nullptr)
    {
        *egl_surface = g_present.egl_surface == EGL_NO_SURFACE ? nullptr : g_present.egl_surface;
    }
    if (window != nullptr)
    {
        *window = static_cast<unsigned long>(g_present.window);
    }
    if (swaps != nullptr)
    {
        *swaps = g_present.swaps;
    }
    return g_present.egl_surface != EGL_NO_SURFACE ? 1 : 0;
}

const char* csr_host_present_error(void)
{
    return g_error[0] == '\0' ? nullptr : g_error;
}
