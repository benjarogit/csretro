# CS-Retro-GameDLL — Source-Manifest

Produkt-Wahrheit: `cmake/CsretroGameDll.cmake` → Target `csretro_gamedll`.
Pin: `UPSTREAM_PIN` / `ATTRIBUTION.md`. Kein Upstream-CMake, keine alte SLN.

## Baum

| Pfad | Rolle |
|------|--------|
| `regamedll/dlls/` | Spiellogik, Waffen, GameRules, Hostage, **ZBot** (`dlls/bot/`) |
| `regamedll/dlls/wpn_shared/` | Shared-Waffen |
| `regamedll/dlls/API/` | ReGameDLL-API (mitvendort, keine Pflicht für Vanilla-Listen) |
| `regamedll/dlls/addons/` | Extra-Entities |
| `regamedll/game_shared/` | Shared inkl. **Bot-Nav** (`game_shared/bot/`) |
| `regamedll/pm_shared/` | PlayerMove (Server) |
| `regamedll/engine/` | GameDLL-Engine-Header + `unicode_strtools.cpp` |
| `regamedll/common/` | GoldSrc-Shared-Header |
| `regamedll/public/` | Filesystem, interface, tier0, `build.h` |
| `regamedll/regamedll/` | Hookchains, Precompiled, SSE-Math |
| `version/appversion.h` | eingefrorener Pin `b088984` |
| `version_script.lds` | Linux-Export-Map |

## Plattformquellen

| Datei | Wann |
|-------|------|
| `public/tier0/platform_posix.cpp` | Linux / macOS |
| `public/tier0/platform_win32.cpp` | Windows |
| `regamedll/sse_mathfun.cpp` | nicht ARM64 |

## Bots (Migration, vollständig mitgebaut)

Nicht amputiert. Liegen in der GameDLL, nicht in `bots/`.

- `dlls/bot/cs_bot*.cpp`, `dlls/bot/cs_gamestate.cpp`, `dlls/bot/states/*`
- `game_shared/bot/{bot,bot_manager,bot_profile,bot_util,nav_*}.cpp`
- Hostage-Improv/Nav teilt sich Nav-Code mit ZBot

Endzustand: eigener CS-Retro-Bot in `bots/`, dann redundante Implementierungen entfernen.

## Nicht übernommen

`.git`, `.github`, `msvc/`, `dep/`, `extra/`, `unittests/`, `dist/`, Upstream-`CMakeLists.txt`, `regamedll/cmake/`.
