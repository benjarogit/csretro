# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1–PX3C: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — PX3C Implementation (nicht voll zertifiziert)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX3B: `bbe418d` / v0.1.12, Cert `8443d0d` / v0.1.13, Issue #5 geschlossen.
PX3C: dieser Stand / v0.1.14. `GL_RenderFrame` bleibt 0.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**Produktpfad**
- Lifecycle / Frame: `client/render/render_core.cpp`
- GL/Offscreen: `client/render/render_backend.cpp` (State-Restore inkl. Sprite-Zustände)
- World/BSP: `client/render/render_world.cpp`
- Entity-Spiegel: `client/render/render_scene.cpp` — Kopie, kein Steal
- Sprite offscreen: `client/render/render_sprite.cpp` (PrimeXT `gl_sprite.cpp` @ `46fb05b`)
- Brücke: `cdll_int.cpp` — `GL_RenderFrame` void + `return 0`; `HUD_AddEntity` spiegelt und behält Return

**CVar:** `r_csretro_renderer` 0 = Xash-only. 1 = Offscreen World+Sprites + sichtbarer Xash-Fallback.
Probe: `./scripts/px3b-offscreen-probe.sh`.

**Callbacks an:** `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps`, `R_ClearScene` (additiv, nur CS-Retro-Liste).
**Callbacks NULL:** `Mod_GetCurrentVis`, `R_ProcessEntData`, Studio-Decals, `CL_UpdateLatchedVars`.

**PX3C Stand**
- TempEnt- und Normal-Sprites werden offscreen gespiegelt und gezeichnet (`mirrored: N drawn: N`).
- Studio nur klassifiziert, nicht gezeichnet. Local Player nicht als Studio-Pass.
- Brush-Entities gezählt, Draw DEFERRED.
- `GL_DrawParticles` / Client-Triangles DEFERRED (ändern Sim-State).
- World+Sprite CRC-Differenz auf 512² noch UNKNOWN. Sichtbares Xash ohne Regression (T/CT, Menüs, HE/Smoke/Flash).
- Issue #6: implemented / verification pending — nicht schließen ohne volle DoD.

**PX0 bleibt offen**
- #1 Movement Replay: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- #3 Erster M1 Granaten: https://github.com/benjarogit/csretro/issues/3
- Gate `./scripts/movement-contract-gate.sh` PASS. Gate ≠ Replay.

**Team / Klasse / Buy** — nach PX2 und PX3B/PX3C visuell CONFIRMED (Inhaber-Werte unverändert).

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**Kein `GL_RenderFrame → 1`.** PX4 = Studio-Fusion (GSMR bewusst aufrufen). Locks: Inferno/Zippo-Gameplay, CS2-Waffen, `pm_shared`, ImGui, PhysX, HDR/PBR.
