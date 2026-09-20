# Changelog

## Unreleased

## 0.1.27 — 2026-09-20

### Renderer

- #7 Offscreen Client-Triangles draw-only hinter Strategie C: Normal nach solid EFX, Transparent nach Sprites und vor trans EFX. ParticleMan mutate=0 bei 150 Wetterpartikeln, CRC differ=1, Xash `advanced=1`. Environment/Molotov nur im sichtbaren Xash-Frame. Spectator-Overview verification pending. `GL_RenderFrame` bleibt 0. Issue #7 bleibt offen.

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.26 — 2026-09-20

### Renderer

- #7 Client-Triangles intern split: Overview Advance vs DrawReadOnly, ParticleMan Advance vs Render (lokale Render-Refs, kein Live-List-Sort, FacePlayer ohne Winkel-Write), Environment/Molotov nur im sichtbaren Xash-Frame, Fog als Render-State. `HUD_DrawNormalTriangles` / `HUD_DrawTransparentTriangles` bleiben Xash-Exports (Advance+Draw). Xash-only Gate PASS. ParticleMan/Wetter auf Stock-CS NOT REPRODUCIBLE WITH CURRENT GAME CONTENT. Issue #7 bleibt offen.

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.25 — 2026-09-20

### Architektur / Dokumentation

- #7 Status synchronisiert: Brush VERIFIED, Engine-EFX VERIFIED draw-only ownership, Client-Triangles IN PROGRESS, restliche Sprite-Modi / Brush-Sonderflächen PENDING/DEFERRED. Issue bleibt offen.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.24 — 2026-09-20

### Architektur / Dokumentation

- #7 Engine-EFX draw-only Ownership dokumentiert: Particles/Tracers/Beams-Split, `DrawEFX` ABI, Offscreen-Nachweis (`mutate=0`, CRC differ, Double-Advance). Client-Triangle-Ownership Research ohne Produktcode. Issue #7 bleibt offen.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.23 — 2026-09-20

### Renderer

- #7 Offscreen Engine-EFX über `gRenderAPI.DrawEFX(rvp, trans, draw_only)` am Ende von `render_api_t` (v37-Prefix eingefroren, CS-Retro-Extension). Reihenfolge wie `R_DrawEntitiesOnList`: solid draw-only nach opaque Studio, trans draw-only nach Sprites, dann return 0. Nachweis: AK-Schuss `mutate=0`, CRC differ=1, Xash `advanced=1`. Issue #7 bleibt offen.

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.22 — 2026-09-20

### Renderer

- #7 Engine-EFX intern split: `CL_DrawEFX` / Particles / Tracers / Beams bekommen `draw_only`. Draw liest Live-State, Advance (Think, org/vel, Beam-freq, Dead-listen) bleibt beim normalen Xash-Zyklus. `REF_API_VERSION` 19. `r_csretro_renderer 0` Gate: ein AK-Schuss erzeugt Particles+Tracer, kein Offscreen-Draw. Issue #7 bleibt offen.

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.21 — 2026-09-20

### Architektur / Dokumentation

- #7 Brush visuell zertifiziert gegen `59f4921`: mit `r_csretro_renderer 1` bleiben Welt, Viewmodel und HUD auf de_aztec und cs_assault im sichtbaren Xash-Frame; `func_door_rotating *11` (index 19, origin 696 2236 48) öffnet und schließt sichtbar. Issue #7 bleibt offen (Engine-EFX, Client-Triangles). `GL_DrawParticles`/`CL_DrawEFX` ist für einen zweiten Offscreen-Aufruf nicht draw-only-sicher.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.20 — 2026-09-20

### Renderer

- #7 Brush-Entities zeichnen offscreen aus der Mirror-Liste: gemeinsamer BSP-Mesh-Builder für World und Brush-Cache (`model_t*`), GoldSrc-Transform (`R_RotateForEntity` / `R_TranslateForEntity`), opaque `kRenderNormal` plus TransTexture/Color/Alpha/Add. Worldmodel wird nicht doppelt gezeichnet. `GL_RenderFrame` bleibt 0.
- Nachweis: `de_aztec` 12 Brush (11 opaque, 1 trans) CRC differ; Stock-`cs_assault` rotierende Tür `*11` yaw 2.0→5.4, `closed_crc ≠ open_crc`. `de_dust`/`de_aztec` haben kein `func_door`. Issue #7 bleibt offen (EFX/Triangles). Player bleibt C (#9).

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.19 — 2026-09-20

### Renderer

- PX4B.1: `MOVETYPE_FOLLOW` children draw offscreen only when a non-player studio parent is in the same-frame mirror list (`StudioDrawModel(0)` bone cache, then child `STUDIO_RENDER` / merge). Player-parent FOLLOW is counted and deferred — no `StudioDrawPlayer(0)`.
- Stock CS maps (`de_aztec` / `de_dust`, bot + weapon give/drop) never expose a studio FOLLOW child (`follow: 0`). Documented as not reproducible with current game content. No dummy gameplay. `GL_RenderFrame` stays 0. Issue #9 stays open.

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.18 — 2026-09-20

### Architektur / Dokumentation

- PX4A visual certification against `91b128e`: with `r_csretro_renderer 1` on de_aztec the offscreen GSMR pass runs (`pred_plant.mdl` ×7) while the visible Xash frame still shows world, studio worldmodels, T/CT spawn, viewmodel, HUD, and Team/class/buy previews. No recognizable studio/bone/texture corruption after the extra offscreen call. Issue #8 closed. No renderer product change.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.17 — 2026-09-20

### Architektur / Dokumentation

- Handoff: PX4A Issue #8 geschlossen. Folgearbeit Player/FOLLOW #9, Viewmodel #10, pre-return-1 #7.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.16 — 2026-09-20

### Renderer

- PX4A: Non-player studio models draw offscreen through the existing CS GameStudioModelRenderer (`STUDIO_RENDER` only). Snapshots are copies; `CurrentEntity` / `CurrentModel` are restored. Player, viewmodel, `MOVETYPE_FOLLOW`, and previews stay out of this slice.
- GL isolation now also saves depth range, polygon mode, shade model, and texenv. `GL_RenderFrame` stays 0. Player/viewmodel follow in [#9](https://github.com/benjarogit/csretro/issues/9) / [#10](https://github.com/benjarogit/csretro/issues/10). Brush/EFX/triangles remain [#7](https://github.com/benjarogit/csretro/issues/7).

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.15 — 2026-09-20

### Renderer

- PX3C certification: offscreen sprite pass now rebinds the FBO after the world readback and draws two-sided sprites (glow scale, no alpha-test on additive). Real `ET_NORMAL` sprites change offscreen pixels vs world-only (aztec 16 drawn, CRC differ). Real HE TempEnt `mirrored: 1 drawn: 1` also changes CRC.
- `GL_RenderFrame` stays 0. Brush/EFX/client-triangles remain deferred ([#7](https://github.com/benjarogit/csretro/issues/7)). Issue #6 closed.

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.14 — 2026-09-20

### Renderer

- PX3C: per-frame entity mirror (no steal) and additive `R_ClearScene` for the CS Retro list only. TempEnt and normal sprites draw offscreen from a PrimeXT-derived sprite path. `GL_RenderFrame` stays 0.
- Studio is classified, not drawn. Brush entities are counted; draw deferred. Engine EFX and client triangles are not called offscreen (they advance simulation).

Diese Version übernimmt den sichtbaren Frame nicht.  
This release does not take over the visible frame.

## 0.1.13 — 2026-09-20

### Architektur / Dokumentation

- PX3B visual certification against `bbe418d`: with `r_csretro_renderer 1` the offscreen world pass stays nonempty (dust/aztec/mapchange/`vid_setmode`) while the visible Xash frame still shows Team/class/buy, T/CT spawn, viewmodel, HUD, and HE/Smoke/Flash. GL state isolation CONFIRMED as far as visible. Issue #5 closed. No renderer product change.

Diese Version ändert keinen Produktcode.  
This release contains no product-code change.

## 0.1.12 — 2026-09-20

### Renderer

- PX3B: PrimeXT-derived World/Offscreen-Pfad in `client/render/` (Lifecycle, FBO, echter BSP-Mesh, textured + Baseline-Lightmap). `GL_RenderFrame` bleibt 0; sichtbarer Frame ist Xash.
- CVar `r_csretro_renderer` 1 startet nur die Offscreen-Probe (kein sichtbarer Custom-Renderer). Diagnose: `r_csretro_probe_seq`, einmaliger `csretro_offscreen.ppm`.
- Additive Callbacks: `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps`. Studio/Sprite/EFX/Viewmodel unverändert bei Xash.

Diese Version übernimmt den sichtbaren Frame nicht. Team-/Klassen-/Kaufvorschau und Viewmodel bleiben der Xash-Fallback.  
This release does not take over the visible frame. Team/class/buy previews and the viewmodel stay on the Xash fallback.

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
