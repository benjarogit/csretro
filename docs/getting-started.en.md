# Getting started

## Requirements

- A 64-bit desktop. **Linux x86_64 is the currently exercised development route.**
- Your own local Steam installation of **Counter-Strike 1.6 (AppID 10)**.
- The [development tools](build.md) for a source build.
- The additional experimental grenade assets required by the current game code.

Windows x86_64 and macOS ARM64/Intel are platform goals, not fully verified installation routes
documented here. Mobile devices, consoles and 32-bit systems are outside the project scope.

!!! warning "No complete download yet"
    Existing GitHub releases are older source snapshots, not a current complete installation.
    Molotov/Incendiary models, sounds and sprites are not part of a standard CS 1.6 installation.
    Reproducible acquisition and redistribution rights are not fully documented yet.
    A successful build alone therefore does not guarantee a runnable data tree.
    We do not link replacement packs with unclear licensing.

## Import game data

The bootstrap reads the Steam installation and creates a separate data tree.
It does not modify the Steam installation.

```bash
python3 scripts/bootstrap-gamedata.py
python3 scripts/bootstrap-gamedata.py --status
```

The default target is `gamedata/` in the repository. Override it with `CSRETRO_GAMEDATA`.
If detection fails, `CSRETRO_STEAM_ROOT` can specify the Steam root.
A later `--refresh` updates imported files; inspect `--status` first.

Steam supplies data, not the engine used by the running CS Retro process.
CS Retro uses its own engine, client, menu and server libraries.
Do not substitute Steam DLLs for these libraries.

## Launch

After building all four components and preparing all required game data:

```bash
./scripts/play.sh
```

The launcher currently expects the Linux x86_64 build. Configuration and logs are stored in
`build/run/`, separately from `gamedata/`. Back up custom configurations before experimenting.
Choose a team and class, open the buy menu and start a local round.

## Troubleshooting

Check missing assets and the [project status](status.md) first.
Include the exact sequence of actions in a
[bug report](https://github.com/benjarogit/csretro/issues/new/choose).
Find the relevant session through `build/run/logs/play-latest.log`.
Share sanitized excerpts only, not private paths, server passwords or personal information.
