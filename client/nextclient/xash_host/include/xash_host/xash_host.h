#pragma once

/*
 * Host factory for the one VGUI_Surface026.
 *
 * vgui2 asks a list of factories. This module loads xash_surface.so into the
 * local namespace and publishes that list with the surface factory first.
 * It does not link the surface into vgui2 and it does not preload itself.
 *
 * Mouse callbacks map onto CInputWin32::InternalCursorMoved,
 * InternalMousePressed and InternalMouseReleased. Each event is followed by
 * one dispatch. The dispatch symbol our vgui2 exports is
 * CSR_VGUI_DISPATCH_SYMBOL. CVGui::RunFrame is not called: it would reset
 * mouse focus from a stored cursor.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define CSR_VGUI_DISPATCH_SYMBOL "csr_vgui_dispatch_messages"
#define CSR_HOST_EXTRA_FACTORY_CAP 8

typedef void* (*csr_host_factory_fn)(const char* name, int* code);

typedef struct csr_host_factories
{
    csr_host_factory_fn* list;
    int count;
} csr_host_factories_t;

typedef struct csr_host_vgui
{
    void* input;
    void (*cursor_moved)(void* input, int x, int y);
    void (*mouse_pressed)(void* input, int code);
    void (*mouse_released)(void* input, int code);
    void (*dispatch)(void* vgui);
    void* vgui;
} csr_host_vgui_t;

#if defined(_WIN32)
#define CSR_HOST_API __declspec(dllexport)
#else
#define CSR_HOST_API __attribute__((visibility("default")))
#endif

/* Returns 1 when the surface factory is live. path is the xash_surface.so file. */
CSR_HOST_API int csr_host_open(const char* path);
CSR_HOST_API void csr_host_close(void);

/*
 * Opens the matched client_mini and vgui2 after the surface factory is live.
 * Returns 0 when a library or a required export is missing, and does not call
 * a null pointer. Binds cursor, press and release to that vgui input, then one
 * dispatch. Does not call RunFrame and does not start the client.
 */
CSR_HOST_API int csr_host_load_pair(const char* client_path, const char* vgui_path);
CSR_HOST_API int csr_host_pair_loaded(void);
CSR_HOST_API void (*csr_host_vgui_dispatch(void))(void);
CSR_HOST_API void* csr_host_scheme(void);

/*
 * Opens gameui after the surface factory is live. Returns 0 when the library
 * or GameUI007 / GameConsole003 is missing, and does not call a null pointer.
 * Does not call into the game UI and does not add a factory.
 */
CSR_HOST_API int csr_host_load_gameui(const char* path);
CSR_HOST_API int csr_host_gameui_loaded(void);
CSR_HOST_API void* csr_host_gameui(void);
CSR_HOST_API void* csr_host_game_console(void);

/*
 * Mounts the one VFileSystem009 on this factory list, after the surface.
 * root is the only directory that is served. Returns 0 when the surface
 * factory is down or root is not a directory, and does not invent a path.
 * A missing file is a miss. Does not open hw.so.
 */
CSR_HOST_API int csr_host_mount_filesystem(const char* root);
CSR_HOST_API int csr_host_filesystem_mounted(void);
/*
 * Copies the mounted root into buffer. Returns 0 when the mount is missing,
 * the directory is empty, or the buffer cannot hold it. Does not invent a path.
 */
CSR_HOST_API int csr_host_filesystem_directory(char* buffer, int maxlen);
/* Mounts that same root. Returns 0 when root is not a directory. Does not invent a path. */
CSR_HOST_API int csr_host_filesystem_set_root(const char* root);
CSR_HOST_API void csr_host_filesystem_clear(void);

/*
 * Calls IVGui::Init with this factory list after the pair is loaded and a
 * root is mounted. vgui2 then takes VGUI_Surface026, VFileSystem009 and
 * VGUI_Localize003. Returns 0 when one of those is missing and does not call
 * a null pointer. Does not call RunFrame and does not start the game.
 */
CSR_HOST_API int csr_host_connect_vgui(void);
CSR_HOST_API int csr_host_vgui_connected(void);

/*
 * Loads one scheme through the connected vgui2 scheme manager. path is
 * relative to the mounted root. Returns 0 when vgui2 is not connected, the
 * file is missing, or the file is not a scheme. Does not invent a path and
 * does not call RunFrame.
 */
CSR_HOST_API int csr_host_load_scheme(const char* path, const char* tag);
CSR_HOST_API int csr_host_scheme_loaded(void);

/*
 * Calls IGameUI::Initialize after the vgui connection. A missing
 * next_engine_mini module is not called. Does not call RunFrame
 * and does not start the game. Returns 0 when the base panel is not a child
 * of the host root panel. The server list is built when
 * resource/schemes/TrackerScheme_ClassicPlus.res is on that root.
 */
CSR_HOST_API int csr_host_init_gameui(void);
CSR_HOST_API int csr_host_gameui_initialized(void);

/*
 * Calls IGameUI::Start with a host engine table. Cvar reads are empty, the
 * clock stays at 0, and recorded commands are not executed. Does not open
 * hw.so, does not call RunFrame, and does not start the game. Returns 0
 * when the UI is not initialized.
 */
CSR_HOST_API int csr_host_start_gameui(void);
CSR_HOST_API int csr_host_gameui_started(void);

/*
 * Loads next_engine_mini and nitro_api after the surface factory is live.
 * Calls EngineMini::Init, then argv, mode and init_game. A missing library
 * or export returns 0 and is not called. A successful load does not call
 * shutdown or the fatal error, does not open hw.so, does not call RunFrame
 * and does not start Steam. basedir is copied into the init record.
 * pfnGetGameDir writes the mounted directory name. An empty mount leaves
 * that pointer null, so SetCStrikeFlags is not called.
 */
CSR_HOST_API int csr_host_load_engine(const char* engine_path, const char* nitro_path, const char* basedir);
CSR_HOST_API int csr_host_engine_loaded(void);
/* COM_CheckParm inside the loaded engine_mini. 0 when the engine is down. */
CSR_HOST_API int csr_host_engine_check_parm(const char* parm);
/* Reads the engine_mini startup record. Returns 0 when the engine is down. */
CSR_HOST_API int csr_host_engine_startup_calls(
    int* argv_calls,
    int* mode_calls,
    int* init_calls,
    int* shutdown_calls,
    int* error_calls);
CSR_HOST_API int csr_host_engine_command_count(void);
/* Last string passed to pfnClientCmd. Empty until a command is recorded. Not executed. */
CSR_HOST_API const char* csr_host_last_client_command(void);
/*
 * Calls IGameUI::ActivateGameUI after Start. Shows the main menu and invalidates
 * its layout. Does not call RunFrame and does not run recorded commands.
 * Returns 0 when Start has not run.
 */
CSR_HOST_API int csr_host_activate_gameui(void);
CSR_HOST_API int csr_host_gameui_child_count(void);
CSR_HOST_API int csr_host_gameui_base_child_count(void);

/*
 * Solves the gameui tree and paints it through the one surface. wide and tall
 * are the root size in pixels. Returns 0 when the UI is not initialized or the
 * paint does not reach a panel. Does not call RunFrame and does not start the game.
 */
CSR_HOST_API int csr_host_paint_gameui(int wide, int tall);
/* Textured quads submitted by the one surface. A missing surface returns 0. */
CSR_HOST_API int csr_host_textured_quad_count(void);

/*
 * One GameUI frame. Opens the X11 display, creates an EGL window surface,
 * shows the menu, paints it, then calls eglSwapBuffers. The EGL config is
 * one whose native visual can be a child of the root: a visual at the root
 * depth is preferred, and a border pixel is set so a deeper visual is not
 * rejected. The window uses that visual. Adds delta to host realtime and
 * stores that delta as the frame interval. Writes those fields directly.
 * Returns 0 when the window surface, the swap or the paint fails, and then
 * the clock stays put. Does not call CVGui::RunFrame, ISurface::SwapBuffers
 * or Host_FilterTime, does not install a hook, does not open hw.so, does
 * not start Steam, and does not exec the launcher command. The X11 display
 * connection is the window's socket.
 */
CSR_HOST_API int csr_host_frame(int wide, int tall, double delta);
/*
 * Calls csr_host_frame `frames` times on one mapped X11 window. Motion and
 * button events that arrive during the call go to csr_host_input_move and
 * csr_host_input_button. Then the window is destroyed. After a successful
 * call the swap count is `frames` and host time has advanced by frames
 * times delta. Returns 0 when a frame fails, and still destroys the window.
 * Does not call CVGui::RunFrame, does not install a hook, and does not exec
 * the launcher command.
 */
CSR_HOST_API int csr_host_pump(int wide, int tall, int frames, double delta);
/*
 * Repeats csr_host_frame on one mapped X11 window until the recorded client
 * command is quit. QuitNoConfirm records that command through pfnClientCmd
 * and does not run it. Motion and button events still go to
 * csr_host_input_move and csr_host_input_button. Then the window is destroyed.
 * After a successful call the swap count is the number of frames and host
 * time has advanced by that count times delta. Returns 0 when a frame fails,
 * and still destroys the window. Does not call CVGui::RunFrame, does not
 * install a hook, and does not exec the launcher command.
 */
CSR_HOST_API int csr_host_run(int wide, int tall, double delta);
CSR_HOST_API void csr_host_read_host_time(double* realtime, double* frametime, double* oldrealtime);
/* The live EGL window surface. Returns 0 when no window is mapped.
   swaps is the count from the present that just ran, including after the
   pump destroys the window. */
CSR_HOST_API int csr_host_present_query(void** egl_display, void** egl_surface, unsigned long* window, int* swaps);
/* The X11 or EGL call that failed, or null when the present is healthy. */
CSR_HOST_API const char* csr_host_present_error(void);

/* list[0] answers VGUI_Surface026. A mounted filesystem follows it, then extras. */
CSR_HOST_API int csr_host_factory_list(csr_host_factory_fn* extra, int extra_count, csr_host_factories_t* out);
CSR_HOST_API void* csr_host_create_interface(const char* name, int* code);

/* Installs the three mouse callbacks and the one dispatch. Does not call RunFrame. */
CSR_HOST_API int csr_host_bind_input(const csr_host_vgui_t* vgui);
CSR_HOST_API void csr_host_input_move(int x, int y);
CSR_HOST_API void csr_host_input_button(int button, int down);
/* Motion calls and button presses since csr_host_run cleared them. x and y
   are the last motion. code is the last pressed button. */
CSR_HOST_API void csr_host_input_observed(int* moves, int* x, int* y, int* presses, int* code);

#ifdef __cplusplus
}
#endif
