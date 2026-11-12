# Xash launcher

The Xash launcher assembles the pieces that are already built: `xash_host`,
the one `VGUI_Surface026`, `client_mini`, `vgui2`, `gameui`, the
`VFileSystem009` root, the ELF64 `libsteam_api.so`, `next_engine_mini.dll`
and `nitro_api2.dll`. It does not add a
second surface, a second factory or a second identity slot.

Installation searches the Steam library and copies the mounted content
outside it. See [Install game content](install.md). The open receives that
copy as the content root. A missing directory is a failure. The copy does not
invent a path, and this open does not start Steam. Original game content,
maps, models, sounds and `valve/resource` stay required and are not in the
repository.

The same process opens that list through the host: the one surface, then
`client_mini` and `vgui2`, then `gameui`, then the filesystem root, then
`IVGui::Init` on that list. A scheme file in the root is loaded there. A
missing scheme file leaves the host open. `IGameUI::Initialize` follows.
Then `libsteam_api.so`.
`SteamAPI_IsSteamRunning` stays false. After `IGameUI::Start` the same open
loads `engine_mini`, calls `EngineMini::Init`, then argv, mode and init_game.
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
`SPR_DrawGeneric` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so the current sprite
stays at the same pointer, no sprite frame is drawn with the caller blend,
`Draw_SpriteFrameGeneric` does not run, no sprite is loaded and no socket is
opened. A null pointer is not a call. A null hook is not a call.
`SPR_EnableScissor` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so a planted scissor
rectangle stays the same, `EnableScissorTest` does not run, no sprite is loaded
and no socket is opened. A null pointer is not a call. A null hook is not a call.
`SPR_DisableScissor` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so a planted scissor
rectangle stays the same, `DisableScissorTest` does not run, no sprite is loaded
and no socket is opened. A null pointer is not a call. A null hook is not a call.
`SPR_Set` is then the function in this process when that retail hook is absent.
Init stores the pointer and does not call it, so the current sprite stays at the
same pointer with the same color and scale, `qglColor4f` does not run, no sprite
is loaded and no socket is opened. A null pointer is not a call. A null hook is
not a call. `SPR_Width` is then the function in this process when that retail hook
is absent. Init stores the pointer and does not call it, so planted sprite
dimensions stay unchanged, no sprite width is read, no sprite is loaded and no
socket is opened. A null pointer is not a call. A null hook is not a call.
`SPR_Height` is then the function in this process when that retail hook
is absent. Init stores the pointer and does not call it, so planted sprite
dimensions stay unchanged, no sprite height is read, no sprite is loaded and no
socket is opened. A null pointer is not a call. A null hook is not a call.
`SPR_Frames` is then the function in this process when that retail hook is
absent. Init stores the pointer and does not call it, so planted sprite frame
and dimension fields stay unchanged, no sprite frame count is read, no sprite is
loaded and no socket is opened. A null pointer is not a call. A null hook is not
a call. `SPR_GetModelPointer` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so planted
sprite and model fields stay unchanged, no model pointer is resolved, no sprite
is loaded and no socket is opened. A null pointer is not a call. A null hook is
not a call. `R_StudioReloadSkin` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so planted
studio and skin fields stay unchanged, no studio skin is reloaded, no texture is
touched, no sprite is loaded and no socket is opened. A null pointer is not a
call. A null hook is not a call. `SetCrosshair` is then the function in this
process when that retail hook is absent. Init stores the pointer and does not
call it, so planted crosshair sprite, color and rectangle stay unchanged, no
crosshair is set, no sprite is loaded and no socket is opened. A null pointer is
not a call. A null hook is not a call. `DrawCrosshair` is then the function in
this process when that retail hook is absent. Init stores the pointer and does
not call it, so planted crosshair sprite, color and rectangle stay unchanged, no
crosshair is drawn, no sprite is loaded and no socket is opened. A null pointer
is not a call. A null hook is not a call. `R_RenderView` is then the function in
this process when that retail hook is absent. Init stores the pointer and does
not call it, so no view is rendered, no GL context is opened from this init, no
sprite is loaded and no socket is opened. A null pointer is not a call. A null
hook is not a call. `GL_Init` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so GL is
not initialized, no GL context is opened from this init, no sprite is loaded and
no socket is opened. A null pointer is not a call. A null hook is not a call.
`GL_Config` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so no GL
config is read or written, no GL context is opened from this init, no sprite is
loaded and no socket is opened. A null pointer is not a call. A null hook is not
a call. `GL_SelectTexture` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so no
texture unit is selected, no GL context is opened from this init, no sprite is
loaded and no socket is opened. A null pointer is not a call. A null hook is not
a call. The next retail hook is `CL_ConnectClient`. This path returns before it.
`csr_host_frame` is a later direct call and does not extend that list.
`OnGameInitialized` is not entered. Shutdown and the fatal error stay
bound and are not called. A missing library or export fails the open and is
not called.
The planned command is not executed. The launcher does not start the game, does not exec Xash3D,
does not call `RunFrame` and does not start Steam. A failed open leaves the
host closed. `PresentXashLauncher` then calls `ActivateGameUI` and paints
that menu at the given size. `Quit` is not run. `setpause` is only recorded.
A missing scheme leaves the options pages unbuilt, and a server browser
without a dialog is not opened.

`csr_host_frame` paints one GameUI frame through that present path and adds
the caller's delta to host time by writing `realtime`, `host_frametime` and
`oldrealtime`. `Host_FilterTime` stays uncalled and no retail hook is added.
`CVGui::RunFrame` stays uncalled. The planned command is not executed and
Steam does not start. The frame opens the X11 display and chooses an EGL
config whose native visual can be a child of the root window. A visual at
the root depth is preferred. A border pixel is set so a deeper visual is
not a BadMatch. The window uses that visual, not the parent's, then calls
`eglCreateWindowSurface`, paints and calls `eglSwapBuffers`. A failed call
does not swap and does not move host time. `csr_host_pump` calls that frame
a given number of times on the same mapped window, forwards X11 motion and
button events to `csr_host_input_move` and `csr_host_input_button`, then
destroys the window. After N frames the swap count is N and host time has
advanced by N times the delta. `csr_host_run` keeps that window mapped and repeats the frame until the recorded client command is `quit`. `QuitNoConfirm` records that command through `pfnClientCmd` and does not run it. Motion and button events still reach `csr_host_input_move` and `csr_host_input_button`. The call then destroys the window. The swap count is the number of frames and host time advances by that count times the delta. `PresentXashLauncher` paints one frame and returns. It does not call this loop. `RunXashLauncher` opens the host the way `OpenXashLauncher` does and then calls `csr_host_run`. The human process is `client/build/linux/out/bin/Release/xash_launcher`. Its `main` calls `RunXashLauncher`. The content root is the first argument and the map name is the second. When a map name is given, `RunXashLauncher` reads `cstrike/maps/<name>.bsp` from that root before the quit loop and records the requested name and a checksum of the bytes it read on the client state. The file has to be a version 30 BSP whose lumps sit inside the file. A missing file, a short file, or a different version fails and does not change the client state, so a null `worldmodel` stays null and the previous map name stays. When the file is valid, that load builds a brush world model from the validated bytes and stores it on `client_state->worldmodel` before the quit loop. The model name is the requested map name. A non-empty entities lump is copied onto that model. Vertex, edge, surfedge and face lumps are copied onto the world model. One face becomes a surface, so `numsurfaces` is at least 1 and that face's vertex bytes match the fixture. Texinfo and textures are not copied. Lighting bytes are not lightmap surfaces, and `GL_BuildLightmaps` does not run. The 128-byte fixture checksum `0xa4ebf95d` still has empty face, texinfo, texture and lighting lumps, so `numsurfaces` stays 0 for that file. After a valid load, `r_worldentity->model` is that world model. A direct draw in this process walks that face through its surfedges and submits those vertices as one primitive. `RunXashLauncher` calls that draw once after a successful bind. When a world model is bound, the human frame loop `csr_host_run` calls that draw once on each frame. The draw records the primitive and does not write the color buffer, so the swapped window stays the menu. When no world model is bound, the frame stays the menu and the draw count stays 0. `PresentXashLauncher` still returns after one frame and does not submit a world primitive. A failed load does not draw and does not replace a previous renderer world pointer. `eng()->R_DrawWorld` is not installed and not called. `eng()->R_NewMap` is not installed and not called. `eng()->Mod_LoadBrushModel` is not that call. `Host_Map_f` is not that call. When no map name is given, the call does not load a map. The call does not play Counter-Strike. The X11 display connection is the window's socket. No game socket is
opened. `ISurface::SwapBuffers` stays empty. The surface tests still use an
EGL pbuffer.

A human runs this command. `$CONTENT_ROOT` is the copy from [Install game content](install.md). It stays outside the Steam library and is not in this repository. `<map>` is the map name, without a directory and without `.bsp`. The command does not start Steam and does not exec the planned launcher command. With a map name, `main` passes that name to `RunXashLauncher`, which reads `cstrike/maps/<map>.bsp` from the content root and builds the world model before the quit loop. Without a map name the call does not load a map. The command does not play Counter-Strike.

```bash
client/build/linux/out/bin/Release/xash_launcher "$CONTENT_ROOT" <map>
```

`nextclient.sh` and [First client start](first-start.md) remain the GoldSrc
path until this path replaces them.
