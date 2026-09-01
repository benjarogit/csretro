# Phase 3M — VGUI2 Metrics / Classic Gate — Diagnosebericht

Stand: 2026-09-02. **Video gesperrt.** Mouse/Audio funktional abgenommen; **visuelle** Classic-Abnahme offen.

Ziel: gemeinsamen VGUI2-Unterbau korrekt machen — keine kosmetischen `.res`-/Dialog-Patches.

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
| Rolle | **nur** visuelle Zieloptik (Screenshot-Pin); proportional/HD-Interna **unknown** |
| Build | **5971** (`Exe build: 11:45:32 Mar 1 2013` laut Console im Ref-Shot) |
| Sprache / Auflösung (Shot) | English / **1366×768** |
| Aus Shot belegt | u. a. Tab-Text **Mouse** |
| Nicht belegt | Byte-Identität von TrackerScheme, vollständiger Loc-Datei oder GameUI-Binary mit heutigem Steam |

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

Klassische Rekonstruktionen (CKF3Alpha / OpenGoldSrc, gepinnte Commits in `UPSTREAM.md`): hart `512×406` + `SetTabWidth(84)` ohne sichtbares `SetHDProportional` im Quelltext — **Evidenz**, nicht Garantie dass 5971 identisch war.

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
| OptionsDialog size | HD-prop. Basen **522×444** | Pixel/Kanten ≈ **520×444** (≈ Basen; bestätigt Code→Runtime) | NextClient `SetBounds(545,406)` | **545×406** prop=0 @ 127,97 |
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

## Evidenztabelle (Classic-Entscheidung — alle Ziele offen)

Nur belegte Zellen. Keine Screenshot-Zahlen für 5971 erfinden. Current Steam = Forschungsdatensatz, **nicht** automatisch Classic-Ziel.

| Metrik | Golden 5971 | Current Steam 2024 | CKF / OpenGoldSrc | NextClient | CS-Retro aktuell | Zielentscheidung |
|--------|-------------|--------------------|-------------------|------------|------------------|------------------|
| Dialog | visuell (keine Zahl) | Code **522×444** HD; Runtime@800≈**520×444** | **512×406** hart | **545×406** hart | **545×406** | **offen** |
| TabWidth | visuell | Code-Basis **72** HD | **84** | **84** | **84** | **offen** |
| Proportional / HD | **unknown** | HD (`SetHDProportional`) | nein (Quelltext) | nein | nein (Classic-Gate 640×480) | **offen** |
| Mouse-Tab Loc | Shot: **Mouse** | **Aim** | (Rekonstruktion) | Aim/Mouse je Loc | Override **Mouse** | **offen** (Pin bewusst) |
| Mouse `.res` | unknown (5971-Bytes) | Steam valve SHA | — | NextClient-Layout | Override ≠ Steam | **offen** |

Gewichtungsregel: Quellen getrennt; weder Current-2024 noch CKF/OGS allein = Wahrheit für 5971.

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

- Current Steam 2024 / `gameui.so` / `vgui2.so`: nur Current-Fragen.  
- **Nicht** daraus 5971-Interna ableiten.  
- Historische 5971-Binary erst analysieren, wenn lokal nachweisbar vorliegend; sonst Punkt = **unknown**, konservativ aus übrigen Quellen.

---

## Nächste Schritte (kein Video)

1. Current Steam Build Mode: OptionsDialog-Felder + Aim/Audio-Controls bei **1024×768** und **1366×768** nachziehen.  
2. Evidenztabelle füllen — **keine** Layoutwahl (545/522/512, Tab 84/72) vor Abschluss.  
3. Danach zentrale Metrics-/Font-/Scheme-Entscheidung.  
4. NextClient-**Funktion** behalten; Layoutabweichungen nicht als Originaloptik.  
5. Mouse+Audio erneut bei gleicher Auflösung abnehmen → Video freigeben.

## Research References

Siehe `docs/UPSTREAM.md` Abschnitt „VGUI2 Research References (Phase 3M)“ und `CREDITS.md` (Research/VGUI2).
