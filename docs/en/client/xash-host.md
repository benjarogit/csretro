# Host factory for the VGUI surface

`xash_host.so` loads `xash_surface.so` and publishes the factory list `vgui2`
uses to ask for `VGUI_Surface026`. The surface stays its own library. It is
not linked into `vgui2.so`, Xash3D is not modified, and `LD_PRELOAD` stays a
spike tool.

The first list entry answers only `VGUI_Surface026`. Later factories are not
asked for that name.

Mouse move, press and release go to the three input calls and then to exactly
one message dispatch. `RunFrame` is not called, so focus cannot jump back to a
stored cursor. `SurfaceSetCursorPos` only stores the position.

The `vgui2.so` built from this SDK exports `csr_vgui_dispatch_messages`. The
host receives function pointers. Process, Steam and frame rate are stated once
in [plan section 3.7](../../plan/csretro-plan.md).

## Build and tests

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target xash_host-tests
./build/linux-tests/out/bin/Release/xash_host-tests
```

The tests load the built surface and do not start the game.
