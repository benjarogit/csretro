# Phase 1 — Architektur: NextClient hinter `GetClientAPI`

Nur Analyse. Kein Code aus Referenz A oder B übernommen.
Rollen: Basis = `client/` · Engine = `engine/` · Ref A = lesen · Ref B = unberührt · Server = Lücke, keine Entscheidung.

## 1. Prämissen

1. Xash lädt genau eine Client-Bibliothek und erwartet den GoldSrc-Client-Vertrag (`cldll_func_t`).
2. NextClient liefert diesen Vertrag nicht. Es hängt sich an die geschlossene Steam-`client.dll` / `hw.dll` (8684, Windows x86).
3. Referenz A beweist, dass ein HL1-SDK-Client nativ unter Xash läuft — **kein Copy**.
4. `server/` ist AMXX/Metamod, nicht `dlls/cs.so`. Bleibt Phase 3, kein stiller ReGameDLL-Import.
5. Repo privat (NextClient ohne LICENSE, Engine GPL-3).

## 2. Was NextClient wirklich ist

Drei Schichten, keine davon ist ein Xash-Client:

| Schicht | Pfad | Job |
|---------|------|-----|
| Injektion | Launcher, `nitro_api`, Address-Provider 8684 | findet Funktionen in Steam-Binaries |
| Engine-Overlay | `nextclient/engine_mini/` | Hooks in `hw.dll`: FS, Netz, Master, Protector, GL, NCLM |
| Client-Overlay | `nextclient/client_mini/` | Hooks in Valve-`client.dll`: HUD_*, View, Studio, UserMsgs |

`client_mini` kopiert `gEngfuncs` / `cldll_func` / `gHUD` **aus NitroApi** (`client_mini/src/main.cpp`). Die Wrapper in `main.h` leiten `HUD_Redraw` usw. an `GetClientData()` weiter — also an Valve. Eigene Arbeit hängt als Pre/Post am Hook:

- nach `HUD_Init`: View/FOV/Inspect/Camera/`GameHud::Init`
- nach `HUD_Redraw`: `GameHud::Draw` (Crosshair, DeathNotice, Health, Radar, Ammo, Damage, Sprite-API)
- um `V_CalcRefdef`, `CL_CreateMove`, Studio-Interface, SetFOV, …

**Es gibt keinen vollständigen CS-Client in der Basis.** Waffen, Prediction, Entities, Input, Vanilla-HUD, Tempents sitzen in der Steam-`client.dll`, die unter Xash nicht existiert.

NitroApi unter Xash umzubiegen löst das nicht: ohne Körper gibt es nichts zu hooken, und die Adressen sind Windows/8684.

## 3. Was Xash verlangt

`engine/engine/client/dll_int/cl_game.c` → `CL_LoadProgs`:

1. optional `vgui_support` (`InitAPI` / `InitVGUISupportAPI`) — VGUI1, nicht `IClientVGUI`
2. Client-Lib laden
3. `GetClientAPI(cldll_func_t*)` oder `F` (secured), sonst **alle** Namen aus `cdll_exports[]`
4. **Jeder** fehlende Eintrag in `cdll_exports` → Load-Abbruch (die „essential“-Liste ändert nur die Fehlermeldung)
5. `pfnInitialize(&gEngfuncs, CLDLL_INTERFACE_VERSION)` muss 1 liefern

Pflicht-Namen (`cdll_exports`):  
`Initialize`, `HUD_VidInit`, `HUD_Init`, `HUD_Shutdown`, `HUD_Redraw`, `HUD_UpdateClientData`, `HUD_Reset`, `HUD_PlayerMove`, `HUD_PlayerMoveInit`, `HUD_PlayerMoveTexture`, `HUD_ConnectionlessPacket`, `HUD_GetHullBounds`, `HUD_Frame`, `HUD_PostRunCmd`, `HUD_Key_Event`, `HUD_AddEntity`, `HUD_CreateEntities`, `HUD_StudioEvent`, `HUD_TxferLocalOverrides`, `HUD_ProcessPlayerState`, `HUD_TxferPredictionData`, `HUD_TempEntUpdate`, `HUD_DrawNormalTriangles`, `HUD_DrawTransparentTriangles`, `HUD_GetUserEntity`, `Demo_ReadBuffer`, `CAM_Think`, `CL_IsThirdPerson`, `CL_CameraOffset`, `CL_CreateMove`, `IN_*`, `V_CalcRefdef`, `KB_Find`.

Optional (`cdll_new_exports`): Studio, Director, Voice, ChatInput, Xash-Render/Clip/Touch/Sound.

Menü ist **ein zweiter Ladeweg**: `UI_LoadProgs` sucht `GetMenuAPI` (`cl_gameui.c`). Nicht `GameUI007`, nicht `IBaseUI`. NextClient-GameUI (`gameui/`) hängt an Steam-Factories, `IEngineVGui`, `next_engine_mini.dll`, HWND — unter Xash nicht startbar.

SDK `IClientVGUI` / `IBaseUI` sind **kein** Xash-Client-Bind. Relevant erst, wenn GameUI später gehostet werden soll.

## 4. Referenz A (nur gelesen)

`refs/a-cs16-client/cl_dll/cdll_int.cpp` + `include/cl_dll.h`: dieselben `DLLEXPORT`-Namen, `Initialize` kopiert `gEngfuncs`, prüft `CLDLL_INTERFACE_VERSION`. CMake baut `client` als Shared Lib für Xash. Zusätzlich optionale Xash-Exports (`HUD_GetRenderInterface`, Mobility/`HUD_MobilityInterface`, MenuFactory).

Das ist das **Muster der Bindung**, nicht eine Importquelle.

## 5. Vergleich der Wege

| Weg | Urteil |
|-----|--------|
| **A. Eigener Export + eigener Körper + NextClient-Module** | Einziger Weg, der den Xash-Vertrag erfüllt, ohne Ref A zu kopieren und ohne Steam-Hooks | **Empfohlen** |
| B. NitroApi auf Xash umschreiben | Braucht trotzdem einen Körper; Address-Provider wertlos; Windows-zentriert | verwerfen |
| C. Ref-A-Client mergen | verboten | |
| D. Overlay ohne Körper | Xash hat nichts zum Laden | unmöglich |
| E. Fremden Portable-Client als Körper vendoren | neue Rollen-Entscheidung, nicht Ref A; nur wenn A am Körper scheitert | später, nicht still |

## 6. Entscheidung (Empfohlen)

CS Retro bekommt **eine** Client-Bibliothek, die Xash lädt (`cstrike/cl_dlls/client.so`). Innen drei Teile, ein Prozess, ein `gEngfuncs` aus `Initialize`:

```
Xash  --GetClientAPI / Named Exports-->  client/export/
                                              |
                         +--------------------+--------------------+
                         |                                         |
                   client/body/                              client/features/
                   (CS-Retro-Körper:                         (aus NextClient gelöst:
                    Pflicht-Exports,                          GameHud, View, FOV,
                    Prediction, Entities,                     Inspect, Studio-Overrides,
                    Vanilla-HUD, Input)                       NCLM-Clientseite)
```

- **Export** füllt `cldll_func_t` bzw. exportiert die Pflicht-Namen. Kein NitroApi.
- **Körper** ist neue CS-Retro-Implementierung. Ref A nur für Aufrufreihenfolge/Vertrag lesen. Kein Diff aus `refs/`.
- **Features** kommen aus `client_mini` (und später ausgewählte `engine_mini`-Teile wie NCLM), umgeschrieben auf direkte `gEngfuncs`-Aufrufe statt Hooks.
- **Phase 3 Menü:** vorhandenes Xash-`libmenu.so` (`GetMenuAPI`). NextClient-GameUI/CEF nicht in Phase 3.
- **Phase 4:** NextClient-Menüs prüfen; Ref B nur wenn die nicht tragen — ein Feature, ein Diff.
- **NitroApi, steam_api_proxy, 8684-Provider, Launcher-als-cstrike.exe:** nicht der Bind-Pfad. Phase 2 entfernen oder ersetzen.
- **Server:** weiter AMXX-Insel. Xash-`dlls/cs.so` ist eine eigene Phase-3-Entscheidung, kein Ref-A-ReGameDLL.

## 7. Was in Phase 2 / 3 angefasst wird

**Phase 2 (Steam raus, kein Körper-Schreiben):** Inventar und Schnitt der Overlay-Reste — `steam_api_proxy`, Master/Tsarvar, `tier2/steam_api.cpp`, Protector soweit Steam, CEF-Pfade. Bind-Architektur nicht wieder öffnen.

**Phase 3 (minimal lauffähig):** Export + kleinster Körper, der alle `cdll_exports` bedient + Connect/Render/Input. Features nur soweit nötig, damit der Körper nicht leer ist (Vanilla-HUD). NextClient-HUD-Extras danach, einzeln.

**Körper-Umfang (offen, nicht jetzt bauen):** Ein kompletter CS-Client ist groß. Phase 3 kann mit Minimal-Körper (Init, leeres HUD, Move/Input-Stubs die nicht crashen, Connect) starten und den Körper schrittweise füllen. Nicht Ref A als Abkürzung.

## 8. Anpassungen (Checkliste, kein Code)

| # | Wo | Was |
|---|-----|-----|
| 1 | `client/` neu | `export/` mit `GetClientAPI` + Pflicht-Namen |
| 2 | `client/` neu | `body/` — eigene Implementierung, Xash-Header nur über `csretro_engine_headers` |
| 3 | `client_mini` | Features von NitroApi lösen; `GameHud` ohne `NitroApiInterface*` |
| 4 | `engine_mini` | kein Bind-Pfad; NCLM/Entity-Sync später einzeln bewerten |
| 5 | GameUI | Phase 3: Xash MainUI; GameUI/VGUI2 zurückstellen |
| 6 | CMake | `CSRETRO_BUILD_CLIENT` wird der neue Client, nicht NextClient-MSVC/vcpkg |
| 7 | `server/` | unangetastet bis Phase-3-GameDLL-Entscheidung |
| 8 | `refs/` | weiter eingefroren |

## 9. Schwachstellen

- Der Körper existiert noch nicht. Phase 3 ist deshalb größer als „NextClient kompilieren“.
- `GameHud` liest heute Valve-`gHUD` (Sprite-Liste freigeben). Das muss am eigenen HUD hängen.
- `engine_mini` fasst Engine-Interna (`cl`, `cls`, `sv`) — unter Xash nur über dokumentierte Engine-APIs, nicht über Pointer-Hooks.
- ncl-hl1 `IClientVGUI`/`IBaseUI` erzeugen falsche Sicherheit: sie ersetzen `GetClientAPI` nicht.
