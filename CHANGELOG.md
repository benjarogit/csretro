# Changelog

Jede Version hier = ein GitHub-Release auf `benjarogit/csretro` (privat).
Der verbindliche Projektstand steht in `docs/HANDOFF.md`.

## Unreleased — Phase 3M (VGUI2 / Desktop-UI)

Kein Phase-3-Tag. Kein FOV. 3C-Baseline (`v0.1.5`) bleibt gültig.

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
- **Global VGUI2 Visual Polish** OPEN (nach Adaptive Layout). Classic 5971 + moderne Desktop-Darstellung.
- **VGUI2 Symbol-Control-Gate grün:** `vgui_symbols.cpp` — Marlett geometrisch; kein Windows-Marlett.ttf; Scheme-lastResort überschreibt Symbolfonts nicht.
- **Metrics-/Classic-Gate:** Preferred Size **512×406**. Mouse+Audio+Video Gates. FOV gesperrt.
- **Windows ShellOpen:** offenes Plattform-Gate (No-Op).
- Eine Menü-Lib `client/menu/` (`GetMenuAPI` + `GameMenuExports001`).
- Font-Resolver: GameData `platform/resource/linux_fonts` (+ System-Fallback-Verzeichnisse); Liberation vor DejaVu (Kandidat, nicht Endurteil).
- Interim-Hauptmenü/Create/Team: Bootstrap — nicht visuell abgenommen.
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
