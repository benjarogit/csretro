# Build prerequisites

CSRETRO builds on a **Linux x86_64 host**. The client is **x86_64**. The
server module stays **i386** because ReHLDS loads it that way. CachyOS/Arch
is the reference distribution.

The retail game is **not** needed to build. It is only needed to run the client,
and it must stay untouched in the Steam library.

## Minimum versions

| Component | Minimum | Verified on the reference host |
|---|---|---|
| Linux kernel | 6.1 | 6.x |
| glibc | 2.43 | 2.44 |
| GCC / G++ | 15 | 16.2.1 |
| CMake | 3.21 | 4.4.5 |
| Ninja | 1.11 | 1.13.2 |
| Python | 3.12 | 3.14.9 |
| Git | 2.30 | 2.56.0 |
| Git LFS | 3.0 | 3.8.1 |
| Free disk space | 20 GB | 636 GB |

`uv` is optional and only used for Python tool environments.

## Packages on Arch / CachyOS

```bash
sudo pacman -S --needed \
  base-devel cmake ninja python uv git git-lfs shellcheck \
  curl zip unzip tar patch pkgconf perl nasm \
  lib32-glibc lib32-gcc-libs lib32-mesa lib32-libglvnd \
  lib32-libx11 lib32-libxext lib32-libxcursor lib32-libxi \
  lib32-libxrandr lib32-libxinerama lib32-libxss lib32-libxfixes lib32-libxrender \
  mesa libglvnd libx11 libxext libxcursor libxi libxrandr libxinerama \
  libxss libxfixes libxrender
```

Or let the bootstrap install exactly this set:

```bash
scripts/bootstrap.sh --install-deps
```

## Why x86_64

The product process is x86_64. The decision is in
[plan section 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md). The client uses the vcpkg
triplet `x64-linux`. The server module stays i386 and still needs the 32-bit
toolchain.

## What the doctor checks

`scripts/doctor.sh` is read-only and exits non-zero when a hard requirement is
missing. It verifies:

- host architecture, distribution, glibc, GCC, CMake, Ninja, Python, Git, Git LFS
- build helpers: `curl`, `zip`, `unzip`, `tar`, `patch`, `make`, `pkg-config`, `perl`
- a real x86_64 C and C++ compile-and-link probe
- native `libGL` and the X11 libraries `X11`, `Xext`, `Xcursor`, `Xi`, `Xrandr`,
  `Xinerama`, `Xss`, `Xfixes`, `Xrender`
- a Mesa `swrast` driver for offscreen rendering tests
- the vcpkg submodule, its binary and the `x64-linux` triplet
- the 32-bit toolchain for the server module (`-m32`, ELF32)
- initialised submodules and at least 20 GB free space

The Steam runtime is reported as a warning only: it is needed to run the retail
game, not to build.

## Deliberately not required

- **System SDL2.** vcpkg builds SDL2 and the client links that copy. Retail
  `hw.so` no longer shares its SDL with this process.
- **Windows tooling.** Windows is not a target.
- **CEF.** The browser UI stays disabled on Linux.

## Verify and prepare

```bash
scripts/doctor.sh          # read-only prerequisite check
scripts/bootstrap.sh       # hooks, LFS, submodules, vcpkg, verification
```

`bootstrap.sh` is idempotent. Run it again after switching branches or updating
submodules.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `cannot find -lX11` during the probe | native X11 development files missing | install `libx11` |
| server probe `-m32` fails | 32-bit libc or libstdc++ missing | install `lib32-glibc lib32-gcc-libs` |
| `GL/gl.h` not found | native OpenGL headers or libs missing | install `mesa libglvnd` |
| `no swrast driver` | Mesa runtime missing | install `mesa` |
| `vcpkg binary: not bootstrapped` | submodule present, tool not built | run `scripts/bootstrap.sh` |
| `glibc ... is too old` | distribution older than the minimum | use a newer distribution or container |
| CMake stays on the old triplet | cache from `x86-linux` | `scripts/build-client.sh --clean` |

Never install CSRETRO into the Steam game directory. See
[security rules](https://github.com/benjarogit/csretro/blob/main/AGENTS.md#10-security).
