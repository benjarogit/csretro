# Phase 3M — Options Video Dependency Matrix

Stand: 2026-09-02. Primäre funktionale Quelle: NextClient `COptionsSubVideo`. Backend: Xash-FWGS CVars / MenuAPI. Visuelle Referenz: Golden 5971. **Kein** Steam-HL25-HD-Dialog-Klon. FOV/Advanced ausgeklammert.

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
| Brightness | `brightness` 0…2 | `brightness` (`gamma.c`, ARCHIVE) | live CVar | **keep** (Range wie NextClient) |
| Gamma | `gamma` 1…3 | `gamma` ARCHIVE | live CVar | **keep** |
| VSync | `gl_vsync` | `gl_vsync` ARCHIVE (kein VIDRESTART) | live CVar | **keep** |
| Resolution | `_setvideomode` + Modes via `IGameUIFuncs` | `width`/`height`/`vid_mode` + `vid_setmode` (FCVAR_VIDRESTART) · Modes: `pfnGetModeString` | transactional + Confirm | **adapt** |
| Display Mode | `Windowed` → `_setrenderer … windowed\|fullscreen` | `fullscreen` **0/1/2** (Windowed/FS/Borderless) | transactional + Confirm | **adapt+extend** (Combo statt Checkbox) |
| Aspect Ratio | Filter Normal/Wide | Filter auf reale Modes (4:3, 5:4, 16:9, 16:10, Other) | UI only | **adapt** |
| Renderer | disabled in NC | `r_refdll` / `r_refdll_loaded`; Auswahl nur bei ≥2 ermittelbaren Refs (Extended API optional) | Confirm wenn änderbar | **adapt** (sonst Anzeige) |
| Color Depth | versteckt / bpp | keine Bedeutung 64-Bit-Desktop | — | **omit** |
| HD Models | `_sethdmodels` | kein Xash-Äquivalent | — | **omit** |
| Addons Folder | `_setaddons_folder` | Launcher/GoldSrc | — | **omit** |
| Low Video Detail | `_set_vid_level` | kein CVar | — | **omit** |
| Disable Multitexture | `user_game_config.ini` | kein Engine-CVar | — | **omit** |
| Stretch Aspect | FileConfig | kein kanonisches CVar | — | **omit** |
| Detail Textures | `r_detailtextures` | `r_detailtextures` (GL) | live CVar | **keep** (sichtbar wenn sinnvoll) |
| Advanced / FOV | FOV-Dialog | FOV gesperrt | — | **omit** (Phase später) |
| Nitro FileConfig / `_restart` | private NC/Steam | — | — | **omit** — Xash-CVars + VIDRESTART |
| Keep-settings Confirm | fehlt in NC | MainUI 10s Yes/No | bei Mode/Display/Renderer | **neu** (QueryBox + Timeout-Rollback) |

## Architekturregeln

1. Keine `user_game_config.ini`, keine `_set*` / `_restart`.
2. Classic Preferred Size bleibt 512×406; content-driven grow nur bei Bedarf.
3. Nach Mode-Reinit: ScreenSize / Zentrierung / Glyphs über bestehende Surface + `vgui_boot`-Recenter.
4. Persistenz = Xash ARCHIVE / RENDERINFO CVars — eine Wahrheit.

## Gate

`scripts/vgui-options-video-gate.sh` — funktional + visuell @640/800/1024/1366; Sanitizer separat.
