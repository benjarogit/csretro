# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach substantieller Arbeit Tabelle und „Offene Arbeit“ in derselben Session nachziehen.

Details: `ROLLEN.md`, `PLATTFORMEN.md`, `SERVER.md`, `INFERNO.md`, `UTILITY.md`, `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `MENUS.md`, `GAMEDATA.md`, `PHASEN.md`, `PHASE3-BODY.md` (nur während Phase 3), `PHASE3M.md`, `PHASE3M-KEYBOARD.md`, `PHASE3M-LAYOUT.md`, `PHASE3M-VIDEO.md`, `PHASE3M-METRICS-DIAGNOSIS.md`, `CHANGELOG.md`. Danksagung: `CREDITS.md`.

## Aktueller Stand

### 2026-09-11 — Recheck ~13:35 (Klasse PASS, Buy-Look)

Inhaber: Class-Lineup und Buy-Figur stimmen jetzt überein (Phoenix gewählt → dieselbe Figur im Buy; Wechsel auf anderes Modell ebenfalls). **Klasse = PASS.**

Buy-Look weiter nicht die Vorgabe `buy-menu-idee.png`. Ursache: Pause-Blur + schwarze Platte decken die Map zu (CS:GO zeigt die scharfe Welt). Gold-3D war verworfen, weil es Kleckse waren, nicht die 2D-Icons. Jetzt: kein Blur im Buy, kein Platten-Slab, leichte Abdunklung, Versionstring aus In-Game-Overlays, Leerzeichen in Labels abgesichert.

### 2026-09-11 — Recheck ~13:20 (Klassen gespiegelt, Gold-Waffen)

Inhaber: Spalte „Arctic Avengers“ zeigte Elite Crew (Sonnenbrille). Buy danach Arctic-Mesh. Waffenkarten waren gelbe Kleckse.

Ursache Klasse: Lineup-Laterals hatten das Vorzeichen der GoldSrc-Y-Achse falsch (+Y = Bildschirm-links). Namen/Slots waren korrekt (`1 terror` Phoenix, `2 leet` Elite Crew, `3 arctic`, `4 guerilla`); die Meshes lagen spiegelverkehrt unter den Labels. Klick auf die Sonnenbrille merkte `arctic` — Buy war dann konsistent falsch.

Gold-Silhouette verworfen. Waffenkarten wieder fullbright `w_*.mdl` mit Skin. Footer-Goldtext und leichtes Dimmen bleiben.

### 2026-09-11 — Recheck ~13:05 (Phoenix≠Buy, Layout, Konsole)

Inhaber: Phoenix Connexion gewählt, Buy zeigte Arctic (blaue Jacke). Kaufmenü weiter nicht die CS:GO-Referenz. Smoke-Nade weg = **PASS**. Landung geschmeidig = **PASS**. `~` war die Engine-Drop-Down-Konsole — falsch.

Ursache Klasse: Phoenix-Stem ist `terror` (Team-Default). HUD-Userinfo eines Bots (`arctic`) durfte `joinclass` überschreiben. Joinclass ist jetzt fest; Preview-Index 64+; Menü-Pass zeichnet das gesetzte MDL.

Buy-Look: Welt nur leicht abdunkeln, Footer Gold-Text, keine Klassen-Caption. Gold-Silhouette **verworfen** (13:20). In-Game-`~` = VGUI-GameUI-Konsole.

### 2026-09-11 — Recheck nach 12:36 (Buy-Klasse, Smoke-Wand, Landung, Konsole)

Inhaber: Elite Crew gewählt, Buy zeigte nicht dasselbe Modell. Smoke-Nade steckt in der Wand sichtbar. Kaufmenü weiter nicht wie CS:GO-Referenz. Feuer **PASS**. Landung klebt zu lange. Konsole besser, aber nicht das alte Tipp-Verhalten.

Fixes damals: Userinfo `model`, Smoke NODRAW, Jump-Stamina 450 ms. **Verworfen:** In-Game-`~` auf Engine-Konsole umbiegen — das war der 13:05-FAIL.

### 2026-09-11 — Recheck 12:17 FAIL (Buy-Klasse, Konsole, Feuer-Hitch)

Inhaber: T Guerrilla Warfare, Buy zeigte nicht Guerrilla. Konsole: Vorschläge da, aber kein Weiterschreiben, Enter sendet nicht. Inc/Molotov Wurf+Feuer OK; durchs Feuer laufen = Schaden plus Stocken (1.6-Tagging jedes Inferno-Ticks).

Fixes: Entity-Modell folgt der gewählten Klasse (nicht mehr immer terror/urban). Buy-Caption nennt die Klasse. Konsole-Vorschläge ohne Fokusklau. `DMG_BURN` ohne Velocity-Modifier/Flinch. Inferno-Zahlen unverändert.

### 2026-09-11 — Utility-Zielbild (Docs, kein Code)

HE/Flash/Smoke modernisieren **nach 3M**. Methode: Quellen vergleichen (1.6, CS:GO-Dump, CS2-Feeling, Inferno), Bestes nativ umsetzen — kein Merge. Wurf und Inferno unangetastet. Reihenfolge: HE-Deckung+Tagging, Flash-Winkel+Audio, Smoke-Zellen. `docs/UTILITY.md`.

### 2026-09-11 — Buy-Kamera 202, Inc-Wurf, Konsole (Inhaber 11:03)

Inhaber: New Game → T → Phoenix Connexion. Team-Menü OK, Buy zeigte nicht die Klasse. Log war `model=terror` — das Modell stimmte, die Kamera nicht. Buy sitzt rechts; Team-T yaw 150 ist die linke Figur. Code: Buy immer yaw **202**, Sequenz 80 T / 33 CT, Modell weiter `joinclass`. Team 150/202 und Class-Lineup 158/169/191/202 unverändert.

Inc: Pin+Wurf OK, danach kam der linke Arm nochmal (DRAW nach 1,2 s Smoke-Throw). Idle-Wait jetzt wie HE/Smoke (0,75 / letzte 0,5). Molotov erster Klick Inhaber-PASS, nicht anfassen.

Konsole: kein TAB, HUD gelb/verschoben, nach `sv_restart` blieb sie offen und die Welt stand still. Ursache: `UI_IsVisible()` war true sobald die Konsole offen war → `V_RenderView` return (`ui_renderworld` default 0). Jetzt Overlay (Visible=false), TAB-Complete über Cmd/CVar-Liste, schließen bei Submit von `sv_restart` und bei `ResetHUD`.

languagelawyer Feuer-Sim **nicht** kopieren. Tempents schon da. Inferno-Zahlen bleiben.

### 2026-09-11 — Modelle zurück, Buy-Kamera = Team (Inhaber 09:05)

Inhaber: GitHub nur für **Funktion** (Wurf/Feuer), **nicht** die Viewmodels tauschen. Zippo-Molotov und Fire-Pack-Inc sind wieder in `gamedata/` (Backup `build/asset-backups/20260911-ll-hands/`). languagelawyer-Rebuild nur noch mit `CSRETRO_HE_HANDS=1`.

Buy 09:03: Log war `model=leet yaw=169` — das ist die Class-Lineup-Kamera (vier kleine Karten). 09:05 nahm Team-Kamera 150/202; Inhaber 11:03: T-Buy weiter falsch (Rücken). 11:03-Stand oben.

### 2026-09-11 — Erster Klick Inhaber-PASS (08:43 / 08:44)

Sessions `play-20260911-084320.log` (T Elite Crew) und `play-20260911-084353.log` (CT Seal Team). Erster `+attack` pinnt/wirft sofort: Smoke, Molotov, Incendiary, HE. Kein `selection_consumed`, kein orphan `up` ohne `down`. Overlay `defer_game_dest` / `restore_game_dest` greift. **Nicht** NextAttack als Ursache verkaufen — HUD-Granaten-Equip + Overlay-Maus-up.

Noch offen, gleicher Test:

- **Buy-Figur:** Recheck 12:17 Guerrilla **FAIL**. Entity war Team-Default (terror). Jetzt gewähltes MDL + Name unter der Figur. Log `CSRETRO_BUY_CHARACTER ... model=guerilla`.
- **Buy-Look:** Referenzbild CS:GO-Kaufmenü (`buy-menu-idee.png`) — Layout/Gold/Footer. Nicht in diesem Schritt.
- **Viewmodels:** Zippo/Fire-Pack. Inc-Wurf 12:17 warf und brannte (Arm nicht bemängelt).
- **Konsole:** 12:17 Dropdown fraß Tastatur/Enter. Vorschläge sind kein Popup mehr.
- **Feuer:** 12:17 Hitch. Burn-Ticks taggen/flinchen nicht mehr.

### 2026-09-11 — Erster Klick tot auf allen fünf Granaten (Inhaber 08:37)

Inhaber: der tote erste `+attack` gilt **HE, Flash, Smoke, Molotov und Incendiary**, nicht nur Fire-Nades. Waffen in der Hand feuern beim ersten Druck. Zwei Schichten, beide im Client:

1. **HUD Slot 4 / Rad:** GoldSrc bestätigt die Waffenwahl mit M1, solange `gpActiveSel` gesetzt ist. Alle fünf Granaten liegen in dem Slot. `hud_fastswitch=1` war nur in der Play-Config und im Gate; Default bleibt `"0"`. Jetzt rüsten Slot-Taste und Mausrad Granaten **immer** aus, unabhängig von `hud_fastswitch`. Ein restliches `gpActiveSel` auf einer Granate verbraucht M1 nicht mehr als Bestätigung.
2. **Buy-/Team-/Class-Overlay:** Klick schließt das Menü bei noch gedrückter Maustaste; `KEY_DEST_GAME` + Grab fraß den nächsten `+attack` (Log 08:12: nur `ATTACK up weapon=32`, kein `down`). Overlay gibt die Steuerung erst frei, wenn MOUSE1 oben ist; der schließende `up` wird nicht als `-attack` ausgeführt.

Fire-Nades: Server-`TE_SPRITE`/`TE_DLIGHT` (Optik). Inferno-Zahlen unverändert. Erster Klick danach Inhaber-PASS 08:43/08:44.

### 2026-09-11 — Erster Granaten-Klick nach Buy (Inhaber 08:12)

Inhaber-Erklärung damals: Team/Klasse, kaufen, Runde startet, Granate wählen, erster Druck tot, zweiter Druck wirft. Waffen feuern sofort. Overlay-Mausgrab (Punkt 2 oben) erklärt den 08:12-Log. 08:37 stellt klar: derselbe Ausfall auf **allen** Granaten, auch ohne frischen Buy-Klick — deshalb zusätzlich HUD-Schicht (Punkt 1).

Referenz (temporär gelesen, nicht als Submodule): [cs16-client molotov](https://github.com/languagelawyer/cs16-client/tree/molotov), [ReGameDLL_CS molotov](https://github.com/languagelawyer/ReGameDLL_CS/tree/molotov). Deren Wurf bleibt Stock-HE; bei uns bleibt der CS2-Input-Layer. Übernommen: GoldSrc-Tempents für sichtbares Feuer. **Nicht** deren Viewmodels — Zippo/Fire-Pack bleiben.

### 2026-09-11 — Nachzug zum Inhaber-Test 19:34/19:35

Weiterer Nachzug nach Inhaber-Logs 07:17/07:19: Fehler weiterhin offen; geladene Client-/Server-Hashes stimmen mit Builds überein. Eingabe-Diagnose `CSRETRO_ATTACK down/up` nennt aktive Waffe/Zeit/Pending-Auswahl, `selection_consumed` dokumentiert explizit ein verbrauchtes M1. Lokaler kurzer Maus-Test: weapon=32, down 6.950, up 7.416, anschließend Inferno-Start; keine Reproduktion des zweiten nötigen Drucks. Capture-Overhead verlängert die nominell kurzen Sleeps: nicht als 100-ms-Klicktest bezeichnen. Zusätzlicher Codefehler korrigiert: Lösch-Event eines Infernos löschte global andere Feuerbilder; nun Abgleich der Server-Entitäts-ID und Erstellungszeit. Mehrfach-Inferno-Sichttest noch offen. Referenz-Branches erneut auf WeaponIdle/Deploy und Kaufanbindung geprüft, kein Gesamtimport.

- Live-Teamwahl ist vom Inhaber bestätigt; nicht erneut umbauen.
- M1 blieb nach Mausradwahl kaputt: `invnext`/`invprev` hatten noch die alte Bestätigung, unabhängig vom bereits korrigierten Slot-4-Pfad. Beide berücksichtigen jetzt `hud_fastswitch`; Developer-Log `CSRETRO_WEAPON_SELECT` nennt das Ziel. Nicht erneut NextAttack verändert.
- Unsichtbares Inferno: `InfernoThink` ruft `KillEveryRound` auf, aber neue Sprites hatten `fuser4=0`. Nach einem Round-Reset wurden sie sofort unsichtbar/gelöscht. Erstellungszeitpunkt wird jetzt gesetzt. Der Inhaber-Log bestätigte zuvor Empfang von `weapon=33` ohne Sprite-/Poolfehler.
- Auf ausdrücklichen neuen Wunsch Schaden reduziert: Maximal 32 statt 40 DPS; DamageTick verwendet jetzt die Konfiguration statt hartem 8-Schaden-Limit. Tick 0,2 s, Anlauframpe, Nodes, Brenndauer und Ausbreitung bleiben gleich.
- Gewünschte Molotov-Variante aus `grenades_09_2.rar` lokal übernommen (nur First-Person-MDL); Import unterstützt `CSRETRO_MOLOTOV_SKIN`. Binärvergleich: identisches Rig/Geometrie/Sequenzen, Unterschiede ab den Feuerzeug-/Flaschentexturen. Vorversion unter `build/asset-backups/20260911-molotov/v_molotov.mdl` gesichert. Keine neu erfundenen Animationen behauptet.
- Bewegungsgefühl noch offen: Fire-Nades haben 245 u/s, HE/Smoke 250 u/s. Kein neuer Bewegungstimer eingeführt und nicht ohne Reproduktion verändert. Buy-Referenz weiterhin offen; dieser Nachzug konzentriert sich auf Granatenfehler.
- Client und GameDLL neu gebaut. Testskript erweitert um echte Maustaste/Mausrad und `sv_restart 1`; Capture ist keine automatische Abnahme. Mausrad-Test erreichte HE, nicht Inc: nicht als Inc-PASS zählen. Details/Captures unter `build/fire-shots/ct-wheel-focus/`, `t-candidate/`, `ct-round-final/`.
- Finaler CT-Test mit direkter Auswahl + echtem M1 nach `sv_restart 1`: `CSRETRO_INFERNO_EVENT start weapon=33`, sichtbares Feuer in `ct-round-final/incendiary-after-throw.png`. Molotov nach Restart ebenfalls sichtbar. Kein vollständiger Mehrfachwechsel-/Smoke-Recheck behauptet.
- Kleiner Buy-Referenzschritt: Produktnamen rechts oben/gold, Kürzel weiter links oben; MDL-Rendering unverändert. Gesamtes Layout/Look bleibt offen.
- Sichtprüfung `buy-labels`: Beschriftung sitzt rechts, Figur Elite Crew korrekt. **Molotov-Haltepose offen:** Capture bei ~0,85 s zeigt kein Viewmodel, obwohl Deploy die alternative Flasche zeigt. Nicht als Animations-PASS führen; Rig/Sequenzen sind binär identisch zur Vorversion, Ursache noch zu verfolgen.

### Nachzug 2026-09-10 nach dem Playtest 18:58 — lokale Tests, keine Inhaber-Abnahme

- Live-Team: `TeamSelect_Show` löscht die gemerkte Klasse nicht mehr. Eigene Vorschau wird über den lokalen Roster-Teamstatus und die bekannte Klasse bestimmt; Erstjoin/andere Seite bleiben zufällig. Keine Yaw-/FOV-Änderung.
- GL-Renderer: Preview setzt `tr.fFlipViewModel` **vor** `R_StudioSetUpTransform` zurück. Vorher konnte die Spiegelung des zuletzt gezeichneten Ego-Modells die erste Menüfigur spiegeln; das Zurücksetzen in DrawPoints kam zu spät. Kein weiterer Dummy-/Yaw-Patch.
- HUD: `hud_fastswitch=1` schaltet jetzt auch in Mehrfach-Slots direkt weiter (insbesondere Slot 4). Vorher fraß die Auswahl den ersten M1-Druck als Bestätigung. Bereits ausgerüstete Auswahl verbraucht ebenfalls keinen Angriff. `hud_fastswitch=0` behält die Bestätigung.
- Builds Engine/Client/Menu erfolgreich. Isolierte native Captures: Elite Crew (`joinclass 2`) sichtbar in Buy und Live-Team, T nicht mehr gespiegelt; SAS (`joinclass 3`) im CT-Live-Team. Inc nach HE+Inc-Kauf, zwei Slot-4-Drucken und genau einem Angriff: Pin-Animation, Wurf und sichtbares Feuer im Bild.
- Inc-Unsichtbarkeit im isolierten Test **nicht reproduziert**, auch schon vor dem neuen Patch war Feuer sichtbar. Kein behaupteter Inferno-Fix; zusätzliche Developer-Diagnose meldet Start-Event, fehlende Sprites und vollen Tempentity-Pool. Zahlen/Assets unverändert.
- Auch T mit HE+Molotov und zwei Slot-4-Drucken geprüft: erster Angriff startet die Animation, Loslassen erzeugt sichtbaren Bodenbrand (`CSRETRO_INFERNO_EVENT start weapon=32`). Elite Crew bleibt beim anschließenden Live-Team erhalten.
- Belege: `build/fire-shots/`, `build/fire-shots/ct-slot/`, `build/fire-shots/t-slot/`; Logs unter `build/run-gate/fire-ct-slot/` und `build/run-gate/fire-t-slot/`. Test `scripts/fire-grenade-gate.sh` meldet jetzt nur **CAPTURED**, keinen visuellen PASS. Persönliche `build/run`-Konfiguration und Playtest-18:58-Log nicht geändert.
- Weiter offen: Inhaber-Recheck der unten historisch dokumentierten FAILs, Inc-Ausfall unter dessen Spielbedingungen, Konsole/HUD, Animations-/Modellfeinschliff. Keine neuen Upstream- oder RAR-Assets importiert.

| Feld | Wert |
|------|------|
| Datum | 2026-09-10 |
| Phase | **3A/3B/3C abgenommen.** **3M in Arbeit** (Menü-Lib + Team/Class/Buy/Radio/Pause/Spectator/Scoreboard-VGUI da). 3D/FOV erst nach 3M. |
| Körper-Quelle | **A1** — Manifest in `ROLLEN.md` |
| GameDLL | `server/game/` — Pin `b088984`, Target `csretro_gamedll` |
| Stapel | Xash → Export → A1-Body → (später) NextClient-Funktionen |
| Plattform | **64-Bit only** — `docs/PLATTFORMEN.md` |
| GitHub | https://github.com/benjarogit/csretro (**privat**) |
| Branch | `main` |
| Letztes Release | `v0.1.5` — Desktop-Menüs + Game-Data. Phase 3 ist **kein** Abschluss-Release |
| Lokaler Worktree | `/home/benny/Dokumente/csretro` |

## Rollen (Kurz)

| | Rolle |
|---|--------|
| CS Retro | Produkt-Worktree |
| Xash3D-FWGS | Engine (`engine/`) |
| NextClient | funktionales Zielverhalten |
| Ref A | Client-Body-Quelle (A1-Manifest) |
| Ref B | Menü-/VGUI-Referenz bereits in Phase 3M; gezielte zusätzliche Feature-Ports später |
| Server | GameDLL `server/game/` — `docs/SERVER.md` |
| Bots | Ziel `bots/` (leer). ZBot **in** der GameDLL (Migration) |

```
Xash3D-FWGS → Export → Body (A1) → NextClient-Funktionen
Xash3D-FWGS → GameDLL (inkl. ZBot, Migration) → optional Module → später bots/
```

Eine Client-Lib, eine GameDLL. Kein „cs16-client weiterentwickeln“.

## Pfade

| Was | Wo |
|-----|-----|
| Worktree | `/home/benny/Dokumente/csretro` |
| Engine | `engine/` |
| NextClient-Herkunft | `client/nextclient/` |
| Export | `client/export/` (`GetClientAPI`) |
| Body | `client/body/` |
| Menü | `client/menu/` — CS-Retro-Menü-Lib (`GetMenuAPI` + `GameMenuExports001`) |
| Server-AMXX-Herkunft | `server/` |
| GameDLL | `server/game/` — `MANIFEST.md`, `UPSTREAM_PIN` |
| Bots | `bots/` (leer; ZBot liegt in `server/game/`) |
| Ref A / B | `refs/a-cs16-client/` · `refs/b-cs16-goldsrc/` |
| TEMP_EXTRA | lokal `TEMP_EXTRA/` — Source SDK 2013 (`hl2_src`) + CS:GO cstrike15 (`cstrike15_src`); **nicht im Git**, nur Abguck (`docs/UPSTREAM.md`) |
| Spielinhalte | `gamedata/` (nicht im Git) |
| Build | `build/` (nicht im Git) |

## GitHub

Remote: `https://github.com/benjarogit/csretro.git`. Privat.

**Nie pushen:** `CLAUDE.md`, `AGENTS.md`, `.cursor/`, `.claude/`, `gamedata/`, `valve/`, `cstrike/`, Builds, Steam-DLLs, WoltLab-Pakete.

**Immer:** diese Tabelle, `CHANGELOG.md`, bei Vendor `UPSTREAM.md`.

Abschluss-Release nur wenn die Phase wirklich fertig ist. Zwischenstand darf auf `main` liegen ohne Phase-3-Tag.

## Runtime (dieser Rechner)

- CachyOS x86_64 · Clang 22.1.8
- CMake 4.4.3: `CMAKE_ROOT=/usr/share/cmake` (sonst `--preset`/`-S` bricht)
- Engine: `./scripts/build-engine.sh` → `build/engine/`
- Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`
- GameDLL: `./scripts/build-gamedll.sh` → `build/gamedll-cmake/cs_amd64.so`
- Sanitizer: `./scripts/build-gamedll.sh --sanitize` → `build/gamedll-sanitize/cs_amd64.so`
- Menü-Sanitizer: `./scripts/build-menu.sh --sanitize` → `build/menu-sanitize/menu/menu_amd64.so`
- Testdaten: `XASH3D_RODIR` = `gamedata/` (Bootstrap, **read-only**). Play-UserConfig: `XASH3D_BASEDIR` = `build/run/`. Gates: `build/run-gate/`. 3C: `build/run-3c/`.
- Steam-HL nie als RODIR. Erkennung: `python3 ./scripts/bootstrap-gamedata.py --print-steam`
- ZBot-Testdaten im Game-Data-Baum (`BotProfile.db`, `de_dust.nav`), nicht in Steam
- Listen-`+map`: `.rc` mit `stuffcmds` in Game-Data und BASEDIR
- 3C/Menü-Tests: headless über `gamescope --backend headless` (kein Fokusdiebstahl). Sichtbar: `CSRETRO_FOREGROUND=1 ./scripts/interactive-3c.sh`
- Menü: Endziel eine Lib (`client/menu/`, `docs/MENUS.md`, `docs/PHASE3M.md`). 3C-Baseline (`v0.1.5`): Xash-MainUI + `ShowMenu`. 3M ersetzt das als Primär-UI, `ShowMenu` bleibt Legacy.
- UI-Schrift: **Noto Sans** (OFL-1.1) liegt im Repo unter `data/ui-overrides/platform/resource/csretro_fonts/` und landet per Bootstrap in `gamedata/`. Keine System-Fonts nötig; Steam-`linux_fonts` wird bewusst entfernt.
- UI-Hintergrund: **CS-Retro-PNG** `data/ui-overrides/cstrike/resource/background/csretro.png` (Bootstrap → `gamedata/cstrike/resource/background/`). Steam-Menükacheln werden nicht importiert.
- `cstrike/liblist.gam` ist CS-Retro-owned (Branding + `dlls/cs.so` → Xash `cs_amd64.so`).
- Steam-`dlls/cs_amd64.so` nicht laden. Ohne `-dll`/`-clientlib` findet Xash die Libraries über `liblist` (`dlls/cs.so` → `cs_amd64.so`, `cl_dlls/client_amd64.so`), sofern sie in BASEDIR oder Game-Data liegen. Tests dürfen die Flags weiter nutzen.

## Quickstart

```bash
git clone git@github.com:benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++
./scripts/build-engine.sh
./scripts/build-client.sh
./scripts/build-menu.sh
./scripts/build-gamedll.sh
python3 ./scripts/bootstrap-gamedata.py
./scripts/smoke-gamedll.sh dedicated
./scripts/smoke-gamedll.sh listen
./scripts/interactive-3c.sh
./scripts/interactive-menus.sh   # Team/Buy/Radio ohne Auto-Join, ohne touch/*.cfg
./scripts/vgui-v1-poc-runtime.sh # V1-PoC Auto-Test (CSRETRO_V1POC)
./scripts/vgui-mainmenu-gate.sh   # Hauptmenü-VGUI2: Items, Localization, Layout, Shot
./scripts/vgui-creategame-gate.sh # Create Game: Map/Bots + settings.scr
./scripts/vgui-serverbrowser-gate.sh # Server Browser: LAN-Liste, Filter, ESC
./scripts/vgui-console-gate.sh    # frei belegbare VGUI2-Konsole: Eingabe + Persistenz
./scripts/vgui-teamselect-gate.sh # In-Game Team-Wahl VGUI2: Slots, Loc, ESC, jointeam 2
./scripts/vgui-classselect-gate.sh # Class-Wahl + echter Live-Teamwechsel: chooseteam, CT-Klasse, Menüs zu, Respawn
./scripts/vgui-buy-gate.sh        # In-Game Buy VGUI2: gemeinsames Raster, direkter glock-Kauf, ESC, quit
./scripts/vgui-radio-gate.sh      # In-Game Radio VGUI2: radio1/2/3, coverme, ESC, roger, quit
./scripts/vgui-pause-gate.sh      # In-Game Escape: GameUI-Pause, Resume/Disconnect, kein Wallpaper
./scripts/vgui-spec-gate.sh       # Spectator: Team → 6, dunkle Rahmen, Scores/Timer, quit
./scripts/vgui-score-gate.sh      # Scoreboard: Team → T → Class → TAB-Tafel, quit
./scripts/play.sh                 # manuelles Fenster; Log: build/run/play.log → logs/play-<stamp>.log, Symlink play-latest.log. Nach Playtest lesen; ältere play-*.log löschen
```

Inhalte: nur `gamedata/` (`docs/GAMEDATA.md`). Client: `-clientlib`. GameDLL: `-dll`.

## Offene Arbeit

**Katalog-Ausnahme:** Native **Molotov** (T, $400) und **Incendiary** (CT, $500). IDs 32/33, Werte: `docs/INFERNO.md`. Inferno-Zahlen sind CS2-Stand — nicht neu bauen. NVG/Schild amputiert. Economy-MR15 und restliche CS2-Waffen nicht anfassen.

**Utility nach 3M:** Zielbild `docs/UTILITY.md`. Kein HE/Flash/Smoke-Code in 3M. Wurf-Layer und Inferno nicht anfassen.

**Playtest 2026-09-10 18:58 (Inhaber) — FAIL.** Session `build/run/logs/play-20260910-185842.log`. Client/GameDLL 09:49 (Pin-/Feuer-Nachzug) waren in dieser Session. Vorherige „erledigt“-Zusagen (Live-Team-Orientierung, erster `+attack`, sichtbares Inferno) sind **falsch**. Code-Patches liegen, Verhalten im Spiel nicht.

| Thema | Code behauptet | Im Spiel 18:58 |
|---|---|---|
| Erstjoin Team/Class | Showcase + Class-Lineup | **OK** (Figuren evtl. etwas tief im Kreis — niedrige Prio) |
| Live-`chooseteam` | Dummy-Studio, kein Live-Gait, yaw 150/202 | **FAIL:** andere Klasse als gewählt (Log: Zufall `arctic`, nicht `leet`); T weiter falschrum/von hinten. Yaw im Log unverändert 150 — nicht der Menü-Yaw, sondern Draw/Live-State |
| Team-Modell = eigene Klasse | Teammenü würfelt `terror/leet/arctic/guerilla` (`RandomizeTeamPreviews`) | Inhaber-Vertrag: **gewählte Klasse** (Elite Crew = `leet`), gleiche Kamera wie Erstjoin |
| Buy-Figur | `joinclass`/`hud` → Log `model=terror src=joinclass` | Inhaber 09:05 und 11:03: **anderes Modell als gewählt** (Phoenix). Modell im Log korrekt, Kamera war Team-T 150 (Rücken). Code: yaw **202**. Recheck `CSRETRO_BUY_CHARACTER ... model=terror ... yaw=202` |
| Granaten M2 / drei Stärken | CS2-Input-Layer | **OK** (Underhand alle fünf) |
| Erster `+attack` / Anzünden | Overlay-Maus + HUD Slot 4 ohne Confirm für alle fünf | **PASS** Inhaber 08:43/08:44, 11:03 Molotov/Inc erster Klick OK. Nicht erneut NextAttack |
| Inferno sichtbar | World-Sprites + `FEV_GLOBAL` | Molotov: kleiner Bodenbrand sichtbar. Inc: **hören + Schaden, kein Bild**. Sprites wurden beim Mapload geladen; kein Tent-Error im Log |
| Inc-Wurf-Arm | Idle-Wait 0,75/0,5 wie HE | Inhaber 11:03: nach dem Wurf kommt der linke Arm nochmal. Fix liegt, Recheck |
| Konsole / HUD | Overlay, TAB-Complete, Close bei Restart | Inhaber 11:03: keine Suggests, HUD kaputt, bleibt nach `sv_restart` offen, Welt steht. Fix liegt (`UI_IsVisible`), Recheck |

**Nächster Agent:** nichts als erledigt markieren ohne Inhaber-Playtest. Erster `+attack` ist Inhaber-PASS 08:43/08:44 und 11:03; nicht erneut NextAttack. **Viewmodels der Fire-Nades nicht ersetzen.** GitHub languagelawyer = nur schon übernommene Tempents, **keine** Feuer-Simulation/CVars. Buy-Figur visuell = `joinclass` bei yaw **202**. Buy-Layout = `buy-menu-idee.png`, noch offen. Yaws der Erstjoin-Abnahme nicht ändern (`150`/`202` Team, Class `158/169/191/202`). **Utility-Zielbild** `docs/UTILITY.md` — HE/Flash/Smoke-Code erst nach 3M; Wurf und Inferno nicht anfassen.

**Inhaber-Klarstellung 2026-09-05:** 3M umfasst Funktion **und** den vollständigen visuellen Feinschliff. In-Game-Menüs (Radio, Team, Class, Buy, Scoreboard, Spectator) sollen **1:1** zu den Referenzen sein, soweit CS 1.6 / Xash Daten und Assets hergeben; fehlendes Machbares wird nachgezogen, nicht wegerklärt. Nicht erfinden: CS2-Waffen (außer den zwei Fire-Nades oben), Faceit-Radar/Veto/Avatare-Pflicht. Erste Look-Scheiben sind Zwischenstände. Nach PlayerList folgt der Rest des 1:1-Feinschliffs, erst danach weitere Feature-Phasen. Gamma/Brightness manueller Recheck bleibt offen: Testlog 07:32 zeigt nur die Titelseite; der bisherige Pause-Blur verdeckte zusätzlich die Live-Spielwelt. Korrektur: Gamma 1.8–3.0 entsprechend Engine, Video-Tab im Spiel zeigt die echte Szene ohne zwischengespeicherten Blur, Titelseite erklärt die Map-Vorschau. Nach Verlassen des Video-Tabs gilt wieder der normale Pause-Blur.

1. **Phase 3M** — Mouse/Audio/Video **PASS / Regression** (Brightness/Gamma manueller Recheck offen — `docs/PHASE3M-VIDEO.md`). Keyboard **AUTOMATED PASS / MANUAL RECHECK OPEN**. Adaptive Layout **AUTOMATED PASS / MANUAL ACCEPTANCE OPEN**. Desktop-UI-Optik (Options, Hauptmenü, Create Game, LAN-Browser) ist seit **2026-09-04 visuell abgenommen**; **Create Server / Options / Find Servers** brauchen eine kurze Nachkontrolle der weniger transparenten `ChromeGlass`-Frames (Alpha 214/200) über dem Titelseiten-Logo. Konsole: **AUTOMATED PASS / MANUAL VISUAL CHECK OPEN**. **Team-Wahl Erstjoin: AUTOMATED PASS + Inhaber 2026-09-07 — Layout/Farben der Erstjoin-Fläche nicht anfassen.** Live-`chooseteam` 2026-09-10 18:58 weiter FAIL (Klasse + Orientierung). Yaws/Seqs der Erstjoin-Abnahme unverändert. Yaws/Seqs der Erstjoin-Abnahme unverändert (`150`/`202` Team, Class-Lineup `158/169/191/202`). Nur die T-Emblem-Stern-Rasterung (Coverage statt Integer-Scanlines) ist neu; Kanten ohne Treppchen manuell prüfen, CT-Seite unverändert. **Class-Wahl: Inhaber visuell bestätigt 2026-09-07.** **Tactical Shield und Nightvision sind amputiert.** **Buy: fullbright `w_*.mdl` mit Skin (Chrome aus, keine Gold-Silhouette), modellabhängige Roll-/Framing-Korrekturen. Figur rechts = `joinclass`, HUD nur wenn Remember leer. Class-T: 1 Phoenix, 2 Elite Crew, 3 Arctic, 4 Guerrilla — Mesh unter demselben Label.** Beim Teamwechsel wird das Buy-Panel neu erzeugt, damit Katalog und Klassenmodell nicht von der alten Seite hängen bleiben. Spielerzahlen stehen über dem Modell-Viewport. GameUI-Flächen dichter (lesbare Texte). Manuelle visuelle Abnahme offen.** `TEMP_EXTRA/cstrike15_src`/`hl2_src` wurden für Zustands-/Lifecycle-Verträge ausgewertet; kein Source-/Scaleform-Import. Die zentrierte 1280×720-Bühne wächst auf großen Auflösungen nicht weiter und nutzt kleine 4:3-Viewports breiter. `cstrike/gfx/vgui` bleibt entfernt. Radio: AUTOMATED PASS + drei getrennte Menüs im Spiel bestätigt. Spectator/Scoreboard: Feinschliff offen. PlayerList bleibt Stub. **Kein Phase-3-Tag. FOV/3D gesperrt.**
2. Renderer-Multi wenn Extended API.
3. Windows/macOS Compile-Gates. **`Csretro_PlatformShellOpen` Windows = offenes Plattform-Gate**.
4. Zwei Zielvorgaben quer über die UI (`docs/MENUS.md`, Abschnitt Endziel): **alles Relevante im Menü** — keine Einstellung, für die man eine `.cfg` von Hand editieren muss — und **gute Voreinstellungen ab Werk** für Grafik, Server und Bots. Die Auswahl, was relevant ist, hängt künftig am **CVar-Katalog** (Server-/Client-/Bot-CVars und Registers), nicht an einer einzelnen `.scr`. Beim Create-Game-Dialog heißt das später eigene Bot-Registerkarten (Auswahl, Anzahl, Team, Waffenfreigaben, `docs/BOTS.md`).
5. Bot-Grenze → `bots/`. Produktiv bleibt der ReGameDLL-ZBot unverändert; Zielbild „CS Retro Bot“ (eigener Bot aus den besten Mechaniken bekannter GoldSrc-Bots, Auto-Nav/Waypoints, Bot-Konfiguration beim Servererstellen, In-Game-Bot-Menü) steht in `docs/BOTS.md` — **Planung, nicht in Arbeit**. Feature-Auswahl dort ausdrücklich interaktiv.
6. Geplante Phasen nach 3M: **Phase 5 Installationslayout** (aufgeräumter Installationsordner, README je Ordner, Pfadtreue als harte Bedingung), **Phase 6 Mehrsprachigkeit** (ein Sprachordner und ein Leser für alles, UTF-8, EN Default + DE, Community kann Sprachen nachlegen), **Phase 7 Natives Plugin-System** (Plugin-Idee von AMXX behalten, AMXX als eigene Ebene auflösen) und **Phase 8 Faceit-Anbindung** (nur wenn kostenlos; Planung, nicht in Arbeit). `docs/PHASEN.md`. **Leitsatz:** alles wird nach und nach nativ; es gibt keinen Fremdcode im Baum, nur Fork — Übergangslösungen sind als Übergang zu kennzeichnen.
7. **Zielbild (nicht 3M):** Dedicated Server vom laufenden Client starten — gleiche `ServerProfile`-Konfiguration wie Create Game, kein Extra-Tool. `docs/SERVER.md`, `docs/MENUS.md`.

## Shield-Schnitt (2026-09-08)

**Amputiert:** Kein Schild-Besitz, keine Schild-Zweige in Waffen/Messer/Granaten/C4, keine Bot-Schild-Taktik, kein Buy/Rebuy/Autobuy, kein Pickup, kein HUD-Flag `PLAYER_HOLDING_SHIELD`, kein Schild-Schaden. `HasShield`/`IsProtectedByShield`/`m_bOwnsShield`/`m_bShieldDrawn` sind weg.

**Map-Ignore:** `LINK_ENTITY_TO_CLASS(weapon_shield)` bleibt, damit Karten mit der Entity nicht `NULL Ent` werden. `CWShield::Spawn` entfernt die Entity sofort. `ARMOURY_SHIELD=19` gibt nichts; die Nummer wird nicht verdichtet.

**Nur ABI:** `CCSPlayer::{Give,Drop,Remove}Shield` und die ReGame-Hooks `CBasePlayer::{Give,Drop}Shield` sind deprecated No-Ops. Die VTable-Slots dürfen nicht entfernt werden, solange binäre ReGame-API-Kompatibilität gilt. `WEAPON_SHIELDGUN=99` und `ITEM_SHIELDGUN=0` bleiben unbenutzte Löcher.

**Nicht enthalten:** Economy-Modernisierung. Externe Plugins/Demos, die ein echtes Schild brauchen, funktionieren nicht. Normale Karten ohne Schild bleiben spielbar.

**Nächster Schritt:** voller Restart `./scripts/play.sh`. Buy: Map hinter dem Raster sichtbar (kein Blur), Zellen auf der Welt, keine schwarze Platte, kein `v40/…` unten rechts. Klasse nicht anfassen. Waffen bleiben echte `w_*.mdl` (kein Gelb). Smoke/Landung/Feuer nicht anfassen.

## Nicht anfassen

- 3D / NextClient-Feature-Port (FOV, View, Camera, Inspect) vor Abschluss von 3M bzw. ohne ausdrückliche Freigabe
- Ref A außerhalb des A1-Manifests
- Ref-A-ReGameDLL / YaPB / Ref-A-mainui
- Ref-B-Vollport als zweiten Menüstapel; Ref B bleibt VGUI-Referenz für 3M (`docs/MENUS.md`)
- Steam-`vgui2.dll`/`vgui2.so` als Runtime
- AMXX/Metamod in die GameDLL backen
- ZBot vor Funktionsübernahme löschen
- 32-Bit-Targets, Steam-Bind, `git submodule add`
- Steam-Installation beschreiben oder überschreiben
- `CLAUDE.md` / Cursor-Attribution
- `TEMP_EXTRA/` vendorn oder in `client/`/`refs/` kopieren — nur Abguck (`docs/UPSTREAM.md`)
