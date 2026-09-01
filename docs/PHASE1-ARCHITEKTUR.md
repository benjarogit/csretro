# Phase 1 — Architektur: NextClient hinter `GetClientAPI`

Analyse + Gate. Kein Body-Code in dieser Datei.
Rollen: `docs/ROLLEN.md`. NextClient = funktionales Ziel. Ref A = Body-Quelle (A1, 2026-09-01).

## 1. Prämissen

1. Xash lädt genau eine Client-Bibliothek und erwartet den GoldSrc-Client-Vertrag (`cldll_func_t`).
2. NextClient liefert diesen Vertrag nicht. Es hängt sich an die geschlossene Steam-`client.dll` / `hw.dll` (8684, Windows x86).
3. Referenz A ist der Xash-fähige CS-Client-Unterbau (GPL-2+ / Valve-Ausnahme). **A1 (2026-09-01):** nur Allowlist als Body. NextClient bleibt die funktionale Zielbasis.
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

## 4. Referenz A (Body-Quelle A1, nicht Produktziel)

`refs/a-cs16-client/cl_dll/cdll_int.cpp` + `include/cl_dll.h`: Pflicht-Namen, `Initialize` kopiert `gEngfuncs`. Das ist der Unterbau, den NextClient aus Steam-`client.dll` vorausgesetzt hat.

A1 erlaubt später nur die Allowlist nach `client/body/`. Kein YaPB/ReGameDLL/mainui. Kein „cs16-client weiterentwickeln“.

## 5. Vergleich der Wege

| Weg | Urteil |
|-----|--------|
| **A. Eine Client-Lib: Export + Körper + NextClient-Module** | Form, die den Xash-Vertrag erfüllt, ohne Steam-Hooks | **Form empfohlen** |
| B. NitroApi auf Xash umschreiben | Braucht trotzdem einen Körper; Address-Provider wertlos | verwerfen |
| C. Ref A blank mergen (inkl. YaPB/ReGameDLL/mainui) | Vermischung, verboten | |
| D. Overlay ohne Körper | Xash hat nichts zum Laden | unmöglich |

Körper-Quelle **A1** (2026-09-01): Ref-A-Allowlist. Form A unverändert.

## 6. Entscheidung (Form A + Quelle A1)

**Eine** Client-Bibliothek. NextClient-Verhalten auf Xash, nicht zwei Clients.

```
Xash3D-FWGS
  → CS-Retro Client-Export
    → CS-Client-Body aus Ref A  (Phase 3: client/body/)
      → darauf integrierte NextClient-Funktionen
```

- **Export:** `GetClientAPI` / Pflicht-Namen. Kein NitroApi.
- **Körper:** A1-Allowlist, wird CS-Retro-Code (GPL-Attribution). `refs/a-cs16-client/` danach nicht als zweiter Client bauen.
- **Features:** aus `client_mini` lösen, `gEngfuncs` direkt, NextClient-Verhalten behalten; bei Redundanz eine CS-Retro-Implementierung (`docs/ROLLEN.md`).
- **Phase 3 Menü:** vorhandenes Xash-`libmenu.so` (`GetMenuAPI`). NextClient-GameUI/CEF nicht in Phase 3.
- **Phase 4:** NextClient-Menüs prüfen; Ref B nur wenn die nicht tragen — ein Feature, ein Diff.
- **NitroApi, steam_api_proxy, 8684-Provider, Launcher-als-cstrike.exe:** nicht der Bind-Pfad. Phase 2: entfernt bzw. deaktiviert.
- **Server:** weiter AMXX-Insel. Xash-`dlls/cs.so` ist eine eigene Phase-3-Entscheidung, kein Ref-A-ReGameDLL.

## 7. Was in Phase 2 / 3 angefasst wird

**Phase 2 (erledigt):** Schnitt der Overlay-Reste, siehe `docs/PHASE2-SCHNITT.md`. Bind-Architektur nicht wieder öffnen.

**Phase 3:** A1-Allowlist vendorn. Eine Lib. NextClient-Features auf den Unterbau, nicht cs16-client pflegen.

## 8. Anpassungen (Checkliste, kein Code)

| # | Wo | Was |
|---|-----|-----|
| 1 | `client/export/` | Vertrag steht (Phase 2). Phase 3: `GetClientAPI` + Pflicht-Namen |
| 2 | `client/body/` | Phase 3: A1-Allowlist; Xash-Header über `csretro_engine_headers` |
| 3 | `client_mini` | Features von NitroApi lösen; `GameHud` ohne `NitroApiInterface*` |
| 4 | `engine_mini` | kein Bind-Pfad; NCLM/Entity-Sync später einzeln bewerten |
| 5 | GameUI | Phase 3: Xash MainUI; GameUI/VGUI2 zurückstellen |
| 6 | CMake | `CSRETRO_BUILD_CLIENT` wird der neue Client, nicht NextClient-MSVC/vcpkg |
| 7 | `server/` | unangetastet bis Phase-3-GameDLL-Entscheidung |
| 8 | `refs/a-cs16-client/` | Referenz; Phase 3 nur Allowlist. YaPB/ReGameDLL/mainui nie |

## 9. Schwachstellen

- NextClient hat keinen Körper. A1 liefert ihn; das Produktziel bleibt NextClient.
- `GameHud` liest heute Valve-`gHUD` (Sprite-Liste freigeben). Das muss am eigenen HUD hängen.
- `engine_mini` fasst Engine-Interna (`cl`, `cls`, `sv`) — unter Xash nur über dokumentierte Engine-APIs, nicht über Pointer-Hooks.
- ncl-hl1 `IClientVGUI`/`IBaseUI` erzeugen falsche Sicherheit: sie ersetzen `GetClientAPI` nicht.
