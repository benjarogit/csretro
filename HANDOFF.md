# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-08
- **Gate:** G05 – server API build, **done**
- **Branch:** `main`
- **Commit:** `9f3af81` (G05 code), handoff commit follows
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** https://github.com/benjarogit/csretro/actions/runs/42483231438 – success

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| Configure succeeds under CMake 4.4.5 | `scripts/build-server.sh --clean` exit 0, log `/tmp/opencode/g05-cleanbuild2.log` |
| Project minimum is CMake 3.21 | `server/api/CMakeLists.txt` first line |
| Module is 32-bit | `nextclientapi_amxx_i386.so` → `ELF 32-bit LSB shared object, Intel i386` |
| Module metadata is correct | script reads name/version/tag from `CMakeLists.txt` and finds them in the binary: `Next Client API 1.6.0 (nclapi)` |
| Full CTest suite is green | `100% tests passed out of 34` |
| Pawn plugin compiles | test `pawn-material-plugin` passes, output `test_nextclient_api.amxx` |
| Build script is idempotent | second `scripts/build-server.sh` run → `ninja: no work to do`, exit 0 |
| Toolchain downloads are verified | AMXX and ReAPI SHA-256 match the digests in `versions.lock.json` |
| Repository checks pass | `scripts/verify.sh` green |

Reference artifacts (Release, 2026-11-08):

| Artifact | Size | Type |
|---|---:|---|
| `nextclientapi_amxx_i386.so` | 8,863,316 | ELF32 |
| `deps/rsa_keygen/rsa_keygen` | 4,437,032 | ELF32 |
| `material-entity-sync-tests` | 672,760 | ELF64 |
| `entity-sync-packets-tests` | 136,040 | ELF64 |
| `material-visibility-tests` | 22,944 | ELF64 |

Build times: configure 8 s, build 17 s, CTest about 1 s, clean rebuild 27 s.
The module needs only `libm`, `libc` and `ld-linux.so.2`; libstdc++ and libgcc
are linked statically.

## Open / known broken

- The module is not loaded by a real server yet. G06 builds the test server.
- **Upstream patches:** `server/api/deps/rsa_keygen/` needed two portability
  fixes (ISO C++ `int main`, `-m32` for the 32-bit OpenSSL archives). They are
  applied and recorded in `patches/README.md`.
- **Network at configure time:** `server/api/cmake/Dependencies.cmake` fetches
  `kangaru`, `sigslot` and `magic_enum` from GitHub. First configure needs
  network; later ones reuse `_deps`.
- **Path with spaces:** all builds go through `/tmp/csretro-src`. Keep using the
  scripts; do not move `build/` between the real path and the symlink.
- Upstream warnings remain: CMake deprecation in `kangaru`/`sigslot`,
  `DT_TEXTREL` when linking `rsa_keygen`. None change the output.
- The vendored NextClient and server API still carry upstream Sentry,
  GameAnalytics, updater and identity strings. Removed in G08.
- Branch protection is unavailable on a private free-plan repository. Mitigated
  by the tracked `pre-push` hook.

## Next task

- **Gate:** G06 – reproducible test server
- **Acceptance:**
  - Docker Compose with ReHLDS + ReGameDLL_CS + AMXX 1.10 + ReAPI + the module
    and the test plugin.
  - Healthcheck, logs and defined start/stop.
  - HLDS base files (App 90) via SteamCMD or a maintained ReHLDS image.
  - `tests/e2e/` skeleton.
  - The server starts reproducibly, the module loads, the test plugin reports in,
    and an automated check proves it.
- **Risk:** SteamCMD availability for App 90. Fallback is a maintained ReHLDS
  image. The game is never started automatically on the host; the server runs in
  a container only.

## Open decisions

1. Windows support: drop the CEF/browser code or keep it disabled. Decided in G08.
2. Discord app, Sentry project, GameAnalytics keys, update signing key and
   hosting are needed for G12/G13.
3. ~~Host build versus container build for vcpkg on Arch~~ — **decided in G04:
   host build works.**
4. G06 base image: SteamCMD download at build time or a prebuilt ReHLDS image.
   Decide from what is reproducible inside CI.

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
| Multilib | works (`-m32`) |
| Steam | installed, Default Public 1.1.2.7 |
| Build symlink | `/tmp/csretro-src` → repository |
| Server toolchain | AMXX `1.10.0.5487`, ReAPI `5.29.0.358` in `.cache/toolchain/` |

The retail game lives in the maintainer's Steam library. It is **not** part of the
repository and must never be committed. CSRETRO is always installed into a separate
copy outside the Steam library.

## Last commands and results

```text
$ scripts/build-server.sh --clean
==> removing server/api/build/linux
==> fetching Pawn toolchain
==> amxmodx: cached amxmodx-1.10.0-git5487-base-linux.tar.gz
==> reapi: cached reapi-bin-5.29.0.358.zip
==> configure
==> build (Release)
==> ctest
100% tests passed out of 34
==> artifacts
module: Next Client API 1.6.0 (nclapi)
build time: 18 s
```

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | done | subtree `3fea0c3`, `501d93b`, verify green |
| G03 | done | doctor 40/0, vcpkg bootstrapped, prerequisites documented |
| G04 | done | 8 ELF32 artifacts, manifest, build script, CI run `42482462774` |
| G05 | done | module ELF32 + metadata, 34/34 tests, build script, `patches/` |
| G06 | next | – |
