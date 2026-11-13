# Nachweisen, dass der Client läuft

Diese Anleitung trennt eine gebaute Erweiterung, ein offenes Oberflächenfenster und einen funktionierenden Spielclient. Die erste lauffähige Version braucht den gesamten Weg: native Spielimplementierung, Xash3D-Engine, Eingabe, gerenderte Karte, Spielsimulation und gemessene Bildrate.

## Spielbibliothek ohne Start untersuchen

Führe im Repository-Wurzelverzeichnis aus:

```sh
python3 tools/check_game_client.py pfad/zum/spielclient.so
```

Geprüft werden ELF64/x86_64 als Shared Library und die von CSRETRO geforderten benannten Kern-Callbacks. Das Werkzeug liest mit `nm` die dynamische Symboltabelle; es lädt oder startet die Bibliothek nie. Exit 1 bedeutet fehlgeschlagene Prüfung oder unlesbares Artefakt. `CreateInterface` allein ist der Erweiterungs-Factory-Vertrag, nicht der Spielclient-Vertrag. Ein alternativer `GetClientAPI`-Einstieg braucht eine eigene Tabellen-/ABI-Prüfung; dieses Werkzeug bescheinigt ihn nicht.

Exit 0 belegt nur diese Kern-Exporte. Damit sind weder Callback-Signaturen noch ABI-Layouts, laufende Engine, wirksame Funktionskörper, Gameplay, Anmeldung oder FPS bewiesen. Im JSON bleibt `playability` immer `unverified`. Die Tests nutzen native künstliche Fixtures, darunter eines, das beim Laden abbrechen würde.

## Integration prüfen

Baue mit dem Preset `linux` und teste mit `linux-tests`. Prüfe die tatsächlichen Shared-Artefakte und nicht nur statische Testbibliotheken. Eine echte Engine muss die Spiel-Callbacks erhalten und ihre eigene Engine-Tabelle übergeben. Herkunft und passende Lizenz des nativen Spielcodes werden vor einem Quellimport dokumentiert. Die zulässigen Upstream-Grenzen stehen im [Plan](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

Ein gelieferter Zeiger, Factory-Aufruf, No-op oder aufgezeichnetes Primitive belegt nur diesen Vorgang und schließt keinen Gameplay-Meilenstein. Erhalte Fehlerfälle für fehlende Callbacks, falsche Architektur, gescheiterte Initialisierung, Eingabe, Kartenwechsel, Trennung und Shutdown.

## Menschliche Spielabnahme

Automatisierung startet das Spiel nie. Sobald der echte Engine-/Client-Pfad vorbereitet ist, startet ein Mensch die separate installierte Kopie und dokumentiert:

1. Quellrevision und Artefakthashes; native x86_64-Engine und Spielbibliothek; der Port verwendet weder Retail-`hw.so` noch einen Steam-Prozess.
2. Eine echte gerenderte Karte aus Originalinhalten, nutzbare Maus/Tastatur, Bewegung, Kollision, Schießen, HUD und Ton.
3. Eine lokale/Testserver-Sitzung mit Verbindung, Teamwahl, Rundenwechsel, Trennung und sauberem Beenden. Öffentliche Server brauchen zusätzlich echte Anmeldung.
4. Mindestens 60 FPS in einer benannten spielbaren Szene, mit Hardware, Auflösung, Renderer, Einstellungen, Messdauer und Framezeitnachweis. Menüleistung wird getrennt erfasst; das Produkt wird nicht auf 60 FPS begrenzt.
5. Zugehörige automatisierte Testergebnisse und grünen CI-Link sowie verbleibende Fehler. Projektnachweise stehen in `HANDOFF.md`, persönliche Pfade/Logs unter `.local/`.

Ein fehlender oder gescheiterter Punkt lässt das Ziel der lauffähigen Version offen. Nutze [Einstellungsberichte](../players/settings.md) und die [Funktionsübersicht](../players/features.md), wenn du das sichtbare Spielergebnis dokumentierst.
