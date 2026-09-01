# Phase 3M — VGUI2 Metrics / Classic Gate — Diagnosebericht

Stand: 2026-09-02. Preferred Size **512×406** implementiert. Mouse+Audio-Gate **PASS** @640–1366. **Video freigegeben** (nächste Subpage). FOV/3D gesperrt.

Ziel des Diagnoseberichts (historisch): gemeinsamen VGUI2-Unterbau korrekt machen — keine kosmetischen `.res`-/Dialog-Patches. Classic Preferred Size und Mouse/Audio sind abgeschlossen; fortlaufende Arbeit: Video-Port.

## Quellenhierarchie (verbindlich)

1. Originales Steam-CS-1.6 (Runtime, Pixel, Build Mode)  
2. Zugehörige Originalressourcen (`.res`, Scheme, Loc)  
3. Vendortes VGUI2 / `vgui_controls`  
4. NextClient (funktionale GameUI)  
5. Valve Developer Community — VGUI/VGUI2-Doku (Konzept; nicht ungeprüft Source-Engine-APIs)  
6. Research: CKF3Alpha, OpenGoldSrc, Ref B, fwgs-vgui2-support, MetaHookSv/VGUI2Extension, Ref A (Desktop)  
7. Ghidra nur bei Restlücke nach 1–6 — und **nur** für den untersuchten Binary-Stand (Current Steam 2024 beantwortet nur Current-Fragen; **keine** Ableitung von 5971-Interna daraus)  

Build Mode Shortcut (Valve-Doku): **Ctrl+Shift+Alt+B**

---

## Golden 5971 vs Current Steam — strikt getrennt

### A) Golden Classic Visual Reference

| Feld | Wert |
|------|------|
| Rolle | visuelle Zieloptik + screenshot-derived pixel metrics; proportional/HD-Interna **unknown** |
| Build | **5971** (`Exe build: 11:45:32 Mar 1 2013` laut Console im Ref-Shot) |
| Sprache / logische Auflösung | English / **1366×768** |
| Artifact | `docs/research/golden-5971/ref-cs16-5971-options-video-1366x768.jpg` |
| SHA-256 | `0f73bd7b45c3980a780c8abfe6f25e779c0893b8935723fff5697199c6f12cfd` |
| Dateiformat | JPEG; gespeicherte Matrix **1024×575** (Downscale der logischen 1366×768-Referenz) — siehe `docs/research/golden-5971/README.md` |
| OptionsDialog rendered | **512×406** — äußere Frame-Bounds im logischen 1366×768-Raum: x=825..1336, y=13..418 |
| Aus Shot belegt | Tab **Mouse**; Video-Tab aktiv; Build Mode auf `OptionsSubVideo.res` / ComboBox `Renderer` |
| Nicht belegt | 5971-Constructor/Proportional-State; Byte-Identität Scheme/Loc/GameUI mit Current Steam |

**Wichtig:** 512×406 ist **Rendered-Pixel-Evidence**, kein Beweis für den internen 5971-Constructor oder HD/prop-State.


### B) Current Steam Resource / BuildMode Reference (dieser Host)

| Feld | Wert |
|------|------|
| Rolle | Live Resources, Binary-Analyse, Build-Mode-Messlauf (**HL25-era/current HD reference**) |
| AppID / appmanifest `buildid` | 10 / **12934623** |
| `cstrike/steam.inf` | `PatchVersion=1.1.2.7` |
| Runtime Console `version` (2026-09-02, 800×600, english) | **Protocol version 48** · **Exe version 1.1.2.7/Stdio (cstrike)** · **Exe build: 01:35:13 Oct 8 2024 (10211)** |
| Sprache | Steam UserConfig **english**; UI-Tab **Aim** (nicht Mouse) |
| Auflösung Messlauf | **800×600** windowed (Shots: `build/steam-bm-shots/`) |
| `valve/cl_dlls/gameui.so` SHA-256 | `6473a660d1f10c0de35c80fd328110266bb5eeb5d1d7eb07697a4867eda2ce0a` |

**Warnung:** Current Steam ≠ Golden 5971. Keine Formulierung „Build 5971 Scheme = aktueller SHA“.

Vor jedem Original-Messlauf erneut: version, Protocol, Exe build/date, Sprache, Auflösung, appmanifest buildid.

---

## Gepinnte Current-Steam-Dateien (SHA-256)

Nur Current-Snapshot — **kein** Beweis für Bytes von Build 5971.

| Logisch (VFS) | Resolved (dieser Host) | SHA-256 | Hinweis |
|---------------|------------------------|---------|---------|
| `resource/TrackerScheme.res` | `gamedata/valve/resource/TrackerScheme.res` (= Steam `valve/...`) | `1915a4740b3f171ca2773f5ffd8b7b0497702475d7298ef3066648727aa8413a` | CS-Retro Runtime-Winner |
| `resource/TrackerScheme.res` (platform) | `gamedata/platform/resource/TrackerScheme.res` | `5593b672cffca50f78bea9103bbd527ef0d2d82639d7c306e5b2a9da7639ba21` | ≠ valve; Fallback |
| `resource/gameui_english.txt` | Steam/gamedata `valve/resource/...` | `aca17e06d49b14d1ae31f14fca77d02a5c0f9d41af27353f5a5362453c39d711` | current: `GameUI_Mouse`=**Aim** |
| CS-Retro Loc-Pin | `data/ui-overrides/cstrike/resource/csretro_gameui_english.txt` | Override | nur Key **Mouse** aus 5971-Visual; **nicht** ganze historische Loc-Datei |
| Steam `OptionsSubMouse.res` | `…/Half-Life/valve/resource/OptionsSubMouse.res` | `d441c5684c3f47f2ae646710c35620da7ff8d5a1030beeaa99d295d31b51779e` | Current Steam |
| CS-Retro Mouse `.res` | `data/ui-overrides/.../OptionsSubMouse.res` | `31540f55e2e0a6ab67bff129f54b23116d655992abb44189c4bc1c21ff8800dd` | NextClient-Layout ≠ Steam |
| Steam `OptionsSubAudio.res` | `…/valve/resource/OptionsSubAudio.res` | `f7616bced2eb9b47c81e5dd7388f1d1666aedde2d24118b76740da0ef90acde8` | Current Steam |
| CS-Retro Audio `.res` | `data/ui-overrides/.../OptionsSubAudio.res` | `6585b66ec0ff63b8429731487fde4741eac227808e1230cc1bf88faaf6d2f205` | kleine Diffs (Label-wide) |

Scheme-Suchreihenfolge CS Retro: `cstrike` → **`valve` (HIT)** → `platform` → UI-Override.

---

## Classic Proportional Base / HD — Quellen getrennt

| | Wert |
|--|------|
| CS-Retro Classic-Gate `GetProportionalBase` | fest **640×480** (keine globale HD-Umschaltung) |
| Kurze Fehlentscheidung (zurückgenommen) | global HD 1280×720 als Classic-Kompensation |
| **Current Steam 2024** | HL25-era/current HD reference: GameUI `SetHDProportional(true)`, Basen **522×444**, TabWidth-Basis **72** |
| MetaHook / Surface | `GetHDProportionalBase` / `SetHDProportionalBase` in `ISurface_HL25` als **HL25-added** dokumentiert (`hzqst/MetaHookSv` `include/Interface/VGUI/ISurface.h`, Pin in `UPSTREAM.md`) |
| MetaHook HiDPI-Modus | bewusst alle Panels proportional — Research only, nicht Classic-Ziel |
| **Golden 5971 proportional/HD behavior** | **currently unknown** — Build ist älter; ob GameUI damals bereits eine frühere HD-Proportional-Form nutzte, ist **nicht** nachgewiesen |
| Verboten | „5971 = definitiv non-HD“ |

Current-Steam-BuildMode = Kontroll-/Forschungsdatensatz, **nicht** automatisch CS-Retro-Classic-Ziel.

---

## Current Steam GameUI — Codewerte (objdump `gameui.so`)

`COptionsDialog::COptionsDialog`:

1. `SetHDProportional(true)`
2. `SetBounds(0, 0, GetProportionalScaledValue(522), GetProportionalScaledValue(444))`
3. `GetPropertySheet()->SetTabWidth(GetProportionalScaledValue(72))`

Das sind **Current-Steam-Codebasen** (HL25-era/current HD reference), keine Golden-5971-Werte und kein Beweis für 5971-Interna.

Klassische Rekonstruktionen (CKF3Alpha / OpenGoldSrc, gepinnte Commits in `UPSTREAM.md`): hart `512×406` + `SetTabWidth(84)` ohne sichtbares `SetHDProportional` im Quelltext. Unabhängig dazu Golden-5971-Shot: **512×406 rendered** — Konvergenz, aber kein blindes Patchen und kein Beweis für 5971-Constructor/HD-State.

NextClient / CS Retro: hart `545×406` + `SetTabWidth(84)`, Options-Baum `IsProportional=0`.

---

## CS-Retro Runtime Dump (`CSRETRO_VGUI_METRICS_DUMP=1`)

Messung 2026-09-01, Audio-Gate, Zielauflösung 800×600 (nach Screen-Stabilize):

| Panel | IsProportional | Bounds (x,y w×h) |
|-------|----------------|------------------|
| OptionsDialog | **0** | 127,97 **545×406** |
| PropertySheet | **0** | 8,30 529×338 |
| OptionsSubMouse (aktiv) | **0** | 0,28 529×310 |
| OK / Cancel / Apply | **0** | y=372, 72×24 |

| Font | prop=0 tall | prop=1 tall |
|------|-------------|-------------|
| Default | 16 | 16 |
| DefaultSmall | 13 | 13 |
| DefaultVerySmall | 12 | 12 |
| DefaultBold | 16 | 16 |
| Marlett | 14 | 14 |

Scheme-Herkunft Runtime: `valve/resource/TrackerScheme.res` (SHA oben). ProportionalBase: **640×480**.

---

## Vergleichstabelle (Build-Mode-/Runtime-Gate)

Legende Spalten:

1. **Current Steam Resource/Code** — `.res` bzw. `gameui.so`-Ctor (HL25-era/current; nicht 5971)  
2. **Current Steam Runtime** — Build Mode und/oder Pixelmessung bei dokumentierter Auflösung  
3. **CS-Retro Resource/Code**  
4. **CS-Retro Runtime** — Metrics-Dump  

Messung Current Steam 2026-09-02: **800×600**, english, Protocol **48**, Exe **Oct 8 2024 (10211)**, appmanifest **12934623**. Shots unter `build/steam-bm-shots/`.

| Control | Current Steam Resource/Code | Current Steam Runtime (800×600) | CS-Retro Resource/Code | CS-Retro Runtime (800×600) |
|---------|----------------------------|----------------------------------|------------------------|----------------------------|
| OptionsDialog size | HD-prop. Basen **522×444** | Pixel/Kanten ≈ **520×444** (≈ Basen; bestätigt Code→Runtime) | NextClient `SetBounds(545,406)` | **545×406** prop=0 @ 127,97; Golden rendered **512×406** |
| OptionsDialog HD/prop | `SetHDProportional(true)` | HD-Pfad aktiv (Basen ≈ Runtime bei 800×600) | kein HD; prop=0 | prop=0 |
| Tab width | Code-Basis **72** (HD-scale) | BM-OCR TabWidth am OptionsDialog noch unvollständig; 7 Tabs inkl. **Aim** sichtbar | `SetTabWidth(84)` | Sheet 8,30 529×338; Tab-Content y=**28** |
| Tab height | Code Default **28** | optisch Tab-Zeile vorhanden | Default 28 | Page Mouse y-offset **28** |
| OK / Cancel / Apply | PropertyDialog 72×24 | sichtbar, rechts unten | dasselbe | OK 297,372 · Cancel 377,372 · Apply 457,372 · je 72×24 |
| Mouse / Aim page | Steam `OptionsSubMouse.res` | Tab-Label **Aim**; Detail-BM Controls noch offen | NextClient Mouse `.res` (+ MouseLook) | ReverseMouse **36,32 155×28** etc. |
| Audio page | Steam `OptionsSubAudio.res` | Tab sichtbar; Detail-BM noch offen | Audio Override | SFX Slider **40,37 420×36** |
| Default font tall | TrackerScheme Tahoma 16 | optisch Tahoma-ähnlich | Liberation/DejaVu, REAL_DIM | GetFontTall **16** |
| `#GameUI_Mouse` | Loc **Aim** | UI „Aim“ | Override **Mouse** | Tab „Mouse“ |
| Scheme | current valve TrackerScheme | geladen | gleicher Winner | valve HIT |
| proportionalBase | HD Surface-Pfad (HL25) | Runtime: Basen 522×444 ≈ Dialoggröße | Classic **640×480** | dump: **640×480** |

Build Mode zusätzlich belegt (andere Panels, nicht OptionsDialog): `GameConsole` wide/tall **560×400**; `ConsoleHistory` **544×324** (xpos8 ypos36). OptionsDialog-Felder in BM: Fokus/OCR noch nachziehen bei 1024/1366.

**1024×768 / 1366×768:** Current-Steam-Messlauf noch ausstehend (nur 800×600 abgeschlossen).

---

## Evidenztabelle (Classic — Dialog-Kandidat, Entscheidung noch offen)

Current Steam = HL25-era Vergleich, **nicht** Classic-Ziel. Keine Screenshot-Zahlen erfinden.

| Metrik | Golden 5971 | Current Steam 2024 | CKF / OpenGoldSrc | NextClient | CS-Retro aktuell | Zielentscheidung |
|--------|-------------|--------------------|-------------------|------------|------------------|------------------|
| Dialog | **512×406** rendered (Shot) | Code **522×444** HD; Runtime@800≈**520×444** | **512×406** code | **545×406** code | **545×406** | **führender Classic-Kandidat 512×406**; endgültig erst mit Tabs/Fonts/Insets/NextClient-Extras |
| TabWidth / Tab-Semantik | kompakt (rendered); Einzelbreiten **unknown** | Code-Basis **72** HD | **84** (min. via `SetTabWidth`) | **84** | **84** | **offen** — `SetTabWidth` ≠ garantierte Renderbreite |
| Proportional / HD | **unknown** | HD (`SetHDProportional`) | nein (Quelltext) | nein | nein (Classic-Gate 640×480) | **offen** |
| Mouse-Tab Loc | Shot: **Mouse** | **Aim** | (Rekonstruktion) | Aim/Mouse je Loc | Override **Mouse** | Pin bewusst |
| Mouse `.res` | unknown (5971-Bytes) | Steam valve SHA | — | NextClient-Layout | Override ≠ Steam | **offen** |

**Konvergenz:** Golden rendered + CKF + OpenGoldSrc → **512×406** als starker Classic-Kandidat. **Nicht** blind `SetBounds(512,406)` patchen, bevor Tabs/Fonts/Insets und NextClient-Zusatzcontrols geklärt sind.

---

## Golden 5971 — screenshot-derived pixel metrics

Kennzeichnung: **Golden 5971 screenshot-derived pixel metrics** — nicht mit BuildMode-/Codewerten gleichsetzen. Unklare Grenzen = **unknown**.

Quelle: Artifact oben; äußere Bounds Benutzer-Messung im logischen 1366×768-Raum. Subpixel aus dem 1024×575-Downscale nur wo eindeutig.

| Element | Screenshot-derived (logisch 1366×768 / Dialog-lokal) | Hinweis |
|---------|------------------------------------------------------|---------|
| Äußere Frame-Bounds (Screen) | x=825..1336, y=13..418 | Benutzer-Messung |
| OptionsDialog wide×tall | **512×406** | rendered |
| Titlebar-Höhe | **unknown** | Downscale unscharf |
| PropertySheet / Content-Bereich | **unknown** | Frame vs. Sheet-Kante nicht eindeutig |
| Tab-Zeilenhöhe | **unknown** (visuell kompakter als CS Retro) | Code-Default `tabheight` **28** = nicht Shot-Beweis |
| Einzelne Tab-Bounds | **unknown** | JPEG/Downscale |
| OK / Cancel / Apply | sichtbar unten rechts; exakte px **unknown** | PropertyDialog-Code typisch 72×24 |
| Renderer Combo (Build Mode **Resource**) | fieldName `Renderer`, xpos**40**, ypos**52**, wide**160**, tall**24** | aus BM-Editor im selben Shot — **Resource**, nicht Screen-Pixel des Frames |
| Resolution / Display Mode Combo | sichtbar; exakte rendered Bounds **unknown** | |
| CheckButtons (Video) | sichtbar rechts; exakte Bounds **unknown** | |
| Brightness / Gamma Slider | sichtbar; exakte Bounds **unknown** | |
| Horizontale / vertikale Insets | **unknown** (nicht eindeutig) | |

---

## PropertySheet / Tab-Semantik (vendortes VGUI2)

`PropertySheet.cpp` / `PageTab`:

- `m_iSpecifiedTabHeight` Default **28** (`tabheight`), Small **14**.
- Bei `IsProportional()`: `m_iTabHeight = GetProportionalScaledValue(specified)`.
- `SetTabWidth(n)` setzt `PageTab::m_bMaxTabWidth` (Name irreführend).
- In `PageTab::ApplySchemeSettings`: `wide = max(m_bMaxTabWidth, contentWide + 10)`.

Daraus: **`SetTabWidth(84)` erzwingt keine feste Renderbreite 84** — es ist eine **Mindestbreite**; kürzere Labels werden auf mindestens 84 gestreckt, längere können breiter sein. Fontmetrik (`contentWide`) bestimmt die sichtbare Kompaktheit mit.

Vergleich später: Golden rendered tab bounds (noch unknown) vs. CKF/OGS vs. NextClient vs. CS-Retro Runtime — **keine** manuellen Tabbreiten in `.res`.

---

## Localization

- Current Steam Loc-Datei: `GameUI_Mouse` = **Aim** (SHA oben); Runtime-Tab **Aim** bestätigt.  
- Golden 5971 Visual: Tab **Mouse** sichtbar — **einzelner** Beleg, keine Ableitung der gesamten historischen Loc.  
- CS-Retro: bewusster Override nur dieses Keys.

---

## mainui.cfg

- Exec entfernt (Body `Localize_Init`).  
- Regression: normale `play.sh` ohne `couldn't exec mainui.cfg`. Punkt geschlossen — keine neue `mainui.cfg` einführen.

---

## Ghidra

- Für **5971 OptionsDialog size** aktuell **nicht nötig** (Screenshot 512×406 + CKF/OGS konsistent).  
- Current Steam 2024: nur Current-Fragen.  
- Historische 5971-Binary erst bei konkreter Restfrage und lokal verfügbarem Stand.

---

## Produktziel (dauerhaft) — Classic ≠ Funktionsdeckel

Der Classic-Metrics-Gate rekonstruiert die historische CS-1.6-VGUI2-**Basis**. Er beschränkt die endgültige CS-Retro-UI **nicht** auf starre historische Fenster oder nur Original-Seiten.

Zielbild:

**klassische CS-1.6-VGUI2-Optik und Bedienlogik** + **NextClient-Funktionalität** + **CS-Retro-Erweiterungen** + **moderne Desktop-/HiDPI-/Responsive-Unterstützung**.

| Ebene | Rolle |
|-------|--------|
| Original-CS-1.6 | visuelle und interaktive **Baseline**, nicht Funktionsdeckel |
| NextClient | funktionale **Basis** |
| CS Retro | ergänzt moderne und zusätzliche Funktionen |

Keine der drei Ebenen gegeneinander ausspielen.

### Classic vs. Modern

- Classic-Metriken = **100%-Referenz** und Regressionstest.  
- Danach kontrolliert skalieren: Linux / Windows / macOS, Desktop-Auflösungen, HiDPI/Retina.  
- Keine globale historische Proportional-Skalierung, die Controls nur aufbläht.  
- Responsive-/HiDPI als **bewusste CS-Retro-Schicht** (nicht MetaHook-HiDPI unbesehen).

### Erweiterbare Options

Neben klassischen Seiten vorgesehen (Feature-UI erst mit Backend, keine toten Optionen): Keyboard/Bindings, Mouse, Audio, Video, Voice, Multiplayer, Gameplay, HUD, Radar, Crosshair-Fine-Tuning, Network, später portierte NextClient-Funktionen.

NextClient-Zusatzcontrols in die Classic-Metrik **integrieren**, nicht Funktionen streichen, falls 512er Breite eng wird — dann bewusstes CS-Retro-Layout.

### Create Game / Server

Echte zentrale Serverkonfiguration über gemeinsames **ServerProfile** (Listen/LAN + Dedicated): Map/Mode/Slots/LAN, GameRules, Round/Freeze/Buy, Team/FF/Balance, Bots, optionale Module (Metamod/AMXX), Plugin-Auswahl, Advanced CVars. Profile speicher- und wiederverwendbar.

### Bots

Bot-UI nicht dauerhaft an ZBot koppeln — gemeinsame Bot-Konfigurationsschnittstelle; implementationsspezifische Advanced-Optionen zusätzlich.

### Plugins / Module

Core ohne Plugins lauffähig. Module optional aus Profil. AMXX später: Aktivierung, Plugin-Auswahl, Profile; strukturierte Seiten für bekannte Plugins möglich.

Details auch: `docs/PHASE3M.md`, `docs/MENUS.md`.

---

## Nächste Schritte (Diagnose-Historie → aktueller Stand)

**Historischer Diagnoseweg (abgeschlossen):** Golden-Metriken → Classic Core Decision → Preferred Size 512×406 → Mouse/Audio-Gate.

**Aktuell:** Video-Subpage (Dependency-Matrix → Xash-Backend-Port → Video-Gate). Tabs/Fonts eigener Pass. FOV/3D gesperrt. Kein Phase-3-Tag.

---

## Classic Core Decision Report (2026-09-02) — **freigegeben und umgesetzt**

### Entscheidung

**Ja: 512×406 als Classic Preferred / Reference Size** für `COptionsDialog`.

Nicht: erzwungene Maximalgröße für alle Zukunft. Nicht: Current-Steam-522×444. Nicht: NextClient-545 als Optikziel.

### Warum 512×406

| Evidenz | Rolle |
|---------|--------|
| Golden 5971 rendered ≈512×406 (logisch; Downscale 384×304 → skaliert konsistent) | Pixel-Wahrheit Classic |
| CKF3Alpha / OpenGoldSrc `SetBounds(512,406)` | unabhängige Code-Rekonstruktion |
| NextClient / CS Retro `545×406` | Abweichung / Slack, nicht Classic |
| Current Steam `522×444` HD | Modernisierungs-/HD-Referenz |

Höhe **406** ist bereits NextClient = Classic; nur die **+33 px Breite** (545−512) ist die bewusste Zurücknahme.

### Fit-Prüfung NextClient-Funktion in 512×406

Runtime-Ist @545 (Metrics-Dump): Sheet **529×338**, Mouse-Page **529×310**, Tab-Content-Y **28**, `prop=0`.

Erwartet @512 (gleiche Insets ±8): Sheet/Page-Breite ≈ **496**, Höhe unverändert ≈310.

| Page / Bereich | .res max extent (relevant) | Passt in ≈496×310? | Bemerkung |
|----------------|----------------------------|--------------------|-----------|
| **Mouse** (CS-Retro/NC, inkl. MouseLook) | interaktiv bis ≈312; Labels `wide` bis 500 | **ja** | Label-`wide=300` ist Textfeld, kein Pflicht-Inhalt; ~4 px Soft-Clip unerheblich |
| **Audio** | Slider bis 460; Miles-Label 486× bis y342 | **Breite ja**; Miles unten wie schon @545 | Miles-Fußzeile ragt vertikal über Page — **höhenbedingt**, nicht durch 512 vs 545 |
| Keyboard (klassisch geplant) | List 480×258 | **ja** | |
| Video / Voice (klassisch) | ≤490 / ≤460 | **ja** | |
| Multiplayer (klassisch) | Guides/Controls bis ~529 | **knapp** | beim Port: Pinning/AutoResize/`.res`, ggf. Dialog **bewusst wachsen** — nicht dauerhaft 545-Slack |
| NextClient Miscellaneous | Controls ≤500 | **ja** in Preferred; Extra-Tab | bei vielen Tabs: Kompaktheit über Font/`SetTabWidth`-Minimum, nicht Dialog-Leerbreite |
| OK/Cancel/Apply | PropertyDialog unten rechts | **ja** | Layout bereits parent-relativ |

**Fazit Fit:** Der vorhandene NextClient-Umfang (Mouse+Audio) und die klassischen Standard-Pages lassen sich in 512×406 **ohne Funktionsentfernung und ohne Übereinanderquetschen** unterbringen. Die 545er Breite ist keine funktionale Notwendigkeit für diese Seiten.

### Bewusst zurücknehmen

- Hartes `SetBounds(…, 545, 406)` als Preferred-Optik (NextClient-Slack).
- Implizite Annahme „breiter = classic-näher“.

### Funktionalität erhalten

- Alle Mouse-/Audio-Controls und CVar-/KeyToggle-Logik (inkl. MouseLook, RawInput, …).
- PropertyDialog Apply/OK/Cancel, `RegisterPage`/`AddPage`.
- NextClient als funktionale Basis; zusätzliche Pages später.
- `SetTabWidth` als **Mindestbreite**-Semantik (kein `.res`-Tabbreiten-Hack).

### Nötige Core-/Layoutänderungen (nach Freigabe dieser Decision)

1. Preferred Size **512×406** setzen (benannte Konstante / klarer Kommentar: Classic Preferred, nicht Max).  
2. PropertySheet weiter über `PropertyDialog::PerformLayout` füllen lassen (ClientArea − `sheetinset_bottom`) — keine per-Dialog-Pixelhacks.  
3. Pages datengetrieben per `.res`; eng werdende künftige Pages: Pinning/AutoResize oder **kontrolliertes Wachstum** des Dialogs.  
4. Tabs: Kompaktheit aus Fontmetrik / ContentWidth / Padding / Scheme / Tab-Minimum — **nicht** aus Dialog-Extra-Breite.  
5. Keine globale HD-/Proportional-Umschaltung für Classic.

### Classic Preferred vs. Responsive / HiDPI (getrennt)

| Schicht | Aufgabe |
|---------|---------|
| **Classic Preferred 512×406** | 100%-Referenz + Regression (Golden) |
| **Content-driven grow** | einzelne CS-Retro-/dichte Pages dürfen Dialog bewusst vergrößern |
| **Responsive Desktop** | sinnvolle Platzierung/Größe je Auflösung — ohne Controls nur aufzublasen |
| **HiDPI (später)** | eigene CS-Retro-Skalierungsschicht — nicht MetaHook/HL25-HD unbesehen |

### Tabs (unverändert festgehalten)

`SetTabWidth(n)` = Mindestbreite (`max(n, contentWide+10)`), keine garantierte Renderbreite. 7×84 px würden selbst 545er Sheet (~529) sprengen — **545 löst das Tab-Problem nicht**. Tab-Pass separat nach Preferred-Size-Core.

### Current Steam

Weiter nur HD-/Responsive-Research (optional 1366); nicht Classic-Ziel.

### Gate nach Implementierung

Mouse + Audio bei gleicher Auflösung gegen Golden/Classic → wenn grün, Video freigeben. FOV/3D gesperrt. Kein Phase-3-Tag.

## Mouse + Audio Gate nach Preferred-Size-Patch (2026-09-02)

**Funktional:** PASS — `vgui-options-mouse-gate.sh` + `vgui-options-audio-gate.sh` bei **640×480, 800×600, 1024×768, 1366×768** (Apply/OK/Cancel/Reset/Persist/Controls/Loc).

**Runtime:** Dialog **512×406** zentriert (z. B. @800: 144,97; @640: 64,37). Sheet **496×338**, Mouse-Page **496×310**. OK/Cancel/Apply 72×24 unten rechts.

**Visuell (Shots `build/options-*-shots/`):** kein Text-Clip, keine Überlappung, Frame/Tabs/Slider/Checks/Symbole OK. Miles-Footer **absichtlich hidden** (kein Miles-Backend) — nicht in sichtbare Layoutabnahme.

**Fix-Nachtrag:** `vgui_boot.cpp` hatte noch hart `SetSize(545,406)` (Show + RunFrame-Zentrierung) — auf `CsretroOptionsClassic` umgestellt.

**Video:** freigegeben nach diesem Gate. FOV/3D weiterhin gesperrt. Kein Phase-3-Tag.
