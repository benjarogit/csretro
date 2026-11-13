# Settings and console commands

The client is not playable yet. This guide explains how settings will be documented and where their current definitions live; it is not a claim that the Xash gameplay settings already work.

## Choose by task

Use the native options menu for input, video, sound, HUD, radar, weapons and monitoring when the corresponding ported feature is available. Change one setting at a time and compare the same map, position and scene. A menu control without its gameplay callback is not a working setting.

For console use, distinguish a cvar (a stored value) from a command (an action). Diagnostic commands can require a loaded map or an initialized renderer. Do not paste an upstream configuration wholesale: old engine versions, Windows-only browser features and upstream online services are outside the product target.

## Names, values and defaults

Cvar registration and the shared defaults in the code are authoritative. Generated command and cvar references must come from those definitions; this guide does not maintain a second default-value table. The upstream technical guides explain [radar controls](https://github.com/benjarogit/csretro/blob/main/client/docs/radar-layers.md) and [performance tools](https://github.com/benjarogit/csretro/blob/main/client/docs/performance-monitor.md), but describe the inherited implementation rather than a validated x64 release.

## Report a settings problem

Record the CSRETRO revision, map, feature, requested setting, actual result and whether it persists after reconnect or restart. Keep personal configuration paths and logs local. Submit the relevant excerpt after removing private information. Maintainers use the [runtime acceptance procedure](../maintainers/runtime-readiness.md).
