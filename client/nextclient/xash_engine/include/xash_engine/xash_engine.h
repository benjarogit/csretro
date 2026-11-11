#pragma once

/*
 * One adapter for the engine calls engine_mini forwards at the retail
 * vgui2 surface. A null function is not a call. This library does not open
 * hw.so and does not create a surface.
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

#ifdef __cplusplus
}
#endif
