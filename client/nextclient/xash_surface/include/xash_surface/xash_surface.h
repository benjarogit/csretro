#pragma once

/*
 * Host contract for the one VGUI_Surface026 object.
 *
 * vgui2 asks the host factory for "VGUI_Surface026" and stores the result as
 * the surface every control draws through. This library is that object.
 * CreateInterface returns the instance created by csr_surface_create.
 *
 * VGUI alpha 0 is opaque. GL alpha 0 is transparent. Draws invert alpha.
 * Panel callbacks may be null. Clip rectangles are absolute screen pixels.
 * Text is a FreeType glyph from the packed atlas. The first glyph set on a
 * handle fixes the height. Arial, Verdana and Tahoma use the shipped
 * substitutes beside this library. Marlett and any other face come from
 * resolve_font. A draw restores the GL state it touches before it returns.
 *
 * SolveTraverse walks the embedded tree through csr_surface_panel_fns_t.
 * PaintTraverse calls the panel paint once with repaint and allow-force set,
 * then paints popups. Child traversal stays in vgui2. Without the function
 * table both calls do nothing.
 *
 * Mouse input is stored and, when csr_surface_input_fns_t is set, forwarded
 * as cursor, press and release followed by one dispatch. Button 0 is the
 * left mouse button. RunFrame does not dispatch and does not move the cursor.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct csr_surface csr_surface_t;

typedef struct csr_surface_host
{
    int screen_wide;
    int screen_tall;
    void (*panel_abs_pos)(void* panel, int* x, int* y, void* user);
    void (*panel_insets)(void* panel, int* left, int* top, int* right, int* bottom, void* user);
    void (*panel_clip_rect)(void* panel, int* x0, int* y0, int* x1, int* y1, void* user);
    // Writes a filesystem path for a face this library does not ship.
    // Returns non-zero when path holds a readable file.
    int (*resolve_font)(const char* name, int weight, char* path, int path_size, void* user);
    void* user;
} csr_surface_host_t;

typedef struct csr_surface_panel_fns
{
    int (*is_visible)(void* panel, void* user);
    int (*is_popup)(void* panel, void* user);
    int (*child_count)(void* panel, void* user);
    void* (*child_at)(void* panel, int index, void* user);
    void (*solve)(void* panel, void* user);
    void (*apply_scheme)(void* panel, void* user);
    void (*paint)(void* panel, int repaint, int allow_force, void* user);
    void* user;
} csr_surface_panel_fns_t;

typedef struct csr_surface_input_fns
{
    void (*cursor_moved)(int x, int y, void* user);
    void (*mouse_pressed)(int code, void* user);
    void (*mouse_released)(int code, void* user);
    void (*dispatch)(void* user);
    void* user;
} csr_surface_input_fns_t;

#if defined(_WIN32)
#define CSR_SURFACE_API __declspec(dllexport)
#else
#define CSR_SURFACE_API __attribute__((visibility("default")))
#endif

CSR_SURFACE_API csr_surface_t* csr_surface_create(const csr_surface_host_t* host);
CSR_SURFACE_API void csr_surface_destroy(csr_surface_t* surface);
CSR_SURFACE_API void csr_surface_begin_frame(csr_surface_t* surface, int wide, int tall);
CSR_SURFACE_API void csr_surface_end_frame(csr_surface_t* surface);
CSR_SURFACE_API int csr_surface_gl_error(const csr_surface_t* surface);
CSR_SURFACE_API void csr_surface_input_move(csr_surface_t* surface, int x, int y);
CSR_SURFACE_API void csr_surface_input_button(csr_surface_t* surface, int button, int down);
CSR_SURFACE_API void* csr_surface_vgui(csr_surface_t* surface);
CSR_SURFACE_API void csr_surface_set_panel(csr_surface_t* surface, const csr_surface_panel_fns_t* panel);
CSR_SURFACE_API void csr_surface_set_input(csr_surface_t* surface, const csr_surface_input_fns_t* input);
CSR_SURFACE_API void* csr_surface_create_interface(const char* name, int* code);

#ifdef __cplusplus
}
#endif
