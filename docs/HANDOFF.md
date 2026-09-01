# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach jeder substantiellen Arbeit die Tabelle und „Offene Arbeit“ in **derselben Session** nachziehen.

Details: `ROLLEN.md`, `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `PHASEN.md`, `PHASE1-ARCHITEKTUR.md`, `PHASE2-SCHNITT.md`, `CHANGELOG.md`.

## Aktueller Stand

| Feld | Wert |
|------|------|
| Datum | 2026-09-01 |
| Phase | **2 abgeschlossen** — als Nächstes **Phase 3** (A1-Body + eine Client-Lib). |
| Körper-Quelle | **A1** (2026-09-01): Ref A nur als Client-Body. NextClient bleibt funktionale Zielbasis. |
| Stapel | Xash3D-FWGS → CS-Retro-Export → A1-Body → NextClient-Funktionen |
| GitHub | https://github.com/benjarogit/csretro (**privat**) |
| Branch | `main` |
| Release | `v0.1.4-phase2` |
| Lokaler Worktree | `/home/benny/Dokumente/csretro` |
| Cutover | 2026-09-01: Remote-`main` ersetzt (altes cs16-client-Monorepo gilt nicht mehr) |

## Rollen (Kurz)

| | Rolle |
|---|--------|
| CS Retro | Produkt-Worktree |
| Xash3D-FWGS | Engine (`engine/`) |
| NextClient | funktionales Zielverhalten |
| Ref A | Xash-Client-Body-Quelle (Allowlist) |
| Ref B | bedingte Menü-Referenz, Phase 4 |
| Server / Bots | getrennte Bereiche |

Vollständig: `docs/ROLLEN.md`.

```
Xash3D-FWGS → Export → Body (Ref A) → NextClient-Funktionen
```

Eine Client-Lib. Kein zweiter Client. Kein „cs16-client weiterentwickeln“.

## Pfade

| Was | Wo |
|-----|-----|
| Worktree | `/home/benny/Dokumente/csretro` |
| Engine | `engine/` |
| NextClient-Herkunft | `client/nextclient/` |
| Exportvertrag | `client/export/csretro_cdll_export.h` |
| Body (Phase 3) | `client/body/` — noch nicht angelegt |
| Server | `server/` |
| Bots | `bots/` (leer) |
| Ref A | `refs/a-cs16-client/` |
| Ref B | `refs/b-cs16-goldsrc/` |
| Spielinhalte | `gamedata/` (nicht im Git) |
| Build | `build/` (nicht im Git) |

## GitHub

Remote: `https://github.com/benjarogit/csretro.git`. Privat.

**Nie pushen:** `CLAUDE.md`, `AGENTS.md`, `.cursor/`, `.claude/`, `gamedata/`, `valve/`, `cstrike/`, Builds, Steam-DLLs, WoltLab-Pakete.

**Immer:** diese Tabelle, `CHANGELOG.md`, bei Vendor `UPSTREAM.md`.

Nach Commit+Push: privates GitHub-Release mit Changelog-Abschnitt.

## Runtime (dieser Rechner, 2026-09-01)

- CachyOS x86_64 · Clang 22.1.8 · Waf-Engine-Build OK
- CMake 4.4.3 `--preset` defekt (`CMAKE_ROOT`) — Engine über `./scripts/build-engine.sh`
- Cross i686/aarch64: Toolchains da, Compiler nicht installiert

## Quickstart

```bash
git clone git@github.com:benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++
./scripts/build-engine.sh
```

Spielinhalte: `gamedata/valve` + `gamedata/cstrike`.

## Offene Arbeit

1. **Phase 3:** A1-Allowlist nach `client/body/` (eine Lib). `GetClientAPI` füllen. NextClient-Features (`GameHud`, View, FOV, …) auf den Unterbau, nicht cs16-client weiterentwickeln.
2. GameDLL (`dlls/cs.so`) und Bots: eigene Entscheidungen, kein ReGameDLL/YaPB.
3. Phase 4 nur bei Bedarf: NextClient-Menüs; Ref B ein Feature.

Schnitt-Inventar: `docs/PHASE2-SCHNITT.md`. Körper-Quelle: A1, nicht wieder öffnen.

## Nicht anfassen

- Ref A außer A1-Allowlist (`cl_dll`, `pm_shared`, zwingende Client-Header/`wpn_shared`)
- YaPB, ReGameDLL, mainui aus Ref A
- Ref B vor Phase 4
- `git submodule add` für Projektquellen
- Steam-Deploy / VAC
- Steam-Bind wieder einbauen (`steam_api_proxy`, 8684-Provider, `hl1master`)
- `CLAUDE.md` / Cursor-Attribution
- alte Remote-Historie vor Cutover

## Cutover 2026-09-01 (erledigt)

Altes GitHub-`main` war Xash+cs16-client (`8ece11c`, `v0.2.0`). Ersetzt. A1 holt daraus später nur den Body, nicht das alte Produktziel.
