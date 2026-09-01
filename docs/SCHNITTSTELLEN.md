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

**Entscheidung (Phase 1):** NextClient bleibt Overlay-Quelle, nicht der Lade-Export. CS Retro liefert **eine** Client-Lib mit `GetClientAPI` / Pflicht-Namen aus `cdll_exports[]` (jeder Name ist Pflicht, sonst bricht `CL_LoadProgs` ab). Innen: eigener Körper + aus `client_mini` gelöste Features, ein `gEngfuncs` aus `Initialize`. NitroApi/8684-Hooks sind nicht der Bind-Pfad. Details: `docs/PHASE1-ARCHITEKTUR.md`.

## Engine → Menü

Xash spricht MainUI über `GetMenuAPI` (`cl_gameui.c`), nicht über Source-`GameUI007` / `IBaseUI`.
Phase 3: vorhandenes Xash-`libmenu.so`. NextClient-GameUI (VGUI2/CEF, Steam-Factories) nicht in Phase 3.
`IClientVGUI` / `IBaseUI` ersetzen `GetClientAPI` nicht.

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
