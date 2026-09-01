# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach substantieller Arbeit Tabelle und „Offene Arbeit“ in derselben Session nachziehen.

Details: `ROLLEN.md`, `PLATTFORMEN.md`, `SERVER.md`, `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `PHASEN.md`, `PHASE3-BODY.md` (nur während Phase 3), `CHANGELOG.md`.

## Aktueller Stand

| Feld | Wert |
|------|------|
| Datum | 2026-09-01 |
| Phase | **3A/3B abgenommen.** 3C unvollständig (keine eigene GameDLL). **3D nicht beginnen.** |
| Körper-Quelle | **A1** — Manifest in `ROLLEN.md` |
| Stapel | Xash → Export → A1-Body → (später) NextClient-Funktionen |
| Plattform | **64-Bit only** — `docs/PLATTFORMEN.md` |
| GitHub | https://github.com/benjarogit/csretro (**privat**) |
| Branch | `main` |
| Letztes Release | `v0.1.4-phase2` — Phase 3 ist **kein** Abschluss-Release |
| Lokaler Worktree | `/home/benny/Dokumente/csretro` |

## Rollen (Kurz)

| | Rolle |
|---|--------|
| CS Retro | Produkt-Worktree |
| Xash3D-FWGS | Engine (`engine/`) |
| NextClient | funktionales Zielverhalten |
| Ref A | Client-Body-Quelle (A1-Manifest) |
| Ref B | bedingte Menü-Referenz, Phase 4 |
| Server | GameDLL geplant `server/game/` — `docs/SERVER.md` |
| Bots | leer, eigenes Interface |

```
Xash3D-FWGS → Export → Body (A1) → NextClient-Funktionen
Xash3D-FWGS → GameDLL → optional Module → separat Bots
```

Eine Client-Lib, eine GameDLL. Kein „cs16-client weiterentwickeln“.

## Pfade

| Was | Wo |
|-----|-----|
| Worktree | `/home/benny/Dokumente/csretro` |
| Engine | `engine/` |
| NextClient-Herkunft | `client/nextclient/` |
| Export | `client/export/` (`GetClientAPI`) |
| Body | `client/body/` |
| Server-AMXX-Herkunft | `server/` |
| GameDLL (geplant) | `server/game/` — noch nicht vendort |
| Bots | `bots/` (leer) |
| Ref A / B | `refs/a-cs16-client/` · `refs/b-cs16-goldsrc/` |
| Spielinhalte | `gamedata/` (nicht im Git) |
| Build | `build/` (nicht im Git) |

## GitHub

Remote: `https://github.com/benjarogit/csretro.git`. Privat.

**Nie pushen:** `CLAUDE.md`, `AGENTS.md`, `.cursor/`, `.claude/`, `gamedata/`, `valve/`, `cstrike/`, Builds, Steam-DLLs, WoltLab-Pakete.

**Immer:** diese Tabelle, `CHANGELOG.md`, bei Vendor `UPSTREAM.md`.

Abschluss-Release nur wenn die Phase wirklich fertig ist. 3A/3B-Zwischenstand darf auf `main` liegen ohne Phase-3-Tag.

## Runtime (dieser Rechner)

- CachyOS x86_64 · Clang 22.1.8
- CMake 4.4.3: `CMAKE_ROOT=/usr/share/cmake` (sonst `--preset`/`-S` bricht)
- Engine: `./scripts/build-engine.sh` → `build/engine/`
- Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`
- Testdaten: `XASH3D_RODIR` = Steam-HL (nur Maps/WADs), `XASH3D_BASEDIR` = `build/run/`
- Menü: Xash-MainUI (`libmenu.so`)

## Quickstart

```bash
git clone git@github.com:benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++
./scripts/build-engine.sh
./scripts/build-client.sh
```

Inhalte: `gamedata/valve` + `gamedata/cstrike` oder Steam-HL als `XASH3D_RODIR`.
Client: `-clientlib` auf `client_amd64.so`. Keine Steam-`client.dll`.

## Offene Arbeit

1. **GameDLL-Gate umsetzen:** `rehlds/ReGameDLL_CS` nach `server/game/` vendorn (`docs/SERVER.md`). Nicht Ref-A-ReGameDLL. Keine Bots mitziehen.
2. Linux x86_64: GameDLL bauen, Xash laden, **Listen-Server + Map** — dann 3C (Render/Input/Movement/HUD/Waffen/Connect) zuende.
3. **3D erst danach.** Erstes Feature: FOV.
4. Windows x86_64 / macOS ARM64+x86_64: Compile-Gates, sobald GameDLL im Tree ist.
5. Phase 4 nur bei Bedarf: NextClient-Menüs; Ref B ein Feature.

## Nicht anfassen

- 3D / NextClient-Feature-Port vor vollständiger 3C
- Ref A außerhalb des A1-Manifests
- Ref-A-ReGameDLL / YaPB / Ref-A-mainui
- Ref B vor Phase 4
- AMXX/Metamod in die GameDLL backen
- 32-Bit-Targets, Steam-Bind, `git submodule add`
- `CLAUDE.md` / Cursor-Attribution
