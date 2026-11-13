# Nachweisen, dass der Client läuft

Portiere zuerst das bestehende NextClient-Verhalten auf Xash3D. Erhalte Counter-Strike-Movement und Spielverhalten, statt CS neu zu entwickeln oder zusätzliche Funktionen aus anderen Clients vorzuziehen. Der historische i386-Spike stellt die Original-Spielbibliothek aus der eigenen lizenzierten Installation bereit. Alle Bibliotheken dieses Laufs müssen zur Engine-Architektur und deren Schnittstellen passen; Xash3D ersetzt die Valve-Engine, nicht automatisch die CS-Spiellogik.

Nach dem funktionierenden Port folgen ausschließlich x64 und der Nachweis von mindestens 60 FPS in einer echten Szene. Zusätzliche Funktionen kommen danach. Ein laufender i386-Port beweist die spätere x64-Phase nicht; die spätere x64-Anforderung darf aber nicht dazu führen, den vorhandenen Portpfad zu überspringen.

## Spielbibliothek ohne Start untersuchen

Führe im Repository-Wurzelverzeichnis aus:

```sh
python3 tools/check_game_client.py pfad/zum/spielclient.so
```

Dies ist eine statische Vorprüfung für die spätere x64-Phase, keine Voraussetzung für den ersten architekturgleichen Port. Geprüft werden ELF64/x86_64 als Shared Library und die von CSRETRO geforderten benannten Kern-Callbacks. Das Werkzeug liest mit `nm` die dynamische Symboltabelle; es lädt oder startet die Bibliothek nie. Exit 1 bedeutet fehlgeschlagene Prüfung oder unlesbares Artefakt. `CreateInterface` allein ist der Erweiterungs-Factory-Vertrag, nicht der Spielclient-Vertrag. Ein alternativer `F`-/`GetClientAPI`-Einstieg braucht eine eigene Tabellen-/ABI-Prüfung; dieses Werkzeug bescheinigt ihn nicht.

Exit 0 belegt nur diese Kern-Exporte. Damit sind weder Callback-Signaturen noch ABI-Layouts, laufende Engine, wirksame Funktionskörper, Gameplay, Anmeldung oder FPS bewiesen. Im JSON bleibt `playability` immer `unverified`. Die Tests nutzen native künstliche Fixtures, darunter eines, das beim Laden abbrechen würde.

## Integration prüfen

Die Presets `linux`/`linux-tests` bauen/testen x64-Komponenten; `linux-port`/`linux-port-tests` liefern die ersten i386-Komponenten. Siehe [Buildprofile](../build/client.md). Keines stellt bereits die vollständige Laufzeit zusammen. Halte Port-Artefakte architekturgleich und dokumentiere ihren Buildpfad getrennt. Prüfe die tatsächlichen Shared-Artefakte und nicht nur statische Testbibliotheken. Eine echte Engine muss die Spiel-Callbacks erhalten und ihre eigene Engine-Tabelle übergeben. Herkunft und passende Lizenz des nativen Spielcodes werden vor einem Quellimport dokumentiert. Die zulässigen Upstream-Grenzen stehen im [Plan](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

Die Brücke `xash_client` erhält jetzt die originale `F`-Callback-Tabelle und prüft die Spielidentität vor dem Laden. Ihre nativen Fixture-Tests prüfen Weitergabe und Fehlerfälle. Original-VGUI-Bindung und tatsächliche NextClient-/Laufzeitbereitstellung fehlen weiterhin; diese Tests starten das Originalspiel nicht und bescheinigen dessen Movement nicht.

Ein gelieferter Zeiger, Factory-Aufruf, No-op oder aufgezeichnetes Primitive belegt nur diesen Vorgang und schließt keinen Gameplay-Meilenstein. Erhalte Fehlerfälle für fehlende Callbacks, falsche Architektur, gescheiterte Initialisierung, Eingabe, Kartenwechsel, Trennung und Shutdown.

## Menschliche Spielabnahme

Automatisierung startet das Spiel nie. Sobald der echte Engine-/Client-Pfad vorbereitet ist, startet ein Mensch die separate installierte Kopie und dokumentiert:

1. Quellrevision und Artefakthashes; passende Engine-/Spiel-/Erweiterungsarchitektur; der Port verwendet weder Retail-`hw.so` noch einen Steam-Prozess. Dokumentiere lokal bereitgestellte Original-Spielbibliotheken. Ausschließlich x64 folgt als getrennte Phase.
2. Eine echte gerenderte Karte aus Originalinhalten, nutzbare Maus/Tastatur, Bewegung, Kollision, Schießen, HUD und Ton.
3. Eine lokale/Testserver-Sitzung mit Verbindung, Teamwahl, Rundenwechsel, Trennung und sauberem Beenden. Öffentliche Server brauchen zusätzlich echte Anmeldung.
4. Nach dem funktionierenden Port ausschließlich x64 und mindestens 60 FPS in einer benannten spielbaren Szene, mit Hardware, Auflösung, Renderer, Einstellungen, Messdauer und Framezeitnachweis. Menüleistung wird getrennt erfasst; das Produkt wird nicht auf 60 FPS begrenzt.
5. Zugehörige automatisierte Testergebnisse und grünen CI-Link sowie verbleibende Fehler. Projektnachweise stehen in `HANDOFF.md`, persönliche Pfade/Logs unter `.local/`.

Ein fehlender oder gescheiterter Punkt lässt die zugehörige Port- oder anschließende x64-/Leistungsphase offen. Nutze [Einstellungsberichte](../players/settings.md) und die [Funktionsübersicht](../players/features.md), wenn du das sichtbare Spielergebnis dokumentierst.
