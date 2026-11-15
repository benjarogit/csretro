# Prove that the client can run

Port the existing NextClient behavior onto Xash3D first. Preserve Counter-Strike movement and game behavior rather than starting a CS rewrite or adding features from other clients. The historical i386 spike supplies the original CS game library from the user's licensed installation. All libraries in that run must match the engine architecture and interfaces; Xash3D replaces the Valve engine, not the CS game logic automatically.

After the port works, require exclusively x64 execution and prove at least 60 FPS in a real scene. Additional features follow that baseline. A running i386 port does not prove the later x64 phase; the later x64 requirement must not be used to skip the existing port path.

## Inspect the game library without starting it

From the repository root, run:

```sh
python3 tools/check_game_client.py path/to/game-client.so
```

This is a static screen for the later x64 phase, not a precondition for the first architecture-matched port. It checks ELF64/x86_64 shared-library identity and CSRETRO's core named client callbacks. It reads the dynamic symbol table with `nm`; it never loads or starts the library. Exit 1 identifies a failed screen or unreadable artifact. `CreateInterface` alone is the extension-factory contract, not the game-client contract. An alternative `F`/`GetClientAPI` entry needs a separate table/ABI check; this tool does not certify it.

Exit 0 establishes only these core exports. It proves neither callback signatures nor ABI layout, a loaded engine, useful callback bodies, gameplay, authentication or FPS. The JSON always keeps `playability` as `unverified`. Tests use native synthetic fixtures, including one that would trap if loaded.

## Verify the integration

The `linux`/`linux-tests` presets build/test x64 components; `linux-port`/`linux-port-tests` supply the initial i386 components. See [build profiles](../build/client.md). Neither assembles the complete runtime yet. Keep port-stage artifacts architecture-matched and record their build path separately. Check the actual shared artifacts, not just static test libraries. A real engine must obtain the game callbacks and pass its own engine table to them. Document the native game-code origin and compatible license before importing source. The [plan](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md) defines the permitted upstream boundaries.

The `xash_client` bridge preserves the original game callbacks and checks identity before loading. Its wrappers bind the registered original VGUI to Surface, connect native panels/popup flags, paint after HUD redraw, read files/fonts through the engine and route UI mouse transitions. Cursor synchronization precedes the original VGUI scheduler; relative mouse deltas remain with the game when UI is hidden. Native fixtures and actual offscreen Surface tests cover these components, including GL pixels. Native GameUI runtime acceptance, keyboard/focus acceptance and actual NextClient/runtime assembly remain open; these tests never run original libraries and do not certify movement or playability.

A supplied address, factory lookup, no-op or recorded primitive is only evidence for that operation. It cannot close a gameplay milestone. Retain failure cases for missing callbacks, wrong architecture, failed initialization, input, map changes, disconnects and shutdown.

## Check actual dependency symbols

A matching ELF architecture and a present DT_NEEDED library do not prove that the provider supplies the consumer's imports. In particular, the SDK host Steam/tier0/vstdlib libraries serve component builds and probes; their filenames do not make them replacements for the locally licensed original runtime. Keep that runtime's genuine matching dependencies together. Do not add placeholder Steam functions or change original gameplay exports to hide a loader failure.

Use `readelf -d` and the real load environment to identify the complete dependency graph, including matching system libraries. Screen each dynamically loaded consumer separately:

```bash
python3 tools/check_elf_imports.py CONSUMER.so \
  --provider DEPENDENCY_A.so --provider DEPENDENCY_B.so
```

Replace the uppercase path placeholders and repeat `--provider` for every required provider. This tool never loads the libraries. It checks strong imports, visible exports, symbol versions, matching ELF metadata and known type conflicts; optional weak imports may remain absent. The provider graph is explicitly supplied and assumed valid: the tool does not resolve DT_NEEDED, choose search paths, prove load order, inspect relocations or certify signatures, Steam authentication or gameplay. A pass stays an ABI/playability-unverified static screen. An unrelated export with the same name is not proof of the right provider.

VGUI1 support errors are a separate boundary. A Valve VGUI1 library is not FWGS's FreeVGUI support adapter, and our original VGUI2 Surface does not implement that API. Do not invent support exports. MainUI's `gfx/shell/btns_main.bmp` and `gfx/fonts` assets also differ from VGUI2 resources; stage the verified engine menu/font resources and their licenses at their real virtual paths. Preserve remaining warnings in the human log instead of treating them as passed.

## Human gameplay acceptance

Automation never starts the game. Once the real engine/client path is prepared, a human runs the separate installed copy and records:

1. Source revision and artifact hashes; matching engine/game/extension architecture; no retail `hw.so` or Steam process used by the port. Record any original game library supplied locally. Exclusively x64 is a separate following phase.
2. A real map rendered from original content, usable mouse/keyboard, movement, collision, weapon firing, HUD and sound.
3. A local/test-server session with connection, team selection, round changes, disconnect and clean shutdown. A public server additionally needs genuine authentication.
4. After the functional port, exclusively x64 and at least 60 FPS in a stated playable scene, with hardware, resolution, renderer, settings, measurement duration and frame-time evidence. Record menu performance separately; do not cap the product at 60 FPS.
5. The associated automated test results and green CI link, plus any remaining failures. Store project evidence in `HANDOFF.md` and maintainer-local paths/logs under `.local/`.

A failed or missing item leaves its port or subsequent x64/performance phase open. See [settings reports](../players/settings.md) and [the feature guide](../players/features.md) when documenting the player-visible result.
