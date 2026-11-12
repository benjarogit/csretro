# CSRETRO

Counter-Strike 1.6, rebuilt as its own thing.

The client is **not playable yet**. You need a retail Counter-Strike 1.6 install
for original content when it is. Never put CSRETRO into the Steam game folder
and never join a VAC-secured server with a modified client.

CSRETRO is a fully vendored CS 1.6 client plus an AMXX server API. It targets the
**Default Public branch** of the retail game, speaks its own protocol, carries its
own identity and infrastructure, and is documented in German and English.

> Status: early development (`1.0.0-dev`). The product process is Linux **x86_64**.
> Gates G01–G05 are complete. G07 (Default Public runtime on Xash3D) is in progress.

## What this is

| Part | Description |
|---|---|
| `client/` | Vendored NextClient client with full history, rebranded from G08 |
| `server/api/` | Vendored NextClientServerApi with full history, AMXX module |
| `docs/` | Bilingual documentation, the single source for the GitHub Page |
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
- Windows is not a target. The product process is Linux x86_64.

## Requirements

See [`docs/en/build/prerequisites.md`](docs/en/build/prerequisites.md) for the
full list, or run `scripts/doctor.sh` to check the host. The short version:
Linux x86_64, multilib, GCC, CMake, Ninja, Python, Git LFS.

## Documentation

Published site (built from `docs/de` and `docs/en`):
https://benjarogit.github.io/csretro/

- [Project plan](docs/plan/csretro-plan.md) (German, authoritative)
- [Documentation index, English](docs/en/index.md)
- [Dokumentationsindex, Deutsch](docs/de/index.md)
- [Credits](CREDITS.md)
- [Security policy](SECURITY.md)

## Working rules

The harness contract is in [`AGENTS.md`](AGENTS.md). Every task is implemented,
verified, documented, committed and pushed before the next one starts.
Collaboration is Issues and pull requests. GitHub Actions only run commands.

## License

No license has been chosen yet for the CSRETRO glue. The repository is public.
Vendored components keep their upstream licenses; see [`CREDITS.md`](CREDITS.md)
and the license files inside the vendored trees. Without a glue license, outside
patches are not accepted.
