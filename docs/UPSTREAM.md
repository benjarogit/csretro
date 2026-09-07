# Upstream-Pins (Vendor, kein Submodule)

**Unser Origin:** https://github.com/benjarogit/csretro (`main`, privat).
Import 2026-09-01, shallow clone, `.git` entfernt. Kein `git submodule`.

Altes Remote-`main` (Xash+cs16-client bis `8ece11c`, Tag `v0.2.0`) ist kein Upstream mehr.

Danksagung: `CREDITS.md`. Lizenzen: `docs/LIZENZEN.md`.

## Upstream-Policy (verbindlich)

CS Retro ist **nicht** von seinen Upstreams abhängig und folgt keinem Upstream automatisch.

Alle relevanten Quellen werden vendort und danach als Bestandteil dieses Worktrees gepflegt.

Upstreams bleiben dokumentiert und werden als Entwicklungs-/Ideenquelle beobachtet. NextClient bleibt die **funktionale Zielbasis des Clients**. Diese Policy ändert die Rollen in `docs/ROLLEN.md` nicht.

### Nicht automatisch

Kein automatisches Merge, Rebase, Vendor-Refresh, Submodule-Update, „immer HEAD ziehen“.

### Selektiver Port (kein Cherry-Pick im Git-Sinn)

Vendorte Trees haben keine `.git`-Historie. „Cherry-pick“ heißt hier: selektiver Upstream-Port/Backport.

Vorgehen:

1. Upstream-Änderung entdecken.
2. Prüfen, ob sie für CS Retro relevant ist.
3. Mit unserer Implementierung vergleichen.
4. Nur das Sinnvolle übernehmen.
5. An Architektur, 64-Bit und Cross-Platform anpassen.
6. Testen.
7. Herkunft hier dokumentieren (Tabelle unten).
8. Keine alte/doppelte Implementierung zurücklassen.

Ist unsere Lösung bereits besser oder vollständiger: nichts übernehmen.

Ein Upstream darf temporär gefetcht werden, um einen einzelnen Patch zu lesen. Daraus entsteht keine dauerhafte Remote-/Submodule-Abhängigkeit.

**Beobachten → Bestes auswählen → in CS Retro integrieren → CS Retro bleibt eigenständig.**

Wenn Upstream A, B und CS Retro dieselbe Funktion haben: alle Varianten vergleichen, eine CS-Retro-Implementierung behalten. Beobachtung darf keine parallelen Kopien erzeugen.

## Quellen

| Pfad | Repo | Rolle | Pin | Import |
|------|------|-------|-----|--------|
| `client/` | https://github.com/CS-NextClient/NextClient | funktionale Client-Zielbasis | `f5addc2ba0276d30d26c40eeb8d51fb9fc416102` | 2026-08-02 |
| `client/dep/NclNitroApi/` | https://github.com/CS-NextClient/NclNitroApi | Port-Quelle / Typen | `f73fc1a7592ba413aa382b5b3857e4a40b1b545e` | mit NextClient (nicht HEAD) |
| `client/dep/NclNitroApi/dep/ncl-hl1-source-sdk/` | https://github.com/CS-NextClient/ncl-hl1-source-sdk | HL1-SDK-Typen | `46c310389d669685ec953b8c23f651bd60465e19` | 2026-06-24 |
| `server/` | https://github.com/CS-NextClient/NextClientServerApi | NCLM/Protokoll-Herkunft, nicht GameDLL | `1c7e5c61191f1b949a28cade96dc1d821eedb335` | 2026-07-01 |
| `engine/` | https://github.com/FWGS/xash3d-fwgs | einzige Engine | `1442d14a69093780389104dcb7369aa3685945cf` | 2026-08-27 |
| `client/body/` | https://github.com/Velaron/cs16-client | A1-Client-Body (Manifest) | `bb60674c120ae9bf8fa7854018bea8a77e71c17f` | 2026-09-01 (Vendor) |
| `refs/a-cs16-client/` | https://github.com/Velaron/cs16-client | Body-Referenz, nicht gebaut | derselbe Pin | 2026-08-24 |
| `refs/b-cs16-goldsrc/` | https://github.com/FuryBaM/cs16-goldsrc-client | Menü-/VGUI-Referenz bereits in Phase 3M; gezielte zusätzliche Feature-Ports später | `b662acca3ce74c2c9851cc842592c58661d95799` | 2026-08-27 |
| `server/game/` | https://github.com/rehlds/ReGameDLL_CS | GameDLL-Körper | `b0889847fe6d03898be88acc9e366660efb40ab5` | 2026-09-01 |

Zuletzt geprüft (Clone/Vergleich, kein Sync): ReGameDLL_CS 2026-09-01 = Pin.

Engine-3rdparty (mitimportiert, kein Submodule): MultiEmulator, bzip2, xash-extras, gl-wes-v2, gl4es, libbacktrace, libogg, library_suffix, maintui, mainui (+ miniutl), mbedtls, nanogl, opus, opusfile, vgui_support (+ vgui-dev), vorbis.

### Beobachtet, nicht vendort

| Repo | Rolle |
|------|--------|
| https://github.com/yapb/yapb | Bot-Ideenquelle; später mit ZBot und weiteren vergleichen |
| https://github.com/dreamstalker/rehlds | ReGameDLL-Grundlage; nicht unsere Engine |
| `microsoft/vcpkg` | Windows-Package-Manager, nicht im Tree |
| https://github.com/nagist/metahook | Original-MetaHook (GoldSrc-Plugin-Framework). **Nur Abschauen:** Radar-/Client-Features. Kein Vendor, keine Hook-Runtime, kein Import |
| https://github.com/hzqst/MetaHookSv | SvEngine-Port von nagist/metahook. Bereits VGUI2/HiDPI-Research (Tabelle unten). Zusätzlich **nur Abschauen:** Radar/HUD-Minimap und Client-Features. Kein Vendor, keine Hook-Runtime |
| https://github.com/DeadZoneLuna/css-community | CS:Source: Community Edition — Port des 2007er Source-Leaks nach Source 2013. **Nur Abschauen:** In-Game-Menüs / GameUI (Team/Class/Buy). Nicht Engine-Ziel, kein Vendor, kein Leak-Engine-Import. CS Retro bleibt GoldSrc/Xash |
| `TEMP_EXTRA/hl2_src/` (lokal, **nicht im Git**) | Source SDK 2013 / Half-Life-2-Enginebaum (inkl. Counter-Strike: Source unter `game/client/cstrike/`). **Nur Abschauen:** In-Game-VGUI (Team/Buy), GameUI, HUD-Radar. Andere Engine, kein Vendor, kein Merge. |
| `TEMP_EXTRA/cstrike15_src/` (lokal, **nicht im Git**) | Counter-Strike: Global Offensive (intern **cstrike15**, Source-1-CSGO; Dump vor Hydra/Mai 2017). **Nur Abschauen:** Scaleform-HUD/Radar, Team/Buy. Andere Engine, kein Vendor, kein Merge. |

Radar-Look-at (öffentlich gemeint, nichts vendorn): [Dynamic Radar (Metahook)](https://gamebanana.com/mods/39419), [Dynamic Radar (Metadrawer)](https://gamebanana.com/mods/39420) — Karte im Radar (CSO/CS:GO-artig, Overview-TGA). NextClient hat dieses Radar **nicht** (`docs/PHASE3M.md`, `docs/MENUS.md`). Selektiver Port: Bestes auswählen, in CS Retro nativ umsetzen — MetaHook bleibt Referenz.

css-community Look-at (öffentlich, nichts vendorn; README + Remote-Pfade 2026-09-04, HEAD `b877078`, kein Clone): In-Game-VGUI `mp/src/game/client/game_controls/` (`teammenu`, `classmenu`, `buymenu`) und `mp/src/game/client/cstrike/VGUI/` (`cstriketeammenu`, `cstrikeclassmenu`, `cstrikebuymenu`); Layouts `mp/game/community/resource/ui/` (`teammenu.res`, `classmenu_*.res`, `buy*.res`). `mp/src/game/gameui2/` ist NicolasDe GameUI2 (Hauptmenü; Credits des Repos) — nicht unser In-Game-Pfad. Team-, Class- und Buy-Wahl bei uns VGUI2 (`CTeamSelectPanel`, `CClassSelectPanel`, `CBuySelectPanel`). Herkunft: 2007er Source-Leak-Port; Lizenzen sind kein Gate (`docs/LIZENZEN.md`) — trotzdem **kein** Vendoring der Source-Engine.

`TEMP_EXTRA/` Look-at (dieser Rechner, **nicht im Git**, `.gitignore`; Stand 2026-09-04, kein Vendor-Pin): zwei weitere Source-Bäume **wie** NextClient / Ref B (`cs16-goldsrc`) / css-community — abgucken für Menüs und spätere Features, **kein Merge jetzt**. Engine-Zuordnung:

- **`hl2_src`** — Source SDK 2013 (Half-Life-2-Engine). CS:Source-Client `game/client/cstrike/` (`cstrikebuymenu`, `cstriketeammenu`, `hud_radar.cpp`); generisches GameUI unter `gameui/`. Nicht dasselbe wie css-community (Community-Port des 2007er Leaks).
- **`cstrike15_src`** — Counter-Strike: Global Offensive (`cstrike15`). Radar/Team über Scaleform (`game/client/cstrike15/Scaleform/HUD/sfhudradar.*`, `teammenu_scaleform.*`); Reste von VGUI-`game_controls/buymenu`. Hinweisdatei: Depot 730 vor Hydra (18. Mai 2017). Kein CS2/Source-2.

Radar-Minimap und verwandte HUD-Ideen hängen **nach 3M** (`docs/PHASE3M.md`, `docs/PHASEN.md` 3D). Selektiver Port später: Bestes auswählen, nativ in CS Retro — Source-/CSGO-Engine nicht importieren.

### VGUI2 Research References (Phase 3M)

Dokumentations- und Vergleichsquellen. **Vendort ≠ Produkt-Build ≠ Runtime-Abhängigkeit.** Research-Quelle darf bei Bedarf reproduzierbar in den Worktree vendort werden (Analyse/Port) — ohne sie als Runtime-Abhängigkeit oder stillschweigende Classic-Baseline zu übernehmen.

Prüfdatum aller Pins unten: **2026-09-01**, sofern nicht anders angegeben.

| Upstream | URL | Commit / Stand | Rolle | Konkrete Erkenntnis |
|----------|-----|----------------|-------|---------------------|
| Valve Developer Community — VGUI Documentation | https://developer.valvesoftware.com/wiki/VGUI_Documentation | Wiki, geprüft 2026-09-01; **oldid** wegen Bot-Schutz der History-API nicht abrufbar — vor nächstem Cite erneut History speichern | Hersteller-Doku: Panel-Hierarchie, Lifecycle, Scheme/Fonts, Loc, Proportionality, Build Mode (**Ctrl+Shift+Alt+B**) | Allgemeine VGUI2-Semantik. **Nicht** ungeprüft Source-only (`PANEL_CLIENTDLL`, `SourceScheme`, BaseViewport) übernehmen. Bei Widerspruch: Original-CS-1.6. |
| Understanding VGUI2 Resource Files | https://developer.valvesoftware.com/wiki/Understanding_VGUI2_Resource_Files | Wiki, geprüft 2026-09-01 (oldid s. o.) | `.res`-Struktur, Position/Size, Parent, AutoResize/Pinning | Resource-Semantik für Options-Seiten |
| VGUI2: Creating a panel | https://developer.valvesoftware.com/wiki/VGUI2:_Creating_a_panel | Wiki, geprüft 2026-09-01 (oldid s. o.) | Panel, EditablePanel, LoadControlSettings, SetScheme, SetProportional, Build Mode | Bestätigt Build-Mode-Shortcut |
| NextClient GameUI | Pin `client/` oben | funktional Options/BasePanel | `COptionsDialog` **545×406**, `SetTabWidth(84)` — NextClient-Layout; Funktion behalten, Optik nicht automatisch = Valve |
| Ref B / FuryBaM | `refs/b-cs16-goldsrc/` Pin oben | Menü-/VGUI; Team-, Class- und Buy-Wahl portiert (siehe Ports-Tabelle) | Steam-`vgui2` nicht als Runtime |
| kungfulon/fwgs-vgui2-support | https://github.com/kungfulon/fwgs-vgui2-support | `91868378f21ebb39aefd255db5ae6e21b74a4a3b` (2019-01-21, HEAD zum Prüfdatum) | historische Xash + Steam-`vgui2`-Forschung | Analyse only; deprecated zugunsten kungfulon/xash3d-fwgs; **nicht** Produktgrundlage |
| CKFDevPowered/CKF3Alpha | https://github.com/CKFDevPowered/CKF3Alpha | `4e1ee1bdb2aeebe3548eb57b104f2a9cf4dccf97` (2020-02-04) | klassische rekonstruierte GoldSrc-GameUI (BasePanel, GameMenu, Options, …) | `OptionsDialog.cpp`: `SetBounds(0,0,512,406)` + `SetTabWidth(84)` — Rekonstruktion; konvergent mit Golden-5971 **rendered** 512×406, nicht Binary-Beweis |
| Counter-Strike-16/OpenGoldSrc | https://github.com/Counter-Strike-16/OpenGoldSrc | `9f7bbee933a1ea337758d8e03dfdd2c0515bb41c` (2017-07-25) | GoldSrc-/GameUI-Rekonstruktion, VGUI2/BaseUI | `ogs/gameui/default/OptionsDialog.cpp`: ebenfalls **512×406** + `SetTabWidth(84)` |
| Golden 5971 Screenshot Artifact | `docs/research/golden-5971/` | SHA-256 `0f73bd7b45c3980a780c8abfe6f25e779c0893b8935723fff5697199c6f12cfd` | visuelle Classic-Referenz + screenshot-derived metrics | OptionsDialog **512×406** rendered (logisch 1366×768); HD/prop-State **unknown** |
| hzqst/MetaHookSv (VGUI2Extension) | https://github.com/hzqst/MetaHookSv | `bb5f7833cd6908638ab48c7315f6b0240c6caafb` (2026-08-30); Doku `docs/VGUI2Extension.md`, `include/Interface/VGUI/ISurface.h` | Scheme/Resource-Injection; HiDPI; HL25 Surface. Radar/HUD: nur Abschauen, siehe Beobachtungsliste oben | `ISurface_HL25`: `GetHDProportionalBase`/`SetHDProportionalBase` (HL25-added). HiDPI-Modus: bewusst alle Panels proportional — **Vergleich**, nicht Classic-Ziel. Current Steam 2024 = HL25-era HD reference; **5971 proportional/HD = unknown** |
| Ref A | `refs/a-cs16-client/` Pin oben | Desktop-/Client-Erkenntnisse | kein Ref-A-mainui als Produkt |

Golden Visual vs Current Steam Resources / Metrics: `docs/PHASE3M-METRICS-DIAGNOSIS.md`.

Spielinhalte `valve/` / `cstrike/`: externe Runtime-Datenquelle. Steam CS 1.6 (AppID 10) wird gelesen, nie geschrieben, und nicht als RODIR benutzt. Materialisiert: `gamedata/` (`docs/GAMEDATA.md`). Nicht im Git.
Ref-A-`3rdparty/ReGameDLL_CS/`, Ref-A-YaPB, Ref-A-mainui: nicht die Produktquelle.

Weitere Repos hier eintragen, sobald daraus Wissen, Code oder Verhalten tatsächlich verwendet wird.

## Übernommene Upstream-Ports

Konkrete Fixes/Commits, nicht jede Idee.

| Quelle | Upstream-Commit/PR | Was übernommen | CS-Retro-Commit |
|--------|--------------------|----------------|-----------------|
| — | ncl-hl1-source-sdk | 64-Bit-VGUI-Patches: `VPANEL`→`uintptr_t`, Bitfield-Swap, mempool/threadtools | 2026-09-01 |
| — | ncl-hl1-source-sdk / vgui_controls | Menu/MenuItem/MenuButton/MenuBar: VPANEL-Transport `SetInt`/`MESSAGE_FUNC_INT` → `SetUint64`/`MESSAGE_FUNC_UINT64` (amd64 Hover-Crash) | 2026-09-02 |
| — | ncl-hl1-source-sdk / Panel.cpp | `CPanelMessageMapDictionary` / KeyBinding-Pool: `alignof(PanelMessageMap)` — UBSan misaligned `PanelMessageMap*` (kein globales `-fno-sanitize=alignment`) | 2026-09-02 |
| — | ncl-hl1-source-sdk / AnimationController.cpp | `CPanelAnimationDictionary` Pool: `alignof(PanelAnimationMap)` — gleicher Mempool-Alignment-Pfad | 2026-09-02 |
| — | ncl-hl1-source-sdk / PropertySheet.cpp | `m_bContextButton` in beiden Ctors auf `false` — UBSan invalid bool (190) | 2026-09-02 |
| — | ncl-hl1-source-sdk / mempool.h | `CClassMemoryPool::Clear`: Walk = AlignValue + `m_NumBytes/m_BlockSize` (AddNewBlob-Vertrag) | 2026-09-02 |
| — | ncl-hl1-source-sdk / AnimationController.cpp | `CPanelAnimationDictionary` Dtor: explizit 1× `Free` pro Map statt `Clear()`-Blob-Walk (Shutdown-SIGABRT) | 2026-09-02 |
| — | CS-Retro `surface_xash` | `MovePopupToFront`/`Back` nur für `IsPopup` — sonst PropertyPages als Fake-Popups übermalen Footer | 2026-09-02 |
| NextClient | `gameui/src/GameUi/ScriptObject.{h,cpp}` | Nur die Parse-Semantik von `settings.scr` (Typen BOOL/NUMBER/STRING/LIST, `-1` = keine Grenze) als `client/menu/gameui/ServerSettingsScript.{h,cpp}`. **Nicht** übernommen: `vgui2::Panel`-Vererbung des Datenobjekts, `WriteToConfig`/`WriteToFile`/`TransferCurrentValues` und `mpcontrol_t` — CVars und Configs schreibt bei uns ausschließlich `ServerProfile`/`Profile_WriteListen`, eine zweite Konfigurationsquelle wäre genau das, was die Policy verbietet | 2026-09-03 |
| NextClient / Xash-MainUI / Ref A | `gameui/src/ServerBrowser/*`, `engine/3rdparty/mainui/menus/ServerBrowser.cpp`, `refs/a-cs16-client/3rdparty/mainui_cpp/menus/ServerBrowser.cpp` | Nur das LAN-Muster: Engine-Kommando `localservers`, Antworten über `UI_AddServerToList`, ListPanel-Spalten die in der Broadcast-Antwort wirklich stehen (Name/Adresse/Spieler/Map/Passwort/Antwortzeit). **Nicht** übernommen: Internet-/Favorites-/History-/Friends-/Spectate-Tabs, Steam-Matchmaking (`ISteamMatchmakingServers`), Master-Query (`internetservers` / `NET_MasterQuery`), `CBaseGamesPage`/`ServerList`, Game-Info-/Passwort-Dialoge, Kontextmenü, Bot-/VAC-Spalten (stehen nicht im A2A-Infostring) | 2026-09-03 |
| Ref B | `refs/b-cs16-goldsrc/cl_dll/ui/vgui2/cs_vgui2_client.cpp` (`ResourceForMenu`, `HTML`/`MapInfo`, `jointeam`-Slots) + Steam `cstrike/resource/UI/Teammenu.res` | Nur das Mapping: `Teammenu.res` als `EditablePanel`, `HTML`→`RichText`, Briefing `maps/<hostmap>.txt`, sichtbare Slots aus der Server-Bitmaske, Commands `jointeam N` / `spectate` / `vguicancel`. **Nicht** übernommen: deren Software-Renderer, Frame-Chrome, VGUI1-`CTeamMenuPanel` | 2026-09-04 |
| Ref B | `refs/b-cs16-goldsrc/cl_dll/ui/vgui2/cs_vgui2_client.cpp` (`MouseOverPanelButton`, `ClassInfo`, `gfx/vgui/<field>.tga`, `joinclass`) + Steam `cstrike/resource/UI/Classmenu_TER.res` / `Classmenu_CT.res` | Nur das Mapping: `Classmenu_*.res` als `EditablePanel` (`ClientScheme`-Overlay wie Team), `MouseOverPanelButton`→Hover-Portrait in `ClassInfo`, Commands `joinclass N` / `vguicancel`. CS-1.6: Militia/Spetsnaz aus, Auto-Select = Slot 5 / `joinclass 5`. **Nicht** übernommen: deren Software-Renderer, Frame-Chrome | 2026-09-04 |
| Ref B | `refs/b-cs16-goldsrc/cl_dll/ui/vgui2/cs_vgui2_client.cpp` (`ResourceForMenu` Buy-`.res`, `ItemInfo`, Aliase) + Steam `cstrike/resource/UI/MainBuyMenu.res` / `Buy*.res` | Nur das Mapping: Buy-`.res` als `EditablePanel` (Overlay wie Team/Class), `_CT`/`_TER` nach Team, `.res`-Command = Unterseite, Waffen-Aliase `glock`/`vest`/… → ReGameDLL `HandleBuyAliasCommands`, Steam-`autobuy`/`rebuy` bleiben Client-HUD-Commands (`autobuy.txt`/`rebuy.txt` → `cl_setautobuy`/`cl_setrebuy`). Nicht direkt `cl_autobuy`/`cl_rebuy` ohne Liste. **Nicht** übernommen: deren Software-Renderer, Frame-Chrome, WizardPanel | 2026-09-04 |
| lokal `TEMP_EXTRA/cstrike15_src` + `TEMP_EXTRA/hl2_src` | `game/client/game_controls/buymenu.*`, `cstrike15/VGUI/cstrikeloadout.*`, CS:S `VGUI/cstrikebuymenu.cpp` | Nur Zustands-/Lifecycle-Abgleich (Öffnen/Schließen, Team-/Konto-/Buy-Time-Daten, Loadout-Vertrag). Die sichtbare CS:GO-Scaleform-Buy-Komposition ist im Dump nicht als editierbares Layout enthalten; sie wird anhand der Produktreferenz nativ in `CBuySelectPanel` rekonstruiert. **Nicht** übernommen: Source-/Scaleform-Code, Flash-Assets, Source-Engine-Klassen oder Katalog | 2026-09-07 |
| ReGameDLL + Steam `titles.txt` + CS:GO `sfhud_radio` | `client.cpp` `radioInfo[]`; `gamedata/cstrike/titles.txt` RadioA/B/C; `TEMP_EXTRA/cstrike15_src/.../sfhud_radio.cpp` `ShowRadioGroup` | Aliase und ShowMenu-Titel → Typ 35–37. Texte/Farben aus `titles.txt`. Karte = kompaktes HUD-Panel (CS:GO: clientseitig raised, kein Viewport-Dim). **Nicht** übernommen: Team-Overlay, Scaleform/Flash, Text-`ShowMenu` als Primär-UI | 2026-09-04 |
| NextClient `CGameUI` / `CBasePanel` + Steam `GameMenu.res` | `ActivateGameUI` → `OpenPauseMenu`; `UpdateMenuItemState` (`OnlyInGame`, `notsingle`, `notmulti`); `HideGameUI` | In-Game Escape = dieselbe GameMenu-Liste, Dim statt Wallpaper, Sichtbarkeit über `gGlobals->maxClients`. **Nicht** übernommen: `setpause`/`unpause` (friert CS-Listen/MP ein), totes `OpenPauseMenu`-Kommando, PlayerList-Dialog | 2026-09-04 |
| Steam `UI/Spectator.res` + Ref B `vgui_SpectatorPanel` + Body `CHudSpectatorGui` | Felder Top/Bottom, Scores, Timer, Map, Ziel+HP; Ref B: transparente Borders | Eigene VGUI-Fläche (`CSpectatorHudPanel`), State vom Client-HUD (`SetSpectatorHud`). Orange `FillRGBABlend`-Balken aus, wenn die Menü-Lib da ist. **Nicht** übernommen: Steam-`.res` als Team-Viewport, Prev/Next-Buttons (später), Faceit-1:1, Radar, Mid-Scoreboard | 2026-09-04 |
| Body `CHudScoreboard` + Ref B `vgui_ScorePanel` | TAB `+showscores`/`-showscores`; Spalten Name/Score/Deaths/Ping; Team-Blöcke T/CT | Eigene VGUI-Fläche (`CScoreboardHudPanel`), State vom Client-HUD (`SetScoreboardHud`). Orange HUD-Tafel aus, wenn die Menü-Lib da ist. **Nicht** übernommen: VGUI1-Grid, Avatare, Voice-Spalte, Steam-`.res` als Viewport, Seitenkarten | 2026-09-04 |

## NextClient-Vendor nach Phase 2

Lokaler Schnitt gegenüber dem Pin: `steam_api_proxy/` weg, 8684-Provider weg, `MatchmakingSteamComp` weg. Ein späterer NextClient-Port muss das wiederholen oder bewusst lassen.

GameDLL-Produkt-Build: `cmake/CsretroGameDll.cmake`. Nicht Ref A. Details: `server/game/ATTRIBUTION.md`.
