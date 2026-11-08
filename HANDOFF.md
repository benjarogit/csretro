# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-08
- **Gate:** G04 – client build on Linux x86, **done**
- **Branch:** `main`
- **Commit:** `git log -1`
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** recorded after the G04 push

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| Configure succeeds | `cmake --preset linux` exit 0, 121.8 s, log `/tmp/opencode/g04-configure2.log` |
| All targets build | `client_mini`, `engine_mini`, `nitro_api`, `gameui`, `cstrike.exe`, `BUILD_ALL` all exit 0 |
| Every artifact is 32-bit | 8 artifacts, all `ELF32` via `readelf` |
| `gameui.so` and `vgui2.so` are built | `out/bin/Release/cstrike/cl_dlls/gameui.so`, `out/bin/Release/vgui2.so` |
| Manifest is generated | `client/build/linux/manifest-Release.txt`, size + SHA-256 + ELF class |
| Build script is idempotent | second `scripts/build-client.sh` run → `ninja: no work to do`, exit 0 |
| Repository checks pass | `scripts/verify.sh` green |

Reference artifacts (Release, 2026-11-08):

| Artifact | Size | Type |
|---|---:|---|
| `client_mini.dll` | 5,482,420 | ELF32 |
| `next_engine_mini.dll` | 26,130,800 | ELF32 |
| `nitro_api2.dll` | 15,000,184 | ELF32 |
| `cstrike/cl_dlls/gameui.so` | 8,785,140 | ELF32 |
| `cstrike.exe` | 19,974,812 | ELF32 |
| `FileSystem_Proxy.dll` | 2,117,508 | ELF32 |
| `steam_api.dll` | 1,748,084 | ELF32 |
| `vgui2.so` | 2,597,024 | ELF32 |

Build times: configure 122 s, `client_mini` 32 s, `engine_mini` 56 s,
`nitro_api` 104 s, `gameui` 18 s, `cstrike.exe` 9 s, `BUILD_ALL` 3 s.
Disk: `client/build/linux` about 282 MB, `client/vcpkg/downloads` about 90 MB.

## Open / known broken

- The client is not installed and not run yet. G07/G09 follow.
- **Path with spaces:** the repository path contains spaces. OpenSSL's `Configure`
  fails there, so `scripts/build-client.sh` builds through `/tmp/csretro-src`.
  The same constraint applies to every future build job; keep using the script.
- The local `client/vcpkg` submodule was a shallow clone and missed the manifest
  baseline. It was unshallowed. `bootstrap.sh` clones fully, so fresh clones are
  not affected.
- Upstream warnings remain: `DT_TEXTREL`, CMake deprecation in the vendored SDK,
  `FetchContent_Populate`. None change the output.
- The vendored NextClient still contains upstream Sentry, GameAnalytics and
  updater references. Removed in G08.
- Branch protection is unavailable on a private free-plan repository. Mitigated
  by the tracked `pre-push` hook.

## Next task

- **Gate:** G05 – server API build
- **Acceptance:**
  - `server/api` configures with CMake 3.21+ and builds under CMake 4.
  - `nextclientapi_amxx` is a 32-bit `.so` with correct module metadata.
  - The complete CTest suite is green.
  - A Pawn test plugin compiles with the AMXX compiler.
  - `scripts/build-server.sh` exists and is idempotent.

## Open decisions

1. Windows support: drop the CEF/browser code or keep it disabled. Decided in G08.
2. Discord app, Sentry project, GameAnalytics keys, update signing key and
   hosting are needed for G12/G13.
3. ~~Host build versus container build for vcpkg on Arch~~ — **decided in G04:
   host build works.** No container needed for the client. Revisit only if a
   future dependency breaks on Arch.

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

The retail game lives in the maintainer's Steam library. It is **not** part of the
repository and must never be committed. CSRETRO is always installed into a separate
copy outside the Steam library.

## Last commands and results

```text
$ scripts/doctor.sh
40 passed, 0 warnings, 0 failed

$ scripts/build-client.sh
==> configure (preset linux)
-- Configuring done (121.8s)
==> build client_mini (Release)   [32 s]
==> build engine_mini (Release)   [56 s]
==> build nitro_api (Release)     [104 s]
==> build gameui (Release)        [18 s]
==> build cstrike.exe (Release)   [9 s]
==> build BUILD_ALL (Release)     [3 s]
manifest: client/build/linux/manifest-Release.txt
```

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | done | subtree `3fea0c3`, `501d93b`, verify green |
| G03 | done | doctor 40/0, vcpkg bootstrapped, prerequisites documented |
| G04 | done | 8 ELF32 artifacts, manifest, build script |
| G05 | next | – |
