# Menüs

Lebende Architektur. Rollen: `docs/ROLLEN.md`. Schnittstelle: `docs/SCHNITTSTELLEN.md`.
Phase: `docs/PHASEN.md` · Arbeitsdokument: `docs/PHASE3M.md`.

CS Retro ist **64-Bit-Desktop** (Linux x86_64, Windows 10+ x86_64, macOS ARM64/x86_64). Kein Android, iOS, Switch, Vita, keine Touch-/Mobile-UI.

## Endziel

Optik und Bedienung: klassisches **Steam Counter-Strike 1.6 mit VGUI2**, nicht WON-Hauptmenü und nicht Textmenü als Primär-UI.

Branding: **CS Retro**. Zusätzliche Funktionen nur in diesem Stil, und nur wenn das Backend existiert.

**Drei Ebenen (nicht gegeneinander ausspielen):**

| Ebene | Rolle |
|-------|--------|
| Original-CS-1.6 | visuelle und interaktive **Baseline**, nicht Funktionsdeckel |
| NextClient | funktionale **Basis** |
| CS Retro | moderne und zusätzliche Funktionen (Desktop/HiDPI/Responsive, ServerProfile, Module, …) |

Classic-Metriken = 100%-Referenz und Regressionstest; danach kontrollierte Skalierung (Linux/Windows/macOS, Auflösungen, HiDPI) als bewusste CS-Retro-Schicht — keine globale historische Proportional-Skalierung, die Controls nur aufbläht.

**Eine** Menü-Library:

```
Xash
 ├─ GetMenuAPI
 │      ↓
 │   CS-Retro GameUI / Hauptmenü
 │
 └─ MenuFactory
        ↓
    CreateInterface
        ↓
 GameMenuExports001
        ↓
 CS-Retro In-Game-VGUI
```

Kein dauerhaftes Nebeneinander aus Xash-MainUI, Textmenü, Ref-A-Touch, NextClient-GameUI-DLL und einem zweiten In-Game-Menü.

## Funktionale Quelle

`client/nextclient/gameui/` ist die **primäre funktionale Quelle**. Nicht von Null beginnen.

Erhalten (Verhalten): BasePanel, Server Browser, Options, Create Multiplayer (Server/Game/Bots), später Module.

**Options (erweiterbar):** klassische Seiten + CS-Retro/NextClient — u. a. Keyboard/Bindings, Mouse, Audio, Video, Voice, Multiplayer, Gameplay, HUD, Radar, Crosshair-Fine-Tuning, Network, später portierte NextClient-Funktionen. Feature-UI erst sichtbar, wenn das Backend existiert (keine toten Optionen). Classic Preferred / Reference Size **512×406** (`CsretroOptionsClassic`); content-driven grow bei Bedarf — kein permanenter 545-Slack.

Ersetzen (Anbindung): Steam-/GoldSrc-GameUI, `HWND`/`SetWindowLongPtr`, `next_engine_mini.dll`, NitroApi-/Steam-Bind, `-m32`, Win32-only-Libs, CEF außer später bewusstem Cross-Platform-Bedarf.

Ziel: **NextClient-GameUI-Verhalten → native CS-Retro/Xash-Desktop-UI.**

## Jetzt vs. Bootstrap (3C)

| Zustand | Hauptmenü | In-Game Team/Klasse/Buy/Radio |
|---------|-----------|-------------------------------|
| **3C-Baseline** (Release `v0.1.5`, bleibt gültig) | Xash-MainUI (`GetMenuAPI`, kein `CreateInterface`) | GoldSrc-`ShowMenu` + `titles.txt` (`_vgui_menus` 0) |
| **3M-Ziel** | CS-Retro-Menü-Lib, `GameMenu.res` + **TrackerScheme** (GameUI) | VGUI-Viewport über `GameMenuExports001` |
| **3M jetzt** (dieser Host) | CS-Retro-Lib (`-menu menu_amd64.so`) | Team/Klasse/Buy = `.res`-VGUI; Radio = `ShowMenu` |

`ShowMenu` wird **nicht gelöscht**. Es bleibt Kompatibilität für serverseitige Textmenüs, Plugins, später AMXX/Metamod. Es ist **nicht** die primäre CS-Retro-Team-/Buy-/Radio-Oberfläche.

Xash-MainUI ist nur Bootstrap, bis die eigene Lib lädt.

## VGUI2-Modell (Entscheidung)

**Kein** Steam-`vgui2.dll` / `vgui2.so` als Runtime.

`MenuFactory` ist nur der Factory-Weg der geladenen Menü-Lib, keine VGUI2-Implementierung.

**Reuse-Gate 2026-09-01: Variante V1** (`docs/PHASE3M.md`):

- vendorter VGUI2-Core + `vgui_controls` aus `ncl-hl1-source-sdk` **intern** in die eine Menü-Lib
- Backends: Surface/Input/System/Localize/Filesystem → Xash / plattformneutral
- NextClient-GameUI (Options/CreateMP/Browser) darauf portieren
- In-Game: `GameMenuExports001` auf demselben Core
- V1-Runtime-PoC **bestanden** unter Xash (zeichnen + Maus/Tastatur/TextEntry/Tab/Escape/Resize, FreeType-Glyphen): `./scripts/vgui-v1-poc-runtime.sh`

V1 zeichnet bereits. Der Text-/Rect-Bootstrap in `client/menu/` bleibt nur **pro UI-Bereich**, bis die jeweilige echte VGUI2-Rekonstruktion ihn ersetzt (Hauptmenü, Create, Team/Buy, …). Kein paralleles zweites GUI-Framework.

## Ressourcen

1. Steam-materialisiert: `gamedata/cstrike/resource/` (inkl. `UI/`), `gamedata/valve/resource/`
2. Bei Bedarf: `gamedata/platform/resource/` (TrackerScheme, Rahmen-Icons, Localization, Fonts) — **keine** Platform-Binaries, kein `vgui2.dll`, kein SteamAPI
3. CS-Retro-Overrides: `data/ui-overrides/` (Bootstrap kopiert darüber)
4. User-Configs: `XASH3D_BASEDIR`

Nicht distributieren. Steam bleibt read-only Quelle.

## Serverprofil

Ein Modell für Listen/LAN und Dedicated — speicher- und wiederverwendbar. Create-Game-VGUI2 ist zentrale Serverkonfiguration, keine UI-Sonderlösung:

```
ServerProfile
 ├─ Server     (Map, Mode, Hostname, Password, MaxPlayers/Slots, LAN)
 ├─ Gameplay   (GameRules, Round/Freeze/Buy, Team/FF/Balance, …)
 ├─ Bots       (über Bot-Konfigurationsschnittstelle; nicht dauerhaft ZBot-only;
 │              Quota/Difficulty/Team/… + implementationsspezifische Advanced)
 ├─ Modules    (optional; Core ohne Plugins lauffähig;
 │              Metamod / AMX Mod X: Aktivierung, Plugin-Auswahl, Profile;
 │              strukturierte Seiten für bekannte Plugins später möglich)
 └─ Advanced   (Server-CVars)
```

New Game / LAN und Dedicated konsumieren dasselbe Profil.

## Quellen (Priorität)

1. NextClient `gameui/` — Funktion
2. Lokale Steam-CS-1.6-Resources — Optik
3. Ref B — Menü-/VGUI-Referenz bereits in Phase 3M (In-Game-Verhalten / `.res`-Mapping); gezielte zusätzliche Feature-Ports später
4. Xash MenuAPI / MenuFactory
5. `kungfulon/fwgs-vgui2-support` — nur Forschung (`docs/UPSTREAM.md`)
6. Ghidra auf lokalen Original-Binaries — nur wenn 1–5 nicht reicht

## Nicht

- Touch-CFG als Desktop-Fallback
- Ref-A-mainui pauschal
- Steam-`vgui2` als Abhängigkeit
- nicht funktionierende Placeholder-Optionen (FOV/Crosshair erst mit Backend)
- parallele Menüsysteme für dieselbe Funktion
