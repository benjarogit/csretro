# CS Retro — Handoff

Anderen Rechner arbeitsfähig machen. Diese Datei ist der **lebende Stand**.
Nach substantieller Arbeit Tabelle und „Offene Arbeit“ in derselben Session nachziehen.

Details: `ROLLEN.md`, `PLATTFORMEN.md`, `SERVER.md`, `UPSTREAM.md`, `LIZENZEN.md`, `SCHNITTSTELLEN.md`, `MENUS.md`, `GAMEDATA.md`, `PHASEN.md`, `PHASE3-BODY.md` (nur während Phase 3), `PHASE3M.md`, `PHASE3M-KEYBOARD.md`, `PHASE3M-LAYOUT.md`, `PHASE3M-VIDEO.md`, `PHASE3M-METRICS-DIAGNOSIS.md`, `CHANGELOG.md`. Danksagung: `CREDITS.md`.

## Aktueller Stand

| Feld | Wert |
|------|------|
| Datum | 2026-09-04 |
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
./scripts/vgui-classselect-gate.sh # In-Game Class-Wahl VGUI2: TER+CT, Loc, Class_Info leer, ESC, joinclass 1
./scripts/vgui-buy-gate.sh        # In-Game Buy VGUI2: Kategorien, Pistolen, glock, ESC, quit
./scripts/vgui-radio-gate.sh      # In-Game Radio VGUI2: radio1/2/3, coverme, ESC, roger, quit
./scripts/vgui-pause-gate.sh      # In-Game Escape: GameUI-Pause, Resume/Disconnect, kein Wallpaper
./scripts/vgui-spec-gate.sh       # Spectator: Team → 6, dunkle Rahmen, Scores/Timer, quit
./scripts/vgui-score-gate.sh      # Scoreboard: Team → T → Class → TAB-Tafel, quit
./scripts/play.sh                 # manuelles Fenster; Log: build/run/play.log + logs/play-latest.log
```

Inhalte: nur `gamedata/` (`docs/GAMEDATA.md`). Client: `-clientlib`. GameDLL: `-dll`.

## Offene Arbeit

1. **Phase 3M** — Mouse/Audio/Video **PASS / Regression** (Brightness/Gamma: CVars ja, **keine sichtbare Wirkung**, offen — `docs/PHASE3M-VIDEO.md`). Keyboard **AUTOMATED PASS / MANUAL RECHECK OPEN**. Adaptive Layout **AUTOMATED PASS / MANUAL ACCEPTANCE OPEN**. Die aktuelle Desktop-UI-Optik (Options, Hauptmenü, Create Game, LAN-Browser) ist seit **2026-09-04 vom Inhaber visuell abgenommen**; keine erneute Grundsatzgestaltung ohne neuen Befund. Die neue frei belegbare VGUI2-Konsole ist **AUTOMATED PASS / MANUAL VISUAL CHECK OPEN** (`vgui-console-gate.sh`). **Team-Wahl: AUTOMATED PASS + Inhaber visuell bestätigt** (Namen/Auswahl). **Class-Wahl: AUTOMATED + Inhaber-Check Namen ok**; `#Cstrike_Class_Info` wird nicht mehr roh gezeigt (Token fehlt in Steam-`cstrike_english`, Label bleibt leer). **Buy: AUTOMATED PASS + Inhaber Grundfunktion abgenommen** (öffnet, Kategorien, Waffen kaufen, Autobuy/Rebuy, Tastatur schließt nach Kauf). Keine Slot-für-Slot-Nacharbeit jetzt. **Radio: AUTOMATED PASS + Inhaber 2026-09-04 Grundfunktion abgenommen** (öffnet, Texte, Aliase, Bewegung bleibt). Gate: `./scripts/vgui-radio-gate.sh`. Team/Class/Buy-Look erste Scheibe: **Inhaber 2026-09-04 Funktion abgenommen**, Optik-Feinschliff später. **Anreize (kein 1:1):** Radio = dunkle nummerierte Listen; Team = moderne Split-T/CT-Fläche; Buy = Kategorie-Raster; Spectator = dunkler Rahmen, Daten am Rand (kein Faceit-1:1). **Pause: AUTOMATED PASS + Inhaber 2026-09-04 Grundfunktion und Optik abgenommen** (Blur + „Pausiert“). Play-Log 13:30: `blur=1 capture=160x90`, Buy `usp`, Radio `needbackup`/`fallback`. PlayerList bleibt Stub (`menu_playerlist`). Gate: `./scripts/vgui-pause-gate.sh`. **Spectator: AUTOMATED PASS + Inhaber Grundfunktion abgenommen**; erste Look-Scheibe da (`HudFrameLook` Rahmen). Gate: `./scripts/vgui-spec-gate.sh`. **Scoreboard: AUTOMATED PASS + Inhaber Grundfunktion abgenommen**; erste Look-Scheibe da (Tafel, T/CT-Kopf). Gate: `./scripts/vgui-score-gate.sh`. **Team/Class/Buy-Look: Funktion abgenommen, Feinschliff später.** Spectator-/Scoreboard-Look **Inhaber-Check offen.** **Play-Log (bekannt, nicht jetzt):** IPv6-Hostname, `alpha_sky`/`solid_sky`, `tutor_enable`, `sprites/hud.txt` 215≠190. Overlay-Dim ohne Buttons war geclipptes 64×24-Panel. Autobuy bei $800 = Munition/Weste, Gewehre erst mit mehr Geld. **Schuss-Hänger: bestätigt weg.** **800corner-Logflut: weg** (nächster Start). **play.sh** schreibt immer `build/run/play.log` und `engine.log`. **Log-Notizen (nicht jetzt):** IPv6-Hostname `cachyos-x8664`; `alpha_sky`/`solid_sky`; `tutor_enable`; `sprites/hud.txt` 215≠190; erster Buy-Frame 64×24 heilt auf fit=1. Runtime-Start/Shutdown ist ohne CS-Retro-Enginewarnungen grün; der umfangreiche historische Compiler-Warnungsbestand ist weiterhin offen und darf nicht als „0 Warnungen“ bezeichnet oder ausgeblendet werden. Internet-Tab bleibt ein eigenes Vorhaben (`docs/SERVER.md`). **Kein Phase-3-Tag. FOV/3D gesperrt.** Radar-Minimap, individuelles Crosshair und MetaHook als Abguck-Quelle sind **Zielbild nach 3M** (`docs/PHASE3M.md`, `docs/UPSTREAM.md`) — kein Bau jetzt. Lokale Extra-Bäume: `TEMP_EXTRA/hl2_src` (Source SDK 2013 / CS:Source) und `TEMP_EXTRA/cstrike15_src` (CS:GO cstrike15) — beobachten, nicht vendorn.
2. Renderer-Multi wenn Extended API.
3. Windows/macOS Compile-Gates. **`Csretro_PlatformShellOpen` Windows = offenes Plattform-Gate**.
4. Zwei Zielvorgaben quer über die UI (`docs/MENUS.md`, Abschnitt Endziel): **alles Relevante im Menü** — keine Einstellung, für die man eine `.cfg` von Hand editieren muss — und **gute Voreinstellungen ab Werk** für Grafik, Server und Bots. Die Auswahl, was relevant ist, hängt künftig am **CVar-Katalog** (Server-/Client-/Bot-CVars und Registers), nicht an einer einzelnen `.scr`. Beim Create-Game-Dialog heißt das später eigene Bot-Registerkarten (Auswahl, Anzahl, Team, Waffenfreigaben, `docs/BOTS.md`).
5. Bot-Grenze → `bots/`. Produktiv bleibt der ReGameDLL-ZBot unverändert; Zielbild „CS Retro Bot“ (eigener Bot aus den besten Mechaniken bekannter GoldSrc-Bots, Auto-Nav/Waypoints, Bot-Konfiguration beim Servererstellen, In-Game-Bot-Menü) steht in `docs/BOTS.md` — **Planung, nicht in Arbeit**. Feature-Auswahl dort ausdrücklich interaktiv.
6. Geplante Phasen nach 3M: **Phase 5 Installationslayout** (aufgeräumter Installationsordner, README je Ordner, Pfadtreue als harte Bedingung), **Phase 6 Mehrsprachigkeit** (ein Sprachordner und ein Leser für alles, UTF-8, EN Default + DE, Community kann Sprachen nachlegen), **Phase 7 Natives Plugin-System** (Plugin-Idee von AMXX behalten, AMXX als eigene Ebene auflösen) und **Phase 8 Faceit-Anbindung** (nur wenn kostenlos; Planung, nicht in Arbeit). `docs/PHASEN.md`. **Leitsatz:** alles wird nach und nach nativ; es gibt keinen Fremdcode im Baum, nur Fork — Übergangslösungen sind als Übergang zu kennzeichnen.
7. **Zielbild (nicht 3M):** Dedicated Server vom laufenden Client starten — gleiche `ServerProfile`-Konfiguration wie Create Game, kein Extra-Tool. `docs/SERVER.md`, `docs/MENUS.md`.

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
