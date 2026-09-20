# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1–PX4B / #7: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — #7 Client-Triangles draw-only (IN PROGRESS, Issue OPEN)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX3B: `bbe418d` / v0.1.12, Cert `8443d0d` / v0.1.13, Issue #5 geschlossen.
PX3C: `d3de222` / v0.1.14, Cert `9c7e7ae` / v0.1.15, Issue #6 geschlossen.
PX4A: `29f5a3c` / v0.1.16, Visual `b42359a` / v0.1.18, Issue #8 geschlossen.
PX4B.1: `344bf73` / v0.1.19. Issue #9 offen (ruhend).
#7 Brush visuell: `59f4921` / v0.1.20, Cert `fba5b54` / v0.1.21.
#7 Engine-EFX Split: `41ee26f` / v0.1.22. Offscreen `c365cda` / v0.1.23. Docs dieser Stand.
`GL_RenderFrame` bleibt 0.

```
Brush entities: VERIFIED
Engine EFX: VERIFIED draw-only ownership
Client triangles: IN PROGRESS
remaining sprite modes / brush special cases: PENDING/DEFERRED
```

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**Produktpfad**
- Lifecycle / Frame: `client/render/render_core.cpp`
- GL/Offscreen: `client/render/render_backend.cpp` (FBO, Depth-Range, Polygon, Shade, Texenv, TMU, Poly-Offset, Restore)
- Shared BSP-Mesh: `client/render/render_bsp_mesh.cpp` — ein Builder für World und Brush-Cache
- World/BSP: `client/render/render_world.cpp`
- Brush offscreen: `client/render/render_brush.cpp` — Cache nach `model_t*`, GoldSrc-Transform, opaque + trans
- Entity-Spiegel: `client/render/render_scene.cpp` — volle `cl_entity_t`-Kopie inkl. latched, kein Steal
- Sprite offscreen: `client/render/render_sprite.cpp`
- Studio offscreen: `client/render/render_studio.cpp` — GSMR `STUDIO_RENDER` only, Snapshot, CurrentEntity save/restore; FOLLOW child nur bei Non-Player-Parent in der Mirror-Liste
- Engine-EFX: `gRenderAPI.DrawEFX(rvp, trans, draw_only)` — CS-Retro-Extension am Ende von `render_api_t` (v37-Prefix eingefroren). Intern Ref `REF_API_VERSION` 19
- Brücke: `cdll_int.cpp` — `GL_RenderFrame` void + `return 0`; `HUD_AddEntity` spiegelt und behält Return

**CVar:** `r_csretro_renderer` 0 = Xash-only. 1 = Offscreen World+Brush+Sprites+Non-Player-Studio+FOLLOW+draw-only EFX + sichtbarer Xash-Fallback.
Probes: `./scripts/px3b-offscreen-probe.sh`, `./scripts/px3c-offscreen-probe.sh`, `./scripts/px4a-offscreen-probe.sh`, `./scripts/px4b1-offscreen-probe.sh`, `./scripts/px7-brush-offscreen-probe.sh`, `./scripts/px7-efx-xash-gate.sh`, `./scripts/px7-efx-offscreen-probe.sh`.
Visual: `./scripts/px3c-visual-cert.sh` → `build/px3c-cert-shots/`; `./scripts/px4a-visual-cert.sh` → `build/px4a-cert-shots/`; `./scripts/px7-brush-visual-cert.sh` → `build/px7-brush-cert-shots/` (nicht committed).

**Callbacks an:** `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps`, `R_ClearScene` (additiv, nur CS-Retro-Liste).
**Callbacks NULL:** `Mod_GetCurrentVis`, `R_ProcessEntData`, Studio-Decals, `CL_UpdateLatchedVars`.

**#7 Brush (VERIFIED 2026-09-20)**
- Shared mesh builder: BSP-Surfaces → gleiche Triangulation für World-Mesh und Brush-Cache. World-Mesh wird nicht von Brush überschrieben.
- Cache: `model_t*`, first/num modelsurfaces, verts, batches, textures, LM-pages, flags. Mapchange/unload gibt frei. Keine Surface-Pointer über Map-Leben.
- Nur `CSRETRO_KIND_BRUSH` aus `HUD_AddEntity`. Worldmodel nicht doppelt. Kein `GetEntityByIndex` als Renderliste.
- Transform: GoldSrc `R_RotateForEntity` / `R_TranslateForEntity` (origin, yaw, −pitch, roll).
- Opaque `kRenderNormal` + TransTexture/Color/Alpha/Add. Lightmaps über `PARM_TEX_LIGHTMAP`.
- Probe PASS + Visual `./scripts/px7-brush-visual-cert.sh` PASS: aztec Welt/Viewmodel/HUD; assault `func_door_rotating *11` index 19 origin 696 2236 48 sichtbar geschlossen → Kante → offen. Mapchange dust + `vid_setmode`. Movement-Gate PASS. `GL_RenderFrame` immer 0.
- Sonderflächen DEFERRED: SURF_DRAWTURB/water, tex anim, decals, dlights; conveyor/fullbright laut Research.

**#7 Engine-EFX (VERIFIED draw-only ownership 2026-09-20)**
- Vertrag: `docs/research/px1-primext.md`. `GL_DrawParticles` bleibt unsicher (Advance). Produktpfad ist `DrawEFX(..., draw_only=1)`.
- Intern: Particles/Tracers ohne Think; Beams `copy=*live`; Dead-list nur im Advance.
- Xash-only Gate PASS (AK-Schuss particles+tracers, kein draw-only).
- Offscreen: draw-only `mutate=0` auf Trans-Pass; CRC differ=1; Xash `advanced=1` dieselbe Frame. Beams Stock-CS NOT REPRODUCIBLE.
- Reihenfolge: World → opaque Brush → Studio → FOLLOW → solid EFX draw-only → trans Brush → Sprites → trans EFX draw-only → restore → return 0.
- Player bleibt C. Kein Triangle-Produktcode.

**PX4B.1** (ruhend)
- FOLLOW parent graph implementiert. Player-parent deferred. Stock-CS: **NOT REPRODUCIBLE WITH CURRENT GAME CONTENT**.
- Player bleibt C. Issue #9 offen, ruht bis A/B-Beweis. Kein Player-Produktcode in diesem Slice.
- Read-only: Variante B kann `gait`/`player_info_t` nicht isolieren ohne GSMR- oder `PlayerInfo`-Änderung (`IEngineStudio.PlayerInfo()` ist Live-State).

**Offen vor return 1**
- [#7](https://github.com/benjarogit/csretro/issues/7): Client-Triangles IN PROGRESS (ParticleMan / Overview / Fog / Wick); `SPR_ANGLED` / Frame-Lerp / Sprite-Lightmap und Brush-Sonderflächen (turb/decals/dlights) DEFERRED. Engine-EFX draw-only ist VERIFIED, Issue bleibt OPEN
- Player-Studio ([#9](https://github.com/benjarogit/csretro/issues/9)) — Variante C, Blocker vor `return 1`
- Player-parent FOLLOW (Slice in #9, hängt an Player-Safety)
- Viewmodel ([#10](https://github.com/benjarogit/csretro/issues/10))
- Vis

**Nächster Schritt:** Client-Triangles draw-only hinter Strategie C (dieser Slice). Danach restliche Sprite-Modi / Brush-Sonderflächen. Kein Viewmodel. `return 1` weiter gesperrt.

**PX0 bleibt offen**
- #1 Movement Replay: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- #3 Erster M1 Granaten: https://github.com/benjarogit/csretro/issues/3
- Gate `./scripts/movement-contract-gate.sh` PASS. Gate ≠ Replay.

**Team / Klasse / Buy** — nach PX2, PX3B und PX3C visuell CONFIRMED.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**Kein `GL_RenderFrame → 1`.** Locks: Inferno/Zippo-Gameplay, CS2-Waffen, `pm_shared`, ImGui, PhysX, HDR/PBR, Entity-Steal, sichtbarer Custom-Frame, PrimeXT-Studio parallel.
