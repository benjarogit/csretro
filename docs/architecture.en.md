# Architecture

CS Retro integrates existing projects into its own runtime.
[Upstream pins](upstream.md) and [credits](credits.md) distinguish provenance from runtime roles.

```text
CS Retro (one codebase)
├── Engine: Xash3D-FWGS-derived — sole runtime, extensible
├── Client: one client_amd64.so
│   ├── CS body (Velaron/NextClient-derived)
│   ├── Movement/prediction (same contract as the GameDLL)
│   └── PrimeXT-derived tech (renderer/graphics/ImGui tools; from PX2)
├── Menu library: CS Retro InGameUi (VGUI2 stays valid; ImGui for tools)
└── GameDLL: ReGameDLL-derived — rules, weapons, inferno, ZBot
```

Supply: Engine → renderer → HUD/UI. Game state → UI. Commands ← UI.
PrimeXT is a client tech upstream, not a second runtime and not a parallel `GetClientAPI`.
The renderer does not own gameplay. Client and GameDLL implement the same movement contract.

| Path | Responsibility |
| --- | --- |
| `engine/` | Xash3D-FWGS-derived runtime; CS Retro extensions allowed, no CS menu logic |
| `client/export/` | CS Retro client export for the engine |
| `client/body/` | Client runtime, HUD, prediction and shared weapons |
| `client/menu/` | GameUI, team, class and buy interfaces |
| `client/nextclient/`, other client sources | Functional port sources from NextClient |
| `server/game/` | ReGameDLL-based gameplay and server logic |
| `bots/` | Bot development area; current ZBot remains in the GameDLL |
| `refs/` | Comparison sources, not a second product client |
| `data/` | Import manifest and project UI resources |
| `scripts/`, `cmake/` | Building, data import and testing |

## Interfaces

The engine loads one client library through `GetClientAPI`; the adapter is in
`client/export/csretro_cdll_export.cpp`. Engine and client must agree on the table layout.
The menu exports `GetMenuAPI` and the GameMenu interface contract.
The GameDLL receives engine functions through `GiveFnptrsToDll` and supplies the entity API.

64-bit support includes pointer widths and interface structures. Do not change just one side
of a boundary. Client prediction and server weapon logic must agree on state transitions.

## Data rather than a Steam runtime

Original game data is imported separately. CS Retro does not substitute Steam's engine for its
own runtime. `XASH3D_RODIR` points to prepared game data; `XASH3D_BASEDIR` points to writable
configuration and runtime files.

## Contributing changes

Upstream changes are compared and integrated selectively, not automatically merged wholesale.
A port should identify its source, commit, affected files, license notices and tests.
Do not add a second parallel implementation of **product logic**. The Xash world renderer may remain as a diagnostic/A/B fallback (`r_csretro_renderer 0`).
PrimeXT updates are ported into the now-owned CS Retro implementation; they do not restore old upstream layers.
Binding integration plan: PX0 (contracts/baselines) before productive renderer work (PX2).
PX1 research and port matrix: [docs/research/px1-primext.md](research/px1-primext.md). PX2 is only the `HUD_GetRenderInterface` bridge (`GL_RenderFrame` returns 0 until a custom path exists).
