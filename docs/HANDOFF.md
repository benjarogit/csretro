# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Details: `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `PHASEN.md`.

## Was das ist

Fork/Vendor von NextClient + NextClientServerApi, Laufzeit **nur** Xash3D-FWGS.
Vier getrennte Bereiche: `engine/` `client/` `server/` `bots/`.
Refs in `refs/` sind eingefroren — kein Code-Copy.

## Pfade

| Was | Wo |
|-----|-----|
| Worktree | `/home/benny/Dokumente/csretro` |
| Engine-Quellen | `engine/` (Xash3D-FWGS, Waf) |
| Client-Basis | `client/` (NextClient + NitroApi + ncl-hl1-source-sdk) |
| Server-Basis | `server/` (NextClientServerApi, AMXX) |
| Bots | `bots/` (leer) |
| Ref A | `refs/a-cs16-client/` |
| Ref B | `refs/b-cs16-goldsrc/` |
| Spielinhalte | lokal `gamedata/` (nicht im Git) — `valve/` + `cstrike/` aus legalem HL/CS |
| Build | `build/` |

## Runtime (dieser Rechner, 2026-09-01)

- OS: CachyOS / Linux x86_64
- CMake 4.4.3 — `cmake -S/-B` und `--preset` scheitern mit `CMAKE_ROOT`/`LoadCache` (CachyOS-Paket). Engine-Build läuft über Waf, nicht über CMake.
- Clang 22.1.8, Ninja 1.13, Python 3.14
- SDL2 + FreeType vorhanden (pkg-config)
- Cross-Compiler i686 / aarch64: **nicht installiert** (Toolchain-Dateien liegen trotzdem)
- Engine-Build (2026-09-01): OK — `build/engine/game_launch/xash3d`, `libxash.so`, `libref_gl.so`, `libmenu.so`, `filesystem_stdio.so`

## Quickstart

```bash
export CC=clang CXX=clang++
./scripts/build-engine.sh          # Xash 64-bit via Waf + Clang
```

Spielstart braucht `gamedata/valve` + `gamedata/cstrike` und die Engine-Binaries aus `build/engine`.
Root-CMake (`CMakePresets.json`) ist vorbereitet, auf diesem Host aber nicht konfigurierbar, bis das CMake-Paket steht.

## Offene Arbeit

1. **Phase 1:** Interface-Liste ncl-hl1-SDK ↔ Xash (Ref A nur lesen).
2. NextClient ist Steam-Hook (8684/Win), kein `GetClientAPI` — Bindung neu.
3. Server ist AMXX, keine Xash-GameDLL.
4. Steam-Code Phase 2 entfernen.
5. NextClient ohne LICENSE — kein öffentlicher GitHub-Release.

## Nicht anfassen

- `refs/**` nicht weiterentwickeln, nicht nach Basis kopieren
- kein `git submodule add` für Projektquellen
- kein Steam-Deploy, kein VAC-Pfad
- kein YaPB/ReGameDLL-Import aus Ref A nach `bots/`/`server/`
- kein Ref-B-Menü vor Phase 4
- `CLAUDE.md` / Cursor-Attribution nicht committen

## Altes Monorepo

Der Ordner war leer. Es gibt hier keinen `cl_dll/csretro/`-Bestand zu retten.
