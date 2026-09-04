# Changelog

Jede Version hier = ein GitHub-Release auf `benjarogit/csretro` (privat).
Der verbindliche Projektstand steht in `docs/HANDOFF.md`.

## Unreleased — Phase 3M (VGUI2 / Desktop-UI)

Kein Phase-3-Tag. Kein FOV. 3C-Baseline (`v0.1.5`) bleibt gültig.

- **Spectator/Scoreboard-Look (erste Scheibe):** Eigene `HudFrameLook`-Familie, kein Team-Viewport. Spectator = Rahmen links/rechts plus T/CT-Akzent im oberen Balken. Scoreboard = durchscheinende Tafel, Gold/Blau-Kopfstreifen, festes Name/K/D/Ping-Raster. Gates spec/score unverändert (Funktion). **Inhaber 2026-09-04: beide ersten Look-Scheiben abgenommen; Feinschliff später.**
- **Team/Class/Buy-Look (erste Scheibe):** Gemeinsame `InGameViewportLook`-Karten statt ClientScheme-Orange. Team Split-T/CT, Buy-Hauptseite als Raster, Welt durchscheinender. Steam-`.res` und `jointeam`/`joinclass`/Kauf-Aliase unverändert. Gates team/class/buy **PASS** @800×600. Eine als UTF-8 gespeicherte `csretro_gameui_english.txt` im BASEDIR hatte die UTF-16-GameData-Datei überschattet (`CSRETRO_LOC_MISSING` nur Browser-Tokens) — Override wieder UTF-16LE; Play/Isolate/Bootstrap wandeln UTF-8-Kopien um.
- **Scoreboard als VGUI2 (eigene Familie):** TAB (`+showscores`) öffnet eine mittige Tafel (T/CT-Spalten, Name/K/D/Ping). Welt bleibt; Tasten beim Spiel. Orange HUD-Tafel wird übersprungen, sobald die Menü-Lib da ist. Steam-`ScoreBoard.res` = Felder, nicht das Overlay. Seitenkarten/Economy und Broadcast-Look später. Gate: `./scripts/vgui-score-gate.sh`. **Inhaber 2026-09-04: Grundfunktion und erste Look-Scheibe abgenommen; Spalten, Schrift und leere Teamseite später verfeinern.**
- **Spectator als VGUI2 (eigene Familie):** Dunkle Top/Bottom-Rahmen, T/CT-Stand, Timer, Map, Kameramodus, Ziel+HP. Welt bleibt frei; Maus/Tasten beim Spiel (`KEY_DEST_GAME`, nicht modal). Orange HUD-Balken werden übersprungen, sobald die Menü-Lib da ist. Steam-`Spectator.res` = Felder, nicht das Overlay. Anreiz (Idee, kein 1:1): Broadcast-Rahmen, Daten am Rand — ohne Veto, Avatare, Waffen-Icons, Radar, Rundenhistorie, Mid-Scoreboard. Gate: `./scripts/vgui-spec-gate.sh`. **Inhaber 2026-09-04: Grundfunktion und erste Look-Scheibe abgenommen; Feinschliff später.**
- **Pause als VGUI2 (GameUI, nicht Overlay):** Escape im Spiel öffnet dieselbe `GameMenu.res`-Liste (Resume/Disconnect/Options/Quit). **Inhaber 2026-09-04: Grundfunktion und Optik abgenommen** (Welt unscharf, mittig „Pausiert“, Liste zentriert). Titelseiten-PNG bleibt das Hauptmenü. Kein `setpause` auf Listen/MP. PlayerList bleibt Stub (`menu_playerlist`). Gate: `./scripts/vgui-pause-gate.sh`.
- **Radio als VGUI2 (eigene Familie):** Kompakte HUD-Karte, Texte aus `titles.txt`. Kein Team-Viewport, kein `KEY_DEST_MENU` — Bewegung und Schuss bleiben, nur 0–9/ESC greifen (`IsModalInGame`). `radio1`/`radio2`/`radio3` → Aliase. Gate: `./scripts/vgui-radio-gate.sh`. **Inhaber 2026-09-04: Grundfunktion abgenommen.** Team/Class/Buy-Modernisierung ist ein anderer Schritt (Anreize 2026-09-04).
- **Buy-Tastatur:** Nach Waffe/Equipment/Autobuy/Rebuy schließt das Overlay (Munition auf der Hauptseite bleibt). A/R für Autobuy/Rebuy, nicht mehr nur 0–9.
- **Buy Autobuy/Rebuy:** Steam-Buttons schickten `cl_autobuy`/`cl_rebuy` ohne Liste. Jetzt Client-`autobuy`/`rebuy` (lädt `autobuy.txt`/`rebuy.txt` → `cl_set*`). Produktlisten in `data/ui-overrides/cstrike/`. Hover/Optik später.
- **play.sh Session-Log:** jeder Lauf schreibt `build/run/play.log` (plus `logs/play-TIMESTAMP.log`) und `-log` → `engine.log`.
- **Schuss-Hänger (Regression):** Dieselbe Klasse wie 2026-08-31 (Dedicated `sv_maxupdaterate 30` → Snapshot alle ~33 ms). Auf Xash-Listen gilt `cl_cmdrate` bei `maxclients>1`; Play-`config.cfg` hatte die Xash-Rohwerte `cl_updaterate 20` / `cl_cmdrate 30` / `sv_maxupdaterate 60`. Produkt-Defaults und Sanitize: 102 / 100 / 102, `rate` 100000. `Menu_Con`→Notify-HUD bleibt in `play.sh` aus. **Inhaber 2026-09-04: Hänger weg.** Buy „teilweise“ unverändert, nicht angefasst.
- **VGUI-Corner-Logflut:** Jedes Panel lud `gfx/vgui2/800corner*` (Steam-Nibble, nicht im Produkt). Defaults leer; `DrawSetTextureFile` wiederholt fehlgeschlagene PIC_Load nicht. DrawBox Type 2 bleibt scharfes Rechteck.
- **V1-Runtime-PoC bestanden:** `vgui_controls` + Xash-Surface/Input + `.res`/Scheme + FreeType-Glyphen; Maus/Tastatur/TextEntry/Tab/Escape/Resize; ASan+UBSan; keine Steam-/vgui2-/Touch-Runtime. Nachweis: `./scripts/vgui-v1-poc-runtime.sh`, manuell `./scripts/play.sh`.
- V1 ist die verbindliche UI-Basis (`docs/PHASE3M.md`). Rekonstruktion der Steam-CS-1.6-VGUI2-Oberfläche beginnt.
- **Options Mouse/Audio:** Gates **PASS** (funktional + Preferred-Size-Layout @640–1366); Miles absichtlich hidden.
- **Options Video:** **Mouse PASS · Audio PASS · Video PASS** (Automated + Mode-Safety + Wanduhr-10s delta_ms≈10072 + Visual Confirm/Reinit). Graceful Shutdown **PASS**. Confirm schließt per `Close()` (Modal-Teardown). Matrix: `docs/PHASE3M-VIDEO.md`. Gamescope-WSI Zenity getrennt. **Kein Phase-3-Tag. FOV/3D gesperrt.**
- **Options Keyboard:** **AUTOMATED PASS / MANUAL RECHECK OPEN** nach User-Retest-Fix. Staged Bindings überleben Page-Wechsel; Apply schreibt Engine/Config; normaler `play.sh` seedet CS-Defaults nur bei leerer/HL-Fallback/Gate-Config; isoliertes Gate prüft `F11=+forward`. Capture, Wheel, Isolation, BIND_AUDIT bleiben grün. `docs/PHASE3M-KEYBOARD.md`.
- **BIND_AUDIT:** Buchstaben über `KeyNameToKeynum`-Scan (Raw-Pfad `w`=119, `c`=99); `hidden_third_plus` = Catalog, nicht unmatched.
- **Adaptive Layout / Resize:** **AUTOMATED PASS / MANUAL ACCEPTANCE OPEN** — Classic 512×406 bleibt; Grow über Dialog−Preferred; abgeleitetes Minimum **512×406**; natives Resize an allen acht Grips; List-Viewport/Footer-Clipping automatisiert geprüft. `docs/PHASE3M-LAYOUT.md`.
- **Window Geometry:** Save/Restore, Live-Save ohne Apply, Full-Workspace-Clamp und echter Prozess-Restart **AUTOMATED PASS**.
- **Gate-Startvertrag:** Scripts verwenden `-menulib` mit absolutem Menüpfad, damit relative `CSRETRO_MENU_SO=build/...` nicht in Engine-Fallbacks läuft.
- **Gate-Isolation:** ältere Mouse/Audio/Video/V1PoC/Shutdown-Gates schreiben standardmäßig nach `build/run-gate/*` statt in den normalen `build/run`-Play-Baum.
- **Options Audio:** sichtbarer Sound-Quality-Block rückt unter MP3 Volume; hidden HEV/Suit-Abstand bleibt nicht mehr als Loch stehen.
- **Create Game als VGUI2 (Schritt 3, Server-Seite):** `CCreateGameDialog` + `CCreateGameServerPage` ersetzen den Interim-`DrawNewGame`; Map-Liste kommt über `FindFirst("maps/*.bsp")`, Werte laufen ausschließlich über `ServerProfile` → `Profile_Start`. Bot-Block nur wählbar, wenn die Map ein `.nav` hat (aktuell nur `de_dust` von 25 Maps) — sonst gesperrt mit Hinweis statt toter Option. Steam-Networking-Checkbox bewusst nicht übernommen. Gate: `./scripts/vgui-creategame-gate.sh` **PASS** @800×600/1024×768.
- **Create Game Gameplay-Seite (Schritt 3, Stufe B):** `CCreateGameGameplayPage` baut ihre Zeilen aus `cstrike/settings.scr` (18 Einträge), gelesen von `ServerSettingsScript` — gezielter Port der Parse-Semantik von NextClients `ScriptObject` ohne dessen Config-Schreibpfad (`docs/UPSTREAM.md`). `settings.scr` ist jetzt im Gamedata-Manifest. Grenzen aus dem Script greifen beim Übernehmen (Gate prüft `mp_roundtime` 99 → 15). Servername, Slots und Passwort sind von der Server-Seite auf die Gameplay-Seite gewandert, wo sie laut `settings.scr` hingehören — vorher standen sie an beiden Orten. `ServerProfile` führt die Gameplay-Regeln als Name/Wert-Liste statt als Feld je CVar, `Profile_WriteListen` schreibt sie durch; neue Zeilen in `settings.scr` wirken damit ohne Codeänderung.
- **Create Game Labels + drei Tabs (Schritt 3, Stufe C):** Ursache der fehlenden Beschriftungen war `SetFirstColumnWidth(0)` ohne Prompt in der Zeile. `CreateGameSettingsList` setzt Label+Control in dieselbe Zeile (Classic-Maße, Zahlenfelder 72px). Gruppierung Identity / Rules / Fairness; Hostname/Slots/Passwort wieder auf der Server-Seite. `settings.scr` um ReGameDLL-gedeckte CVars erweitert (`mp_c4timer`, `mp_maxmoney`, `mp_autoteambalance`, `mp_limitteams`, `mp_playerid`, `allow_spectators`); Logging/Infinite-Ammo/Scoreboard bewusst weggelassen. Totes Feld `ServerProfile::teambalance` entfernt. Gate prüft `CSRETRO_CREATE_LABELS`.
- **Create Game Visual-Abnahme (Fehlerliste):** Combo-Selection füllt die Zelle (kein versetztes Gold-Insel-Rect). Radio/Checkbox/Scroll-Pfeile/Fenster-X sitzen in der Glyphe (`vgui_symbols` + Control-Offsets 0).
- **Hauptmenü-Hintergrund:** einziges Motiv `resource/background/csretro.png` (CS Retro, 1672×941 PNG). Steam-Kacheln (`800_*_loading.tga`, `BackgroundLayout.txt` in cstrike/valve) werden nicht importiert und aus Altbeständen entfernt. Nachweis: Log `CSRETRO_BG`.
- **Dialog-Chrome korrigiert (CS:Source-Hierarchie):** Die zweite Runde hat innen mitgerundet — Frame 14px **und** Settings-Liste/ListPanel 8px. Das war falsch (Doppel-Kante). Standard ist jetzt **eine** äußere Hülle (`Frame` ~8px + Outline, Glass-Alpha 188) und innen eckig: flaches dunkles Rechteck, `ButtonDepressedBorder`, Padding bleibt. `DrawBox` Type 2 ohne Ecktexturen fällt auf ein scharfes Rechteck zurück. Create Game, Find Servers und Options teilen dieselbe Sprache. Create-Game-Doppel-Rundung ist keine Referenz mehr.
- **Create Game / Options / Find Servers — Combo/Liste:** geschlossenes Combo ohne Dauer-Fill, LAN-Empty-Text nur in der Liste, Settings-Padding und gemeinsame Control-Kante. Options teilt dieselbe Chrome (kein Seiten-Rewrite).
- **VGUI-Deadlock behoben:** `CheckButton::SetSelected` postet `CheckButtonChecked` auch ohne Zustandswechsel — ein Handler, der daraufhin wieder `SetSelected` ruft, hängt die Nachrichtenschleife auf. Zustandskorrekturen laufen jetzt über `SilentSetSelected`.
- **Server Browser als LAN-VGUI2:** `CServerBrowserDialog` ersetzt den Interim-`DrawBrowser`. Discovery über Engine-`localservers` / `UI_AddServerToList` (`pfnAdrToString`). Fremde `gamedir` und GoldSrc-`gs` werden abgelehnt. **Kein** Internet-Tab, kein Steam-Master — eigene Internet-Serverliste ist ein separates Vorhaben (`docs/SERVER.md`). Gate: `./scripts/vgui-serverbrowser-gate.sh` @800×600/1024×768 (quit-Vertrag, kein Connect).
- **Team-Wahl als VGUI2 (Schritt 5, erste Scheibe):** `CTeamSelectPanel` ersetzt den Interim-Rechteck-Renderer für `MENU_TEAM`. Steam-`resource/UI/Teammenu.res`, `ClientScheme`, Overlay statt Frame-Hülle, `HTML/MapInfo` → `RichText` aus `maps/<hostmap>.txt`, sichtbare Buttons nach `validSlots`, Commands nur über das Backend (`jointeam`/`spectate`/`vguicancel`). `ShowMenu` bleibt. Buy weiter Interim. Factory-Lookup: `Csretro_GetGameMenuExports` plus Retry, damit `_vgui_menus` nicht still auf 0 fällt. Gate: `./scripts/vgui-teamselect-gate.sh` **PASS** @800×600 (quit-Vertrag).
- **Class-Wahl als VGUI2 (Schritt 5, zweite Scheibe):** Ursache der rohen Tokens (`Cstrike_Terror`, …) war der Interim-Pfad: `Menu_LoadRes` + `Menu_L` (ASCII-Parser auf UTF-16-Loc) + Konsolenfont statt `Label::SetText`-`#`. Zusätzlich setzt Steam-`Classmenu_*.res` bei mehreren Buttons `labelText` zweimal (erst leer, dann `#Token`) — `KeyValues::GetString` nimmt den ersten. `CClassSelectPanel` lädt die `.res` als echte VGUI2 (gleiches Overlay/ClientScheme wie Team) und setzt die `#`-Tokens nach. `MouseOverPanelButton` zeigt Hover-Portraits aus `gfx/vgui/`. Commands `joinclass N` / `vguicancel` (ReGameDLL `HandleMenu_ChooseAppearance`). CS-1.6: Militia/Spetsnaz aus, Auto-Select = Slot 5. `Menu_Con` schreibt Notify-HUD nicht mehr in `play.sh` (nur stderr; Gates weiter engine.log). Gate: `./scripts/vgui-classselect-gate.sh` (quit-Vertrag, TER+CT). Team **Inhaber visuell bestätigt** (Namen/Auswahl). Class **AUTOMATED + Inhaber-Check Namen ok**.
- **Class_Info-Fix:** `#Cstrike_Class_Info` auf `classInfoLabel` steht in Steam-`Classmenu_*.res` (`visible 0`), fehlt aber in `cstrike_english`. `ShowClassPreview` blendete das Label trotzdem ein. `Label::SetText("#…")`; bei Fehlschlag leerer String, Label unsichtbar. Leerer Text statt Roh-Token.
- **Buy als VGUI2 (Schritt 5, dritte Scheibe):** `CBuySelectPanel` ersetzt den Interim-Rechteck-Renderer für `MENU_BUY` und die Waffen-/Equipment-Typen. Steam-`MainBuyMenu.res` plus team-spezifische `BuyPistols_*.res` / `BuyShotguns_*.res` / `BuyRifles_*.res` / `BuySubMachineguns_*.res` / `BuyMachineguns_*.res` / `BuyEquipment_*.res`. Kategorie-Klick lädt die Unterseite clientseitig; Kauf geht nur über das Backend (`glock`/`vest`/`primammo`/… → ReGameDLL `HandleBuyAliasCommands`; Steam-`autobuy`/`rebuy` → `cl_autobuy`/`cl_rebuy`). `vguicancel` auf der Unterseite zurück zur Hauptseite, ESC schließt. `ShowMenu` bleibt. **Navigation:** Steam-Hauptseite schreibt `"Command"` (groß); unser KeyValues-Symbol ist case-sensitiv, `Button::ApplySettings` sucht `"command"` — ohne Nachzug blieb der Klick tot. `Menu_LoadRes` setzt beide Schreibweisen. **Layout:** `MainBuyMenu.res` hat kein `BuyMenu`/Frame mit `wide`/`tall` (Team/Class schon). Default-Panel 64×24 clippt die Buttons — Overlay-Dim ohne UI. Panel füllt das Overlay (Viewport-Koordinaten, wie Ref B). Gate prüft `CSRETRO_BUY_LAYOUT fit=1`. Gate: `./scripts/vgui-buy-gate.sh` **PASS** @800×600 (quit-Vertrag, Loc ohne Roh-Tokens, ESC, Pistolen-Unterseite, `glock`, Layout).
- **Quit-Vertrag im Create-Gate:** Das Gate schoss die Engine per Signal ab und blendete die Job-Control-Meldung aus. Jetzt setzt das Menü `quit` ab (`CSRETRO_GATE_GRACEFUL_QUIT`), und der gemeinsame Helfer `csretro_gate_wait_quit` prüft Exit 0 und Crash-Signaturen — keine Doppelimplementierung mehr. Nativ bevorzugt, weil unter gamescope der beobachtete Exit-Code dem Wrapper gehört (gamescope stürzt im eigenen Teardown ab), nicht der Engine. Übriggebliebene Prozesse werden gemeldet, nicht still weggeräumt; SIGTERM allein reicht bei hängender Engine nicht.
- **Gamedata-Aufräumen deklarativ:** hartcodiertes `prune_steam_fonts` ersetzt durch `prune`-Regeln im Manifest (`ignore` = nicht importieren, `prune` = aus Altbeständen entfernen); Ergebnis landet in `origin.json` unter `pruned`. Umfang bewusst klein gehalten: `gamedata/valve` bleibt vollständig, weil `halflife.wad` von 22 der 25 Maps und `xeno.wad` von zweien referenziert wird. `docs/GAMEDATA.md`.
- **Localization-Kodierung:** Valve-`*_english.txt` sind **UTF-16LE**. Eine als UTF-8 eingefügte Zeile macht die Datei unlesbar, ohne dass die UI abstürzt (`CSRETRO_LOC_csretro_gameui FAIL`). Das Create-Gate prüft Ladeerfolg und fehlende Tokens jetzt explizit.
- **ComboBox Visual Polish:** Dropdown-Items innerhalb Menu-Border; Arrow zentriert ohne Extra-Border; Itemhöhe **20px** greift (Default + `Menu/ItemHeight` + Bootstrap-Patch); Content-Inset ≥1 gegen Rahmen-Bleed; TextInset-Y 1.
- **CheckButton/Slider Visual Polish:** CheckImage über Marlett; **sichtbarer** Fill (`CheckButton.BgColor`→`WindowBG`, Fallback opaque); Marlett `g`=Fill / `e`/`f`=Bevel / `b`=dicker Haken; Scheme `GetColor` Default-Alpha **255** bei RGB-only (verhindert unsichtbare Controls); `Slider.NobColor`→`ControlBG`. Gates: Mouse/Video/Keyboard **PASS** nach Fix.
- **Keyboard Capture UX:** „Press a key…“ während Capture in `BrightControlText` (Primary) bzw. `BrightBaseText` (Alternate); Inline-Panel + Listenzelle.
- **PropertySheet Tab Polish:** Inaktive Tabs über `PropertySheet.TextColor`→`DimBaseText` (statt fehlendem `FgColorDim`); aktive Tabs gold (`BrightControlText`); Tab-Label zentriert, Inset 4px; kompakter **72×24** in Options (statt 84×28); `SetTabHeight()` API.
- **Font-Metrik (FreeType→GDI):** `surface_xash.cpp` — Ascent/Descent/Internal-Leading; `textOffsetY` (+1px bei Zellenüberlauf); Scheme-`weight` 0→400; Marlett-Symbole mit gleichem Y-Offset.
- **Hauptmenü als echte VGUI2-Controls:** `client/menu/vgui/main_menu.cpp` baut `GameMenu.res` als `vgui2::Menu` mit `MenuItem`s nach (Muster: NextClient `CBasePanel`/`CGameMenu`/`CGameMenuItem`). Farben und Maße aus `TrackerScheme` `InGameDesktop` (`MenuColor`, `ArmedMenuColor`, `MenuItemHeight`, `GameMenuInset`), Schrift `MenuLarge`, Anker unten links. Localization läuft über den `#`-Pfad von `Label::SetText` statt über den Interim-`Menu_L`, damit fehlende Tokens nicht still als Rohtext erscheinen. `OnlyInGame`-Einträge werden bei Levelwechsel neu bewertet. Das Menü-Popup liegt bewusst hinten (`MovePopupToBack`), sonst fängt es den linken Resize-Griff des Options-Dialogs ab. `vgui2::Menu` schließt sich als Popup selbst — beim Aktivieren eines Eintrags, bei ESC und bei Fokusverlust; das Hauptmenü war dadurch nach dem ersten Klick auf „Options“ dauerhaft weg. `SetVisible(false)` wird deshalb nur noch über `SetVisibleExplicit` aus `MainMenu_Show/Hide` durchgelassen, und das Menü ist nicht beim `CMenuManager` registriert (`EnableUseMenuManager(false)`), der bei jedem Klick außerhalb alle Menüs abräumt. Interim-Hauptmenü und Interim-Options-Textscreen entfernt. Neues Gate `./scripts/vgui-mainmenu-gate.sh` (Items, Localization, Bottom-Left-Anker, Screenshot sowie Überleben von Options-Öffnen/Klick/ESC) **PASS** bei 640×480/800×600/1024×768/1366×768; alle Options-Gates und Graceful-Shutdown weiter **PASS**. Der Survive-Teil treibt bewusst `UI_MouseMove`/`UI_KeyEvent` statt der VGUI-Interna — eine Simulation über `IInputInternal` verfehlte den Pfad und bestand auch mit ausgebautem Fix.
- **Text-Antialiasing:** VGUI-Glyphen werden immer mit Graustufen-AA gerastert. `TrackerScheme` setzt `antialias 0`, weil Win32-Tahoma gehintete Embedded-Bitmaps mitbringt — Noto hat keine, 1-Bit-Rasterung wirkte ausgefranst. Scheme-Wunsch bleibt als `aa_req` im Metrics-Log. Gates Mouse/Audio/Video/Keyboard/Layout **PASS**.
- **UI-Schrift Noto Sans (komplett):** Noto Sans / Noto Sans Mono (**SIL OFL 1.1**) werden mitgeliefert (`data/ui-overrides/platform/resource/csretro_fonts/`) — keine System- oder Steam-Font-Abhängigkeit, gleiche Optik auf Linux/Windows/macOS. Resolver mappt alle Scheme-Familien deterministisch (kein `<Family>.ttf`-Raten); Liberation/DejaVu/FiraSans-Reste entfernt; Scheme-`lastResort` → `Noto Sans`. Steam-`platform/resource/linux_fonts` wird nicht mehr importiert und aus bestehenden Bäumen entfernt. Vertikalmetrik identisch zu vorher (cell 16 / asc 13 / desc 4), horizontal minimal schmaler. Gates Mouse/Audio/Video/Keyboard/Layout **PASS**.
- **Menü-Build Warning-Hygiene:** Suppress-Flags entfernt; Root-Cause-Fixes. Clean Rebuild `csretro_menu`: **0 Warnungen / 0 Fehler**. Kein `--clean-first` am gemeinsamen CMake-Baum ohne anschließendes `build-client.sh`.
- **Global VGUI2 Visual Polish** OPEN (Feinschliff Controls/Video-Footer). Classic 5971 + moderne Desktop-Darstellung.
- **VGUI2 Symbol-Control-Gate grün:** `vgui_symbols.cpp` — Marlett geometrisch; kein Windows-Marlett.ttf; Scheme-lastResort überschreibt Symbolfonts nicht.
- **Metrics-/Classic-Gate:** Preferred Size **512×406**. Mouse+Audio+Video Gates. FOV gesperrt.
- **Windows ShellOpen:** offenes Plattform-Gate (No-Op).
- Eine Menü-Lib `client/menu/` (`GetMenuAPI` + `GameMenuExports001`).
- Font-Resolver: GameData `platform/resource/csretro_fonts` (+ System-Verzeichnisse nur als Notnagel).
- Team-Wahl: echte VGUI2 — Inhaber visuell bestätigt. Class-Wahl: echte VGUI2 — AUTOMATED + Namen ok, Class_Info-Fix. Buy: echte VGUI2 — AUTOMATED PASS (Hauptseite + Pistolen/`glock`). Radio: echte VGUI2-Overlay (kein Steam-`.res`). Hauptmenü, Options, Create Game und LAN-Server-Browser sind VGUI2.
- `ShowMenu` bleibt Legacy. 3D/FOV erst nach Freigabe.
## 0.1.5 — 2026-09-01

Kein Phase-3-Tag. Kein FOV.

### Desktop-Menüs

- Kein modaler `MenuFactory`-Dialog: Xash hat das Native Object; Phase-3-`libmenu.so` exportiert kein `GameMenuExports001`.
- Touch-/`exec touch/*.cfg`-Pfad entfernt. Team/Klasse/Buy/Radio = GoldSrc-`ShowMenu` + `titles.txt`.
- `_vgui_menus` 0. `cstrike/liblist.gam` ist CS-Retro-owned.
- Architektur: `docs/MENUS.md`. Tests: `./scripts/interactive-menus.sh`, `./scripts/interactive-3c.sh`.

### Game-Data

Kein Phase-3-Abschluss-Tag. 3D nicht automatisch (FOV erst nach Freigabe).

- Steam CS 1.6 (AppID 10) nur als read-only Quelle. Bootstrap: `scripts/bootstrap-gamedata.py`.
- Manifest `data/gamedata-manifest.json` aus Runtime-Traces. RODIR = `gamedata/`, nicht Steam-HL.
- User-Configs bleiben in BASEDIR. Steam-Updates: `--refresh` nur für Steam-sourced Dateien.

### 3C

- Interaktiver Listen-Lauf `de_dust`: Team, Spawn, Movement, Duck/Jump, Waffenwechsel, Schießen, Reload, HUD, Round/GameRules, Host-Admin-Binds, Shutdown.
- Script: `./scripts/interactive-3c.sh`. ZBot-Runtime-Daten nur unter `build/run/`.
- Listen-`+map`: BASEDIR `valve.rc`/`cstrike.rc` mit `stuffcmds` (auch im Smoke).

### GameDLL

- `rehlds/ReGameDLL_CS` `b088984` nach `server/game/` vendort. Target `csretro_gamedll` → `cs_amd64.so`.
- Eigenes CMake, kein Upstream-CMake/SLN, kein CMake-3.5-Workaround.
- ZBot vollständig mitgebaut (Migration, nicht amputiert).
- 64-Bit: Xash-`unsigned long` für Funktionsnamen, `XASH_64BIT`/`MAKE_STRING`, Save-`FIELD_FUNCTION` über `uintptr_t`.
- Linux x86_64: Xash lädt die Lib; Dedicated- und Listen-Smoke `de_dust` (Connect + Shutdown).
- ASan+UBSan-Entwicklungsbuild: `./scripts/build-gamedll.sh --sanitize`.

### 3A/3B Client (unverändert)

- A1-Body, `client_amd64.so`, `GetClientAPI`.

### Herkunft

- `docs/UPSTREAM.md`: verbindliche Upstream-Policy (beobachten, selektiv porten, kein Auto-Sync).
- `CREDITS.md`: dauerhafte Danksagung; ergänzt AUTHORS/README der Upstreams, ersetzt sie nicht.

### Nicht enthalten

- Kein 3D-Feature-Port, kein Phase-3-Release, keine Windows-/macOS-Runtime.

## 0.1.4-phase2 — 2026-09-01

### Schnitt

- `steam_api_proxy/` entfernt; Launcher lädt `steam_api.dll` nicht mehr.
- 8684-Address-Provider entfernt; NitroApi hookt keine Steam-`hw.dll`/`client.dll` mehr.
- Steam-Master (`hl1master`) und `MatchmakingSteamComp` entfernt.
- Xash-Exportvertrag: `client/export/csretro_cdll_export.h`, CMake-Target `csretro_client_export`.

### Behalten

- Feature-Quellen: `client_mini` (GameHud, View, FOV, …), `engine_mini` NCLM/HTTP-Master, GameUI als Quelle.

### Nicht enthalten

- Kein Phase-3-Body, kein `GetClientAPI`-Rumpf, kein `client/body/`.

## 0.1.3-a1 — 2026-09-01

### Dokumentation

- Gate: **A1 — Ref A als Client-Body** (2026-09-01).
- NextClient bleibt funktionale Zielbasis. cs16-client nur Xash-Unterbau (Allowlist).
- `docs/ROLLEN.md`; Handoff, Phasen, Lizenzen, Architektur, Refs nachgezogen.
- A1-Lizenz dokumentiert, nicht als vollständig geklärt markiert.

### Nicht enthalten

- Kein Phase-3-Body-Code, kein Ref-A-Vendor nach `client/body/`.

## 0.1.2-gate — 2026-09-01

### Dokumentation

- Gate vor Phase 3: Körper-Quelle A0 (neu schreiben) oder A1 (Ref A nur als `cl_dll`-Körper, GPL-Attribution).
- Option A bleibt die Form (Export + Körper + Features), nicht die stillschweigende Entscheidung „Körper von Null“.
- `docs/PHASEN.md`; Architektur/Handoff/Lizenzen nachgezogen.

## 0.1.1-phase1 — 2026-09-01

### Dokumentation

- Phase-1-Analyse: NextClient ist Overlay, kein Client-Körper.
- Entscheidung: eigener `GetClientAPI`-Export + eigener Körper + Features aus `client_mini` als Module. Ref A nur gelesen.
- Architektur festgehalten (heute: `docs/PHASEN.md`, `docs/SCHNITTSTELLEN.md`).

### Nicht enthalten

- Kein Client-Code, kein Ref-A/B-Import, kein Steam-Schnitt (Phase 2).

## 0.1.0-phase0 — 2026-09-01

### Repo

- GitHub `benjarogit/csretro` `main` geleert und durch diesen Stand ersetzt.
- Altes Xash+cs16-client-Monorepo und Release `v0.2.0` gelten nicht mehr.

### Hinzugefügt

- Leeren Worktree als CS-Retro-Monorepo angelegt (kein Altbestand).
- Vendoring ohne Git-Submodule:
  - `engine/` — Xash3D-FWGS `1442d14`
  - `client/` — NextClient `f5addc2` + NclNitroApi `f73fc1a` + ncl-hl1-source-sdk `46c3103`
  - `server/` — NextClientServerApi `1c7e5c6`
  - `refs/a-cs16-client/` — Velaron/cs16-client `bb60674` (eingefroren)
  - `refs/b-cs16-goldsrc/` — FuryBaM/cs16-goldsrc-client `b662acc` (eingefroren)
- `bots/` als leerer, getrennter Bereich.
- Einheitliches CMake-Gerüst (Clang; 32-Bit-Presets später entfernt).
- Engine-Build-Skript (Waf + Clang, 64-bit); erster erfolgreicher Clang-Build der Engine.
- Rollen-, Lizenz-, Upstream-, Schnittstellen- und Handoff-Doku.

### Nicht enthalten

- Microsoft vcpkg (bewusst nicht vendort).
- Spielinhalte (valve/cstrike).
- Lauffähiger NextClient unter Xash (Phase 1–3).
- Code aus Referenz A oder B.
