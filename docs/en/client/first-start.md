# First client start

This check proves G07d: the client identifies the public build, patches `hw.so`,
opens the console and does not crash. It is run by hand; CSRETRO never starts the
game itself.

## Prerequisites

- Counter-Strike 1.6 from the user's own Steam installation (App 10)
- Steam is installed; the starter starts it when needed and waits for it
- The 32-bit runtime libraries are present (`scripts/doctor.sh`)

## Steps

1. Create a game copy **outside** the Steam library:

   ```bash
   cp -a "$HOME/.local/share/Steam/steamapps/common/Half-Life" "$HOME/csretro-g07d"
   ```

   The Steam library path can differ. Never install into the Steam folder.

2. Build the client and install it into the copy:

   ```bash
   ./scripts/build-client.sh --install-dir "$HOME/csretro-g07d"
   ```

   This installs eight libraries, `nextclient.sh` and the assets, keeps
   Valve's `vgui2.so` as a backup and replaces `libtier0.so` with the SDK
   variant. Valve's original stays next to it as `libtier0.so.valve`.

3. Start:

   ```bash
   cd "$HOME/csretro-g07d"
   ./nextclient.sh -windowed -console
   ```

## Expected result

| Observation | Meaning |
|---|---|
| the console opens | the engine runs |
| `Game build identified: public-linux-1.0.1.4` | the SHA-256 pair is recognised |
| no `Unknown game build` line | the registry matches |
| no `Can't get engine address provider` line | engine hooks are active |
| no crash before the console | `hw.so` is patched |

If `nextclient.sh` reports that the Steam runtime is missing, Steam has to be
installed and running. Starting without the launcher is not part of this check.

## Record

- exit code of the start
- console excerpt with the lines from the table
- log file under `logs/` in the game copy, if present
- anything unusual: missing hooks, crash, black screen

## Cleanup

The copy is disposable:

```bash
rm -rf "$HOME/csretro-g07d"
```

## Security

- Never install inside the Steam library.
- Never connect to VAC-secured servers.
- Never give Steam credentials to CSRETRO.
