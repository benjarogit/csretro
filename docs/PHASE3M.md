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

**Verbindliche Reihenfolge, bestätigt 2026-09-05:** GameUI und In-Game-UI werden innerhalb von 3M funktional vervollständigt und anschließend visuell fertig ausgearbeitet. Die ersten Look-Scheiben von Team/Class/Buy/Radio/Spectator/Scoreboard sind Zwischenstände, keine pauschale finale Design-Abnahme. PlayerList gehört noch zur funktionalen Menüarbeit; danach folgen Aufteilung, Spalten, Schrift, Abstände, Icons und Zustände der In-Game-Flächen. Erst nach dieser gemeinsamen Menü-Abnahme beginnen die weiteren Feature-Phasen; deren Optionen werden in die fertige Menüstruktur integriert.

**Visuelles Ziel:** Feeling CS-1.6/VGUI2, Dialog-Chrome wie CS:Source (`docs/MENUS.md`). Standard: **eine** gerundete Hülle, innen 90°. Combo ohne Dauer-Fill, Listen-Padding, LAN-Empty-Text einmal bleiben. **Abnahme 2026-09-04:** Der Inhaber ist mit der aktuellen Optik von Hauptmenü, Options, Create Game und LAN-Server-Browser vollständig zufrieden; dieser Stand ist die aktuelle visuelle Produktbaseline. **Team-Wahl visuell bestätigt 2026-09-07 — nicht anfassen.** Class-Wahl **visuell bestätigt 2026-09-07**. Buy **AUTOMATED PASS, visuelle Abnahme 2026-09-07 abgelehnt**; Feinschliff aktiv (CS:GO-Aufbau, CS-1.6-Katalog, `w_*.mdl` Gold-Seitenprofil über denselben Studio-Pfad wie Class/Team, Figur = eigene Klasse).

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
| Fonts | `font_resolver.cpp` | **Noto Sans** (mitgeliefert): `CSRETRO_UI_FONTS` → gamedata `platform/resource/csretro_fonts` → relative → System-**Verzeichnisse** (nur Notnagel) |

**Nicht im Produkt-Build:** `SurfaceNext.cpp`, `System.cpp`, `FontReplace.cpp`, `InputWin32.cpp`, `vgui_internal.cpp`, `key_values_export.cpp`.

**Nicht:** externe Runtime `menu → vgui2.so → GameUI.so`.

## Font-Resolver (akzeptiert)

**UI-Familie: Noto Sans** (SIL OFL 1.1), mitgeliefert unter `data/ui-overrides/platform/resource/csretro_fonts/`.
Keine System-/Steam-Font-Abhängigkeit; Windows/macOS bekommen dieselbe Optik.

Suchreihenfolge:

1. `CSRETRO_UI_FONTS`
2. `$XASH3D_RODIR/platform/resource/csretro_fonts/`
3. relative GameData-Pfade
4. System-Font-**Verzeichnisse** — nur Notnagel, wenn die mitgelieferten Dateien fehlen

Mapping (deterministisch, keine `<Family>.ttf`-Ratepfade):

| Scheme-Name | Datei |
|-------------|-------|
| Tahoma / Verdana / Trebuchet MS / … | `NotoSans-Regular.ttf`, weight ≥600 → `NotoSans-Bold.ttf` |
| Courier / Consolas / Lucida Console | `NotoSansMono-Regular.ttf` |
| Marlett | geometrisch in `vgui_symbols.cpp` — nie Datei |

`TrackerScheme.res` bleibt unangetastet (Steam-Original sagt weiter „Tahoma“); die Zuordnung passiert im Resolver.
Steam-`platform/resource/linux_fonts` wird nicht mehr importiert und aus bestehenden Bäumen entfernt (`prune`-Regel im Manifest).

FreeType-Glyphenpfad bleibt verbindlich. Console-Text ist nicht das endgültige VGUI-Rendering.

## Surface-Erweiterungen

Nur bedarfsgesteuert (`PlaySound`, Texturen, `DrawTexturedPolygon`, Combo, QueryBox, …) — erst wenn eine echte NextClient-Seite oder Originalverhalten sie braucht.

## Referenzmatrix (Rekonstruktion)

Quellen: Steam-CS-1.6 lokal · `cstrike/resource` + `platform/resource` · NextClient GameUI · Ref B / FuryBaM · css-community nur In-Game Class/Buy-Vergleich (beobachten, nicht Engine) · `TEMP_EXTRA/` lokal (Source SDK 2013 / CS:GO cstrike15; beobachten, nicht Engine, nicht 3M-Merge) · fwgs-vgui2-support · Ref A nur Desktop-Erkenntnisse · Xash MenuAPI. Ghidra nur bei unklaren ABI-/Abläufen (keine Projektdateien committen).

| Bereich | Original CS 1.6 | NextClient | Ref B | weitere Referenz | Ghidra? | CS-Retro-Ziel |
|---------|-----------------|------------|-------|------------------|---------|---------------|
| Main Menu | Steam `BasePanel` + `GameMenu.res` | `BasePanel.cpp` | VGUI-Menus | — | nur wenn Command-/OnlyInGame-Ablauf unklar | echte VGUI2-GameMenu, Localization, CS-Retro-Branding |
| Escape/Pause | In-Game Overlay | `CGameUI` Activate | Ref B pause | Xash key_menu | ggf. KeyDest | **VGUI2 da + Inhaber abgenommen** — Blur der Welt, mittig „Pausiert“, `GameMenu.res` zentriert; kein `setpause` auf MP |
| Options | PropertyDialog + Tabs | `OptionsDialog/*` | — | Steam `.res` | selten | echte Tabs; Apply/Cancel/Reset wie NextClient |
| Create Game | CreateMultiplayerDialog | `CreateMultiPlayerGameDialog` | — | — | selten | VGUI2 + gemeinsames `ServerProfile` |
| Server Browser | ServerBrowser | `ServerBrowser/*` | — | kein Steam-MM | LAN: `localservers` | **LAN-VGUI2 da**; Internet-Tab bewusst nicht — eigene Serverliste fehlt noch (`docs/SERVER.md`) |
| Team Select | `UI/Teammenu.res` | HUD/ShowMenu | Ref B | — | wenn Viewport-Parent unklar | **VGUI2 da** (`CTeamSelectPanel`, Gate PASS) |
| Class Select | `UI/Classmenu_*.res` | — | Ref B | css-community (CS:S In-Game, beobachten) | wie Team | **VGUI2 da** (`CClassSelectPanel`, Gate PASS) |
| Buy Menu | `UI/Buy*.res` | — | Ref B | css-community (CS:S In-Game, beobachten) | wie Team | **VGUI2 da** (`CBuySelectPanel`, Gate PASS gemeinsames Raster + direkter Kauf) |
| Radio | `ShowMenu` / `titles.txt` | — | — | CS:GO Scaleform `RadioPanel` (`TEMP_EXTRA/cstrike15_src`) | nein | **eigene HUD-Karte** (`CRadioSelectPanel`) — nicht Team-Viewport |
| Spectator | `UI/Spectator.res` | — | Ref B `vgui_SpectatorPanel` | Broadcast-Rahmen 1:1 soweit Daten da; kein Faceit-Erfinden | nein | **eigene Fläche da** (`CSpectatorHudPanel`) — Rahmen, nicht Team-Viewport |
| Scoreboard | `UI/ScoreBoard.res` | — | Ref B `vgui_ScorePanel` | Broadcast-Mid-Tafel (Idee) | nein | **eigene Fläche da** (`CScoreboardHudPanel`) — nicht Team-Viewport |
| Command-Menü | klassisches `+commandmenu` | Binding vorhanden | Ref B / GoldSrc | moderne hierarchische Schnellaktion | bei unklarem Engine-Ablauf | **UI fehlt**; Binding allein ist kein Feature |
| AMX-/Plugin-Menüs | serverseitiges `ShowMenu` | — | AMXX/Metamod | dynamische Plugin-Einträge | nur bei Protokollunklarheit | **Legacy-Transport erhalten, moderne dynamische Hülle offen** |
| Bot-Menü | abhängig vom Bot-Backend | — | YaPB nur Referenz | administrative Bot-Aktionen | nein | **Backend und UI fehlen**; keine wirkungslosen Knöpfe bauen |

## Rekonstruktionsplan (Reihenfolge)

1. **Options-Fundament:** Mouse + Audio + Video-Basis **PASS / Regression**. **Brightness/Gamma bleibt FUNKTIONAL OPEN und ist auf einen späteren Featureblock verschoben; die bisherigen automatischen Nachweise ersetzen den fehlgeschlagenen Inhaber-Test nicht.** Keyboard **AUTOMATED PASS / MANUAL RECHECK OPEN** nach Persistenz-/Config-Isolation-Fix. Provenance PASS. Adaptive Layout / Resize **AUTOMATED PASS / MANUAL ACCEPTANCE OPEN** (`docs/PHASE3M-LAYOUT.md`). Optik **VISUAL ACCEPTED 2026-09-04**. `docs/PHASE3M-KEYBOARD.md`.
2. **Main Menu: AUTOMATED PASS / VISUAL ACCEPTED 2026-09-04.** `GameMenu.res` als echte VGUI2-Controls (`client/menu/vgui/main_menu.cpp`, Muster NextClient `CBasePanel`/`CGameMenu`/`CGameMenuItem`); Localization über `Label::SetText`-`#`-Pfad statt Interim-`Menu_L`; Interim-Textliste entfernt. Gate: `./scripts/vgui-mainmenu-gate.sh`.
3. **Create Game: Server / Game / Fairness AUTOMATED PASS / VISUAL ACCEPTED 2026-09-04.** Tabs Server/Game/Fairness; CS:Source-Hierarchie (eine Hülle, innen eckig), Combo ohne Dauer-Markierung und konsistente Listen-Achse/Padding. Gate: `./scripts/vgui-creategame-gate.sh` (inkl. Label-Audit `CSRETRO_CREATE_LABELS`).
   - **Server-Seite** (Layout `data/ui-overrides/cstrike/resource/CreateGameServerPage.res`): Map, Identity-Liste (`hostname` / `maxplayers` / `sv_password`) und Bots. Maps über `FindFirst("maps/*.bsp")`. Bots nur wählbar, wenn die Map ein `.nav` hat — sonst gesperrt mit Hinweis. `EnableSteamNetworkingCheck` und CZ-Tutor bewusst nicht übernommen (kein Backend).
   - **Game-Seite:** `ScrGroup::Rules` (Runde/Zeit/Geld) aus `cstrike/settings.scr`.
   - **Fairness-Seite:** `ScrGroup::Fairness` (Team/Bestrafung/Zuschauer). Tab-Titel `#CsretroGameUI_Fairness`.
   - **Liste:** `CreateGameSettingsList` baut Zeilen aus `settings.scr` (Beschriftung **in der Zeile**, `SetFirstColumnWidth(0)`). Classic-Maße aus NextClient `ScriptObject` (Zeile 28, Control 24, Prompt `wide/2+20`, Zahlenfelder 72px). `ServerSettingsScript` bleibt der Parser — ohne dessen Config-Schreibpfad (`docs/UPSTREAM.md`). Grenzen aus dem Script greifen beim Übernehmen (`mp_roundtime` 99→15). Getippt bleiben nur Hostname/Slots/Passwort.
4. **Server Browser: LAN AUTOMATED PASS / FUNKTION AKZEPTIERT.** Leere LAN-Liste erwartet. `CServerBrowserDialog` teilt die CS:Source-Chrome (Frame rund, Liste eckig); Empty-Text nur in der Liste, nicht noch einmal in der Statuszeile. Gate: `./scripts/vgui-serverbrowser-gate.sh`. Internet-Tab bewusst nicht — eigene Serverliste fehlt noch (`docs/SERVER.md`).
5. **Team/Class/Buy/Radio:** Funktional automatisiert. **Team und Class visuell bestätigt 2026-09-07 — nicht anfassen.** Buy-Look **abgelehnt**, Referenzumbau aktiv: CS:GO-Komposition mit CS-1.6-Katalog. Die Hauptseite liegt auf einer zentrierten, nach oben begrenzten 1280×720-Referenzbühne und zeigt nummerierte Kategorien/Produkte als einzeln gerahmte Karten; Geld und Buy-Time stammen aus dem Client-HUD. Rechts steht die eigene Klasse aus dem HUD-Modell, gleicher `SetPreview`-Pfad wie Team/Class (Yaw 206, Sequenz 80 T / 33 CT, `p_ak47`/`p_m4a1`) — Figur nicht anfassen. Jede Kaufkarte zeichnet ihr `w_*.mdl` direkt mit derselben `pfnRenderScene`-Kamera wie Class/Team; die Engine mappt die GoldSrc-Idle-Box (dünnste Achse = Blick, längste = waagerecht) und flacht die Dicke, damit ein lesbares Seitenprofil entsteht statt Euler-Klecks; ein menüspezifischer Studio-Farbpfad macht die Geometrie gold, ohne TGA, Sprite oder GlowShell. Kacheln: Nummer am linken Rand, Name oben links, Silhouette mittig, Preis unten rechts, Hover helleres Grau. Auch auf 800×600 bleiben Raster und Figur proportional nebeneinander. Der visuelle Gate kauft Glock direkt aus dem gemeinsamen Raster und öffnet keine alte Pistolen-Unterseite. Der alte Baum `cstrike/gfx/vgui` ist amputiert. Geöffnete Vollbild-Overlays folgen echten Mode-Änderungen statt oben links kleben zu bleiben; Buy-Gates sind bei 800×600, 1280×720 und 1920×1080 sowie Live-Resize 1024×768→1280×720 grün. `TEMP_EXTRA/cstrike15_src`/`hl2_src` wurden für Zustands-/Lifecycle-Verträge gelesen; die fehlende editierbare Scaleform-Komposition wird nicht vorgetäuscht oder vendort. Kein CS2-Katalog. Team-Wahl: CS:GO-Aufbau (große Figuren vor den Kreisen, Titel oben, echte Namen und Teamzahlen mittig); T/CT wechseln pro Öffnen zufällig zwischen ihren vier Originalmodellen, Waffen bleiben `p_ak47` / `p_m4a1`. Class: vier Originalklassen in einem Studio-Pass, `joinclass 1..4`. `radio1`/`radio2`/`radio3` = drei getrennte Listen. **Radio im echten Spiel durch den Inhaber bestätigt (2026-09-05).** Gates: `./scripts/vgui-teamselect-gate.sh`, `./scripts/vgui-classselect-gate.sh`, `./scripts/vgui-buy-gate.sh`, `./scripts/vgui-radio-gate.sh`.
6. **Escape/Pause AUTOMATED PASS + Inhaber Optik abgenommen** — Blur der Szene + zentriertes „Pausiert“, `GameMenu.res` darunter. Titelseiten-PNG bleibt das Hauptmenü. PlayerList = Stub. Gate: `./scripts/vgui-pause-gate.sh`.
7. **Spectator Referenzumbau AKTIV / Grundfunktion AUTOMATED PASS** — `CSpectatorHudPanel`: Broadcast-Kopf, Scores/Timer/Map/Modus/Ziel und reale T-/CT-Spielerlisten aus dem erweiterten Client-State. Keine erfundenen Economy-/Waffendaten. Kein `KEY_DEST_MENU`, kein Team-`.res`. Gate: `./scripts/vgui-spec-gate.sh`. Radar nach 3M.
8. **Scoreboard Referenzumbau AKTIV / Grundfunktion AUTOMATED PASS** — `CScoreboardHudPanel`: breite mittige Tafel, T/CT vertikal gestapelt, Name/K/D/Ping. Client `CHudScoreboard` liefert den State bei `+showscores`. Die Referenzoptik ist noch nicht abgenommen. Gate: `./scripts/vgui-score-gate.sh`.
9. **Team/Class/Buy-Look (erste Scheibe) AUTOMATED PASS + Inhaber 2026-09-04 Funktion abgenommen** — Optik grob da, Feinschliff später. `InGameViewportLook`: Karten, Split-T/CT, Buy-Raster. Commands unverändert. Gates team/class/buy **PASS** @800×600.
10. **Kompletter In-Game-1:1-Feinschliff AKTIV** — nicht nur Spectator/Scoreboard. Latte: Layout und Informationsdichte der Inhaber-Referenzen, soweit CS-1.6-Daten existieren; fehlende machbare Stücke nachziehen. Familien bleiben getrennt. Command-, AMX-/Plugin- und Bot-Menü erst nach Backend-Vertrag. Eine erste Look-Scheibe oder ein grüner Funktions-Gate ist noch keine visuelle Fertigstellung.

Pro fertiger Dialoggruppe visueller Vergleich Steam-CS 1.6 bei 640×480, 800×600, 1024×768, einer 16:9. Responsive Team/Buy zusätzlich gegen Ultrawide prüfen; aktueller automatischer Satz: 640×480, 800×600, 1280×720, 1920×1080 und 2560×1080.

## Options-Port (aktiv)

| Schritt | Status |
|---------|--------|
| Stub-Pages Mouse/Audio/Video | **entfernt** — keine Dummy-Tabs |
| NextClient Controls (`CvarToggle`/`Negate`/`Slider`/`TextEntry`/`KeyToggle`) | **portiert** → `client/menu/gameui/Controls/` + Xash `MenuEngine` |
| `COptionsSubMouse` | Gate grün (funktional + Preferred 512×406 @640–1366) |
| `COptionsSubAudio` | Gate grün; `MP3 volume *` original; Miles hidden (kein Backend) |
| `COptionsSubVideo` | Video-Modi-Basis automatisiert geprüft; `docs/PHASE3M-VIDEO.md`. **Brightness/Gamma: FUNKTIONAL OPEN, späterer Featureblock; Inhaber-Test fehlgeschlagen.** |
| Effektives Scheme (Runtime-Winner) | `gamedata/valve/resource/TrackerScheme.res` (= Current Steam `valve/…`); `platform/…/TrackerScheme.res` nur Fallback; `ClientScheme` parallel (HUD) |
| Effektive `.res` | Mouse/Audio unter `data/ui-overrides/cstrike/resource/` |
| CVar-Mapping Mouse | `m_filter` → `look_filter` |
| CVar-Mapping Audio | `hisound` → `room_hires` (Semantik 0/1 → 1/2); `mp3volume` → `MP3Volume` |
| Apply/Cancel/Reset/OK/Persistenz | `./scripts/vgui-options-mouse-gate.sh`, `./scripts/vgui-options-audio-gate.sh` |
| Localization | UTF-16→wchar_t; gameui/vgui/cstrike/platform |
| Video → … | Modus-/Safety-Pfade automatisiert geprüft; Brightness/Gamma **OPEN / SPÄTER** |
| Keyboard | **AUTOMATED PASS / MANUAL RECHECK OPEN** — staged Bindings überleben Page-Wechsel; Apply schreibt Engine/Config; `docs/PHASE3M-KEYBOARD.md` |
| Video | Xash-Modusoptionen vorhanden. Brightness/Gamma nicht abgenommen und auf später verschoben (`docs/PHASE3M-VIDEO.md`) |
| CS-Retro-Advanced-Tab | leer bis Features existieren (kein FOV-UI vor FOV) |
| Create MP | nach stabiler Options-Grundlage |

## Global VGUI2 Visual Polish Gate

**Status: VISUAL ACCEPTED 2026-09-04** für Hauptmenü, Options, Create Game und LAN-Server-Browser. **Team und Class visuell bestätigt 2026-09-07.** Buy-Look 2026-09-07 abgelehnt. Spectator/Scoreboard erste Scheiben abgenommen, Feinschliff offen. Die neue Konsole braucht ihren manuellen visuellen Check. Neue Desktop-Dialoge müssen die abgenommene Baseline übernehmen; In-Game-Familien folgen ihren eigenen Referenzen.

Untersuchen zentral: Font-Familie (Steam/GameData), FreeType Hinting/AA/Kerning, Glyph Advances, Cell Height, Ascent/Descent/Baseline, DPI, Tab-/CheckButton-/Combo-/Slider-/Button-/ScrollBar-/QueryBox-Metriken.

**Classic Fidelity** = erkennbare Steam-CS-1.6-VGUI2-Basis + sauberes modernes Desktop-Rendering — **nicht** absichtlich schlechte 2003er Rasterisierung.

Themen (Scheme / Font-Backend / Controls-Core / Layout-Unterbau; **keine** Pixelhack-Sammlungen pro Dialog):

- Font-Familie / Metrik / Schärfe
- Text-/Control-Ausrichtung; einheitliche Insets
- Tabs, ComboBoxes, CheckButtons, Slider, ScrollBar
- **Tabs (Options):** `PropertySheet.TextColor`→`DimBaseText`; Selected→`BrightControlText`; Label zentriert; **72×24**; `SetTabHeight()`
- **Font-Metrik:** `surface_xash.cpp` — GDI-Zelle (REAL_DIM), `textOffsetY` für Internal-Leading/1px Zellenüberlauf; weight 0→400
- **UI-Schrift:** **Noto Sans / Noto Sans Mono** (OFL-1.1) mitgeliefert; keine System-/Steam-Font-Abhängigkeit
- **Antialiasing:** immer Graustufen-AA, `antialias 0` im Scheme wird bewusst ignoriert (Win32-Tahoma-Bitmapannahme gilt für Noto nicht)
- **ComboBox zentral:** Border/Inset, kompakter Arrow, 20px Dropdown-Items; Textbaseline/Font-Metrik/Selected-Hover bleiben Feinschliff — nicht als Video-/Audio-Einzelpatch
- **CheckButton/Slider:** sichtbar (Marlett Fill/Bevel/Haken; `WindowBG`; Scheme-Alpha 255); `Slider.NobColor`≠Tick-Grün; Mouse/Video-Gate **PASS**
- **ScrollBar:** `ScrollBar.Wide` Default 17 (Code-`SCROLLBAR_DEFAULT_WIDTH`); Keyboard-Gate **PASS**
- **Build-Hygiene:** Warnings/Errors zuerst; kein CMake `--clean-first` am shared Tree ohne `./scripts/build-client.sh` danach
- Keyboard-Liste: Scrollbar/Pfeile, untere Rows, Spalten/Insets
- Keyboard Capture-Slot: Scheme-`Capture`/`Edit`-Foreground (Primary≠Alternate; ESC stellt Slot wieder her) — kein hartcodiertes RGB, kein Blinken
- Video-Alignment: Resolution / Renderer / Aspect Ratio / Display Mode (Labels + Combos + Spalten) gegen Golden/Classic `.res` — Backend bleibt geschlossen
- Video-Footer-Hinweis vs. Buttons (Visual only; Backend PASS)
- Modal-/QueryBox Spacing

Gezielte `.res`-Korrektur nur wenn Abweichung zur Original-Resource bewiesen ist. Golden **5971** = Classic-Referenz; Current Steam/HL25 = Research only.

## Adaptive Layout / Resize (AUTOMATED PASS / MANUAL OPEN)

Natives Resize ist freigegeben. Alle vier Kanten, vier Ecken, Mindestgröße, Live-Save ohne Apply, Workspace-Clamp und echter Prozess-Restart sind automatisiert grün. Manual Acceptance bleibt offen. Vertrag: `docs/PHASE3M-LAYOUT.md`. Gate: `./scripts/vgui-options-layout-gate.sh`.

Classic Preferred **512×406** = Referenz. Abgeleitetes Minimum **ebenfalls 512×406** (Mouse/Audio/Video-`.res` füllen Classic). Keyboard-Grow = Dialog−Preferred (700×520 → Liste 668×372). Danach Visual Polish, dann volle Options-Regression.

| Oberfläche | Bei Classic Size | Extra Fläche |
|------------|------------------|--------------|
| Keyboard | Classic | ListViewport + Spalten + Scrollbar; Footer unten |
| Mouse / Video | Classic-Geometrie **behalten** | Extra = Leerraum |
| Audio | kompakte sichtbare Geometrie ohne hidden HEV/Suit-Loch | Extra = Leerraum |
| ComboBox-Alignment | — | Visual Polish, nicht dieser Block |

## Window move / persistence / resize foundation

**Status: AUTOMATED PASS / MANUAL OPEN.** Save/Restore, Resolution/Workspace-Clamp und Prozess-Restart sind im Adaptive-Layout-/Resize-Gate belegt.

| Fähigkeit | Stand |
|-----------|--------|
| Move | Titelleiste; Manual ok im Basic-Test |
| Persist | `$XASH3D_BASEDIR/cfg/csretro_ui_geometry.txt` — Roundtrip belegt |
| Clamp | Gate: 700×520 → 640×480 Workspace; Offscreen-Geometrie vollständig eingefangen |
| Resize | `SetSizeable(true)` — vier Kanten + vier Ecken, Min-/Workspace-Clamp grün |

512×406 = Classic Preferred / Reference. Abgeleitetes Minimum **ist** 512×406, weil die vier Pages Classic bereits füllen — nicht blind gesetzt.

Mindestens Options; Architektur später Console, Server Browser, weitere Frames.

## Language selector (Future)

Noch **keine** Subpage. Später: Sprachen aus verfügbaren Loc-Ressourcen; CS-Retro-Loc ergänzt Originale; persistent; keine hartcodierten EN-Strings; UI-Refresh nach Wechsel; lange Strings Layout-sicher. Grundlage: gemeinsamer `%language%`-Pfad (`valve`/`gameui`/…).

## Console (derselbe UI-/Input-Vertrag)

**Status: AUTOMATED PASS / MANUAL VISUAL CHECK OPEN.** `toggleconsole` ist ein echtes frei belegbares Binding und funktioniert aus Spiel und Menü; kein fest verdrahteter Backtick-/Tilde-Sonderweg. Die VGUI2-Konsole besitzt Engine-Scrollback, farbige Ausgabe, TextEntry, Befehlsausführung, Up/Down-History, Escape/Frei-Bind zum Schließen, Move/Resize und Geometry-Persistenz. Gate: `./scripts/vgui-console-gate.sh` (`F6` → öffnen → `echo` ausführen → schließen → Geometrie prüfen). Copy/Paste läuft über das vorhandene TextEntry-Verhalten; Auswahl/Scroll und die visuelle Wirkung werden manuell geprüft.

## Zielbild

Classic CS 1.6 VGUI2 → korrekte Basis → NextClient-Funktionen → CS-Retro-Extensions → responsive Desktop / HiDPI.

Später sichtbar nur mit Backend: moderne Video/Renderer/Borderless · Audio · Crosshair-Fine · HUD/Radar-Minimap · Network · NextClient · ServerProfile/GameRules · Bots · Module · Metamod/AMXX.

### Nach 3M — HUD/Options (kein Code in 3M)

FOV/3D/In-Game bleiben gesperrt bis nach 3M. Danach, nur mit Backend:

| Feature | Ziel | Quelle | Nicht |
|---------|------|--------|-------|
| **Crosshair** | Sehr individuell über die Optionen (feiner als Presets) | NextClient `HudCrosshair` + `OptionsSubMultiplayer` (`cl_crosshair_type/color/size/translucent`, `cl_dynamiccrosshair`; Typen Cross/T/Kreis/Punkt) | tote Options vor Backend |
| **Radar** | Minimap: **Karte im Radar** (CS:GO/CS2-artig), nicht nur Punkte auf leerem Kreis | NextClient hat **kein** Map-Radar (`HudRadar.cpp:13–14` → Steam-`CHudHealth__DrawRadar`). Body = klassisches Sprite-Radar. Abguck: MetaHook / GameBanana Dynamic Radar; lokal `TEMP_EXTRA/hl2_src` (CS:Source `hud_radar.cpp`) und `TEMP_EXTRA/cstrike15_src` (CS:GO Scaleform `sfhudradar`) — `docs/UPSTREAM.md` | MetaHook/Source/CSGO vendorn oder als Hook-Runtime |

## NextClient-GameUI — Inventar

Pfad: `client/nextclient/gameui/`. **Nicht** unser Produkt-Build (`-m32`, …).

| Komponente | Dateien | Wiederverwendbar | Runtime-Bind (entfernen) |
|------------|---------|------------------|--------------------------|
| `CGameUI` / `IGameUI` | `GameUi/GameUi.cpp` | Init, Activate | HWND / WndProc |
| `CBasePanel` / GameMenu | `BasePanel.cpp` | `GameMenu.res`, Commands | Steam-ISurface |
| Options | `OptionsDialog/*` | Tabs + CVar | Steam-vgui2 |
| Create MP | `CreateMultiPlayerGameDialog/*` | Server/Bot-Seiten | dasselbe |
| Controls | `GameUi/Controls/*` | Cvar-/Key-Controls | `engine->*` → Xash |
| Server Browser | `ServerBrowser/*` | Listen/LAN-Spaltenmodell | Steam-MM, Internet/Favorites/History/Friends, `ISteamMatchmakingServers` |
| CEF | `Browser/*` | **nicht** in 3M | CEF |

## Xash-Adapter

| Xash | Verwendung |
|------|------------|
| `GetMenuAPI` / `UI_FUNCTIONS` | Pflicht |
| `ui_enginefuncs_t` | PIC, Fill, CVars, Cmds, Keys |
| `UI_GetMenuFactory` → `CreateInterface` | `GameMenuExports001` |
| `-menulib` | Testoverride |

## Originalressourcen (Game-Data)

COPY: `cstrike/resource/`, **`platform/resource/`** (Schemes, `vgui_*.txt`) — **ohne** Steam-`linux_fonts`. Overrides: `data/ui-overrides/` (inkl. `platform/resource/csretro_fonts/` mit Noto Sans).

## Serverprofil

Gemeinsames Profil für Listen + Dedicated. Modules = `none` bis Module existieren.

## Stand (dieser Host)

| Stück | Status |
|-------|--------|
| V1-Runtime-PoC | **bestanden** |
| Menü-Lib | `menu_amd64.so` V1-Core + Controls + Xash-Backends |
| Options Mouse | **PASS / Regression** Preferred 512×406 @640/800/1024/1366 |
| Options Audio | **PASS / Regression**; Miles absichtlich hidden |
| VGUI2 Symbol-Controls | **Gate grün** — `vgui_symbols.cpp` |
| VGUI2 Metrics Preferred Size | **512×406** (`OptionsClassicMetrics.h`) — Classic Preferred, nicht Max |
| Video | **PASS / Regression** — Xash-Backends; Confirm für Mode; FOV ausgeklammert. Brightness/Gamma-Spielweltwirkung automatisiert belegt; manueller Recheck offen |
| Keyboard | **AUTOMATED PASS / MANUAL RECHECK OPEN** — Persistenz- und Config-Isolation-Fix automatisiert grün; `docs/PHASE3M-KEYBOARD.md` |
| Adaptive Layout / Resize | **AUTOMATED PASS / MANUAL OPEN** — acht Grips, Min 512×406, Live-Save ohne Apply, Persist/Clamp/Restart grün; `docs/PHASE3M-LAYOUT.md` |
| Global VGUI2 Visual Polish | **VISUAL ACCEPTED 2026-09-04** für Hauptmenü, Options, Create Game und LAN-Browser; neue Konsole manuell offen |
| Konsole | **AUTOMATED PASS / MANUAL VISUAL CHECK OPEN** — bindbar, Engine-Scrollback/-Befehl, History, Move/Resize/Persistenz; `vgui-console-gate.sh` |
| Windows ShellOpen | **offenes Plattform-Gate** (`system_shell_win.cpp` No-Op) |
| Hauptmenü | **echte VGUI2-Controls** (Menu + MenuItems, Noto Sans, unten links); Hintergrund **CS-Retro-PNG**, nicht Steam-Kacheln |
| Create Game | **echte VGUI2-Controls / VISUAL ACCEPTED 2026-09-04**: drei Tabs Server (Map + Identity + Bots) / Game (Rules) / Fairness; Zeilen aus `settings.scr` über `CreateGameSettingsList`. Bots nur mit `.nav`-Mesh wählbar. Combo ohne Dauer-Fill, Listen-Achse. Chrome: Frame rund, Settings-Liste eckig |
| Browser | **LAN-VGUI2** (`CServerBrowserDialog`). Funktion akzeptiert (leere LAN-Liste erwartet). Dieselbe Chrome wie Create/Options (Frame rund, Liste eckig). Empty-Text nicht doppelt. Internet-Tab offen / eigenes Vorhaben |
| In-Game Team/Buy | Interim-`.res`-Pfad bis VGUI2-Ersatz |
| Tests | `vgui-v1-poc-runtime.sh`, Mouse/Audio/Video/Keyboard-Gates, Main-Menu/Create-Game/Server-Browser/Console-Gates, `play.sh`, `build-menu.sh --sanitize` |

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

**Classic Preferred Size:** 512×406 implementiert (`CsretroOptionsClassic`). Mouse+Audio-Gate grün. Details: `docs/PHASE3M-METRICS-DIAGNOSIS.md`.

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

FOV, Crosshair, HUD-/Radar-/Camera-/Inspect-Schalter ohne Backend. 3D. CEF-Hauptmenü. Ref-A-mainui. Steam-vgui2. `TEMP_EXTRA/` (Source SDK 2013 / CS:GO cstrike15) nicht mergen und nicht vendorn. Phase-3-Abschluss-Tag erst bei echter VGUI2-Optik + Funktion. Crosshair-Fine und Radar-Minimap sind **Zielbild nach 3M** (Tabelle oben), kein 3M-Bau.
