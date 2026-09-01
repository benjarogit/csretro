# Changelog

Jede Version hier = ein GitHub-Release auf `benjarogit/csretro` (privat).
Der verbindliche Projektstand steht in `docs/HANDOFF.md`.

## 0.1.1-vgui2-analyse — 2026-09-01

### Repo

- Branch `vgui2cs16Menu` für Game-Menu/VGUI2, getrennt von `main`.

### Hinzugefügt

- `docs/GAMEUI-ANALYSE.md` — Teil B (VGUI1), kungfulon `vgui2_support`, Thanatos-Launcher, FWGS `new_vgui_support_api`, Integrationsplan.
- Handoff-Isolation: Worktree `/home/benny/Dokumente/vgui2cs16Menu`.

### Nicht enthalten

- Keine Screens, kein Engine-Patch, kein Steam-`gameui.so`.

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
- Einheitliches CMake-Gerüst (Clang, Presets x86_64 / i686 / aarch64).
- Engine-Build-Skript (Waf + Clang, 64-bit); erster erfolgreicher Clang-Build der Engine.
- Rollen-, Lizenz-, Upstream-, Schnittstellen- und Handoff-Doku.

### Nicht enthalten

- Microsoft vcpkg (bewusst nicht vendort).
- Spielinhalte (valve/cstrike).
- Lauffähiger NextClient unter Xash (Phase 1–3).
- Code aus Referenz A oder B.
