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

The `xash_client` bridge now preserves the original `F` callback table and checks the game identity before loading. Its native fixture tests cover delegation and failure cases. Its initialization/shutdown wrappers now bind the registered original VGUI to Surface through named symbols. Live traversal/rendering/input/file callbacks, GameUI and actual NextClient/runtime assembly remain open; these tests do not run the original game or certify its movement.

A supplied address, factory lookup, no-op or recorded primitive is only evidence for that operation. It cannot close a gameplay milestone. Retain failure cases for missing callbacks, wrong architecture, failed initialization, input, map changes, disconnects and shutdown.

## Human gameplay acceptance

Automation never starts the game. Once the real engine/client path is prepared, a human runs the separate installed copy and records:

1. Source revision and artifact hashes; matching engine/game/extension architecture; no retail `hw.so` or Steam process used by the port. Record any original game library supplied locally. Exclusively x64 is a separate following phase.
2. A real map rendered from original content, usable mouse/keyboard, movement, collision, weapon firing, HUD and sound.
3. A local/test-server session with connection, team selection, round changes, disconnect and clean shutdown. A public server additionally needs genuine authentication.
4. After the functional port, exclusively x64 and at least 60 FPS in a stated playable scene, with hardware, resolution, renderer, settings, measurement duration and frame-time evidence. Record menu performance separately; do not cap the product at 60 FPS.
5. The associated automated test results and green CI link, plus any remaining failures. Store project evidence in `HANDOFF.md` and maintainer-local paths/logs under `.local/`.

A failed or missing item leaves its port or subsequent x64/performance phase open. See [settings reports](../players/settings.md) and [the feature guide](../players/features.md) when documenting the player-visible result.
