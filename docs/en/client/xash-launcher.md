# Launcher integration

The launcher combines the host, surface, client-extension/VGUI pair, GameUI, local Steam API shim, engine extensions and NitroApi with a separate content root. Its current executable presents a menu and accepts quit/input. It does not yet start Xash3D gameplay.

## Content and launch behavior

Prepare content with the [content installer](install.md). The content root is required and must be outside the Steam library. A supplied map name can be inspected by the harness; that is distinct from the real engine loading a map, predicting movement and playing a match. Rendering a menu or recording world vertices does not establish a rendered game world.

The local Steam API shim reports logged out. It starts no Steam process and supplies no production authentication. Public server authentication requires its own implementation and evidence.

## Build and tests

Build `xash_launcher` from `linux` and `xash_launcher-tests` from `linux-tests`; see [client build](../build/client.md). Launcher tests cover input validation, content errors and the host lifecycle. Actual game startup remains a human action under the [runtime acceptance procedure](../maintainers/runtime-readiness.md).

See [host responsibilities](xash-host.md) for factory and filesystem ownership. Progress, reproducible results and next work are recorded in [HANDOFF](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
