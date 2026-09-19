# Changelog

## Unreleased

## 0.1.11 — 2026-09-20

### Architektur / Dokumentation

- PX3A follow-up: Inferno/Smoke TempEnts are drawn by `R_DrawSpriteModel` on the entity list, not by `CL_DrawEFX`. `GL_DrawParticles` keeps beams/particles only. Strategy C unchanged.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.10 — 2026-09-20

### Architektur / Dokumentation

- PX3A frame-composition research: `GL_RenderFrame → 1` skips all of Xash `R_RenderScene` (world, entities, both EFX passes, client triangles, viewmodel). Recommended strategy C — build world technique offscreen while the callback still returns 0. No renderer product change.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.9 — 2026-09-20

### Architektur / Dokumentation

- PX2 visual certification against `17bd79f`: Team/class/buy previews, T/CT spawn, viewmodel, HUD, and HE/Smoke/Flash still draw through the Xash fallback (`GL_RenderFrame` returns 0). Issue #4 closed. No renderer product change.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.8 — 2026-09-19

### Architecture

- Client now hands Xash a CS Retro `render_interface_t` (version 37). Only `GL_RenderFrame` is set and it always returns 0, so Xash keeps drawing.
- Diagnostic CVar `r_csretro_renderer`: 0 = Xash fallback, 1 = custom requested but still returns 0 (no custom renderer in this release).

Diese Version ändert die Darstellung nicht absichtlich. Team-/Klassen-/Kaufvorschau wurden nicht neu abgenommen.  
This release is not intended to change what is drawn. Team/class/buy previews were not re-certified.

## 0.1.7 — 2026-09-19

### Architecture / Dokumentation

- PrimeXT comparison tree pinned for a fresh clone: tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23). Restore via `git clone` + SHA checkout into gitignored `refs/primext/` — no submodule, no vendor refresh.
- PX1 research and port matrix in `docs/research/px1-primext.md`. PX2 remains the render-API bridge only (issue #4). No PrimeXT product import in this release.

Diese Version enthält keinen Renderer- oder Gameplay-Code.  
This release contains no renderer or gameplay code.

## 0.1.6 — 2026-09-19

### Gameplay

- Freeze-Countdown default 15s with Xonotic announcer voices (Prepare at 10s if at least 8s remain, then 5–1, Begin). Top HUD shows Prepare for Battle only; the clock stays at the bottom.
- Optional warmup/ready (`mp_warmup`); F12 or chat/console `ready`. Default off.
- Client and GameDLL again share the 450 ms `fuser2` landing hitch (`scripts/movement-contract-gate.sh`). Replay/N-frame verification is still open.
- Inferno no longer emits a `TE_SPRITE` per spread node (ignition only), to reduce Incendiary tent-pool overflow. In-game visual check is still pending.

### UI

- Team, class and buy preview cameras and card layout refined (owner pass plus follow-up).
- Buy-menu silhouettes as TGA overrides; announcer WAVs vendored with notice.

### Architecture / Dokumentation

- Public player and contributor documentation in German and English (MkDocs).
- PrimeXT is documented as a client tech upstream, not a second runtime. No PrimeXT renderer import in this release.
- `engine/ROLE.md` / `client/ROLE.md` allow engine extensions and a single client library.

Diese Version bestätigt keine vollständige Behebung offener Gameplay-Fehler.  
This release does not certify that outstanding gameplay issues are fully fixed.

## Earlier development snapshots / Frühere Entwicklungsstände

The [existing tags and releases](https://github.com/benjarogit/csretro/releases) describe historical
source snapshots. They are not a current, supported binary distribution.
For current limitations, see the [project status](https://benjarogit.github.io/csretro/en/status/).

[Projektstatus auf Deutsch](https://benjarogit.github.io/csretro/status/).
