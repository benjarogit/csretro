# VGUI surface on Xash3D

CSRETRO draws VGUI2 through its own `VGUI_Surface026`. Xash3D does not provide
that interface, and the shipped `vgui2.so` does not draw by itself: it asks
the host factory for the surface and forwards every call.

## Module

The module is `client/nextclient/xash_surface/`. The build produces
`xash_surface.so`. `CreateInterface("VGUI_Surface026")` returns the instance
created by `csr_surface_create`.

The host supplies the screen size, the absolute panel position, optional
insets and the absolute clip rectangle. Draw coordinates stay parent-relative.
VGUI alpha 0 is opaque; the conversion happens at the OpenGL call. Each draw
restores the OpenGL state it touched.

Text in this slice is one cell per character. That covers position, colour and
advance. Glyph shapes arrive with the font atlas. `PaintTraverse` does not
walk the panel tree, and mouse events are only stored.

## Build and tests

The library is part of the `linux` preset and therefore of `BUILD_ALL`. It is
not installed into the game directory yet, because the launcher does not load
it yet.

Tests use the `linux-tests` preset:

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target xash_surface-tests
./build/linux-tests/out/bin/Release/xash_surface-tests
```

The offscreen run sets `LIBGL_ALWAYS_SOFTWARE=1` and opens an EGL pbuffer. It
does not start the game. The spike with the external stub remains the proof
for a real VGUI2 tree until the launcher registers this library.

The decision and the scope are in
[plan section 3.7](../../plan/csretro-plan.md).
