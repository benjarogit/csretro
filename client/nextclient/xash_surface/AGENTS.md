# Xash surface

`xash_surface.so` is the single `VGUI_Surface026` object. `vgui2` loads it from the host factory (`VGui_InternalLoadInterfaces`) and `SurfaceNext` forwards every draw to `g_pSurface`. Linking a second surface into `vgui2.so` would split that identity. Xash3D is not modified: the CSRETRO launcher will pass this factory. `LD_PRELOAD` stays a spike tool.

## Draw contract

- Coordinates passed to draw calls are parent-relative. `PushMakeCurrent` replaces the origin with the host's absolute panel position. `useInsets` adds the host inset. `PopMakeCurrent` restores the previous origin. Clip rectangles from the host are absolute screen pixels.
- VGUI alpha 0 is opaque. Convert with `VguiAlphaToGl` at the GL call. Stored colors stay in VGUI space.
- Each draw saves and restores the GL state it changes, including `GL_TEXTURE_2D` and the texture binding. Leaving texturing disabled makes Xash3D's following 2D batch draw untextured white quads.
- Text is a FreeType glyph in a packed atlas. The first glyph set fixes the height. Advance and ascent come from that face. Arial, Verdana and Tahoma map to the shipped Liberation Sans and DejaVu Sans files copied next to the library. Marlett and any other name come from `resolve_font`. Draws happen in call order, not in a deferred queue.
- `CreateInterface("VGUI_Surface026")` returns the live `csr_surface_create` instance and nothing else.

## Verification

Build `xash_surface` from the `linux` preset. Build and run `xash_surface-tests` from the `linux-tests` preset. The map tests do not need a GL context. The GL tests create an EGL pbuffer and require `LIBGL_ALWAYS_SOFTWARE=1`. They do not start the game.
