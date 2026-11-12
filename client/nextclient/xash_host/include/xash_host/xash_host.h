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
 * a null pointer. Does not call the dispatch and does not start the client.
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
 * next_engine_mini module is not called. Does not call Start or RunFrame
 * and does not start the game. Returns 0 when the base panel is not visible.
 */
CSR_HOST_API int csr_host_init_gameui(void);
CSR_HOST_API int csr_host_gameui_initialized(void);

/* list[0] answers VGUI_Surface026. A mounted filesystem follows it, then extras. */
CSR_HOST_API int csr_host_factory_list(csr_host_factory_fn* extra, int extra_count, csr_host_factories_t* out);
CSR_HOST_API void* csr_host_create_interface(const char* name, int* code);

/* Installs the three mouse callbacks and the one dispatch. Does not call RunFrame. */
CSR_HOST_API int csr_host_bind_input(const csr_host_vgui_t* vgui);
CSR_HOST_API void csr_host_input_move(int x, int y);
CSR_HOST_API void csr_host_input_button(int button, int down);

#ifdef __cplusplus
}
#endif
