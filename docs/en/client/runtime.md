# Client runtime

CSRETRO runs only on the **Default Public branch** of Counter-Strike 1.6.
The product process engine is Xash3D. The decision is in
[plan section 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Build detection

The product identity is `xash` (`BuildFamily::Xash`). Startup does not open,
hash, or hook `hw.so`, and it does not read `client.so`. Missing engine
addresses are not hooks. A call does not jump through a null trampoline.

`vgui2_surface`, `VGUI2_ResetCurrentTexture` and `VGUI2_Draw_SetTextColor` go
through `xash_engine` to the one surface. `VGUI2_Draw_Init` has no entry
there. A missing function pointer is not called.
`csr_xash_engine_argv`, `csr_xash_engine_mode`, `csr_xash_engine_init_game`,
`csr_xash_engine_shutdown` and `csr_xash_engine_error` take arguments, video
mode, game init, shutdown and a fatal error. When the matching retail hook is
present, that call stays. When it is absent, `engine_mini` records the values
and records that the engine is Xash. The host loads `next_engine_mini.dll`
and `nitro_api2.dll` after the other host pieces, calls `EngineMini::Init`,
then argv, mode and init_game. A missing library or export is a failure and
is not called. A successful open does not call shutdown or the fatal error;
both stay bound. `COM_InitArgv` runs. `OnGameInitializing` reads a pointer
table from this process. A null function pointer is not a call, and
`Sys_Error` is not called. `Con_Init` then runs. It reads `-condebug` through
`COM_CheckParm` and does not call a null filesystem. `pfnGetGameDir` then
writes the name of the mounted directory. An empty mount leaves that pointer
null, and `SetCStrikeFlags` is not called. A mounted name runs
`SetCStrikeFlags`. `NET_SendPacket` is the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so no
socket is opened. A null pointer is not a call. A null hook is not a call.
`Netchan_CopyFileFragments` is then the function in this process when that
retail hook is absent. Init stores the pointer and does not call it, so no
fragment file is copied and no socket is opened. A null pointer is not a
call. A null hook is not a call. `SVC_TimeScale` is then the function in
this process when that retail hook is absent. That function is
`CL_Parse_Timescale`. Init stores the pointer and does not call it, so the
timescale stays unchanged and no socket is opened. A null pointer is not a
call. A null hook is not a call. `SVC_SendCvarValue` is then the function
in this process when that retail hook is absent. That function is
`CL_Send_CvarValue`. Init stores the pointer and does not call it, so no
cvar is sent, cvar values stay unchanged and no socket is opened. A null
pointer is not a call. A null hook is not a call. `CL_HTTPUpdate` is then
the function in this process when that retail hook is absent. Init stores
the pointer and does not call it, so no HTTP request is made and no socket
is opened. A null pointer is not a call. A null hook is not a call.
`CL_HTTPStop_f` is then the function in this process when that retail hook
is absent. Init stores the pointer and does not call it, so no download is
stopped and no socket is opened. A null pointer is not a call. A null hook
is not a call. `CL_StartResourceDownloading` is then the function in this
process when that retail hook is absent. Init stores the pointer and does
not call it, so no download is started and no socket is opened. A null
pointer is not a call. A null hook is not a call. `CL_ReadPackets` is then
the function in this process when that retail hook is absent. Init stores
the pointer and does not call it, so no packet is read and no socket is
opened. A null pointer is not a call. A null hook is not a call.
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
hook is absent. Init stores the pointer and does not call it, so the model
list is not printed and no socket is opened. A null pointer is not a call.
A null hook is not a call. `Mod_ValidateCRC` is then the function in this
process when that retail hook is absent. Init stores the pointer and does
not call it, so no CRC is checked, no model is looked up or rejected and
no socket is opened. A null pointer is not a call. A null hook is not a
call. The next retail hook is `Mod_NeedCRC`. This path returns before it.
`csr_host_frame` is a later direct call and does not extend that list.
`OnGameInitialized` is not entered. The retail hook chain
does not run. `hw.so` is not opened.
`RunFrame` is not called. The other `engine_mini` forwards stay on the retail
hook.

`client_mini` and `vgui2` come from the same SDK revision. Both are ELF64 and
link the host `libtier0.so`, `libvstdlib.so` and `libsteam_api.so`. Both carry
`VGUI_Scheme009`. Valve's i386 `client.so` is not opened and not linked.

`xash_host` opens that pair after the surface factory is live. It sees
`csr_vgui_dispatch_messages` and `VGUI_Scheme009`. A missing library is a
failure and is not called. The host does not start the game.

`gameui` is ELF64 from the same SDK revision and links the same host runtime.
It carries `GameUI007` and `GameConsole003` and no surface of its own. The
host opens it after the surface factory. That load does not call into the
game UI and does not add a second factory. `IGameUI::Initialize` follows the
mount.

The same factory list answers `VFileSystem009` after the host mounts one root.
The surface stays the first entry. The proxy reads only files under that root.
A missing file is a miss. It does not open `hw.so`. After that mount,
`IVGui::Init` takes this list so `vgui2` holds the one surface. A scheme
file under that root is loaded through that manager. A missing file leaves
the host open. `IGameUI::Initialize` then parents the base panel to the host root. The default scheme is read from `resource/schemes/TrackerScheme_ClassicPlus.res` when that file is present, and the server list is then a child of the base panel. A missing
engine module is not called. `csr_host_paint_gameui` then sizes that root and paints the tree. When the scheme file is present, that paint reads `resource/BackgroundLayout.txt` and loads a texture name with no extension from the `.tga` beside it. The background image is drawn at time 0. `csr_host_start_gameui` then calls `IGameUI::Start` on a host engine table that does not open `hw.so`. The following paint builds the options pages. `RunFrame` is not that call. The
host does not start the game.

The one `libsteam_api.so` is the ELF64 host library. It does not load Valve's
i386 library and it does not start Steam. When Steam is not already logged in
it reports logged out. A missing interface name is not called. The name stays
`ResolvePlayerIdentity` and the `name` setinfo, with no portrait.

The Xash launcher assembles those pieces into one command. Installation
searches Steam and copies the mounted content outside the library; the open
receives that copy as the content root. A missing root starts nothing. The
launcher is not executed, does not start Steam, and requires the original
game content. See [Install game content](install.md).
`nextclient.sh` and the first-start steps remain the GoldSrc path.

The build registry in `client/nextclient/nitro_api/src/build_registry.cpp`
remains the offline record of the public-linux fingerprint. It is not the
startup path.

| ID | Platform | Steam build | Role |
|---|---|---|---|
| `xash` | Linux | – | product process |
| `public-linux-1.0.1.4` | Linux | 12934623 | offline fingerprint of the retail files |

## Address resolution

The product provider is `EngineAddressProviderXash`. It has no retail symbol
table. The public providers in `EngineAddressProviderPublicLinux.cpp` and
`ClientAddressProviderPublicLinux.cpp` remain the catalog of retail names for
the offline check. The product process does not use them. Every retail entry
is resolved in three stages:

1. **dynsym** – `dlsym` through the dynamic symbol table entry.
2. **symtab** – the full ELF symbol table of the file on disk.
3. **pattern** – a byte pattern as the last resort.

On the public build the engine functions exist only in `symtab`; `dynsym`
contains mostly C++ libraries. Stage 2 therefore carries the resolution. The
provider tables cover 327 engine entries and 81 client entries; every name was
checked against the retail binaries.

## Checking the providers

`tools/check_address_providers.py` compares both providers with an installation:

```bash
python3 tools/check_address_providers.py --game-dir /path/to/Half-Life
```

The tool checks every symbol name against `dynsym` and `symtab` and counts every
string literal in the binary. A missing symbol, a non-unique string or an
unsupported search type makes it exit with code 1. That is the evidence to run
after a Steam update and before adding a new provider.

Three entries differ from the rule:

| Entry | Type | Reason |
|---|---|---|
| `isFogEnabled.26640` | symbol | compiler-generated name of a static local; present only under this name in the public build |
| `R_ForceCVars.part.3` | symbol | compiler-generated part of a function; present only under this name in the public build |
| `GL_ARB_multitexture ` | string | occurs exactly once in `hw.so` |

The two generated names are bound to this exact binary. The retail catalog is
pinned by SHA-256. The tool reports every mismatch. The product process does
not load this catalog.

`EngineMsgBase` is the `cl_parsefuncs` table; its layout
`{opcode, pszname, pfnParse}` matches `goldsrc::EngineMsg`.

## Adding a build

1. Take both files from an unmodified installation.
2. Run `sha256sum hw.so cstrike/cl_dlls/client.so`.
3. Add an entry with ID, Steam build number, platform and both digests to
   `build_registry.cpp`.
4. Run `nitro_api-tests`; the registry tests cover unknown, incomplete and
   platform-mismatched fingerprints.

Another retail catalog needs its own address provider whose symbol names are
checked against the new binaries. The resolver itself stays unchanged. The
product path remains `EngineAddressProviderXash`.

## First start

The manual check after a client build is described in
[First client start](first-start.md).
