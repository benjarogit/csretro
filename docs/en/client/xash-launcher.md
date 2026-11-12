# Xash launcher

The Xash launcher assembles the pieces that are already built: `xash_host`,
the one `VGUI_Surface026`, `client_mini`, `vgui2`, `gameui`, the
`VFileSystem009` root and the ELF64 `libsteam_api.so`. It does not add a
second surface, a second factory or a second identity slot.

The caller passes the content root. A missing directory is a failure. The
launcher does not search a Steam library and does not invent a path. Original
game content, maps, models, sounds and `valve/resource` stay required and are
not in the repository.

The same process opens that list through the host: the one surface, then
`client_mini` and `vgui2`, then `gameui`, then the filesystem root, then
`IVGui::Init` on that list. A scheme file in the root is loaded there. A
missing scheme file leaves the host open. `IGameUI::Initialize` follows.
Then `libsteam_api.so`.
`SteamAPI_IsSteamRunning` stays false. The planned command is not executed. The launcher does not start the game, does not exec Xash3D,
does not call `RunFrame` and does not start Steam. A failed open leaves the
host closed. `PresentXashLauncher` then calls `ActivateGameUI` and paints
that menu at the given size. `Quit` is not run. `setpause` is only recorded.
A missing scheme leaves the options pages unbuilt, and a server browser
without a dialog is not opened.

`nextclient.sh` and [First client start](first-start.md) remain the GoldSrc
path until this path replaces them.
