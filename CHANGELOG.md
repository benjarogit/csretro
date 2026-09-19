# Changelog

## Unreleased

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
