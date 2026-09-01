# Phase 3M — VGUI2 Metrics / Classic Gate — Diagnosebericht

Stand: 2026-09-01. **Video gesperrt.** Mouse/Audio **nicht** visuell vollständig abgenommen.

Ziel dieses Gates: gemeinsamen VGUI2-Unterbau korrekt machen — keine kosmetischen `.res`-/Dialog-Patches.

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

## Golden References — klar trennen

### A) Golden Classic Visual Reference (Screenshot-Pin)

| Feld | Wert |
|------|------|
| Produkt | Steam Counter-Strike 1.6 |
| Build | **5971** (Mar 1 2013) — laut Screenshot-Console |
| Sprache | English |
| Auflösung (Ref-Shot) | 1366×768 |
| Rolle | Visuelle 1:1-Zieloptik (klassisch) |

### B) Current Steam BuildMode Reference (dieser Host)

| Feld | Wert |
|------|------|
| Steam AppID | 10 |
| `appmanifest` buildid | **12934623** (≠ 5971) |
| Rolle | Live Build Mode / Resource-Pfad / aktuelle Loc |
| Warnung | **Nicht** automatisch Ground-Truth für Build 5971 |

Vor jedem Original-Messlauf erneut Build/Exe-Datum/Protocol/Sprache/Auflösung protokollieren.

---

## Gepinnte Dateien (SHA-256)

| Logisch (VFS) | Resolved (dieser Host) | SHA-256 | Hinweis |
|---------------|------------------------|---------|---------|
| `resource/TrackerScheme.res` | `gamedata/valve/resource/TrackerScheme.res` (= Steam `valve/...`) | `1915a4740b3f171ca2773f5ffd8b7b0497702475d7298ef3066648727aa8413a` | Runtime-Winner CS Retro |
| `resource/TrackerScheme.res` (platform) | `gamedata/platform/resource/TrackerScheme.res` | `5593b672cffca50f78bea9103bbd527ef0d2d82639d7c306e5b2a9da7639ba21` | ≠ valve; Fallback |
| `resource/gameui_english.txt` | Steam/gamedata `valve/resource/...` | `aca17e06d49b14d1ae31f14fca77d02a5c0f9d41af27353f5a5362453c39d711` | `GameUI_Mouse`=**Aim** |
| CS-Retro Loc-Pin | `data/ui-overrides/cstrike/resource/csretro_gameui_english.txt` | (Override) | `GameUI_Mouse`=**Mouse** (5971-Optik) |
| Steam `OptionsSubMouse.res` | `…/Half-Life/valve/resource/OptionsSubMouse.res` | `d441c5684c3f47f2ae646710c35620da7ff8d5a1030beeaa99d295d31b51779e` | Original-Resource |
| CS-Retro Mouse `.res` | `data/ui-overrides/.../OptionsSubMouse.res` | `31540f55e2e0a6ab67bff129f54b23116d655992abb44189c4bc1c21ff8800dd` | **≠ Steam** (NextClient-Layout) |
| Steam `OptionsSubAudio.res` | `…/valve/resource/OptionsSubAudio.res` | `f7616bced2eb9b47c81e5dd7388f1d1666aedde2d24118b76740da0ef90acde8` | |
| CS-Retro Audio `.res` | `data/ui-overrides/.../OptionsSubAudio.res` | `6585b66ec0ff63b8429731487fde4741eac227808e1230cc1bf88faaf6d2f205` | kleine Diffs (Label-wide) |

Scheme-Suchreihenfolge CS Retro: `cstrike` → **`valve` (HIT)** → `platform` → UI-Override.

---

## Classic Proportional Base

| | Wert |
|--|------|
| Historische VGUI-Basis | **640×480** |
| Kurze Fehlentscheidung | `GetProportionalBase` global auf HD 1280×720 bei ≥720p |
| Status | **zurückgenommen** — Classic-Gate wieder fest **640×480** |
| HiDPI/MetaHook-Modus | späterer **bewusster** CS-Retro-Modus, nicht Classic-Baseline |

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

**Erkenntnis:** Options-Baum ist **nicht** proportional. Tab-Inhalt beginnt bei **y=28** (= `m_iSpecifiedTabHeight` Default). `SetTabWidth(84)` kommt von **NextClient**, nicht automatisch Valve-Original.

Screen startet oft 640×480 und wächst danach — Dump/Layout nach Stabilize wiederholen.

---

## Vergleichstabelle (Stand Diagnose — Build Mode Original noch offen)

| Control | Original Resource | Original BuildMode/Runtime | CS-Retro Resource | CS-Retro Runtime |
|---------|-------------------|----------------------------|-------------------|------------------|
| OptionsDialog size | (GameUI, oft 512×406 in Rekonstruktionen) | **Build Mode 5971: noch zu messen** | NextClient `SetBounds(545,406)` | **545×406** prop=0 |
| Tab height | Scheme/Code Default **28** | zu messen | Code Default 28 | Page y-offset **28** |
| Tab width | zu messen | zu messen | NextClient **84** | (Sheet layout) |
| Default font tall | TrackerScheme Tahoma **16** weight 0, kein AA | Win32 CreateFont cell | Liberation/DejaVu via Resolver, AA=0, REAL_DIM | GetFontTall **16** |
| Titlebar caption | Frame Code **23** | zu messen | Code 23 | (Font-abhängig optisch) |
| Close symbol | Marlett `r` | geometrisch OK im Original | `vgui_symbols` | X geometrisch |
| Mouse page `.res` | Steam valve SHA oben | Build Mode | **abweichend** (MouseLook + xpos-Shifts) | Layout folgt Override |
| `#GameUI_Mouse` | 5971-Shot: **Mouse**; current Steam: **Aim** | — | CS-Retro Override **Mouse** | Tab „Mouse“ |

Auflösungsregel: Vergleiche nur **gleiche** Auflösung (640/800/1024/1280|1366). Kein 800↔1366-Abnahmeurteil.

---

## Localization

- Current Steam + gamedata: `GameUI_Mouse` = **Aim**  
- Golden 5971 Visual: **Mouse**  
- CS-Retro: bewusster Override `csretro_gameui_english.txt` — dokumentiert, nicht „zufällig“.

---

## mainui.cfg

- Ursache: Body `Localize_Init` → `exec mainui.cfg` (nicht Menu-Lib)  
- Status: Exec **entfernt**  
- Regression: `play.sh` ohne `couldn't exec mainui.cfg` (zu bestätigen nach Client-Deploy)

---

## Nächste Core-Schritte (kein Video)

1. Original Build Mode auf **current Steam** (buildid 12934623) bei 800/1024/1366 — Werte erfassen; klar als „current“ labeln.  
2. Wenn möglich: Build **5971**-Artefakt/Shot nur als Visual; Abweichungen current↔5971 notieren.  
3. CS-Retro Mouse `.res` vs Steam `.res` entscheiden: Classic-Resource angleichen **oder** NextClient-Delta bewusst dokumentieren (nicht heimlich).  
4. Dialoggröße 545 vs 512 vs Original Build Mode.  
5. Font: quantitative ABC/Textbreiten vs Win32-Tahoma (nicht Liberation „fühlt sich besser an“ als Endurteil).  
6. Erst dann Mouse+Audio bei gleicher Auflösung erneut abnehmen.

## Research References

Siehe `docs/UPSTREAM.md` Abschnitt „VGUI2 Research References (Phase 3M)“.
