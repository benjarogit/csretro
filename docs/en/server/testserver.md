# Test server

The test server is a reproducible GoldSrc server for end-to-end tests. It is
**not a production server**: it only runs locally or in CI, listens with
`sv_lan 1` and starts with `-insecure`.

## Stack

| Component | Version | Role |
|---|---|---|
| [ReHLDS](https://github.com/dreamstalker/rehlds) | `3.15.0.896` | Engine |
| [ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) | `5.30.0.814` | Game logic |
| [Metamod-R](https://github.com/rehlds/Metamod-R) | `1.3.0.149` | Plugin loader |
| [AMX Mod X](https://github.com/alliedmodders/amxmodx) | `1.10.0.5487` | Plugin runtime |
| [ReAPI](https://github.com/rehlds/ReAPI) | `5.29.0.358` | Module API |
| CSRETRO module | `1.6.0` | `nextclientapi_amxx_i386.so` |
| Test plugin | – | `test_nextclient_api.amxx` |

All versions and SHA-256 digests live in `versions.lock.json`. The HLDS base
files come from App 90 through SteamCMD.

## Layout

| Path in the image | Content |
|---|---|
| `/server/hlds_linux` | ReHLDS engine plus App 90 base files |
| `/server/cstrike/dlls/cs.so` | ReGameDLL_CS |
| `/server/addons/metamod/metamod_i386.so` | Metamod-R |
| `/server/addons/plugins.ini` | Metamod plugin list, loads AMXX |
| `/server/cstrike/addons/amxmodx` | AMX Mod X, modules and plugins |
| `/server/cstrike/models`, `sound`, `sprites`, `materials` | Test plugin assets |
| `/server/libcsretro_steamcompat.so` | Steam stub, see below |
| `/entrypoint.sh`, `/healthcheck.py` | Start and healthcheck |

The image is built from `server/pack/`. The build context additionally holds the
vendored runtime archives from `server/vendor/dist/`, the built module, the
compiled test plugin and the test assets. `tests/e2e/run.sh` assembles that
context; by hand, `scripts/build-server.sh --stage` does the same.

## Quick start

```bash
tests/e2e/run.sh
```

The script

1. builds the module and the test plugin through `scripts/build-server.sh --stage`,
2. checks the vendored runtime archives from `server/vendor/dist/` by SHA-256,
3. builds the image and starts it with Docker Compose,
4. waits for the healthcheck,
5. checks log and RCON with `tests/e2e/test_server.py`,
6. removes containers and volumes on every exit path.

Requirements: Docker with Compose v2 and network access on the first run, because
SteamCMD downloads App 90. The server binaries come from `server/vendor/dist/`.

| Variable | Default | Meaning |
|---|---|---|
| `CSR_E2E_HOST_PORT` | `27015` | published UDP port |
| `CSR_E2E_RCON_PASSWORD` | `csretro-e2e` | RCON password |
| `CSR_E2E_MAP` | `de_dust2` | start map |
| `CSR_E2E_PROJECT` | `csretro-e2e` | Compose project name |

Inside the container, `entrypoint.sh` sets `CSR_MAP`, `CSR_MAXPLAYERS`,
`CSR_PORT` and `CSR_RCON_PASSWORD`; all have defaults so the image also starts
without Compose.

## Healthcheck and RCON

The healthcheck requests `getchallenge` over UDP and then sends
`rcon <challenge> <password> status`. This is the classic GoldSrc procedure:
without a prior challenge the server answers `Bad rcon_password.` even when the
password is correct. A successful reply starts with `\xff\xff\xff\xffl` and
contains `hostname`, `version`, `map` and `players`.

`tests/e2e/test_server.py` additionally checks:

- `Metamod-r v1.3.0.149`, AMXX `1.10.0.5487` and
  `[nclapi] Next Client API Successfully loaded, version 1.6.0` in the log,
- `Started map "de_dust2"`,
- `nclapi_version` is `1.6.0`,
- `amxx plugins` reports `Test NCL API` as `running`,
- no `FATAL ERROR`, `Fencepost`, `Segmentation fault`, no dump and no
  `[AMXX] Run time error`.

## Steam stub

ReHLDS 3.15 was built against the Steamworks ABI from before the 25th
anniversary: it imports `SteamGameServer_Init`, `SteamGameServer` and `SteamApps`
and expects `ISteamGameServer011`. The Default Public App 90 package only ships
`SteamInternal_GameServer_Init` and `SteamGameServer015`. The vtable layouts are
not interchangeable; loading the new library directly crashes in
`CSteam3Server::Activate()` or floods the log with interface assertions.

The test server therefore answers that ABI itself.
`server/pack/steamcompat/steamstub.cpp` is built as
`libcsretro_steamcompat.so` and preloaded into the engine process with
`LD_PRELOAD`:

- `SteamGameServer_Init` reports success and `SteamGameServer()` returns a no-op
  `ISteamGameServer011` implementation.
- `SteamAPI_Init` reports `false`, which is the path ReGameDLL already handles
  when no Steam client is installed.

The stub authenticates nobody and exists only for the local, insecure test
server. A production server needs real Steam connectivity; that is not part of
G06.

## No A2S with the stub

ReHLDS no longer answers connectionless queries itself: `SVC_Info` and
`ValidInfoChallenge` are `NOXREF`, and `SV_ReadPackets` forwards every
connectionless packet to `Steam_HandleIncomingPacket`. The engine collects the
replies from the Steam game-server layer through `GetNextOutgoingPacket`. With
the stub that layer does not exist, so the server does **not** answer
`A2S_INFO`.

The healthcheck therefore uses RCON, which the engine handles itself. How A2S
becomes available again under Default Public is an open decision for G07: either
an adapter from `ISteamGameServer011` to the 015 interface or a dedicated query
responder.

## Runtime base

The runtime image is `i386/debian:trixie-slim` (glibc 2.41). The CSRETRO module
requires `GLIBC_ABI_GNU_TLS` and `GLIBC_2.38`; Debian 12 (glibc 2.36) cannot load
it. The build stages run on `debian:bookworm-slim`, because they only unpack
archives and compile the stub with `g++-multilib`.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `CSR_STEAMCMD_IMAGE` is missing | Compose started without a digest | use `tests/e2e/run.sh` or set the variable from `versions.lock.json` |
| image build fails on App 90 | SteamCMD network error | run again; the build retries the download four times |
| healthcheck stays `unhealthy` | map does not load or RCON password is empty | check `docker compose logs server` |
| `Bad rcon_password.` despite the right password | challenge step missing | send `getchallenge` first, then `rcon <challenge> <password> <command>` |
| `GLIBC_ABI_GNU_TLS not found` | runtime older than the build host | do not move the runtime image back to bookworm |
| `Module ... failed to load` | module missing or wrong AMXX version | compare the staging manifest with `versions.lock.json` |
| `Mod_LoadModel: ... not found` | test assets missing from the context | rerun `scripts/build-server.sh --stage` |
| `Not loading plugin ... already loaded` | module listed twice in `modules.ini` | keep `modules.ini` empty; AMXX detects modules itself |
| no `A2S_INFO` | Steam stub without a game-server layer | expected; use RCON for healthchecks |

## Evidence (G06, 8 November 2026)

```text
ok   log: Metamod-r v1.3.0.149
ok   log: AMX Mod X version 1.10.0.5487
ok   log: [nclapi] Next Client API Successfully loaded, version 1.6.0
ok   log: Started map "de_dust2"
ok   rcon: map     :  de_dust2 at: 0 x, 0 y, 0 z
ok   rcon: "nclapi_version" is "1.6.0"
ok   rcon: amxx plugins: Test NCL API running
all e2e checks passed
```
