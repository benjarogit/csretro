# Schnittstellen zwischen den vier Bereichen

Bereiche inkludieren sich **nur** über diese Verträge, nicht über Ad-hoc-Pfade quer durch den Tree.

## Engine → Client

Xash lädt `cstrike/cl_dlls/client.so` (Name plattformabhängig) und holt Exports in `engine/engine/client/dll_int/cl_game.c`:

1. `GetClientAPI(cldll_func_t *)` oder gesicherte Variante `F`
2. sonst einzelne exportierte Funktionen
3. Pflicht: `pfnInitialize(&gEngfuncs, CLDLL_INTERFACE_VERSION)`

Vertrag: `engine/engine/cdll_exp.h` (`cldll_func_t`).
Erweiterungen gegenüber GoldSrc: `pfnGetRenderInterface`, Touch/Move/Look, Sound-API.

**Referenz A (nur lesen):** `refs/a-cs16-client/cl_dll/cdll_int.cpp` exportiert `Initialize` / `HUD_VidInit` / … — das ist das Muster, das Xash erwartet.

**NextClient (Basis) exportiert das nicht.** NextClient ist eine Hook-/Proxy-Schicht auf Steam `hw.dll` + `client.dll` (Build 8684, Windows x86): `engine_mini`, `client_mini`, `steam_api_proxy`, `filesystem_proxy`, Launcher als `cstrike.exe`. Phase 1 muss entscheiden, wie NextClient-Logik hinter `GetClientAPI` gelegt wird — nicht die Steam-Hooks nachbauen.

## Engine → Menü

Xash spricht MainUI über `GetMenuAPI`, nicht über Source-`GameUI007`.
NextClient-GameUI (VGUI2 + optional CEF) sitzt auf `IBaseUI` / `IClientVGUI` (`VClientVGUI001`, `BaseUI001`) aus `ncl-hl1-source-sdk`.

Das ist die zentrale Phasen-1-Frage: SDK-Interfaces vs. Xash-MainUI/VGUI-Support.

## Client → SDK (intern, Basis)

- `IClientVGUI` — `client/dep/NclNitroApi/dep/ncl-hl1-source-sdk/public/IClientVGUI.h`
- `IBaseUI` — dieselbe SDK-`public/`-Leiste
- NitroApi-Hooks: Windows-Address-Provider für Engine 8684 — unter Xash **wertlos**, nicht als Bindung missbrauchen

## Server

`server/` ist ein AMXX/Metamod-Modul (GoldSrc/ReHLDS), kein Xash-`dlls/cs.so`.
Protokollseite: NCLM / NextClient-Verifikation.

Xash-GameDLL (Spieler-Logik) ist **nicht** dieser Baum. Referenz A hat ReGameDLL — nicht kopieren. Server-Anbindung an Xash: eigene Entscheidung ab Phase 3, nicht stillschweigend ReGameDLL pullen.

## Bots → Server

Noch kein Vertrag. Später nur über eine dokumentierte Server-Schnittstelle, nicht durch `#include` aus `client/`.

## CMake-Interface-Targets

| Target | Gibt frei |
|--------|-----------|
| `csretro_engine_headers` | `engine/common`, `engine/public`, `engine/pm_shared`, `engine/engine` |
| `csretro_client_sdk_headers` | ncl-hl1-source-sdk `public/`, NitroApi `include/` |
