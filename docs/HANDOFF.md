# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1-Research + PX2-Nachweis: `docs/research/px1-primext.md`.

## Stand 2026-09-19 — PX2 (Brücke da, kein PX3)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX1-Docs: `1235cb7` / Release v0.1.7. PX2 folgt als eigener Commit / v0.1.8.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**PX2:** `HUD_GetRenderInterface` liefert eigene `render_interface_t` (v37), nur `GL_RenderFrame` → 0.
CVar `r_csretro_renderer` 0/1; 1 täuscht keinen Custom-Renderer vor. Kein PrimeXT-Copy nach `client/body/`, kein ImGui, kein PhysX, kein Studio-Ersatz.

**PX0 bleibt offen**
- #1 Movement Replay / N-Frame: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- Gate `./scripts/movement-contract-gate.sh` PASS (Konstanten + fuser2 450). Gate ≠ Replay.

**Weitere Issues**
- #3 Erster M1 bei Granaten: https://github.com/benjarogit/csretro/issues/3
- #4 PX2-Brücke: implemented / verification pending — https://github.com/benjarogit/csretro/issues/4

**Team / Klasse / Buy** (unverändert seit 13.09., Inhaber)
- Team: World `44×70`, `SetCameraHeight(-2)`. Emblem `cy = 56%`.
- Klasse: vier `CTeamModelPreview`, World `48×80`.
- Buy: dunkle Zellen, `w_*.mdl`, Aspect `2.20`, FOV `13°`.
- PX2 hat diese Pfade nicht angefasst. In-Game-Vorschau nach PX2: UNKNOWN.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz. **Nicht Inhaber-PASS.**

Locks: Inferno/Zippo-Gameplay und MR15, keine CS2-Waffen. `pm_shared` nicht umbauen. **PX3 nicht still starten.**
