# IDE- und Provider-Adapter

Der Koordinator arbeitet über die Repository-CLI und den gemeinsamen lokalen SQLite-Zustand. Er benötigt weder Codex noch Cursor, einen laufenden Editor, ein Modellkonto oder eine Modell-API. Ein Clientname kennzeichnet einen kooperierenden Leser/Schreiber; er wählt kein Modell und beweist keinen laufenden Agenten. Verwende einen kleingeschriebenen Bezeichner aus höchstens 48 ASCII-Buchstaben, Ziffern, Unterstrichen oder Bindestrichen, beginnend mit einem Buchstaben. Standard ist `cli`.

## Erkennen und übergeben

Im Repository-Wurzelverzeichnis:

```sh
scripts/chatdev.sh adapters
scripts/chatdev.sh --client external-ide export 1
scripts/chatdev.sh --client external-ide resume 1
```

Ersetze `1` durch eine vorhandene Aufgaben-ID. `adapters` meldet immer den eingebauten lokalen JSON-Transport. Optionale Programme werden nur per PATH-Suche für `codex`, `cursor-agent` und den Desktop-Starter `cursor` erkannt. Dabei werden diese Programme nicht ausgeführt, Anmeldungen nicht geprüft, keine Software installiert, keine fremden Sitzungen übernommen und keine Modellkontingente verbraucht. Ein erkannter Desktop-Starter belegt keine separat installierte Agent-CLI.

`export` schreibt eine versionierte Übergabe mit privaten Dateirechten unter `.local/chatdev/assignments/`. Der Beleg enthält einen repository-relativen Dateinamen und SHA-256. Identische Aufgaben-/Clientdaten verwenden dieselbe unveränderliche Datei; geänderter Zustand erzeugt eine neue Datei. Gleichzeitige Exporte veröffentlichen vollständige Dateien ohne bestehende Übergaben zu überschreiben. Geschützte Pfade, unsichere Clientnamen, ausbrechende Scope-/Quellpfade und Symlinks in verwalteten Verzeichnissen werden abgewiesen. Das ist kooperative Koordination, keine Dateisystem-Sandbox gegen andere Prozesse.

Schemaversion 1 enthält die gespeicherte Aufgaben-ID, Ziel, Modus, Zustand, ausgewählte Rollen, Scope und Quellfingerabdrücke sowie Verweise auf gemeinsame Datenbank und Statusdatei. Vollständige Chats, rohe GitHub-Snapshots und Rollenergebnis-Transkripte werden nicht übernommen. Die Übergabe erteilt keine Ausführungsrechte. Ein Verbraucher muss die Aufgabe fortsetzen, aktuelle Harness/Plan/Handoff lesen und vor Änderungen Datei-Claims erwerben. Gespeicherte Übergaben sind historische Momentaufnahmen; eine Datei allein beweist keine weiterhin gültigen Fingerabdrücke. Ergebnisse kommen weiterhin über `record` mit realen Akteuren und Nachweisen herein; unabhängige Qualitätssicherung und frische ausdrückliche Merge-Freigabe bleiben erforderlich.

## Optionale offizielle Anbindungen

Cursor dokumentiert seine [Agent-CLI](https://cursor.com/docs/cli/using), einschließlich MCP und ACP, sowie den [Headless-Print-Modus](https://cursor.com/docs/cli/headless). Das sind mögliche spätere Ausführungsschnittstellen, keine durch diesen Adapter installierten Funktionen. Die untersuchte lokale Hilfe des Desktop-Starters nennt einen Unterbefehl `agent`; das separate Programm `cursor-agent` war bei der Prüfung nicht vorhanden. Der Starter kann bei Nutzung dieses Unterbefehls Komponenten installieren; deshalb wurde er nicht aufgerufen. Kein Installer, keine Berechtigungsumgehung und keine Cursor-Hintergrundsitzung wurden gestartet.

Codex dokumentiert seine [CLI-Befehle](https://developers.openai.com/codex/cli/reference/), einschließlich nichtinteraktiver Ausführung. Lokal bestätigte `codex --help` die CLI und ihren Befehl `exec`, ohne einen Modellauftrag zu starten. Der Adapter erkennt nur das Programm. Dessen Vorhandensein beweist weder Anmeldung noch Modellverfügbarkeit, Schreibrechte oder Freigabe kostenpflichtiger Aufrufe.

Die Dateiübergabe ist in jedem Editor nutzbar, der JSON lesen kann. Sie ist kein ACP-/MCP-Server, keine Editor-Erweiterung, kein Modell-Runner und keine automatische Synchronisierung von Editorsitzungen. Beide Editoren können ausdrücklich dieselbe Repository-Aufgabe fortsetzen; keine bestehende Sitzung wird hinter dem Rücken des Benutzers verändert.

## Wiederverwendung für ein späteres Orqen-Projekt

Wiederverwendbare Grenzen sind das versionierte Übergabeschema, sichere Clientidentität, Fähigkeitserkennung, Aufgaben-/Nachweisdatensätze und gemeinsame Zustandsverweise. Ein ausdrücklich freigegebener Ausführungsadapter könnte diese Grenzen nutzen, ohne den Koordinator an einen IDE-Anbieter zu binden. Dieser Adapter bräuchte eigene dokumentierte Berechtigungen, Ergebnisprüfung, Abbruchverhalten und Kostenkontrollen.

CSRETROs Autoritätspfade, Gate-Semantik, Einschränkungen für Originalinhalte, Upstream-Register und Build-/Testbefehle bleiben das Profil dieses Projekts. Vor einer Installation anderswo muss ein konfigurierbares Projektprofil extrahiert und getestet werden; keine CSRETRO-Defaults nach Orqen kopieren und keinen zweiten autoritativen Plan anlegen. Diese Änderung bearbeitet, initialisiert oder installiert nichts in einem Orqen-Repository.
