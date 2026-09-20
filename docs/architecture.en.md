# Architecture

CS Retro integrates existing projects into its own runtime.
[Upstream pins](upstream.md) and [credits](credits.md) distinguish provenance from runtime roles.

```text
CS Retro (one codebase)
├── Engine: Xash3D-FWGS-derived — sole runtime, extensible
├── Client: one client_amd64.so
│   ├── CS body (Velaron/NextClient-derived)
│   ├── Movement/prediction (same contract as the GameDLL)
│   └── PrimeXT-derived tech (`client/render/`; PX3B world/BSP, PX3C entity/sprite, PX4A non-player studio, PX4B.1 FOLLOW, PX4B.2 remote-player variant B, PX4B.3 local eligibility + player shadows, PX4C.1 viewmodel BODY VERIFIED, PX4C.2 event ownership VERIFIED, PX5 vis seam behind return 0, #7 brush special A/B/C/D/E complete + engine EFX + client triangles draw-only)
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
| `client/render/` | CS Retro renderer (lifecycle, offscreen GL, world/BSP, brush entities, sprite, non-player studio). One `client_amd64.so`. |
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
PX1 research and port matrix: [docs/research/px1-primext.md](research/px1-primext.md). PX2 (`17bd79f`) is the `HUD_GetRenderInterface` bridge. PX3A: strategy C. PX3B (`bbe418d`, visually certified): world/offscreen. PX3C (visually certified, #6 closed): entity mirror + sprite offscreen. PX4A (visually certified, #8 closed): non-player studio offscreen via GSMR `STUDIO_RENDER`. PX4B.1: non-player FOLLOW path exists, not reproducible on stock CS maps. PX4B.2/B.3 and #7 brush/EFX/triangles remain as in the German architecture note. PX5 vis seam sits behind return 0 (`PrepareCurrentFrameVis`, client PVS buffer, world surface mask). Issue [#11](https://github.com/benjarogit/csretro/issues/11) stays open until the full DoD. #7 CLOSED, #9 CLOSED, viewmodel [#10](https://github.com/benjarogit/csretro/issues/10) CLOSED (PX4C.1 body + PX4C.2 event ownership). Visible takeover (`return 1`) only after a new release.
