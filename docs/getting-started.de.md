# Einstieg

## Was du brauchst

- Einen 64-Bit-Desktop. **Linux x86_64 ist derzeit der praktisch erprobte Entwicklungsweg.**
- Eine eigene lokale Steam-Installation von **Counter-Strike 1.6 (AppID 10)**.
- Für einen Quellcode-Build die [Entwicklungswerkzeuge](build.md).
- Für den aktuellen Spielstand zusätzlich die benötigten experimentellen Granaten-Assets.

Windows x86_64 und macOS ARM64/Intel sind Plattformziele, aber hier noch keine als vollständig
getestet ausgewiesenen Installationswege. Mobile Geräte, Konsolen und 32-Bit gehören nicht zum Projektziel.

!!! warning "Noch kein vollständiger Download"
    Die vorhandenen GitHub-Releases sind ältere Quellcode-Stände, keine aktuelle Komplettinstallation.
    Molotov-/Incendiary-Modelle, Sounds und Sprites gehören nicht zur normalen CS-1.6-Installation.
    Ihre reproduzierbare Beschaffung und Weitergaberechte sind noch nicht vollständig dokumentiert.
    Ein erfolgreicher Build allein garantiert deshalb noch keinen startfähigen Datenbaum.
    Wir verlinken keine unklar lizenzierten Ersatzpakete.

## Spieldaten importieren

Der Bootstrap liest die Steam-Installation und erzeugt einen separaten Datenbaum.
Er verändert die Steam-Installation nicht.

```bash
python3 scripts/bootstrap-gamedata.py
python3 scripts/bootstrap-gamedata.py --status
```

Der Standard ist `gamedata/` im Repository. `CSRETRO_GAMEDATA` erlaubt einen anderen Zielpfad.
Bei Problemen mit der Erkennung kann `CSRETRO_STEAM_ROOT` den Steam-Root angeben.
Ein späteres `--refresh` aktualisiert importierte Daten; lies vorher die Ausgabe von `--status`.

Steam dient als Datenquelle, nicht als Engine des gestarteten CS-Retro-Prozesses.
CS Retro nutzt seine eigenen Engine-, Client-, Menü- und Serverbibliotheken.
Kopiere keine Steam-DLLs als Ersatz für diese Bibliotheken hinein.

## Starten

Nach Build aller vier Komponenten und vollständig vorbereiteten Spieldaten:

```bash
./scripts/play.sh
```

Das Startskript erwartet derzeit den Linux-x86_64-Build. Es legt Konfigurationen und Logs
unter `build/run/` getrennt von `gamedata/` ab. Sichere eigene Konfigurationen vor Experimenten.
Wähle Team und Klasse, öffne das Kaufmenü und beginne eine lokale Runde.

## Wenn etwas nicht funktioniert

Prüfe zuerst fehlende Daten und den [Projektstatus](status.md).
Beschreibe bei einem [Fehlerbericht](https://github.com/benjarogit/csretro/issues/new/choose)
die genaue Abfolge. Die relevante Session findest du über
`build/run/logs/play-latest.log`. Veröffentliche nur bereinigte Ausschnitte, keine kompletten
privaten Pfade, Serverpasswörter oder personenbezogenen Daten.
