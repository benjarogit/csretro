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

Die Brücke `xash_client` erhält die originalen Spielcallbacks und prüft die Identität vor dem Laden. Ihre Wrapper binden die registrierte Original-VGUI an Surface, verbinden native Panels/Popup-Flags, zeichnen nach HUD-Redraw, lesen Dateien/Fonts über die Engine und leiten UI-Mauszustandswechsel weiter. Die Cursorsynchronisierung kommt vor dem originalen VGUI-Scheduler; bei versteckter UI bleibt relativer Mausweg beim Spiel. Native Fixtures und echte Offscreen-Surface-Tests prüfen diese Komponenten einschließlich GL-Pixeln. Native GameUI-Laufzeitabnahme, Tastatur-/Fokusabnahme und tatsächliche NextClient-/Laufzeitbereitstellung bleiben offen; diese Tests führen Originalbibliotheken nie aus und bescheinigen weder Movement noch Spielbarkeit.

Ein gelieferter Zeiger, Factory-Aufruf, No-op oder aufgezeichnetes Primitive belegt nur diesen Vorgang und schließt keinen Gameplay-Meilenstein. Erhalte Fehlerfälle für fehlende Callbacks, falsche Architektur, gescheiterte Initialisierung, Eingabe, Kartenwechsel, Trennung und Shutdown.

## Tatsächliche Abhängigkeitssymbole prüfen

Passende ELF-Architektur und eine vorhandene DT_NEEDED-Bibliothek beweisen nicht, dass der Provider die Importe des Consumers bereitstellt. Insbesondere dienen die Steam-/tier0-/vstdlib-Hostbibliotheken des SDKs Komponentenbuilds und Probes; ihre Dateinamen machen sie nicht zum Ersatz für die lokal lizenzierte Originalruntime. Deren echte zusammengehörige Abhängigkeiten müssen gemeinsam bereitgestellt werden. Keine Steam-Platzhalterfunktionen oder geänderten Original-Gameplay-Exporte dürfen einen Ladefehler verdecken.

Mit `readelf -d` und der tatsächlichen Ladeumgebung den vollständigen Abhängigkeitsgraphen einschließlich passender Systembibliotheken ermitteln. Jeden dynamisch geladenen Consumer getrennt prüfen:

```bash
python3 tools/check_elf_imports.py CONSUMER.so \
  --provider DEPENDENCY_A.so --provider DEPENDENCY_B.so
```

Die großgeschriebenen Pfadplatzhalter ersetzen und `--provider` für jeden benötigten Provider wiederholen. Das Werkzeug lädt die Bibliotheken nie. Es prüft starke Importe, sichtbare Exporte, Symbolversionen, passende ELF-Metadaten und bekannte Typkonflikte; optionale schwache Importe dürfen fehlen. Der Providergraph wird ausdrücklich vorgegeben und als gültig vorausgesetzt: Das Werkzeug löst weder DT_NEEDED noch Suchpfade auf und beweist weder Ladereihenfolge, Relokationen, Signaturen, Steam-Authentifizierung noch Spielbarkeit. Ein Pass bleibt ein statischer Screen mit ungeprüfter ABI/Spielbarkeit. Ein beliebiger gleichnamiger Export beweist nicht den richtigen Provider.

VGUI1-Supportfehler sind eine getrennte Grenze. Eine Valve-VGUI1-Bibliothek ist nicht der FreeVGUI-Supportadapter von FWGS, und unsere originale VGUI2-Surface implementiert diese API nicht. Keine Support-Exporte erfinden. MainUIs `gfx/shell/btns_main.bmp` und `gfx/fonts` unterscheiden sich ebenfalls von VGUI2-Ressourcen; geprüfte Engine-Menü-/Fontdateien mit ihren Lizenzen an den echten virtuellen Pfaden bereitstellen. Verbleibende Warnungen im menschlichen Log erhalten und nicht als bestanden behandeln.

## Menschliche Spielabnahme

Automatisierung startet das Spiel nie. Sobald der echte Engine-/Client-Pfad vorbereitet ist, startet ein Mensch die separate installierte Kopie und dokumentiert:

1. Quellrevision und Artefakthashes; passende Engine-/Spiel-/Erweiterungsarchitektur; der Port verwendet weder Retail-`hw.so` noch einen Steam-Prozess. Dokumentiere lokal bereitgestellte Original-Spielbibliotheken. Ausschließlich x64 folgt als getrennte Phase.
2. Eine echte gerenderte Karte aus Originalinhalten, nutzbare Maus/Tastatur, Bewegung, Kollision, Schießen, HUD und Ton.
3. Eine lokale/Testserver-Sitzung mit Verbindung, Teamwahl, Rundenwechsel, Trennung und sauberem Beenden. Öffentliche Server brauchen zusätzlich echte Anmeldung.
4. Nach dem funktionierenden Port ausschließlich x64 und mindestens 60 FPS in einer benannten spielbaren Szene, mit Hardware, Auflösung, Renderer, Einstellungen, Messdauer und Framezeitnachweis. Menüleistung wird getrennt erfasst; das Produkt wird nicht auf 60 FPS begrenzt.
5. Zugehörige automatisierte Testergebnisse und grünen CI-Link sowie verbleibende Fehler. Projektnachweise stehen in `HANDOFF.md`, persönliche Pfade/Logs unter `.local/`.

Ein fehlender oder gescheiterter Punkt lässt die zugehörige Port- oder anschließende x64-/Leistungsphase offen. Nutze [Einstellungsberichte](../players/settings.md) und die [Funktionsübersicht](../players/features.md), wenn du das sichtbare Spielergebnis dokumentierst.
