# Host factory for the VGUI surface

`xash_host.so` loads `xash_surface.so` and publishes the factory list `vgui2`
uses to ask for `VGUI_Surface026`. The surface stays its own library. It is
not linked into `vgui2.so`, Xash3D is not modified, and `LD_PRELOAD` stays a
spike tool.

The first list entry answers only `VGUI_Surface026`. Once a root is mounted,
`VFileSystem009` follows on that same list. The proxy reads only files under
that root. A missing file or a path outside the root is a miss. After the
pair is loaded and the root is mounted, `IVGui::Init` takes this list.
`vgui2` then holds the one surface. A scheme file under that root is loaded
through the same manager. A missing file leaves the host open. The host
then calls `IGameUI::Initialize`. A missing engine module is not called.
The base panel is a child of the host root. When
`resource/schemes/TrackerScheme_ClassicPlus.res` is on that root, the default
scheme is read from the source file and the server list becomes a child of
the base panel. A missing scheme leaves the list unbuilt. `csr_host_paint_gameui`
then sizes the root and its non-popup children and paints the tree. A panel
that paints calls `PushMakeCurrent`. When that scheme file is on the root,
the same paint applies scheme settings. That reads
`resource/BackgroundLayout.txt`. A texture name with no extension is the
`.tga` file beside it. A missing file is not a texture. `csr_host_start_gameui`
calls `IGameUI::Start` with a host engine table. It does not open `hw.so`.
Cvar reads are empty, the clock stays at 0, and recorded commands are not
executed. A missing scheme file does not call the server browser. The
background image is drawn at time 0 before that call. The paint after it
builds the options pages. `RunFrame` is not that call. Later
factories are not asked for the surface name.

Mouse move, press and release go to the three input calls and then to exactly
one message dispatch. `RunFrame` is not called, so focus cannot jump back to a
stored cursor. `SurfaceSetCursorPos` only stores the position.

The `vgui2.so` built from this SDK exports `csr_vgui_dispatch_messages`. The
host receives function pointers. Process, Steam and frame rate are stated once
in [plan section 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Build and tests

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target xash_host-tests
./build/linux-tests/out/bin/Release/xash_host-tests
```

The tests load the built surface and do not start the game.
