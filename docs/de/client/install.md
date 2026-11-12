# Spielinhalte installieren

CSRETRO kopiert die Counter-Strike-1.6-Dateien, die der Client mountet, in ein
Verzeichnis außerhalb der Steam-Bibliothek. Die Kopie landet nicht im
Steam-Ordner, startet Steam nicht und startet das Spiel nicht.

Du brauchst eine retail Counter-Strike-1.6-Installation (App 10). Die Dateien
liegen nicht in diesem Repository. Das Ziel nie in die Steam-Bibliothek
legen und mit dem veränderten Client nie auf einen VAC-Server.

## Suche

Das Werkzeug liest eine Steam-Wurzel, dann `steamapps/libraryfolders.vdf`,
dann `steamapps/appmanifest_10.acf`. Die Fixture unter
`tests/install_content/fixtures/` ist das aktuelle Bibliotheksobjekt (`path`
plus `apps`) und die ältere String-Form derselben Datei. `installdir` im
Manifest muss ein relativer Ordner unter `steamapps/common` sein, der
`cstrike` und `valve` enthält. Die erste gültige App 10 in dieser Reihenfolge
ist die Quelle. Eine spätere Bibliothek ist kein Ersatz, wenn die erste ein
vollständiges Spiel ist.

| Ergebnis | Bedeutung |
|---|---|
| `steam_missing` | Die Wurzel hat kein Steam-Layout. |
| `game_missing` | Steam ist da, App 10 fehlt oder ist nicht mountbar. |
| `steam_metadata_invalid` | `libraryfolders.vdf` oder das App-Manifest ist kein gültiges KeyValues. |
| `destination_inside_source` | Das Ziel liegt in der Steam-Bibliothek oder im Spielbaum. |
| `destination_not_empty` | Das Ziel enthält schon Dateien. |
| `destination_unavailable` | Das Ziel ist eine Datei oder die Wurzel des Dateisystems. |

Es wird kein Pfad erfunden. Ein Fehlschlag schreibt keine Kopie.

`--steam-root` beschränkt die Suche auf diesen Baum. Der Schalter darf mehrfach
stehen. Ohne ihn gelten die Linux-Steam-Orte: `~/.steam/steam`,
`~/.local/share/Steam` und das Datenverzeichnis der Steam-Flatpak. Tests
übergeben eine nachgebaute Bibliothek und lesen keine echte Steam-Installation.

## Was kopiert wird

Das Ziel behält das Half-Life-Layout. Die Allowlist steht in
`tools/install_content.py`.

| Baum | Kopiert |
|---|---|
| `cstrike/` | Maps, Modelle, Sounds, Sprites, Grafik, Resource, Events, Overviews, Klassen, Logos, WAD-Dateien und die Root-Dateien, die das Spiel liest (`liblist.gam`, `delta.lst`, Menüs, Mapcycle, Motd) |
| `valve/` | Modelle, Sounds, Sprites, Grafik, Resource (darunter `marlett.ttf` und `BackgroundLayout.txt`), Media, Scripts, Logos, WAD-Dateien und dieselben Arten von Root-Dateien |
| `platform/resource/` | Lokalisierung und die Linux-Schriften |

Fehlt eine dieser gemounteten Gruppen, `marlett.ttf` oder
`BackgroundLayout.txt`, endet der Lauf mit `game_missing` und schreibt nichts.

## Was liegen bleibt

Steam- und VAC-Binaries, `hw.so` und die übrigen Engine-Binaries, die
Retail-Verzeichnisse `cl_dlls` und `dlls`, persönliche Konfigurationen
(`config.cfg`, `autoexec.cfg` und die anderen lokalen cfg-Dateien), Demos,
Screenshots, Caches, `htmlcache`, `cstrike_downloads`, `cstrike_hd`, fremde
Mods, `platform/config` und `platform/steam`.

## Befehl

```bash
scripts/install-content.sh --steam-root <steam-wurzel> --destination <verzeichnis-außerhalb-von-steam>
```

Erfolg gibt `ok copied=<anzahl>` aus. Das Spiel wird nicht gestartet. Das
Xash-Öffnen bekommt dieses Verzeichnis als Inhaltswurzel; siehe
[Xash-Launcher](xash-launcher.md). Die GoldSrc-Prüfung in
[Erster Client-Start](first-start.md) ist eine eigene Vollkopie, weil diese
Prüfung die Retail-Engine noch lädt.
