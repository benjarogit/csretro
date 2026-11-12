# Changelog

All notable changes to CSRETRO are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
The authoritative version string is the `VERSION` file.

## [Unreleased]

### Changed

- The HL1 SDK source is tracked at `client/dep/ncl-hl1-source-sdk/`, including
  the LP64 edits. The upstream base remains
  `0e9be00fc52b66742a09c84c8f6169dc7c83ca8a`. The rollup patch and the SDK
  submodule gitlink are gone. vcpkg stays a submodule.
- The public docs site uses Zensical's default template. The vgui.css skins are
  no longer part of the Page.
- AMX Mod X, ReAPI, ReHLDS, ReGameDLL_CS and Metamod-R are vendored under
  `server/vendor/`. Fetch scripts read the pinned archives in
  `server/vendor/dist/` and compile plugins against the vendored includes.
- GitHub Actions pins: `checkout`/`setup-python` v7, Pages deploy/upload v5.
  Dependabot groups Action updates into one weekly PR. `main` requires the
  `repository verification` check and rejects force-pushes. Spent Cursor work
  plans are deleted after a gate; the harness forbids stacking them as a second
  backlog.
- Version line is `1.0.0-dev` (pre-release of 1.0.0). First release tag remains
  `v1.0.0` at gate G14 when the client is playable. Repository visibility in
  `versions.lock.json` is `public`. Authoritative plan section 11 records public
  collaboration (Issues/PRs, Pages, Dependabot for Actions, upstream watch
  issues), SemVer from 1.0.0, and the three upstream responses (leave, refresh
  pin, rebuild as `patches/`). Docs mirror is a GitHub Page from `docs/de` and
  `docs/en`, not a wiki. Cursor work plans are staging only; buildable decisions
  land in the same gate as the plan entry (`AGENTS.md`, plan §5.4).

### Added

- `OnGameInitializing` reads an engine pointer table from this process when
  the retail hook is absent. Null function pointers are not calls, and
  `Sys_Error` is not called on that table. `Con_Init` then runs. It reads
  `-condebug` through `COM_CheckParm` and does not call a null filesystem, so
  `qconsole.log` stays. `pfnGetGameDir` then writes the mounted directory
  name. An empty mount leaves that pointer null, and `SetCStrikeFlags` is
  not called. A mounted name runs `SetCStrikeFlags`. `NET_SendPacket` is the
  function in this process when that retail hook is absent. Init stores the
  pointer and does not call it, so no socket is opened. A null pointer is not
  a call. A null hook is not a call. `Netchan_CopyFileFragments` is then the
  function in this process when that retail hook is absent. Init stores the
  pointer and does not call it, so no fragment file is copied and no socket
  is opened. A null pointer is not a call. A null hook is not a call. The
  `SVC_TimeScale` is then the function in this process when that retail hook
  is absent. That function is `CL_Parse_Timescale`. Init stores the pointer
  and does not call it, so the timescale stays unchanged and no socket is
  opened. A null pointer is not a call. A null hook is not a call.
  `SVC_SendCvarValue` is then the function in this process when that retail
  hook is absent. That function is `CL_Send_CvarValue`. Init stores the
  pointer and does not call it, so no cvar is sent, cvar values stay
  unchanged and no socket is opened. A null pointer is not a call. A null
  hook is not a call. `CL_HTTPUpdate` is then the function in this process
  when that retail hook is absent. Init stores the pointer and does not call
  it, so no HTTP request is made and no socket is opened. A null pointer is
  not a call. A null hook is not a call. `CL_HTTPStop_f` is then the function
  in this process when that retail hook is absent. Init stores the pointer
  and does not call it, so no download is stopped and no socket is opened. A
  null pointer is not a call. A null hook is not a call.
  `CL_StartResourceDownloading` is then the function in this process when
  that retail hook is absent. Init stores the pointer and does not call it,
  so no download is started and no socket is opened. A null pointer is not a
  call. A null hook is not a call. `CL_ReadPackets` is then the function in
  this process when that retail hook is absent. Init stores the pointer and
  does not call it, so no packet is read and no socket is opened. A null
  pointer is not a call. A null hook is not a call.
  `CL_RequestMissingResources` is then the function in this process when that
  retail hook is absent. Init stores the pointer and does not call it, so no
  resource is requested and no socket is opened. A null pointer is not a
  call. A null hook is not a call. `CL_Disconnect` is then the function in
  this process when that retail hook is absent. Init stores the pointer and
  does not call it, so no disconnect runs and no socket is opened. A null
  pointer is not a call. A null hook is not a call. `CL_ClearState` is then
  the function in this process when that retail hook is absent. Init stores
  the pointer and does not call it, so client state stays unchanged and no
  socket is opened. A null pointer is not a call. A null hook is not a call.
  `Host_Map_f` is then the function in this process when that retail hook is
  absent. Init stores the pointer and does not call it, so no map is loaded,
  the level name stays unchanged and no socket is opened. A null pointer is
  not a call. A null hook is not a call. `Host_FilterTime` is then the
  function in this process when that retail hook is absent. Init stores the
  pointer and does not call it, so host time and the frame interval stay
  unchanged and no socket is opened. A null pointer is not a call. A null
  hook is not a call. `Mod_ClearAll` is then the function in this process
  when that retail hook is absent. Init stores the pointer and does not call
  it, so the model list stays unchanged and no socket is opened. A null
  pointer is not a call. A null hook is not a call. `Mod_FindName` is then
  the function in this process when that retail hook is absent. Init stores
  the pointer and does not call it, so no model is looked up or loaded and
  no socket is opened. A null pointer is not a call. A null hook is not a
  call. `Mod_Print` is then the function in this process when that retail
  hook is absent. Init stores the pointer and does not call it, so the
  model list is not printed and no socket is opened. A null pointer is not
  a call. A null hook is not a call. The next retail hook is
  `Mod_ValidateCRC`. This path returns before it.
  `csr_host_frame` is a later direct call. One call paints one GameUI frame
  through the present path and adds the caller's delta to host time by
  writing `realtime`, `host_frametime` and `oldrealtime`. `Host_FilterTime`
  is not called, no hook is installed, and `CVGui::RunFrame` is not called.
  The retail hook list is not extended.
  `OnGameInitialized` is not entered. The retail hook chain
  does not run. `hw.so` stays
  unmapped. The game is not started.
- `scripts/install-content.sh` searches a Steam library (`libraryfolders.vdf`,
  `appmanifest_10.acf`) and copies the mounted Counter-Strike 1.6 files into a
  directory outside that library. Files shipped in `client/assets` overlay
  that copy: the same relative path keeps the shipped file, and a retail file
  with no shipped replacement stays. A missing Steam install or a missing game
  fails. Personal configs, demos, screenshots, caches, Steam and VAC binaries
  and `hw.so` stay behind. The game is not started.
- The host loads `next_engine_mini.dll` and `nitro_api2.dll` after the
  other host pieces, calls `EngineMini::Init`, then `csr_xash_engine_argv`,
  `csr_xash_engine_mode` and
  `csr_xash_engine_init_game`. `COM_InitArgv` runs. A successful open does
  not call shutdown or the fatal error. A missing library or export is a
  failure and is not called. The open does not exec Xash3D, does not call
  `RunFrame` and does not start Steam.
- `xash_engine` takes the startup sequence through `csr_xash_engine_argv`,
  `csr_xash_engine_mode`, `csr_xash_engine_init_game`,
  `csr_xash_engine_shutdown` and `csr_xash_engine_error`. A present retail
  hook keeps the retail call. A missing hook is recorded by `engine_mini`,
  which also records that the engine is Xash. `COM_InitArgv` runs. The call
  does not open `hw.so` and does not call `RunFrame`.
- `csr_host_load_pair` binds cursor, press and release to the pair's
  `VGUI_InputInternal001` and dispatches once per event without
  `IInputInternal::RunFrame` or `CVGui::RunFrame`.
- A laid-out `GameMenu.res` item receives that bind. Paint associates the
  embedded root with the default input context. `QuitNoConfirm` records
  `quit` and does not run it. LP64 edits to the SDK are the files under
  `client/dep/ncl-hl1-source-sdk/`.
- `PresentXashLauncher` activates that menu and paints one frame. The planned
  command is not executed and `RunFrame` is not called. `setpause` is only
  recorded.
- Public documentation site: Zensical (pinned) builds `docs/de` and `docs/en`
  with four audience hubs (players, maintainers, plugins, servers). Workflows:
  Pages deploy after
  green `ci` on `main`, Dependabot for Actions, weekly upstream-watch issue.
  `SECURITY.md` is the only security policy text.
- `xash_surface.so`, a `VGUI_Surface026` the host factory can hand to `vgui2`:
  fills, texture quads and a FreeType glyph atlas, with panel offset, clip and
  GL state restored after each draw. Shipped substitutes stand in for Arial,
  Verdana and Tahoma. `PaintTraverse` enters the embedded panel and popups.
  Mouse move, press and release are forwarded to the host and each one is
  followed by a dispatch. `RunFrame` stays empty.
- `xash_host.so` loads that surface and publishes it as the first
  `VGUI_Surface026` factory. Our `vgui2` exports `csr_vgui_dispatch_messages`
  because the retail library keeps `DispatchMessages` local. The Xash client
  does not start a Steam process; original game content stays required.
- `ResolvePlayerIdentity` is the one local player slot. A logged-in Steam
  persona supplies the shown name and, when pixels are already present, the
  portrait. Otherwise the slot is the in-game name and has no portrait. The
  lookup does not start Steam, store credentials, or download an image.
- The Xash launcher plans one command from the existing host, surface,
  client pair, `gameui`, filesystem root and `libsteam_api.so`. The same
  process opens that list and does not execute the command. A missing
  content root is a failure. `RunFrame` stays uncalled and Steam stays down.
  The GoldSrc starter stays in place.
- The ELF64 `libsteam_api.so` reports logged out when Steam is not already
  logged in. A missing interface is not called. The shown name stays
  `ResolvePlayerIdentity`. The proxy does not start Steam.
- `xash_host` serves `VFileSystem009` on the existing surface factory after
  one root is mounted. A missing file is a miss. `DrawSetTextureFile` reads
  an uncompressed TGA from that root. A missing or compressed file is not a
  texture. A cursor bound to that texture is drawn at the end of
  `PaintTraverse`. `RunFrame` does not draw it. After the root is mounted,
  `IVGui::Init` gives `vgui2` that surface. A scheme file under that root is
  loaded through the same manager. A missing file leaves the host open.
  `IGameUI::Initialize` parents the base panel to the host root and builds the server list once `resource/schemes/TrackerScheme_ClassicPlus.res` can be read. A missing engine module is
  not called. `csr_host_paint_gameui` sizes that root and paints the tree. When the scheme file is present, that paint reads `resource/BackgroundLayout.txt` and loads an extensionless texture name from the `.tga` beside it. The background image is drawn at time 0. `csr_host_start_gameui` calls `IGameUI::Start` on a host engine table that does not open `hw.so`. The following paint builds the options pages. `RunFrame` is not that call.
  The proxy does not open `hw.so` and does not start the game.
- `xash_host` opens `gameui` after the one surface factory and reads
  `GameUI007` and `GameConsole003`. It does not call into the game UI.
- `gameui` builds as ELF64 from the same SDK revision and links the host
  runtime. It exposes `GameUI007` and does not carry a second surface.
- `xash_host` loads the matched `client_mini` and `vgui2` after the one
  surface factory. A missing library is not called. The host does not start
  the game.
- `client_mini` and `vgui2` build together from the vendored SDK as ELF64 and
  link the host `libtier0.so`, `libvstdlib.so` and `libsteam_api.so`.
- `xash_engine` forwards `vgui2_surface`, texture reset and text color to the
  one Xash surface. A missing function is not called. Other `engine_mini`
  forwards stay on the retail hook path.
- The product engine identity is Xash3D. `nitro_api` does not open, hash, or hook
  `hw.so`. Missing engine addresses are not hooks and are not called.
- The Linux client preset builds x86_64 and uses the vcpkg triplet `x64-linux`.
  There is no second 32-bit client preset. Valve's i386 `hw.so`, `client.so`
  and `vgui2.so` are not loaded. The server module stays i386.
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
- Vendored HL1 SDK source at `client/dep/ncl-hl1-source-sdk/`. The upstream
  base is `0e9be00`. vcpkg stays the submodule at `b5d1a94`.
- `scripts/bootstrap.sh` for hooks, LFS and submodules.
- `scripts/sync-upstream.sh` to report upstream changes since the pins.
- `scripts/doctor.sh`: read-only host check with real 32-bit compile-and-link
  probes, Mesa swrast, vcpkg triplet and disk space.
- `scripts/bootstrap.sh --install-deps` for the Arch/CachyOS package set and
  automatic vcpkg bootstrap.
- Bilingual build prerequisites: `docs/de/build/prerequisites.md` and
  `docs/en/build/prerequisites.md`.
- `scripts/build-client.sh`: configure, build in dependency order, write a hash
  manifest, fail on non-ELF32 artifacts, transparent space-free symlink.
- The Linux starter starts Steam when it is closed, waits up to 90 seconds for
  the process and fails with a clear message instead of starting a client that
  cannot authenticate.
- Bilingual client build guide: `docs/de/build/client.md` and
  `docs/en/build/client.md`.
- Verified 32-bit client build: eight ELF32 artifacts including `gameui.so` and
  `vgui2.so`.
- `scripts/fetch-server-toolchain.sh`: downloads AMX Mod X `1.10.0.5487` and
  ReAPI `5.29.0.358`, verifies every SHA-256 against `versions.lock.json`,
  caches them under `.cache/` and provides an `amxxpc` wrapper.
- `scripts/build-server.sh`: configure, build, CTest, module metadata and ELF32
  validation, hash manifest.
- Bilingual server API build guide: `docs/de/build/server.md` and
  `docs/en/build/server.md`.
- Verified 32-bit server module `nextclientapi_amxx_i386.so` and 34/34 green
  tests, including the compiled Pawn test plugin.
- `patches/` with the documented `rsa_keygen` portability fix for GCC 16 and
  the 32-bit OpenSSL archives.
- `scripts/lib/fetch.sh`: shared download helper that reads pins from
  `versions.lock.json`, verifies SHA-256 and reuses `.cache/downloads/`.
- `scripts/fetch-server-runtime.sh`: stages ReHLDS `3.15.0.896`, ReGameDLL_CS
  `5.30.0.814`, Metamod-R `1.3.0.149`, the AMXX cstrike package and ReAPI, and
  reports the pinned SteamCMD image digest.
- `scripts/build-server.sh --stage`: assembles the Docker build context with
  runtime archives, module, compiled test plugin, test assets and a hash
  manifest.
- `server/pack/`: multi-stage image (SteamCMD/App 90, Steam ABI stub, runtime),
  Docker Compose file, entrypoint, RCON healthcheck, Metamod and AMXX
  configuration.
- `server/pack/steamcompat/steamstub.cpp`: no-op `ISteamGameServer011`
  implementation for ReHLDS, which was built against the pre-25th-anniversary
  Steamworks ABI.
- `tests/e2e/run.sh` and `tests/e2e/test_server.py`: reproducible test server
  with log and RCON checks, clean `down --volumes` on every exit path.
- Bilingual test server guide: `docs/de/server/testserver.md` and
  `docs/en/server/testserver.md`.
- `scripts/doctor.sh` reports Docker and Compose as optional prerequisites.
- SHA-256 build registry in `nitro_api` (`build_registry.cpp`) that identifies
  the Default Public build from `hw.so` and `client.so` and refuses unknown
  fingerprints.
- `docs/de/client/runtime.md` and `docs/en/client/runtime.md`: build detection
  and how to register a build.
- Three-stage address resolver (`SearchType::SymbolOrPattern`): dynamic symbol,
  full ELF symbol table, byte pattern as the last resort.
- Public address providers `EngineAddressProviderPublicLinux` (327 entries) and
  `ClientAddressProviderPublicLinux` (81 entries); every name is checked against
  the retail binaries.
- `BuildFamily` in `BuildVersion` so the provider choice follows the detected
  build family instead of a single hardcoded provider.
- Resolver tests with a dedicated fixture library covering dynsym, symtab,
  pattern fallback, a missing symbol and an unmatched pattern that must not turn
  into an address through its offset.
- `tools/check_address_providers.py`: checks both public providers against a game
  installation, rejects missing symbols, non-unique strings and unsupported
  search types.

### Changed

- CI concurrency group is `verify-` plus the ref. Actions run 43095634906 still
  holds the previous group and GitHub will not cancel it, so a later push would
  not have been scheduled.
- Root `.gitattributes` no longer reinterprets vendored binaries through the LFS
  rules; each vendored tree keeps its own attribute policy.
- Vendored NextClient moved from `90b4bf7` to `24cc986` (G06b radar): Linux
  process-map parsing no longer truncates module paths that contain spaces.
  Imported with full history via `git subtree pull`; `versions.lock.json` updated.
- Build detection switched from `hw.so` file size to SHA-256 fingerprints; the
  legacy 8684 address providers are removed because CSRETRO targets only the
  Default Public build.
- `scripts/fetch-server-toolchain.sh` uses the shared `scripts/lib/fetch.sh`
  download helper.

### Fixed

- LFS objects for the vendored client are imported, so a fresh clone resolves all
  790 binary assets.
- The test plugin's custom models, sounds, sprites and material preset are
  staged into the image; a missing model aborted the map load.
- The test server keeps `modules.ini` empty so AMXX detects modules itself
  instead of loading each one twice through Metamod.
- `GetSysModuleInfo` on Linux now returns the span across all mapping segments
  of a module. It previously reported only the first mapping, so a pattern
  search never reached executable code.
- The Linux client start no longer stops at `undefined symbol`:
  - The launcher loads `steam_api.dll` into the global scope before the first
    module, so `next_engine_mini.dll`, `gameui.so` and `vgui2.so` find the legacy
    Steam accessors they import.
  - The proxy resolves those accessors through the flat
    `SteamAPI_Steam*_vNNN` functions of the retail `libsteam_api.so`, which does
    not export the legacy names. Missing accessors return `nullptr` instead of
    calling through a null pointer.
  - The installer replaces `libtier0.so` with the SDK build, which exports
    `GetCPUInformation` for `client_mini.dll`; Valve's original is kept as
    `libtier0.so.valve`.

## [0.1.0-dev] - 2026-11-08

First tracked state. Not released, not installable.
