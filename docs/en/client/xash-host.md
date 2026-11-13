# Host and surface integration

The host is the integration harness for one VGUI2 surface, the mounted content filesystem, the matched client-extension/VGUI libraries and GameUI. It is not yet a running Xash3D game engine. See [runtime acceptance](../maintainers/runtime-readiness.md) before treating an open window as a successful port.

The surface factory has one identity across those modules. Input reaches that surface through the host; drawing and filesystem reads use the mounted content copy. Missing modules, interfaces or files must return a failure rather than execute a null function. Retail `hw.so` is outside the x64 process.

## Build and tests

Build `xash_host` with the `linux` preset and `xash_host-tests` with `linux-tests`. The tests exercise factories, content access, UI loading and window/input behavior with their own offscreen or Xvfb environment. They do not prove a running CS match. Follow [client build instructions](../build/client.md).

Surface drawing has its own [adapter guide](xash-surface.md). Launch composition belongs to the [launcher guide](xash-launcher.md). Current test counts, source identity, CI and remaining integration work belong to [HANDOFF](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md), not copies on each component page.
