# Phase 3M — Adaptive Layout / Resize

Stand: 2026-09-03.
**Status: AUTOMATED PASS / MANUAL ACCEPTANCE OPEN** — natives Resize ist freigegeben; alle acht Grips, Live-Save ohne Apply sowie Save/Restart/Restore sind im Gate grün.

Kein Phase-3-Tag. FOV/3D gesperrt. Keine neue Options-Subpage.
Keyboard/Mouse/Audio/Video = **PASS / Regression**. Visual Polish **danach**.

## Ziel

`512×406` = **Classic Preferred / Reference Size** (`CsretroOptionsClassic`).

CS-Retro Options kann **sauber größer** und — bis zum definierten Minimum — **kleiner** gezogen werden.

Die Layout-Semantik der vier Seiten und das abgeleitete Minimum wurden vor der Resize-Freigabe festgelegt.

## Foundation (dieser Build)

| Stück | Stand |
|-------|--------|
| Classic 512×406 Keyboard-Liste | `.res` **8,10,480×258** — Gate `classic_list_512x406` |
| Keyboard Grow | Extra = Dialog − Preferred; bei 700×520 Liste **668×372** in Page 684×424 |
| AutoResize-Pins | nach `LoadControlSettings` verworfen (64×24-Parent); Layout CS-Retro-owned |
| PropertySheet / OK-Cancel-Apply | Sheet füllt ClientArea; Footer unten und bleibt aligned |
| Mouse/Audio/Video | Classic-`.res` **unverändert** auch nach Grow (Slider / SFX / Resolution) |
| Audio visible spacing | Sound Quality rückt unter MP3; hidden HEV/Suit-Loch bleibt nicht sichtbar |
| Minimum | **abgeleitet 512×406** = Preferred (siehe unten) |
| Persistenz | `$XASH3D_BASEDIR/cfg/csretro_ui_geometry.txt` — Roundtrip + Clamp |
| Live-Save | Move/Resize wird nach Maus-Release gespeichert, unabhängig von Apply |
| List-Clip | sichtbare Keyboard-Rows/Section-Headers bleiben im List-Viewport; Footer bleibt frei |
| Grip | unterer rechter Resize-Griff als diagonale Linien statt Blocksymbol |
| vgui_boot | kein Per-Frame-Stomp auf 512×406; Workspace-Clamp bei Auflösungswechsel |
| `SetSizeable` | **true** — native VGUI-Grips an vier Kanten und vier Ecken |
| Gate | `./scripts/vgui-options-layout-gate.sh` (isoliert `build/run-gate/layout`) |

## Abgeleitetes Minimum

`ComputeMinimum` vereinigt sichtbare Child-BBoxen aller vier Pages + Dialog-Chrome, dann **cap** auf Preferred.

**Gemessen: 512×406.** Das ist kein Blind-Default: Mouse/Audio/Video-`.res` füllen Classic bereits (Controls bis x+w≈500). Kleiner als Preferred würde diese Pages clippen.

Keyboard-List-Floor (280×120) gilt erst, wenn die anderen Pages schrumpfen dürfen. Solange nicht: Dialog-Minimum = Preferred. Liste bei Minimum = Classic 480×258.

## Grow-Regel

Zusätzliche Pixel = `dialogW − 512` / `dialogH − 406`, **nicht** Page-Größe zum 64×24-Konstruktionszeitpunkt.

- Keyboard-Liste wächst H+V; Footer-Buttons unter der Liste; Change/Clear rechtsbündig zur Liste
- Spalten: Action 45 % / Primary / Alternate teilen die neue Breite
- Mouse/Audio/Video behalten Classic-Control-Geometrie; Extrafläche = Leerraum

## Persistenz / Clamp

| Check | Ergebnis |
|-------|----------|
| Save/Load Roundtrip | **OK** (`persist_roundtrip`) |
| 700×520 → Workspace 640×480 | **640×480**, vollständig im Workspace |
| Offscreen/übergroß → 800×600 | Size in `[min, workspace]`, vollständig im Workspace |
| Live-Restore auf aktuellem Workspace | **OK** (`restore_clamped_to_workspace`) |
| Move/Resize ohne Apply | **OK** (`geometry_live_without_apply`) |
| Save 700×520 → neuer Prozess @640×480 | **OK** (`OPTIONS_LAYOUT_RESTART_RESTORE AUTOMATED_OK`) |
| Ungültige Geometry beim Öffnen | Preferred + Zentrierung (`vgui_boot`) |

Prozess-übergreifendes Save → Restart → Restore läuft über den echten Show-Pfad in `VGuiXash_ShowOptionsDialog`; das Gate prüft ihn in einem separaten zweiten Prozess.

## Gate (2026-09-03)

```
./scripts/vgui-options-layout-gate.sh
# optional: CSRETRO_GATE_RES=640x480
# ASan: ./scripts/build-menu.sh --sanitize && \
#   CSRETRO_MENU_SO=build/menu-sanitize/menu/menu_amd64.so CSRETRO_GATE_RES=640x480 \
#   ./scripts/vgui-options-layout-gate.sh
```

| Workspace | Automated |
|-----------|-----------|
| 640×480 | OK |
| 800×600 | OK |
| 1024×768 | OK |
| 1366×768 | OK |
| ASan/UBSan @800×600 | Gate-Marker OK; Restart-Teardown meldet noch ASan-DeadlySignal nach Restore-Marker |

Mitgeprüft: `sizeable_enabled`, alle acht nativen Frame-Grips, Top/Left-Anker am Workspace-Rand, Minimum- und Workspace-Clamp, Classic-Liste, Grow-Delta, Liste in Page/Dialog, Footer nach unten, List-Child-Clipping, Page-Wechsel, Wheel, Capture Primary, QueryBox, Persist/Clamp, Live-Save ohne Apply, Shrink→Classic, Regrow und Prozess-Restart/Restore.

Shots: `build/options-layout-shots/csretro-options-layout-{640x480,800x600,1024x768,1366x768}.png`.

**STATUS: AUTOMATED PASS / MANUAL ACCEPTANCE OPEN.**

## Manual Acceptance

```bash
./scripts/build-menu.sh
./scripts/play.sh
```

1. An allen vier Kanten und vier Ecken ziehen; der sichtbare Griff unten rechts gehört dazu.
2. Verkleinern: Stopp bei **512×406**, keine abgeschnittenen Controls.
3. Vergrößern: Keyboard-Liste wächst, Footer bleibt unten; Mouse/Audio/Video bleiben kollisionsfrei.
4. Verschieben/vergrößern, mit Cancel/X schließen ohne Apply, Spiel neu starten und gespeicherte Position/Größe prüfen.
5. Nach Manual PASS beginnt Global VGUI2 Visual Polish.

## Reihenfolge

1. Adaptive Layout-Semantik: Keyboard, Mouse, Audio, Video — **automated grün**
2. Minimum + Persistenz + Clamp — **automated grün** (Min = 512×406, begründet)
3. Native Kanten/Ecken + Prozess-Restart/Restore — **automated grün**
4. Manual Acceptance — **offen**
5. Danach: Global VGUI2 Visual Polish

## Seiten-Semantik

| Seite | Bei 512×406 | Extra Fläche / kleiner |
|-------|-------------|-------------------------|
| Keyboard | Classic-Geometrie | ListViewport wächst mit; Scrollbar folgt; Primary/Alternate nutzen Breite; Footer unten |
| Mouse | Classic-Geometrie **behalten** | Extra = Leerraum |
| Audio | kompakte sichtbare Geometrie ohne HEV/Suit-Loch | Extra = Leerraum |
| Video | Classic-Geometrie **behalten** | Combo-Alignment = Visual Polish; Backend geschlossen |

## Nicht in diesem Block

- Neue Subpage
- FOV / 3D
- Dialogweise Pixelhacks
- Visual Polish (Scrollbar-Optik, Fonts, ComboBox-Metrik, Capture-Akzent)
