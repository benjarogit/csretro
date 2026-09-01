# Phase 3M — VGUI2 / Desktop-UI

Arbeitsdokument. 3A/3B/3C bleiben abgenommen. **3D/FOV startet nicht vor Abschluss von 3M.**
Release `v0.1.5` ist der historische ShowMenu-/MainUI-Baseline-Stand und wird nicht umgeschrieben.

Architektur: `docs/MENUS.md`.

## Ziel dieser Phase

Eine CS-Retro-Menü-Library (`client/menu/` → `menu_amd64.so` / `.dll` / `.dylib`):

- Xash `GetMenuAPI` (Hauptmenü)
- `CreateInterface` → `GameMenuExports001` (In-Game)
- Optik: **Steam-CS-1.6-VGUI2** (`.res` + Schemes + Localization)
- Funktion: NextClient-GameUI, ohne Steam/Win32-Runtime

Linux x86_64 zuerst.

**Visuelles Ziel:** möglichst originalgetreue Steam-CS-1.6-VGUI2-Oberfläche mit CS-Retro-Branding. Der aktuelle Text-/Rect-Bootstrap (rohe `GameUI_*`-Keys, Interim-Hauptmenü, Debug-Create-Server, Interim-Team) ist **nur V1-Funktionsnachweis** und **nicht** visuell abgenommen.

## V1 — verbindliche UI-Basis (Runtime-PoC bestanden)

**Gate geschlossen (2026-09-01).** Kein V1/V2-Vergleich mehr.

| Gate | Nachweis |
|------|----------|
| `vgui_controls` (Frame, Label, Button, TextEntry, …) | gelinkt in `menu_amd64.so` |
| Xash-Surface | `client/menu/vgui/surface_xash.cpp` |
| Xash-Input | `input_core.cpp` + `key_translation_xash.cpp` |
| `.res` / Scheme | PoC: `CsretroV1Poc.res`; GameUI-Default: **`TrackerScheme.res`** (ClientScheme nur HUD/Ingame) |
| FreeType-Glyphen | Face → Glyph → TGA → `pfnPIC_*` (`CSRETRO_V1POC_FREETYPE_GLYPHS`) |
| Maus / Tastatur / TextEntry / Tab / Escape / Resize | `./scripts/vgui-v1-poc-runtime.sh` |
| ASan+UBSan | `./scripts/build-menu.sh --sanitize` + derselbe PoC |
| keine Steam-/vgui2-/GameUI.dll-/Touch-Runtime | `ldd` / Log |

Skripte: `./scripts/build-menu.sh`, `./scripts/vgui-v1-poc-runtime.sh`, manuell `./scripts/play.sh`.

## Interface-Matrix (Ist)

| Interface | Ist-Implementierung | Hinweis |
|-----------|---------------------|---------|
| `IVGui` / `IPanel` | vendorter VGUI2-Core (`vgui.cpp`, `VPanel`, `VPanelWrapper`) in `menu_*` | 64-Bit-Patches; **kein** Steam-`vgui2` |
| `ISurface` / `ISurfaceNext` | `surface_xash.cpp` + `vgui_symbols.cpp` → `ui_enginefuncs_t` | FreeType mit Win32-naher Zellhöhe (`REAL_DIM`) + `FONTFLAG_ANTIALIAS`; Marlett = `vgui_symbols`; Classic-`GetProportionalBase` **640×480** (kein HD-Base im Classic-Gate) |
| `IInput` / `IInputInternal` | `input_core.cpp` + `key_translation_xash.cpp` | Xash Key/Mouse/Char |
| `IScheme` | vendort `Scheme.cpp` | Default **`TrackerScheme`** (GameUI); `ClientScheme` parallel geladen; Fonts über Resolver |
| `ISystem` | `system_xash.cpp` + `system_shell_posix.cpp` | **Offenes Windows-Plattform-Gate:** `system_shell_win.cpp` = No-Op-Stub für `Csretro_PlatformShellOpen` (blockiert Linux-3M nicht; vor Windows-Runtime-Gate muss ShellExecuteW real sein — kein permanenter Stub im Endprodukt). Posix: `xdg-open`/`open` |
| `ILocalize` | vendort `LocalizedStringTable.cpp` | **Pflicht:** echte Texte; rohe Keys = Fehler |
| `IFileSystem` / KV | `filesystem_xash.cpp`, `keyvalues_system.cpp` | |
| Controls | `vgui_controls` + NextClient-GameUI-Controls (`Cvar*`/`KeyToggle`) | weitere Controls nur bedarfsweise |
| Fonts | `font_resolver.cpp` | `CSRETRO_UI_FONTS` → gamedata `platform/resource/linux_fonts` → relative → System-**Verzeichnisse** |

**Nicht im Produkt-Build:** `SurfaceNext.cpp`, `System.cpp`, `FontReplace.cpp`, `InputWin32.cpp`, `vgui_internal.cpp`, `key_values_export.cpp`.

**Nicht:** externe Runtime `menu → vgui2.so → GameUI.so`.

## Font-Resolver (Linux, akzeptiert)

1. `CSRETRO_UI_FONTS`
2. `$XASH3D_RODIR/platform/resource/linux_fonts/`
3. relative GameData-Pfade
4. System-Fallback-**Verzeichnisse** (kein festes Distro-Einzel-TTF)

FreeType-Glyphenpfad bleibt verbindlich. Console-Text ist nicht das endgültige VGUI-Rendering.

## Surface-Erweiterungen

Nur bedarfsgesteuert (`PlaySound`, Texturen, `DrawTexturedPolygon`, Combo, QueryBox, …) — erst wenn eine echte NextClient-Seite oder Originalverhalten sie braucht.

## Referenzmatrix (Rekonstruktion)

Quellen: Steam-CS-1.6 lokal · `cstrike/resource` + `platform/resource` · NextClient GameUI · Ref B / FuryBaM · fwgs-vgui2-support · Ref A nur Desktop-Erkenntnisse · Xash MenuAPI. Ghidra nur bei unklaren ABI-/Abläufen (keine Projektdateien committen).

| Bereich | Original CS 1.6 | NextClient | Ref B | weitere Referenz | Ghidra? | CS-Retro-Ziel |
|---------|-----------------|------------|-------|------------------|---------|---------------|
| Main Menu | Steam `BasePanel` + `GameMenu.res` | `BasePanel.cpp` | VGUI-Menus | — | nur wenn Command-/OnlyInGame-Ablauf unklar | echte VGUI2-GameMenu, Localization, CS-Retro-Branding |
| Escape/Pause | In-Game Overlay | `CGameUI` Activate | Ref B pause | Xash key_menu | ggf. KeyDest | Escape → Pause-VGUI, nicht Bootstrap-Text |
| Options | PropertyDialog + Tabs | `OptionsDialog/*` | — | Steam `.res` | selten | echte Tabs; Apply/Cancel/Reset wie NextClient |
| Create Game | CreateMultiplayerDialog | `CreateMultiPlayerGameDialog` | — | — | selten | VGUI2 + gemeinsames `ServerProfile` |
| Server Browser | ServerBrowser | `ServerBrowser/*` | — | kein Steam-MM | ggf. LAN-Query | später; kein Steam-Matchmaking |
| Team Select | `UI/Teammenu.res` | HUD/ShowMenu | Ref B | — | wenn Viewport-Parent unklar | VGUI2 auf V1-Core |
| Class Select | `UI/Classmenu_*.res` | — | Ref B | — | wie Team | VGUI2 |
| Buy Menu | `UI/Buy*.res` | — | Ref B | — | wie Team | VGUI2 |
| Radio | `ShowMenu` / titles | — | — | — | nein | `ShowMenu` Legacy ok |
| Spectator | `UI/Spectator.res` | — | — | — | später | VGUI2 später |
| Scoreboard | `UI/ScoreBoard.res` | — | — | — | später | VGUI2 später |

## Rekonstruktionsplan (Reihenfolge)

1. **Options-Fundament:** `COptionsSubMouse` + `COptionsSubAudio` funktional/symbol-grün, **visuell noch nicht 1:1**. Zuerst gemeinsames **Metrics-/Scheme-/Font-Gate** schließen. Video danach.
2. **Main Menu:** NextClient/`GameMenu.res` als echte VGUI2-Controls; Localization fixen; Interim-Textliste ersetzen.
3. **Create Game:** `CreateMultiplayerGameDialog` + `ServerProfile` (eine Konfiguration).
4. **Team/Class/Buy:** `.res` + V1-Core; Interim-Renderer entfernen sobald ersetzt.
5. Escape/Pause, Browser, Spectator, Scoreboard danach.

Pro fertiger Dialoggruppe visueller Vergleich Steam-CS 1.6 bei 640×480, 800×600, 1024×768, einer 16:9.

## Options-Port (aktiv)

| Schritt | Status |
|---------|--------|
| Stub-Pages Mouse/Audio/Video | **entfernt** — keine Dummy-Tabs |
| NextClient Controls (`CvarToggle`/`Negate`/`Slider`/`TextEntry`/`KeyToggle`) | **portiert** → `client/menu/gameui/Controls/` + Xash `MenuEngine` |
| `COptionsSubMouse` | funktional abgenommen; **visuell Metrics-Gate offen** |
| `COptionsSubAudio` | funktional abgenommen; **visuell Metrics-Gate offen**; `MP3 volume *` original |
| Effektives Scheme (Runtime-Winner) | `gamedata/valve/resource/TrackerScheme.res` (= Current Steam `valve/…`); `platform/…/TrackerScheme.res` nur Fallback; `ClientScheme` parallel (HUD) |
| Effektive `.res` | Mouse/Audio unter `data/ui-overrides/cstrike/resource/` |
| CVar-Mapping Mouse | `m_filter` → `look_filter` |
| CVar-Mapping Audio | `hisound` → `room_hires` (Semantik 0/1 → 1/2); `mp3volume` → `MP3Volume` |
| Apply/Cancel/Reset/OK/Persistenz | `./scripts/vgui-options-mouse-gate.sh`, `./scripts/vgui-options-audio-gate.sh` |
| Localization | UTF-16→wchar_t; gameui/vgui/cstrike/platform |
| Video → … | **nächste** echte Subpage nach Dependency-Closure; kein Stub |
| Keyboard | Bind/Unbind Xash, keine Touch-first-UI |
| Video | Xash-Optionen, keine toten D3D/32-Bit-Einträge |
| CS-Retro-Advanced-Tab | leer bis Features existieren (kein FOV-UI vor FOV) |
| Create MP | nach stabiler Options-Grundlage |

## NextClient-GameUI — Inventar

Pfad: `client/nextclient/gameui/`. **Nicht** unser Produkt-Build (`-m32`, …).

| Komponente | Dateien | Wiederverwendbar | Runtime-Bind (entfernen) |
|------------|---------|------------------|--------------------------|
| `CGameUI` / `IGameUI` | `GameUi/GameUi.cpp` | Init, Activate | HWND / WndProc |
| `CBasePanel` / GameMenu | `BasePanel.cpp` | `GameMenu.res`, Commands | Steam-ISurface |
| Options | `OptionsDialog/*` | Tabs + CVar | Steam-vgui2 |
| Create MP | `CreateMultiPlayerGameDialog/*` | Server/Bot-Seiten | dasselbe |
| Controls | `GameUi/Controls/*` | Cvar-/Key-Controls | `engine->*` → Xash |
| Server Browser | `ServerBrowser/*` | Listen/LAN | Steam-MM |
| CEF | `Browser/*` | **nicht** in 3M | CEF |

## Xash-Adapter

| Xash | Verwendung |
|------|------------|
| `GetMenuAPI` / `UI_FUNCTIONS` | Pflicht |
| `ui_enginefuncs_t` | PIC, Fill, CVars, Cmds, Keys |
| `UI_GetMenuFactory` → `CreateInterface` | `GameMenuExports001` |
| `-menu` | Testdoverride |

## Originalressourcen (Game-Data)

COPY: `cstrike/resource/`, **`platform/resource/`** (Schemes, `vgui_*.txt`, `linux_fonts`). Overrides: `data/ui-overrides/`.

## Serverprofil

Gemeinsames Profil für Listen + Dedicated. Modules = `none` bis Module existieren.

## Stand (dieser Host)

| Stück | Status |
|-------|--------|
| V1-Runtime-PoC | **bestanden** |
| Menü-Lib | `menu_amd64.so` V1-Core + Controls + Xash-Backends |
| Options Mouse | funktional + Symbole OK; **visuell Metrics-Gate offen** |
| Options Audio | funktional + Symbole OK; **visuell Metrics-Gate offen**; `MP3 volume *` = Steam-Loc (behalten) |
| VGUI2 Symbol-Controls | **Gate grün** — `vgui_symbols.cpp` |
| VGUI2 Metrics/Scheme/Font | **in Arbeit** — Golden Ref Build 5971; siehe Abschnitt unten |
| Video | **gesperrt** bis Metrics-Gate grün |
| Windows ShellOpen | **offenes Plattform-Gate** (`system_shell_win.cpp` No-Op) |
| Hauptmenü / Create / Team | Interim-Bootstrap (Negativreferenz) |
| In-Game Team/Buy | Interim-`.res`-Pfad bis VGUI2-Ersatz |
| Tests | `vgui-v1-poc-runtime.sh`, `vgui-options-mouse-gate.sh`, `vgui-options-audio-gate.sh`, `play.sh`, `build-menu.sh --sanitize` |

## Golden Visual vs Current Steam (strikt getrennt)

### Golden Classic Visual Reference A

| Feld | Wert |
|------|------|
| Rolle | visuelle Zieloptik + screenshot-derived pixel metrics; proportional/HD-Interna **unknown** |
| Produkt | Steam Counter-Strike 1.6 |
| Build | **5971** (`Exe build: 11:45:32 Mar 1 2013` laut Console im Ref-Shot) |
| Sprache | English |
| Logische Auflösung | **1366×768** |
| Artifact | `docs/research/golden-5971/` (SHA-256 in README / Diagnose) |
| OptionsDialog rendered | **512×406** (x=825..1336, y=13..418) — Rendered-Pixel, nicht Constructor-Beweis |
| Belegt aus Shot | Tab **Mouse**; Video aktiv |
| Nicht belegt | 5971 HD/prop-State; Byte-Identität Scheme/Loc/GameUI mit Current Steam |

**Führender Classic-Dialog-Kandidat → Decision:** 512×406 Preferred (Fit geprüft). Endgültig implementieren erst nach Abnahme des Decision Reports in `docs/PHASE3M-METRICS-DIAGNOSIS.md`. Kein blindes `SetBounds`-Patch ohne Freigabe.

### Produktziel (dauerhaft)

Classic-Metrics-Gate = historische **Basis**, nicht Funktionsdeckel. Ziel: klassische CS-1.6-VGUI2-Optik/Bedienlogik + NextClient-Funktion + CS-Retro-Erweiterungen + moderne Desktop-/HiDPI-/Responsive-Schicht. Original = visuelle Baseline; NextClient = funktionale Basis; CS Retro ergänzt — Ebenen nicht gegeneinander ausspielen. Feature-UI nur mit Backend. Create Game über gemeinsames ServerProfile; Bots über Abstraktion; Module optional. Siehe `docs/MENUS.md`, Diagnose-Abschnitt „Produktziel“.

### Current Steam Resource / BuildMode Reference (dieser Host)

| Feld | Wert |
|------|------|
| Rolle | Live Build Mode, Resource-SHA, GameUI-Binary (**HL25-era/current HD reference**) |
| Steam AppID / appmanifest buildid | 10 / **12934623** |
| `cstrike/steam.inf` | `PatchVersion=1.1.2.7` |
| Engine-Binary-String (`hw.so`) | `Exe build: 01:35:13 Oct  8 2024` |
| Sprache (Steam UserConfig) | english |
| Scheme Runtime-Winner (CS Retro + gamedata) | `valve/resource/TrackerScheme.res` — SHA siehe Diagnose; **≠** Golden-5971-Beweis |
| Loc current | `GameUI_Mouse`=**Aim**; CS-Retro Classic-Pin Override → **Mouse** |
| Build Mode | Ctrl+Shift+Alt+B; Messwerte klar als **current** labeln |

Vergleichsshots nur bei **gleicher** Auflösung (640/800/1024/1280|1366).

Späterer transparenterer Steam-Stil = optionale Scheme-Variante **nach** korrektem klassischem Tracker-Stil.

## Nicht in 3M

FOV, Crosshair, HUD-/Radar-/Camera-/Inspect-Schalter ohne Backend. 3D. CEF-Hauptmenü. Ref-A-mainui. Steam-vgui2. Phase-3-Abschluss-Tag erst bei echter VGUI2-Optik + Funktion.
