# Mitentwickler

Der Client ist noch nicht spielbar. Mitentwicklung läuft über Issues und Pull
Requests. GitHub Actions führen nur Befehle aus; sie ersetzen keine Entscheidung
und starten das Spiel nie.

## Einstieg

1. Repository klonen und `./scripts/bootstrap.sh` ausführen.
2. Harness lesen: [`AGENTS.md`](https://github.com/benjarogit/csretro/blob/main/AGENTS.md) – nicht hier abschreiben.
3. Stand und nächsten Task nur in [`HANDOFF.md`](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
4. Ziele und Gates nur in [`docs/plan/csretro-plan.md`](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Gate-Zyklus

Ein Gate: umsetzen → Tests → Doku DE+EN → `scripts/verify.sh` → Commit → Push →
grünes CI → Handoff. Beschlossener Bau gehört in denselben Gate wie der
Plan-Eintrag. Cursor-Arbeitspläne sind Staging, keine zweite Wahrheit.

## Build und Laufzeit

| Thema | Seite |
|---|---|
| Voraussetzungen | [Build-Voraussetzungen](../build/prerequisites.md) |
| Client bauen | [Client](../build/client.md) |
| Server-API bauen | [Server](../build/server.md) |
| Client-Laufzeit | [Runtime](../client/runtime.md) |
| Xash-Surface | [xash-surface](../client/xash-surface.md) |
| Host-Factory | [xash-host](../client/xash-host.md) |
| Launcher | [xash-launcher](../client/xash-launcher.md) |
| Spieleridentität | [player-identity](../client/player-identity.md) |
