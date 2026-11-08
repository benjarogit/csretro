# Credits

CSRETRO stands on the work of many people. This file is the single source for
attribution. The wiki links here; it does not copy this list.

Every project below is credited, including those we only took ideas or procedures
from. If a name is missing, it is an oversight, not a decision – open an issue.

## Vendored components

These are imported with their history and keep their upstream licenses.

| Project | Authors | License | What we use |
|---|---|---|---|
| [NextClient](https://github.com/CS-NextClient/NextClient) | Next21 Team and contributors | **No license file** (default copyright) | The client: engine patching, HUD, radar, GameUI, launcher, tooling |
| [NextClientServerApi](https://github.com/CS-NextClient/NextClientServerApi) | Next21 Team and contributors | MIT | AMXX module, natives, client verification |
| [ncl-hl1-source-sdk](https://github.com/CS-NextClient/ncl-hl1-source-sdk) | Fork of [tmp64/hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk) | Valve Source SDK terms + third-party notices | VGUI2, tier1/tier2, Linux port, optional CEF |
| [vcpkg](https://github.com/microsoft/vcpkg) | Microsoft and contributors | MIT | C/C++ dependency management |
| [Half-Life 1 SDK](https://github.com/ValveSoftware/halflife) | Valve Corporation | Valve SDK license | GoldSrc interfaces and types |

## Ideas and procedures only (not vendored)

| Project | Authors | License | What we took |
|---|---|---|---|
| [FuryBaM/cs16-goldsrc-client](https://github.com/FuryBaM/cs16-goldsrc-client) | FuryBaM and contributors | GPL-3.0 | Client-side prediction approach, `client.so` replacement study |
| [FWGS/cs16-client](https://github.com/FWGS/cs16-client) | [Velaron](https://github.com/Velaron), [a1batross](https://github.com/a1batross), Vladislav4KZ, SNMetamorph, Flying With Gauss and contributors | GPL-2.0-or-later with HL engine exception | Buy presets, extended crosshair, HUD colour, Linux build layout, prediction reference |
| [Xash3D-FWGS](https://github.com/FWGS/xash3d-fwgs) | FWGS and contributors | GPL-3.0 | Evaluated as a GoldSrc engine replacement, rejected in favour of ReHLDS; kept as a reference for Linux engine layout |
| [hlsw-portable](https://github.com/portapps/hlsw-portable) | portapps, crazy-max | MIT | Portable application layout for `csretro-server` |

No code from GPL projects is copied into CSRETRO. The license of this repository
is still undecided; see `HANDOFF.md`.

## Upstream projects and libraries

| Project | Authors | License | What we use |
|---|---|---|---|
| [ReHLDS](https://github.com/dreamstalker/rehlds) | ReHLDS team | GPL-3.0 | Test server engine |
| [Metamod-R](https://github.com/rehlds/Metamod-R) | ReHLDS team, theAsmodai | GPL-3.0 | Test server plugin loader |
| [ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) | s1lentq and contributors | GPL-3.0 | Test server game library |
| [AMX Mod X](https://github.com/alliedmodders/amxmodx) | AlliedModders | GPL-3.0 | Server plugin runtime |
| [ReAPI](https://github.com/rehlds/ReAPI) | ReHLDS team | GPL-3.0 | Server module API |
| [MetaAudio](https://github.com/MoeMod/MetaAudio) | MoeMod and contributors | GPL-3.0 | Reference for the upstream audio path |
| [YaPB](https://github.com/jeefo/yapb) | jeefo and contributors | GPL-3.0 | Bot reference used by the upstream client |
| [csldr](https://github.com/mikkokko/csldr) | mikkokko | – | Loader reference used by the upstream client |
| [cm2network/steamcmd](https://github.com/CM2Walki/steamcmd) | CM2Walki and contributors | MIT | SteamCMD base image that downloads HLDS App 90 for the test server build |

## People

- **Next21 Team** – created NextClient and NextClientServerApi.
- **Valve** – GoldSrc, Half-Life SDK, Source SDK, Counter-Strike.
- **SamVanheer** – GoldSrc reverse engineering behind `hl1_source_sdk`.
- **tmp64** – `hl1_source_sdk` upstream.
- **KotyaraKryt** – Linux port, Discord integration, commands, FastDL.
- **Velaron / FWGS** – cs16-client, the canonical open client reference.
- **Nordic Warrior, fl0werD, lozatto, SanyaSho, MoeMod, Felipe** – upstream
  contributions that shaped the client we build on.
- Everyone who filed bugs, wrote tests or answered questions upstream.

## Dependencies

All vcpkg dependencies are credited through their own license files in
`client/vcpkg_installed/` at build time and in the release notices. A generated
dependency list is published with each release.
