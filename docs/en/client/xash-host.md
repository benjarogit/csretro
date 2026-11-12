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
After `IGameUI::Start` the host loads `next_engine_mini.dll` and
`nitro_api2.dll`, calls `EngineMini::Init`, then argv, mode and init_game.
`COM_InitArgv` runs. `OnGameInitializing` reads a pointer table from this
process, runs `Con_Init`, and does not call `Sys_Error`. `-condebug` does not
call a null filesystem. `pfnGetGameDir` writes the mounted directory name.
An empty mount leaves that pointer null, and `SetCStrikeFlags` is not called.
A mounted name runs `SetCStrikeFlags`. `NET_SendPacket` is the function in this
process when that retail hook is absent. Init stores the pointer and does not
call it, so no socket is opened. A null pointer is not a call. A null hook is
not a call. `Netchan_CopyFileFragments` is then the function in this process
when that retail hook is absent. Init stores the pointer and does not call
it, so no fragment file is copied and no socket is opened. A null pointer is
not a call. A null hook is not a call. `SVC_TimeScale` is then the function
in this process when that retail hook is absent. That function is
`CL_Parse_Timescale`. Init stores the pointer and does not call it, so the
timescale stays unchanged and no socket is opened. A null pointer is not a
call. A null hook is not a call. `SVC_SendCvarValue` is then the function
in this process when that retail hook is absent. That function is
`CL_Send_CvarValue`. Init stores the pointer and does not call it, so no
cvar is sent, cvar values stay unchanged and no socket is opened. A null
pointer is not a call. A null hook is not a call. `CL_HTTPUpdate` is then
the function in this process when that retail hook is absent. Init stores
the pointer and does not call it, so no HTTP request is made and no socket
is opened. A null pointer is not a call. A null hook is not a call.
`CL_HTTPStop_f` is then the function in this process when that retail hook
is absent. Init stores the pointer and does not call it, so no download is
stopped and no socket is opened. A null pointer is not a call. A null hook
is not a call. `CL_StartResourceDownloading` is then the function in this
process when that retail hook is absent. Init stores the pointer and does
not call it, so no download is started and no socket is opened. A null
pointer is not a call. A null hook is not a call. `CL_ReadPackets` is then
the function in this process when that retail hook is absent. Init stores
the pointer and does not call it, so no packet is read and no socket is
opened. A null pointer is not a call. A null hook is not a call.
`CL_RequestMissingResources` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so no
resource is requested and no socket is opened. A null pointer is not a
call. A null hook is not a call. `CL_Disconnect` is then the function in
this process when that retail hook is absent. Init stores the pointer and
does not call it, so no disconnect runs and no socket is opened. A null
pointer is not a call. A null hook is not a call. `CL_ClearState` is then
the function in this process when that retail hook is absent. Init stores
the pointer and does not call it, so client state stays unchanged and no
socket is opened. A null pointer is not a call. A null hook is not a call.
`Host_Map_f` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so no map is loaded,
the level name stays unchanged and no socket is opened. A null pointer is
not a call. A null hook is not a call. `Host_FilterTime` is then the
function in this process when that retail hook is absent. Init stores the
pointer and does not call it, so host time and the frame interval stay
unchanged and no socket is opened. A null pointer is not a call. A null
hook is not a call. `Mod_ClearAll` is then the function in this process
when that retail hook is absent. Init stores the pointer and does not call
it, so the model list stays unchanged and no socket is opened. A null
pointer is not a call. A null hook is not a call. `Mod_FindName` is then
the function in this process when that retail hook is absent. Init stores
the pointer and does not call it, so no model is looked up or loaded and
no socket is opened. A null pointer is not a call. A null hook is not a
call. `Mod_Print` is then the function in this process when that retail
hook is absent. Init stores the pointer and does not call it, so the model
list is not printed and no socket is opened. A null pointer is not a call.
A null hook is not a call. `Mod_ValidateCRC` is then the function in this
process when that retail hook is absent. Init stores the pointer and does
not call it, so no CRC is checked, no model is looked up or rejected and
no socket is opened. A null pointer is not a call. A null hook is not a
call. `Mod_NeedCRC` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so
the need-CRC flag stays unchanged, no model is looked up and no socket is
opened. A null pointer is not a call. A null hook is not a call.
`Mod_LoadModel` is then the function in this process when that retail hook
is absent. Init stores the pointer and does not call it, so no model is
loaded, no model file is read, no CRC is computed and the need-CRC flag
stays unchanged. A null pointer is not a call. A null hook is not a call.
`Mod_ForName` is then the function in this process when that retail hook
is absent. Init stores the pointer and does not call it, so no model is
looked up or loaded, no model file is read, no model slot is allocated,
no CRC is computed and needload stays unchanged. A null pointer is not a
call. A null hook is not a call. `Mod_Extradata` is then the function in
this process when that retail hook is absent. Init stores the pointer and
does not call it, so no extradata is read, no model is loaded, no model
file is read, no CRC is computed, needload stays unchanged and `Sys_Error`
does not run. A null pointer is not a call. A null hook is not a call.
`SPR_Init` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so an empty sprite
list stays unallocated, no sprite is loaded, no file is read and no socket
is opened. A null pointer is not a call. A null hook is not a call.
`SPR_Shutdown` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so an empty sprite
list stays unallocated, an allocated sprite list stays allocated, no sprite
texture is unloaded, no sprite is loaded, no file is read and no socket is
opened. A null pointer is not a call. A null hook is not a call.
`SPR_Shutdown_NoModelFree` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so an
empty sprite list stays unallocated, an allocated sprite list stays at the
same pointer, and its sprite name stays at the same pointer. No sprite
texture is unloaded, no sprite name is freed, no sprite is loaded, no file
is read and no socket is opened. A null pointer is not a call. A null hook
is not a call.
`SPR_Load` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so an empty sprite
list stays unallocated, an allocated sprite list stays at the same pointer
with the same count, and its sprite name stays at the same pointer. No
sprite is loaded, no sprite name is allocated, `Mod_ForName` does not run,
`Sys_Error` does not run, no file is read and no socket is opened. A null
pointer is not a call. A null hook is not a call.
`SPR_Draw` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so the current sprite
stays at the same pointer, no sprite frame is drawn, `Draw_SpriteFrame` does
not run, no sprite is loaded and no socket is opened. A null pointer is not
a call. A null hook is not a call.
`SPR_DrawAdditive` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so the current sprite
stays at the same pointer, no sprite frame is drawn additively,
`Draw_SpriteFrameAdditive` does not run, no sprite is loaded and no socket is
opened. A null pointer is not a call. A null hook is not a call.
`SPR_DrawHoles` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so the current sprite
stays at the same pointer, no sprite frame is drawn with holes,
`Draw_SpriteFrameHoles` does not run, no sprite is loaded and no socket is
opened. A null pointer is not a call. A null hook is not a call.
The next retail hook is `SPR_DrawGeneric`. This path returns before it.
`csr_host_frame` is a later direct call and does not extend that list.
`OnGameInitialized` is not entered. Shutdown and the fatal error stay
bound and that open does not call them. A missing library or export fails
the load and is not
called. The load does not open `hw.so` and does not call `RunFrame`.
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

`csr_host_frame` paints one GameUI frame through that present path
(`ActivateGameUI`, then the paint above) and adds the caller's delta to
host time. It writes `realtime`, `host_frametime` and `oldrealtime` directly.
`Host_FilterTime` stays uncalled and no retail hook is added.
`CVGui::RunFrame` stays uncalled. `hw.so` stays unmapped, no Steam client
starts, and the planned launcher command is not executed. The frame opens
the X11 display and chooses an EGL config whose native visual can be a child
of the root window. A visual at the root depth is preferred. A border pixel
is set so a deeper visual is not a BadMatch. The window uses that visual,
not the parent's, then calls `eglCreateWindowSurface`, paints and calls
`eglSwapBuffers`. A failed call does not swap and does not move host time.
`csr_host_pump` calls that frame a given number of times on the same mapped
window, forwards X11 motion and button events to `csr_host_input_move` and
`csr_host_input_button`, then destroys the window. After N frames the swap
count is N and host time has advanced by N times the delta. `csr_host_run` keeps that window mapped and repeats the frame until the recorded client command is `quit`. `QuitNoConfirm` records that command through `pfnClientCmd` and does not run it. Motion and button events still reach `csr_host_input_move` and `csr_host_input_button`. The call then destroys the window. The swap count is the number of frames and host time advances by that count times the delta. `PresentXashLauncher` paints one frame and returns. It does not call this loop. `RunXashLauncher` opens the host the way `OpenXashLauncher` does and then calls `csr_host_run`. The human process is `client/build/linux/out/bin/Release/xash_launcher`. Its `main` calls `RunXashLauncher`. The content root is the first argument and the map name is the second. When a map name is given, `RunXashLauncher` reads `cstrike/maps/<name>.bsp` from that root before the quit loop and records the requested name and a checksum of the bytes it read on the client state. The file has to be a version 30 BSP whose lumps sit inside the file. A missing file, a short file, or a different version fails and does not change the client state, so a null `worldmodel` stays null and the previous map name stays. When the file is valid, that load builds a brush world model from the validated bytes and stores it on `client_state->worldmodel` before the quit loop. The model name is the requested map name. A non-empty entities lump is copied onto that model. Vertex, edge, surfedge and face lumps are copied onto the world model. One face becomes a surface, so `numsurfaces` is at least 1 and that face's vertex bytes match the fixture. Texinfo and textures are not copied. Lighting bytes are not lightmap surfaces, and `GL_BuildLightmaps` does not run. The 128-byte fixture checksum `0xa4ebf95d` still has empty face, texinfo, texture and lighting lumps, so `numsurfaces` stays 0 for that file. After a valid load, `r_worldentity->model` is that world model. A direct draw in this process walks that face through its surfedges and submits those vertices as one primitive. `RunXashLauncher` calls that draw once after a successful bind. The human frame loop `csr_host_run` does not call it, so the menu frame does not show the map. A failed load does not draw and does not replace a previous renderer world pointer. `eng()->R_DrawWorld` is not installed and not called. `eng()->R_NewMap` is not installed and not called. `eng()->Mod_LoadBrushModel` is not that call. `Host_Map_f` is not that call. When no map name is given, the call does not load a map. The call does not play Counter-Strike. The X11 display connection
is the window's socket. No game socket is opened. `ISurface::SwapBuffers`
stays empty. The surface tests still use an EGL pbuffer.
The GameUI engine-table clock stays at 0. `OnGameInitialized` stays out,
because that entry loads material files, resolves `vgui2` and continues into
the rest of retail init.

`csr_host_load_pair` binds mouse move, press and release to that pair's
`VGUI_InputInternal001`. Each event ends with exactly one
`csr_vgui_dispatch_messages`. `IInputInternal::RunFrame` and `CVGui::RunFrame`
are not called, so focus cannot jump back to a stored cursor.
`SurfaceSetCursorPos` only stores the position. A manual `csr_host_bind_input`
remains for tests with fake callbacks. The host root hit-tests its children. Paint associates that root with the
default input context, so the hit test walks the embedded tree. A menu hover
message carries the panel handle, because a `VPANEL` does not fit in a
KeyValues int on LP64.
`csr_host_activate_gameui` calls `IGameUI::ActivateGameUI` after `Start` and
does not call `RunFrame`. A click on a laid-out `GameMenu.res` item is
delivered through that input bind. `QuitNoConfirm` records `quit` through
`pfnClientCmd` and does not run it. LP64 edits to the vendored SDK, including
that menu-handle message, are the files under `client/dep/ncl-hl1-source-sdk/`.
The commit in `versions.lock.json` is the upstream base this tree started from.

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
