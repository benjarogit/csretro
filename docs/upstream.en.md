# Upstream and provenance

This table records the original vendor revisions. It does not claim that today's source tree
is identical to those commits. CS Retro changes are recorded in this repository's Git history.
Reference trees are not built as a second client.

| Path / Pfad | Upstream | Imported revision / Importierter Stand |
| --- | --- | --- |
| `engine/` | [FWGS/xash3d-fwgs](https://github.com/FWGS/xash3d-fwgs) | `1442d14a69093780389104dcb7369aa3685945cf` |
| `client/` | [CS-NextClient/NextClient](https://github.com/CS-NextClient/NextClient) | `f5addc2ba0276d30d26c40eeb8d51fb9fc416102` |
| `client/dep/NclNitroApi/` | [CS-NextClient/NclNitroApi](https://github.com/CS-NextClient/NclNitroApi) | `f73fc1a7592ba413aa382b5b3857e4a40b1b545e` |
| `client/dep/NclNitroApi/dep/ncl-hl1-source-sdk/` | [CS-NextClient/ncl-hl1-source-sdk](https://github.com/CS-NextClient/ncl-hl1-source-sdk) | `46c310389d669685ec953b8c23f651bd60465e19` |
| `client/body/`, `refs/a-cs16-client/` | [Velaron/cs16-client](https://github.com/Velaron/cs16-client) | `bb60674c120ae9bf8fa7854018bea8a77e71c17f` |
| `refs/b-cs16-goldsrc/` | [FuryBaM/cs16-goldsrc-client](https://github.com/FuryBaM/cs16-goldsrc-client) | `b662acca3ce74c2c9851cc842592c58661d95799` |
| `server/` | [CS-NextClient/NextClientServerApi](https://github.com/CS-NextClient/NextClientServerApi) | `1c7e5c61191f1b949a28cade96dc1d821eedb335` |
| `server/game/` | [rehlds/ReGameDLL_CS](https://github.com/rehlds/ReGameDLL_CS) | `b0889847fe6d03898be88acc9e366660efb40ab5` |


## Review 2026-09-13 (no vendor refresh)

Pins unchanged. Selective ports only, no HEAD merge.

| Upstream | Delta vs pin | Taken | Skipped |
| --- | --- | --- | --- |
| FWGS/xash3d-fwgs | 55 commits after `1442d14` | `ec88a62` do not free SDL3 `GetBasePath` | freevgui/MainUI, rumble, `r_showtextures`, NS `always_textinput`, Vita CI, listen-rate reverts |
| Velaron/cs16-client | 5 commits after `bb60674` | `9c891b8` scope/`TrueWidth` via `vid_width`/`vid_height` | `57607ab` GetGunPosition already present; YY-Thunks/XP, Vita, mainui |
| rehlds/ReGameDLL_CS | 0 — identical `b088984` | — | — |
| rehlds/ReHLDS | not vendored (Xash is the engine) | — | reconnect/speedhack/bzip2 — watch only |

Already present: `R_StudioGetPlayerState` requires `currententity` (`dccfaf3`).

## PrimeXT (tech upstream, not a runtime pin)

[SNMetamorph/PrimeXT](https://github.com/SNMetamorph/PrimeXT) is the primary technical upstream for the client renderer, materials, lighting, post-FX and ImGui tools. It is **not** an engine replacement and not a second `client.so`.

- Product code lives in CS Retro paths and may be rewritten.
- `refs/primext/` is the not-built comparison tree (gitignored). PX1 pin: tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23). No submodule/vcpkg fetch. Not `latest`.
- First productive integration is PX2: render-API bridge only (`17bd79f`, visually verified, issue #4 closed). PX3B: targeted world/offscreen adaptation in `client/render/` (`GL_RenderFrame` stays 0). See `docs/research/px1-primext.md`.
- Updates: understand the fix → find our implementation → take it in CS Retro form.
- Record provenance and license per port before public source or builds.

The earlier “watch PrimeXT only” decision is **replaced**. PrimeXT PhysX is not taken for PX2/PX3.

## Selective integration

Relevant changes are compared and ported selectively. There are no automatic whole-tree
merges or implicit updates to upstream HEAD.
For ports, document the exact revision, scope and tests.
See [architecture](architecture.md) for runtime responsibilities.

## Grenade references

The following were consulted for Molotov weapon logic, menu integration and visible inferno effects:

- [languagelawyer/cs16-client: molotov](https://github.com/languagelawyer/cs16-client/tree/molotov)
- [languagelawyer/ReGameDLL_CS: molotov](https://github.com/languagelawyer/ReGameDLL_CS/tree/molotov)
- [languagelawyer/mainui_cpp: molotov](https://github.com/languagelawyer/mainui_cpp/tree/molotov)

These branches are moving references, not newly established fixed vendor pins.
They are not imported wholesale and do not provide automatic permission to redistribute
third-party models. Additional credits and UI comparison sources are in the
[complete attribution ledger](https://github.com/benjarogit/csretro/blob/main/CREDITS.md).

## Announcer sounds

Only the WAVs `prepareforbattle`, `5`–`1`, `begin` and `1minuteremains` are vendored from
[xonotic/xonotic-data.pk3dir](https://github.com/xonotic/xonotic-data.pk3dir). No QuakeC and no
automatic data refresh. Notice: `data/ui-overrides/cstrike/sound/announcer/NOTICE.txt`.

[Credits](credits.md) · [Licenses](licenses.md)
