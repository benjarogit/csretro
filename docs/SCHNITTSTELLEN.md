# Schnittstellen

Bereiche inkludieren sich nur über diese Verträge.

## Engine → Client

Xash lädt `cstrike/cl_dlls/client_amd64.so` (Name: `docs/PLATTFORMEN.md`) in `CL_LoadProgs`:

1. `GetClientAPI(cldll_func_t *)` — einzig aktiver Tabellen-Export
2. vollständige Pflicht-Callback-Tabelle gemäß `engine/engine/cdll_exp.h`
3. `pfnInitialize(&gEngfuncs, CLDLL_INTERFACE_VERSION)`

Vertrag: `engine/engine/cdll_exp.h`. CS-Retro füllt das in `client/export/csretro_cdll_export.cpp` (eigenes Struct, gleiches Layout). Ref-A-`F()` ist entfernt: deren `cldll_func_t` hat `HUD_GetPlayerTeam` dort, wo Xash `pfnGetRenderInterface` erwartet.

Kein Einzel-Symbol-, Secured-Client-, Mobility-, Touch-, Sound- oder Voice-Fallback. Fehlt `GetClientAPI` oder ein Pflicht-Callback, bricht der Ladevorgang mit einem Fehler ab. Zusätzliche Engine/Client-Funktionen werden erst nach einer bewussten Erweiterung dieses gemeinsamen, versionierten Produktvertrags aufgenommen.

`Initialize` bekommt `gEngfuncs` direkt von Xash. Kein NitroApi-Laufzeitbind, keine Valve-`client.dll`, kein `hw.dll`, kein Steam, keine 8684-Annahme.

## Steam → Game-Data (kein Runtime-Bind)

Steam CS 1.6 ist nur lokale Dateiquelle (VDF/`appmanifest_10.acf`). Kein SteamAPI, keine `steam_api`-Lib, kein Hook in Engine/Client/GameDLL.

Nach dem Bootstrap zeigt `XASH3D_RODIR` auf den CS-Retro-Datenbaum. Die Steam-Half-Life-Installation ist read-only und zur Laufzeit nicht nötig. Details: `docs/GAMEDATA.md`.

## Engine → Menü

Xash `MenuFactory` ist ein plattformübergreifendes Native Object und liefert den `CreateInterface`-Pointer der geladenen Menü-Lib (`cl_gameui.c` / `UI_GetMenuFactory`). Das ist keine VGUI2-Implementierung.

**3M-Ziel:** eine Lib `client/menu/` mit `GetMenuAPI` **und** `CreateInterface` (`GameMenuExports001`). Xash-MainUI nur Bootstrap. NextClient-GameUI ist Port-Quelle, nicht Runtime-`GameUI.dll`. `IClientVGUI` / `IBaseUI` ersetzen `GetClientAPI` nicht. Kein Steam-`vgui2`. Details: `docs/MENUS.md`, `docs/PHASE3M.md`.

**3C-Baseline:** MainUI ohne `CreateInterface`; In-Game `ShowMenu`. `IGameMenuExports` optional (kein Modal). `ShowMenu` bleibt Legacy.

## Engine → GameDLL

`GiveFnptrsToDll` + `GetEntityAPI` (`sv_game.c`). `GetEntityAPI2` ist im Vendor nicht exportiert; Xash nutzt die Legacy-API. Funktionsnamen: `unsigned long` wie `engine/engine/eiface.h`. Eine Lib für Listen und Dedicated. Override: `-dll`. Details: `docs/SERVER.md`.

## Client intern

NextClient-`client_mini` bleibt Port-Quelle. NitroApi-Typen dürfen Compile-Hilfe sein, nicht Laufzeitbind. Nach verifiziertem Feature-Port: Hook-Weg löschen.

SDK-`IClientVGUI` / `IBaseUI` sind kein Xash-Client-Bind.

## Servermodule / Bots

Module optional, nicht in die GameDLL gebacken. ZBot liegt vorübergehend **in** `server/game/` (Migration). Ziel: Interface nach `bots/`. Nicht aus `client/` inkludieren.

## Analyse (Ghidra)

Erlaubt, wenn Quelle und Refs das Originalverhalten nicht klären: CS-Client, GameDLL, Menü, Exports, ABI/Structs. Ergebnis in Code oder Tests. Keine dauerhaften Ghidra-Projekte im Repo.

## CMake-Targets

| Target | Gibt frei |
|--------|-----------|
| `csretro_engine_headers` | Xash-Header — **nicht** `-I` für den A1-Body |
| `csretro_client_sdk_headers` | ncl-hl1 `public/`, NitroApi `include/` (Port-Quelle) |
| `csretro_client_export` | `client/export/` |
| `csretro_client` | Body + Export → eine 64-Bit-Lib |
| `csretro_gamedll` | ReGameDLL-Körper → `cs_amd64.so` / `.dll` / `.dylib` |
