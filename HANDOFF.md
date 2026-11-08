# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-08
- **Gate:** G01 – repository and harness, **done**
- **Branch:** `main`
- **Commit:** `b50197c` (harness), follow-up commit for this handoff
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** https://github.com/benjarogit/csretro/actions/runs/42479518181 – success

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| Repository exists with remote `origin` | `git remote -v` → `https://github.com/benjarogit/csretro.git` |
| Harness rules are defined | `AGENTS.md`, 13 sections |
| Plan is part of the repository | `docs/plan/csretro-plan.md`, 708 lines |
| Documentation is bilingual | `docs/de/index.md`, `docs/en/index.md`, parity enforced by tool |
| Repository checks pass locally | `scripts/verify.sh` → `OK: 50 checks passed on 20 tracked files` |
| Repository checks pass in CI | run `42479518181`, job `repository verification`, success |
| Upstream pins and game hashes are recorded | `versions.lock.json` |
| Git LFS is active | `git lfs install --local` |
| No maintainer-local paths in tracked files | `grep -rn '/mnt/\|/home/benny'` → no matches |

## Open / known broken

- `client/` and `server/api/` do not exist yet. They are imported in G02.
- No build, no tests, no installer yet.
- License of the project is undecided. The repository stays private.
- **Branch protection is not available**: GitHub rejects it for a private
  repository on the free plan (`Upgrade to GitHub Pro or make this repository
  public`). CI is required by convention until the repository is public or the
  plan changes. Revisit at G10.

## Next task

- **Gate:** G02 – vendor import
- **Acceptance:**
  - `client/` contains NextClient at `90b4bf76ee4ff1ea3afb2c823feb2137bd65034e`
    with history.
  - `server/api/` contains NextClientServerApi at
    `1da463e0548c3bdb69e441caf2a39caae31edffa` with history.
  - Submodules `client/vcpkg` and `client/dep/ncl-hl1-source-sdk` are pinned.
  - `versions.lock.json` has no `null` commits left.
  - A fresh clone plus bootstrap reproduces the same hashes.

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
$ git init -b main && git remote add origin https://github.com/benjarogit/csretro.git
$ scripts/verify.sh
OK: 50 checks passed on 20 tracked files
Verification passed

$ git push -u origin main
 * [new branch]      main -> main

$ gh run list --limit 1
completed success chore(repo): bootstrap harness and documentation skeleton ci main push 42479518181 15s
```

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | next | – |
