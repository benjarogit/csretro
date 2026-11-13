# Prove that the client can run

Use this procedure to distinguish a built extension, an open UI window and a working game client. The first runnable version needs the complete path: native game implementation, Xash3D engine, input, rendered map, game simulation and a measured frame rate.

## Inspect the game library without starting it

From the repository root, run:

```sh
python3 tools/check_game_client.py path/to/game-client.so
```

This checks ELF64/x86_64 shared-library identity and CSRETRO's core named client callbacks. It reads the dynamic symbol table with `nm`; it never loads or starts the library. Exit 1 identifies a failed screen or unreadable artifact. `CreateInterface` alone is the extension-factory contract, not the game-client contract. An alternative `GetClientAPI` entry needs a separate table/ABI check; this tool does not certify it.

Exit 0 establishes only these core exports. It proves neither callback signatures nor ABI layout, a loaded engine, useful callback bodies, gameplay, authentication or FPS. The JSON always keeps `playability` as `unverified`. Tests use native synthetic fixtures, including one that would trap if loaded.

## Verify the integration

Build with the `linux` preset and test with `linux-tests`. Check the actual shared artifacts, not just static test libraries. A real engine must obtain the game callbacks and pass its own engine table to them. Document the native game-code origin and compatible license before importing source. The [plan](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md) defines the permitted upstream boundaries.

A supplied address, factory lookup, no-op or recorded primitive is only evidence for that operation. It cannot close a gameplay milestone. Retain failure cases for missing callbacks, wrong architecture, failed initialization, input, map changes, disconnects and shutdown.

## Human gameplay acceptance

Automation never starts the game. Once the real engine/client path is prepared, a human runs the separate installed copy and records:

1. Source revision and artifact hashes; native x86_64 engine and game library; no retail `hw.so` or Steam process used by the port.
2. A real map rendered from original content, usable mouse/keyboard, movement, collision, weapon firing, HUD and sound.
3. A local/test-server session with connection, team selection, round changes, disconnect and clean shutdown. A public server additionally needs genuine authentication.
4. At least 60 FPS in a stated playable scene, with hardware, resolution, renderer, settings, measurement duration and frame-time evidence. Record menu performance separately; do not cap the product at 60 FPS.
5. The associated automated test results and green CI link, plus any remaining failures. Store project evidence in `HANDOFF.md` and maintainer-local paths/logs under `.local/`.

A failed or missing item leaves the runnable-version objective open. See [settings reports](../players/settings.md) and [the feature guide](../players/features.md) when documenting the player-visible result.
