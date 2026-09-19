# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1–PX3C / PX4A-Research: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — PX3C zertifiziert, PX4A pending

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX3B: `bbe418d` / v0.1.12, Cert `8443d0d` / v0.1.13, Issue #5 geschlossen.
PX3C: `d3de222` / v0.1.14 Implementation; dieser Stand zertifiziert Issue #6.
`GL_RenderFrame` bleibt 0.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**Produktpfad**
- Lifecycle / Frame: `client/render/render_core.cpp`
- GL/Offscreen: `client/render/render_backend.cpp` (FBO nach World-Readback neu binden; State-Restore)
- World/BSP: `client/render/render_world.cpp`
- Entity-Spiegel: `client/render/render_scene.cpp` — Kopie, kein Steal
- Sprite offscreen: `client/render/render_sprite.cpp` (PrimeXT `gl_sprite.cpp` @ `46fb05b`)
- Brücke: `cdll_int.cpp` — `GL_RenderFrame` void + `return 0`; `HUD_AddEntity` spiegelt und behält Return

**CVar:** `r_csretro_renderer` 0 = Xash-only. 1 = Offscreen World+Sprites + sichtbarer Xash-Fallback.
Probes: `./scripts/px3b-offscreen-probe.sh`, `./scripts/px3c-offscreen-probe.sh`.
Visual: `./scripts/px3c-visual-cert.sh` → `build/px3c-cert-shots/` (nicht committed).

**Callbacks an:** `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps`, `R_ClearScene` (additiv, nur CS-Retro-Liste).
**Callbacks NULL:** `Mod_GetCurrentVis`, `R_ProcessEntData`, Studio-Decals, `CL_UpdateLatchedVars`.

**PX3C Cert**
- `ET_NORMAL` Sprites ändern Offscreen-Pixel reproduzierbar (aztec 16 drawn, CRC differ=1).
- TempEnt: echter HE-Spark `mirrored: 1 drawn: 1`, CRC differ=1. Kein Dummy.
- Sichtbares Xash: T/CT Team/Klasse/Buy, Spawn, Viewmodel, HUD, Mapchange, `vid_setmode`. Kein Takeover.
- Movement-Gate PASS. `GL_RenderFrame` immer 0.
- Issue #6 geschlossen.

**Offen vor return 1** — [#7](https://github.com/benjarogit/csretro/issues/7)
- Brush-Entity Draw
- Engine-EFX (`GL_DrawParticles` / Think)
- Client-Triangles (ParticleMan / Fog / Wick)
- `SPR_ANGLED` / Frame-Lerp / Sprite-Lightmap
- Player-Studio, Viewmodel, Vis (PX4)

**Nächster Schritt:** PX4A — Studio invocation safety + Non-Player Studio offscreen. Nur `STUDIO_RENDER`. Kein Player/Viewmodel/FOLLOW im ersten Slice. GSMR bleibt CS-Was.

**PX0 bleibt offen**
- #1 Movement Replay: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- #3 Erster M1 Granaten: https://github.com/benjarogit/csretro/issues/3
- Gate `./scripts/movement-contract-gate.sh` PASS. Gate ≠ Replay.

**Team / Klasse / Buy** — nach PX2, PX3B und PX3C visuell CONFIRMED.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**Kein `GL_RenderFrame → 1`.** Locks: Inferno/Zippo-Gameplay, CS2-Waffen, `pm_shared`, ImGui, PhysX, HDR/PBR, Entity-Steal, sichtbarer Custom-Frame, PrimeXT-Studio parallel.
