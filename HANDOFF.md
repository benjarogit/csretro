# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-09
- **Gate:** G07 – Default Public runtime, **in progress** (G07a–G07c done, G07d fixed; the Xash3D client spike now completes `HUD_Init`, engine direction decided: Option A)
- **Branch:** `main`
- **Commit:** `c20dd1a` (engine decision docs), this handoff update on top
- **Version:** `0.1.0-dev` (`VERSION`)
- **CI:** https://github.com/benjarogit/csretro/actions/runs/42614991668 – success

## Verified (with evidence)

| Claim | Evidence |
|---|---|
| Test server starts reproducibly | `tests/e2e/run.sh` exit 0, container `healthy` |
| Image builds from the pinned SteamCMD digest | staging manifest records the digest from `versions.lock.json`; build exits 0 |
| ReHLDS, ReGameDLL, Metamod-R and AMXX load | log: `Metamod-r v1.3.0.149`, `AMX Mod X version 1.10.0.5487`, ReGameDLL `5.30.0.814-dev` |
| The module loads in a real server | log: `[nclapi] Next Client API Successfully loaded, version 1.6.0` |
| The module version is queryable | rcon `nclapi_version` → `"nclapi_version" is "1.6.0"` |
| The test plugin runs | rcon `amxx plugins` → `Test NCL API … running`, `1 plugins, 1 running` |
| The map starts | log: `Started map "de_dust2" (CRC "1159425449")` |
| RCON challenge flow works | `tests/e2e/test_server.py` checks `status` and the plugin list |
| No crash and no assertion spam | log has no `FATAL ERROR`, `Fencepost`, `Segmentation fault`, no dump |
| The test plugin finds its assets | `test_nextclient_api.log`: material preset uploaded, no runtime error |
| Cleanup is guaranteed | `run.sh` trap runs `docker compose down --volumes --remove-orphans` |
| Doctor reports container tooling | `scripts/doctor.sh`: 43 passed, 0 warnings, 0 failed |
| Repository checks pass | `scripts/verify.sh` green |
| NextClient upstream fix is imported | `git subtree pull` merge `bcd2e79`, `pmparser.h` uses `pathname[PATH_MAX]` |
| The imported fix is covered by upstream tests | `ctest -C Release -R LinuxModulePath` → 2/2 passed |
| Client still builds after the import | `scripts/build-client.sh` exit 0, 211 s, 8 ELF32 artifacts |
| Pins match the imported code | `versions.lock.json`: NextClient `24cc986…`, `updated_in: G06b` |
| Address resolver covers dynsym, symtab and pattern | `nitro_api-tests`: 5 resolver tests green against a fixture library |
| Public provider names exist in the retail binaries | offline check: 327/327 engine names in `hw.so` `symtab`, 81/81 client names in `client.so` `symtab` |
| Provider check tool works and fails loudly | `tools/check_address_providers.py --game-dir …` exit 0; injected missing symbol, duplicated string and `SearchType::Offset` each reported |
| All Linux client modules load | 32-bit `dlopen` probe: `next_engine_mini.dll`, `client_mini.dll`, `gameui.so`, `vgui2.so`, `nitro_api2.dll`, `FileSystem_Proxy.dll` load after the global `steam_api.dll` |
| The installer keeps Valve's libraries | install test green for Windows/Linux × Tracy off/on, including `vgui2.so.valve` and `libtier0.so.valve` preservation on repeat install |
| Xash3D x86_64 starts headless | dedicated log: `Dll loaded for game "Counter-Strike"`, `ReGameDLL version: 5.30.0.871-dev`, `execing maps/de_dust2_load.cfg`, `4 player server started` |
| Xash3D 32-bit loads and fully initialises Valve's `client.so` | `Initialize` returns 1 (`cl_enginefunc_t` 536 bytes, version 7); with the spike surface adapter `HUD_Init` returns and the engine reaches `exec config.cfg` and a clean `+quit` shutdown |
| The Xash3D client spike completes `HUD_Init` | `CounterStrikeViewport::Start` returns with viewport field `0x48` non-null; run exit 0, no `crash_backtrace.txt` |
| Valve's `client.so` requires Valve's `vgui2.so` | pairing it with NextClient's `vgui2.so` crashes in `Label::ApplySettings` (`pScheme->GetFont`, slot 6 → type_info vtable); the interface implementations live in `vgui2.so`, not `client.so` |
| Xash3D ships FreeVGUI (VGUI1) but no `VGUI_Surface026` | `3rdparty/freevgui` plus `platform/xash3d-fwgs/surface.cpp`; `nm -D vgui.so` exports only `GetVGUISupportAPI`/`InitAPI` |
| The ReHLDS test server is ready for the spike | `tests/e2e/test_server.py` green on udp/27016: Metamod-R, AMXX, nclapi, `de_dust2`, `nclapi_version 1.6.0` |

Reference run (2026-11-08, local):

```text
ok   log: Metamod-r v1.3.0.149
ok   log: AMX Mod X version 1.10.0.5487
ok   log: [nclapi] Next Client API Successfully loaded, version 1.6.0
ok   log: Started map "de_dust2"
ok   rcon: map     :  de_dust2 at: 0 x, 0 y, 0 z
ok   rcon: "nclapi_version" is "1.6.0"
ok   rcon: amxx plugins: Test NCL API running
all e2e checks passed
```

## Upstream radar (G06b)

`scripts/sync-upstream.sh` run on 2026-11-09 against the four pins.

| Upstream | Pin | Head | Verdict |
|---|---|---|---|
| NextClient | `90b4bf7` | `24cc986` | **taken** – 1 commit, Linux maps parsing for paths with spaces |
| NextClientServerApi | `1da463e` | `1da463e` | up to date, nothing to do |
| ncl-hl1-source-sdk | `0e9be00` | `0e9be00` | up to date, nothing to do |
| vcpkg | `b5d1a94` | `ec48c11` | **deferred** – 2919 commits behind |

**NextClient `24cc986` – taken.** `pmparser` split map lines on whitespace, so a
process path containing a space (this repository itself) was truncated. The fix
switches `pathname` to `PATH_MAX` and scans to the line end. It is imported with
full history via `git subtree pull` (`bcd2e79`); the pin in `versions.lock.json`
now points at `24cc986`. Upstream added `dep/nitro_utils/tests/`
(`LinuxModulePathTests`); a local build with `NEXTCLIENT_BUILD_TESTS=ON` runs
2/2 green. The client rebuild after the import stays green with the same eight
ELF32 artifacts.

**vcpkg – deferred, not ignored.** The 2919 commits are a registry-wide baseline
move (port updates, no security-driven pin). Bumping it forces a full port
rebuild of every dependency and can change ABI and CMake minimums. That is a
separate dependency gate with its own build-and-test evidence, not a side effect
of the radar. The pin stays at `b5d1a94`; the gap is recorded here so the next
radar does not treat it as new.

## Default Public runtime (G07)

**G07a – build registry, done.** The client no longer guesses the build from
`hw.so` file size. `NitroApi::DetectBuildIdentity()` hashes `hw.so` and
`cstrike/cl_dlls/client.so`, and `build_registry.cpp` maps the pair to an
identity. Only an exact match is accepted; an unknown pair logs both digests and
disables the hooks. The legacy 8684 address providers are deleted: they target a
build CSRETRO does not support and are not a fallback.

Registered build:

| Field | Value |
|---|---|
| ID | `public-linux-1.0.1.4` |
| Platform | Linux |
| Steam build | 12934623 |
| `hw.so` SHA-256 | `fca6628b5a4d76a945e11b9796f327004edc65420d9f9cc23f883143508edd78` |
| `client.so` SHA-256 | `2e799ef31931c0f4372ff31ad30f0aab529b16461ed3b0928dfbd958183b2e5a` |

Evidence: `nitro_api-tests` 65/65 green, including 7 registry tests (known,
unknown, incomplete, wrong platform, case-insensitive, file hashing); client
build exit 0 with the same eight ELF32 artifacts.

**G07b – three-stage resolver, done.** `SearchConfig` has a
`SearchType::SymbolOrPattern` mode that resolves in order: dynamic symbol
(`dlsym`), full ELF symbol table (read from the file on disk), then a byte
pattern. The public providers are `EngineAddressProviderPublicLinux.cpp`
(327 entries) and `ClientAddressProviderPublicLinux.cpp` (81 entries); an offline
check confirmed that every name exists in the retail `symtab` of `hw.so` or
`client.so`. `BuildVersion` gained `BuildFamily`, and `NitroApi` selects the
provider by family instead of by a single hardcoded build.

`EngineMsgBase` is `cl_parsefuncs`; DWARF confirms the layout
`svc_func_s { int opcode; const char* pszname; pfnParse; }`, which matches
`goldsrc::EngineMsg`.

Fixed `GetSysModuleInfo` in `client/dep/nitro_utils/src/platform/platform_linux.cpp`:
it returned only the first mapping segment (the ELF header page), so pattern
searches never saw code. It now spans all segments of the module.

Evidence: `nitro_api-tests` 70/70 green, including 5 resolver tests (dynsym,
symtab, pattern fallback, missing symbol without pattern, unmatched pattern
with offset); client build exit 0
with the same eight ELF32 artifacts.

**G07c – remaining entries, done.** No numeric offsets and no byte patterns
remain in the public providers; every entry resolves by symbol or by a unique
string. `tools/check_address_providers.py` checks both providers against a game
installation: every name must exist in `dynsym` or `symtab`, every string must
occur exactly once, and only `SymbolOrPattern` and `String` are accepted.

Result on the retail binaries: 326 unique engine symbols and 1 engine string
resolve against `hw.so`; 81 client symbols resolve against `client.so`.

Two engine entries use compiler-generated names (`isFogEnabled.26640`,
`R_ForceCVars.part.3`) that exist only in this exact binary. The build is pinned
by SHA-256, a different build gets its own provider, and the tool reports any
mismatch. The negative cases (missing symbol, duplicated string, unsupported
search type) were exercised and each is reported.

**G07d – first start, fixed after the first run.** The first manual run stopped
with `Error: …/next_engine_mini.dll: undefined symbol: SteamMatchmakingServers`
and then `Module next_engine_mini.dll not found`. Two Linux-only gaps caused it:

1. The retail `libsteam_api.so` exports the flat `SteamAPI_Steam*_vNNN`
   functions but not the legacy C++ accessors (`SteamApps`, `SteamUser`,
   `SteamUtils`, `SteamMatchmakingServers`) that `next_engine_mini.dll`,
   `gameui.so` and `vgui2.so` import. The launcher now loads `steam_api.dll`
   with `RTLD_GLOBAL` before the first module, and the proxy resolves every
   accessor from the legacy name first and from the flat function second.
   An accessor that resolves to neither returns `nullptr`.
2. `client_mini.dll` imports `GetCPUInformation`, which the retail
   `libtier0.so` does not export. The installer now installs the SDK
   `libtier0.so` (278 exports, a superset of the retail 197) and keeps Valve's
   file as `libtier0.so.valve`.

Evidence: a 32-bit `dlopen` probe loads `next_engine_mini.dll`,
`client_mini.dll`, `gameui.so`, `vgui2.so`, `nitro_api2.dll` and
`FileSystem_Proxy.dll` once `steam_api.dll` is global; the install test covers
the tier0 backup and its preservation on a repeated install; the client build
exits 0 with the same eight ELF32 artifacts; the installed `libtier0.so`
SHA-256 `3056f15e…` equals the SDK file.

The manual check is documented in `docs/de/client/first-start.md` and
`docs/en/client/first-start.md`: copy the game outside Steam, run
`scripts/build-client.sh --install-dir <copy>`, start with
`nextclient.sh -windowed -console`, and record the console lines
`Game build identified: public-linux-1.0.1.4`, the absence of
`Unknown game build` and `Can't get engine address provider`, and whether the
game reaches the console without a crash.

**Blocked on the maintainer run.** Automation stops at the process boundary; the
game is never started by a script. The result goes into this file, then G07e ABI
validation against DWARF and G07f feature smoke follow.

**Xash3D spike – the engine path is proven through `HUD_Init`.** The spike runs
Xash3D-FWGS `e9b94dc` (32-bit build, SDL2 2.32.10, Xvfb/llvmpipe) with Valve's
`cstrike/cl_dlls/client.so` (SHA-256 `2e799ef3…`, the G07a registry build) and
Valve's `vgui2.so` (SHA-256 `f0291c95…`). It does not use FWGS `cs16-client`.

Result: `Initialize` accepts `cl_enginefunc_t` (536 bytes, version 7) and returns
1; `HUD_Init` returns; `CounterStrikeViewport::Start` returns with the viewport
panel set (field `0x48` non-null); the engine then reaches `exec config.cfg` and
shuts down cleanly on `+quit` (exit 0, no crash dump). Nothing is rendered yet.

How: a spike-only `LD_PRELOAD` shim (`kvshim.so`, outside the repository) hooks
the `F` export, patches `pfnInitialize`, and after `Initialize`:

- loads a generated stub `VGUI_Surface026` (119 slots taken from the `hw.so`
  `BaseUISurface` vtable, all no-ops) and injects it into the client;
- injects `vgui2.so`'s `KeyValues003`, `VGUI_ivgui006`, `VGUI_Panel007`,
  `VGUI_Scheme009`, `VGUI_System009`, `VGUI_Input004`, `VGUI_Localize003` and the
  client's own `VFileSystem009`;
- fakes `SteamApps`/`SteamAPI_Init`, because no Steam login is wanted;
- calls `CounterStrikeViewport::Initialize`/`Start`, because Xash3D has no GoldSrc
  GameUI to do it.

**Critical ABI finding.** Valve's `client.so` statically links only the VGUI2
*controls*. `CScheme`, `CSystem`, `CInput`, `CPanel`, `CLocalize` and the
`KeyValues` system come from `vgui2.so`. So Valve's `client.so` pairs only with
Valve's `vgui2.so`. NextClient's `vgui2.so` has a different `IScheme` vtable
layout; pairing it with Valve's `client.so` crashes in `Label::ApplySettings`
(`pScheme->GetFont`, slot 6 jumps into a type_info vtable). NextClient binaries
are only compatible with each other (`client_mini.dll` + `vgui2.so`). CSRETRO
therefore builds client and VGUI2 from the same SDK revision (the vendored
`ncl-hl1-source-sdk`) and never mixes prebuilt binaries.

Xash3D ships **FreeVGUI** (`3rdparty/freevgui`, BSD-3-Clause) with a native
Xash3D platform surface (`platform/xash3d-fwgs/surface.cpp`). It is VGUI1, not
VGUI2: it shows how 2D drawing is done on Xash3D but does not provide
`VGUI_Surface026`.

Decision: **Option A** (`VGUI_Surface026` adapter on Xash3D), recorded in
`docs/plan/csretro-plan.md` section 3.6. The remaining work is a real surface
backend (drawing, fonts, textures, mouse), not an ABI blocker. Option B
(FreeVGUI with GoldSrc VGUI2 exports) stays the fallback; Option C (retail)
stays the working comparison. NextClient's `engine_mini`/`nitro_api` also hook
`hw.so` (`vgui2_surface`), so Xash3D needs an engine adapter for those hooks.
HTML/MOTD needs a ChromeHTML controller/serializer; without CEF it is disabled
or replaced by a simple text view. Details: `.local/xash-spike.md` (ignored).

## Decisions taken in G06

1. **Steam ABI.** ReHLDS 3.15 imports `SteamGameServer_Init`, `SteamGameServer`
   and `SteamApps` and expects `ISteamGameServer011`. App 90 ships only
   `SteamInternal_GameServer_Init` and `SteamGameServer015`; the vtable layouts
   are incompatible. The test server uses a self-contained no-op stub
   (`server/pack/steamcompat/steamstub.cpp`) instead of the Steam client.
2. **No A2S with the stub.** ReHLDS forwards connectionless packets to the Steam
   game-server layer (`Steam_HandleIncomingPacket` / `GetNextOutgoingPacket`),
   so the stub yields no `A2S_INFO`. Healthcheck and e2e use RCON, which the
   engine handles itself.
3. **Runtime base `i386/debian:trixie-slim`.** The module needs
   `GLIBC_ABI_GNU_TLS` and `GLIBC_2.38`; Debian 12 has glibc 2.36.
4. **Empty `modules.ini`.** AMXX detects modules from the natives a plugin uses.
   Listing them explicitly makes Metamod load each one twice and log
   `already loaded`.
5. **Assets are staged.** The test plugin precaches custom models, sounds,
   sprites and a material preset from `server/api/amxx_test/`; a missing model
   aborts the map load.

## Distribution and data acquisition

- **Docker is not the shipping path.** The container in `server/pack/` exists for
  reproducible tests and CI. Players and server admins get a normal, portable
  installation: a directory plus `csretro.sh` (G09). Docker stays an optional
  way to host a *server*; the client always runs natively because it needs
  32-bit graphics and input, not a container.
- **Server base files are free.** App 90 (HLDS) downloads anonymously through
  SteamCMD; no Steam account, no ownership, no login. That is what the image
  build does and what a self-hosted server install will do.
- **Client files are not free.** CS 1.6 is the paid App 10. CSRETRO never logs
  into Steam and never asks for credentials. The installer discovers the local
  Steam libraries (`steamapps/libraryfolders.vdf`, `appmanifest_10.acf`), verifies
  the stock files and copies them into the separate CSRETRO directory; a manual
  `--source <dir>` works too. The vendored code for this is
  `client/installer/import_game.py`.
- **Manual copy stays possible.** `scripts/install.sh` (G09) is a convenience
  around copy, hashes and backups; a user who already has a valid game folder can
  point CSRETRO at it.
- **The installer is the normal path.** G09 ships `scripts/install.sh` with a
  `--target client|server|hltv|all` selector, `--dry-run` and `--uninstall`.
  Each target gets its own launcher (`csretro.sh`, `csretro-server.sh`,
  `csretro-hltv.sh`) and a `.desktop` entry with icon and menu category.
- **Import is allowlist-based.** Only the files CSRETRO needs leave the Steam
  installation: `cstrike`, `valve`, `platform`, the engine and client binaries,
  VGUI2 and required assets. Steam binaries, CEF and HTML caches, logs, dumps,
  demos, local configs and foreign mods are skipped, and the import reports what
  was skipped and why. Retail binaries are checked against the build registry.
- **Open:** whether the client still needs a running Steam client for auth
  tickets after G08 replaces the protocol. The vendored launcher starts through
  the Steam runtime today.

## Server performance backlog (after G06b)

Optimisation work on the AMXX module and plugins. Order is deliberate: measure
first, then change one thing at a time and re-measure on the test server.

1. **Measure first.** Baseline server FPS (`stats`), CPU (`docker stats`,
   `pidstat`), per-plugin cost (AMXX profiler) and the hot forwards/hookchains
   before touching code.
2. **Prefer ReAPI hookchains over Fakemeta.** `RegisterHookChain(RG_*)` is
   cheaper and more targeted than `register_forward(FM_*)`; keep Fakemeta only
   where ReAPI has no equivalent. This applies to code and plugins we control.
   The Fakemeta *module* stays shipped: third-party plugins declare
   `#include <fakemeta>` and refuse to load without it, and AMXX only loads a
   module when a plugin needs its natives. Per event, exactly one source owns
   the hook (Fakemeta or ReAPI), otherwise both run.
3. **Move hot paths into a native Metamod plugin.** C++ for per-frame or
   per-player work, AMXX for gameplay glue and the public native surface.
4. **Avoid or batch per-frame hooks.** Throttle to every N frames where
   semantics allow, aggregate work, and keep string formatting out of hot paths.

These need a gate entry in `docs/plan/csretro-plan.md` (working name G06c) before
implementation; the plan is the only source for the gate list.

## Open / known broken

- **A2S under Default Public:** open decision for G07. Either adapt
  `ISteamGameServer011` to the 015 interface or write a dedicated query
  responder.
- **Client engine decided (Option A):** Xash3D runs Valve's `client.so` through
  `HUD_Init` with a stub surface; the open work is a real `VGUI_Surface026`
  backend (drawing, fonts, textures, mouse) and the `engine_mini`/`nitro_api`
  hooks ported off `hw.so`. Retail GoldSrc remains the working fallback.
- **Steam stub is test-only.** It authenticates nobody and must never be used
  for a public server. A production server needs real Steam connectivity.
- **E2E is not in CI yet.** The Docker job arrives with G10; the test needs
  network access for App 90.
- **Runtime and toolchain are downloaded, not vendored.** `.cache/` is ignored
  and removed by `scripts/clean.sh`; the first run after a clean needs network.
- **Vendored identity strings:** NextClient and the server API still carry
  upstream Sentry, GameAnalytics, updater and identity strings. Removed in G08.
- **Upstream patches:** `server/api/deps/rsa_keygen/` needs two portability
  fixes, recorded in `patches/README.md`.
- **Path with spaces:** all builds go through `/tmp/csretro-src`. Keep using the
  scripts.
- **Branch protection** is unavailable on a private free-plan repository. The
  tracked `pre-push` hook and CI are the two checks.

## Next task

- **Gate:** G07 – Default Public runtime, in progress.
- **Engine decided:** Option A from `docs/plan/csretro-plan.md` section 3.6.
- **Next step:** turn the stub `VGUI_Surface026` into a real Xash3D backend, in
  this order: (1) screen size, proportional base and text metrics; (2) filled
  rects, lines and text through Xash3D's engine draw calls (FreeVGUI's
  `platform/xash3d-fwgs/surface.cpp` is the reference); (3) textures and fonts;
  (4) mouse/cursor and `VGUI_Input004`. Re-run the spike after each step and
  record the first thing that is actually visible.
- **Then:** port `engine_mini`/`nitro_api` off the `hw.so` hooks
  (`vgui2_surface` and friends) onto an Xash3D engine adapter, and build
  `client_mini` + `vgui2` from the vendored `ncl-hl1-source-sdk` as one matched
  pair.
- **After the decision:** schedule the server performance backlog above
  (measurement first, then hookchain/native/per-frame work). It needs a plan gate
  entry.
- **Dependency backlog:** vcpkg baseline bump (`b5d1a94` → current) as its own
  gate with a full rebuild and test run.

## Open decisions

1. Windows support: drop the CEF/browser code or keep it disabled. Decided in G08.
2. Discord app, Sentry project, GameAnalytics keys, update signing key and
   hosting are needed for G12/G13.
3. A2S under Default Public: 011→015 adapter versus own query responder (G07).

## Environment

| Item | Value |
|---|---|
| Host | Linux x86_64, CachyOS |
| glibc | 2.44 |
| GCC | 16.2.1 |
| CMake | 4.4.5 |
| Ninja | 1.13.2 |
| Python | 3.14.9 |
| Git LFS | 3.8.1 |
| Docker | 29.9.3 |
| Docker Compose | 5.7.0 |
| Multilib | works (`-m32`) |
| Steam | installed, Default Public 1.1.2.7 |
| Build symlink | `/tmp/csretro-src` → repository |
| Server toolchain | AMXX `1.10.0.5487`, ReAPI `5.29.0.358` in `.cache/toolchain/` |
| Server runtime | ReHLDS `3.15.0.896`, ReGameDLL `5.30.0.814`, Metamod-R `1.3.0.149` in `.cache/downloads/` |

The retail game lives in the maintainer's Steam library. It is **not** part of the
repository and must never be committed. CSRETRO is always installed into a separate
copy outside the Steam library.

## Last commands and results

```text
$ tests/e2e/run.sh
==> staging runtime archives, module, plugin and test assets
==> assembling the Docker build context in dist/testserver-context
==> building and starting the test server on udp/27015
==> collecting the server log
==> running protocol and log checks
ok   log: Metamod-r v1.3.0.149
ok   log: AMX Mod X version 1.10.0.5487
ok   log: [nclapi] Next Client API Successfully loaded, version 1.6.0
ok   log: Started map "de_dust2"
ok   rcon: "nclapi_version" is "1.6.0"
ok   rcon: amxx plugins: Test NCL API running
all e2e checks passed
==> e2e passed
```

G06b radar and verification (2026-11-09, local):

```text
$ scripts/sync-upstream.sh
nextclient           90b4bf7 -> 24cc986  fix Linux maps parsing for paths with spaces
nextclient_server_api 1da463e -> 1da463e  up to date
ncl_hl1_source_sdk   0e9be00 -> 0e9be00  up to date
vcpkg                b5d1a94 -> ec48c11  (2919 commits, deferred)

$ git subtree pull --prefix=client … main
Merge made by the 'ort' strategy.  (bcd2e79)

$ ctest -C Release -R LinuxModulePath
2/2 tests passed

$ scripts/build-client.sh
exit 0, 211 s, 8 ELF32 artifacts
```

Xash3D 32-bit spike (2026-11-09, local):

```text
$ ./xash3d -game cstrike -console +quit   (LD_PRELOAD=kvshim.so, Xvfb, llvmpipe)
[kvshim] intercepted client F at 0xceca9530
[kvshim] client Initialize returned 1
[kvshim] VGUI_Surface026=(nil)
[kvshim] VGUI_InternalLoadInterfaces not exported
[kvshim] viewport Initialize done, field48=(nil)
[kvshim] fake SteamAPI_Init
[kvshim] viewport Start done, field48=0x598d4b30
... execing config.cfg
Issuing host shutdown due to reason "command"
exit 0, no crash_backtrace.txt
```

Valve's `vgui2.so` exports the init entry point under the mangling
`…PPFPvPKcPiEi`, not NextClient's `…PPFP14IBaseInterfacePKcPiEi`; the shim
resolves both. The surface stub still renders nothing.

## Gate log

| Gate | State | Evidence |
|---|---|---|
| G01 | done | commit `b50197c`, CI run `42479518181` |
| G02 | done | subtree `3fea0c3`, `501d93b`, verify green |
| G03 | done | doctor 40/0, vcpkg bootstrapped, prerequisites documented |
| G04 | done | 8 ELF32 artifacts, manifest, build script, CI run `42482462774` |
| G05 | done | module ELF32 + metadata, 34/34 tests, build script, `patches/` |
| G06 | done | test server `healthy`, module and plugin loaded, e2e green, CI run `42514310451` |
| G06b | done | NextClient `24cc986` imported (`bcd2e79`), 2/2 path tests, client rebuild green, vcpkg deferred, CI run `42579531440` |
| G07 | in progress | G07a: SHA-256 build registry, 7 registry tests. G07b: three-stage resolver, 327 engine + 81 client entries, 70/70 `nitro_api-tests`, CI `42590218846`. G07c: provider check tool, 0 offsets, 0 unverified names. G07d: loader fix `79ba393` (global Steam proxy, SDK tier0), CI `42595518318`, maintainer rerun pending. Xash3D client spike: Xash3D-FWGS `e9b94dc`, headless dedicated and ReHLDS e2e green; 32-bit Xash3D runs Valve's `client.so` + `vgui2.so` through `HUD_Init` with a stub `VGUI_Surface026`, engine direction Option A (plan section 3.6) |
