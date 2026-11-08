# AGENTS.md – CSRETRO harness contract

This file is the binding contract for every agent and every human working on
CSRETRO. If a task conflicts with this file, this file wins. Changes to it are
reviewed like code.

---

## 1. Purpose and non-goals

CSRETRO is a fully vendored Counter-Strike 1.6 client plus an AMXX server API.

- Target is the **Default Public branch** of the retail game only.
- No `steam_legacy`, no 8684 fallback, no legacy address providers.
- No backward compatibility with NextClient. CSRETRO servers accept CSRETRO clients.
- Linux x86 (32-bit) inside a 64-bit host. Windows is not a target.
- Reference projects under GPL are read for ideas and procedures, never vendored.

The authoritative plan is [`docs/plan/csretro-plan.md`](docs/plan/csretro-plan.md).
Current progress, evidence and the next task live in [`HANDOFF.md`](HANDOFF.md).

---

## 2. Maintainer and project separation

Everything that belongs to CSRETRO goes into this repository. Everything that
belongs to the maintainer stays local and never enters the repository.

| Category | Location | In repository |
|---|---|---|
| Plan, gates, architecture, docs | `docs/` | yes |
| Harness rules | `AGENTS.md` | yes |
| Credits, changelog, version | repository root | yes |
| Project state (gate, commit, CI) | `HANDOFF.md` | yes |
| Maintainer notes, local paths, Steam library, personal decisions | `.local/` | **no** |
| Secrets, `.env`, logs, caches | `.local/`, build dirs | **no** |

`HANDOFF.md` contains only project facts: state, evidence, next task, open
decisions. Personal context goes to `.local/handoff.local.md`, which is ignored.

Never commit absolute paths of the maintainer's machine into project files.
Use repository-relative paths.

---

## 3. Work cycle

One task at a time. A gate is only started when the previous gate is closed.

```
task -> implement -> test -> docs (DE+EN) -> verify -> commit -> push -> CI -> handoff
```

Rules:

1. Read `HANDOFF.md` before starting.
2. Read the relevant section of the plan.
3. Implement the smallest complete change.
4. Run the tests that belong to the change, not only the ones that are convenient.
5. Update documentation in the same commit as the behaviour.
6. Run `scripts/verify.sh`.
7. Commit with a conventional commit message.
8. Push. The tracked `pre-push` hook runs `scripts/verify.sh` again and blocks the
   push on failure. Never bypass it with `--no-verify` unless the maintainer
   explicitly asks; if it is bypassed, CI must still be green before the task
   continues.
9. Wait for CI, fix red CI before anything else.
10. Update `HANDOFF.md` with state, evidence and the next task.
11. Clean temporary files, logs and caches before closing the gate.

Never leave the repository in a state where `main` is red or undocumented.

---

## 4. Definition of Done

A gate is done only when all of the following are proven:

- [ ] Build is green and every related test passes.
- [ ] `scripts/verify.sh` passes.
- [ ] Documentation updated in German and English, drift check green.
- [ ] `HANDOFF.md` updated: state, evidence, next task.
- [ ] Conventional commit created and pushed.
- [ ] CI run is green; the link is recorded in `HANDOFF.md`.
- [ ] No temporary, log, cache or dead files in the tree.
- [ ] No secrets, no Steam files, no maintainer-local data.

"Verified" means a log, a test output, a hash manifest or a CI link. A claim
without evidence is not verification.

---

## 5. Branches and commits

- One branch per gate: `gate/G04-client-build`.
- Pull request against `main`, squash merge, CI required.
- `main` is always releasable in the sense that it builds and verifies.

Enforcement: GitHub branch protection is not available on a private free-plan
repository. The repository therefore ships a tracked `pre-push` hook in
`.githooks/` that runs `scripts/verify.sh`. `scripts/bootstrap.sh` sets
`core.hooksPath` on every clone, so the gate is local and does not depend on the
GitHub plan. CI remains the second, independent check.

Commit format:

```
<type>(<scope>): <imperative summary>
```

Types: `feat`, `fix`, `build`, `ci`, `docs`, `test`, `refactor`, `perf`, `chore`.
Scopes: `client`, `server`, `api`, `protocol`, `docs`, `ci`, `scripts`, `deps`, `repo`.

Examples:

```
feat(client): add public build address provider
fix(protocol): reject mismatched protocol version
build(deps): bump vcpkg baseline
ci(linux): add arch container job
docs(wiki): add cvar reference
test(server): cover handshake timeout
chore(repo): remove legacy 8684 providers
```

Rules:

- Subject in imperative mood, English, no trailing period, max 72 characters.
- Body explains **why**, not what. The diff shows what.
- One logical change per commit. No mixed refactor plus feature.
- Never commit generated build output.

---

## 6. Single Source of Truth (SSoT)

Every piece of information has exactly one authoritative location. Everything
else is derived or linked, never copied.

| Information | Authoritative source | Derived / linked |
|---|---|---|
| Goals, decisions, gates | `docs/plan/csretro-plan.md` | wiki links |
| Current state, evidence | `HANDOFF.md` | – |
| Cvar defaults | code (`cvar_defaults.h`, registration) | wiki tables (generated) |
| Commands and natives | code + `csretro_api.inc` | wiki tables (generated) |
| Protocol | one shared `csr_proto.h` | client and server include it |
| Version | `VERSION` | CMake, launcher, docs |
| Upstream pins | `versions.lock.json` | CI, docs |
| Credits | `CREDITS.md` | wiki links |
| Build flags | CMake presets | scripts call presets only |
| Human docs | `docs/de`, `docs/en` | GitHub wiki (generated mirror) |

Redundancy is a defect. Concretely:

1. **Reference projects are not vendored.** Only NextClient, NextClientServerApi,
   ncl-hl1-source-sdk and vcpkg are vendored. FuryBaM/cs16-client, FWGS/cs16-client
   and HLSW are references with credits.
2. **No duplicated cvar defaults.** UI code reads the shared registration, never a
   second literal table.
3. **One address provider.** A generic provider plus a build registry, not one copy
   per game build.
4. **Upstream documentation is not translated.** It stays in the vendored tree as
   technical reference. Our `docs/` is authoritative.
5. **No copy-pasted version strings.** Read `VERSION`.
6. **No copy-pasted credit lists.** Link to `CREDITS.md`.

CI gates against drift:

- `tools/verify_repo.py` – structure, docs parity, links, secrets, forbidden files.
- `tools/docs_drift.py` (G11) – code ↔ documentation.
- `tools/proto_sync.py` (G08) – client ↔ server protocol header.
- grep gate (G08) – no leftover `nextclient`, `ncl_`, `8684` outside credits.

---

## 7. Tests

| Level | Tool | Runs |
|---|---|---|
| Client unit | CTest | local + CI |
| Offscreen GL | CTest + llvmpipe | local + CI |
| Server unit | CTest + fakes | local + CI |
| Pawn plugin | AMXX compiler | local + CI |
| Python tools | pytest via `uv` | local + CI |
| Shell and install | shellcheck + script tests | local + CI |
| End-to-end | Docker + protocol client | local + CI |
| Documentation | drift check + link check | local + CI |

Rules:

- The game is **never started automatically**. Automation stops at the process
  boundary. A human runs the game and records the result in `HANDOFF.md`.
- A test without a reproducible failure case is not a test.
- Fix the cause, not the assertion. Never delete a failing test to get green.
- Every bug fix gets a regression test, or a documented reason why none is possible.

---

## 8. Documentation

- German and English are both mandatory for human documentation.
- `docs/de/` and `docs/en/` mirror each other file by file. `verify_repo.py`
  fails on a missing counterpart.
- The GitHub wiki is generated from `docs/` by CI. Never edit the wiki by hand.
- Code comments, commit messages and identifiers are English.
- Documentation changes are part of the same commit as the behaviour they describe.
- Generated tables (cvars, commands, natives) are never maintained by hand.

---

## 9. Cleanup

Ignored and removed after every gate:

```
build/  out/  install/  dist/  .cache/  logs  .env
CMakeUserPresets.json  vcpkg_installed/  __pycache__/
```

- `scripts/clean.sh` is the only sanctioned way to remove generated state.
- No dead code, no commented-out blocks, no unused assets, no stale patches.
- No temporary directories inside the repository. Use `/tmp/opencode/`.

---

## 10. Security

- **VAC:** Never install CSRETRO into the Steam game directory. Always use a
  separate copy outside the Steam library. Never connect to VAC-secured servers
  with a modified client.
- **Secrets:** Never commit tokens, keys, DSNs or webhook URLs. Use `.env` locally
  and GitHub Secrets in CI. `.env.example` documents the names only.
- **Downloads:** Verify SHA-256 of every external artifact before use.
- **Upstream:** Do not add code from a repository without a compatible license.
  GPL reference projects are read-only sources of ideas.

---

## 11. Environment

| Item | Value |
|---|---|
| Host | Linux x86_64 |
| Target | 32-bit x86 (`-m32`) |
| Reference OS | CachyOS / Arch, multilib enabled |
| Build system | CMake presets + Ninja |
| Python | 3.12 or newer, `uv` for tool environments |
| Shell | bash, scripts pass `shellcheck` |
| Version control | git, Git LFS required |

Scripts must work from a clean shell and from CI. No reliance on the caller's
current directory: resolve the repository root from the script location.

---

## 12. Language

| Artifact | Language |
|---|---|
| Code, identifiers, comments | English |
| Commit messages | English |
| `AGENTS.md`, `HANDOFF.md` | English |
| `README.md` | English |
| `README.de.md` | German |
| `docs/en/**` | English |
| `docs/de/**` | German |
| `docs/plan/**` | German (authoritative plan) |
| `CREDITS.md` | English |

---

## 13. Gates

The gate list is defined once in the plan, section 7. Do not duplicate it here.
Progress is tracked once in `HANDOFF.md`. Do not duplicate it here.
