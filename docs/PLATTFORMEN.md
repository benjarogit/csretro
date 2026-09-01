# Plattformen (64-Bit only)

CS Retro ist ein **reines 64-Bit-Projekt**. Nicht „x86-64 everywhere“: Apple Silicon ist ARM64.

## Desktop-Matrix

| OS | Architektur | Rolle |
|----|-------------|--------|
| Linux | x86_64 / amd64 | erstes Runtime-Ziel, möglichst distributionsneutral |
| Windows 10+ | x86_64 / amd64 | gleichberechtigt, Compile-Gate sobald Client+GameDLL stehen |
| macOS | ARM64 | modernes Hauptziel auf Apple |
| macOS | Intel x86_64 | zusätzlich, soweit sinnvoll |

Alle Komponenten einer Installation haben **dieselbe** Architektur: Engine, Client-Lib, GameDLL, Menü, später Bots.

## Verboten

- i386, i686, sonstige 32-Bit-Desktop-Targets
- stiller 32-Bit-Build (CMake bricht in `cmake/CsretroPlatform.cmake` ab)
- Engine ohne Waf-`-8`
- Mischung 32/64 im selben Prozess

Linux-aarch64 ist 64-Bit und nicht verboten, aber **kein** Produkt-Desktopziel. Toolchain-Datei bleibt optional.

## Build

```bash
export CMAKE_ROOT=/usr/share/cmake   # CachyOS/CMake 4.4
export CC=clang CXX=clang++
./scripts/build-engine.sh    # immer -8
./scripts/build-client.sh    # client_amd64.so / später client_arm64
./scripts/build-gamedll.sh   # cs_amd64.so / später cs_arm64.dylib
./scripts/smoke-gamedll.sh dedicated
```

`CMAKE_GENERATOR` nicht global setzen — CMake 4.4 verliert sonst `CMAKE_ROOT`. Die Scripts übergeben `-G Ninja` selbst.

Presets (host-nativ, kein Cross von Linux nach Windows/macOS):

- `linux-clang-x86_64`
- `windows-clang-x86_64`
- `macos-clang-arm64`
- `macos-clang-x86_64`

CI-Compile-/Link-Gates für die volle Matrix: sobald Client **und** GameDLL als Baseline stehen. Runtime-Tests dürfen später nachziehen.

## Xash-Bibliotheksnamen

Quelle: `engine/Documentation/extensions/library-naming.md` und Xash/`LibraryNaming.cmake`.
Schema: `name_$arch.$ext` auf Win/Lin/Mac bei **nicht-x86** (x86_64 und ARM64 sind nicht das nackte x86).
`$ext` kommt vom Betriebssystem, nicht von uns erfunden: Windows `dll`, Linux `so`, macOS `dylib`.

| Ziel | Client | GameDLL |
|------|--------|---------|
| Linux x86_64 | `client_amd64.so` | `cs_amd64.so` |
| Windows x86_64 | `client_amd64.dll` | `cs_amd64.dll` |
| macOS ARM64 | `client_arm64.dylib` | `cs_arm64.dylib` |
| macOS Intel x86_64 | `client_amd64.dylib` | `cs_amd64.dylib` |

CMake setzt `PREFIX ""` und `OUTPUT_NAME` + `CMAKE_SHARED_LIBRARY_SUFFIX` (plattformnative Extension).

## 64-Bit-Audit (A1-Body, 2026-09-01)

GoldSrc-Code bleibt LP64-pflichtig. Wo das Netz/Savegame 32-Bit verlangt: feste Typen (`int32_t`, `string_t` als 32-Bit-Index). Sonst `uintptr_t` / `size_t`.

| Befund | Urteil |
|--------|--------|
| `string_t` = `int` | **behalten** — Xash 64-Bit-ABI hält `string_t` bewusst 32-Bit (String-Pool) |
| miniutl `sizeof(int)==4` | ok auf LP64 |
| `environment.cpp` nutzt `intptr_t` als Rauschen | ok |
| Pointer-Diff als `int` in miniutl-Puffern | akzeptabel für GoldSrc-Größen; bei Port prüfen |
| Win32-`DWORD`-Joystick in `input_sdl` / `inputw32` | Dateien entfernt |
| Kein systematischer Pointer→`int`-Cast auf Entity-Zeigern im aktiven HUD-Pfad | weiter prüfen |
| GameDLL `MAKE_STRING` ohne `XASH_64BIT` | behoben — Pointer nicht nach `uint32` |
| GameDLL `pfnNameForFunction(uint32)` | behoben — Xash-`unsigned long` |

Ghidra: erlaubt, wenn Quelle/Refs das Originalverhalten nicht klären (Exports, Structs, ABI). Erkenntnis in Code/Tests überführen, keine Analyseartefakte im Repo lassen.

## Cross-Platform

Neuer CS-Retro-Code nicht unnötig Windows-spezifisch. Plattformteile kapseln. Der A1-Body definiert auf POSIX `LINUX`/`_LINUX` auch unter macOS — GoldSrc prüft oft `LINUX`, nicht `__APPLE__`.
