# Building the server API

The server API is an AMX Mod X module. ReHLDS loads it through Metamod, and
plugins use its natives through `next_client_api.inc`. It is a **32-bit i386
shared object**, like every GoldSrc server module.

Prerequisites: [build prerequisites](prerequisites.md).

## Quick start

```bash
scripts/doctor.sh          # host check, including -m32
scripts/build-server.sh    # fetch toolchain, configure, build, test, manifest
```

The script downloads the Pawn toolchain on first use, configures with tests
enabled, builds, runs CTest and writes a manifest. A second run is a no-op.

## Pawn toolchain

Two upstream packages are needed and neither is vendored:

| Package | Version | Purpose |
|---|---|---|
| AMX Mod X base | `1.10.0.5487` | `amxxpc` compiler and the standard includes |
| ReAPI | `5.29.0.358` | `reapi.inc` and the ReAPI module |

Both URLs and their SHA-256 digests are pinned in `versions.lock.json` under
`server_toolchain`. `scripts/fetch-server-toolchain.sh` downloads them into
`.cache/downloads/`, verifies every digest, unpacks them into
`.cache/toolchain/` and merges the two include directories into one.

`amxxpc` `dlopen()`s `amxxpc32.so` by name, so it only runs when its own
directory is on `LD_LIBRARY_PATH`. The script generates a small wrapper that
sets it; callers use the wrapper like a normal compiler:

```bash
eval "$(scripts/fetch-server-toolchain.sh)"
"$AMXXPC" plugin.sma -i"$PAWN_INCLUDE_DIR" -oplugin.amxx
```

`.cache/` is ignored by git. `scripts/clean.sh` removes it.

## Configuration

`scripts/build-server.sh` calls CMake directly; there is no preset because the
module has a single supported configuration.

| Setting | Value |
|---|---|
| Minimum CMake | 3.21 |
| C++ standard | C++17 |
| Target ABI | `-m32`, suffix `_i386.so` |
| Linking | `-static-libstdc++ -static-libgcc` |
| Build directory | `server/api/build/linux` |

| Option | Default | Meaning |
|---|---|---|
| `NEXTCLIENTAPI_BUILD_TESTS` | `OFF` | build and register the test suite |
| `NEXTCLIENTAPI_PAWN_COMPILER` | empty | `amxxpc` wrapper |
| `NEXTCLIENTAPI_PAWN_INCLUDE_DIR` | empty | merged AMXX + ReAPI includes |

The build script sets all three when tests are enabled. Setting only one of the
two Pawn variables is a hard configure error.

Dependencies `kangaru` (v4.3.2), `sigslot` (v1.2.0) and `magic_enum` (v0.9.7) are
fetched by `server/api/cmake/Dependencies.cmake` at configure time and pinned by
tag or commit. The first configure therefore needs network access.

## Targets and artifacts

| Target | Artifact | Type | Purpose |
|---|---|---|---|
| `nextclientapi_amxx` | `nextclientapi_amxx_i386.so` | ELF32 | the module |
| `rsa_keygen` | `deps/rsa_keygen/rsa_keygen` | ELF32 | development key generator |
| `material-visibility-tests` | host binary | ELF64 | native policy tests |
| `entity-sync-packets-tests` | host binary | ELF64 | native packet tests |
| `material-entity-sync-tests` | host binary | ELF64 | native scenario tests |

The tests are host programs: they exercise engine-free policy code and are
compiled for the host ABI. Only the module and `rsa_keygen` must be 32-bit, and
the script fails if the module is not `ELF32`.

## Tests

`scripts/build-server.sh` runs the full suite. Reference run: **34/34 passed in
about one second**.

| Group | Count | What it covers |
|---|---|---|
| `pawn-material-plugin` | 1 | compiles `amxx_test/.../test_nextclient_api.sma` with `amxxpc` and fails on any warning |
| `material-visibility-policy` | 1 | who may see a material |
| `entity-sync-packets` | 1 | packet layout |
| `material-*` scenarios | 31 | binding, admission, ownership, reuse and unbind rules |

The Pawn test is skipped unless both Pawn variables are set; the build script
sets them.

## Module metadata

The metadata comes from `server/api/CMakeLists.txt` and ends up in the binary:

| Field | Value |
|---|---|
| Name | `Next Client API` |
| Version | `1.6.0` (from `project(... VERSION ...)`) |
| Log tag | `nclapi` |

`scripts/build-server.sh` reads the expected values from that file and fails if
the built module does not contain them. It also records the `NEEDED` libraries
in the manifest; the module links only `libm`, `libc` and the loader, because
libstdc++ and libgcc are static.

## Reference build (G05, 2026-11-08, Release)

| Artifact | Size | Type |
|---|---:|---|
| `nextclientapi_amxx_i386.so` | 8,863,316 | ELF32 |
| `deps/rsa_keygen/rsa_keygen` | 4,437,032 | ELF32 |
| `material-entity-sync-tests` | 672,760 | ELF64 |
| `entity-sync-packets-tests` | 136,040 | ELF64 |
| `material-visibility-tests` | 22,944 | ELF64 |

| Step | Time |
|---|---:|
| configure, including dependency fetch | 8 s |
| build | 17 s |
| CTest, all 34 tests | 1 s |
| clean rebuild end to end | 27 s |

## Upstream patches

Two portability fixes are applied to `server/api/deps/rsa_keygen/` and recorded
in [`patches/README.md`](https://github.com/benjarogit/csretro/blob/main/patches/README.md). They affect a development
helper only, not the module.

## Known warnings

- `cmake_minimum_required` below 3.10 in the vendored `kangaru` and `sigslot` —
  upstream, warning only, removed when those pins are refreshed.
- `creating DT_TEXTREL` when linking `rsa_keygen` against the static OpenSSL —
  upstream, harmless for a development helper.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `compiler failed to instantiate: amxxpc32.so` | `amxxpc` run without its directory on `LD_LIBRARY_PATH` | use `$AMXXPC` from `scripts/fetch-server-toolchain.sh` |
| `SHA-256 mismatch` | corrupted or changed download | delete the file in `.cache/downloads/` and rerun |
| `Missing PAWN_COMPILER` | only one of the two Pawn variables set | set both or neither |
| `void main must return int` | an unpatched upstream `rsa_keygen` | apply `patches/server-api/0001-rsa-keygen-portability.patch` |
| `i386 architecture ... incompatible with i386:x86-64` | a host tool linking the 32-bit OpenSSL archives | see the same patch |
| `could not find baseline` or a fetch error | no network on first configure | connect once, or pre-populate `server/api/build/linux/_deps` |
| module is `ELF64` | `-m32` flags removed | keep `NAPI_TARGET_PROPS` intact |
| `material-*` test fails | policy regression | fix the policy, never the assertion |
