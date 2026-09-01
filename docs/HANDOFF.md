# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach substantieller Arbeit Tabelle und „Offene Arbeit“ in derselben Session nachziehen.

Details: `ROLLEN.md`, `PLATTFORMEN.md`, `SERVER.md`, `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `PHASEN.md`, `PHASE3-BODY.md` (nur während Phase 3), `CHANGELOG.md`. Danksagung: `CREDITS.md`.

## Aktueller Stand

| Feld | Wert |
|------|------|
| Datum | 2026-09-01 |
| Phase | **3A/3B/3C abgenommen** (Listen interaktiv auf `de_dust`). **3D nicht automatisch beginnen** (erstes Feature wäre FOV). |
| Körper-Quelle | **A1** — Manifest in `ROLLEN.md` |
| GameDLL | `server/game/` — Pin `b088984`, Target `csretro_gamedll` |
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
| Server | GameDLL `server/game/` — `docs/SERVER.md` |
| Bots | Ziel `bots/` (leer). ZBot **in** der GameDLL (Migration) |

```
Xash3D-FWGS → Export → Body (A1) → NextClient-Funktionen
Xash3D-FWGS → GameDLL (inkl. ZBot, Migration) → optional Module → später bots/
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
| GameDLL | `server/game/` — `MANIFEST.md`, `UPSTREAM_PIN` |
| Bots | `bots/` (leer; ZBot liegt in `server/game/`) |
| Ref A / B | `refs/a-cs16-client/` · `refs/b-cs16-goldsrc/` |
| Spielinhalte | `gamedata/` (nicht im Git) |
| Build | `build/` (nicht im Git) |

## GitHub

Remote: `https://github.com/benjarogit/csretro.git`. Privat.

**Nie pushen:** `CLAUDE.md`, `AGENTS.md`, `.cursor/`, `.claude/`, `gamedata/`, `valve/`, `cstrike/`, Builds, Steam-DLLs, WoltLab-Pakete.

**Immer:** diese Tabelle, `CHANGELOG.md`, bei Vendor `UPSTREAM.md`.

Abschluss-Release nur wenn die Phase wirklich fertig ist. Zwischenstand darf auf `main` liegen ohne Phase-3-Tag.

## Runtime (dieser Rechner)

- CachyOS x86_64 · Clang 22.1.8
- CMake 4.4.3: `CMAKE_ROOT=/usr/share/cmake` (sonst `--preset`/`-S` bricht)
- Engine: `./scripts/build-engine.sh` → `build/engine/`
- Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`
- GameDLL: `./scripts/build-gamedll.sh` → `build/gamedll-cmake/cs_amd64.so`
- Sanitizer: `./scripts/build-gamedll.sh --sanitize` → `build/gamedll-sanitize/cs_amd64.so`
- Testdaten: `XASH3D_RODIR` = Steam-HL (nur Maps/WADs), `XASH3D_BASEDIR` = `build/run/`
- ZBot-Runtime (nicht im Git): `BotProfile.db` / `BotChatter.db` aus ReGameDLL `extra/zBot/bot_profiles.zip`; `maps/*.nav` lokal unter `build/run/cstrike/`
- Listen-`+map`: BASEDIR braucht `valve/valve.rc` + `cstrike/cstrike.rc` mit `stuffcmds` (schreibt das Smoke-/3C-Script)
- Menü: Xash-MainUI (`libmenu.so`)
- Steam-`dlls/cs_amd64.so` nicht laden (`executable stack`). Immer `-dll` auf unsere Lib oder Kopie unter `build/run/cstrike/dlls/`

## Quickstart

```bash
git clone git@github.com:benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++
./scripts/build-engine.sh
./scripts/build-client.sh
./scripts/build-gamedll.sh
./scripts/smoke-gamedll.sh dedicated
./scripts/smoke-gamedll.sh listen
./scripts/interactive-3c.sh          # Team/Spawn/Movement/Waffen/Round/Shutdown
```

Inhalte: `gamedata/valve` + `gamedata/cstrike` oder Steam-HL als `XASH3D_RODIR`.
Client: `-clientlib` auf `client_amd64.so`. GameDLL: `-dll` auf `cs_amd64.so`.

## Offene Arbeit

1. **3D nicht automatisch.** Erst nach Freigabe; erstes Feature wäre FOV. Kein Phase-3-Tag.
2. Windows x86_64 / macOS ARM64+x86_64: Compile-Gates (CMake ist vorbereitet, auf diesem Host nicht gebaut).
3. Bot-Grenze analysieren und schrittweise nach `bots/` — nicht amputieren.
4. Phase 4 nur bei Bedarf: NextClient-Menüs; Ref B ein Feature.

## Nicht anfassen

- 3D / NextClient-Feature-Port (FOV, View, Camera, Inspect) ohne ausdrückliche Freigabe
- Ref A außerhalb des A1-Manifests
- Ref-A-ReGameDLL / YaPB / Ref-A-mainui
- Ref B vor Phase 4
- AMXX/Metamod in die GameDLL backen
- ZBot vor Funktionsübernahme löschen
- 32-Bit-Targets, Steam-Bind, `git submodule add`
- `CLAUDE.md` / Cursor-Attribution
