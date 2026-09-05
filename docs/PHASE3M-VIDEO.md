# Phase 3M — Options Video Dependency Matrix

Stand: 2026-09-03. Primäre funktionale Quelle: NextClient `COptionsSubVideo`. Backend: Xash-FWGS CVars / MenuAPI. Visuelle Referenz: Golden 5971. **Kein** Steam-HL25-HD-Dialog-Klon. FOV/Advanced ausgeklammert.

## Status (eindeutig)

| Gate | Status |
|------|--------|
| **Automated Video Gate** | **PASS** |
| **Mode-Safety (semi-auto A–G, nativer Desktop)** | **PASS** (CVars/UI/Combo/Reinit; Ergebnisse bleiben) |
| **Graceful Shutdown (normal + ASan/UBSan)** | **PASS** — `quit` → `wait` → exit 0; kein SIGABRT |
| **Overall Video Gate** | **PASS** |
| **Sanitizer** | runtime alignment reports fixed; clean shutdown **PASS** (nicht nur „0 UBSan“) |

### Overall Video Gate — Abnahme

- [x] clean graceful shutdown / no SIGABRT
- [x] echter Wanduhr-10s-Timeout (`CSRETRO_MODE_SAFETY_WALLCLOCK=1`, kein `ExpireConfirmNow`)
- [x] kurzer visueller Confirm/Reinit-Check (Keep + Timeout-Rollback)

Gemessene monotone Dauer (nativ, `DISABLE_GAMESCOPE_WSI=1`):

| Marker | Wert |
|--------|------|
| `confirm_shown_ms` → `timeout_fired_ms` | **delta_ms=10072** (Accept 9000…12000) |
| Engine-Wanduhr | 10:25:25 → 10:25:35 |
| `rollback_complete_ms` − shown | ~10104 ms |

Shots: `build/options-video-mode-safety-shots/` (`confirm_after_mode`, `options_after_keep`, `confirm_wallclock`, `options_after_timeout`).

Preferred Size 512×406 + Mouse/Audio/Video grün. **Kein Phase-3-Tag/Release. FOV/3D gesperrt.** Nächste Options-Subpage erst nach Vorlage.

### Automated Video Gate PASS umfasst

Layout · CVars · ComboBox/Menu-Hover · Mouse/Keyboard · Resolution-/Aspect-/DisplayMode-Control · Renderer-Anzeige · Brightness/Gamma/VSync · Apply/OK/Cancel/Reset · 640/800/1024/1366

**Nachkontrolle 2026-09-05:** Der frühere Scoreboard-Bildvergleich belegte nur die Engine-Wirkung außerhalb der Options. Er belegte keine sichtbare Vorschau bei geöffnetem Menü. Das Play-Log 07:32 zeigt einen Test auf der Titelseite; deren Wallpaper ist keine Spielwelt. Im Spiel blockierten zusätzlich `ui_renderworld=0` und das zwischengespeicherte Pause-Blur-Bild eine echte Vorschau.

Korrektur: Extended Menu API **2** meldet über `pfnNeedsWorldRender`, wann die Video-Seite die frische Szene benötigt. Die Engine rendert dann trotz offenem Menü; die Seite lässt das dunkle Blur-Bild während der Kalibrierung weg. `ui_renderworld` und Benutzerkonfiguration werden dafür nicht umgeschrieben. Beim Verlassen gilt wieder der normale Pause-Hintergrund. Gamma-Regler **1.8…3.0** entsprechend der Engine-Untergrenze. Die Titelseite erklärt die Vorschau auf einer Map. **Engine und Menü müssen gemeinsam neu gebaut werden.**

Der Video-Test sendet echte `KeyCodeTyped`-Nachrichten an die Slider und prüft die gesamte VGUI-Signalkette, Apply-Aktivierung, Cancel-Rollback und Übernahme. Das Pause-Gate öffnet Video im Spiel, verändert die Regler und schreibt `scrshots/video-live-world.png` sowie `scrshots/video-live-bright.png`; anschließend prüft es die Wiederherstellung des Blur-Hintergrunds. **Manueller Recheck offen.**

Build-Nachweis 2026-09-05: Engine und Menü erfolgreich neu gebaut; Video- und Pause-Gate @800×600 inklusive regulärem Exit 0 bestanden. Menü-Build ohne Diagnosen. Der Engine-Neubau meldet weiterhin 40 historische Compilerwarnungen (vollständiges Log dieses Laufs: `/tmp/csretro-video-engine-build.log`); diese werden nicht als behoben oder unterdrückt ausgegeben. Sichtbare Pause-Menütexte hinter den durchscheinenden Options gehören noch zum In-Game-Feinschliff.

Behoben (nicht mehr Blocker): VPANEL-Crash, Footer-Labels, AnimationDictionary-Shutdown-SIGABRT, Confirm-`Close()` (statt nur `MarkForDeletion`).

### Gates

| Script | Zweck |
|--------|--------|
| `scripts/vgui-options-video-gate.sh` | Automated Video (+ erweiterte Crash-Erkennung) |
| `scripts/vgui-options-video-mode-safety.sh` | Mode-Safety; nativ bevorzugt; `quit`+Exitcode |
| `scripts/vgui-graceful-shutdown-gate.sh` | Options+Combo → `quit` → wait exit 0 (Loops) |
| `scripts/gate-crash.sh` | gemeinsame Crash-Muster + `csretro_gate_wait_quit` |

Nativ: `DISABLE_GAMESCOPE_WSI=1` (Gamescope-WSI-Zenity ist **separat**, nicht VGUI-Abort).

Mode-Safety (schnell, Expire für Timeout-Pfad):

`CSRETRO_FOREGROUND=1 CSRETRO_MODE_SAFETY_ALLOW_FS=1 CSRETRO_VID_REINIT_TRACE=1 ./scripts/vgui-options-video-mode-safety.sh`

Overall / Wanduhr + Visual:

`CSRETRO_FOREGROUND=1 CSRETRO_MODE_SAFETY_ALLOW_FS=1 CSRETRO_MODE_SAFETY_WALLCLOCK=1 CSRETRO_VID_REINIT_TRACE=1 DISABLE_GAMESCOPE_WSI=1 ./scripts/vgui-options-video-mode-safety.sh`

## Quellen

| Quelle | Rolle |
|--------|--------|
| NextClient `OptionsSubVideo.cpp` | Control-Liste / Apply-Vertrag |
| Xash `vid_common.c`, `gamma.c`, `ref_common.c`, MainUI `VideoModes.cpp` | echte Backends |
| Golden 5971 Video-Shot | Classic-Struktur |
| `client/assets/.../OptionsSubVideo.res` | Ausgangslayout (gestript) |

## Matrix

| Control | NextClient / Original | Xash / CS-Retro Backend | Apply | Entscheidung |
|---------|----------------------|-------------------------|-------|--------------|
| Brightness | `brightness` 0…2 | `brightness` (`gamma.c`, ARCHIVE) | Live-Vorschau; Apply/OK committen | **keep** — Spielweltwirkung und Cancel-Rollback belegt |
| Gamma | `gamma` 1…3 | `gamma` ARCHIVE, Engine-Untergrenze **1.8** | Live-Vorschau; Apply/OK committen | **adapt** — UI **1.8…3.0**, keine tote Strecke unter dem Engine-Minimum |
| VSync | `gl_vsync` | `gl_vsync` ARCHIVE (kein VIDRESTART) | live CVar | **keep** — Control vorhanden + sichtbar (ypos 145); bei offenem Display-Mode-Dropdown vom Menu-Popup verdeckt (normal) |
| Resolution | `_setvideomode` + Modes via `IGameUIFuncs` | `width`/`height`/`vid_mode` + `vid_setmode` (FCVAR_VIDRESTART) · Modes: `pfnGetModeString` | transactional + Confirm | **adapt** |
| Display Mode | `Windowed` → `_setrenderer … windowed\|fullscreen` | `fullscreen` **0/1/2** (Windowed/FS/Borderless) | transactional + Confirm | **adapt+extend** |
| Aspect Ratio | Filter Normal/Wide | Filter auf reale Modes | UI only | **adapt** |
| Renderer | disabled in NC | `r_refdll` / `r_refdll_loaded`; Auswahl nur bei ≥2 Refs | Confirm wenn änderbar | **adapt** (sonst Anzeige) |
| Detail Textures | `r_detailtextures` | `r_detailtextures` (GL) | live CVar | **keep** — vorhanden + sichtbar (ypos 170); gleiches Popup-Verdecken wie VSync |
| Color Depth / HD Models / Addons / Low Detail / Multitexture / Stretch / Advanced·FOV / Nitro | — | kein sinnvolles Xash-Backend bzw. FOV gesperrt | — | **omit** |
| Keep-settings Confirm | fehlt in NC | MainUI 10s Yes/No | bei Mode/Display/Renderer | **neu** (QueryBox + Timeout-Rollback) |

## vid_setmode vs width/height/fullscreen

Xash: `vid_setmode` ist ein eingeschränktes Kommando; `width`/`height`/`fullscreen` sind native Renderinfo-/VIDRESTART-Werte.

**Kanonische Apply-Semantik in CS Retro:** `fullscreen` setzen, dann **ein** Reinit über `vid_setmode` → `R_ChangeDisplaySettings` → `R_SaveVideoMode` (setzt `host.renderinfo_changed` zurück). Kein zweites `VID_CheckChanges→VID_SetMode` erwartet. Nachweis: `CSRETRO_VID_REINIT_TRACE=1` (Logs `CSRETRO_VID_REINIT via=…`). Persistenz = `host_writeconfig` **nur nach Keep** (nicht nach Cancel/Timeout).

## Architekturregeln

1. Keine `user_game_config.ini`, keine `_set*` / `_restart`.
2. Classic Preferred Size bleibt 512×406; content-driven grow nur bei Bedarf.
3. Nach Mode-Reinit: ScreenSize / Zentrierung / Glyphs über Surface + `vgui_boot`-Recenter; Combo-Hover/Escape + OK/Cancel/Apply-Labels regressieren.
4. Persistenz = Xash ARCHIVE / RENDERINFO CVars — eine Wahrheit; abgelehnter Mode darf Neustart nicht überleben.
5. VPANEL an Message-Grenzen nur als `uint64` / `uintptr_t` — nie `SetInt` / `MESSAGE_FUNC_INT`.
6. Nach Cancel/Timeout: Runtime **und** UI (Resolution/Aspect/DisplayMode/Renderer) auf laufenden Zustand syncen.

## Sanitizer + Shutdown

**runtime alignment reports fixed; clean shutdown PASS.**

Nachgewiesen (`CSRETRO_POOL_AUDIT=1`):

- `sizeof(PanelAnimationMap)=48`, `alignof=8`, Alloc-Adressen im 0x30-Raster
- 29 alloc / 29 free (1× Free pro Alloc) im Dictionary-Dtor — **kein** `CClassMemoryPool::Clear()`-Blob-Walk mehr
- `CClassMemoryPool::Clear` (falls woanders): Walk = `AlignValue(m_Data)` + `nBlocks = m_NumBytes/m_BlockSize` (AddNewBlob-Vertrag)

Ownership: `static CPanelAnimationDictionary` in `GetPanelAnimationDictionary()`; Dtor Free't jede Map explizit; danach `~CUtlMemoryPool` nur Blob-free ohne zweites Destruct.

Gates: `scripts/vgui-graceful-shutdown-gate.sh` (normal + sanitize, 2+ Loops, exit 0).

## Gamescope WSI (getrennt)

Zenity „CreateSwapchainKHR… Hooking has failed“ = `VK_LAYER_FROG_gamescope_wsi` wenn `ENABLE_GAMESCOPE_WSI=1` auf nativem GL. **Nicht** der VGUI-Abort.

Gegenmaßnahme nativ: `DISABLE_GAMESCOPE_WSI=1` / `unset ENABLE_GAMESCOPE_WSI` (`scripts/headless-x11.sh` bei `CSRETRO_FOREGROUND=1`).

Mode-Safety primär nativ; Gamescope optional zusätzlich.

## Offene / getrennte Punkte

| Punkt | Befund |
|-------|--------|
| Graceful Shutdown / SIGABRT | **PASS** (Free-1:1 + quit/exit 0) |
| Mode-Safety CVars/UI/Combo | **PASS** |
| Wanduhr-10s + Visuell Confirm | **PASS** (delta_ms≈10072; Shots unter `build/options-video-mode-safety-shots/`) |
| Gamescope WSI Zenity | getrennt; nativ `DISABLE_GAMESCOPE_WSI=1` |
| Alignment Resolution/Renderer/Aspect/Display Mode | **OPEN** — Visual/Layout; Backend geschlossen. Global Visual Polish + Adaptive Layout |
| Brightness / Gamma sichtbare Wirkung | **AUTOMATED PASS / MANUAL RECHECK OPEN (2026-09-05)** — Echte Slider-Tastennachrichten erreichen die Engine; Apply wird aktiv, Cancel stellt zurück, Apply übernimmt. Pause-Gate zeigt die Welt bei offenen Options vor/nach Regleränderung (`video-live-world.png`, `video-live-bright.png`), visuell kontrolliert. Engine rendert per Menu-API-Anforderung trotz `ui_renderworld=0`; gespeicherter Blur verdeckt die Kalibrierung nicht mehr. Der frühere Scoreboard-Vergleich allein war kein Nachweis für diesen Options-Pfad. |

## Mode-Safety-Matrix (Stand Overall PASS)

| Test | Gamescope | Native Desktop |
|------|-----------|----------------|
| Windowed | PASS | PASS |
| Fullscreen Keep | — | PASS |
| Fullscreen Rollback | — | via E/F PASS |
| Borderless Keep | — | PASS |
| Borderless Rollback | — | PASS |
| Timeout (Expire) | PASS | PASS (semi-auto) |
| Timeout (Wanduhr) | — | **PASS** delta_ms=10072 |
| VGUI after reinit | PASS | PASS |
| Persistence | — | PASS (`video.cfg` nach Rollback windowed 800×600) |
| Clean shutdown | — | PASS |

Reinit: genau ein `vid_setmode` pro Apply; kein `VID_CheckChanges`-Zweitlauf.
