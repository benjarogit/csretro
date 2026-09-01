# Phase 2 — Steam-/Hook-/Proxy-Schnitt

Schnitt ausgeführt 2026-09-01. Kein Body-Vendor. Architektur unverändert: Export → A1-Body → NextClient-Funktionen.

Inventar und was damit passiert ist. Feature-Quellen bleiben lesbar für Phase 3.

## Weg (Bind, nicht Feature)

| Was | Pfad | Aktion |
|-----|------|--------|
| Steam-API-Proxy (lädt `steam_api_orig.dll`) | `client/nextclient/steam_api_proxy/` | Verzeichnis entfernt |
| 8684-Adresstabellen (Steam `hw.dll` / `client.dll`) | `NclNitroApi/.../EngineAddressProvider8684*` , `ClientAddressProvider8684*` | Dateien entfernt; NitroApi liefert keine Provider mehr |
| Steam-ISteamMatchmakingServers-Compat | `engine_mini/.../MatchmakingSteamComp.*`, `SteamMatchmaking*`, `SteamServerListRequestData.h` | Dateien entfernt |
| Steam-Master | `assets/platform/defaults/MasterServer.vdf` Eintrag `Steam` / `hl1master.steampowered.com` | Eintrag entfernt |
| Launcher-Ladepfad `steam_api.dll` | `launcher/.../ClientLauncher.cpp` | Block entfernt |
| NextClient-CMake-Target `steam_api_proxy` | `client/CMakeLists.txt`, Launcher-Link | aus dem Build genommen |

NitroApi hängt sich nicht mehr an Steam-Binaries. `GetEngineAddressProvider` / `GetClientAddressProvider` geben `nullptr` zurück. SDL2-`sdl2.dll`-Hook (Steam-Nebenpfad) wird nicht mehr geladen. `RetrieveEngineBuildVersion` sucht nicht mehr nach `hw.dll`-Größe 8684.

## Behalten (Feature-Quelle, Phase 3 setzt auf den Body)

| Was | Pfad | Warum |
|-----|------|--------|
| GameHud, View, FOV, Inspect, Camera, Studio | `client/nextclient/client_mini/` | Zielverhalten |
| NCLM, HTTP-Download, Protector (Cmd-Logger) | `client/nextclient/engine_mini/` | Zielverhalten, kein Steam-Ladeweg |
| HTTP-/File-Master, Source-Query-Client | `engine_mini/.../matchmaking/master/` | Serverliste ohne Steam-API |
| Tsarvar-Eintrag in `MasterServer.vdf` | Assets | NextClient-HTTP/Query-Master, nicht Valve-Steam |
| GameUI / CEF | `client/nextclient/gameui/` | Feature-Quelle; Phase-3-Menü bleibt Xash-MainUI |
| NitroApi-Typen / Hook-Gerüst | `client/dep/NclNitroApi/include/` | `client_mini` liest die Typen noch; Bind ist tot |

## Altlast (Typen, kein Ladeweg)

Bleibt im Vendor-Tree, wird **nicht** gebaut, ist **nicht** der Xash-Bind:

- `ncl-hl1-source-sdk/public/steam/*`, `tier2/steam_api.cpp`, `Findsteam_api.cmake`
- `#include <steam/steam_api.h>` in GameUI / `MasterClientFactory` / `cl_main` (u. a. `SteamUtils()->GetAppID()`)
- `EngineMiniInterface::GetSteamMatchmakingServers()` — gibt `nullptr` zurück
- Windows-Launcher (`cstrike.exe`, `hw.dll`) — Overlay-Rest, nicht CS-Retro-Start

Diese Stellen werden beim Feature-Port in Phase 3/4 durch Xash-APIs ersetzt, nicht jetzt umgeschrieben.

## Xash-Anbindung (vorbereitet, nicht implementiert)

| Was | Wo |
|-----|----|
| Exportvertrag | `client/export/csretro_cdll_export.h` |
| CMake-Interface | `cmake/CsretroClient.cmake` → Target `csretro_client_export` |
| Engine-Vertrag | `engine/engine/cdll_exp.h`, Lader `CL_LoadProgs` |

Kein `GetClientAPI`-Rumpf, kein `client/body/`. Das ist Phase 3.

## Plattformen

Eine Client-Lib, Name plattformabhängig (`client.so` / `client.dll`) unter `cstrike/cl_dlls/`.  
Dieser Host: Linux x86_64. Später i686/aarch64 über vorhandene Toolchain-Dateien — Compiler dort noch nicht installiert.
