# Vendored server stack

Source snapshots of the server foundations CSRETRO builds and patches.
Each tree is the release commit recorded in `versions.lock.json`. Upstream
licenses stay in the tree they belong to.

| Tree | Release | Commit |
|---|---|---|
| `amxmodx` | 1.10.0.5487 | `8fc12670ea86470a0bfed4c35db5de73d3a39e8a` |
| `reapi` | 5.29.0.358 | `1c448d06e8c1cebaea061d6b81b94e85f6262649` |
| `rehlds` | 3.15.0.896 | `196300131858d1e087267fb2fb60cb935f8f8873` |
| `regamedll` | 5.30.0.814 | `781a68ae1c6fb652cf4fbc894970b4fb4dde19f9` |
| `metamod-r` | 1.3.0.149 | `603a2574e937171aa5e737bd5e7b4a07f74e88ee` |

`dist/` holds the pinned Linux release archives of those same versions.
`scripts/fetch-server-toolchain.sh` and `scripts/fetch-server-runtime.sh`
read those files and check the SHA-256. They do not download them.

Pawn includes used by the compiler come from `amxmodx/plugins/include` and
`reapi/reapi/extra/amxmodx/scripting/include`. `amxxpc` itself is the binary
from the AMXX base archive until it is built from `amxmodx/`.

Valve game content and the SteamCMD image are not in this directory.
