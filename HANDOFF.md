# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-10
- **Gate:** G07 – Default Public runtime, **in progress** (G07a–G07c done, G07d fixed; the Xash3D client spike completes `HUD_Init` and paints real VGUI2 text in every frame; engine direction decided: Option A)
- **Branch:** `main`
- **Commit:** `a51ef3e` (steady-state VGUI2 text evidence) plus this handoff update on top
- **CI:** https://github.com/benjarogit/csretro/actions/runs/42821354428 – success (`a51ef3e`)
- **Version:** `0.1.0-dev` (`VERSION`)

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
| The VGUI_Surface026 backend paints real VGUI2 trees | `paint_tree` traverses the scoreboard dialog (25 children, nested) and draws its rects through Xash3D's `pfnFillRGBA` (12 `fill_rect` calls per paint, constant); no crash; IPanel slot mapping verified against `vgui2.so` VPanelWrapper vtable (GetSize=8, GetAbsPos=13, IsVisible=17, GetChildCount=18, GetChild=19, GetName=39, GetClassName=40, PaintTraverse=44) |
| The VGUI_Surface026 backend paints real VGUI2 **text** into a rendered frame | `docs/evidence/g07-xash3d-vgui2-text.png`: `Spectators - 0 players`, `benny`, `Score`, `Deaths`, `Latency` over the lit `de_dust2` scene; `glReadPixels` before/after the HUD paint = 1755 changed pixels, bbox screen rows 5..23, columns 56..810, `glGetError()` 0, identical on a second run |
| The VGUI2 text overlay is **steady state**, not a single frame | `docs/evidence/g07-xash3d-vgui2-text-steady.png`: the back-buffer diff around the HUD paint is 1755 px, rows 5..23, columns 56..810 at paint 1 *and* at paint 60; the patched stub emits 50 surface text calls per paint (unpatched: 50 in paint 1, then 0), constant; gdb watchpoint traced the disappearing images to `Panel::InvalidateLayout(true,true)` -> `PerformApplySchemeSettings` -> `SectionedListPanelHeader::ApplySchemeSettings` -> `ClearImages()` |
| The VGUI_Surface026 backend paints **filled panels** with text | `docs/evidence/g07-xash3d-vgui2-filled-panel.png` / `-zoom.png` (paint 20): the HUD paint changes 499 200 px = exactly 832x600, rows 0..599, columns 0..831, `glGetError()` 0; pixel (400,300) `32,30,26` -> `220,142,26` shows the translucent scheme fill `188,112,0` over the scene with `Score`/`Deaths`/`Latency` text; 12 `fill_rect` calls per paint, constant; paint 2 and paint 32 bit-identical |
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
- **Next step:** the stub `VGUI_Surface026` backend traverses and paints the
  real VGUI2 panel tree, draws real VGUI2 text in every frame and fills the
  panel backgrounds with the scheme colour (evidence above). The earlier
  `fill_rect` colour/alpha suspicion is disproved: marshalling and alpha
  inversion are correct, the "38-row band" was an artefact of dumping only
  paint 1 (scheme not yet applied, VGUI alpha 0 = transparent). Open work, in
  order:
  (1) header bar of the scoreboard: it still uses the default white scheme
  colour, so its white label has no contrast and `Spectators - 0 players` is
  unreadable while the column headers are fine; (2) textures and
  fonts; (3) mouse/cursor and `VGUI_Input004`; (4)
  `engine_mini`/`nitro_api` port onto an Xash3D adapter. Re-run the spike after
  each step and record the first thing that is actually visible.
- **Wiki reference gathered:** Tier1, VGUI Documentation, Category:VGUI (54
  pages + 3 subcategories), KeyValues, Panel, RootPanel, VPANEL, Frame. Key
  findings: `PaintBackground()`/`Paint()` are on by default; `IsFullyVisible()`
  checks parents; `ShouldPaintChildPanel()` gates child painting; `PaintTraverse`
  promotes `repaint` only when `+0x30` (NEEDS_REPAINT) is set and `allowForce`
  is true; `surface()->PushMakeCurrent`/`PopMakeCurrent` bracket every paint;
  VGUI alpha 0 = opaque (engine 255 = opaque).
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

## Xash3D spike update (2026-11-10)

- Engine text path proven per-frame: with `GLTRACE_SWAPPAINT=1 GLTRACE_ENGDRAW=1`, the swap hook forces a 1024x768 ortho and calls `pfnDrawString`/`pfnDrawConsoleString` every frame; screenshot `/tmp/opencode/vgui_text2.png` shows yellow `ENGINEHELLO`, cyan `VGUI2 TEXT OK` and orange `CONSOLEHELLO` persisting across frames (95 yellow / 76 cyan pixels sampled).
- Real surface path was text-less for a *drawing-order* reason, not a coordinate
  or font reason; see the section below.

## Xash3D spike update (2026-11-10, real VGUI2 text on screen)

**Result: the stub `VGUI_Surface026` now paints real VGUI2 text into a rendered
Xash3D frame.** The frame shows the scoreboard dialog's own strings –
`Spectators - 0 players`, the local player name `benny`, and the section
headers `Score`, `Deaths`, `Latency` – in the dialog's colour, at the dialog's
own top-down coordinates, over the lit `de_dust2` scene.

![G07 Xash3D VGUI2 text](docs/evidence/g07-xash3d-vgui2-text.png)

![G07 Xash3D VGUI2 text, zoom](docs/evidence/g07-xash3d-vgui2-text-zoom.png)

Root cause chain, each step proven by a measurement:

1. **The stub is called twice per frame and only one call is visible.**
   `kvshim.so` hooks both client exports: `frame_wrap` (the client `Frame`
   export, called from `Host_ClientFrame`, i.e. *before* `R_RenderScene`) and
   `redraw_wrap` (`HUD_Redraw`, called from `CL_DrawHUD`, i.e. *after*
   `R_RenderScene` and inside the engine 2D block). Confirmed with gdb: every
   `frame_wrap` paint has `scene=0`, every `redraw_wrap` paint has `scene=1`
   and `R_Set2DMode` is already active (`flag2d=1`). A `frame_wrap` paint is
   silently overwritten by the 3D scene.
2. **VGUI emits its text draw calls only on the first paint of the process.**
   `CSectionHeader::Paint`/`Label::Paint` run every frame (gdb counter), but
   `DrawPrintText` reaches the stub only on paint 1. Because `frame_wrap` runs
   first in each frame, it consumed that single emission and the visible
   `redraw_wrap` paint had nothing left to draw.
3. **Disabling the `frame_wrap` paint makes the `redraw_wrap` paint the first
   paint**, so the real VGUI2 text lands after the world is rendered.
   Spike shim `hudonly.so` patches kvshim `frame_wrap` at `+0x12ce` from
   `mov eax,[ebx+0x110]` to `xor eax,eax` (the following `test/je` then skips
   the paint call). No engine or stub change is involved.
4. **Coordinates, projection, depth and scissor are all correct.** GL state
   dumped inside the HUD paint: viewport `0,0 1024x768`, scissor box
   `0,0 1024x768` with `GL_SCISSOR_TEST` disabled, `GL_DEPTH_TEST` disabled,
   projection `[0.0020 0 0 -1 | 0 -0.0026 0 1]`, i.e. a full-screen top-down
   ortho for `0..1024 x 0..768`. The glyph quads arrive from the stub as
   `x=623 y=3 w=7 h=19` etc. with the dialog colour `0xffff9d48` (alpha 255).

**Verification (reproducible, twice, identical numbers):** the spike shim
`huddump.so` reads the back buffer with `glReadPixels(GL_BACK)` immediately
before and after the first HUD paint.

```text
call 1 changed px 1755
bbox (GL bottom-up) y 744..762  x 56..810
=> screen rows 5..23, columns 56..810
call 2 changed px 0
```

1755 changed pixels inside exactly the rows and columns the stub reported,
zero changed pixels on the next paint. `glGetError()` is `0` throughout.
Because the text is emitted only in that one paint, an X11 front-buffer
screenshot misses the frame; the frame image above is the engine's own back
buffer, read at the paint.

Reproduce (all paths outside the repo, spike tree only):

```text
LD_PRELOAD="<spike>/kvshim.so <spike>/huddump.so <spike>/hudonly.so" \
LD_LIBRARY_PATH="<spike>:<sdl32>:/usr/lib32" \
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
./xash3d -game cstrike +map de_dust2 +showscores
```

## Xash3D spike update (2026-11-10, steady-state VGUI2 text)

**Result: the same real VGUI2 strings now paint in every frame, not just the
first one.** The back-buffer diff around the HUD paint is bit-identical at
paint 1 and at paint 60: 1755 changed pixels, screen rows 5..23, columns
56..810.

![G07 Xash3D VGUI2 text, steady state](docs/evidence/g07-xash3d-vgui2-text-steady.png)

Root cause, proven with a gdb watchpoint on the label's image vector:

1. `stub_surface_paint` increments a per-process paint counter
   (`[GOT+0x2b8]`) and then calls the paint-root helper at stub offset
   `0x1b32`. That helper called
   `vgui2::Panel::InvalidateLayout(true, true)` on the root panel **every
   frame** (`stub_surface.so +0x1b9e -> +0x1afd`, vtable slot `0xf4`, which is
   `Panel::InvalidateLayout`; the object vptr points past the two Itanium
   header words, so the byte offset is `0xf4`, not `0xec`).
2. `InvalidateLayout(layoutNow, reloadScheme=true)` recurses into every child
   and ends in `Panel::PerformApplySchemeSettings` ->
   `SectionedListPanelHeader::ApplySchemeSettings` -> `Label::ApplySchemeSettings`
   (`Label.cpp:1001`), which clears `_imageDar` entries. `SectionedListPanelHeader`
   calls `ClearImages()` explicitly; the header rebuilds its per-column text
   images only in `PerformLayout`.
3. Watchpoint evidence at paint 2:
   `#0 Label::ApplySchemeSettings` <- `#1 CSectionHeader::ApplySchemeSettings`
   <- `#2 Panel::PerformApplySchemeSettings` <- `#3/#4/#5 Panel::InvalidateLayout`
   <- `#6 paint_tree` (stub). The label vector's first entry is written to
   `NULL` there.
4. Therefore the first paint draws text and every later paint has an empty
   image list.

Fix, spike-only binary patch (no engine or client change): allow the first
`InvalidateLayout(true, true)` call, skip it from paint 2 on. The call site
`0x1b9a` jumps into a code cave at `0x1be0` that compares the paint counter
with `1`:

```text
0x1b9a  e9 41 00 00 00      jmp 0x1be0
0x1be0  8b 83 b8 02 00 00   mov 0x2b8(%ebx),%eax
0x1be6  83 f8 01            cmp $1,%eax
0x1be9  75 0c               jne 0x1bf7
0x1beb  83 ec 0c            sub $0xc,%esp
0x1bee  51                  push %ecx
0x1bef  e8 09 ff ff ff      call 0x1afd
0x1bf4  83 c4 10            add $0x10,%esp
0x1bf7  e9 aa ff ff ff      jmp 0x1ba6
```

`spike-shims/patch_once.py` in the spike tree reproduces the patched
`stub_surface.once.so` byte for byte from `stub_surface.nofill.so`.

Verification: the patched stub emits 50 surface text calls per paint (instead
of 50 only in paint 1 and 0 afterwards), constant across paints, and
`huddump2.so` (`HUDDUMP_CALL=<n>`) reads the back buffer immediately before and
after the HUD paint of paint `n`:

```text
paint  1: changed=1755 rows_screen=5..23 cols=56..810
paint 60: changed=1755 rows_screen=5..23 cols=56..810
glGetError() 0
```

Remaining defects, each with its own next task:

- **`fill_rect` is not a colour/alpha defect.** The old entry claimed the stub
  turns `set_color 255,255,255,128` into `rgba=188,112,0,128`. That is wrong:
  the log line reads the colour struct *after* the engine call, and gdb at
  `fill_rect+0x79` shows `x,y,w,h,r,g,b,a` marshalled correctly with the alpha
  inversion (`not`/`movzbl`, VGUI 0 = opaque). The "38-row band" was an
  artefact of dumping paint 1 only: in paint 1 the stub still emits its fills
  as `255,255,255` with VGUI alpha 0 (engine alpha 0 = transparent) and the
  scheme `188,112,0`/128 only takes effect from paint 2. A direct
  `CL_FillRGBA` call after the paint fills the full rect, so engine and ref are
  fine. Evidence: `docs/evidence/g07-xash3d-vgui2-filled-panel.png`.
- **Scoreboard header bar.** The `0,0 400x38` header background keeps the
  default white scheme colour (alpha 127). The verified draw order per paint is
  header bar, then the ten panel fills, then 18 text calls, so the white label
  is drawn *on top* of the bar and simply has no contrast on it:
  `Spectators - 0 players` is unreadable while `Score`, `Deaths`, `Latency`
  render cleanly outside the bar. Next: check that panel's scheme colour
  against the retail client.
- **Font atlas.** `cls.creditsFont` (texture 37) is used by `pfnDrawString`;
  `gfx/creditsfont.fnt` does not exist in the spike tree, yet texture 37 is
  valid and draws. The steady-state patch did not change this; leave it alone
  until filled panels render.
- **Stale assumption to drop:** the y-band search (y=3 vs y=751) that ran
  before this finding was a symptom of the drawing order, not of a clip
  rectangle. Text renders at its true coordinates.

## Xash3D spike update (2026-11-10, filled panels)

**Result: the stub paints the scoreboard's filled panels and its text in the
same frame.** The HUD paint of paint 20 changes 499 200 pixels, exactly
832 × 600, screen rows 0..599 and columns 0..831; pixel (400,300) goes from
`32,30,26` to `220,142,26`, i.e. the scheme colour `188,112,0` blended at 50 %
over the scene, with the `Score`/`Deaths`/`Latency` labels on top.

![G07 Xash3D VGUI2 filled panel](docs/evidence/g07-xash3d-vgui2-filled-panel.png)

![G07 Xash3D VGUI2 filled panel, zoom](docs/evidence/g07-xash3d-vgui2-filled-panel-zoom.png)

How the earlier "fills are clipped to 38 rows" reading was produced and
refuted:

1. The old dumps were taken at paint 1 only. In paint 1 the stub emits ten
   panel fills as `255,255,255` with engine alpha 0, i.e. fully transparent,
   plus one header rect at alpha 127. Paint 1 therefore really does show only
   that one rect, and the diff bounded it to rows 0..37, columns 0..399.
2. From paint 2 on, the scheme takes effect: ten `188,112,0` fills at alpha 127
   cover `811x599` at `10,0` plus borders, then `0,0 822x33` and `0,0 400x38`.
   The per-fill back-buffer dumps show every rect landing at its exact
   coordinates and accumulating.
3. Cross-check: calling `libxash.so` `CL_FillRGBA` (`0xfc270`) directly after
   the paint with `10,0 811x599 188,112,0,127` fills 485 789 px (rows 0..598,
   columns 10..820). The engine path was never broken.
4. gdb at `fill_rect+0x79` (`0x17b9`) prints the eight pushed arguments and they
   are correct; `R_FillRGBA` in `libref_gl.so` receives them as floats and
   draws a quad with the right vertices. The `[surface] fill` log line prints
   the colour struct *after* the engine call and must not be used as evidence.

Steady state: 12 `fill_rect` calls and 50 text calls per paint, constant;
paint 2 versus paint 32 is bit-identical, paint 32 versus 62 differs in 444
pixels only (animated scene behind the translucent panel). The draw order
inside one paint is header bar `0,0 400x38`, then the ten panel fills, then 18
text calls, so all text lands on top of the fills.

CI: run `42849944792` green.

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
| G07 | in progress | G07a: SHA-256 build registry, 7 registry tests. G07b: three-stage resolver, 327 engine + 81 client entries, 70/70 `nitro_api-tests`, CI `42590218846`. G07c: provider check tool, 0 offsets, 0 unverified names. G07d: loader fix `79ba393` (global Steam proxy, SDK tier0), CI `42595518318`, maintainer rerun pending. Xash3D client spike: Xash3D-FWGS `e9b94dc`, headless dedicated and ReHLDS e2e green; 32-bit Xash3D runs Valve's `client.so` + `vgui2.so` through `HUD_Init` with a stub `VGUI_Surface026`; the stub paints real VGUI2 panel trees, real VGUI2 text and the scheme-coloured panel fills into a rendered frame in **every** frame (`docs/evidence/g07-xash3d-vgui2-text.png`, `-steady.png`, `-filled-panel.png`; 1755 px text diff at paint 1 and paint 60, 499 200 px = 832x600 fill at paint 20, 12 fills and 50 text calls per paint); engine direction Option A (plan section 3.6) |
