#include "host_present.h"

#include "xash_host/xash_host.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

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
    };

    Present g_present;
    // Survives shutdown so a pump can report how many swaps it presented.
    int g_swaps = 0;
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

    // Negative when the config has no X visual or the color buffer is below 8 bits.
    // A visual at the root depth outranks a deeper one: the deeper visual is what
    // BadMatch rejects when the parent visual is not that visual.
    int ScoreConfig(Display* display, EGLDisplay egl_display, EGLConfig config, int root_depth, VisualID root_visual)
    {
        EGLint visual_id = 0;
        if (eglGetConfigAttrib(egl_display, config, EGL_NATIVE_VISUAL_ID, &visual_id) == EGL_FALSE || visual_id == 0)
        {
            return -1;
        }
        EGLint red = 0;
        EGLint green = 0;
        EGLint blue = 0;
        EGLint alpha = 0;
        eglGetConfigAttrib(egl_display, config, EGL_RED_SIZE, &red);
        eglGetConfigAttrib(egl_display, config, EGL_GREEN_SIZE, &green);
        eglGetConfigAttrib(egl_display, config, EGL_BLUE_SIZE, &blue);
        eglGetConfigAttrib(egl_display, config, EGL_ALPHA_SIZE, &alpha);
        if (red < 8 || green < 8 || blue < 8)
        {
            return -1;
        }

        XVisualInfo request{};
        request.visualid = static_cast<VisualID>(visual_id);
        request.screen = DefaultScreen(display);
        int visual_count = 0;
        XVisualInfo* visual = XGetVisualInfo(display, VisualIDMask | VisualScreenMask, &request, &visual_count);
        if (visual == nullptr || visual_count < 1)
        {
            if (visual != nullptr)
            {
                XFree(visual);
            }
            return -1;
        }
        int score = alpha;
        if (visual->depth == root_depth)
        {
            score += 1000;
        }
        if (visual->visualid == root_visual)
        {
            score += 100;
        }
        XFree(visual);
        return score;
    }

    // Creates the window with the config's visual. A border pixel lets that
    // visual be a child when its depth is not the parent's. On failure the
    // display stays open so another config can be tried.
    bool TryConfig(Display* display, EGLDisplay egl_display, Window root, EGLConfig config, int wide, int tall)
    {
        EGLint visual_id = 0;
        if (eglGetConfigAttrib(egl_display, config, EGL_NATIVE_VISUAL_ID, &visual_id) == EGL_FALSE)
        {
            SetError("eglGetConfigAttrib", eglGetError());
            return false;
        }
        XVisualInfo request{};
        request.visualid = static_cast<VisualID>(visual_id);
        request.screen = DefaultScreen(display);
        int visual_count = 0;
        XVisualInfo* visual = XGetVisualInfo(display, VisualIDMask | VisualScreenMask, &request, &visual_count);
        if (visual == nullptr || visual_count < 1)
        {
            if (visual != nullptr)
            {
                XFree(visual);
            }
            SetError("XGetVisualInfo", 0);
            return false;
        }

        g_x_error = 0;
        Colormap colormap = XCreateColormap(display, root, visual->visual, AllocNone);
        XSetWindowAttributes attributes{};
        attributes.colormap = colormap;
        attributes.border_pixel = 0;
        attributes.background_pixel = 0;
        attributes.event_mask = StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
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
            CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
            &attributes
        );
        XFree(visual);
        XSync(display, False);
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
            XSync(display, False);
            g_x_error = 0;
            return false;
        }

        XMapWindow(display, window);
        XFlush(display);
        bool viewable = false;
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            XWindowAttributes mapped{};
            if (XGetWindowAttributes(display, window, &mapped) != 0 && mapped.map_state == IsViewable)
            {
                viewable = true;
                break;
            }
            timespec delay{};
            delay.tv_nsec = 20 * 1000 * 1000;
            nanosleep(&delay, nullptr);
        }
        if (!viewable)
        {
            SetError("XMapWindow", 0);
            XDestroyWindow(display, window);
            XFreeColormap(display, colormap);
            return false;
        }

        EGLSurface surface = eglCreateWindowSurface(egl_display, config, static_cast<EGLNativeWindowType>(window), nullptr);
        if (surface == EGL_NO_SURFACE)
        {
            SetError("eglCreateWindowSurface", eglGetError());
            XDestroyWindow(display, window);
            XFreeColormap(display, colormap);
            return false;
        }
        const EGLint context_attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
        EGLContext context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attributes);
        if (context == EGL_NO_CONTEXT)
        {
            SetError("eglCreateContext", eglGetError());
            eglDestroySurface(egl_display, surface);
            XDestroyWindow(display, window);
            XFreeColormap(display, colormap);
            return false;
        }

        g_present.window = window;
        g_present.colormap = colormap;
        g_present.egl_surface = surface;
        g_present.egl_context = context;
        g_present.wide = wide;
        g_present.tall = tall;
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

    // Prefer a visual the root can parent. The first alpha-8 config is often a
    // deeper visual than the root; creating it without a border pixel is
    // BadMatch, and the parent visual with that config is EGL_BAD_CONFIG.
    XWindowAttributes root_attributes{};
    if (XGetWindowAttributes(display, root, &root_attributes) == 0)
    {
        SetError("XGetWindowAttributes", 0);
        Release(true);
        return 0;
    }
    const int root_depth = root_attributes.depth;
    const VisualID root_visual = XVisualIDFromVisual(root_attributes.visual);
    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLint count = 0;
    if (eglChooseConfig(egl_display, config_attributes, nullptr, 0, &count) == EGL_FALSE || count < 1)
    {
        SetError("eglChooseConfig", eglGetError());
        Release(true);
        return 0;
    }
    std::vector<EGLConfig> configs(static_cast<size_t>(count));
    if (eglChooseConfig(egl_display, config_attributes, configs.data(), count, &count) == EGL_FALSE || count < 1)
    {
        SetError("eglChooseConfig", eglGetError());
        Release(true);
        return 0;
    }

    struct Ranked
    {
        EGLConfig config;
        int score;
    };
    std::vector<Ranked> ranked;
    ranked.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        const int score = ScoreConfig(display, egl_display, configs[static_cast<size_t>(index)], root_depth, root_visual);
        if (score >= 0)
        {
            ranked.push_back(Ranked{configs[static_cast<size_t>(index)], score});
        }
    }
    if (ranked.empty())
    {
        SetError("eglChooseConfig", 0);
        Release(true);
        return 0;
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked& left, const Ranked& right) { return left.score > right.score; });

    g_swaps = 0;
    XErrorHandler previous_handler = XSetErrorHandler(OnXError);
    bool created = false;
    for (const Ranked& candidate : ranked)
    {
        if (TryConfig(display, egl_display, root, candidate.config, wide, tall))
        {
            created = true;
            break;
        }
    }
    XSetErrorHandler(previous_handler);
    if (!created)
    {
        Release(true);
        return 0;
    }
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
    ++g_swaps;
    g_error[0] = '\0';
    return 1;
}

void csr_host_present_dispatch(void)
{
    if (g_present.display == nullptr || g_present.window == 0)
    {
        return;
    }
    XSync(g_present.display, False);
    while (XPending(g_present.display) > 0)
    {
        XEvent event{};
        XNextEvent(g_present.display, &event);
        if (event.xany.window != g_present.window)
        {
            continue;
        }
        if (event.type == MotionNotify)
        {
            csr_host_input_move(event.xmotion.x, event.xmotion.y);
            continue;
        }
        if (event.type != ButtonPress && event.type != ButtonRelease)
        {
            continue;
        }
        const int code = event.xbutton.button - 1;
        if (code < 0)
        {
            continue;
        }
        csr_host_input_button(code, event.type == ButtonPress ? 1 : 0);
    }
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
        *swaps = g_swaps;
    }
    return g_present.egl_surface != EGL_NO_SURFACE ? 1 : 0;
}

const char* csr_host_present_error(void)
{
    return g_error[0] == '\0' ? nullptr : g_error;
}
