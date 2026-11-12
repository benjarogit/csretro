# xash_host

The host owns the `VGUI_Surface026` factory. `vgui2.so` does not link this
library. Xash3D is not patched. `LD_PRELOAD` stays a spike tool.

- `csr_host_open` loads `xash_surface.so` with `RTLD_LOCAL` and creates the
  one surface. The first factory entry is `csr_surface_create_interface`.
  The same object is bound into `xash_engine` for the vgui2 forwards.
- `csr_host_load_pair` opens the matched `client_mini` and `vgui2` after that
  factory. A missing library or export returns 0 and is not called. It binds
  cursor, press and release to that `VGUI_InputInternal001`, then one
  `csr_vgui_dispatch_messages` per event. `IInputInternal::RunFrame` and
  `CVGui::RunFrame` stay unused. The pair does not add a factory.
- `csr_host_load_gameui` opens `gameui` after that factory and reads
  `GameUI007` and `GameConsole003`. It does not call into the game UI and
  does not add a factory.
- `csr_host_mount_filesystem` puts `VFileSystem009` on that same list after
  the surface and installs that root as the surface file reader.
  `DrawSetTextureFile` loads an uncompressed TGA from it. A missing file or
  a path outside that root is a miss. The proxy does not open `hw.so`.
- `csr_host_connect_vgui` calls `IVGui::Init` after the pair and the mount.
  `vgui2` takes the one surface, `VFileSystem009` and `VGUI_Localize003`.
  A missing piece returns 0 and is not called. `RunFrame` stays unused.
- `csr_host_load_scheme` reads one scheme file from that root after the
  connection. A missing file returns 0 and leaves the host open. The launcher
  asks for `resource/schemes/TrackerScheme_ClassicPlus.res`.
- `csr_host_init_gameui` calls `IGameUI::Initialize` after that connection.
  A missing `next_engine_mini` module is not called. The base panel is
  visible and a child of the host root. The server list is built when
  `resource/schemes/TrackerScheme_ClassicPlus.res` is on that root. `RunFrame`
  stays unused.
- `csr_host_start_gameui` calls `IGameUI::Start` with a host engine table.
  It does not open `hw.so`. Cvar reads are empty, the clock stays at 0, and
  recorded commands are not executed. A missing scheme file does not call the
  server browser. `RunFrame` stays unused.
- `csr_host_load_engine` opens `next_engine_mini.dll` and `nitro_api2.dll`
  after the surface factory is live. It calls `EngineMini::Init`, then argv,
  mode and init_game. `COM_InitArgv` runs. A missing library or export
  returns 0 and is not called. A successful load does not call shutdown or
  the fatal error. `OnGameInitializing` reads the Xash pointer table and does
  not call `Sys_Error`. `hw.so` stays closed.
  `RunFrame` stays unused.
- `csr_host_paint_gameui` sizes the root and its non-popup children, solves
  the tree and paints it. A paint calls `PushMakeCurrent`. When
  `resource/schemes/TrackerScheme_ClassicPlus.res` is on the root, that paint
  applies scheme settings and reads `resource/BackgroundLayout.txt`. A texture
  name with no extension is the `.tga` beside it. A missing file is not a
  texture. The background image is drawn at time 0 before `Start`. The paint
  after `Start` builds the options pages. `RunFrame` stays unused.
- Extra factories follow those entries. The surface name is not forwarded to them.
- `csr_host_bind_input` forwards cursor, press and release, then one dispatch.
  `CVGui::RunFrame` is not called. `SurfaceSetCursorPos` does not dispatch.
- Our `vgui2` exports `csr_vgui_dispatch_messages` through
  `patches/sdk/0001-export-vgui-dispatch.patch`. The SDK submodule pin does
  not move. Retail `vgui2` keeps `DispatchMessages` local, so the host takes
  a function pointer instead of a hardcoded address.
