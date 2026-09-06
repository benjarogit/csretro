# Game-Data

CS Retro setzt originale Valve-/CS-1.6-Daten **nicht** im Git und nicht in der Distribution voraus.

Der Benutzer braucht eine lokale Steam-Installation von Counter-Strike 1.6 (AppID `10`, InstallDir `Half-Life`, GameDir `cstrike`). Steam ist nur die **Quelle** für diese Daten. Danach ist Steam keine Runtime.

```
Steam-CS-1.6  (read-only)
    → Manifest anwenden
    → CS-Retro-Datenbaum
    → bereinigen / REPLACE
    → Xash startet nur mit diesem Baum
```

`XASH3D_RODIR` zeigt auf den CS-Retro-Baum, nie auf `steamapps/common/Half-Life`.

## Bootstrap

```bash
python3 ./scripts/bootstrap-gamedata.py
python3 ./scripts/bootstrap-gamedata.py --status
python3 ./scripts/bootstrap-gamedata.py --refresh   # nur wenn Steam CS 1.6 sich geändert hat
```

Manifest (versioniert, im Repo): `data/gamedata-manifest.json`  
Beobachtete Dateizugriffe (2026-09-01, dedicated+listen `de_dust`): `data/gamedata-observed.json`

Die Steam-Installation wird **nie** geschrieben.

## Wo der Baum liegt

| Betrieb | Pfad |
|---------|------|
| Entwickler-Repo | `<repo>/gamedata/` (nicht im Git) |
| Override | `CSRETRO_GAMEDATA` |
| Linux-Release | `$XDG_DATA_HOME/csretro/gamedata` bzw. `~/.local/share/csretro/gamedata` |
| Windows-Release | `%LOCALAPPDATA%\CS Retro\gamedata` |
| macOS-Release | `~/Library/Application Support/CS Retro/gamedata` |

User-Daten (configs) liegen getrennt: `XASH3D_BASEDIR` (Entwicklung: `build/run/`) bzw. der plattformübliche `user/`-Pfad. Ein Re-Import überschreibt sie nicht.

Herkunft des aktuellen Baums: `gamedata/.csretro-origin.json` (Steam-Root, Library, ACF-Hash, kopierte Relativpfade).

## Steam-Erkennung (kein SteamAPI)

1. Steam-Root: Plattformpfade + `CSRETRO_STEAM_ROOT` / `STEAM_DIR`.
2. `steamapps/libraryfolders.vdf` (und `config/libraryfolders.vdf`) lesen.
3. Alle Libraries.
4. `appmanifest_10.acf` suchen.
5. `installdir` (typisch `Half-Life`) → `steamapps/common/<installdir>`.
6. `valve/` und `cstrike/` müssen existieren.

Windows zusätzlich: Registry `SteamPath`. Linux: inkl. Flatpak-Pfad. macOS: `~/Library/Application Support/Steam`.

## Klassen

**COPY** — beobachtete Content-Gruppen und Dateien (Maps, Models, Sounds, Sprites, Events, gfx, resource, WADs, `delta.lst`, `titles.txt`). `valve/liblist.gam` bleibt HL-Metadaten. Gruppen, von denen der Trace nur ein Verzeichnis sah (`valve/maps`, `valve/media`), werden nicht kopiert. Für VGUI2 zusätzlich **`platform/resource/`** (TrackerScheme, Icons, Localization) — nicht `platform/steam`, nicht Platform-Binaries, **nicht** `platform/resource/linux_fonts` (CS Retro liefert eigene Schriften).

**REPLACE** — `cstrike/dlls`, `cstrike/cl_dlls`, `valve/cl_dlls`, `valve.rc` / `cstrike.rc`, **`cstrike/liblist.gam`**. GameDLL/Client sind CS-Retro; `liblist.gam` ist CS-Retro-owned (Name „CS Retro“, `gamedll_linux "dlls/cs.so"` — Xash hängt `_amd64` an).

**IGNORE** — Steam-/GoldSrc-Binaries (`*.so`/`*.dll`/`*.exe`, `hl_linux`, `hw.so`, `steam_api`, CEF), `platform/steam`, `platform/servers`, `platform/config`, `platform/gl_shaders`, `redist/`, `cstrike_hd/`, Steam-`config.cfg`, `steam.inf` / `steam_appid.txt`.

**OVERRIDE** — `data/ui-overrides/` (Git) wird nach der Steam-Kopie über `gamedata/` gelegt (Branding, zusätzliche Menüeinträge, **`platform/resource/csretro_fonts/`** mit Noto Sans, **Hauptmenü-Hintergrund** `cstrike/resource/background/csretro.png`, **`cstrike/autobuy.txt`** / **`rebuy.txt`**). Originalressourcen bleiben unangetastet.

**VALIDIERTE REPARATUR** — Steam liefert `cstrike/sprites/hud.txt` mit dem alten Kopfzähler `215`, aber 190 vollständigen Sprite-Datensätzen. Der Bootstrap validiert jeden Datensatz auf sieben Felder und korrigiert ausschließlich diesen bekannten Zustand im privaten CS-Retro-Datenbaum auf `190`. Bei jeder anderen Abweichung bricht er ab. Der Engine-Parser bleibt streng; die Steam-Installation bleibt unangetastet.

Steam-Schriften (`platform/resource/linux_fonts`) werden nicht importiert; bestehende Bäume räumt die `prune`-Regel im Manifest auf. Dasselbe gilt für die Steam-Menükacheln (`cstrike`/`valve` `resource/background/`, `BackgroundLayout.txt`): CS Retro liefert ein eigenes Motiv unter `data/ui-overrides/cstrike/resource/background/csretro.png`.

## Aufräumen: `ignore` gegen `prune`

Zwei Ebenen, beide deklarativ in `data/gamedata-manifest.json`:

| Regel | Wirkung |
|---|---|
| `ignore` | Datei wird gar nicht erst importiert |
| `prune`  | Pfad wird im Zielbaum gelöscht — für Bäume aus früheren Importen, als die `ignore`-Regel noch fehlte. Ein Refresh überschreibt nur, er räumt nicht ab |

Neue Regel = Manifest-Eintrag, kein Codeanbau. Was `prune` entfernt hat, steht in `origin.json` unter `pruned`.

**Nicht ins Blaue löschen.** `gamedata/valve` ist mit rund 200 MB der größte Block, aber CS-Maps greifen darauf zurück: `halflife.wad` wird von 22 der 25 Maps referenziert, und selbst `xeno.wad` von zweien. Welche `valve`-Assets wirklich entbehrlich sind, braucht eine Auswertung der tatsächlichen Referenzen (BSP-`wad`-Keys, Modelle, Sounds) — bis dahin bleibt der Baum vollständig.

Dasselbe gilt für alte Team-/Class-/Buy-Grafiken: Sobald die jeweilige Fläche vollständig auf reale `.mdl`-Vorschauen oder eigene Controls umgestellt ist, werden ihre Verweise zuerst baumweit geprüft und nur nachweislich unbenutzte Dateien per Manifest-`prune` entfernt. Die bereits genutzten Player- und `p_*.mdl` bleiben die gemeinsame, animierte Laufzeitquelle; parallele Standbilder werden nicht als Fallback konserviert.

Seit der Studioaufstellung der Class-Wahl werden `leet`/`arctic`/`guerilla`, `gsg9`/`sas`/`gign` sowie `t_random`/`ct_random` unter `cstrike/gfx/vgui` nicht mehr importiert und aus bestehenden Bäumen entfernt. `terror.tga` und `urban.tga` bleiben vorläufig erhalten, weil das Buy-Menü sie noch nachweislich nutzt; sie folgen erst mit dessen Modellumbau.

Nicht als Runtime übernehmen: Valve-Engine, `client.dll`/`client.so`, Steam-`cs_amd64.so`, `hw.dll`, SteamAPI.

## Updates

Ändert sich `appmanifest_10.acf` (Hash), sagt `--status` das. `--refresh` kopiert nur Steam-sourced Dateien neu. `csretro_owned` (unsere Libs, `.rc`, ZBot-Profile, `.nav`) bleibt erhalten. User-`config.cfg` in BASEDIR bleibt erhalten.

## Test

```bash
python3 ./scripts/bootstrap-gamedata.py
./scripts/smoke-gamedll.sh dedicated
./scripts/interactive-3c.sh
```

RODIR muss `gamedata` sein. ZBot-Testdaten liegen im Baum (`BotProfile.db`, `de_dust.nav`), nicht in Steam.
