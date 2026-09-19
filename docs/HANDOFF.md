# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1-Research: `docs/research/px1-primext.md`.

## Stand 2026-09-19 — PX1 (Research fertig, kein PX2-Code)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
HEAD: `76ee12f` (Release v0.1.6). PrimeXT-Vergleichsbaum lokal, nicht im Produktbuild.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**PX1-Schlussfolgerung:** PX2 = nur `HUD_GetRenderInterface` + `GL_RenderFrame` return 0 (Xash-Fallback).
Kein PrimeXT-Copy nach `client/body/`, kein ImGui, kein PhysX, kein Studio-Ersatz.

**PX0 bleibt offen**
- #1 Movement Replay / N-Frame: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- Gate `./scripts/movement-contract-gate.sh` PASS (Konstanten + fuser2 450). Gate ≠ Replay.

**Weitere Issues**
- #3 Erster M1 bei Granaten: https://github.com/benjarogit/csretro/issues/3
- #4 PX2-Brücke (nicht gestartet): https://github.com/benjarogit/csretro/issues/4

**Team / Klasse / Buy** (unverändert seit 13.09., Inhaber)
- Team: World `44×70`, `SetCameraHeight(-2)`. Emblem `cy = 56%`.
- Klasse: vier `CTeamModelPreview`, World `48×80`.
- Buy: dunkle Zellen, `w_*.mdl`, Aspect `2.20`, FOV `13°`.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz. **Nicht Inhaber-PASS.**

Locks: Inferno/Zippo-Gameplay und MR15, keine CS2-Waffen. `pm_shared` nicht umbauen. PX2 nicht still starten.
