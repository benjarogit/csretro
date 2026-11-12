#pragma once

/*
 * Adapter for engine calls that engine_mini forwards.
 * A null function pointer is not a call. This library does not open hw.so,
 * does not create a surface, and does not call RunFrame.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct csr_xash_engine_surface
{
    void* object;
    void (*draw_init)(void* object);
    void (*reset_texture)(void* object);
    void (*set_text_color)(void* object, int r, int g, int b);
} csr_xash_engine_surface_t;

/*
 * Host-callable startup sequence. The product process delivers these because
 * the retail nitro hooks do not fire. Pointers are borrowed for the call.
 */
typedef struct csr_xash_engine_startup
{
    void (*argv)(int argc, char** argv);
    void (*mode)(void* window, void* device, void* context, const char* driver, const char* cmdline, int result);
    void (*init_game)(char* cmdline, char* basedir, void* window, int dedicated, int result);
    void (*shutdown)(void);
    void (*error)(const char* message);
} csr_xash_engine_startup_t;

#if defined(_WIN32)
#define CSR_XASH_ENGINE_API __declspec(dllexport)
#else
#define CSR_XASH_ENGINE_API __attribute__((visibility("default")))
#endif

CSR_XASH_ENGINE_API void csr_xash_engine_bind(const csr_xash_engine_surface_t* surface);
CSR_XASH_ENGINE_API void csr_xash_engine_clear(void);
CSR_XASH_ENGINE_API const csr_xash_engine_surface_t* csr_xash_engine_surface(void);
CSR_XASH_ENGINE_API void csr_xash_engine_draw_init(void);
CSR_XASH_ENGINE_API void csr_xash_engine_reset_texture(void);
CSR_XASH_ENGINE_API void csr_xash_engine_set_text_color(int r, int g, int b);

CSR_XASH_ENGINE_API void csr_xash_engine_bind_startup(const csr_xash_engine_startup_t* startup);
CSR_XASH_ENGINE_API void csr_xash_engine_argv(int argc, char** argv);
CSR_XASH_ENGINE_API void csr_xash_engine_mode(void* window, void* device, void* context, const char* driver, const char* cmdline, int result);
CSR_XASH_ENGINE_API void csr_xash_engine_init_game(char* cmdline, char* basedir, void* window, int dedicated, int result);
CSR_XASH_ENGINE_API void csr_xash_engine_shutdown(void);
CSR_XASH_ENGINE_API void csr_xash_engine_error(const char* message);

#ifdef __cplusplus
}
#endif
