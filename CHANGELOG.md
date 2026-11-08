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

### Changed

- Root `.gitattributes` no longer reinterprets vendored binaries through the LFS
  rules; each vendored tree keeps its own attribute policy.

### Fixed

- LFS objects for the vendored client are imported, so a fresh clone resolves all
  790 binary assets.

## [0.1.0-dev] - 2026-11-08

First tracked state. Not released, not installable.
