# Schnittstellen

Bereiche inkludieren sich nur über diese Verträge.

## Engine → Client

Xash lädt `cstrike/cl_dlls/client_amd64.so` (Name: `docs/PLATTFORMEN.md`) in `CL_LoadProgs`:

1. `GetClientAPI(cldll_func_t *)` — einzig aktiver Tabellen-Export
2. sonst einzelne Namen aus `cdll_exports[]`
3. Pflicht: `pfnInitialize(&gEngfuncs, CLDLL_INTERFACE_VERSION)`

Vertrag: `engine/engine/cdll_exp.h`. CS-Retro füllt das in `client/export/csretro_cdll_export.cpp` (eigenes Struct, gleiches Layout). Ref-A-`F()` ist entfernt: deren `cldll_func_t` hat `HUD_GetPlayerTeam` dort, wo Xash `pfnGetRenderInterface` erwartet.

`Initialize` bekommt `gEngfuncs` direkt von Xash. Kein NitroApi-Laufzeitbind, keine Valve-`client.dll`, kein `hw.dll`, kein Steam, keine 8684-Annahme.

## Engine → Menü

`GetMenuAPI` (`cl_gameui.c`). Phase 3: Xash-`libmenu.so`. NextClient-GameUI/CEF nicht in Phase 3. `IClientVGUI` / `IBaseUI` ersetzen `GetClientAPI` nicht.

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
