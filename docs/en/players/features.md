# What CSRETRO provides

The client is not playable yet. CSRETRO builds on the vendored NextClient source and is being ported to Linux x86_64 with Xash3D. Original Counter-Strike 1.6 content is required. Source code being present does not mean a feature has passed the Xash runtime checks.

## HUD and radar

The inherited client contains health, damage direction, damage icons, killfeed, crosshair and radar extensions. Advanced radar supports generated map imagery, floor selection and markers for known player positions. It must not reveal positions the client does not know. These features still need validation in a real x64 match.

## Weapons and camera

The source includes weapon inspection, viewmodel placement and field-of-view controls, camera movement and weapon material rendering. Rendering, animation, prediction and weapon events must be checked together during the port; a working settings menu does not establish that weapons work.

## Menu, servers and downloads

NextClient contributes native VGUI2 menus, a server browser, resource downloads and protection against unwanted server commands. CSRETRO keeps the native UI direction. Browser-based MOTD, Steam authentication, downloads and server connectivity must not be assumed to work merely because the host displays the menu.

## Performance tools

The source includes CPU/GPU frame monitoring, network diagnostics, map inspection and renderer capture tools. They are development aids. The product target is at least 60 FPS, without a 60 FPS cap; it needs a measurement in an actual scene, not a menu-only benchmark.

## Server-controlled features

The vendored server API supports controlled cvar changes, HUD sprites, viewmodel effects, weapon sound overrides and material synchronization. A plugin must wait for client readiness and check capability support. See [plugin development](../plugins/index.md).

## Ideas from other clients

Buy presets, additional crosshair and HUD options, prediction improvements and spectator polish are tracked in the [authoritative backlog](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md#15-backlog-nach-100). A reference project's feature is not a CSRETRO feature until it is implemented, tested and documented here. Attribution and license boundaries live in [CREDITS](https://github.com/benjarogit/csretro/blob/main/CREDITS.md).

## Where to continue

- [Prepare a separate game-content copy](../client/install.md).
- [Understand settings and commands](settings.md).
- [Read the current evidence and remaining work](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
