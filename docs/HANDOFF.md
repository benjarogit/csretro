# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach jeder substantiellen Arbeit (Phase, Deploy-Ziel, Upstream-Pin, Breaking Change)
die Tabelle unten und „Offene Arbeit“ in **derselben Session** nachziehen.

Details nicht hier duplizieren: `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `PHASEN.md`, `PHASE1-ARCHITEKTUR.md`, `CHANGELOG.md`.

## Aktueller Stand

| Feld | Wert |
|------|------|
| Datum | 2026-09-01 |
| Phase | **1 abgeschlossen** — als Nächstes Phase 2 (Steam). Vor Phase 3: Gate Körper-Quelle (A0/A1) |
| GitHub | https://github.com/benjarogit/csretro (**privat**) |
| Branch | `main` — einzige Arbeitslinie |
| Release | `v0.1.2-gate` (Changelog: `CHANGELOG.md`) |
| Lokaler Worktree | `/home/benny/Dokumente/csretro` |
| Cutover | 2026-09-01: Remote-`main` geleert/ersetzt. Alte Historie (Xash+cs16-client, Tag `v0.2.0`) gilt nicht mehr. |

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
| Build | `build/` (nicht im Git) |

## GitHub — was hoch darf, was nie

Remote: `git@github.com:benjarogit/csretro.git` bzw. HTTPS `https://github.com/benjarogit/csretro.git`.
Repo bleibt **privat**, solange NextClient keine LICENSE hat.

**Nie pushen:** `CLAUDE.md`, `AGENTS.md`, `.cursor/`, `.claude/`, `gamedata/`, `valve/`, `cstrike/`, Build-Artefakte, Steam-DLLs, WoltLab-Pakete. Siehe `.gitignore`.

**Immer mitziehen:** `docs/HANDOFF.md` (diese Tabelle), `CHANGELOG.md`, bei Vendor-Änderung `docs/UPSTREAM.md`.

Nach Commit+Push: GitHub-Release mit dem Changelog-Abschnitt der Version (privat ist in Ordnung). Kein öffentliches Repo, keine GitHub-Pages für den Quellstand.

## Runtime (dieser Rechner, 2026-09-01)

- OS: CachyOS / Linux x86_64
- CMake 4.4.3 — `cmake -S/-B` und `--preset` scheitern mit `CMAKE_ROOT`/`LoadCache` (CachyOS-Paket). Engine-Build läuft über Waf, nicht über CMake.
- Clang 22.1.8, Ninja 1.13, Python 3.14
- SDL2 + FreeType vorhanden (pkg-config)
- Cross-Compiler i686 / aarch64: **nicht installiert** (Toolchain-Dateien liegen trotzdem)
- Engine-Build (2026-09-01): OK — `build/engine/game_launch/xash3d`, `libxash.so`, `libref_gl.so`, `libmenu.so`, `filesystem_stdio.so`

## Quickstart

```bash
git clone git@github.com:benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++
./scripts/build-engine.sh          # Xash 64-bit via Waf + Clang
```

Spielstart braucht `gamedata/valve` + `gamedata/cstrike` und die Engine-Binaries aus `build/engine`.
Root-CMake (`CMakePresets.json`) ist vorbereitet, auf diesem Host aber nicht konfigurierbar, bis das CMake-Paket steht.

## Offene Arbeit

1. **Phase 2:** Steam-Schicht identifizieren und entfernen/ersetzen (`steam_api_proxy`, Master, 8684-Provider). Kein Körper-Schreiben.
2. **Gate vor Phase 3:** Körper-Quelle A0 (neu) oder A1 (Ref-A-`cl_dll` + GPL-Attribution). Ohne Eintrag in `docs/PHASEN.md` kein Körper-Code. Nicht still A0.
3. **Phase 3:** Export + Körper laut Gate. Menü = Xash MainUI. GameDLL (`dlls/cs.so`) eigene Entscheidung — nicht ReGameDLL, nicht AMXX als Xash-Server.
4. NextClient-Features (`GameHud`, View, FOV, …) erst nach dem Körper, einzeln, ohne NitroApi-Hooks.
5. NextClient ohne LICENSE — Repo bleibt privat; kein öffentliches GitHub.

Bind-**Form**: `docs/PHASE1-ARCHITEKTUR.md` (nicht wieder aufmachen).  
Körper-**Quelle**: offen, `docs/PHASEN.md` Gate A0/A1.

## Nicht anfassen

- `refs/**` nicht weiterentwickeln; kein Copy nach Basis, **außer** das Gate A1 ausdrücklich den Ref-A-Körper freigibt (nur `cl_dll`/`pm_shared`, nicht YaPB/ReGameDLL)
- kein `git submodule add` für Projektquellen
- kein Steam-Deploy, kein VAC-Pfad
- kein YaPB/ReGameDLL-Import aus Ref A nach `bots/`/`server/`
- kein Ref-B-Menü vor Phase 4
- `CLAUDE.md` / Cursor-Attribution nicht committen
- Alte Remote-Historie vor dem Cutover nicht wiederherstellen, außer Benny fordert das explizit

## Cutover 2026-09-01 (erledigt)

Vorher auf GitHub: Xash3D + Velaron/cs16-client-Monorepo (`8ece11c`), Release `v0.2.0`, README verwies auf GitHub Pages.
Das war **Referenz-A-Arbeit**, nicht die NextClient-Basis. Force-Push `8ece11c` → `8e378f0`. Tag/Release `v0.2.0` entfernt. Neuer Stand: Tag `v0.1.0-phase0`.
