# Changelog

All notable changes to CSRETRO are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
The authoritative version string is the `VERSION` file.

## [Unreleased]

### Added

- Repository harness: `AGENTS.md`, `HANDOFF.md`, `CREDITS.md`, `VERSION`,
  `versions.lock.json`.
- Repository verification: `scripts/verify.sh`, `scripts/clean.sh`,
  `tools/verify_repo.py`.
- Local push gate: tracked `.githooks/pre-push`, installed by `bootstrap.sh`.
- Bilingual documentation skeleton (`docs/de`, `docs/en`).
- Authoritative project plan in `docs/plan/csretro-plan.md`.
- CI workflow for structure, documentation, secrets and shell checks.
- Vendored NextClient at `90b4bf7` with full history (`client/`).
- Vendored NextClientServerApi at `1da463e` with full history (`server/api/`).
- Pinned submodules `client/vcpkg` at `b5d1a94` and
  `client/dep/ncl-hl1-source-sdk` at `0e9be00`.
- `scripts/bootstrap.sh` for hooks, LFS and submodules.
- `scripts/sync-upstream.sh` to report upstream changes since the pins.
- `scripts/doctor.sh`: read-only host check with real 32-bit compile-and-link
  probes, Mesa swrast, vcpkg triplet and disk space.
- `scripts/bootstrap.sh --install-deps` for the Arch/CachyOS package set and
  automatic vcpkg bootstrap.
- Bilingual build prerequisites: `docs/de/build/prerequisites.md` and
  `docs/en/build/prerequisites.md`.
- `scripts/build-client.sh`: configure, build in dependency order, write a hash
  manifest, fail on non-ELF32 artifacts, transparent space-free symlink.
- Bilingual client build guide: `docs/de/build/client.md` and
  `docs/en/build/client.md`.
- Verified 32-bit client build: eight ELF32 artifacts including `gameui.so` and
  `vgui2.so`.
- `scripts/fetch-server-toolchain.sh`: downloads AMX Mod X `1.10.0.5487` and
  ReAPI `5.29.0.358`, verifies every SHA-256 against `versions.lock.json`,
  caches them under `.cache/` and provides an `amxxpc` wrapper.
- `scripts/build-server.sh`: configure, build, CTest, module metadata and ELF32
  validation, hash manifest.
- Bilingual server API build guide: `docs/de/build/server.md` and
  `docs/en/build/server.md`.
- Verified 32-bit server module `nextclientapi_amxx_i386.so` and 34/34 green
  tests, including the compiled Pawn test plugin.
- `patches/` with the documented `rsa_keygen` portability fix for GCC 16 and
  the 32-bit OpenSSL archives.
- `scripts/lib/fetch.sh`: shared download helper that reads pins from
  `versions.lock.json`, verifies SHA-256 and reuses `.cache/downloads/`.
- `scripts/fetch-server-runtime.sh`: stages ReHLDS `3.15.0.896`, ReGameDLL_CS
  `5.30.0.814`, Metamod-R `1.3.0.149`, the AMXX cstrike package and ReAPI, and
  reports the pinned SteamCMD image digest.
- `scripts/build-server.sh --stage`: assembles the Docker build context with
  runtime archives, module, compiled test plugin, test assets and a hash
  manifest.
- `server/pack/`: multi-stage image (SteamCMD/App 90, Steam ABI stub, runtime),
  Docker Compose file, entrypoint, RCON healthcheck, Metamod and AMXX
  configuration.
- `server/pack/steamcompat/steamstub.cpp`: no-op `ISteamGameServer011`
  implementation for ReHLDS, which was built against the pre-25th-anniversary
  Steamworks ABI.
- `tests/e2e/run.sh` and `tests/e2e/test_server.py`: reproducible test server
  with log and RCON checks, clean `down --volumes` on every exit path.
- Bilingual test server guide: `docs/de/server/testserver.md` and
  `docs/en/server/testserver.md`.
- `scripts/doctor.sh` reports Docker and Compose as optional prerequisites.

### Changed

- Root `.gitattributes` no longer reinterprets vendored binaries through the LFS
  rules; each vendored tree keeps its own attribute policy.
- Vendored NextClient moved from `90b4bf7` to `24cc986` (G06b radar): Linux
  process-map parsing no longer truncates module paths that contain spaces.
  Imported with full history via `git subtree pull`; `versions.lock.json` updated.
- `scripts/fetch-server-toolchain.sh` uses the shared `scripts/lib/fetch.sh`
  download helper.

### Fixed

- LFS objects for the vendored client are imported, so a fresh clone resolves all
  790 binary assets.
- The test plugin's custom models, sounds, sprites and material preset are
  staged into the image; a missing model aborted the map load.
- The test server keeps `modules.ini` empty so AMXX detects modules itself
  instead of loading each one twice through Metamod.

## [0.1.0-dev] - 2026-11-08

First tracked state. Not released, not installable.
