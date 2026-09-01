# Changelog

Jede Version hier = ein GitHub-Release auf `benjarogit/csretro` (privat).
Der verbindliche Projektstand steht in `docs/HANDOFF.md`.

## Unreleased — 3A/3B-Zwischenstand (2026-09-01)

Kein Phase-3-Abschluss-Tag. 3C In-Game und 3D fehlen.

### 3A/3B Client

- A1-Body aus Ref A `bb60674` nach `client/body/` (Manifest: `docs/ROLLEN.md`).
- Eine Lib `csretro_client` → `client_amd64.so`; `GetClientAPI`.
- Steam-Include → `archtypes.h`; Versionscheck akzeptiert Dev-`Q_buildnum()` `-1`.
- Xash lädt die Lib (Init/MainUI/`+quit`). In-Game nicht verifiziert.

### Leitplanken

- 64-Bit-only: `docs/PLATTFORMEN.md`. i686-Preset/Toolchain entfernt. CMake bricht bei 32-Bit ab.
- Leichen entfernt: `F()`, `voice_gamemgr.cpp`, `ev_hldm.cpp`, `inputw32.cpp`, `input_sdl.cpp`, `cs_baseentity.cpp`, Ref-A-`cl_dll`-CMake/DSP.
- PHASE1/PHASE2-Detaildateien konsolidiert und gelöscht. Neu: `PLATTFORMEN.md`, `SERVER.md`.
- GameDLL-Gate: rehlds/ReGameDLL_CS empfohlen, noch nicht vendort.

### Nicht enthalten

- Keine eigene GameDLL, kein 3D-Feature-Port, kein Phase-3-Release.

## 0.1.4-phase2 — 2026-09-01

### Schnitt

- `steam_api_proxy/` entfernt; Launcher lädt `steam_api.dll` nicht mehr.
- 8684-Address-Provider entfernt; NitroApi hookt keine Steam-`hw.dll`/`client.dll` mehr.
- Steam-Master (`hl1master`) und `MatchmakingSteamComp` entfernt.
- Xash-Exportvertrag: `client/export/csretro_cdll_export.h`, CMake-Target `csretro_client_export`.

### Behalten

- Feature-Quellen: `client_mini` (GameHud, View, FOV, …), `engine_mini` NCLM/HTTP-Master, GameUI als Quelle.

### Nicht enthalten

- Kein Phase-3-Body, kein `GetClientAPI`-Rumpf, kein `client/body/`.

## 0.1.3-a1 — 2026-09-01

### Dokumentation

- Gate: **A1 — Ref A als Client-Body** (2026-09-01).
- NextClient bleibt funktionale Zielbasis. cs16-client nur Xash-Unterbau (Allowlist).
- `docs/ROLLEN.md`; Handoff, Phasen, Lizenzen, Architektur, Refs nachgezogen.
- A1-Lizenz dokumentiert, nicht als vollständig geklärt markiert.

### Nicht enthalten

- Kein Phase-3-Body-Code, kein Ref-A-Vendor nach `client/body/`.

## 0.1.2-gate — 2026-09-01

### Dokumentation

- Gate vor Phase 3: Körper-Quelle A0 (neu schreiben) oder A1 (Ref A nur als `cl_dll`-Körper, GPL-Attribution).
- Option A bleibt die Form (Export + Körper + Features), nicht die stillschweigende Entscheidung „Körper von Null“.
- `docs/PHASEN.md`; Architektur/Handoff/Lizenzen nachgezogen.

## 0.1.1-phase1 — 2026-09-01

### Dokumentation

- Phase-1-Analyse: NextClient ist Overlay, kein Client-Körper.
- Entscheidung: eigener `GetClientAPI`-Export + eigener Körper + Features aus `client_mini` als Module. Ref A nur gelesen.
- Architektur festgehalten (heute: `docs/PHASEN.md`, `docs/SCHNITTSTELLEN.md`).

### Nicht enthalten

- Kein Client-Code, kein Ref-A/B-Import, kein Steam-Schnitt (Phase 2).

## 0.1.0-phase0 — 2026-09-01

### Repo

- GitHub `benjarogit/csretro` `main` geleert und durch diesen Stand ersetzt.
- Altes Xash+cs16-client-Monorepo und Release `v0.2.0` gelten nicht mehr.

### Hinzugefügt

- Leeren Worktree als CS-Retro-Monorepo angelegt (kein Altbestand).
- Vendoring ohne Git-Submodule:
  - `engine/` — Xash3D-FWGS `1442d14`
  - `client/` — NextClient `f5addc2` + NclNitroApi `f73fc1a` + ncl-hl1-source-sdk `46c3103`
  - `server/` — NextClientServerApi `1c7e5c6`
  - `refs/a-cs16-client/` — Velaron/cs16-client `bb60674` (eingefroren)
  - `refs/b-cs16-goldsrc/` — FuryBaM/cs16-goldsrc-client `b662acc` (eingefroren)
- `bots/` als leerer, getrennter Bereich.
- Einheitliches CMake-Gerüst (Clang; 32-Bit-Presets später entfernt).
- Engine-Build-Skript (Waf + Clang, 64-bit); erster erfolgreicher Clang-Build der Engine.
- Rollen-, Lizenz-, Upstream-, Schnittstellen- und Handoff-Doku.

### Nicht enthalten

- Microsoft vcpkg (bewusst nicht vendort).
- Spielinhalte (valve/cstrike).
- Lauffähiger NextClient unter Xash (Phase 1–3).
- Code aus Referenz A oder B.
