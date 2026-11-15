# Projektmanager und Fortsetzung

Der eingebaute `project_manager` ist eine tatsächlich ausgeführte Koordinationsrolle, kein zusätzliches dauerhaft laufendes Modell. Die Rollenregistrierung umfasst fünfzehn Zuständigkeiten: die bisherigen vierzehn Host-Phasen plus den automatischen PM. Start/Resume/Status/Backlog rufen `ProjectManager.report` auf, speichern dessen Ergebnis und liefern es dem Orchestrator als `project_manager`. Der Bericht enthält den Meilenstein aus Handoff/Plan, verifizierte gegenüber nur erfassten Ergebnissen, Prioritäten, Abhängigkeiten, Blocker, Zusammenarbeit und nächste Aktion.

## Vorhandenen Arbeitsstand verwenden

```sh
scripts/chatdev.sh --client local-host status
scripts/chatdev.sh --client local-host continue
scripts/chatdev.sh assign 2 --priority P1 --depends-on 1
```

Tatsächlich vorhandene Aufgaben-IDs verwenden. `continue` aktualisiert GitHub, wählt eine offene Aufgabe und liefert `resume-existing` mit derselben ID. Es erzeugt kein neues Projekt, startet kein Modell und verändert keine IDE-Sitzung. Der Host setzt die Empfehlung in tatsächliche Arbeit um. Eine neu benötigte Repository-/CI-Rolle wird der vorhandenen Aufgabe hinzugefügt; die bisherige Qualitätsfreigabe wird entwertet. Endgültig abgeschlossene Aufgaben starten nicht erneut. Ohne offenen lokalen Eintrag wird zuerst der echte Abschnitt `Next task` in HANDOFF.md herangezogen, vor allgemeinen historischen Port-Zusammenfassungen. `--offline` lässt den Remote-Stand ausdrücklich ungeprüft.

`assign` speichert P0–P3-Prioritäten und lokale Abhängigkeiten. Unbekannte, eigene oder zyklische Abhängigkeiten werden atomar zurückgewiesen. Erfüllung verlangt aktuelle Nachweise und unabhängige Qualität, nicht nur den Status done. Rote Checks oder angeforderte Reviews führen zurück zur bestehenden Aufgabe. Die CI-Auswahl schließt abgeschlossene Branch-Historie und überholte Läufe aus; unbekannte Metadaten werden nicht als grün ausgegeben. Änderungen an Scope/Code/Autoritäten stufen den Fortschritt auf nur erfasst zurück.

Eine offene Integrations-PR hält die Fortsetzung bis zur Live-Prüfung des geschützten Auto-Merge gegen `.github/merge-policy.json` durch `tools/github_merge_gate.py` an. Das Merge-Gate bindet den Digest der Abnahmeerklärung über `CSRETRO-ACCEPTANCE <sha256>` an ein unabhängiges, genehmigtes Review des aktuellen Heads; spätere Erklärungsänderungen entwerten diesen Nachweis. Der PM empfiehlt `verify-protected-auto-merge`; aus gespeicherten Checks bescheinigt er weder wirksamen Schutz noch Abnahme oder Autorisierung. Fehlender Schutz blockiert den Merge. Sensible Labels aus der zentralen Richtlinie, bei deren Ausfall mit konservativer Ersatzliste, führen zur Empfehlung einer ausdrücklichen, abgegrenzten Maintainer-Freigabe; das vertrauenswürdige Merge-Gate klassifiziert die tatsächlich geänderten Pfade verbindlich. Releases, produktive Deployments, kostenpflichtige Aktionen, kritische Berechtigungen und weitere sicherheitsrelevante Änderungen benötigen weiterhin ausdrückliche Freigabe. Das vorherige Gate nicht überspringen, um G07-Code zu beginnen. Nach Abschluss der Voraussetzung setzt der Host die dokumentierte Aufgabe fort, ohne fertige Implementierung zu wiederholen.

## Gemeinsame Clients ohne Anbieterbindung

Jede sichere Client-Kennung kann das Protokoll verwenden; Standard ist `cli`. Cursor/Codex sind optionale Nutzer, keine Abhängigkeiten. Beide verwenden im selben Checkout die ignorierte `.local/chatdev/memory.sqlite3`. PM-Berichte haben monoton steigende IDs; eine Prozesssperre serialisiert SQLite-Erfassung und atomare Veröffentlichung von `status.json`. Letzte Client-Beobachtungen belegen keine laufenden IDE-Agenten. Die letzten 100 abgeleiteten Berichte bleiben erhalten; Aufgaben, Ergebnisse und Wissen liegen getrennt dauerhaft vor.

Der [Adapter-Leitfaden](chatdev-adapters.md) beschreibt Dateiaustausch, optionale offizielle Schnittstellen und die Wiederverwendungsgrenze für Orqen. Installer, Modellaufrufe und Änderungen laufender Sitzungen sind nie implizit. Die Implementierung verwendet Linux/POSIX-Prozesssperren; diese Grenze wäre vor einem späteren Windows-Port anzupassen.

## Validierung

Unit-Tests prüfen PM-Entscheidungen und Nachweise. Integrationstests beobachten echte PM-Aufrufe des Koordinators, lesen persistierte Ergebnisse und starten getrennte CLI-Prozesse mit Client-Kennungen. Parallele Veröffentlichung, frische CI-Zuweisung, Merge-Halt, Abhängigkeitszyklen und veraltete Nachweise werden geprüft. Das belegt Protokoll und ausgeführten PM, keine laufende Cursor-/Codex-Modellsitzung. Tatsächliche Testzahlen und CI-Links stehen in HANDOFF.md.
