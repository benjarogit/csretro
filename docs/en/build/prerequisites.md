# Build prerequisites

CSRETRO builds on a **Linux x86_64 host** and produces **32-bit x86** binaries.
CachyOS/Arch is the reference distribution; the checks are generic and work on
any distribution that provides a 32-bit toolchain and X11 libraries.

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
  lib32-libxrandr lib32-libxinerama lib32-libxss lib32-libxfixes lib32-libxrender
```

Or let the bootstrap install exactly this set:

```bash
scripts/bootstrap.sh --install-deps
```

## Why 32-bit

GoldSrc is a 32-bit engine. Every client module must match its ABI, so the
project compiles with `-m32 -march=pentium4` and vcpkg uses the custom triplet
`client/custom-vcpkg-triplets/x86-linux.cmake`. Without multilib the client
cannot link, no matter how modern the host is.

## What the doctor checks

`scripts/doctor.sh` is read-only and exits non-zero when a hard requirement is
missing. It verifies:

- host architecture, distribution, glibc, GCC, CMake, Ninja, Python, Git, Git LFS
- build helpers: `curl`, `zip`, `unzip`, `tar`, `patch`, `make`, `pkg-config`, `perl`
- a real 32-bit C and C++ compile-and-link probe with `-m32 -march=pentium4`
- 32-bit `libGL` and the X11 libraries `X11`, `Xext`, `Xcursor`, `Xi`, `Xrandr`,
  `Xinerama`, `Xss`, `Xfixes`, `Xrender`
- a 32-bit Mesa `swrast` driver for offscreen rendering tests
- the vcpkg submodule, its binary and the `x86-linux` triplet
- initialised submodules and at least 20 GB free space

The Steam runtime is reported as a warning only: it is needed to run the retail
game, not to build.

## Deliberately not required

- **System SDL2 for 32-bit.** vcpkg builds SDL2 from source and links it
  dynamically for the engine's SONAME. The headers come from the vendored copy
  in `client/dep/SDL2/include`.
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
| `cannot find -lX11` during the probe | 32-bit X11 development files missing | install `lib32-libx11` |
| `-m32` fails before linking | 32-bit libc or libstdc++ missing | install `lib32-glibc lib32-gcc-libs` |
| `GL/gl.h` not found | 32-bit OpenGL headers or libs missing | install `lib32-mesa lib32-libglvnd` |
| `no 32-bit swrast driver` | Mesa 32-bit runtime missing | install `lib32-mesa` |
| `vcpkg binary: not bootstrapped` | submodule present, tool not built | run `scripts/bootstrap.sh` |
| `glibc ... is too old` | distribution older than the minimum | use a newer distribution or container |
| CMake configure picks 64-bit libraries | multilib not enabled or triplet overridden | enable multilib, keep the `x86-linux` triplet |

Never install CSRETRO into the Steam game directory. See
[security rules](../../../AGENTS.md#10-security).
