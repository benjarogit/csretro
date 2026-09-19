# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1-Research + PX2-Nachweis: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — PX2 verifiziert, PX3A: Strategie C

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX1-Docs: `1235cb7` / v0.1.7. PX2-Brücke: `17bd79f` / v0.1.8. Visuell zertifiziert 2026-09-20.

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**PX2:** `HUD_GetRenderInterface` liefert eigene `render_interface_t` (v37), nur `GL_RenderFrame` → 0.
CVar `r_csretro_renderer` 0/1; 1 täuscht keinen Custom-Renderer vor. Issue #4 geschlossen nach visuellem DoD.
Kein PrimeXT-Copy nach `client/body/`, kein ImGui, kein PhysX, kein Studio-Ersatz.

**PX0 bleibt offen**
- #1 Movement Replay / N-Frame: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- Gate `./scripts/movement-contract-gate.sh` PASS (Konstanten + fuser2 450). Gate ≠ Replay.

**Weitere Issues**
- #3 Erster M1 bei Granaten: https://github.com/benjarogit/csretro/issues/3
- #5 PX3B World offscreen (`GL_RenderFrame` bleibt 0): https://github.com/benjarogit/csretro/issues/5 — nicht gestartet

**Team / Klasse / Buy** (Inhaber-Werte unverändert; nach PX2 visuell CONFIRMED)
- Team: World `44×70`, `SetCameraHeight(-2)`. Emblem `cy = 56%`. T- und CT-Preview sichtbar.
- Klasse: vier `CTeamModelPreview`, World `48×80`. T- und CT-Lineup sichtbar.
- Buy: dunkle Zellen, `w_*.mdl`, Aspect `2.20`, FOV `13°`. Player- und Waffen-Preview T/CT sichtbar.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.

**PX3A:** Strategie **C** (World offscreen/diagnostisch, `GL_RenderFrame` bleibt 0). Sichtbarer Takeover erst nach Entity/EFX/Studio-Besitz. Details: `docs/research/px1-primext.md` Abschnitt PX3A.

Locks: Inferno/Zippo-Gameplay und MR15, keine CS2-Waffen. `pm_shared` nicht umbauen.
**Kein `GL_RenderFrame → 1` und kein PrimeXT-World-Port ohne neue Freigabe (PX3B).**
`return 1` erst wenn klar ist, wer jeden notwendigen sichtbaren Pass besitzt.
