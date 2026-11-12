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
`libsteam_api.so`. `SteamAPI_IsSteamRunning` stays false. The planned command
is not executed. The launcher does not start the game, does not exec Xash3D,
does not call `RunFrame` and does not start Steam. A failed open leaves the
host closed.

`nextclient.sh` and [First client start](first-start.md) remain the GoldSrc
path until this path replaces them.
