# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.

## Stand 2026-09-19 — PX0 (Architektur eingefroren)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`. PrimeXT = Technik-Upstream im Client, nicht zweite DLL. PX1 nur Research. PX2 erst nach Movement-Gate PASS. ImGui fest für Tools, Spieler-UI nur nach Evaluation. Xash-Renderer bleibt optionaler Diagnose-Fallback.

**PX0 in dieser Session**
- `engine/ROLE.md`, `client/ROLE.md`, `server/ROLE.md`, `docs/architecture.{de,en}.md`, `docs/upstream.{de,en}.md`: Beobachten-Beschluss ersetzt.
- Movement-Vertrag wieder lockstep: `fuser2`-Landebremse 450 ms in Client- und GameDLL-`pm_shared` (uncommitted Drift hatte sie entfernt). Gate: `./scripts/movement-contract-gate.sh`.
- Incendiary: kein `TE_SPRITE` mehr pro Inferno-Node (nur Zündung). 10×-Spread füllte den Tent-Pool.

Noch offen für das Gate (nicht PX2-blockierend, aber messen): Replay-Inputs über N Frames (Position/Velocity). Bis dahin gilt der Konstanten-/Hitch-Vergleich.

**Team / Klasse / Buy** (unverändert seit 13.09., Inhaber)
- Team: World `44×70`, `SetCameraHeight(-2)`. Emblem `cy = 56%`.
- Klasse: vier `CTeamModelPreview`, World `48×80`.
- Buy: dunkle Zellen, `w_*.mdl`, Aspect `2.20`, FOV `13°`.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen (falscher `dirname`). Keine Zweitinstanz. **Nicht Inhaber-PASS.**

Locks: Inferno/Zippo-**Gameplay** und MR15, keine CS2-Waffen. Inferno-**Darstellung** (Tent-Last) in PX0 angepasst; Zippo unangetastet.

Kein PrimeXT-Renderer-Import in PX0.
