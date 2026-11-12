# Handoff

Project state for the next session. Personal context belongs in
`.local/handoff.local.md` and never here.

## State

- **Date:** 2026-11-12
- **Gate:** G07 – Default Public runtime, **in progress** (G07a–G07c done, G07d fixed; the Xash3D spike proved text, fills, textures, layout, fonts and a team-menu click; engine direction Option A; `xash_surface.so` is a host-factory `VGUI_Surface026` with fills, texture quads, a FreeType glyph atlas, a panel solve/paint entry, mouse dispatch and an `xash_host` factory; the Xash client does not start a Steam process, original game content stays required; local identity is one lookup, `player_identity-tests` 6/6)
- **Branch:** `main`
- **Commit:** `57e5e7e` (`OnGameInitializing` stores a process engine table, then `Con_Init` and `SetCStrikeFlags` run; `NET_SendPacket` is the process function and is not called; no socket is opened; `hw.so` stays unmapped)
- **CI:** https://github.com/benjarogit/csretro/actions/runs/43200642517 – success (`57e5e7e`)
- **Pages:** https://github.com/benjarogit/csretro/actions/runs/43183431937 – success; site https://benjarogit.github.io/csretro/ uses the Zensical default template
- **Version:** `1.0.0-dev` (`VERSION`; first release tag `v1.0.0` only at G14 when playable)
- **Repo visibility:** `public` (`versions.lock.json`); collaboration, Page, SemVer and upstream policy live in `docs/plan/csretro-plan.md` §11

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
| The VGUI_Surface026 backend paints real VGUI2 trees | `paint_tree` traverses the scoreboard dialog (25 children, nested) and draws its rects through Xash3D's `pfnFillRGBA` (12 `fill_rect` calls per paint, constant); no crash; IPanel slots read straight out of `_ZTV13VPanelWrapper` (`vgui2.so` 0x2a0b0, address point 0x2a0b8): SetPos=3, GetPos=4, SetSize=5, GetSize=6, GetAbsPos=11, SetInset=13, IsVisible=16, GetChildCount=18, GetChild=19, GetName=37, GetClassName=38, PaintTraverse=42 |
| The VGUI_Surface026 backend paints real VGUI2 **text** into a rendered frame | `docs/evidence/g07-xash3d-vgui2-text.png`: `Spectators - 0 players`, `benny`, `Score`, `Deaths`, `Latency` over the lit `de_dust2` scene; `glReadPixels` before/after the HUD paint = 1755 changed pixels, bbox screen rows 5..23, columns 56..810, `glGetError()` 0, identical on a second run |
| The VGUI2 text overlay is **steady state**, not a single frame | `docs/evidence/g07-xash3d-vgui2-text-steady.png`: the back-buffer diff around the HUD paint is 1755 px, rows 5..23, columns 56..810 at paint 1 *and* at paint 60; the patched stub emits 50 surface text calls per paint (unpatched: 50 in paint 1, then 0), constant; gdb watchpoint traced the disappearing images to `Panel::InvalidateLayout(true,true)` -> `PerformApplySchemeSettings` -> `SectionedListPanelHeader::ApplySchemeSettings` -> `ClearImages()` |
| The VGUI_Surface026 backend paints **filled panels** with text | `docs/evidence/g07-xash3d-vgui2-filled-panel.png` / `-zoom.png` (paint 20): the HUD paint changes 499 200 px = exactly 832x600, rows 0..599, columns 0..831, `glGetError()` 0; pixel (400,300) `32,30,26` -> `220,142,26` shows the translucent scheme fill `188,112,0` over the scene with `Score`/`Deaths`/`Latency` text; 12 `fill_rect` calls per paint, constant; paint 2 and paint 32 bit-identical |
| Every scoreboard fill is traced to its VGUI caller | gdb backtrace at each of the 12 `fill_rect` calls of paint 2: seven rects from `CCSClientScoreBoardDialog::PaintBackground`, `832x600` and `400x38` from `vgui2::Panel::PaintBackground` (`Panel.cpp:556`), two `1,21 819x1` from `vgui2::CSectionHeader::Paint` and `822x33` from `vgui2::CItemButton::PaintBackground`; the white `400x38` is the default `255,255,255,128` background of the label `ServerName` |
| The stock stub has no texture path | the four texture slots are one-instruction `ret`s and `DrawGetTextureSize` always returns `16x16`; VGUI asks for the eight `gfx/vgui/round_corner*` files per paint through `vgui2::Bitmap::Paint` and every `DrawTexturedRect` arrives as `0,0 10,10` because `PushMakeCurrent` is a no-op |
| The VGUI_Surface026 backend paints **textured quads** | `docs/evidence/g07-xash3d-vgui2-texture-corner.png` / `-zoom.png`: 8 TGAs loaded per paint (ids 11–14 64x64, 16–19 10x10), 8 `DrawTexturedRect` at `0,0 10,10`, paint 40 vs 41 differs in 27 px in `x 0..9, y 0..9` (`(0,9)` `255,255,255` → `255,200,32`), `glGetError()` 0; `docs/evidence/g07-xash3d-vgui2-texture-sampled.png` / `-zoom.png`: a diagnostic 256x256 blit of `round_corner_nw` reproduces the TGA's quarter-circle alpha mask with the scene visible through the transparent part |
| Ending a textured quad with `glDisable(GL_TEXTURE_2D)` white-washes the frame | separate frames with the real draw vs. no draw show that only the trailing state teardown breaks it; six probe stages (enable/disable, colour, bind, empty `glBegin`, immediate quad, vertex arrays) all keep the frame intact; saving/restoring `GL_TEXTURE_2D` + binding + blend + depth + cull + colour fixes it |
| VGUI2 fonts collapse to one engine font | 14 s spike run: 36 `CreateFont` calls all return `1`, 54 `AddGlyphSetToFont` calls all carry `font=1` and request 27 distinct (name, tall, weight) triples (Verdana 12/20, Verdana Bold 18/38, Tahoma 12–55, Arial 13–55, Marlett 11/23), 42 089 `GetFontTall` calls all return `16`, `GetFontAscent` and `AddCustomFontFile` are never called; the `16` is the stub's one global tall from measuring `"Wg."` with `DrawConsoleStringLen` |
| The scoreboard renders at its real layout position | with push/pop translation on all three primitive families, `PushMakeCurrent` reports `96,84` for the scoreboard dialog, the four corner textures land on `96,84`, `917,84`, `96,673`, `917,673`, and paints 40 vs. 41 differ in 108 px inside `96..926, 84..682`; `docs/evidence/g07-xash3d-vgui2-laid-out.png`, `-header.png`, `-corner.png` show the filled panel, the rounded corners, the white `ServerName` bar and the right-aligned `Score`/`Deaths`/`Latency` |
| The panel offset comes from `VPanel::GetAbsPos`, slot 19 | the `VPANEL` passed to `PushMakeCurrent` is a `vgui2::VPanel`; `_ZTVN5vgui26VPanelE` (`vgui2.so 0x29fb0`, address point `0x29fb8`) has `VPanel::GetAbsPos(int&, int&)` (`0x17200`) at slot 19, while slot 11 is `GetAbsPos` only in the engine-side `VPanelWrapper` vtable |
| VGUI screen size must match the framebuffer | `get_screen_size` forwards `funcs+0x30` (virtual, `this` plus two `int*`); a 1280x1024 Xvfb with a 1024x768 buffer made VGUI lay out for 1280x1024 (dialog `1098x714` at `85,149`, corners at `1183,863`, outside the buffer); with a 1024x768 Xvfb the same run reports `1024x768` and the dialog sits at `96,84` |

| VGUI2 gets real fonts: 36 handles, 15 heights, glyph quads | font slots are patched as soon as the stub is mapped (VGUI creates scheme fonts in `HUD_Init`, before the first paint; late patching sees 0 `CreateFont` calls): 36 `CreateFont` calls return handles `1`..`36`, 54 `AddGlyphSetToFont` calls carry 36 primary sets plus 18 charset fallbacks (first set defines face and height), the primary heights are `11,12,14,17,18,19,20,22,24,26,28,32,35,38,41` instead of one `16`, `GetCharacterWidth` is answered from FreeType (4611 calls by paint 41), `GetTextSize`/`GetFontAscent` are never called; text goes through the glyph backend (50 calls and 42 glyph quads per paint, 21 glyph textures, 0 engine fallbacks) || Valve's `client.so` requires Valve's `vgui2.so` | pairing it with NextClient's `vgui2.so` crashes in `Label::ApplySettings` (`pScheme->GetFont`, slot 6 → type_info vtable); the interface implementations live in `vgui2.so`, not `client.so` |
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
  into Steam and never asks for credentials. `scripts/install-content.sh`
  discovers the local Steam libraries (`steamapps/libraryfolders.vdf`,
  `appmanifest_10.acf`) and copies the allowlist in `tools/install_content.py`
  into a directory outside that library. Files shipped in `client/assets`
  overlay that copy: the same relative path keeps the shipped file, and a
  retail file with no shipped replacement stays. A missing Steam install or a
  missing game fails. Tests use a fake library. The vendored NextClient installer
  `client/installer/import_game.py` is not this copy.
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
- **Decided:** the Xash client does not start a Steam process and does not
  need one for auth. Original game content stays required. The vendored
  GoldSrc launcher still starts through the Steam runtime until the Xash host
  replaces it.

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

- **Actions run 43095634906 never started.** The push of `dfe17cc` created a run
  that stays `queued` with no job. The API refuses to cancel it. That run holds
  concurrency group `ci-refs/heads/main`. The workflow group is now `verify-`
  plus the ref. That change is `52b3e2e`, and its run is
  https://github.com/benjarogit/csretro/actions/runs/43160802936.
- **A2S under Default Public:** open decision for G07. Either adapt
  `ISteamGameServer011` to the 015 interface or write a dedicated query
  responder.
- **Client engine decided (Option A):** Xash3D runs Valve's `client.so` through
  `HUD_Init` with a stub surface. The product surface is now `xash_surface.so`
  (plan section 3.7): the host factory exposes one `VGUI_Surface026`, and it is
  not linked into `vgui2.so`. The font atlas is in: FreeType metrics, one packed
  atlas, Liberation Sans for Arial and DejaVu Sans for Verdana and Tahoma.
  Marlett is not shipped; the host passes it through `resolve_font`.
  `SolveTraverse` walks the visible tree and `PaintTraverse` enters the
  embedded panel, then popups. Mouse move, press and release go through
  `csr_surface_input_fns_t` and each event is followed by one dispatch.
  `RunFrame` stays empty. `xash_host.so` loads the surface with `RTLD_LOCAL`
  and publishes it as the first `VGUI_Surface026` factory. Retail `vgui2`
  keeps `DispatchMessages` local, so our `vgui2` exports
  `csr_vgui_dispatch_messages`. The Xash client does not start a Steam
  process; original maps, models, sounds and `valve/resource` stay required.
  Product process is x86_64 (plan section 3.7). The Linux client preset is
  `linux` with the vcpkg triplet `x64-linux`. There is no second 32-bit client
  preset. `engine_mini-tests` (`XashStartup` 6/6; full run 561), `nitro_api-tests` (70),
  `xash_surface-tests` (21), `xash_host-tests` (20), `xash_engine-tests` (6),
  `sdk_pair-tests` (4), `player_identity-tests` (6), `steam_api-tests` (2) and
  `xash_launcher-tests` (4)
  pass as ELF64 binaries
  and do not load Valve's
  i386 `hw.so`, `client.so`
  or `vgui2.so`. Product identity is `xash`. `NitroApi::Initialize` does not
  open, hash, or hook `hw.so`. `nitro_api-tests` 70/70, including
  `XashIdentity.InitializeDoesNotOpenOrHashTheRetailEngine`. `engine_mini-tests`
  filter `XashStartup` is 6/6. Full `engine_mini-tests` is 561/561. Missing engine addresses are not hooks and are not called.
  `xash_engine` forwards `vgui2_surface`, texture reset and text color to that
  one surface. `VGUI2_Draw_Init` has no entry there. The same library takes
  `csr_xash_engine_argv`, `csr_xash_engine_mode`, `csr_xash_engine_init_game`,
  `csr_xash_engine_shutdown` and `csr_xash_engine_error`. `EngineMini::Init`
  binds each one only when that retail hook is absent, and records that the
  engine is Xash. `COM_InitArgv` runs. Mode and init run. `OnGameInitializing`
  reads a pointer table from this process and does not call `Sys_Error`.
  `Con_Init` then runs. It reads `-condebug` through `COM_CheckParm` and does
  not call a null filesystem. `pfnGetGameDir` writes the final path component
  of the mounted content root. An empty mount leaves that pointer null, so
  `SetCStrikeFlags` is not called. A mounted name runs `SetCStrikeFlags`.
  `NET_SendPacket` is the function in this process when that retail hook is
  absent. Init stores the pointer and does not call it, so no socket is
  opened. A null pointer is not a call. A null hook is not a call. The next
  retail hook is `eng()->Netchan_CopyFileFragments`. This path returns before
  it. `OnGameInitialized` is not entered.
  The retail hook chain does not run.
  Shutdown and the fatal error stay uncalled on that path. A null function
  is not a call. The call does not open `hw.so` and does not call `RunFrame`.
  The other `engine_mini` forwards still call the retail hook, which stays
  unset. After `IGameUI::Start` the host loads `next_engine_mini.dll` and
  `nitro_api2.dll`, calls `EngineMini::Init`, then argv, mode and init_game.
  A missing library or export fails that load and is not called. A successful
  open does not call shutdown or the fatal error; both stay bound.
  `client_mini` and
  `vgui2` now build from that SDK as ELF64 and link the host runtime.
  `xash_host` opens that pair after the surface factory. A missing library is
  not called. The host does not start the game. `gameui` now builds from the
  same SDK as ELF64 and links the host runtime. The host opens it after the
  surface factory. That load does not call into the game UI.
  `IGameUI::Initialize` follows the mount.   The same factory list
  answers `VFileSystem009` after one root is mounted. A missing file is a miss.
  `DrawSetTextureFile` reads an uncompressed TGA from that root. A bound
  cursor texture is drawn at the end of `PaintTraverse`. `RunFrame` does not
  draw it. `IVGui::Init` then takes that factory list. `vgui2` holds the one
  surface. A scheme file under that root is loaded through the same manager.
  A missing file leaves the host open. `IGameUI::Initialize` builds the base
  panel and parents it to the host root. The server list is a child of that
  panel when `resource/schemes/TrackerScheme_ClassicPlus.res` is on the root.
  `csr_host_paint_gameui` sizes that root and paints the tree. When the scheme
  file is present, that paint reads `resource/BackgroundLayout.txt` and loads
  an extensionless texture name from the `.tga` beside it.
  `csr_host_start_gameui` calls `IGameUI::Start` on a host engine table. It
  does not open `hw.so`. The background image is drawn at time 0. The
  following paint builds the options pages.
  A missing engine module is not called.
  `Start` and `RunFrame` are
  not that call.
  The ELF64 `libsteam_api.so` reports logged out and does not start Steam.
  A missing interface is not called.   The Xash launcher plans that command and the same process opens the list.
  It does not execute the command or call `RunFrame`. A missing content root
  is a failure and leaves the host closed. `nextclient.sh`
  stays the GoldSrc starter.
  Retail GoldSrc remains the working fallback. The server module, its
  `-m32` doctor probe, the goldsrc fact extractor, `golden_layout.cpp`,
  the Windows presets and the G04 ELF32 size table stay 32-bit records.
  `GenericHookJit` does not install on this process.
- **Steam stub is test-only.** It authenticates nobody and must never be used
  for a public server. A production server needs real Steam connectivity.
- **E2E is not in CI yet.** The Docker job arrives with G10; the test needs
  network access for App 90.
- **Server stack is vendored.** Source and the pinned Linux release archives
  live under `server/vendor/`. Fetch scripts read those archives. `.cache/`
  only unpacks them and is removed by `scripts/clean.sh`. SteamCMD still
  downloads Valve App 90; that content is not vendored.
- **HL1 SDK is vendored.** The source, including the LP64 edits, is the
  tracked tree `client/dep/ncl-hl1-source-sdk/`. The rollup patch
  `client/dep/ncl-hl1-source-sdk.patch` is gone. Configure does not apply it.
  This path is not a submodule. `versions.lock.json` records the upstream
  base `0e9be00fc52b66742a09c84c8f6169dc7c83ca8a`. vcpkg stays a submodule.
  Valve game content, the SteamCMD image and the idea-only GPL references
  are not vendored.
- **Vendored identity strings:** NextClient and the server API still carry
  upstream Sentry, GameAnalytics, updater and identity strings. Removed in G08.
- **Upstream patches:** `server/api/deps/rsa_keygen/` needs two portability
  fixes, recorded in `patches/README.md`.
- **Path with spaces:** all builds go through `/tmp/csretro-src`. Keep using the
  scripts.
- **Branch protection** is unavailable on a private free-plan repository. The
  tracked `pre-push` hook and CI are the two checks.

## Plan note (2026-11-12)

Public collaboration, SemVer from `1.0.0`, and upstream policy are in
`docs/plan/csretro-plan.md` §11. `VERSION` is `1.0.0-dev`. No `v1.0.0` tag yet.
The public docs site is live: Zensical default template, audience hubs,
`SECURITY.md`, Pages after green `ci` on `main`, Dependabot for Actions,
weekly upstream-watch. Site: https://benjarogit.github.io/csretro/
Pages source is GitHub Actions. Site: https://benjarogit.github.io/csretro/
(and `/de/`, `/en/`). `main` branch protection requires
`repository verification` and blocks force-push. Dependabot Action bumps are
grouped; the four one-Action PRs were closed as superseded. Benny still flips
the Security overview switches in the browser.

## Next task

- **Gate:** G07 – Default Public runtime, in progress.
- **Engine decided:** Option A from `docs/plan/csretro-plan.md` section 3.6.
- **Install copy:** `scripts/install-content.sh` searches `libraryfolders.vdf`
  and `appmanifest_10.acf`, copies the mounted `cstrike`, `valve` and
  `platform/resource` files, and refuses a destination inside that library.
  Missing Steam is `steam_missing`. Missing App 10 is `game_missing`.
  `hw.so`, Steam binaries, personal configs, demos, screenshots and caches
  stay behind. Shipped files in `client/assets` overlay that copy. The same
  relative path keeps the shipped file (maps, weapons, models, sounds, sprites,
  resources, and any other shipped asset). A retail file with no shipped
  replacement stays. `tests/install_content` 15/15 on a fake library. The game
  was not started. CI https://github.com/benjarogit/csretro/actions/runs/43195336643
  success (`b052069`).
- **Next step:** `xash_surface` answers `VGUI_Surface026` and draws fills,
  texture quads and FreeType glyphs from a packed atlas (plan section 3.7).
  The spike team-click was reproduced on 2026-11-11: exit 137, `Button/terbutton`
  at `239,201`, `CTeamMenu` hidden on release, `g_iTeamNumber` `0` at paint 30
  and `1` at paint 31, both framebuffer dumps `glerr=0`. The Linux client
  preset is `linux` / `x64-linux`. The test binaries above are ELF64.
  Product identity is `xash`. Startup does not open `hw.so`.
  `xash_engine` covers `vgui2_surface`, texture reset and text color, and the
  startup sequence `csr_xash_engine_argv`, `csr_xash_engine_mode`,
  `csr_xash_engine_init_game`, `csr_xash_engine_shutdown` and
  `csr_xash_engine_error`. `engine_mini` binds those when the retail hook is
  absent. `COM_InitArgv` runs. `OnGameInitializing` reads a pointer table from
  this process and does not call `Sys_Error`. `Con_Init` then runs. It reads
  `-condebug` through `COM_CheckParm` and does not call a null filesystem.
  `pfnGetGameDir` writes the mounted directory name. An empty mount leaves
  that pointer null, so `SetCStrikeFlags` is not called. A mounted name runs
  `SetCStrikeFlags`. `NET_SendPacket` is the function in this process when that
  retail hook is absent. Init stores the pointer and does not call it, so no
  socket is opened. A null pointer is not a call. A null hook is not a call.
  The next retail hook is `eng()->Netchan_CopyFileFragments`. This path
  returns before it. `OnGameInitialized` is not entered. The retail hook
  chain does not run.
  `client_mini` and `vgui2` are the matched ELF64 pair and link the host runtime.
  `xash_host` loads that pair. It does not start the game.
  `gameui` is the ELF64 library from the same SDK. The host opens it after the
  surface factory and does not call into it.
  The filesystem proxy answers `VFileSystem009` on that same list. The surface
  stays first. A missing file is a miss. `DrawSetTextureFile` reads an
  uncompressed TGA from that root. A compressed file is not a texture. A bound
  cursor texture is drawn at the end of `PaintTraverse`. `IVGui::Init` takes
  that list after the mount, so `vgui2` holds the one surface. `RunFrame` is
  not that call. A scheme file under the root is loaded there. A missing file
  leaves the host open. `IGameUI::Initialize` parents the base panel to the
  host root. The server list is built from
  `resource/schemes/TrackerScheme_ClassicPlus.res` on that root.
  `csr_host_paint_gameui` sizes that root and paints the tree. When the scheme
  file is present, that paint reads `resource/BackgroundLayout.txt`.
  `csr_host_start_gameui` calls `IGameUI::Start`. `RunFrame` is not that call.
  The ELF64 `libsteam_api.so` reports logged out. It does not start Steam.
  The Xash launcher plans one command and the same process opens that list.
  It does not execute the command or call `RunFrame`. A missing content root
  is a failure. `nextclient.sh` stays the GoldSrc starter.
  Next: that open now loads `engine_mini` and calls argv, mode and init_game.
  It still does not exec Xash3D or call `RunFrame`. The game still does not
  start. `OnGameInitializing` reads a pointer table from this process, not
  from `hw.so`, and does not call `Sys_Error`. `Con_Init` then runs.
  `pfnGetGameDir` writes the mounted directory name and `SetCStrikeFlags`
  runs for that name. An empty mount is not a call. `NET_SendPacket` is the
  function in this process. Init stores that pointer and does not call it.
  A null pointer is not a call. A null hook is not a call. No socket is
  opened. The next retail hook is `eng()->Netchan_CopyFileFragments`. This
  path returns before it. `OnGameInitialized` is not entered. The retail
  hook chain and a real Xash frame still do not run.
  `csr_host_load_pair` binds `VGUI_InputInternal001` (cursor/press/release +
  one dispatch). Paint associates the embedded root with the default input
  context. `HostFactory.GameMenuClickSendsTheCommandWithoutRunFrame` passes:
  a laid-out `QuitGame` item is under the cursor and `QuitNoConfirm` records
  `quit` without running it. `xash_host-tests` HostFactory 20/20. `RunFrame`
  stays unused. The HL1 SDK source is the tracked tree
  `client/dep/ncl-hl1-source-sdk/`, including the LP64 edits. The rollup
  patch is gone. This tree is not a submodule. The upstream base in
  `versions.lock.json` is `0e9be00fc52b66742a09c84c8f6169dc7c83ca8a`.
  Local identity is `ResolvePlayerIdentity` (plan section 3.7).
  `player_identity-tests` 6/6, no network. The lookup does not read Steam.
  Keep the Xvfb screen equal to the game resolution.
- **Wiki reference gathered:** Tier1, VGUI Documentation, Category:VGUI (54
  pages + 3 subcategories), KeyValues, Panel, RootPanel, VPANEL, Frame. Key
  findings: `PaintBackground()`/`Paint()` are on by default; `IsFullyVisible()`
  checks parents; `ShouldPaintChildPanel()` gates child painting; `PaintTraverse`
  promotes `repaint` only when `+0x30` (NEEDS_REPAINT) is set and `allowForce`
  is true; `surface()->PushMakeCurrent`/`PopMakeCurrent` bracket every paint;
  VGUI alpha 0 = opaque (engine 255 = opaque).
- **Then:** the client still does not start and play. `OnGameInitializing`
  returns on a pointer table in this process and does not call `Sys_Error`.
  `Con_Init` then runs: `con_initialized` is set, `-condebug` is read through
  `COM_CheckParm`, and a null filesystem is not a call, so `qconsole.log`
  stays. `pfnGetGameDir` comes from this process. It writes the final path
  component of the mounted content root. An empty mount leaves the pointer
  null, so `SetCStrikeFlags` is not called. A mounted `cstrike` directory
  sets `g_bIsCStrike`. The host loads `next_engine_mini.dll` and
  `nitro_api2.dll` after `IGameUI::Start` and calls argv, mode and init_game.
  `COM_InitArgv` runs. A successful open does not call shutdown or the fatal
  error. The open does not exec Xash3D and does not call `RunFrame`.
  `NET_SendPacket` is the function in this process. Init stores that pointer
  and does not call it. A null pointer is not a call. A null hook is not a
  call. No socket is opened. The next retail hook in `OnGameInitializing` is
  `eng()->Netchan_CopyFileFragments`. The Xash path returns before that
  install. `OnGameInitialized` is not entered. A real GL/Xash frame and
  `CVGui::RunFrame` are still missing. The planned command is not executed.
  The game was not started.
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
| Server toolchain | AMXX `1.10.0.5487`, ReAPI `5.29.0.358` under `server/vendor/` |
| Server runtime | ReHLDS `3.15.0.896`, ReGameDLL `5.30.0.814`, Metamod-R `1.3.0.149` under `server/vendor/` |

The retail game lives in the maintainer's Steam library. It is **not** part of the
repository and must never be committed. CSRETRO is always installed into a separate
copy outside the Steam library.

## Last commands and results

Process `NET_SendPacket` on the Xash path (2026-11-12, preset `linux-tests-release`, ELF64, game not started):

```text
$ engine_mini-tests --gtest_filter=XashStartup.*
6/6 passed
AbsentHooksRecordTheSequenceAndInitArgv
NullBindingIsNotACall
ConInitReadsCondebugAndLeavesTheLog
WiredCallDoesNotOpenHwSo
SetCStrikeFlagsReadsTheMountedDirectory
NetSendPacketUsesTheProcessFunction

$ engine_mini-tests
561/561 passed

$ xash_host-tests
20/20 passed

$ xash_launcher-tests
4/4 passed
```

`OnGameInitializing` stores a pointer table in this process and then calls
`Con_Init`. `pfnGetGameDir` writes the mounted directory name and
`SetCStrikeFlags` runs for that name. An empty mount leaves that pointer
null. `NET_SendPacket` is the function in this process when the retail hook
is absent. Init stores that pointer and does not call it. `net_send_calls`
stays 0. A null pointer leaves the recorded pointer null and is not a call.
A null hook is not a call. The open-socket list in `/proc/self/fd` is
unchanged, so no socket was opened. The next retail hook is
`eng()->Netchan_CopyFileFragments`. That hook stays null. The Xash path
returns before it. `OnGameInitialized` is not entered. `Sys_Error` is not
called. `initializing_calls` is 1. Shutdown and the fatal error stay 0 until
the test calls them. A planted `hw.so` is not mapped. `RunFrame` is not
called. Steam is not started. The planned command is not executed. The game
was not started.

CI: https://github.com/benjarogit/csretro/actions/runs/43200642517 success (`57e5e7e`).

`scripts/verify.sh`: 68 checks passed.

Mounted game directory on the process path (2026-11-12, preset `linux-tests-release`, ELF64, game not started):

```text
$ engine_mini-tests --gtest_filter=XashStartup.*
5/5 passed
AbsentHooksRecordTheSequenceAndInitArgv
NullBindingIsNotACall
ConInitReadsCondebugAndLeavesTheLog
WiredCallDoesNotOpenHwSo
SetCStrikeFlagsReadsTheMountedDirectory

$ engine_mini-tests
560/560 passed

$ xash_host-tests
20/20 passed

$ xash_launcher-tests
4/4 passed
```

`OnGameInitializing` stores a pointer table in this process and then calls
`Con_Init`. `pfnGetGameDir` is `XashGetGameDir` in this process. It writes the
final path component of the mounted content root: a root whose leaf is
`cstrike` writes `cstrike`, and `SetCStrikeFlags` sets `g_bIsCStrike` and
`g_bCS_CZ_Flags_Initialized`. A leaf that is not a known game name still runs
`SetCStrikeFlags` and does not set `g_bIsCStrike`. An empty or missing mount
leaves `g_engfuncs.pfnGetGameDir` null, so `SetCStrikeFlags` is not called and
`g_bCS_CZ_Flags_Initialized` stays false. `Sys_Error` is not called.
`initializing_calls` is 1. Shutdown and the fatal error stay 0 until the test
calls them. A planted `hw.so` is not mapped. The retail hook chain does not
run. `RunFrame` is not called. Steam is not started. The planned command is
not executed. The game was not started.

CI: https://github.com/benjarogit/csretro/actions/runs/43199170090 success (`a60af72`).

`scripts/verify.sh`: 68 checks passed.

Console init on the process path (2026-11-12, preset `linux-tests-release`, ELF64, game not started):

```text
$ engine_mini-tests --gtest_filter=XashStartup.*
4/4 passed
AbsentHooksRecordTheSequenceAndInitArgv
NullBindingIsNotACall
ConInitReadsCondebugAndLeavesTheLog
WiredCallDoesNotOpenHwSo

$ engine_mini-tests
559/559 passed

$ xash_host-tests
20/20 passed

$ xash_launcher-tests
4/4 passed
```

`OnGameInitializing` stores a pointer table in this process and then calls
`Con_Init`. `con_initialized` becomes true. Without `-condebug`,
`con_debuglog` stays false. With `-condebug`, `COM_CheckParm` is non-zero and
a planted `qconsole.log` stays, because `g_pFileSystem` is null and that
pointer is not a call. `SetCStrikeFlags` does not run.
`initializing_calls` is 1. Shutdown and the fatal error stay 0 until the
test calls them. A planted `hw.so` is not mapped. `Sys_Error` is not called.
`RunFrame` is not called. Steam is not started. The planned command is not
executed. The game was not started.

CI: https://github.com/benjarogit/csretro/actions/runs/43196946044 success (`235274a`).

`scripts/verify.sh`: 68 checks passed.

Process engine table (2026-11-12, preset `linux-tests-release`, ELF64, game not started):

```text
$ engine_mini-tests --gtest_filter=XashStartup.*
3/3 passed
AbsentHooksRecordTheSequenceAndInitArgv
NullBindingIsNotACall
WiredCallDoesNotOpenHwSo

$ xash_host-tests --gtest_filter=HostFactory.EngineStartupRunsArgvWithoutHwSo
1/1 passed
```

`OnGameInitializing` reads a pointer table stored in this process. Null
function pointers are not calls. `Sys_Error` is not called on that path.
`initializing_calls` is 1, and shutdown and the fatal error stay 0 until the
test calls them. A planted `hw.so` is not mapped. `Con_Init`, the retail hook
chain and `RunFrame` do not run. The game was not started.

CI: https://github.com/benjarogit/csretro/actions/runs/43195855883 success (`8f15927`).

Install copy (2026-11-12, fake library, game not started):

```text
$ uv run --with pytest pytest tests/install_content/test_install_content.py -q
15 passed
```

`scripts/install-content.sh` reads `libraryfolders.vdf` and
`appmanifest_10.acf`, copies the allowlist, and refuses a destination inside
the source library. A fake shipped tree at the same relative path replaces the
retail bytes for a map, a model, a weapon sound, a sprite and a resource. A
retail-only map text, a retail-only weapon event and a retail-only valve sound
stay. `hw.so` stays in the source library. Missing Steam is `steam_missing`.
Missing App 10 is `game_missing`. An overlay inside the library is
`overlay_inside_source`. Personal configs, demos, screenshots and caches are
not copied. The game was not started.

CI: https://github.com/benjarogit/csretro/actions/runs/43195336643 success (`b052069`).

Host calls the Xash startup entries (2026-11-12, preset `linux-tests-release`, ELF64, game not started):

```text
$ xash_launcher-tests
4/4 passed
OpenLoadsThePlanAndDoesNotCallRunFrame
PresentPaintsTheMenuWithoutRunFrame

$ xash_host-tests
20/20 passed
HostFactory.GameMenuClickSendsTheCommandWithoutRunFrame
HostFactory.EngineStartupRunsArgvWithoutHwSo

$ xash_engine-tests
6/6 passed

$ engine_mini-tests --gtest_filter=XashStartup.*
3/3 passed
```

`OpenXashLauncher` calls `csr_host_load_engine` after `csr_host_start_gameui`.
That loads `next_engine_mini.dll` and `nitro_api2.dll`, calls
`EngineMini::Init`, then argv, mode and init_game. `EngineMini_CheckParm("-console")`
is non-zero, so `COM_InitArgv` ran. The successful open records calls 1/1/1
and leaves shutdown and the fatal error at 0. The same test then calls those
two entries and reads 1 and 1 back. A planted `hw.so` is not mapped.
`SteamAPI_IsSteamRunning` stays false. `RunFrame` is not called.
`OnGameInitializing` is not entered. The client does not start and play.

Xash startup entry (2026-11-12, preset `linux-tests-release`, ELF64, game not started):

```text
$ xash_engine-tests
6/6 passed
MissingStartupCallbacksAreNotCalled
BoundStartupCallbacksReceiveTheArguments
StartupCallDoesNotOpenHwSo

$ engine_mini-tests --gtest_filter=XashStartup.*
3/3 passed
AbsentHooksRecordTheSequenceAndInitArgv
NullBindingIsNotACall
WiredCallDoesNotOpenHwSo

$ xash_host-tests --gtest_filter=HostFactory.GameMenuClickSendsTheCommandWithoutRunFrame
1/1 passed
```

`COM_InitArgv` records `-console`. Mode, init, shutdown and the fatal error
move a counter and keep the arguments. Neither call maps a planted `hw.so`.
`OnGameInitializing` is not entered. The client does not start and play.

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
- **Scoreboard header bar is a Label background.** The `0,0 400x38` white rect
  is the default background colour `255,255,255,128` of the VGUI label named
  `ServerName` (`Label::GetClassName`), painted by `vgui2::Panel::PaintBackground`
  (`Panel.cpp:556`); its text is the scheme colour `BaseText` `255 176 0 255`
  (`resource/ClientScheme.res:24`) set through `DrawSetTextColor`. Both are
  client decisions and the stub marshals them correctly, so the low contrast on
  that label is not a stub defect. The retail comparison is still open
  (maintainer run).
- **Textures are no longer missing in the spike.** In the stock stub
  `draw_set_texture_file` (`0x12f0`), `draw_set_texture_rgba` (`0x1300`),
  `draw_set_texture` (`0x1310`) and `draw_textured_rect` (`0x1340`) are
  one-instruction `ret`s, `create_new_texture_id` (`0x1360`) hands out plain
  counters and `draw_get_texture_size` (`0x1320`) always reports `16x16`.
  A spike shim now replaces them; see the texture section below.
- **Push/Pop are still no-ops.** All eight `DrawTexturedRect` calls arrive as
  `0,0 10,10` because VGUI draws bitmaps in parent-relative coordinates and
  expects the offset from `PushMakeCurrent` (`0x1de0`) /
  `PopMakeCurrent` (`0x11b0`). The correct `GetAbsPos` source is IPanel slot
  **11** (`VPanelWrapper::GetAbsPos`, `_ZTV13VPanelWrapper` at
  `vgui2.so 0x2a0b0`); slot 13 is `SetInset` and corrupts the layout.
  The offset is deliberately off in the spike so fills and texture quads share
  the same local basis; it belongs in the adapter.
- **Fill provenance is fully mapped** (gdb backtrace per fill, paint 2): seven
  rects from `CCSClientScoreBoardDialog::PaintBackground`
  (`cstrikeclientscoreboard.cpp:402/409/416/428/434/442/450`), `0,0 832x600`
  and `0,0 400x38` from `vgui2::Panel::PaintBackground` (`Panel.cpp:556`), two
  `1,21 819x1` from `vgui2::CSectionHeader::Paint`
  (`SectionedListPanel.cpp:97`) and `0,0 822x33` from
  `vgui2::CItemButton::PaintBackground` (`SectionedListPanel.cpp:501`).
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

CI: run `42849944792` (evidence) and `42850326226` (paint-order correction),
both green.

### Fill provenance and the missing texture path

A gdb backtrace at each of the twelve `fill_rect` calls of paint 2 names the
VGUI caller of every rect:

```text
0,10 10x579    CCSClientScoreBoardDialog::PaintBackground  cstrikeclientscoreboard.cpp:402
10,0 811x599   CCSClientScoreBoardDialog::PaintBackground  :409
821,10 10x579  CCSClientScoreBoardDialog::PaintBackground  :416
0,10 1x580     CCSClientScoreBoardDialog::PaintBackground  :428
10,0 812x1     CCSClientScoreBoardDialog::PaintBackground  :434
10,599 812x1   CCSClientScoreBoardDialog::PaintBackground  :442
831,10 1x580   CCSClientScoreBoardDialog::PaintBackground  :450
0,0 832x600    vgui2::Panel::PaintBackground               Panel.cpp:556
1,21 819x1     vgui2::CSectionHeader::Paint                SectionedListPanel.cpp:97
1,21 819x1     vgui2::CSectionHeader::Paint                SectionedListPanel.cpp:97
0,0 822x33     vgui2::CItemButton::PaintBackground         SectionedListPanel.cpp:501
0,0 400x38     vgui2::Panel::PaintBackground               Panel.cpp:556
```

The last one is the white band: gdb on the same breakpoint reports
`this->GetClassName()` = `Label`, `this->GetName()` = `ServerName`, and the
colour is that panel's default background `255,255,255,128`. Its text is
`BaseText` `255 176 0 255` from `resource/ClientScheme.res:24`, set through
`DrawSetTextColor` and marshalled correctly. So neither the band nor the text
is a stub defect; only the retail comparison stays open.

What the stock stub lacks is the texture path. Its `draw_set_texture_file`
(`0x12f0`), `draw_set_texture_rgba` (`0x1300`), `draw_set_texture` (`0x1310`)
and `draw_textured_rect` (`0x1340`) are single `ret` instructions,
`create_new_texture_id` (`0x1360`) returns a counter and `draw_get_texture_size`
(`0x1320`) always reports `16x16`. VGUI therefore requests eight textures per
paint and the stub draws none of them:

```text
gfx/vgui/round_corner_nw   gfx/vgui/round_corner_small_nw
gfx/vgui/round_corner_ne   gfx/vgui/round_corner_small_ne
gfx/vgui/round_corner_sw   gfx/vgui/round_corner_small_sw
gfx/vgui/round_corner_se   gfx/vgui/round_corner_small_se
```

The caller is `vgui2::Bitmap::Paint` (`Bitmap.cpp:134`): `DrawSetTexture`
(vtable slot `0x68`), `DrawSetColor` (slot `0x20`), `DrawTexturedRect` (slot
`0x78`, additive variant slot `0x190`). All eight rects arrive as `0,0 10,10`
even though the four corners sit in different places, because VGUI hands
bitmaps to the surface in parent-relative coordinates and expects the offset
from `PushMakeCurrent` (stub `0x1de0`, currently a logging no-op). The texture
step therefore has two halves: load the TGA and upload it as a GL texture with
a real `DrawTexturedRect`, and apply the push/pop offset.

## Xash3D spike update (2026-11-10, textured quads)

**Result: the stub loads VGUI2's TGAs, uploads them as GL textures and draws
textured quads into the frame.** The scoreboard's rounded corners are the
first real textures: at paint 40 the eight `DrawTexturedRect` calls land on
`0,0 10,10` (parent-relative, see below) and the next frame differs in the
`x 0..9, y 0..9` block, e.g. `(0,9)` goes from `255,255,255` to `255,200,32`.
`glGetError()` is `0`.

![G07 Xash3D VGUI2 texture corner](docs/evidence/g07-xash3d-vgui2-texture-corner.png)

![G07 Xash3D VGUI2 texture corner, zoom](docs/evidence/g07-xash3d-vgui2-texture-corner-zoom.png)

A diagnostic 256×256 blit of `gfx/vgui/round_corner_nw` shows the texture's
actual texel content: the TGA is a black RGB image whose alpha is a
quarter-circle mask, and the quad reproduces that mask exactly, with the map
visible through the transparent part. This proves loader, upload, sampling and
blending separately from the 10×10 in-place draw.

![G07 Xash3D VGUI2 sampled texture](docs/evidence/g07-xash3d-vgui2-texture-sampled.png)

![G07 Xash3D VGUI2 sampled texture, zoom](docs/evidence/g07-xash3d-vgui2-texture-sampled-zoom.png)

What the shim does (spike-only, `texture.c`, not in the repository):

- `DrawSetTextureFile(id, name, filtered, reload)` loads the TGA from
  `cstrike/`, uploads it once and remembers the id; all eight files load per
  paint: ids 11–14 `round_corner_{nw,ne,sw,se}` at 64×64, ids 16–19
  `round_corner_small_{nw,ne,sw,se}` at 10×10.
- `DrawGetTextureSize` returns the real size instead of `16x16`.
- `DrawTexturedRect` enqueues a quad; the queue is flushed after
  `stub_surface_paint`, otherwise the rest of the panel paints over it.
- The tint is read from the stub's `DrawSetColor` state (four ints at GOT
  `+0x5c`, `+0x60`, `+0x64`, `+0x68`; alpha inverted for the engine), so it is
  `255,255,255,128` for four corners and `188,112,0,128` for the other four —
  not a hardcoded colour.

Three engine behaviours had to be handled, and they are requirements for the
real adapter:

1. **Save and restore the GL state.** The first working quad ended with
   `glBindTexture(GL_TEXTURE_2D, 0)` and `glDisable(GL_TEXTURE_2D)`. Xash3D's
   own batched 2D drawing then rendered untextured white quads over the whole
   frame, which is exactly the "white screen" that looked like a surface bug.
   Restoring `GL_TEXTURE_2D`, the binding, blend, depth, cull and the current
   colour fixes it. A six-stage probe (enable/disable, colour, bind, empty
   `glBegin`, immediate-mode quad, vertex arrays) confirmed stages 1–6 all keep
   the frame intact; only the trailing state teardown broke it.
2. **Use an own projection and viewport.** The engine's HUD-paint state has
   `Viewport 0,0 1280x1024` while the stub coordinates are `1024x768`, so a
   quad drawn in engine state lands at the wrong place. The shim pushes
   projection and modelview, loads `glOrtho(0,1024,768,0)` and
   `glViewport(0,0,1024,768)`, draws, then pops.
3. **Defer the draw.** Drawing inside `DrawTexturedRect` is overpainted by the
   remaining panel paint. The deferred flush after `stub_surface_paint` is what
   makes the pixels survive to the back buffer.

Remaining defect: VGUI hands bitmaps over in parent-relative coordinates, so
all eight rects are `0,0 10,10`. `PushMakeCurrent` (`0x1de0`) and
`PopMakeCurrent` (`0x11b0`) are no-ops in the stub. The `GetAbsPos` call has to
come from IPanel slot **11** (`VPanelWrapper::GetAbsPos`, `_ZTV13VPanelWrapper`
at `vgui2.so 0x2a0b0`); an earlier attempt used slot 13, which is `SetInset`
and corrupted the panel layout. The offset is off in the spike on purpose so
fills and texture quads share one local basis; the adapter owns the
translation.

Reproduce (all paths outside the repo):

```text
LD_PRELOAD="<spike>/kvshim.so <spike>/texture.so <spike>/hudonly.so" \
LD_LIBRARY_PATH="<spike>:<sdl32>:/usr/lib32" \
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
./xash3d -game cstrike +map de_dust2 +showscores
```

Back-buffer dumps are the only valid capture here: `import`/`xwd` cannot open
the GL window and `ffmpeg -f x11grab` records a black or white rectangle, while
`glReadPixels(GL_BACK)` at the paint contains the HUD.

## Xash3D spike update (2026-11-11, font metrics)

**Result: VGUI2 asks for 27 real font faces and gets one engine console font
back.** A spike-only preload shim (`fontprobe.c`, outside the repository)
patches the stub's five font slots and logs every call during a 14 s run:

| Slot | Calls | Result |
|---|---|---|
| `CreateFont` (`0x14b0`) | 36 | always `1` |
| `AddGlyphSetToFont` (`0x14c0`) | 54 | always `1`, always `font=1` |
| `AddCustomFontFile` (`0x14d0`) | 0 | not used (`ClientScheme.res` has no `CustomFontFiles`) |
| `GetFontTall` (`0x14e0`) | 42 089 | always `16` |
| `GetFontAscent` (`0x1680`) | 0 | VGUI2 only queries `GetFontTall` |

The 54 glyph sets request 27 distinct (name, tall, weight) triples
(`Name height/weight`, scaled by the scheme factor):

```text
Verdana       12/0     20/0     20/600
Verdana Bold  18/500   38/500
Tahoma        12/500   25/500
Tahoma        20/800   22/800   24/800  26/800
Tahoma        42/800   46/800   51/800  55/800
Arial         13/600   16/600   27/600  34/600
Arial         20/700   22/700   24/700  26/700
Arial         42/700   46/700   51/700  55/700
Marlett       11/0     23/0
```

Every handle is the number `1`, so every glyph set lands on the same handle and
every metric query returns `16`. The stub computes that `16` once in
`draw_set_text_font` (`0x11c0`): it measures the literal `"Wg."` (`.rodata
0x3000`) through the engine's `DrawConsoleStringLen` (`funcs+0x74`) and stores
the result as one global tall (`stub+0x5ff4+0x3c`). `get_text_size` (`0x2510`)
and `get_character_width` (`0x2480`) measure the same string with the same
engine call; drawing goes through `pfnDrawString` (`funcs+0x1c8`).

So the gap is not the text path (VGUI2 text already renders every frame) but
**font metrics**: every face collapses onto the engine's console font. The
`VGUI_Surface026` adapter needs its own TTF rasterizer, glyph atlas and
per-handle metrics (tall, ascent, ABC widths) instead of forwarding to the
engine font API. That is adapter work, not a stub patch: the stub has no font
data structure to patch.

Reproduce (spike paths outside the repo):

```text
csretro_fontlog=100000 LD_PRELOAD="<spike>/kvshim.so <spike>/texture.so \
<spike>/hudonly.so <spike>/fontprobe.so" \
LD_LIBRARY_PATH="<spike>:<sdl32>:/usr/lib32" \
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
./xash3d -game cstrike +map de_dust2 +showscores 2> fontprobe.log
```

`fontprobe` watches for the `stub_surface.so` mapping with `dl_iterate_phdr`
(KVShim exports its own `dlsym`, so `RTLD_NEXT` interposition is unsafe here)
and patches the five slots before VGUI loads its scheme. The hook signatures
take raw words: the stub's entry points are reached through a C++ interface, so
the first stack slot is the interface pointer and the font arguments follow.

`<sdl32>` holds the real SDL2 2.32.10 from the Steam runtime; the distro and
flatpak `sdl2-compat` builds abort with `Failed loading SDL3 library.` The
preload shims are built from `<spike>/spike-shims/*.c`, and `/tmp` scratch is
not durable, so rebuild them after a reboot:

```text
gcc -m32 -shared -fPIC -O2 -o <tmp>/fontprobe.so <spike>/spike-shims/fontprobe.c \
    -ldl -lpthread
```

## Xash3D spike update (2026-11-11, laid-out scoreboard)

**Result: the VGUI2 scoreboard now paints at its real position on screen, not
at the top-left corner.** Two corrections made the difference.

First, the offset source was wrong. The `VPANEL` that VGUI hands to
`PushMakeCurrent` is a `vgui2::VPanel`, so `GetAbsPos` is slot **19** in
`_ZTVN5vgui26VPanelE` (`vgui2.so 0x29fb0`, address point `0x29fb8`,
`VPanel::GetAbsPos(int&, int&)` at `0x17200`). Slot 11 is only correct for the
engine-side `VPanelWrapper` interface (`_ZTV13VPanelWrapper` at `0x2a0b0`).
Calling the wrong slot (11, or earlier 13 = `SetInset`) yields `0,0` or
corrupts the layout.

Second, the offset has to be applied to every primitive, not just to textures.
The shim now copies the engine funcs table and replaces two slots: fills
(`funcs+0x2c`) and text (`funcs+0x1c8`) get wrappers that add the current push
offset to `x`/`y`; textures already use it. The copy is installed at
`stub+0x5ff4+0x3fc`, and `PopMakeCurrent` restores the previous offset.

Measurements (paint 40 vs. 41, 1024x768):

| Item | Value |
|---|---|
| engine screen size | `1024x768` |
| dialog offset (`GetAbsPos`) | `96,84` |
| texture corners | `96,84`, `917,84`, `96,673`, `917,673` |
| texture diff between paints | 108 px inside `96..926, 84..682` |

The frame shows the `832x600` panel at `96,84`, its four rounded corners, the
white `ServerName` bar and the right-aligned `Score`, `Deaths`, `Latency`
header.

![G07 Xash3D VGUI2 laid-out scoreboard](docs/evidence/g07-xash3d-vgui2-laid-out.png)

![G07 Xash3D VGUI2 laid-out scoreboard, header](docs/evidence/g07-xash3d-vgui2-laid-out-header.png)

![G07 Xash3D VGUI2 laid-out scoreboard, corner](docs/evidence/g07-xash3d-vgui2-laid-out-corner.png)

Third finding, and a trap for every later run: **the engine screen size must
match the framebuffer size.** The stub asks for it through `funcs+0x30`
(`get_screen_size` is virtual: `this` plus two `int*`). With an Xvfb desktop
of 1280x1024 and a 1024x768 game buffer, VGUI reported 1280x1024 and laid the
dialog out for that area (`1098x714` at `85,149`, corner textures at
`1183,863`, outside the buffer). Matching the Xvfb screen to the game
resolution makes the same run report `1024x768` and place the dialog at
`96,84`.

Reproduce (spike paths outside the repo):

```text
csretro_push=1 csretro_translate=1 csretro_texdebug=1 \
LD_PRELOAD="<spike>/kvshim.so <tmp>/texture.so <spike>/hudonly.so" \
LD_LIBRARY_PATH="<spike>:<sdl32>:/usr/lib32" \
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
./xash3d -game cstrike +map de_dust2 +showscores
```

The Xvfb screen has to be 1024x768; a 1280x1024 desktop reproduces the
mis-scaled layout above.

## Xash3D spike update (2026-11-11, font backend)

**Result: VGUI2 now lays out and draws with real per-handle fonts.** The stub
patched its font slots at the first paint, which is too late: VGUI creates its
scheme fonts during `HUD_Init`, and a run with late patching logged **zero**
`CreateFont` calls. The shim now patches the five font slots (plus
`DrawSetTextFont`) as soon as `stub_surface.so` is mapped, using the same
polling thread as `fontprobe.c`.

Measurements (`+map de_dust2 +showscores`, 1024x768, paint 41):

| Slot | Calls | Result |
|---|---|---|
| `CreateFont` | 36 | handles `1`..`36` |
| `AddGlyphSetToFont` | 54 | 36 primary sets + 18 charset fallbacks |
| `GetFontTall` | 1024 | the requested height per handle |
| `GetCharABCwide` | 1471 | FreeType advances (only during paint 1, then cached) |
| `GetCharacterWidth` | 4611 | FreeType advance per character |
| `GetTextSize` | 0 | VGUI2 never calls it |
| `GetFontAscent` | 0 | VGUI2 never calls it |

The 36 primary sets ask for 15 distinct heights
(`11,12,14,17,18,19,20,22,24,26,28,32,35,38,41`); before this change every one
of the 42 089 `GetFontTall` calls returned the stub's single `16`.

`AddGlyphSetToFont` can be called twice for one handle: the first set is the
face (`Tahoma 26/800`), the second a charset fallback with the same height and
another family (`Arial 26/700`). The backend keeps the first set for face and
height and lets the later ones only extend coverage.

Linux has neither Verdana, Tahoma nor Arial, and the retail tree ships only
`valve/resource/marlett.ttf`, so the spike resolves faces explicitly: Marlett
from the retail file, Arial to Liberation Sans, Verdana/Tahoma to DejaVu Sans
(bold for weight >= 600).

Drawing replaces the `funcs+0x1c8` engine string call: each character is
rasterised with FreeType, uploaded as a small GL texture (white RGB, coverage
in alpha) and queued as a quad at baseline `y + ascent`. Per paint: 50 text
calls, 42 glyph quads, 21 glyph textures in total, **0** engine fallbacks - the
visible letters come entirely from the backend. A control run with
`csretro_face=DejaVuSerif-Bold.ttf` renders the header in serif, which shows
the glyphs follow the configured file.

![G07 Xash3D VGUI2 fonts](docs/evidence/g07-xash3d-vgui2-fonts.png)

![G07 Xash3D VGUI2 fonts, header](docs/evidence/g07-xash3d-vgui2-fonts-header.png)

![G07 Xash3D VGUI2 fonts, serif control](docs/evidence/g07-xash3d-vgui2-fonts-serif.png)

Reproduce (spike paths outside the repo):

```text
csretro_push=1 csretro_translate=1 csretro_fonts=1 csretro_glyphs=1 \
LD_PRELOAD="<spike>/kvshim.so <tmp>/texture.so <spike>/hudonly.so" \
LD_LIBRARY_PATH="<spike>:<sdl32>:/usr/lib32" \
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
./xash3d -game cstrike +map de_dust2 +showscores
```

The shim is built with FreeType:

```text
gcc -m32 -shared -fPIC -O2 -o <tmp>/texture.so <spike>/spike-shims/texture.c \
    -I/usr/include/freetype2 -lfreetype -lpthread
```

Known spike limitation: glyph quads are deferred like the texture quads, so
text is drawn after later child backgrounds. The adapter must draw text in
traverse order; see plan section 3.6.6.

## Xash3D spike update (2026-11-11, input, hover and click)

**Result: the stub now feeds mouse input into VGUI2, and a synthetic click on
the team menu reaches the panel and changes the game state.** `CInputWin32` is
a `.bss` singleton (`vgui2.so+0x34420`, handed out as `VGUI_Input004`); the
shim calls its methods directly and flushes the queue through
`CVGui::DispatchMessages`:

| Step | Symbol | Address |
|---|---|---|
| cursor | `CInputWin32::InternalCursorMoved` | `vgui2+0x1dc00` |
| press | `CInputWin32::InternalMousePressed` | `vgui2+0x1e8a0` |
| release | `CInputWin32::InternalMouseReleased` | `vgui2+0x1e200` |
| dispatch | `CVGui::DispatchMessages` | `vgui2+0x1b120`, singleton `vgui2+0x34340` |
| query | `CInputWin32::GetMouseOver` | `vgui2+0x1ce60` |

`CVGui::RunFrame` must not be used: it calls
`UpdateMouseFocus(GetCursorPosition())` and the stub reports `0,0`, which
would clear the mouse focus every frame. `UpdateMouseFocus` also needs an
embedded panel, because the input context has no `_rootPanel`
(`g_Input+0x1b4` is `NULL`) and falls back to
`surface()->GetEmbeddedPanel()->Client()->IsWithinTraverse(x,y,false)`; the
shim sets the embedded panel to the team menu root (`stub+0x1cc0`).

Measurements (`chooseteam` at paint 20, cursor `239,201` at paint 29, press
and release at paint 30, `+map de_dust2`, 1024x768):

| Check | Call | Result |
|---|---|---|
| hit test | `IsWithinTraverse(239,201)` | `Button/terbutton` |
| hover | `InternalCursorMoved` + `DispatchMessages` | `GetMouseOver()` = `Button/terbutton` |
| click | `InternalMousePressed`/`Released` + `DispatchMessages` | `CTeamMenu` `SetVisible(0)` on release |
| team change | `g_iTeamNumber` (`client.so+0x1e54ec`) | paint 30 `0`, paint 31 **`1`** (Terrorist) |
| negative control | click at `700,600` (`RichText/MapInfo`) | no `SetVisible`, `g_iTeamNumber` stays `0` |

The chain mouse → `CInputWin32` → `PostMessage` → `DispatchMessages` →
`Button` → `CTeamMenu` command → team change is therefore proven, and the
negative control rules out that any press closes the menu. The team change
shows up one frame later because it round-trips through the server.

![G07 Xash3D VGUI2 team menu](docs/evidence/g07-xash3d-vgui2-team-menu.png)

![G07 Xash3D VGUI2 team menu, header](docs/evidence/g07-xash3d-vgui2-team-menu-header.png)

![G07 Xash3D VGUI2 after the click](docs/evidence/g07-xash3d-vgui2-team-click.png)

617 714 pixels change between paint 29 and paint 30 inside
`121,0..1023,715`, i.e. exactly the area the menu covered.

Reproduce (spike paths outside the repo):

```text
csretro_cmd=chooseteam csretro_cmdpaint=20 csretro_input=1 \
csretro_mouse=239,201 csretro_mousepaint=29 csretro_click=30 \
csretro_teamprobe=1 csretro_push=1 csretro_translate=1 \
csretro_fonts=1 csretro_glyphs=1 csretro_dump=29,30 \
LD_PRELOAD="<spike>/kvshim.so <tmp>/texture.so <spike>/hudonly.so" \
LD_LIBRARY_PATH="<spike>:<sdl32>:/usr/lib32" \
LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 \
./xash3d -game cstrike +map de_dust2
```

Known spike limitations (all adapter work, not VGUI2 findings):

1. **No rollover image.** `Button::OnCursorEntered` sets `armed`, but the
   appearance comes from scheme textures (`button/rollover`) the stub never
   loads. A cursor move without a click changes only 99 pixels between paint
   29 and 30, so hover is proven semantically via `GetMouseOver`, not
   visually.
2. **No mouse cursor.** The stub draws none; retail uses a cursor texture
   through `VGUI_Surface026`.
3. **Hit area != painted area.** The spike offset shifts fills and text a few
   pixels against the solved panel rects: `239,201` is inside `terbutton`
   according to the hit test but sits just above its painted edge. The adapter
   needs one transform for both hit testing and painting.
4. **The stub's scoreboard path always paints**, independent of visibility, so
   it shows up behind the team menu in the evidence frames.

The `g_iTeamNumber` probe reads `client.so+0x1e54ec` (team ids: `0`
unassigned, `1` Terrorist, `2` CT, `3` spectator) and logs every change; it is
spike-only instrumentation.

## Xash surface library (2026-11-11)

**Result: the first product slice of Option A is a host-owned `VGUI_Surface026`,
not a spike shim.** `client/nextclient/xash_surface/` builds `xash_surface.so`.
`CreateInterface("VGUI_Surface026")` returns the live `csr_surface_create`
instance and rejects every other name. `vgui2` already loads that name from
the host factory into `g_pSurface`; the surface is therefore not linked into
`vgui2.so`, and Xash3D is not patched.

What the tests cover (`xash_surface-tests` 18/18 and `xash_host-tests` 3/3,
preset `linux-tests`; shipping ELF32 BuildIDs `xash_host`
`1c31106327a06a849467a3eb102f81092c0e0404`, `xash_surface`
`a392e4fdf47b7eaebf6702085d84fb94921bfe76`, `vgui2`
`da042bdd164c647a20c8db2d3bc9ea3cf896904b` with dynamic export
`csr_vgui_dispatch_messages`):

| Check | Result |
|---|---|
| VGUI alpha 0 / 255 / 128 | GL alpha 255 / 0 / 127 |
| Push origin and clip | parent rect moves by the host position; a clip shrinks the quad and its UVs; an empty intersection draws nothing |
| Font handle | first glyph set keeps height 26 when a later set asks for 40; advance is the FreeType advance, and bold is wider than regular |
| Opaque fill | red cell on the pbuffer, VGUI alpha 0 |
| Transparent fill | VGUI alpha 255 leaves the cleared pixel |
| Texture | nearest 2×2 RGBA quad samples the yellow texel |
| Offset | a rect at `0,0` with host position `8,4` lands on pixel `8,4` |
| Clip | an 8×8 rect clipped to 2×2 leaves the outside pixel untouched |
| Glyph | one character paints ink from the atlas and leaves part of its cell clear; the cursor advances by the FreeType width |
| GL state | current colour, `GL_TEXTURE_2D`, matrix mode and viewport match the values from before the frame; `glGetError()` is 0 |

`SolveTraverse` visits visible nodes and skips a hidden branch. `PaintTraverse`
calls the root paint with repaint and allow-force set, then each popup.
`ShouldPaintChildPanel` is false for a popup. The child loop stays in vgui2.

Mouse move, press and release each call the matching host function and then
one dispatch. `RunFrame` adds no call. `SurfaceSetCursorPos` stores the
position and does not dispatch.

`xash_host` loads the surface and binds mouse move, press and release to
caller-supplied function pointers, then one dispatch.
`csr_vgui_dispatch_messages` is the product export. It lives in the tracked
SDK tree `client/dep/ncl-hl1-source-sdk/`. `RunFrame` is not called.
Calls do not use file offsets. REA's Ghidra provider did not bind to the
installed Ghidra 12.2. Headless Ghidra 12.2 confirmed that the retail library
does not export `DispatchMessages`, that the first factory returning the
surface name wins, and that `RunFrame` updates mouse focus before dispatch.

Not in this slice: scheme textures, blur, scanlines, outline, and starting
Xash from this host. Marlett is resolved by the host. The spike stub is still
what paints the real VGUI2 tree.

## Player identity (2026-11-11)

`ResolvePlayerIdentity` is the local slot. Evidence: `player_identity-tests`
6/6 on preset `linux-tests`, ELF32, no network. The rule is plan section 3.7.

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
| G07 | in progress | G07a: SHA-256 build registry, 7 registry tests. G07b: three-stage resolver, 327 engine + 81 client entries, 70/70 `nitro_api-tests`, CI `42590218846`. G07c: provider check tool, 0 offsets, 0 unverified names. G07d: loader fix `79ba393` (global Steam proxy, SDK tier0), CI `42595518318`, maintainer rerun pending. Xash3D client spike: Xash3D-FWGS `e9b94dc`, headless dedicated and ReHLDS e2e green; 32-bit Xash3D runs Valve's `client.so` + `vgui2.so` through `HUD_Init` with a stub `VGUI_Surface026`; the stub paints real VGUI2 panel trees, real VGUI2 text, the scheme-coloured panel fills and textured quads into a rendered frame in **every** frame (`docs/evidence/g07-xash3d-vgui2-text.png`, `-steady.png`, `-filled-panel.png`, `-texture-corner.png`, `-texture-sampled.png`; 1755 px text diff at paint 1 and paint 60, 499 200 px = 832x600 fill at paint 20, 12 fills and 50 text calls per paint, 8 TGAs loaded per paint and 27 px texture diff in `0..9 x 0..9` at paint 40, `glGetError()` 0; CI `42867114212`; font measurement: 36 `CreateFont` -> `1`, 54 glyph sets -> `font=1` with 27 real faces, 42 089 `GetFontTall` -> `16`, `GetFontAscent`/`AddCustomFontFile` never called; laid-out scoreboard: `VPanel::GetAbsPos` slot 19, dialog at `96,84`, four corners at `96,84`/`917,84`/`96,673`/`917,673`, 108 px texture diff, `docs/evidence/g07-xash3d-vgui2-laid-out.png`, `-header.png`, `-corner.png`, CI `42973488306`; font backend: 36 handles, 15 heights, FreeType metrics, 50 text calls / 42 glyph quads per paint with 0 engine fallbacks, `docs/evidence/g07-xash3d-vgui2-fonts*.png`, CI `42976346788`; input proof: `CInputWin32` singleton `vgui2+0x34420`, `IsWithinTraverse(239,201)` = `Button/terbutton`, `GetMouseOver()` = `Button/terbutton`, press/release hides `CTeamMenu` and `g_iTeamNumber` `0`→`1`, negative control on `RichText/MapInfo` changes nothing, 617 714 px diff in `121,0..1023,715`, `docs/evidence/g07-xash3d-vgui2-team-*.png`, CI `42987243071`); engine direction Option A (plan section 3.6) |
