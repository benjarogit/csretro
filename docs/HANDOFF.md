# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1–PX4A: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — PX4A Non-Player Studio offscreen

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX3B: `bbe418d` / v0.1.12, Cert `8443d0d` / v0.1.13, Issue #5 geschlossen.
PX3C: `d3de222` / v0.1.14, Cert `9c7e7ae` / v0.1.15, Issue #6 geschlossen.
PX4A: dieser Stand. `GL_RenderFrame` bleibt 0.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**Produktpfad**
- Lifecycle / Frame: `client/render/render_core.cpp`
- GL/Offscreen: `client/render/render_backend.cpp` (FBO, Depth-Range, Polygon, Shade, Texenv, TMU, Restore)
- World/BSP: `client/render/render_world.cpp`
- Entity-Spiegel: `client/render/render_scene.cpp` — volle `cl_entity_t`-Kopie inkl. latched, kein Steal
- Sprite offscreen: `client/render/render_sprite.cpp`
- Studio offscreen: `client/render/render_studio.cpp` — GSMR `STUDIO_RENDER` only, Snapshot, CurrentEntity save/restore
- Brücke: `cdll_int.cpp` — `GL_RenderFrame` void + `return 0`; `HUD_AddEntity` spiegelt und behält Return

**CVar:** `r_csretro_renderer` 0 = Xash-only. 1 = Offscreen World+Sprites+Non-Player-Studio + sichtbarer Xash-Fallback.
Probes: `./scripts/px3b-offscreen-probe.sh`, `./scripts/px3c-offscreen-probe.sh`, `./scripts/px4a-offscreen-probe.sh`.
Visual: `./scripts/px3c-visual-cert.sh` → `build/px3c-cert-shots/`; `./scripts/px4a-visual-cert.sh` → `build/px4a-cert-shots/` (nicht committed).

**Callbacks an:** `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps`, `R_ClearScene` (additiv, nur CS-Retro-Liste).
**Callbacks NULL:** `Mod_GetCurrentVis`, `R_ProcessEntData`, Studio-Decals, `CL_UpdateLatchedVars`.

**PX4A Cert**
- Side-effect map in `docs/research/px1-primext.md`.
- Offscreen: `de_aztec` `models/skeleton.mdl` ×8 / visual-run `pred_plant.mdl` ×7, CRC differ, events=0. Player C.
- Visuell 2026-09-20 mit `r_csretro_renderer 1` auf **de_aztec** (`build/px4a-cert-shots/`, nicht committed): Welt, Studio-Worldmodels (`pred_plant`), T/CT Spawn, Viewmodel+HUD, Team/Class/Buy-Previews, Folgeframes, Mapchange dust, `vid_setmode`. Keine erkennbare Studio/Bone/Texture-Korruption nach Offscreen-GSMR.
- `STUDIO_EVENTS` offscreen aus. CurrentEntity/CurrentModel restore. Kein Live-Pointer über Frames.
- Mapchange aztec→dust, `vid_setmode`, Movement-Gate PASS. `GL_RenderFrame` immer 0.
- Visual: `./scripts/px4a-visual-cert.sh`. Issue #8 visuell zertifiziert, geschlossen.

**Offen vor return 1** — [#7](https://github.com/benjarogit/csretro/issues/7)
- Brush-Entity Draw
- Engine-EFX (`GL_DrawParticles` / Think)
- Client-Triangles (ParticleMan / Fog / Wick)
- `SPR_ANGLED` / Frame-Lerp / Sprite-Lightmap
- Player-Studio ([#9](https://github.com/benjarogit/csretro/issues/9)), FOLLOW (Slice in #9), Viewmodel ([#10](https://github.com/benjarogit/csretro/issues/10)), Vis

**Nächster Schritt:** PX4B.1 — Non-Player FOLLOW (Parent in Mirror-Liste). Player-parent FOLLOW deferred. Player bleibt C. Kein Viewmodel. `return 1` weiter gesperrt.

**PX0 bleibt offen**
- #1 Movement Replay: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- #3 Erster M1 Granaten: https://github.com/benjarogit/csretro/issues/3
- Gate `./scripts/movement-contract-gate.sh` PASS. Gate ≠ Replay.

**Team / Klasse / Buy** — nach PX2, PX3B und PX3C visuell CONFIRMED.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**Kein `GL_RenderFrame → 1`.** Locks: Inferno/Zippo-Gameplay, CS2-Waffen, `pm_shared`, ImGui, PhysX, HDR/PBR, Entity-Steal, sichtbarer Custom-Frame, PrimeXT-Studio parallel.
