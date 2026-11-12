# Player identity

There is one slot. `ResolvePlayerIdentity` receives the in-game name from the
`name` setinfo and an optional Steam persona.

When nobody is logged in, the slot keeps the in-game name and has no portrait.
When the persona is logged in and has a name, that is the shown name. The
portrait is the pixels the call already carries. The function does not
download an image, store credentials, or start Steam.

The decision is in [plan section 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Test

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target player_identity-tests
./build/linux-tests/out/bin/Release/player_identity-tests
```

The test does not use the network and does not start the game.
