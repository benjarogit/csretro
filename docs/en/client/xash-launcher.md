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
call. A null hook is not a call. The next retail hook is `SVC_SendCvarValue`.
This path returns before it. `OnGameInitialized` is not
entered. Shutdown and the fatal error stay
bound and are not called. A missing library or export fails the open and is
not called.
The planned command is not executed. The launcher does not start the game, does not exec Xash3D,
does not call `RunFrame` and does not start Steam. A failed open leaves the
host closed. `PresentXashLauncher` then calls `ActivateGameUI` and paints
that menu at the given size. `Quit` is not run. `setpause` is only recorded.
A missing scheme leaves the options pages unbuilt, and a server browser
without a dialog is not opened.

`nextclient.sh` and [First client start](first-start.md) remain the GoldSrc
path until this path replaces them.
