# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-08
- **Gate:** G06 – reproducible test server, **done**
- **Branch:** `main`
- **Commit:** `164e4e7` (G06 code), handoff commit follows
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** https://github.com/benjarogit/csretro/actions/runs/42514310451 – success

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| Test server starts reproducibly | `tests/e2e/run.sh` exit 0, container `healthy` |
| Image builds from the pinned SteamCMD digest | staging manifest records the digest from `versions.lock.json`; build exits 0 |
| ReHLDS, ReGameDLL, Metamod-R and AMXX load | log: `Metamod-r v1.3.0.149`, `AMX Mod X version 1.10.0.5487`, ReGameDLL `5.30.0.814-dev` |
| The module loads in a real server | log: `[nclapi] Next Client API Successfully loaded, version 1.6.0` |
| The module version is queryable | rcon `nclapi_version` → `"nclapi_version" is "1.6.0"` |
| The test plugin runs | rcon `amxx plugins` → `Test NCL API … running`, `1 plugins, 1 running` |
| The map starts | log: `Started map "de_dust2" (CRC "1159425449")` |
| RCON challenge flow works | `tests/e2e/test_server.py` checks `status` and the plugin list |
| No crash and no assertion spam | log has no `FATAL ERROR`, `Fencepost`, `Segmentation fault`, no dump |
| The test plugin finds its assets | `test_nextclient_api.log`: material preset uploaded, no runtime error |
| Cleanup is guaranteed | `run.sh` trap runs `docker compose down --volumes --remove-orphans` |
| Doctor reports container tooling | `scripts/doctor.sh`: 43 passed, 0 warnings, 0 failed |
| Repository checks pass | `scripts/verify.sh` green |

Reference run (2026-11-08, local):

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

## Decisions taken in G06

1. **Steam ABI.** ReHLDS 3.15 imports `SteamGameServer_Init`, `SteamGameServer`
   and `SteamApps` and expects `ISteamGameServer011`. App 90 ships only
   `SteamInternal_GameServer_Init` and `SteamGameServer015`; the vtable layouts
   are incompatible. The test server uses a self-contained no-op stub
   (`server/pack/steamcompat/steamstub.cpp`) instead of the Steam client.
2. **No A2S with the stub.** ReHLDS forwards connectionless packets to the Steam
   game-server layer (`Steam_HandleIncomingPacket` / `GetNextOutgoingPacket`),
   so the stub yields no `A2S_INFO`. Healthcheck and e2e use RCON, which the
   engine handles itself.
3. **Runtime base `i386/debian:trixie-slim`.** The module needs
   `GLIBC_ABI_GNU_TLS` and `GLIBC_2.38`; Debian 12 has glibc 2.36.
4. **Empty `modules.ini`.** AMXX detects modules from the natives a plugin uses.
   Listing them explicitly makes Metamod load each one twice and log
   `already loaded`.
5. **Assets are staged.** The test plugin precaches custom models, sounds,
   sprites and a material preset from `server/api/amxx_test/`; a missing model
   aborts the map load.

## Distribution and data acquisition

- **Docker is not the shipping path.** The container in `server/pack/` exists for
  reproducible tests and CI. Players and server admins get a normal, portable
  installation: a directory plus `csretro.sh` (G09). Docker stays an optional
  way to host a *server*; the client always runs natively because it needs
  32-bit graphics and input, not a container.
- **Server base files are free.** App 90 (HLDS) downloads anonymously through
  SteamCMD; no Steam account, no ownership, no login. That is what the image
  build does and what a self-hosted server install will do.
- **Client files are not free.** CS 1.6 is the paid App 10. CSRETRO never logs
  into Steam and never asks for credentials. The installer discovers the local
  Steam libraries (`steamapps/libraryfolders.vdf`, `appmanifest_10.acf`), verifies
  the stock files and copies them into the separate CSRETRO directory; a manual
  `--source <dir>` works too. The vendored code for this is
  `client/installer/import_game.py`.
- **Manual copy stays possible.** `scripts/install.sh` (G09) is a convenience
  around copy, hashes and backups; a user who already has a valid game folder can
  point CSRETRO at it.
- **Open:** whether the client still needs a running Steam client for auth
  tickets after G08 replaces the protocol. The vendored launcher starts through
  the Steam runtime today.

## Server performance backlog (after G06b)

Optimisation work on the AMXX module and plugins. Order is deliberate: measure
first, then change one thing at a time and re-measure on the test server.

1. **Measure first.** Baseline server FPS (`stats`), CPU (`docker stats`,
   `pidstat`), per-plugin cost (AMXX profiler) and the hot forwards/hookchains
   before touching code.
2. **Prefer ReAPI hookchains over Fakemeta.** `RegisterHookChain(RG_*)` is
   cheaper and more targeted than `register_forward(FM_*)`; keep Fakemeta only
   where ReAPI has no equivalent.
3. **Move hot paths into a native Metamod plugin.** C++ for per-frame or
   per-player work, AMXX for gameplay glue and the public native surface.
4. **Avoid or batch per-frame hooks.** Throttle to every N frames where
   semantics allow, aggregate work, and keep string formatting out of hot paths.

These need a gate entry in `docs/plan/csretro-plan.md` (working name G06c) before
implementation; the plan is the only source for the gate list.

## Open / known broken

- **A2S under Default Public:** open decision for G07. Either adapt
  `ISteamGameServer011` to the 015 interface or write a dedicated query
  responder.
- **Steam stub is test-only.** It authenticates nobody and must never be used
  for a public server. A production server needs real Steam connectivity.
- **E2E is not in CI yet.** The Docker job arrives with G10; the test needs
  network access for App 90.
- **Runtime and toolchain are downloaded, not vendored.** `.cache/` is ignored
  and removed by `scripts/clean.sh`; the first run after a clean needs network.
- **Vendored identity strings:** NextClient and the server API still carry
  upstream Sentry, GameAnalytics, updater and identity strings. Removed in G08.
- **Upstream patches:** `server/api/deps/rsa_keygen/` needs two portability
  fixes, recorded in `patches/README.md`.
- **Path with spaces:** all builds go through `/tmp/csretro-src`. Keep using the
  scripts.
- **Branch protection** is unavailable on a private free-plan repository. The
  tracked `pre-push` hook and CI are the two checks.

## Next task

- **Gate:** G06b – upstream radar (decided after G06; add it to the plan's gate
  list before starting), then G07 – Default Public runtime.
- **After G07:** schedule the server performance backlog above (measurement
  first, then hookchain/native/per-frame work). It needs a plan gate entry.
- **G06b acceptance:**
  - Report upstream changes since the vendored pins for NextClient,
    NextClientServerApi, ncl-hl1-source-sdk and vcpkg.
  - Classify each change: take, defer or ignore, with a reason.
  - Update `versions.lock.json` only for what is actually imported.
- **G07 acceptance (plan section 7):** Default Public build registry, resolver,
  offsets, first start, ABI validation, feature smoke. Includes the A2S decision
  from above.

## Open decisions

1. Windows support: drop the CEF/browser code or keep it disabled. Decided in G08.
2. Discord app, Sentry project, GameAnalytics keys, update signing key and
   hosting are needed for G12/G13.
3. A2S under Default Public: 011→015 adapter versus own query responder (G07).
4. G06b needs a gate entry in `docs/plan/csretro-plan.md`; the plan is the only
   source for the gate list.

## Environment

| Item | Value |
|---|---|
| Host | Linux x86_64, CachyOS |
| glibc | 2.44 |
| GCC | 16.2.1 |
| CMake | 4.4.5 |
| Ninja | 1.13.2 |
| Python | 3.14.9 |
| Git LFS | 3.8.1 |
| Docker | 29.9.3 |
| Docker Compose | 5.7.0 |
| Multilib | works (`-m32`) |
| Steam | installed, Default Public 1.1.2.7 |
| Build symlink | `/tmp/csretro-src` → repository |
| Server toolchain | AMXX `1.10.0.5487`, ReAPI `5.29.0.358` in `.cache/toolchain/` |
| Server runtime | ReHLDS `3.15.0.896`, ReGameDLL `5.30.0.814`, Metamod-R `1.3.0.149` in `.cache/downloads/` |

The retail game lives in the maintainer's Steam library. It is **not** part of the
repository and must never be committed. CSRETRO is always installed into a separate
copy outside the Steam library.

## Last commands and results

```text
$ tests/e2e/run.sh
==> staging runtime archives, module, plugin and test assets
==> assembling the Docker build context in dist/testserver-context
==> building and starting the test server on udp/27015
==> collecting the server log
==> running protocol and log checks
ok   log: Metamod-r v1.3.0.149
ok   log: AMX Mod X version 1.10.0.5487
ok   log: [nclapi] Next Client API Successfully loaded, version 1.6.0
ok   log: Started map "de_dust2"
ok   rcon: "nclapi_version" is "1.6.0"
ok   rcon: amxx plugins: Test NCL API running
all e2e checks passed
==> e2e passed
```

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | done | subtree `3fea0c3`, `501d93b`, verify green |
| G03 | done | doctor 40/0, vcpkg bootstrapped, prerequisites documented |
| G04 | done | 8 ELF32 artifacts, manifest, build script, CI run `42482462774` |
| G05 | done | module ELF32 + metadata, 34/34 tests, build script, `patches/` |
| G06 | done | test server `healthy`, module and plugin loaded, e2e green, CI run `42514310451` |
| G06b | next | upstream radar |
| G07 | planned | Default Public runtime |
