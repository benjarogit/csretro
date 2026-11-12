# Install game content

CSRETRO copies the Counter-Strike 1.6 files the client mounts into a directory
outside the Steam library. The copy does not install into the Steam folder,
does not start Steam, and does not start the game.

You need a retail Counter-Strike 1.6 install (App 10). The files are not in
this repository. Never point the destination at the Steam library, and never
join a VAC-secured server with the modified client.

## Search

The tool reads a Steam root, then `steamapps/libraryfolders.vdf`, then
`steamapps/appmanifest_10.acf`. The fixture under `tests/install_content/fixtures/`
is the current library-folder object (`path` plus `apps`) and the older string
form of the same file. The manifest `installdir` must be a relative directory
under `steamapps/common` that contains `cstrike` and `valve`. The first valid
App 10 in that order is the source. A later library is not a fallback when the
first one is a complete game.

| Result | Meaning |
|---|---|
| `steam_missing` | The root has no Steam layout. |
| `game_missing` | Steam is there, and App 10 is missing or not mountable. |
| `steam_metadata_invalid` | `libraryfolders.vdf` or the app manifest is not valid KeyValues. |
| `destination_inside_source` | The destination is inside the Steam library or the game tree. |
| `destination_not_empty` | The destination already contains files. |
| `destination_unavailable` | The destination is a file or the filesystem root. |
| `overlay_inside_source` | The overlay is inside the Steam library or the game tree. |
| `overlay_missing` | The overlay path that was passed does not exist. |

Nothing is invented. A failure writes no copy.

`--steam-root` limits the search to that tree. You can pass it more than once.
Without it, the tool uses the Linux Steam locations: `~/.steam/steam`,
`~/.local/share/Steam`, and the Steam flatpak data directory. Tests pass a
fake library and do not read a real Steam install.

## What is copied

The destination keeps the Half-Life layout. The allowlist is
`tools/install_content.py`.

| Tree | Copied |
|---|---|
| `cstrike/` | maps, models, sounds, sprites, gfx, resource, events, overviews, classes, logos, WAD files, and the stock root files the game reads (`liblist.gam`, `delta.lst`, menus, mapcycle, motd) |
| `valve/` | models, sounds, sprites, gfx, resource (including `marlett.ttf` and `BackgroundLayout.txt`), media, scripts, logos, WAD files, and the same kind of root files |
| `platform/resource/` | localization and the Linux fonts |

A tree that is missing one of those mounted groups, `marlett.ttf`, or
`BackgroundLayout.txt` fails as `game_missing` and writes nothing.

## Shipped files win

The Steam copy is the required base. Files CSRETRO itself ships
(`client/assets`) are an overlay on that copy. When the relative path is the
same, the shipped file is the one the client resolves. That covers maps,
weapons, and every other shipped asset with the same name: models, sounds,
sprites, and resources. A retail file with no shipped replacement stays. The
retail copy does not overwrite a file we ship.

`--overlay` selects that tree. When the flag is omitted, the tool uses
`client/assets` if the directory exists. A test passes a fake shipped tree
and does not read a Steam install. An overlay inside the Steam library is
refused as `overlay_inside_source` and writes nothing. A missing overlay path
is `overlay_missing`.

## What stays behind

Steam and VAC binaries, `hw.so` and the other engine binaries, retail
`cl_dlls` and `dlls`, personal configs (`config.cfg`, `autoexec.cfg`, and the
other local cfg files), demos, screenshots, caches, `htmlcache`,
`cstrike_downloads`, `cstrike_hd`, foreign mods, `platform/config`, and
`platform/steam`.

## Command

```bash
scripts/install-content.sh --steam-root <steam-root> --destination <directory-outside-steam>
```

Success prints `ok copied=<count>`. The game is not started. The Xash open
receives this directory as its content root; see
[Xash launcher](xash-launcher.md). The GoldSrc check in
[First client start](first-start.md) is a separate full copy because that
check still loads the retail engine.
