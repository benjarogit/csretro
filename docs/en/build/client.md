# Building the client

Build the existing NextClient components in the architecture required for your task. The first port runs on Xash3D with matching locally supplied original game libraries; exclusively x64 gameplay is the following phase. The sequence is defined in [plan section 3.8](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md). A successful component build does not establish a playable client. Nothing here starts the game or changes the Steam installation.

Prerequisites: [build prerequisites](prerequisites.md).

## Choose a profile

- `linux` (default): x64 components, vcpkg `x64-linux`, host compiler flags.
- `linux-port`: i386 components for the first port, vcpkg `x86-linux`, `-m32` for C, C++ and assembler. Requires matching 32-bit OpenGL and FreeType libraries; on Arch they are found under `/usr/lib32`.
- `linux-tests` and `linux-port-tests`: matching profiles with test targets enabled. Test configurations do not produce the shipping engine library; use the non-test profile for deployment.

All profiles use Ninja Multi-Config and separate `client/build/<preset>` directories. Shared artifacts are under `out/bin/<config>/` within that directory. Never combine binaries from different architectures or configurations.

## Build components

```bash
scripts/doctor.sh
scripts/bootstrap.sh
scripts/build-client.sh --target xash_surface
scripts/build-client.sh --preset linux-port \
  --target client_mini --target nitro_api --target engine_mini --target gameui
```

Repeated `--target` options build those targets sequentially. Without them, the script builds the existing shipping set, including the launcher and `BUILD_ALL`. An install directory requests the existing installation recipe; it does not assemble a working Xash3D runtime automatically.

Other options: `--config Debug`, `--jobs 8`, `--reconfigure`, `--clean`, `--install-dir <dir>`, `--manifest <file>`. The default configuration is Release. Reconfigure after changing a preset or adding sources collected by CMake globbing.

The manifest records version, source commit, preset, configuration, build time, size, SHA-256, ELF class and relative artifact path. CMake derives the expected ELF class from the compiler ABI. Empty shared output or an artifact of a different class fails the build. The manifest inventories the output directory; it does not certify callbacks, movement, performance or gameplay.

## Verify components without starting the game

After configuring from a space-free client source path:

```bash
cmake --preset linux-port-tests
cmake --build --preset linux-port-tests-release --target xash_surface-tests
LIBGL_ALWAYS_SOFTWARE=1 build/linux-port-tests/out/bin/Release/xash_surface-tests
cmake --build --preset linux-port-release --target verify-nitro-api-factory goldsrc_abi_golden
```

The Surface tests use their own offscreen context. The factory probe loads our provider, and the golden target compiles ABI assertions. Neither starts a game. The build-script regression suite runs with `python3 -m unittest discover -s tests/build_scripts -v` from the repository root and is also part of CI. Gameplay evidence follows [runtime acceptance](../maintainers/runtime-readiness.md).

## Original-client bridge

Build it with `scripts/build-client.sh --preset linux-port --target xash_client`. The output `csretro_client.so` exposes the original callback-table entry. It is intended for the engine's game-client location, beside the locally supplied original library renamed `client.valve.so`. The runtime assembly/installer is not connected yet. Do not replace files in Steam or launch this incomplete component set as a finished port.

At callback lookup, the bridge checks i386 ELF identity and the shared Default Public game hash before loading. Movement, prediction, CreateMove and keyboard entries remain the original callbacks. Initialization/shutdown, HUD frame/redraw and mouse-event/reset/deactivation wrappers forward to the original and connect the UI lifetime and work. It verifies the paired `vgui2.valve.so` beside the bridge and loads `xash_surface.so` with its shipped `fonts` folder. Named original methods supply panel traversal, layout, paint, popup flags and mouse input. The real engine supplies screen/mouse position, file reads and Marlett lookup. SDL2 is borrowed from the already-loaded client; relative mouse motion is consumed only for visible UI. The cursor is synchronized before each original VGUI scheduler frame. GameUI, actual NextClient initialization, keyboard/focus acceptance and runtime assembly remain open. See [runtime acceptance](../maintainers/runtime-readiness.md).

`xash_client-tests` runs through the matching test preset. CI additionally uses `python3 tests/xash_client/run.py --profile linux-port --gtest-source /usr/src/googletest` on a multilib host with GoogleTest sources. This builds the same bridge and synthetic callback tests without SDK builds, vcpkg or licensed game files. Omitting `--gtest-source` uses a matching installed GoogleTest package; `--profile linux` tests native x64 delegation and rejects use of the i386-only production game contract. Add `--with-surface` to build and test the actual Surface with vendored SDK headers and system OpenGL/EGL/FreeType. Offscreen tests use an EGL pbuffer with `LIBGL_ALWAYS_SOFTWARE=1`; CI runs that combination on x64. Neither runner starts Xash3D or loads original game libraries.

## Paths and cleanup

Dependency builds such as OpenSSL fail with source paths containing spaces. The wrapper uses a space-free symlink `${TMPDIR:-/tmp}/csretro-src`; files remain in the repository. For manual CMake commands, enter its `client` directory. Do not move a configured build between the real path and this symlink: CMake caches absolute paths.

Use `scripts/build-client.sh --preset linux-port --clean` to rebuild only that profile. Use `scripts/clean.sh --client-build linux-port-tests` to remove only its test build, optionally with `--dry-run`. These commands preserve other profiles. Full generated-state cleanup remains `scripts/clean.sh`.

## Troubleshooting

- Git LFS pointer during configure: run `git lfs pull`.
- Missing vcpkg baseline: initialise the pinned submodule; fetch full history if its clone is shallow.
- Wrong ELF class or incompatible system library: check the selected profile and matching system dependencies, then clean only that profile and reconfigure. ELF32 is expected for `linux-port`.
- i386 assembler register/operand errors: reconfigure with the tracked port preset so `CMAKE_ASM_FLAGS=-m32` reaches assembly dependencies too.
- Missing 32-bit GL or FreeType: install the matching multilib packages from the prerequisite guide. A cached x64 library cannot serve an i386 build.

Historical build measurements and their commits live in [HANDOFF.md](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
