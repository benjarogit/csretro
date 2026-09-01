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

**COPY** — beobachtete Content-Gruppen und Dateien (Maps, Models, Sounds, Sprites, Events, gfx, resource, WADs, `delta.lst`, `titles.txt`, `liblist.gam`). Gruppen, von denen der Trace nur ein Verzeichnis sah (`valve/maps`, `valve/media`), werden nicht kopiert.

**REPLACE** — `cstrike/dlls`, `cstrike/cl_dlls`, `valve/cl_dlls`, `valve.rc` / `cstrike.rc`. Dort liegen CS-Retro-GameDLL, CS-Retro-Client, stuffcmds.

**IGNORE** — Steam-/GoldSrc-Binaries (`*.so`/`*.dll`/`*.exe`, `hl_linux`, `hw.so`, `steam_api`, CEF), `platform/`, `redist/`, `cstrike_hd/`, Steam-`config.cfg`, `steam.inf` / `steam_appid.txt`.

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
