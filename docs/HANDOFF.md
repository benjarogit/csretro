# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach substantieller Arbeit Tabelle und „Offene Arbeit“ in derselben Session nachziehen.

Details: `ROLLEN.md`, `PLATTFORMEN.md`, `SERVER.md`, `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `MENUS.md`, `GAMEDATA.md`, `PHASEN.md`, `PHASE3-BODY.md` (nur während Phase 3), `PHASE3M.md`, `PHASE3M-KEYBOARD.md`, `PHASE3M-LAYOUT.md`, `PHASE3M-VIDEO.md`, `PHASE3M-METRICS-DIAGNOSIS.md`, `CHANGELOG.md`. Danksagung: `CREDITS.md`.

## Aktueller Stand

| Feld | Wert |
|------|------|
| Datum | 2026-09-03 |
| Phase | **3A/3B/3C abgenommen.** **3M in Arbeit** (Menü-Lib + Team/Buy-VGUI). 3D/FOV erst nach 3M. |
| Körper-Quelle | **A1** — Manifest in `ROLLEN.md` |
| GameDLL | `server/game/` — Pin `b088984`, Target `csretro_gamedll` |
| Stapel | Xash → Export → A1-Body → (später) NextClient-Funktionen |
| Plattform | **64-Bit only** — `docs/PLATTFORMEN.md` |
| GitHub | https://github.com/benjarogit/csretro (**privat**) |
| Branch | `main` |
| Letztes Release | `v0.1.5` — Desktop-Menüs + Game-Data. Phase 3 ist **kein** Abschluss-Release |
| Lokaler Worktree | `/home/benny/Dokumente/csretro` |

## Rollen (Kurz)

| | Rolle |
|---|--------|
| CS Retro | Produkt-Worktree |
| Xash3D-FWGS | Engine (`engine/`) |
| NextClient | funktionales Zielverhalten |
| Ref A | Client-Body-Quelle (A1-Manifest) |
| Ref B | Menü-/VGUI-Referenz bereits in Phase 3M; gezielte zusätzliche Feature-Ports später |
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
| Menü | `client/menu/` — CS-Retro-Menü-Lib (`GetMenuAPI` + `GameMenuExports001`) |
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
- Menü-Sanitizer: `./scripts/build-menu.sh --sanitize` → `build/menu-sanitize/menu/menu_amd64.so`
- Testdaten: `XASH3D_RODIR` = `gamedata/` (Bootstrap, **read-only**). Play-UserConfig: `XASH3D_BASEDIR` = `build/run/`. Gates: `build/run-gate/`. 3C: `build/run-3c/`.
- Steam-HL nie als RODIR. Erkennung: `python3 ./scripts/bootstrap-gamedata.py --print-steam`
- ZBot-Testdaten im Game-Data-Baum (`BotProfile.db`, `de_dust.nav`), nicht in Steam
- Listen-`+map`: `.rc` mit `stuffcmds` in Game-Data und BASEDIR
- 3C/Menü-Tests: headless über `gamescope --backend headless` (kein Fokusdiebstahl). Sichtbar: `CSRETRO_FOREGROUND=1 ./scripts/interactive-3c.sh`
- Menü: Endziel eine Lib (`client/menu/`, `docs/MENUS.md`, `docs/PHASE3M.md`). 3C-Baseline (`v0.1.5`): Xash-MainUI + `ShowMenu`. 3M ersetzt das als Primär-UI, `ShowMenu` bleibt Legacy.
- `cstrike/liblist.gam` ist CS-Retro-owned (Branding + `dlls/cs.so` → Xash `cs_amd64.so`).
- Steam-`dlls/cs_amd64.so` nicht laden. Ohne `-dll`/`-clientlib` findet Xash die Libraries über `liblist` (`dlls/cs.so` → `cs_amd64.so`, `cl_dlls/client_amd64.so`), sofern sie in BASEDIR oder Game-Data liegen. Tests dürfen die Flags weiter nutzen.

## Quickstart

```bash
git clone git@github.com:benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++
./scripts/build-engine.sh
./scripts/build-client.sh
./scripts/build-menu.sh
./scripts/build-gamedll.sh
python3 ./scripts/bootstrap-gamedata.py
./scripts/smoke-gamedll.sh dedicated
./scripts/smoke-gamedll.sh listen
./scripts/interactive-3c.sh
./scripts/interactive-menus.sh   # Team/Buy/Radio ohne Auto-Join, ohne touch/*.cfg
./scripts/vgui-v1-poc-runtime.sh # V1-PoC Auto-Test (CSRETRO_V1POC)
./scripts/play.sh                 # manuelles Fenster (bleibt offen)
```

Inhalte: nur `gamedata/` (`docs/GAMEDATA.md`). Client: `-clientlib`. GameDLL: `-dll`.

## Offene Arbeit

1. **Phase 3M** — Mouse/Audio/Video **PASS / Regression**. Keyboard **AUTOMATED PASS / MANUAL RECHECK OPEN** nach Persistenz-Fix (staged Bindings überleben Page-Wechsel, Apply schreibt Engine/Config; `play.sh` schützt `build/run` und seedet CS-Defaults nur bei leerer/HL-Fallback/Gate-Config) — `docs/PHASE3M-KEYBOARD.md`. Alte Options-Gates defaulten auf `build/run-gate/*`, nicht `build/run`. Adaptive Layout **AUTOMATED PASS / MANUAL ACCEPTANCE OPEN** (natives Resize über acht Grips, Min **512×406**, Live-Save ohne Apply, Restart/Restore grün; ASan-Restart-Teardown noch offen; Audio-Sound-Quality-Abstand reduziert) — `docs/PHASE3M-LAYOUT.md`, Gate `./scripts/vgui-options-layout-gate.sh`. Danach Visual Polish **OPEN**. **Kein Phase-3-Tag. FOV/3D gesperrt.** Keine nächste Subpage.
2. Renderer-Multi wenn Extended API.
3. Windows/macOS Compile-Gates. **`Csretro_PlatformShellOpen` Windows = offenes Plattform-Gate**.
4. Bot-Grenze → `bots/`.

## Nicht anfassen

- 3D / NextClient-Feature-Port (FOV, View, Camera, Inspect) vor Abschluss von 3M bzw. ohne ausdrückliche Freigabe
- Ref A außerhalb des A1-Manifests
- Ref-A-ReGameDLL / YaPB / Ref-A-mainui
- Ref-B-Vollport als zweiten Menüstapel; Ref B bleibt VGUI-Referenz für 3M (`docs/MENUS.md`)
- Steam-`vgui2.dll`/`vgui2.so` als Runtime
- AMXX/Metamod in die GameDLL backen
- ZBot vor Funktionsübernahme löschen
- 32-Bit-Targets, Steam-Bind, `git submodule add`
- Steam-Installation beschreiben oder überschreiben
- `CLAUDE.md` / Cursor-Attribution
