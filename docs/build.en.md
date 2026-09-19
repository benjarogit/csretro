# Building

## Scope

This describes the existing Linux x86_64 development build.
It is not a finished installation or a verified Windows/macOS guide.
Launching also requires the [game data prerequisites](getting-started.md).

You need Git, Clang/Clang++, CMake, Ninja, Python 3, pkg-config, and SDL2/OpenGL development
files. Additional library/header packages depend on the system; Waf configuration reports
missing dependencies. Several codec sources are included with the engine.
There is no installation recipe verified here for every distribution.

## Sources and components

```bash
git clone https://github.com/benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++

./scripts/build-engine.sh
./scripts/build-client.sh
./scripts/build-gamedll.sh
./scripts/build-menu.sh
```

Sources are vendored; recursive submodule updates are unnecessary.
Client and menu share a CMake build directory: run those scripts sequentially.

| Component | Typical output |
| --- | --- |
| Engine | `build/engine/game_launch/xash3d` |
| Client | `build/client-cmake/client/client_amd64.so` |
| Server | `build/gamedll-cmake/cs_amd64.so` |
| Menu | `build/client-cmake/menu/menu_amd64.so` |

All loaded libraries must match in architecture and interface version.
Do not mix in individual libraries from unrelated downloads.

## Data and launch

```bash
python3 scripts/bootstrap-gamedata.py
python3 scripts/bootstrap-gamedata.py --status
# Launch only after all additional assets are available:
./scripts/play.sh
```

Additional grenade assets are a known onboarding limitation and are not supplied by the Steam import.
See [getting started](getting-started.md). The website can be built independently.

## Verification

```bash
git diff --check
./scripts/smoke-gamedll.sh dedicated
```

The smoke test requires prepared game data. Successful linking or server startup does not verify
mouse interaction, animation or menu appearance. Report compilation, automated tests and manual
playtests separately; include the commit and any local changes.

## Common pitfalls

- Missing `client_amd64.so`, `cs_amd64.so` or menu library: build the matching component.
- Missing models/sprites: do not hide data problems with arbitrary downloads.
- CMake module paths: do not point `CMAKE_ROOT` at an unrelated system path;
  the installed CMake version and its module directory must match.
- Platform ports: existing CMake presets do not establish that a complete port has been tested.
