# Phasen

Vor jeder Phase die Rollen in `docs/ROLLEN.md` bestätigen. Nicht mischen.

## Phase 0 — Rollen + Vendor (abgeschlossen)

Vier Bereiche + eingefrorene Refs. CMake orchestriert; Engine über Waf. NextClient-MSVC/vcpkg ist nicht der Produkt-Build.

## Phase 1 — Bindungs-Analyse (abgeschlossen)

NextClient ist Overlay auf Steam-`client.dll`, kein `GetClientAPI`-Körper. Xash verlangt `GetClientAPI` + `Initialize(&gEngfuncs)`. Form: eine Client-Lib = Export + Körper + NextClient-Module. Körper-Quelle **A1**. Menü = zweiter Ladeweg (`GetMenuAPI`).

## Phase 2 — Steam-/Hook-Schnitt (abgeschlossen)

Entfernt: `steam_api_proxy/`, 8684-Address-Provider, Steam-Master `hl1master`, `MatchmakingSteamComp`, Launcher-`steam_api.dll`. Feature-Quellen (`client_mini`, `engine_mini` NCLM/HTTP-Master, GameUI) bleiben als Port-Quelle. Exportvertrag unter `client/export/`.

## Gate A1 (2026-09-01)

Ref-A-Allowlist als Client-Body. NextClient bleibt Zielbasis. Manifest: `docs/ROLLEN.md`.

## Phase 3 — Minimal lauffähig (offen)

Arbeitsdokument: `docs/PHASE3-BODY.md`. Plattform: `docs/PLATTFORMEN.md`. Server: `docs/SERVER.md`.

| Teil | Status |
|------|--------|
| 3A A1-Body vendorn | abgenommen |
| 3B nackte Client-Lib unter Xash | abgenommen |
| 3C In-Game-Baseline | **abgenommen** — Listen interaktiv `de_dust` (Team/Spawn/Movement/Waffen/Round/Shutdown) |
| GameDLL-Gate | vendort `server/game/` Pin `b088984`, Target `csretro_gamedll` |
| **3M VGUI2 / Desktop-UI** | **in Arbeit** — Mouse/Audio/Video **PASS / Regression**; Keyboard **AUTOMATED PASS / MANUAL RECHECK OPEN**; Adaptive Layout / Resize **AUTOMATED PASS / MANUAL OPEN**; bestehende Desktop-UI-Optik **vom Inhaber abgenommen (2026-09-04)**; VGUI2-Konsole **AUTOMATED PASS / MANUAL CHECK OPEN**; Team **AUTOMATED + Inhaber visuell bestätigt**; Class **AUTOMATED + Inhaber-Check Namen ok** (Class_Info-Fix); Buy **AUTOMATED PASS** (Hauptseite + Pistolen/`glock`; restliche Unterseiten manueller Check). FOV gesperrt. `docs/PHASE3M.md`, `docs/PHASE3M-KEYBOARD.md`, `docs/PHASE3M-LAYOUT.md` |
| 3D NextClient-Features | **nach 3M** — erstes Feature FOV, nur nach Freigabe. Danach u. a. individuelles Crosshair (NextClient-Basis, feiner als Presets) und Radar-Minimap (Karte im Radar; Abguck: MetaHook/GameBanana, `TEMP_EXTRA/hl2_src` CS:S-HUD, `TEMP_EXTRA/cstrike15_src` CS:GO-Scaleform; NextClient hat keins; nichts vendorn). `docs/PHASE3M.md`, `docs/UPSTREAM.md` |

3C vollständig heißt: Listen-Server, Map, Rendering, Input, Movement, Prediction, Vanilla-HUD, Waffen, Connect, Shutdown — alles auf derselben 64-Bit-Architektur. 3C wird für 3M **nicht** neu aufgerollt. `ShowMenu` bleibt Legacy-Kompatibilität.

## Phase 4 — Gezielte Ports

Ref B ist **bereits in Phase 3M** Menü-/VGUI-Referenz (In-Game-Verhalten / `.res`-Mapping). Phase 4 ist nur für gezielte zusätzliche Feature-Ports, die nach 3M übrig bleiben — nicht der erste Ref-B-Einstieg. NextClient-GameUI-Ports laufen in **3M**, nicht als zweiter Stapel.

## Phase 5 — Installationslayout (geplant)

Ziel: Der Installationsordner ist aufgeräumt und selbsterklärend. Der Nutzer findet seine
Configs und Einstellmöglichkeiten, ohne die GoldSrc-Historie zu kennen.

- Ordnerstruktur sortieren; Nutzerdaten (Configs, Screenshots, Demos, Logs) klar von
  Spieldaten und Binaries trennen.
- Pro Ordner eine kurze `README`: wo bin ich, wofür ist das, darf ich hier etwas ändern
  (ja / auf eigene Gefahr / nein, wird überschrieben).
- **Harte Randbedingung:** GoldSrc-Pfade sind an vielen Stellen fest verdrahtet — Engine,
  GameDLL, `.res`, WAD-/Map-Referenzen, `liblist.gam`, Suchpfade der Filesystem-Schicht.
  Umsortieren ohne vollständige Pfadprüfung bricht das Spiel. Jede Verschiebung braucht
  einen Nachweis, dass alle Konsumenten sie mitgehen; im Zweifel Kompatibilitätspfad
  statt Verschiebung.
- Berührt `scripts/bootstrap-gamedata.py` (`docs/GAMEDATA.md`): das Layout entsteht beim
  Import, nicht durch nachträgliches Verschieben von Hand.

## Phase 6 — Mehrsprachigkeit (geplant)

Ziel: **Ein** einheitliches Sprachsystem für ganz CS Retro — Menü, HUD, Server und Plugins.
Auslieferung mit Englisch (Default) und Deutsch.

- **Ein** Sprachordner statt verstreuter Dateien. Heutiger Ist-Stand: 32 Localization-Dateien
  in `cstrike/resource/`, `valve/resource/` und `platform/resource/`, alle **UTF-16LE**,
  benannt `<domain>_<sprache>.txt` — plus AMXX mit eigenem Ordner und eigenem Format.
- Community soll eine Sprache **hinzufügen** können, indem sie eine Datei ablegt
  (z. B. `fr.lang`), ohne Build und ohne Eingriff in bestehende Dateien.
- **Format ist offen** und wird vor der Umsetzung entschieden.
- **Es gibt keinen Fremdcode.** Alles im Baum ist Fork und darf umgeschrieben, verdrahtet
  und verschmolzen werden — auch der Leser in AMX Mod X. Ein bestehendes Format ist ein
  Ist-Zustand, keine Randbedingung: wenn ein Leser das Zielformat nicht versteht, wird der
  Leser angepasst, nicht das Zielformat verbogen.
- **Generierung nur als Übergang, nicht als Ziel.** Solange ein Konsument noch sein altes
  Format liest, darf beim Import daraus erzeugt werden. Der Zielzustand ist ein Leser für
  alle: eine Quelle, ein Format, kein Konvertierungsschritt. Ein dauerhafter Generator wäre
  genau die Extraschicht, die hier nicht bleiben soll.
- Die Kodierungsfalle UTF-16LE (`docs/MENUS.md`) fällt damit weg — das Zielformat ist UTF-8.

## Phase 7 — Natives Plugin-System (geplant)

Ziel: Plugins laufen **direkt** gegen CS Retro. Keine Zwischenschicht, die Aufrufe
übersetzt — das ist die Schicht, die Leistung kostet und den Baum zweiteilig hält.

- Behalten wird die **Idee** von AMX Mod X: Dritte können Plugins liefern, ohne den
  Kern zu bauen. Nicht behalten wird AMXX als eigenständige Ebene daneben.
- Richtung: schrittweise nativ. Was heute über AMXX läuft, wandert in eine CS-Retro-eigene
  Plugin-Schnittstelle; AMXX löst sich dabei auf, statt parallel weiterzulaufen.
- Das ist ausdrücklich **kein** Vendoring-Verhältnis: AMXX-Code im Baum ist Fork und darf
  umgeschrieben, umbenannt und verschmolzen werden wie jeder andere Teil.
- Berührt den `Modules`-Zweig des Serverprofils (`docs/MENUS.md`) und Phase 6: ein
  Plugin-Leser, ein Sprachsystem, kein zweiter Stapel.

**Leitsatz für 5–7:** Alles wird nach und nach nativ. Am Ende steht CS Retro, nicht
CS Retro plus drei fremde Ebenen. Übergangslösungen sind erlaubt, aber als Übergang zu
kennzeichnen — nichts davon ist ein Zielzustand.

Reihenfolge von 5, 6 und 7 ist nicht festgelegt; alle setzen einen stabilen 3M-Stand voraus.

## Phase 8 — Faceit-Anbindung (geplant)

Später: CS Retro über die Faceit-API anbinden — **nur wenn das nichts kostet**.

- **Kostenlos-Vorbehalt:** API-Keys / Data API ohne Pflicht-Bezahlplan für unser Vorhaben.
  Vor Umsetzung klären, nicht annehmen.
- Faceit ist eine **Anbindung**, kein Ersatz für das eigene Netz und keine
  Steam-Kompatibilität durch die Hintertür. Eigenes Protokoll bleibt.
- Status: **Planung, nicht in Arbeit.** Kein Code, kein Vendor, keine Keys ins Repo.

Quellen (nur verlinken):

- https://docs.faceit.com/
- https://docs.faceit.com/docs/data-api/
- https://docs.faceit.com/getting-started/authentication/api-keys/
- https://docs.faceit.com/docs/data-api/data/#tag/Championships/operation/getChampionships
