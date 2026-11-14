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

## Vertrauenswürdigen Prüfer einrichten

Auto-Merge bleibt deaktiviert, bis eine eigene GitHub App installiert und getestet
ist. Die gemeinsame Actions-App-ID beweist nicht, welcher Workflow einen Check
erstellt hat. Benötigt werden Administration lesen (Schutzregeln), Checks schreiben,
Contents schreiben (nativer Merge), Pull Requests schreiben und Code-Scanning-
Warnungen lesen. Diese Berechtigungs-/Schlüsseleinrichtung benötigt ausdrückliche
Maintainer-Freigabe. Keine Rechte für beliebige PR-Jobs und kein Ersatz durch die
allgemeine Actions-Identität.

Den App-Schlüssel ausschließlich als Environment-Secret
`CSRETRO_MERGE_APP_PRIVATE_KEY` und die App-ID als `CSRETRO_MERGE_APP_ID` in
`csretro-merge-controller` hinterlegen. Dessen serverseitige Branch-Regel erlaubt
nur Branch `main`, keine weiteren Branches oder Tags. Kein repositoryweites Secret:
PR-Workflows desselben Repositories könnten es lesen. acceptance_app_id in der
zentralen Policy und den obligatorischen Akzeptanzcheck an diese App binden, bevor
Auto-Merge wieder aktiviert wird. Ein unprivilegierter Review-Signaljob weckt den
vertrauenswürdigen Koordinator; kein Codeprüfen, keine Checks, keine Secrets dort.
Eigene Check-Abschlüsse lösen keinen neuen Koordinatorlauf aus. Einen echten Lauf
mit App-Token und einen gesperrten PR testen, bevor der Betrieb als nachgewiesen
gilt; fehlende API-Rechte bleiben blockierend.

Der Bootstrap ist eine gesondert geprüfte Sicherheitsoperation. Vor Aufnahme des
Koordinators in main kann der geprüfte lokale Koordinator mit genehmigtem App-Token
den Akzeptanzcheck veröffentlichen; kein Ersatz durch gemeinsame Actions-Checks.
PR #16 enthält bereits den Python-Scanner und kann unter denselben vollständigen
Gates das erste Workflow-Fundament liefern. Die Reihenfolge muss ausdrücklich
abgeglichen werden; #15 folgt immer #14. Die lokale Brücke wird weder zweiter
Dauer-Orchestrator noch Schutzregel-Umgehung.
