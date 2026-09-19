# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1-Research + PX2-Nachweis + PX3B: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — PX3B visuell zertifiziert (Strategie C)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX1-Docs: `1235cb7` / v0.1.7. PX2-Brücke: `17bd79f` / v0.1.8. Visuell zertifiziert 2026-09-20.
PX3A: `1df63fb` / v0.1.11. PX3B: `bbe418d` / v0.1.12. Cert: dieser Docs-Stand / v0.1.13.
`GL_RenderFrame` bleibt 0.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).
„Kein PrimeXT-Produktimport“ ≠ „keinen PrimeXT-Code adaptieren“.

**PX3B Produktpfad**
- Lifecycle: `client/render/render_core.cpp` (Init/VidInit/Shutdown/Map)
- GL/Offscreen: `client/render/render_backend.cpp` (Default aus, State-Restore, 512² FBO)
- World/BSP: `client/render/render_world.cpp` (Xash-`model_t`-View, Mesh-Kopie, textured + Baseline-LM)
- Brücke: `client/body/cl_dll/cdll_int.cpp` — `GL_RenderFrame` ist void-Aufruf + `return 0`

**CVar:** `r_csretro_renderer` 0 = Xash-only (kein Extra-Cost), 1 = Offscreen-Probe + sichtbarer Xash-Fallback.
Diagnose: `r_csretro_offscreen_dump`, `r_csretro_probe_seq`. Probe: `./scripts/px3b-offscreen-probe.sh`.

**Callbacks an:** `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps` (additiv).
**Callbacks NULL:** `Mod_GetCurrentVis`, `R_ClearScene`, `R_ProcessEntData`, Studio-Decals, `CL_UpdateLatchedVars`.
`R_ClearScene` wird erst in PX3C additiv gesetzt (nur CS-Retro-Liste; Xash-Liste bleibt).

**PX3B Cert 2026-09-20 (Issue #5 geschlossen)**
Isolierter Lauf `r_csretro_renderer 1`, Shots `build/px3b-cert-shots/` (nicht committed).
T/CT Join+Spawn, World/VM/HUD, Team/Klasse/Buy-Previews, HE-Explosion, Smoke-Wolke, Flash-Wurf.
Offscreen nonempty: dust `empty=0 crc=24f7c85e`, aztec `empty=0 crc=b574ac9e`, `vid_setmode` `crc=6a698541`.
GL state isolation: **CONFIRMED** soweit sichtbar (kein FBO/Blend/Depth/Viewport-Leak, Folgerahmen sauber).
`./scripts/movement-contract-gate.sh` PASS. `./scripts/px3b-offscreen-probe.sh` PASS.

**PX2 unverändert sichtbar:** Xash zeichnet World/Entities/HUD/VM. Issue #4 geschlossen.

**PX0 bleibt offen**
- #1 Movement Replay / N-Frame: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- Gate `./scripts/movement-contract-gate.sh` PASS (Konstanten + fuser2 450). Gate ≠ Replay.

**Weitere Issues**
- #3 Erster M1 bei Granaten: https://github.com/benjarogit/csretro/issues/3
- #5 PX3B World offscreen: geschlossen (DoD erfüllt)

**Team / Klasse / Buy** (Inhaber-Werte unverändert; nach PX2 und PX3B visuell CONFIRMED)
- Team: World `44×70`, `SetCameraHeight(-2)`. Emblem `cy = 56%`. T- und CT-Preview sichtbar.
- Klasse: vier `CTeamModelPreview`, World `48×80`. T- und CT-Lineup sichtbar.
- Buy: dunkle Zellen, `w_*.mdl`, Aspect `2.20`, FOV `13°`. Player- und Waffen-Preview T/CT sichtbar.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**PX3C als Nächstes.** TempEnt-`cl_entity_t` ist bereits über `HUD_AddEntity` sichtbar (keine neue Engine-ABI).
Sprite-Draw ≠ `GL_DrawParticles`. GSMR nur bei bewusstem Aufruf.
Locks: Inferno/Zippo-Gameplay und MR15, keine CS2-Waffen. `pm_shared` nicht umbauen.
**Kein `GL_RenderFrame → 1`.** `return 1` erst wenn klar ist, wer jeden notwendigen sichtbaren Pass besitzt.
