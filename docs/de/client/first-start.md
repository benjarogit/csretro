# Erster Client-Start

Diese Prüfung belegt G07d: Der Client erkennt den Public-Build, patcht `hw.so`,
öffnet die Konsole und stürzt nicht ab. Sie wird von Hand ausgeführt; CSRETRO
startet das Spiel nie selbst.

## Voraussetzungen

- Counter-Strike 1.6 aus der eigenen Steam-Installation (App 10)
- Steam ist installiert; der Starter startet es bei Bedarf und wartet darauf
- Der Client-Build ist x86_64 (`scripts/doctor.sh`) und wird nicht in die i386-`hw.so` geladen

## Schritte

1. Spielkopie **außerhalb** der Steam-Library anlegen:

   ```bash
   cp -a "$HOME/.local/share/Steam/steamapps/common/Half-Life" "$HOME/csretro-g07d"
   ```

   Der Pfad zur Steam-Library kann abweichen. Niemals im Steam-Ordner
   installieren.

2. Client bauen und in die Kopie installieren:

   ```bash
   ./scripts/build-client.sh --install-dir "$HOME/csretro-g07d"
   ```

   Das installiert acht Bibliotheken, `nextclient.sh` und die Assets, legt
   Valves `vgui2.so` als Sicherung ab und ersetzt `libtier0.so` durch die
   SDK-Variante. Valves Original bleibt als `libtier0.so.valve` daneben liegen.

3. Starten:

   ```bash
   cd "$HOME/csretro-g07d"
   ./nextclient.sh -windowed -console
   ```

## Erwartetes Ergebnis

| Beobachtung | Bedeutung |
|---|---|
| Konsole öffnet sich | Engine läuft |
| `Game build identified: public-linux-1.0.1.4` | SHA-256-Paar erkannt |
| keine Zeile `Unknown game build` | Registry passt |
| keine Zeile `Can't get engine address provider` | Engine-Hooks aktiv |
| kein Absturz vor der Konsole | `hw.so` gepatcht |

Meldet `nextclient.sh`, dass die Steam-Runtime fehlt, muss Steam installiert und
gestartet sein. Ein Start ohne Launcher ist für diese Prüfung nicht vorgesehen.

## Festhalten

- Exit-Code des Starts
- Konsolenauszug mit den Zeilen aus der Tabelle
- Logdatei unter `logs/` in der Spielkopie, falls vorhanden
- Auffälligkeiten: fehlende Hooks, Absturz, schwarzer Bildschirm

## Aufräumen

Die Kopie ist wegwerfbar:

```bash
rm -rf "$HOME/csretro-g07d"
```

## Sicherheit

- Nie in der Steam-Library installieren.
- Nie auf VAC-gesicherten Servern verbinden.
- Keine Steam-Zugangsdaten an CSRETRO geben.
