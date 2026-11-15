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

At callback lookup, the bridge checks i386 ELF identity and the shared Default Public game hash before loading. Movement, prediction and CreateMove remain original callbacks; the authorized keyboard wrapper forwards unconsumed events without changing their arguments or results. Initialization/shutdown, HUD frame/redraw and mouse-event/reset/deactivation wrappers forward to the original and connect the UI lifetime and work. It verifies the paired `vgui2.valve.so` beside the bridge and loads `xash_surface.so` with its shipped `fonts` folder. Named original methods supply panel traversal, layout, paint, popup flags and mouse input. The real engine supplies screen/mouse position, file reads and Marlett lookup. SDL2 is borrowed from the already-loaded client; relative mouse motion is consumed only for visible UI. The cursor is synchronized before each original VGUI scheduler frame. The original viewport receives four callable factory entries because its native initializer reads all four and prepends its own; a generic two-factory list is insufficient. The bridge binds original VGUI interfaces, initializes the viewport and starts it in separate checked phases. Frame, paint and input are enabled only after startup completes. Original GameUI initialization/startup now occupies these checked boundaries; partial shutdown restores globals and releases the binding when native leases are retired. The native video-data provider now requires real Xash renderer/mount cvars and an existing SDL2 GL window. Surface size and pointer bounds use engine dimensions, with actual SDL pixel formats/display modes rather than test-host defaults. Missing data fails initialization; no window or game is started by the provider. Actual NextClient initialization, native keyboard/focus acceptance and runtime assembly remain open. See [runtime acceptance](../maintainers/runtime-readiness.md).

The native `VENGINE_GAMEUIFUNCS_VERSION001` provider now implements the complete original nine-method Linux contract, including destructor slots and the five-argument renderer query. It is advertised only while real video and key backends are ready. Key names/bindings come from verified functions in the already-loaded Xash engine; action state uses the original client's `pFindKey`, and command keys are translated through SDL2 and original VGUI. Unknown engine fingerprints and missing services fail initialization. The registered i386 reference fingerprint is a compatibility observation, not clean-build provenance or gameplay acceptance. The shared resolver checks fresh ELF/build-ID metadata, symbol ownership and executable mappings; it does not claim resistance to malicious in-place modification of a local binary. BaseUI, system ownership and GameUI lifecycle are connected below; native runtime acceptance remains open. Tests use synthetic native modules and do not start the game.

The native engine-VGUI provider owns three distinct structural containers allocated by original VGUI. Its callback contract preserves the original 32-bit handles on both build architectures. It binds the complete original viewport after startup; Surface painting and input use the common root. Shutdown detaches borrowed children and frees only owned containers. Native UI operations defer requested teardown until the active call returns. Obsolete Steam login calls report unsupported through the original failure path and never claim authentication. The native GameUI lifecycle reuses these engine panel services.

The bridge now supplies the original System/BaseUI contracts and connects original GameUI and console startup at the existing viewport phases. Before original initialization, it checks the paired GameUI and the Xash filesystem; the engine must identify the same already initialized filesystem instance. It retains the actual modules loaded by the original client, without creating another GameUI singleton or mounting another filesystem. System services use real engine files, console, parameters, module/command registration and frame timing. Optional demo modules remain unavailable.

SDL2 2.0.8 or newer supplies a borrowed event watch: keyboard/text/focus events are queued and delivered on the UI owner thread. No SDL initialization, window creation, global filter replacement or text-input-mode change is performed. Gameplay keyboard events retain original handling outside UI capture, and text/focus/gameplay acceptance still needs a human runtime test. The single Surface answers keyboard demand from live native UI state. BaseUI painting accepts the actual full-window bounds only. An event watch cannot consume engine key bindings. The maintainer-authorized engine-facing key wrapper handles that separately: UI presses, repeats and releases stay consumed even after the UI closes. Presses begun in gameplay retain original repeat/release handling when UI opens. Matching engine key releases end press ownership, including focus-loss releases. Original mouse clear/deactivation callbacks do not reset keyboard ownership; ordinary UI changes do not either. Escape/toggleconsole activate once per press. Other events preserve the original arguments, binding pointer and result. Native visible keyboard-enabled popups can request capture; cursor visibility alone cannot. The authenticated capture domain also includes Xash international keys 256–264. Per-key guards preserve nested game repeats/releases even while UI teardown is pending, without duplicate dispatch. This adds no duplicate VGUI text/key delivery. Real keyboard/focus/gameplay acceptance remains pending.

Shutdown waits for active native calls, including before original client shutdown, and retires UI-owned commands, watchers and controls before native allocator/module teardown. The original shutdown is invoked at most once per successful initialization cycle. Uncertain cleanup retains the complete graph and requires a process restart; it never counts as successful cleanup. The same GameUI context is not reusable after shutdown. Compatibility fingerprints identify observed reference binaries; they do not establish build provenance or a runnable port.

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
