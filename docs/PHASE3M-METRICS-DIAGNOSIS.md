# Phase 3M — VGUI2 Metrics / Classic Gate — Diagnosebericht

Stand: 2026-09-01 (Provenance-Cleanup + Current-Steam-Codeanalyse). **Video gesperrt.** Mouse/Audio funktional abgenommen; **visuelle** Classic-Abnahme offen.

Ziel: gemeinsamen VGUI2-Unterbau korrekt machen — keine kosmetischen `.res`-/Dialog-Patches.

## Quellenhierarchie (verbindlich)

1. Originales Steam-CS-1.6 (Runtime, Pixel, Build Mode)  
2. Zugehörige Originalressourcen (`.res`, Scheme, Loc)  
3. Vendortes VGUI2 / `vgui_controls`  
4. NextClient (funktionale GameUI)  
5. Valve Developer Community — VGUI/VGUI2-Doku (Konzept; nicht ungeprüft Source-Engine-APIs)  
6. Research: CKF3Alpha, OpenGoldSrc, Ref B, fwgs-vgui2-support, MetaHookSv/VGUI2Extension, Ref A (Desktop)  
7. Ghidra nur bei Restlücke nach 1–6  

Build Mode Shortcut (Valve-Doku): **Ctrl+Shift+Alt+B**

---

## Golden 5971 vs Current Steam — strikt getrennt

### A) Golden Classic Visual Reference

| Feld | Wert |
|------|------|
| Rolle | **nur** visuelle Zieloptik (Screenshot-Pin) |
| Build | **5971** (`Exe build: 11:45:32 Mar 1 2013` laut Console im Ref-Shot) |
| Sprache / Auflösung (Shot) | English / **1366×768** |
| Aus Shot belegt | u. a. Tab-Text **Mouse** |
| Nicht belegt | Byte-Identität von TrackerScheme, vollständiger Loc-Datei oder GameUI-Binary mit heutigem Steam |

### B) Current Steam Resource / BuildMode Reference (dieser Host)

| Feld | Wert |
|------|------|
| Rolle | Live Resources, Binary-Analyse, künftiger Build-Mode-Messlauf |
| AppID / appmanifest `buildid` | 10 / **12934623** |
| `cstrike/steam.inf` | `PatchVersion=1.1.2.7` |
| Engine (`hw.so` String) | `Exe build: 01:35:13 Oct  8 2024` |
| Protocol | Console-`version` bei laufendem Steam-Client; Stand 2026-09-01: Headless-`hl_linux` ohne Steam-Pipe abgebrochen — **noch offen** (nicht mit appmanifest-buildid verwechseln) |
| Sprache | Steam UserConfig **english** |
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

## Classic Proportional Base

| | Wert |
|--|------|
| Historische VGUI-Basis (Classic-Gate) | **640×480** |
| Kurze Fehlentscheidung | `GetProportionalBase` global auf HD 1280×720 |
| Status | **zurückgenommen** — Classic-Gate fest **640×480** |
| Current Steam GameUI | ruft `SetHDProportional(true)` auf OptionsDialog — **andere** Generation als Classic-Baseline |
| MetaHookSv HiDPI | alle Elemente proportional — Research only, nicht Classic-Baseline |

---

## Current Steam GameUI — Codewerte (objdump `gameui.so`)

`COptionsDialog::COptionsDialog`:

1. `SetHDProportional(true)`
2. `SetBounds(0, 0, GetProportionalScaledValue(522), GetProportionalScaledValue(444))`
3. `GetPropertySheet()->SetTabWidth(GetProportionalScaledValue(72))`

Das sind **Current-Steam-Codebasen**, keine Golden-5971-Werte.

Klassische Rekonstruktionen (CKF3Alpha / OpenGoldSrc, gepinnte Commits in `UPSTREAM.md`): hart `512×406` + `SetTabWidth(84)` **ohne** HD-Proportional — ebenfalls nicht automatisch = 5971, aber näher am klassischen GameUI-Muster.

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

1. **Current Steam Resource/Code** — `.res` bzw. `gameui.so`-Ctor (nicht 5971)  
2. **Current Steam Runtime** — Build Mode bzw. skaliertes Ergebnis; interaktive BM-Werte noch ergänzen  
3. **CS-Retro Resource/Code**  
4. **CS-Retro Runtime** — Metrics-Dump  

Auflösung Referenzzeile: **800×600** (CS-Retro gemessen). Current-Steam-Runtime für HD-Skalierung: `value * screen / HD-Base` — absolutes Build-Mode-Ergebnis bei gleicher Auflösung **noch zu erfassen** (nicht aus 5971-Shot schätzen).

| Control | Current Steam Resource/Code | Current Steam Runtime (BM) | CS-Retro Resource/Code | CS-Retro Runtime (800×600) |
|---------|----------------------------|----------------------------|------------------------|----------------------------|
| OptionsDialog size | HD-prop. Basen **522×444** | BM bei 800×600: **offen** | NextClient `SetBounds(545,406)` | **545×406** prop=0 @ 127,97 |
| OptionsDialog HD/prop | `SetHDProportional(true)` | BM: offen | kein HD; prop=0 | prop=0 |
| Tab width | Code-Basis **72** (danach HD-scale) | BM: offen | `SetTabWidth(84)` | Sheet 8,30 529×338; Tab-Content y=**28** |
| Tab height | Code Default **28** | BM: offen | Default 28 | Page Mouse y-offset **28** |
| OK / Cancel / Apply | PropertyDialog 72×24 | BM: offen | dasselbe | OK 297,372 · Cancel 377,372 · Apply 457,372 · je 72×24 |
| ReverseMouse | xpos**30** ypos32 wide**160** tall28 | BM: offen | xpos**36** ypos32 wide**155** tall28 | **36,32 155×28** prop=0 |
| MouseFilter | xpos30 ypos**56** … | BM: offen | xpos36 ypos**76** … (+ MouseLook) | **36,76 155×28** |
| MouseLook | (nicht in current Steam Mouse.res) | — | xpos36 ypos54 | **36,54 155×28** |
| Sensitivity Slider | xpos34 ypos222 wide272 tall40 | BM: offen | xpos40 ypos222 wide272 tall40 | **40,222 272×40** |
| Audio SFX Slider | xpos40 ypos37 wide420 tall36 | BM: offen | gleich | **40,37 420×36** |
| Audio sfx label | wide **160** | BM: offen | wide **240** | **42,14 240×24** |
| Sound Quality Combo | xpos40 ypos226 wide180 tall24 | BM: offen | gleich | **40,226 180×24** |
| Default font tall | TrackerScheme Tahoma 16, weight 0, kein AA | Win32 CreateFont | Liberation/DejaVu Resolver, AA=0, REAL_DIM | GetFontTall **16** |
| `#GameUI_Mouse` | current Loc **Aim** | UI „Aim“ | CS-Retro Override **Mouse** (5971-Visual-Pin) | Tab „Mouse“ |
| Scheme file | current valve TrackerScheme SHA | geladen | gleicher Winner in CS Retro | valve HIT |
| proportionalBase | Current Steam: HD-Pfad (Surface HD-Base) | BM: offen | Classic **640×480** | dump: **640×480** |

**Golden 5971:** nur Visual; Dialoggröße/Tabs **nicht** aus Shot als Zahlen übernommen. Rekonstruktions-Hinweis (CKF/OGS): 512×406 + TabWidth 84 — Entscheidungsgrundlage erst nach Current-BM **und** Abgleich mit Classic-Ziel.

---

## Localization

- Current Steam Loc-Datei: `GameUI_Mouse` = **Aim** (SHA oben).  
- Golden 5971 Visual: Tab **Mouse** sichtbar — **einzelner** Beleg, keine Ableitung der gesamten historischen Loc.  
- CS-Retro: bewusster Override nur dieses Keys.

---

## mainui.cfg

- Exec entfernt (Body `Localize_Init`).  
- Regression: normale `play.sh` ohne `couldn't exec mainui.cfg`. Punkt geschlossen — keine neue `mainui.cfg` einführen.

---

## Nächste Schritte (kein Video)

1. Current Steam bei **gleicher** Auflösung mit laufendem Steam-Client: Console `version` (Protocol + Exe) + Build Mode für OptionsDialog / Sheet / Tabs / OK·Cancel·Apply / Mouse·Audio-Controls — Runtime-Spalte füllen.  
2. Entscheidung **nach** Tabelle: 545 vs 522-HD vs klassisch-512; TabWidth 84 vs 72-HD; Steam-`.res` vs NextClient-`.res`; Font-ABC vs Win32.  
3. NextClient-**Funktion** behalten; Layoutabweichungen nicht als Originaloptik dokumentieren.  
4. Zentrale Core-Fixes — dann Mouse+Audio erneut bei gleicher Auflösung abnehmen → Video freigeben.

## Research References

Siehe `docs/UPSTREAM.md` Abschnitt „VGUI2 Research References (Phase 3M)“ und `CREDITS.md` (Research/VGUI2).
