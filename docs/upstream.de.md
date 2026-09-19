# Upstream und Herkunft

Die Tabelle dokumentiert die ursprünglichen Vendor-Stände. Sie ist keine Behauptung,
dass der heutige Quellbaum unverändert diesen Commits entspricht. CS-Retro-Änderungen
sind in der eigenen Git-Historie nachvollziehbar. Referenzbäume werden nicht als zweiter Client gebaut.

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


## Review 2026-09-13 (kein Vendor-Refresh)

Pins unverändert. Nur selektive Ports, kein HEAD-Merge.

| Upstream | Delta zum Pin | Übernommen | Nicht übernommen |
| --- | --- | --- | --- |
| FWGS/xash3d-fwgs | 55 Commits nach `1442d14` | `ec88a62` SDL3 `GetBasePath` nicht freigeben (`filesystem_engine.c`) | freevgui/MainUI, Vibration, `r_showtextures`, NS-`always_textinput`, Vita-CI, Listen-Rate-Reverts |
| Velaron/cs16-client | 5 Commits nach `bb60674` | `9c891b8` Scope/`TrueWidth` über `vid_width`/`vid_height` | `57607ab` GetGunPosition — bei uns schon so; YY-Thunks/XP, Vita, mainui |
| rehlds/ReGameDLL_CS | 0 — identisch `b088984` | — | — |
| rehlds/ReHLDS | nicht vendort (Xash ist die Engine) | — | Reconnect/Speedhack/bzip2 — nur beobachten, kein ReHLDS-Baum |

Schon vorhanden, nicht nochmal: `R_StudioGetPlayerState` braucht `currententity` (`dccfaf3`).

## PrimeXT (Technik-Upstream, kein Runtime-Pin)

[SNMetamorph/PrimeXT](https://github.com/SNMetamorph/PrimeXT) ist der primäre technische Upstream für Client-Renderer, Materialien, Licht, PostFX und ImGui-Tools. Es ist **kein** Engine-Ersatz und keine zweite `client.so`.

- Produktcode liegt in CS-Retro-Pfaden und darf umgebaut werden.
- `refs/primext/` ist der vorgesehene, nicht gebaute Vergleichsbaum (PX1). Noch kein Vendor-Pin.
- Erste produktive Integration erst PX2, nach bestandenem Movement-Gate.
- Updates: Fix verstehen → unsere Implementierung finden → in CS-Retro-Form übernehmen.
- Herkunft/Lizenz je Port festhalten, bevor Source oder Builds öffentlich verteilt werden.

Der frühere Beschluss „PrimeXT nur beobachten“ ist **ersetzt**.

## Selektive Integration

Wir vergleichen relevante Änderungen und übernehmen sie gezielt. Keine automatischen
Gesamt-Merges und kein stilles Update auf das neueste Upstream-HEAD.
Bitte dokumentiere bei Ports den genauen Commit, den übernommenen Umfang und die Tests.
Die [Architektur](architecture.md) beschreibt die Laufzeitrollen.

## Granatenreferenzen

Als Vergleich für Molotov-Waffenlogik, Menüanbindung und sichtbare Inferno-Effekte wurden verwendet:

- [languagelawyer/cs16-client: molotov](https://github.com/languagelawyer/cs16-client/tree/molotov)
- [languagelawyer/ReGameDLL_CS: molotov](https://github.com/languagelawyer/ReGameDLL_CS/tree/molotov)
- [languagelawyer/mainui_cpp: molotov](https://github.com/languagelawyer/mainui_cpp/tree/molotov)

Diese Branches sind bewegliche Referenzen, keine festgeschriebenen neuen Vendor-Pins.
Sie werden nicht vollständig übernommen; aus ihnen folgt auch keine automatische
Freigabe fremder Modelle. Weitere Credits und UI-Vergleichsquellen stehen im
[vollständigen Nachweis](https://github.com/benjarogit/csretro/blob/main/CREDITS.md).

## Announcer-Sounds

Aus [xonotic/xonotic-data.pk3dir](https://github.com/xonotic/xonotic-data.pk3dir) sind nur die WAVs
`prepareforbattle`, `5`–`1`, `begin` und `1minuteremains` vendort. Kein QuakeC, kein automatischer
Daten-Refresh. Notice: `data/ui-overrides/cstrike/sound/announcer/NOTICE.txt`.

[Credits](credits.md) · [Lizenzen](licenses.md)
