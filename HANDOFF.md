# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-08
- **Gate:** G02 – vendor import, **done**
- **Branch:** `main`
- **Commit:** `git log -1`
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** recorded after the G02 push

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| NextClient is vendored with history | subtree merge `3fea0c3`, pinned tree `90b4bf76ee4ff1ea3afb2c823feb2137bd65034e` |
| NextClientServerApi is vendored with history | subtree merge `501d93b`, pinned tree `1da463e0548c3bdb69e441caf2a39caae31edffa` |
| Upstream history is present | `git rev-list --count HEAD` → 512 commits |
| Submodules are pinned and checked out | `git submodule status` → `0e9be00` and `b5d1a94`, both clean |
| Lockfile matches the checked-in gitlinks | `tools/verify_repo.py`, check `check_pins` |
| Binary assets are imported | `git lfs ls-files` → 790 files, 130 MB in `.git/lfs/objects` |
| Repository checks pass | `scripts/verify.sh` → `OK: 52 checks passed on 3980 tracked files` |
| Push gate is active | `git config core.hooksPath` → `.githooks` |

## Open / known broken

- No build, no tests, no installer yet. G03 and G04 follow.
- License of the project is undecided. The repository stays private.
- **Branch protection is not available**: GitHub rejects it for a private
  repository on the free plan. This is now mitigated by the tracked `pre-push`
  hook, which runs the same verification as CI. CI remains the second check.
- The vendored NextClient still contains upstream Sentry, GameAnalytics and
  updater references. They are removed in G08, not before, so the baseline stays
  comparable.

## Next task

- **Gate:** G03 – toolchain on CachyOS
- **Acceptance:**
  - `scripts/doctor.sh` checks OS, glibc, GCC, CMake, Ninja, Python, `uv`,
    `git-lfs`, a `-m32` test compile, 32-bit GL/X11/Xext/Xcursor/Xi/Xrandr/Xss/
    Xfixes/Xrender/SDL2, Mesa/llvmpipe, Steam runtime and free disk space.
  - `scripts/bootstrap.sh` installs missing Arch packages and bootstraps vcpkg,
    idempotently.
  - `docs/de/build/prerequisites.md` and `docs/en/build/prerequisites.md` exist
    and match the checks.

## Open decisions

1. Windows support: drop the CEF/browser code or keep it disabled. Decided in G08.
2. Discord app, Sentry project, GameAnalytics keys, update signing key and
   hosting are needed for G12/G13.
3. Host build versus container build for vcpkg on Arch. Decided in G04.

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
$ git subtree add --prefix=client https://github.com/CS-NextClient/NextClient.git 90b4bf7
Added dir 'client'

$ git subtree add --prefix=server/api https://github.com/CS-NextClient/NextClientServerApi.git 1da463e
Added dir 'server/api'

$ git submodule status
 0e9be00fc52b66742a09c84c8f6169dc7c83ca8a client/dep/ncl-hl1-source-sdk (heads/main)
 b5d1a94fb7f88fd835e360fd23a45a09ceedbf48 client/vcpkg (b5d1a94f)

$ scripts/verify.sh
OK: 52 checks passed on 3980 tracked files
Verification passed
```

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | done | subtree `3fea0c3`, `501d93b`, verify green |
| G03 | next | – |
