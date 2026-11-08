# CSRETRO

Counter-Strike 1.6, rebuilt as its own thing.

CSRETRO is a fully vendored CS 1.6 client plus an AMXX server API. It targets the
**Default Public branch** of the retail game, speaks its own protocol, carries its
own identity and infrastructure, and is documented in German and English.

> Status: early development (`0.1.0-dev`). Gates G01–G05 are complete: harness,
> vendored upstreams, toolchain, a full 32-bit client build and a tested 32-bit
> server module. Nothing here is installable yet.

## What this is

| Part | Description |
|---|---|
| `client/` | Vendored NextClient client with full history, rebranded from G08 |
| `server/api/` | Vendored NextClientServerApi with full history, AMXX module |
| `docs/` | Bilingual documentation, the single source for the GitHub wiki |
| `scripts/` | Build, install, verify and clean entry points |
| `tools/` | Repository checks and generators |

## First clone

```bash
git clone https://github.com/benjarogit/csretro.git
cd csretro
./scripts/bootstrap.sh
```

`bootstrap.sh` installs the tracked push hook, initialises Git LFS, checks out the
pinned submodules and runs the verification.

## Non-goals

- No `steam_legacy` branch and no 8684 fallback. Default Public only.
- No backward compatibility with NextClient. CSRETRO servers accept CSRETRO clients.
- No vendoring of GPL reference projects. Ideas are credited, code is not copied.
- Windows is not a target. Linux x86 (32-bit) inside a 64-bit host is.

## Requirements

See [`docs/en/build/prerequisites.md`](docs/en/build/prerequisites.md) for the
full list, or run `scripts/doctor.sh` to check the host. The short version:
Linux x86_64, multilib, GCC, CMake, Ninja, Python, Git LFS.

## Documentation

- [Project plan](docs/plan/csretro-plan.md) (German, authoritative)
- [Documentation index, English](docs/en/index.md)
- [Dokumentationsindex, Deutsch](docs/de/index.md)
- [Credits](CREDITS.md)

## Working rules

The harness contract is in [`AGENTS.md`](AGENTS.md). Every task is implemented,
verified, documented, committed and pushed before the next one starts.

## License

No license has been chosen yet. The repository is private. Vendored components keep
their upstream licenses; see [`CREDITS.md`](CREDITS.md) and the license files inside
the vendored trees.
