# Game-Menu / VGUI2 — Analyse (Schritte 1–6)

Stand 2026-09-01. Nur Analyse, keine Screens. Branch `vgui2cs16Menu`.
Scratch: `/home/benny/.cache/vgui2-analysis/` (nicht committen).

Kennzeichnung: **[ORIGINAL BEHAVIOR]** Ghidra/Steam · **[OPEN SOURCE]** einsehbarer Code · **[INFERRED]** plausibel, unbelegt · **[XASH IMPLEMENTATION]** eigener späterer Code.

---

## 1. Teil B — bestehendes In-Game-Menü (nicht anfassen)

Zwei verschiedene „In-Game“-Pfade existieren. Keiner von beiden ist das CS-1.6-**Game-Menu**.

### 1.1 Xash `vgui_support` = VGUI1-Host

**[OPEN SOURCE]** in diesem Tree: `engine/engine/client/vgui/vgui_draw.c`, `engine/engine/vgui_api.h`, `engine/3rdparty/vgui_support/`.

Ablauf:

1. `CL_LoadProgs` lädt **vor** der Client-SO `vgui_support` (`VGui_LoadProgs(NULL)`), außer `gameinfo.internal_vgui_support`.
2. Export `InitAPI` füllt `vguiapi_t` (Draw/Startup/Paint/Mouse/Key).
3. `vgui_int.cpp` startet VGUI1-`App` + `Panel` + `CEngineSurface`.
4. Der Client (Buy/Team/Scoreboard-VGUI1) zeichnet über diese Surface.

Das ist der klassische FWGS-Pfad. README-Zeilen „External VGUI support module“ meinen **genau das** — nicht GameUI007.

### 1.2 cs16-client / MainUI-Buy als Alternative

**[OPEN SOURCE]** `refs/a-cs16-client/3rdparty/mainui_cpp/menus/client/BuyMenu.cpp`: Buy-Menüs als `CMenuFramework`-Fenster, ausgelöst über `MENU_BUY*`. Das ist **nicht** Valve-VGUI1 und **nicht** GameUI.

### 1.3 Übertragbarkeit auf Teil A

| | Teil B | Teil A (Game-Menu) |
|--|--------|-------------------|
| Toolkit | VGUI1 (`vgui::Panel`, C-API) | VGUI2 (`vgui2::`, `CreateInterface`) |
| Host | `vguiapi_t` / `InitAPI` | `IBaseUI` / `IEngineVGui` / `IGameUI` |
| Modul | `vgui_support` + Client | `gameui` + `vgui2` |
| Engine-Stelle | `VGui_LoadProgs` in `CL_LoadProgs` | heute `GetMenuAPI` in `cl_gameui.c` |

Wiederverwendbar ist nur das **Muster**: Engine lädt ein Support-Modul, füllt eine Tabelle, ruft Startup/Paint/Input. Die ABI ist eine andere. Teil B **nicht** umbauen, um Teil A zu tragen — paralleler Host.

Altes Monorepo (Papierkorb / frühere Session): `menu_amd64.so` blieb für Buy/Team (`GameMenuExports001`); ein x64-`gameui_amd64.so` war Stage-0-ähnlich unter Xash. Das bestätigt: zwei Module, nicht ein Ersatz.

---

## 2. kungfulon/xash3d-fwgs `vgui2_support`

Klon: `/home/benny/.cache/vgui2-analysis/kungfulon-xash3d-fwgs` (shallow, ohne Submodule-Checkout).
Letzter Push: 2022-11-03. Gegen FWGS `master`: **52 ahead / 5033 behind**.

### 2.1 Geänderte Engine-Dateien (Diff vs. FWGS master)

Kern: `engine/client/vgui/vgui_draw.c`, `engine/vgui_api.h`, `engine/client/cl_game.c`, `engine/client/keys.c`, `ref_gl/gl_vgui.c`, `ref_soft/r_vgui.c`, `vgui_support/*`.

`vguiapi_t` bekommt Extra-Slots: `EngineFree`, `SetCursorPos`, `PlaySound`, `KeyForBind`, Texture-BGRA, `DrawCharacter`, `NeedKeyboard`. **[OPEN SOURCE]**

### 2.2 Was wirklich geladen wird

**Nicht GameUI007.** `BaseUI::Initialize` (`vgui_support/vgui2_surf.cpp`):

1. `Sys_LoadModule("vgui2." OS_LIB_EXT)` — **Steam-Binary**
2. `Sys_LoadModule("chromehtml." OS_LIB_EXT)`
3. Factories: Support, vgui2, filesystem, chrome, optional Client
4. `IClientVGUI` aus **client.dll** (`VClientVGUI001`)
5. Scheme `resource/trackerscheme.res`, Localize, `ivgui()->Start()`
6. `clientVGUI->Initialize` / `Start` / `SetParent(rootPanel)`

`IGameUIFuncs` ist ein Stub (`vgui2_gameui.cpp`). `ActivateGameUI` / `HideGameUI` / Console sind **leer**.

README-cs.md:

- `valve/cl_dlls/gameui.dll` **löschen**
- Original-DLLs kopieren (`vgui2`, `tier0`, `vstdlib`, `steam_api`, CEF, …)
- „Xash Extras for the **main menu**“
- Steam muss laufen, Account muss CS 1.6 besitzen

**[OPEN SOURCE]** kungfulon hostet VGUI2 für das **Original-client.dll** (Scoreboard/Fonts/`IClientVGUI`). Das Game-Menu bleibt MainUI. Das ist näher an „Original-Client unter Xash“ als an Teil A.

`IEngineVGui` / `PANEL_GAMEUIDLL`: kungfulon hat **kein** `GetPanel(2)`. Root ist ein eigenes `RootPanel`. Valve hängt GameUI an `IEngineVGui::GetPanel(PANEL_GAMEUIDLL)`. **[ORIGINAL BEHAVIOR]** vs. **[OPEN SOURCE]** — kungfulon bildet das nicht ab.

### 2.3 MainUI

Bleibt. `cl_gameui.c` lädt weiter `GetMenuAPI`. Koexistenz, kein Ersatz.

### 2.4 Linux-Build

Theoretisch `OS_LIB_EXT` = `.so`. Praktisch: README und FWGS-Doku verlangen **32-Bit-Steam-Bibliotheken**. Unser Produkt ist **x64** und darf Steam-`vgui2.so` nicht laden. kungfulon ist damit **kein reproduzierbarer Linux-x64-Produktpfad**. Build des 2022-Forks gegen aktuelle Waf/ref-Aufteilung: nicht sinnvoll (5000 Commits hinten).

---

## 3. MoeMod/Thanatos-Launcher

Klon: `/home/benny/.cache/vgui2-analysis/Thanatos-Launcher`. GitHub-Lizenz: **null**. Dateien tragen Valve-Copyright-Header. Windows (`io.h`, `direct.h`, `Win32Font`).

### 3.1 Verzeichnisse

| Pfad | Rolle |
|------|--------|
| `GameUI/` | `CGameUI`, `CBasePanel`, `CGameMenu`, Options, Create-Server, Console |
| `ServerBrowser/` | Internet/LAN/Favorites/History |
| `vgui2/` | Controls + Surface/Scheme (Valve-SDK-Abkömmling) |

### 3.2 Panel-Hierarchie / Commands (Abgleich)

**[OPEN SOURCE]** `GameUI_Interface.cpp` / `BasePanel.cpp`:

1. `CGameUI::Initialize` → `VGuiControls_Init("GameUI", factories)`
2. `new CBasePanel` → `SetParent(enginevguifuncs->GetPanel(PANEL_GAMEUIDLL))`
3. `IGameUIFuncs` + `IBaseUI` aus Engine-Factory
4. `IServerBrowser` parented an `CBasePanel`
5. `CBasePanel::CreateGameMenu` lädt `Resource/GameMenu.res`
6. Commands: `OpenServerBrowser`, `OpenOptionsDialog`, `Quit` / `QuitNoConfirm`, Create-Multiplayer

Das trifft die bereits bekannte Valve-Kette (CGameUI / CBasePanel / GameMenu.res / PANEL_GAMEUIDLL). **[ORIGINAL BEHAVIOR]** aus früherer hw.so-Analyse, hier im Quelltext sichtbar.

Thanatos ist die **Panel-Referenz** für Teil A. kungfulon übernimmt davon **nichts** — er lädt Binaries statt Panels.

### 3.3 Lizenz

Kein Copy in die GPL-Engine. Nur Struktur/Call-Flow lesen. `GameMenu.res` / Schemes sind Spieldaten (legaler CS-Bestand unter `gamedata/`), kein Quellcode.

---

## 4. FWGS `new_vgui_support_api` (PR #1346)

Offen, dirty, +293/−75, 7 Dateien. Doku `Documentation/vgui2-support.md`:

> Doesn't draw anything, doesn't take any input.

VGUI2-Init hinter proprietären 32-Bit-Libs (`vgui2`, `tier0`, `vstdlib`, CEF, `steam_api`). Linux-Spalte existiert, bleibt i386-Steam.

**Teil B:** ja — neue `vguiapi_t`, Adapter Legacy→Neu, `VGui_Startup` um VGUI2-Init erweitert. Wenn FWGS das mergen sollte, muss unser VGUI1-Buy-Pfad den neuen Adapter nutzen, ohne Verhalten zu ändern.

**Teil A:** nur die Host-API-Idee. Kein `IGameUI`, kein `PANEL_GAMEUIDLL`, kein Main-Menu. Überschneidung = „wie FWGS VGUI2 in `vgui_support` andocken will“, nicht GameUI-Panels.

---

## 5. Abgleich kungfulon / Thanatos / Steam / dieser Tree

| Punkt | Herkunft |
|-------|----------|
| Valve `hw.so` lädt `gameui.so` als `GameUI007`, hängt an `IEngineVGui::GetPanel(PANEL_GAMEUIDLL)` | **[ORIGINAL BEHAVIOR]** (ältere Ghidra-Doku; Dateien nicht in diesem Vendor-Tree) |
| kungfulon lädt `vgui2`+`chromehtml`+`IClientVGUI`, **nicht** GameUI007; löscht `gameui.dll` | **[OPEN SOURCE]** |
| kungfulon `ActivateGameUI` leer; MainUI bleibt Shell | **[OPEN SOURCE]** |
| Thanatos `CGameUI` + `CBasePanel` + `GameMenu.res` + ServerBrowser | **[OPEN SOURCE]** |
| NextClient-GameUI bereits vendort: `client/nextclient/gameui/` — dieselbe Hierarchie plus CEF, `EngineMini007`, `ISurfaceNext`, Win32 | **[OPEN SOURCE]** in diesem Tree |
| CSMoE `citrus` hat VGUI2 verworfen | **[OPEN SOURCE]** — toter Pfad |
| kungfulon-Engine-Patch 1:1 auf `engine/` (Pin `1442d14`) | **nicht** übertragbar (Layout/API 2022 vs. 2026) |
| Idee „IBaseUI-Host neben VGUI1, MainUI-Fallback“ | **[INFERRED]** / **[XASH IMPLEMENTATION]** |
| Eigenes x64-`ISurface`/`IVGui`/`IEngineVGui` ohne Steam-SO | **[XASH IMPLEMENTATION]** |
| Klassische Panels selbst bauen (kein Valve-cpp-Copy) | **[XASH IMPLEMENTATION]** |

hlsdk-portable#269: Xash „kann nicht direkt von VGUI2 in der Client-Library profitieren“, aber VGUI2 darf den Start nicht brechen. kungfulon umgeht das, indem die **Engine** VGUI2 hostet. Für Teil A brauchen wir denselben Gedanken, aber für **GameUI**, nicht für Steam-`vgui2.dll`.

---

## 6. Integrationsplan (dieser Tree)

### Übernehmen (Idee, nicht der 2022-Diff)

- Paralleler VGUI2-Host **neben** unangetastetem VGUI1-`vgui_support`.
- Factories: `IEngineVGui` (`GetPanel` 0/1/2), `IGameUIFuncs`, `IBaseUI` (oder schlanker Host, der `IGameUI` selbst startet).
- MainUI als Fallback, bis GameUI `ActivateGameUI` wirklich zeichnet.
- Client-Exports `pfnVGUI2DrawCharacter*` können später auf denselben Font-Pfad (kungfulon hat das für Original-client.dll). Für Teil A nicht Stufe 1.

### Nicht übernehmen

- kungfulon-Engine-Dateien nach `engine/` kopieren.
- Steam-`vgui2`/`gameui`/`chromehtml` laden.
- Thanatos-`vgui2/` oder Valve-Copyright-cpp einchecken.
- NextClient-CEF / `IGameUINext` / `next_engine_mini.dll` in dieser Phase.
- MainUI-Screens als Ziel-UI.

### Lizenz / Wiederverwendung

| Quelle | Technisch | Rechtlich |
|--------|-----------|-----------|
| kungfulon `vgui_support` VGUI2-Host | Idee ja, Patch nein | FWGS/GPL — Idee frei |
| Thanatos GameUI-Panels | beste klassische Struktur | **kein Copy** (keine LICENSE, Valve-Header) |
| NextClient `client/nextclient/gameui/` | schon im Vendor, aber Win32/CEF/`EngineMini007` | NextClient ohne LICENSE — privater Fork, trotzdem kein Valve-cpp nach `engine/` |
| ncl-hl1-source-sdk Header (`IBaseUI`, `IGameUI`, `IEngineVGui`) | ja, schon unter `client/dep/...` | Source-SDK-Lizenz; Header-Nutzung wie bestehender Vendor |
| Ghidra `hw.so`/`gameui.so` | ABI, wenn Quelle und Header widersprechen | nur Verhalten, kein Code |

Was aus Ghidra kommen muss (gezielt, später): Abweichungen Steam-`CGameUI`/`CTaskbar` vs. Thanatos/NextClient (Factory-Reihenfolge, Pause-Overlay, Console-Parent). Nicht jetzt, nicht pauschal.

### Konkrete Schichten (nach Freigabe)

```
Xash cl_gameui.c          GetMenuAPI (MainUI, Fallback)
        │
        └─ neu: GameUI-Host (x64)
              IEngineVGui + minimale VGUI2-Factories
              lädt eigene gameui-SO (nicht Steam)
                    CGameUI / CBasePanel / GameMenu.res
```

Teil B (`vgui_support` VGUI1) unverändert daneben.

---

## 7. Vorschlag erste testbare Stufe (warte auf Freigabe)

Keine Panels implementieren, bis dieser Plan bestätigt ist.

**Stufe A0 — Rechteck im Produktpfad**

1. Neue Dateien (nicht `3rdparty/vgui_support/` ändern): z. B. `engine/engine/client/vgui2/` Host + kleines x64-`gameui`-Modul außerhalb von NextClient-CEF.
2. Host: `CreateInterface(VEngineVGui001)`, `GetPanel(PANEL_GAMEUIDLL)` liefert ein Root-Handle.
3. GameUI: `GameUI007` / `GAMEUI_INTERFACE_VERSION_GS` — `Initialize` / `Start` / `ActivateGameUI` / `RunFrame`.
4. Ein gefülltes Rechteck + Logzeile. Input optional (ESC → Hide).
5. Schlägt Initialize fehl → bisheriges MainUI, kein Crash.
6. Test: Engine-Build dieses Worktrees + vorhandene `gamedata/`. Nicht Valve-`hw.so`, nicht `play-linux.sh` des alten Monorepos.
7. Danach erst: Main-Menu-Einträge aus `GameMenu.res` → Options → ServerBrowser → Console.

Linux zuerst, Interfaces so, dass Windows/macOS denselben Host nutzen (kein Win32 in der Host-Schicht).
