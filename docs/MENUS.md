# Menüs

Lebende Architektur. Rollen: `docs/ROLLEN.md`. Schnittstelle: `docs/SCHNITTSTELLEN.md`.
Phase: `docs/PHASEN.md` · Arbeitsdokument: `docs/PHASE3M.md`.

CS Retro ist **64-Bit-Desktop** (Linux x86_64, Windows 10+ x86_64, macOS ARM64/x86_64). Kein Android, iOS, Switch, Vita, keine Touch-/Mobile-UI.

## Endziel

Bedienung und Grundkonzept: klassisches **Steam Counter-Strike 1.6 mit VGUI2**, nicht WON-Hauptmenü und nicht Textmenü als Primär-UI.

Branding: **CS Retro**. Zusätzliche Funktionen nur in diesem Stil, und nur wenn das Backend existiert.

**Drei Ebenen (nicht gegeneinander ausspielen):**

| Ebene | Rolle |
|-------|--------|
| Original-CS-1.6 | interaktive und metrische **Baseline**, nicht Funktionsdeckel und nicht Optik-Deckel |
| NextClient | funktionale **Basis** |
| CS Retro | moderne und zusätzliche Funktionen (Desktop/HiDPI/Responsive, ServerProfile, Module, …) **und die moderne visuelle Schicht** |

### Visuelle Schicht: Feeling von damals, Fenster wie CS:Source

Verbindlich seit 2026-09-03, **korrigiert 2026-09-03 (Abend):** Create-Game mit Doppel-Rundung (Frame **und** innere Liste/ListPanel) ist **keine** Referenz. Eine Kante außen und darin nochmal abgerundet ergibt keinen Sinn.

| | |
|---|---|
| **Feeling und Grundkonzept** | CS 1.6 / VGUI2: Wiedererkennbarkeit, Bedienlogik, Olive, Tabs, Footer (OK/Cancel/Apply), Control-Arten, Sprache der UI, Classic-Metriken als Maßstab für Zeilenhöhen, Beschriftungsbreiten und Feldbreiten |
| **Hauptmenü-Hintergrund** | CS-Retro-eigenes Motiv (`resource/background/csretro.png`). Nicht die Steam-CS-1.6-Kacheln (`800_*_loading.tga` / `BackgroundLayout.txt`). |
| **Dialog-Chrome (Standard-Optik)** | **Counter-Strike: Source:** genau **eine** gerundete Hülle (`Frame`, dezent ~8px, dünner heller Rand, durchscheinendes Olive). Alles innen — Tab-Inhalt, Listen, Settings-Liste, Buttons, Combos — **scharf, 90°**, flaches dunkles Rechteck, dünne 1px-Kante. Aktiver Tab geht in die Inhaltsfläche über. |
| **Nicht** | Nibble/Glass-Karte an Kind-Panels · 14px-Ball plus innere 8px-Kurve · Orange-Theme · Glow · pixelgenaue 2003-Kopie · fremdes zweites Design-System · Hauptmenü auf CS:S-Versalien umbauen |

Feeling bleibt CS 1.6. Aussehen der **Fenster** ist die CS:Source-Hierarchie — dieselbe Sprache auf Create Game, Find Servers und Options. Diese Desktop-Optik ist seit **2026-09-04 visuell abgenommen** und bleibt. Sie gilt **nicht** für In-Game-Menüs.

### In-Game-Familien (nicht eine Optik für alles)

Team/Class/Buy sind **eine** Familie (Steam-`.res`, Viewport). Radio ist eine **andere** (HUD-Befehlsliste). Spectator ist wieder eine **eigene** Fläche (Rahmen um die Welt). Scoreboard kommt danach und bleibt getrennt.

| Familie | Wer | Jetzt | Nicht |
|---------|-----|-------|--------|
| **Desktop-GameUI** | Hauptmenü, **Pause** (Escape im Spiel), Options, Create Game, LAN-Browser, Konsole | CS:Source-Frame, TrackerScheme — **bleibt**. Pause = Welt unscharf + mittig „Pausiert“, dieselbe Liste zentriert | In-Game-Orange, Titelseiten-PNG als Pause, Team-Viewport |
| **Team / Class / Buy** | Wahl- und Kauf-Viewport | Steam-`.res` bleibt Funktion; Look: dunkle Karten, Team Split-T/CT, Buy-Raster, Welt durchscheinend (`InGameViewportLook`) | Radio-Karte, GameUI-Frame, Orange-ClientScheme |
| **Radio** | `radio1`/`radio2`/`radio3` | Kompakte HUD-Karte, `titles.txt`, Bewegung bleibt (`KEY_DEST_GAME`). Anreiz 2026-09-04: dunkle Liste, nummeriert, Welt sichtbar | Team-Viewport, Bewegungssperre, GameUI-Frame |
| **Spectator** | Zuschauermodus | Grundfunktion + erste Look-Scheibe **abgenommen 2026-09-04** (`HudFrameLook`: Rahmen, T/CT-Akzent oben). Feinschliff später. Kein Faceit-1:1 | Team-Viewport, `KEY_DEST_MENU`, Steam-`.res` als Overlay, Radar/Veto/Avatare/Waffen-Icons/Rundenhistorie |
| **Scoreboard** | TAB / `+showscores` | Grundfunktion + erste Look-Scheibe **abgenommen 2026-09-04** (Tafel, Gold/Blau-Kopf, Name/K/D/Ping). Spalten, Schrift und leere Teamseite später verfeinern | Spectator-Klon, Team-Viewport, `KEY_DEST_MENU` |

Abguck für In-Game-Modernisierung: Steam-`.res` + Ref B (Team/Class/Buy/Spectator-Felder), `TEMP_EXTRA/hl2_src` (CS:S VGUI), `TEMP_EXTRA/cstrike15_src` (CS:GO Scaleform `RadioPanel` / Team/Buy). **Inhaber-Anreize 2026-09-04 (Idee, kein Pixel-1:1):** Radio = kompakte dunkle Listen (Bild 1); Team = moderne Split-T/CT-Fläche mit Modellen/Listen (Bilder 2–3); Buy = Kategorie-Raster mit Preis/Icon, Welt durchscheinend (Bild 4); Spectator = dunkler Rahmen, Scores/Timer oben, Spielerdaten am Rand (Bild 5) — ohne Map-Veto, Avatare, Waffen-Icons, Radar, 30-Runden-Historie, Turnier-Branding. Selektive Ideen, kein Engine-Merge, kein CS2-Port.

Umsetzungsweg: die Schicht entsteht **im VGUI2-Core und im Scheme**, nicht als Pixelhack pro Dialog. `Frame::PaintBackground` zeichnet die einzige Rundung + Outline. `CreateGameSettingsList` und `ListPanel` sind rechteckig (Padding bleibt). `Panel::DrawBox` Type 2 ohne Ecktexturen fällt auf ein scharfes Rechteck zurück, nicht auf Nibble.

Classic-Metriken = 100%-Referenz und Regressionstest; danach kontrollierte Skalierung (Linux/Windows/macOS, Auflösungen, HiDPI) als bewusste CS-Retro-Schicht — keine globale historische Proportional-Skalierung, die Controls nur aufbläht.

**Eine** Menü-Library:

```
Xash
 ├─ GetMenuAPI
 │      ↓
 │   CS-Retro GameUI / Hauptmenü
 │
 └─ MenuFactory
        ↓
    CreateInterface
        ↓
 GameMenuExports001
        ↓
 CS-Retro In-Game-VGUI
```

Kein dauerhaftes Nebeneinander aus Xash-MainUI, Textmenü, Ref-A-Touch, NextClient-GameUI-DLL und einem zweiten In-Game-Menü.

**Config-Freiheit als Zielvorgabe:** Was relevant ist, wird im Menü angeboten — Grafik, Server,
Gameplay, Bots. Der Nutzer soll nichts in einer `.cfg` von Hand eintragen müssen, um an eine
wichtige Einstellung zu kommen. Eine Einstellung, die es nur in der Config gibt, ist eine
offene Aufgabe im Menü, kein Feature der Config. Bestandsdateien (`.scr`, `.res`, Engine-Defaults)
sind Ausgang, nicht Deckel: fehlt dort etwas Relevantes, wird es ergänzt und im Menü angeboten.

**Wie die Auswahl entsteht (verbindlich):** Wichtige Einstellungen kommen ins Menü
(Create Game, Optionen, Dedicated, Bots), damit niemand in einer `.cfg` rumwurschteln muss.
Gefunden werden sie **nicht**, indem man `settings.scr` als Katalog nimmt. Man geht die
CVars und Register durch und **entscheidet** dann, was wirklich wichtig ist — nicht alles
1:1 ins Menü kippen.

Fundorte (Katalog, nicht Deckel):

| Bereich | Durchgehen |
|---------|------------|
| Server | `listenserver.cfg`, Server-CVars, ReGameDLL `game.cpp` und verwandte Registers |
| Client | `config.cfg`, User-CVars, Options-Seiten |
| Bots | Bot-CVars + später CS Retro Bot (`docs/BOTS.md`) |

Was wichtig ist und in den Bestandsdateien fehlt, wird ergänzt (`settings.scr`,
Options-`.res`, eigene Tokens). Die Bestandsdateien bleiben Ausgang, nicht Deckel.

**Gute Voreinstellungen sind Teil des Produkts.** Ausgeliefert wird ein Stand, der ohne
Nachjustieren gut spielbar ist — Grafikdetails, Serverwerte und Bot-Konfiguration. Nicht
Nullwerte oder Engine-Rohdefaults, die der Nutzer erst brauchbar machen muss.

## Funktionale Quelle

`client/nextclient/gameui/` ist die **primäre funktionale Quelle**. Nicht von Null beginnen.

Erhalten (Verhalten): BasePanel, Server Browser, Options, Create Multiplayer (Server/Game/Bots), später Module.

**Options (erweiterbar):** klassische Seiten + CS-Retro/NextClient — u. a. Keyboard/Bindings, Mouse, Audio, Video, Voice, Multiplayer, Gameplay, HUD, Radar, Crosshair-Fine-Tuning, Network, später portierte NextClient-Funktionen. Feature-UI erst sichtbar, wenn das Backend existiert (keine toten Optionen). Classic Preferred / Reference Size **512×406** (`CsretroOptionsClassic`); content-driven grow bei Bedarf — kein permanenter 545-Slack.

**Spätere Options-/HUD-Ziele (nicht 3M, nur mit Backend):**

| Ziel | Quelle / Korrektur |
|------|---------------------|
| **Crosshair-Fine** | Sehr individuell über die Optionen. Funktionale Quelle NextClient: `client/nextclient/client_mini/src/hud/HudCrosshair.{h,cpp}` (Typen Cross/T/Kreis/Punkt, `cl_crosshair_*`, Dynamic) und GameUI `OptionsSubMultiplayer.cpp` (Farbe/Größe/Typ/Translucent/Vorschau). Produktziel feiner als die NextClient-Presets. |
| **Radar-Minimap** | Karte im Radar (CS:GO/CS2-artig), nicht nur Punkte auf leerem Kreis. NextClient hat **kein** Map-Radar: `HudRadar` (`HudRadar.cpp:13–14`) ruft nur Steam-`CHudHealth__DrawRadar`. Body-Ist ist das klassische Sprite-Radar (`client/body/cl_dll/hud/radar.cpp`). Abguck — nicht Vendor — MetaHook / GameBanana Dynamic Radar **und** lokale `TEMP_EXTRA/`-Bäume (Source SDK 2013 CS:S-`hud_radar`, CS:GO Scaleform-`sfhudradar`): `docs/UPSTREAM.md`. Nach 3M. |

Ersetzen (Anbindung): Steam-/GoldSrc-GameUI, `HWND`/`SetWindowLongPtr`, `next_engine_mini.dll`, NitroApi-/Steam-Bind, `-m32`, Win32-only-Libs, CEF außer später bewusstem Cross-Platform-Bedarf.

Ziel: **NextClient-GameUI-Verhalten → native CS-Retro/Xash-Desktop-UI.**

## Jetzt vs. Bootstrap (3C)

| Zustand | Hauptmenü / Pause | In-Game Team/Klasse/Buy/Radio |
|---------|-----------|-------------------------------|
| **3C-Baseline** (Release `v0.1.5`, bleibt gültig) | Xash-MainUI (`GetMenuAPI`, kein `CreateInterface`) | GoldSrc-`ShowMenu` + `titles.txt` (`_vgui_menus` 0) |
| **3M-Ziel** | CS-Retro-Menü-Lib, `GameMenu.res` + **TrackerScheme** (GameUI) | VGUI-Viewport über `GameMenuExports001` |
| **3M jetzt** (dieser Host) | CS-Retro-Lib (`-menulib menu_amd64.so`); Pause = dieselbe Liste über der Welt | Team/Class/Buy = VGUI2-`.res`-Viewport (Modernisierung offen); Radio = eigene HUD-Karte; Spectator = Rahmen; Scoreboard = mittige Tafel |

`ShowMenu` wird **nicht gelöscht**. Es bleibt Kompatibilität für serverseitige Textmenüs, Plugins, später AMXX/Metamod. Es ist **nicht** die primäre CS-Retro-Team-/Buy-/Radio-Oberfläche.

Xash-MainUI ist nur Bootstrap, bis die eigene Lib lädt.

## VGUI2-Modell (Entscheidung)

**Kein** Steam-`vgui2.dll` / `vgui2.so` als Runtime.

`MenuFactory` ist nur der Factory-Weg der geladenen Menü-Lib, keine VGUI2-Implementierung.

**Reuse-Gate 2026-09-01: Variante V1** (`docs/PHASE3M.md`):

- vendorter VGUI2-Core + `vgui_controls` aus `ncl-hl1-source-sdk` **intern** in die eine Menü-Lib
- Backends: Surface/Input/System/Localize/Filesystem → Xash / plattformneutral
- NextClient-GameUI (Options/CreateMP/Browser) darauf portieren
- In-Game: `GameMenuExports001` auf demselben Core
- V1-Runtime-PoC **bestanden** unter Xash (zeichnen + Maus/Tastatur/TextEntry/Tab/Escape/Resize, FreeType-Glyphen): `./scripts/vgui-v1-poc-runtime.sh`

V1 zeichnet bereits. Der Text-/Rect-Bootstrap in `client/menu/` bleibt nur **pro UI-Bereich**, bis die jeweilige echte VGUI2-Rekonstruktion ihn ersetzt. **Hauptmenü, Pause, Options, Create Game, LAN-Server-Browser, Team-, Class-, Buy- und Radio-Wahl sind ersetzt.** Der Browser hat bewusst kein Internet-Tab — eigene Serverliste kommt später (`docs/SERVER.md`). Kein paralleles zweites GUI-Framework.

Der Interim-Pfad `Menu_DrawText` → `pfnDrawConsoleString` zeichnet mit dem **Engine-Konsolenfont**, nicht mit der UI-Schrift. Jeder Bereich, der noch darüber läuft, sieht deshalb anders aus als der Rest — das ist der sichtbare Rest-Indikator, kein Font-Bug.

## Ressourcen

1. Steam-materialisiert: `gamedata/cstrike/resource/` (inkl. `UI/`), `gamedata/valve/resource/`
2. Bei Bedarf: `gamedata/platform/resource/` (TrackerScheme, Rahmen-Icons, Localization, Fonts) — **keine** Platform-Binaries, kein `vgui2.dll`, kein SteamAPI
3. CS-Retro-Overrides: `data/ui-overrides/` (Bootstrap kopiert darüber) — inkl. **Hauptmenü-Hintergrund** `cstrike/resource/background/csretro.png`
4. User-Configs: `XASH3D_BASEDIR`

Nicht distributieren. Steam bleibt read-only Quelle.

**Kodierung:** Valve-Localization (`resource/*_english.txt`) ist **UTF-16LE**. Overrides in `data/ui-overrides/` müssen das behalten — eine als UTF-8 eingefügte Zeile macht die ganze Datei unlesbar, ohne dass die UI abstürzt; sichtbar an `CSRETRO_LOC_<tag> FAIL` oder `CSRETRO_LOC_MISSING` (UTF-8 im BASEDIR überschattet die gültige GameData-Datei). Play/Isolate/Bootstrap wandeln UTF-8-Kopien nach UTF-16LE. `.res`-Dateien sind dagegen ASCII/UTF-8.

## Serverprofil

Ein Modell für Listen/LAN und Dedicated — speicher- und wiederverwendbar. Create-Game-VGUI2 ist zentrale Serverkonfiguration, keine UI-Sonderlösung:

```
ServerProfile
 ├─ Server     (Map, Mode, Hostname, Password, MaxPlayers/Slots, LAN)
 ├─ Gameplay   (GameRules, Round/Freeze/Buy, Team/FF/Balance, …)
 ├─ Bots       (über Bot-Konfigurationsschnittstelle; nicht dauerhaft ZBot-only;
 │              Quota/Difficulty/Team/… + implementationsspezifische Advanced)
 ├─ Modules    (optional; Core ohne Plugins lauffähig; Aktivierung, Plugin-Auswahl,
 │              Profile. Heute Metamod / AMX Mod X, Zielzustand eine native
 │              CS-Retro-Plugin-Schnittstelle ohne Zwischenschicht — `docs/PHASEN.md`
 │              Phase 7; strukturierte Seiten für bekannte Plugins später möglich)
 └─ Advanced   (Server-CVars)
```

Create Game, Server Browser und Dedicated-Start teilen sich dasselbe Profil.
Dedicated ist nur ein anderer Startweg (Listen/LAN vs. Dedicated), keine zweite
Konfigurationsquelle und kein zweiter Wizard. Die Dedicated-UI kommt, wenn das
Backend dafür da ist (Xash dedicated / vorhandene Engine-Wege). **Nicht jetzt bauen.**
`docs/SERVER.md`.

Welche Felder ins Profil gehören, entscheidet die CVar-Katalog-Methode unter
Config-Freiheit — nicht der aktuelle Inhalt von `settings.scr` allein.

**Zielbild (nicht jetzt bauen; nicht verloren gehen):**

- **Clan-Tag** neben dem Spielernamen (Create / Profil / Name). Im Spiel der Tag
  etwas fetter als der Name. Kleine Spielerei, kommt später.
- **Team-Balance, Team-Beschuss (FF)** und vergleichbare klassische Serverregeln
  gehören ins Create-Game-Menü. FF und Autoteambalance sind im laufenden
  Create-Game-Umbau schon vorgesehen — Methode festhalten, den Umbau nicht
  überschreiben.
- **Online vs. offline** beim Server erstellen: LAN/Internet-Sichtbarkeit
  (`sv_lan` bzw. eigenes Netz), an unser Protokoll koppeln — `docs/SERVER.md`.
- **RCON / Server-Zugriff** beim Dedicated/Create: Passwort, ob RCON an, die
  üblichen Zugangs-Einstellungen.

Umsetzungsstand (Phase 3M, Schritt 3): `Server` und `Gameplay` liegen im `ServerProfile`
(`client/menu/src/menu_priv.h`) und gehen über `Profile_WriteListen` in `listenserver.cfg`.
Der Gameplay-Zweig ist bewusst **kein Feld je CVar**, sondern eine Liste aus Name und Wert:
die Seite liest ihre Einträge aus `cstrike/settings.scr`, jede neue Zeile dort wirkt ohne
Codeänderung. Getippt bleiben nur `Hostname`, `MaxPlayers` und `Password` — `Profile_Start`
braucht die Slots als Zahl. `Bots` und `Modules` folgen den geplanten Phasen (`docs/BOTS.md`).

## Quellen (Priorität)

1. NextClient `gameui/` — Funktion
2. Lokale Steam-CS-1.6-Resources — Optik
3. Ref B — Menü-/VGUI-Referenz bereits in Phase 3M (In-Game-Verhalten / `.res`-Mapping); gezielte zusätzliche Feature-Ports später
4. [DeadZoneLuna/css-community](https://github.com/DeadZoneLuna/css-community) — CS:Source In-Game-VGUI-Vergleich (Class/Buy; Team bei uns schon `CTeamSelectPanel`). Beobachten, nicht importieren; nicht Engine-Ziel. `docs/UPSTREAM.md`
5. `TEMP_EXTRA/` (lokal, nicht im Git) — Source SDK 2013 (`hl2_src`, CS:Source Team/Buy-VGUI) und CS:GO cstrike15 (`cstrike15_src`, Scaleform `RadioPanel`/Team/Buy/Radar). Abgucken **jetzt** für In-Game-Familien (Radio ≠ Team), Radar nach 3M. Andere Engines, kein Vendor. `docs/UPSTREAM.md`
6. Xash MenuAPI / MenuFactory
7. `kungfulon/fwgs-vgui2-support` — nur Forschung (`docs/UPSTREAM.md`)
8. Ghidra auf lokalen Original-Binaries — nur wenn 1–7 nicht reicht

## Nicht

- Touch-CFG als Desktop-Fallback
- Ref-A-mainui pauschal
- Steam-`vgui2` als Abhängigkeit
- nicht funktionierende Placeholder-Optionen (FOV/Crosshair erst mit Backend)
- parallele Menüsysteme für dieselbe Funktion
