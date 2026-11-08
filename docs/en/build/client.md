# Building the client

The client is a set of 32-bit shared libraries that ship next to the retail game
and patch the engine at runtime. Nothing here touches the Steam installation.

Prerequisites: [build prerequisites](prerequisites.md).

## Quick start

```bash
scripts/doctor.sh          # host check
scripts/bootstrap.sh       # hooks, LFS, submodules, vcpkg
scripts/build-client.sh    # configure if needed, build, write a manifest
```

## Paths with spaces

Several dependency builds, OpenSSL's `Configure` in particular, break when the
build path contains spaces. This repository may live under such a path.

`scripts/build-client.sh` detects it and builds through a space-free symlink:

```text
/tmp/csretro-src -> <repository>
```

The files stay in the repository; only the path the compiler sees is different.
If you run CMake manually from a path with spaces, expect the OpenSSL port to
fail. Use the script, or configure from a space-free checkout.

Do not move an existing `build/linux` between the real path and the symlink:
CMake caches absolute paths. Run `scripts/build-client.sh --clean` after such a
change.

## Configuration

The `linux` preset is a Ninja Multi-Config preset:

| Setting | Value |
|---|---|
| Generator | Ninja Multi-Config |
| Triplet | `x86-linux` (`client/custom-vcpkg-triplets/x86-linux.cmake`) |
| C/C++ flags | `-m32 -march=pentium4 -mfpmath=sse` |
| Build directory | `client/build/linux` |
| Output | `client/build/linux/out/bin/<config>/` |

## Targets

`scripts/build-client.sh` builds in dependency order:

| Target | Artifact | Purpose |
|---|---|---|
| `client_mini` | `client_mini.dll` | HUD, view, weapons, radar |
| `engine_mini` | `next_engine_mini.dll` | runtime detours and engine patches |
| `nitro_api` | `nitro_api2.dll` | engine address and hook layer |
| `gameui` | `cstrike/cl_dlls/gameui.so` | native VGUI2 menus |
| `cstrike.exe` | `cstrike.exe` | launcher and process entry point |
| `BUILD_ALL` | all shipping modules | also `FileSystem_Proxy.dll`, `steam_api.dll`, `vgui2.so` |

Build a single target:

```bash
scripts/build-client.sh --target client_mini
```

Other options: `--config Debug`, `--jobs 8`, `--reconfigure`, `--clean`,
`--install-dir <dir>`, `--manifest <file>`.

## Reference build (G04, 2026-11-08, Release)

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

Measured on the reference host:

| Step | Time |
|---|---:|
| configure, including all vcpkg ports | 122 s |
| `client_mini` | 32 s |
| `engine_mini` | 56 s |
| `nitro_api` | 104 s |
| `gameui` | 18 s |
| `cstrike.exe` | 9 s |
| `BUILD_ALL` | 3 s |

Disk: `client/build/linux` about 282 MB, `client/vcpkg/downloads` about 90 MB.

The script writes a manifest with version, commit, configuration, build time and
one line per artifact:

```text
# size_bytes  sha256  elf  path
5482420  <sha256>  ELF32  client_mini.dll
```

The build fails if any artifact is not `ELF32`.

## Known warnings

- `creating DT_TEXTREL in a shared object` — upstream code, expected, not fatal.
- `cmake_minimum_required` below 3.10 in the vendored SDK — upstream, will be
  addressed when the tree is rebranded.
- `FetchContent_Populate is deprecated` — upstream `dep/funchook`.

None of these change the output.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `is a Git LFS pointer file` during configure | LFS objects not pulled | `git lfs pull` |
| `could not find baseline version` from vcpkg | vcpkg submodule is a shallow clone | `git -C client/vcpkg fetch --unshallow` |
| OpenSSL `Configure` fails with paths containing spaces | build path has spaces | use `scripts/build-client.sh`, which builds through a space-free symlink |
| artifact is `ELF64` | triplet or `-m32` flags overridden | keep the `linux` preset and `x86-linux` triplet |
| CMake picks up 64-bit libraries | multilib missing | run `scripts/doctor.sh` |
| vcpkg install fails after a toolchain change | stale binary cache or build trees | `scripts/build-client.sh --clean` |
