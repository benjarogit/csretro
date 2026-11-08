# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-08
- **Gate:** G03 – toolchain on CachyOS, **done**
- **Branch:** `main`
- **Commit:** `b1ee521` (G03 code), handoff commit follows
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** https://github.com/benjarogit/csretro/actions/runs/42481361534 – success

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| Host check passes | `scripts/doctor.sh` → `40 passed, 0 warnings, 0 failed`, exit 0 |
| 32-bit C and C++ build works | doctor probes with `-m32 -march=pentium4`, probe binary is `ELF32` |
| 32-bit GL and X11 link | doctor resolves `libGL`, `X11`, `Xext`, `Xcursor`, `Xi`, `Xrandr`, `Xinerama`, `Xss`, `Xfixes`, `Xrender` |
| Offscreen rendering is possible | 32-bit Mesa swrast at `/usr/lib32/dri/swrast_dri.so` |
| vcpkg is bootstrapped | `client/vcpkg/vcpkg version` → `2026-03-04-4b3e4c2...` |
| Bootstrap is idempotent | second `scripts/bootstrap.sh` run exits 0 |
| Repository checks pass | `scripts/verify.sh` → 52+ checks green |
| Prerequisites are documented | `docs/de/build/prerequisites.md`, `docs/en/build/prerequisites.md` |

## Open / known broken

- No build yet. G04 is the first real compile of the client.
- License of the project is undecided. The repository stays private.
- Branch protection is unavailable on a private free-plan repository. Mitigated
  by the tracked `pre-push` hook, which runs the same verification as CI.
- The vendored NextClient still contains upstream Sentry, GameAnalytics and
  updater references. Removed in G08.
- Upstream quirk: `client/dep/cpr/nuget/resources/cpr.png` is a plain blob although
  the upstream `.gitattributes` declares `*.png` as LFS. One warning per checkout,
  harmless.

## Next task

- **Gate:** G04 – client build on Linux x86
- **Acceptance:**
  - `cmake --preset linux` configures in `client/`.
  - Targets build in order: `client_mini`, `engine_mini`, `nitro_api`, `gameui`,
    `launcher`, then `BUILD_ALL`.
  - `vgui2` and `gameui.so` are part of the build.
  - Every artifact is a 32-bit ELF.
  - A hash manifest, build time and peak disk usage are recorded.
  - Arch/CMake 4 patches live in `patches/` with a written reason.

## Open decisions

1. Windows support: drop the CEF/browser code or keep it disabled. Decided in G08.
2. Discord app, Sentry project, GameAnalytics keys, update signing key and
   hosting are needed for G12/G13.
3. Host build versus container build for vcpkg on Arch. **Decided in G04** based
   on the actual configure result.

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

The retail game lives in the maintainer's Steam library. It is **not** part of the
repository and must never be committed. CSRETRO is always installed into a separate
copy outside the Steam library.

## Last commands and results

```text
$ scripts/doctor.sh
39 passed, 1 warnings, 0 failed
All build prerequisites are met.

$ client/vcpkg/bootstrap-vcpkg.sh -disableMetrics
vcpkg package management program version 2026-03-04-4b3e4c276b5b87a649e66341e11553e8c577459c

$ scripts/bootstrap.sh
==> git hooks
==> git lfs
==> submodules
==> vcpkg
==> lfs objects
==> verification
Verification passed
```

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | done | subtree `3fea0c3`, `501d93b`, verify green |
| G03 | done | doctor 39/0, vcpkg bootstrapped, prerequisites documented |
| G04 | next | – |
