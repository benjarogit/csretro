# Phase 3M — Options Keyboard

Stand: 2026-09-03.
**Keyboard = AUTOMATED PASS / MANUAL RECHECK OPEN**

Nach User-Retest wurden zwei Persistenzpfade gefunden und geschlossen: staged Key-Änderungen konnten beim Page-Wechsel durch einen neuen Engine-Snapshot überschrieben werden; außerdem hatten alte Gates den normalen `build/run`-Play-Baum mit Test-Configs/HL-Fallbacks kontaminiert. Fix ist umgesetzt und automatisiert grün; normaler `./scripts/play.sh`-Manual-Recheck bleibt offen. Debug-/Audit-Infrastruktur bleibt, ist **kein** Blocker.

| Nachbarstatus | Wert |
|---------------|------|
| Runtime Provenance | **PASS** (maßgeblich: Menu-SHA / git-dirty, nicht nur `__DATE__`/`built=`) |
| Capture Transport | **PASS** (`extApi=1`) |
| Capture UX | **PASS** — „Press a key…“ in `BrightControlText` (Primary) / `BrightBaseText` (Alternate) |
| Binding Snapshot / BIND_AUDIT | **PASS** — Klassen, gleicher BASEDIR-Kontext (Infrastruktur bleibt) |
| Test isolation | **PASS** — Gates `build/run-gate/`, 3C `build/run-3c`, play `build/run` |
| Wheel scrolling | **PASS** — idle scrollt; Capture = `MWHEELUP`/`DOWN`; danach wieder Scroll |
| Window Geometry Basic Save/Restore | **PASS** |
| Window Geometry Resolution/Workspace Clamp | **AUTOMATED PASS** — inklusive Prozess-Restart |
| Adaptive Layout / Resize | **AUTOMATED PASS / MANUAL OPEN** — acht native Grips, Min 512×406; `docs/PHASE3M-LAYOUT.md` |
| Global VGUI2 Visual Polish | **OPEN** |

Mouse/Audio/Video = **PASS / Regression**. Keyboard = **AUTOMATED PASS / MANUAL RECHECK OPEN**. **Kein Phase-3-Tag. FOV/3D gesperrt.** Keine nächste Subpage.

## Manual Acceptance

2026-09-02 gab es einen isolierten normalen `play.sh`-PASS. Der User-Retest danach hat Persistenzverlust gemeldet; deshalb ist dieser Punkt wieder **MANUAL RECHECK OPEN**.

- [ ] Binding setzen, Tab wechseln, zurück wechseln: staged Änderung bleibt sichtbar
- [ ] Apply schreibt Binding in Engine/Config
- [ ] Restart: Binding bleibt erhalten
- [ ] Doppelklick Primary und Alternate
- [ ] Edit key, sichtbarer Press-a-key-State, ESC-Abbruch
- [ ] Conflict No/Yes
- [ ] Wheel außerhalb Capture scrollt; während Capture bindet `MWHEELUP`/`MWHEELDOWN`

## Automated Gates (Regression)

`OPTIONS_KEYBOARD_GATE PASS / Regression` + `KEYBOARD_CAPTURE_PHYS AUTOMATED_OK`.

Adapter: `Edit→F8`, `Primary→Q`, `Alternate→F7`, `Enter→F6`, `letter→Q` + `path_waiting_visible`.
Phys: F8 + `q` während `capturing=1`.

| Marker | Bedeutung |
|--------|-----------|
| `CSRETRO_KEYBOARD_WHEEL_OK idle_scroll` | nicht capturing → Liste scrollt |
| `CSRETRO_KEYBOARD_WHEEL_OK capture_bind` | capturing → `MWHEELDOWN` wird Binding |
| `CSRETRO_KEYBOARD_WHEEL_OK after_scroll` | Capture beendet → Wheel scrollt wieder, bindet nicht |
| `CSRETRO_KEYBOARD_GATE_OK persist_stage_survives_tab` | staged Binding bleibt nach Page-Wechsel erhalten |
| `CSRETRO_KEYBOARD_GATE_OK persist_engine_after_apply` | Apply schreibt die Änderung in die Engine |
| `OPTIONS_KEYBOARD_PERSIST AUTOMATED_OK F11=+forward` | isolierte `config.cfg` enthält das neue Binding |

Skript: `./scripts/vgui-options-keyboard-gate.sh` (isoliert `build/run-gate/keyboard`).

## Repair (geschlossen)

| Fix | Was |
|-----|-----|
| Doppelklick | nur `ItemDoubleLeftClick` — kein synthetisches `KEY_ENTER` |
| Capture UX | Slot-aware: DblClick Primary/Alt, Edit-Heuristik; `#GameUI_PressAKey` |
| ESC | restores Slot-Text |
| Conflict | lokalisierte Action-Namen + `#GameUI_KeyReplacePrompt` |
| Wheel ownership | `K_MWHEEL*` → Capture: raw bind; Idle: `InternalMouseWheeled` |
| BIND_AUDIT | Klassen, `$XASH3D_BASEDIR/cstrike/config.cfg` |
| Isolation | Gates nicht in `build/run`; GameData read-only |
| Pending Snapshot Guard | `OnPageShow` nimmt nur ohne staged Changes einen neuen Engine-Snapshot |
| Engine-Startvertrag | Scripts nutzen `-menulib` mit absolutem Menüpfad |
| Play-Config-Schutz | `play.sh` seedet CS-Defaults nur bei leerer/HL-Fallback/Gate-Config; User-Binds bleiben danach unangetastet |
| Legacy-Gate-Isolation | alte Mouse/Audio/Video/V1PoC/Shutdown-Gates defaulten jetzt auf `build/run-gate/*`, nicht `build/run` |

Defaults-Semantik: Öffnen = Engine-Snapshot; Use Defaults nur nach Bestätigung.

## Offene Punkte (nicht Keyboard-PASS)

Visuelle/Layout-Reste — **kein** Keyboard-Funktionsblocker.

| # | Befund | Klasse | Wann |
|---|--------|--------|------|
| 2 | Capture-Slot farbig | **erledigt** — `BrightControlText`/`BrightBaseText` während `#GameUI_PressAKey` | — |
| 3 | Scrollbar / Pfeile / Spalten/Insets | Visual Polish | danach |
| 4 | Audio-Leerraum Volume ↔ Sound quality | Adaptive Layout | Audio bleibt **PASS** |
| 5 | ComboBox/Dropdown | zentraler Control-Polish | Visual Polish — keine Seiten-Patches |
| 6 | Video Resolution/Renderer/Aspect/Display Mode | Visual/Layout | Video-Backend geschlossen |

## Wheel ownership

Xash: `SDL_MOUSEWHEEL` → `IN_MWheelEvent` → `K_MWHEELUP`/`K_MWHEELDOWN`. Dasselbe Event, zwei Owner:

| Zustand | Owner | Wirkung |
|---------|-------|---------|
| nicht capturing, Cursor über Liste | `InternalMouseWheeled` → Hover/`SectionedListPanel` | scrollt |
| capturing | `OnRawXashKey` | Binding |
| Capture beendet | wieder `InternalMouseWheeled` | scrollt |

## BIND_AUDIT (Infrastruktur, kein Blocker)

`CSRETRO_KEYBOARD_CAPTURE_DEBUG=1 ./scripts/play.sh`

| Feld | Bedeutung |
|------|-----------|
| `BIND_AUDIT config_path=` | **`$XASH3D_BASEDIR/cstrike/config.cfg`** |
| `BIND_AUDIT userconfig_path=` | `$XASH3D_BASEDIR/cstrike/userconfig.cfg` |
| `BIND_AUDIT engine_exec rc=` | `cstrike.rc` → `config.cfg` → `userconfig.cfg` → `userconfigd` |
| `keynum=` | `MenuEngine::KeyNameToKeynum` — Buchstaben = lowercase Raw-Pfad (`W`→`w`=119, `c`=99). Kein `'W'`=87 |
| `canon=` | `KeynumToString(keynum)` — dieselbe Schreibweise wie `Host_WriteConfig` |
| `UI=…/hidden_third_plus` | Catalog-Bind existiert, aber nicht in Primary/Alternate (dritter+ Key) — **CATALOG_MAPPED**, nicht unmatched |

| `class=` | Bedeutung |
|----------|-----------|
| `CONFIG_ENGINE_MATCH+CATALOG_MAPPED` | CONFIG==ENGINE und UI-Katalog |
| `CONFIG_ENGINE_MATCH+CUSTOM_UNMATCHED` | CONFIG==ENGINE, nicht im Katalog — **preserve-safe** |
| `CONFIG_ENGINE_MISMATCH` | beide gesetzt, verschieden |
| `ENGINE_EMPTY` | Live-Engine ohne Bind; CONFIG darf Archiv-Rest haben |

Leere UI-Row ≠ Engine-Gap. `+csretro_*` = 3C-Dummies (`build/run-3c`).

## Test-Isolation

| Lauf | BASEDIR | GameData |
|------|---------|----------|
| `./scripts/play.sh` | `build/run` | **read-only** |
| Keyboard-/Phys-Gate | `build/run-gate/keyboard` bzw. `keyboard-phys` | read-only |
| Layout-Gate | `build/run-gate/layout` | read-only |
| `interactive-3c.sh` | `build/run-3c` | read-only |
| `interactive-menus.sh` | `build/run-menus` | read-only |

`play.sh` entfernt Gate-Marker und 3C-`+csretro_*` aus `build/run` (Backup: `cstrike/.gate-legacy/`). Override: `CSRETRO_PLAY_KEEP_FIXTURES=1`.
