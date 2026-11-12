# Maintainers

The client is not playable yet. Collaboration is Issues and pull requests.
GitHub Actions only run commands; they do not replace a human decision and they
never start the game.

## Start here

1. Clone the repository and run `./scripts/bootstrap.sh`.
2. Read the harness: [`AGENTS.md`](https://github.com/benjarogit/csretro/blob/main/AGENTS.md) — do not copy it here.
3. State and next task live only in [`HANDOFF.md`](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
4. Goals and gates live only in [`docs/plan/csretro-plan.md`](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Gate cycle

One gate: implement → tests → docs DE+EN → `scripts/verify.sh` → commit → push →
green CI → handoff. Buildable decisions belong in the same gate as the plan
entry. Cursor work plans are staging, not a second source of truth.

## Build and runtime

| Topic | Page |
|---|---|
| Prerequisites | [Build prerequisites](../build/prerequisites.md) |
| Build the client | [Client](../build/client.md) |
| Build the server API | [Server](../build/server.md) |
| Client runtime | [Runtime](../client/runtime.md) |
| Xash surface | [xash-surface](../client/xash-surface.md) |
| Host factory | [xash-host](../client/xash-host.md) |
| Launcher | [xash-launcher](../client/xash-launcher.md) |
| Player identity | [player-identity](../client/player-identity.md) |
