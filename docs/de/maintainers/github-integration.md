# Geschützte GitHub-Integration

Normale Entwicklung nutzt native GitHub-Squash-Auto-Merges nach bestandener
zentraler [Policy](../../../.github/merge-policy.json). Die NextClient-Portierung
auf Xash3D bleibt der Entwicklungsauftrag. Kein automatischer Spielstart.

Das Gate liest wirksamen Branch-Schutz, CI-/Sicherheitsprüfungen zum aktuellen
Head, CodeRabbit-Freigabe, alle Review-Konversationen und eine aktuelle
Akzeptanzerklärung. Übersprungene, neutrale, fehlende oder veraltete Nachweise
reichen nicht. Der Schutz gilt für Administratoren, verlangt aktuellen Branch,
unabhängiges Review und gelöste Konversationen und verbietet Force-Push/Löschen.
Bestehende Tests bleiben in der vorhandenen CI. Der Koordinator führt nur
vertrauenswürdigen Main-Code aus, keinen privilegierten PR-Code.

Nach dem letzten Push diesen JSON-Kommentar in die PR-Beschreibung aufnehmen;
Platzhalter durch vollständigen Head-SHA und tatsächliche Kriterien/Belege ersetzen:

```text
<!-- chatdev-acceptance {"head":"FULL_HEAD_SHA","criteria":[{"criterion":"Acceptance criterion","evidence":"CI/test/report link","passed":true}],"human_runtime_required":false} -->
```

Das unabhängige freigebende Review von CodeRabbit oder einem berechtigten
Maintainer muss außerdem eine eigene Zeile `CSRETRO-ACCEPTANCE DIGEST` enthalten.
DIGEST ist SHA-256 der mit Python `json.dumps(sort_keys=True, separators=(',', ':'))`
kodierten JSON-Erklärung. Änderungen der Erklärung entwerten das Testat auch
bei gleichem Head. Reviews fremder öffentlicher Nutzer zählen dafür nicht.

CodeRabbit muss diese Aussagen prüfen. Ist menschliche Gameplay-Abnahme nötig,
bleibt der PR bis zum Nachweis gesperrt; sie darf nicht nur zum Bestehen des Gates
als unnötig erklärt werden. Text allein ist keine unabhängige Verifikation.

Releases, produktive Deployments, Kosten, Berechtigungsänderungen und andere
sicherheitsrelevante Änderungen brauchen ausdrückliche Maintainer-Freigabe zum
aktuellen Head. Sensible Pfade/Labels stehen zentral in der Policy. Neben einem
unabhängigen Maintainer-Review kann ein berechtigter Maintainer ausdrücklich
`CSRETRO-APPROVE-SENSITIVE FULL_HEAD_SHA` kommentieren. Das gilt nur für diesen
unveränderlichen Head; Labels oder vom Agenten vermutete Zustimmung zählen nicht.
Native unabhängige Review-Anforderungen gelten weiterhin, auch für eigene PRs.
Der entwickelnde Agent darf diese Freigabe nicht selbst herstellen.

Der Koordinator reagiert auf vorhandene CI-, PR- und Review-Ereignisse. Er
veröffentlicht `protected merge acceptance` und aktiviert natives Auto-Merge erst
nach Auflösung aller Sperren; kein Admin-Bypass, Force-Push oder ungeschützter
Merge. API-Fehler sperren. Fehler behebt weiterhin der aktive Entwicklungsagent
im vorhandenen Test-/Korrekturzyklus. Kein zusätzliches Modellabo und keine
besondere Runner-Klasse. Auch der erstmalige Bootstrap muss das Sicherheitsreview
bestehen, bevor der vertrauenswürdige Main-Koordinator existiert. Den Pflichtcheck
nicht vorübergehend entfernen.

Aktuelle Betriebsnachweise stehen in HANDOFF.md. Administrativ wurden strikter
Schutz, Auto-Merge-Verfügbarkeit, Abhängigkeitswarnungen/Sicherheitsupdates,
Secret Scanning und Push Protection aktiviert. Dependabot behält seine vorhandene
wöchentliche Actions-Gruppe. CodeQL verwendet den Python-Workflow aus PR #16;
das ist keine C++-Engine-Analyse. Dienstseitige CodeRabbit-Grenzen können Reviews
weiter verhindern; Konfiguration übersteuert keine Anbieterbedingungen.

Siehe [GitHub-Branch-Schutz](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches)
und [CodeRabbit-Konfiguration](https://docs.coderabbit.ai/reference/configuration).

## Native Prüfung und optionaler Check-Schreiber

Natives GitHub Auto-Merge ist für dieses öffentliche, persönlich geführte
Repository verfügbar und benötigt keine eigene App. Das aktive Ruleset `27257972`
erzwingt strikte quellgebundene Checks, aktuelles unabhängiges Review, gelöste
Konversationen, Squash/lineare Historie, Force-Push-/Löschschutz und CodeQL-
Befundschutz ohne Bypass-Akteure. Die native Scanning-Regel blockiert konfigurierte
Befunde, unvollständige Analyse und ein fehlendes Werkzeug. Kein zweiter Scanner;
der bestehende Branch-Schutz bleibt während der Validierung erhalten.

Native Checks formulieren keine beliebigen Projekt-Akzeptanzkriterien und erkennen
keinen bestimmten vertrauenswürdigen Actions-Workflow allein an seinem Checknamen.
GitHub akzeptiert auch übersprungene/neutrale Pflichtchecks; das beweist kein
ausgeführtes CodeRabbit-Review. Fest vorgeschriebene vertrauenswürdige Workflows
beschreibt GitHub auf Organisations-/Enterprise-Ebene; teamspezifische Pfadreviewer
stehen persönlich geführten Repositories nicht zur Verfügung. Zwei nicht erzwingende
Workflow-Regel-Proben per API ergaben HTTP 422, auch mit vorhandenem CI-Workflow.
Dieser Fehler allein beweist keine Tarifbeschränkung. Kein Probe-Ruleset entstand;
kein Organisationsumzug ist beauftragt.

Ohne App kann der vorhandene Administrator-Host die nativen Regeln prüfen,
unabhängige aktuelle Akzeptanz/Freigabe sensibler Änderungen bewerten und natives
Auto-Merge aktivieren. Keine neuen Zugangsdaten nötig; die zusätzliche Projekt-
Akzeptanzentscheidung bleibt dabei hostgesteuert und hat keinen eigenständig
authentifizierten Servercheck. Die vollständig automatische Variante mit solchem
Check bleibt deshalb ein optionaler, nicht genehmigter Entwurf. Der vorhandene
Akzeptanz-Pflichtcheck wird nicht entfernt, nur um einen Teiltest mergen zu können.

Bei Genehmigung wäre die App ausschließlich **Check-Schreiber**: Checks schreiben
und GitHubs verpflichtendes Metadata lesen, nur für CSRetro installiert. Der
vorhandene Host liest PR-/Review-Belege, prüft Regeln und aktiviert natives
Auto-Merge mit vorhandener Berechtigung. Die App benötigt keine Administration-,
Contents-, Pull-Request- oder Sicherheitswarnungsrechte und kann weder mergen noch
Code/Regeln ändern. Native CodeQL-Regeln sichern Befunde serverseitig. Bypass-Akteure
werden ohne Schreibzugriff auf das Ruleset ausgeblendet: Der vorhandene
Administrator-Host muss sie prüfen. Ein gewöhnlicher Actions-Token kann dabei
scheitern. Fehlende Prüfsicht sperrt den Koordinator; keine stille App-Aufrüstung.

Der Schlüssel läge ausschließlich als `CSRETRO_MERGE_APP_PRIVATE_KEY` im Environment
`csretro-merge-controller`, dessen serverseitige Branch-Regel nur Branch `main`
erlaubt, keine Tags oder anderen Branches. Die App-ID ist die nicht geheime
Environment-Variable `CSRETRO_MERGE_APP_ID`. Eine gepinnte offizielle Action erzeugt
einen Installations-Checktoken für den Job und widerruft ihn anschließend;
`GH_ACCEPTANCE_TOKEN` dient nur dem Erstellen/Aktualisieren von Checks. Vorhandene
Host-Berechtigung nutzt einen getrennten Token. Kein Schlüssel in repositoryweiten
Secrets, Quelltext, Logs, Caches oder PR-Jobs. Installation nur auf dieses Repository
begrenzen; bei Offenlegung Schlüssel widerrufen/rotieren, Environment-Secret ersetzen
und Negativtests wiederholen. Kein Schlüssel und keine App-Rechte wurden angelegt.
Die Einrichtung braucht weiterhin ausdrückliche Freigabe.

Vor Aktivierung sind echter Token-Zugriff, Main-Schlüsselisolation, Bypass-Prüfung,
Fehler-/Headwechsel-Sperren und CodeRabbit-Reviews nachzuweisen. Auto-Merge bleibt
bis dahin aus. Beim Bootstrap folgt #15 weiterhin auf #14; ein geprüfter lokaler
Host kann den ersten App-gebundenen Akzeptanzcheck ohne entfernte Pflichtchecks
liefern, während #16 den bestehenden Python-Scanner bereitstellt. Kein zweiter
Dauer-Orchestrator.

Quellen: [natives Auto-Merge](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/automatically-merging-a-pull-request),
[Regeln und Workflow-Geltungsbereich](https://docs.github.com/en/enterprise-cloud%40latest/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets),
[Scanning-Mergeschutz](https://docs.github.com/en/code-security/concepts/code-scanning/merge-protection),
[Sichtbarkeit der Rules-API](https://docs.github.com/en/rest/repos/rules#get-a-repository-ruleset).
