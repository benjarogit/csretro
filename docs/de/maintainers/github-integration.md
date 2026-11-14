# Unabhängige geschützte Akzeptanz

Normale Entwicklung nutzt natives GitHub-Squash-Auto-Merge nach sieben vorhandenen
quellgebundenen Checks, aktuellem unabhängigem Review und gelösten Konversationen.
Die NextClient-/Xash3D-Portierung bleibt das Ziel; hier startet kein Spiel.

`protected merge acceptance` ist ein gewöhnlicher schreibgeschützter Actions-Job.
Keine neue App, kein Schlüssel, Secret, `checks:write` oder privilegierter PR-Code.
Die vorhandene Actions-Identität authentifiziert den Dienst, keinen bestimmten
Workflow. Auch Workflow-Code unterliegt aktuellem unabhängigem Review und nativem
Schutz; die stärkere Unfälschbarkeit eines externen separaten Prüfers wird nicht
behauptet.

## An den Commit gebundene Belege

Jeder PR liefert `docs/evidence/acceptance.json` an seinem exakten Head:

- `schema`: 1; `base_sha`: tatsächlicher Basis-Commit des PRs.
- `human_runtime_required`: nur false, wenn keine menschliche Laufzeitabnahme nötig ist.
- `changed_paths`: alle aktuellen und alten umbenannten Pfade inklusive dieses Manifests.
- `criteria`: eindeutige `id`, konkrete `requirement` und `checks` mit tatsächlichen
  obligatorischen CI-/CodeQL-Kontexten aus der zentralen Policy. Keine erfundenen Tests.

Fehlende menschliche Abnahme wird nicht simuliert. Eine neue Basis oder ein geändertes
Manifest erfordert neues Review. Der Prüfer liest GitHub-Inhalte am unveränderlichen
Head; PR-Beschreibungen und inzwischen fortgeschrittene Branchnamen reichen nicht.

CodeRabbit zählt nur mit tatsächlicher Bot-Identität, Freigabe zum aktuellen Head und
erfolgreichem CodeRabbit-Lauf zu diesem Head. Übersprungene, neutrale, begrenzte,
abgebrochene, fehlgeschlagene, reine Zusammenfassungs- oder leere Antworten zählen
nicht. CodeRabbit muss alle geänderten Pfade und Kriterien unabhängig prüfen und ein
Review mit folgender Struktur abgeben:

```text
<!-- csretro-acceptance-review {"schema":1,"head":"PR_HEAD_SHA","acceptance_sha256":"DIGEST","decision":"pass","reviewed_paths":["all changed paths"],"summary":"Concrete independent assessment of the inspected changes","criteria":[{"id":"criterion-id","passed":true,"reason":"Concrete verified evidence for this criterion"}]} -->
```

DIGEST ist SHA-256 der UTF-8-kodierten kanonischen Python-Darstellung
`json.dumps(artifact, sort_keys=True, separators=(',', ':'))`. Der Autor liefert kein
Testat; der Koordinator erfindet keines. Spätere negative Bewertung oder widerrufene
Freigabe sperrt. Ein Statusname oder eine Freigabe ohne inhaltliche Kriterien-/
Pfadbelege genügt nicht.

Der konfigurierte CodeRabbit-Pre-Merge-Check prüft die Belege unabhängig. Automatische
Freigaben lehnen übersprungene Reviews und Freigabekommandos des Autors ausdrücklich
ab. Dienstseitige Verfügbarkeit, Kapazitäts- oder Tarifgrenzen bleiben wirksam.

## Native Integration und Ausnahmen

Der Akzeptanz-Workflow prüft bei PR-Änderungen und abgegebenen, bearbeiteten oder
widerrufenen Reviews erneut. Er hat nur Leserechte und keine Secrets. Ein Review-
Ereignis wiederholt keine teuren CI-Builds. Der redundante Review-Signalworkflow
entfällt. Alle übrigen CI- und CodeQL-Ergebnischecks bleiben eigene native Pflichten.

Der vorhandene berechtigte Host prüft wirksame Rulesets/Branch-Regeln, aktuelle
Checks, echte Review-Belege und Sensibilität mit `tools/github_merge_gate.py`.
`--publish` aktiviert jetzt ausschließlich natives `--auto --squash --match-head-commit`
nach bestandener Prüfung und schreibt keinen erfolgreichen Check. Ausgeblendete
Bypassdetails oder API-Fehler sperren; kein neuer Administrator-Token wird installiert.

Releases, produktive Deployments, Kosten, Rechte und andere sensible Änderungen
brauchen weiter ausdrückliche Maintainer-Freigabe zum aktuellen Head. Ein berechtigter
Maintainer kann reviewen oder `CSRETRO-APPROVE-SENSITIVE FULL_HEAD_SHA` kommentieren;
der entwickelnde Agent darf diese Freigabe nicht selbst herstellen. Unabhängige
Review-Anforderungen bleiben erhalten.

Auch der Bootstrap benötigt echte Reviews und alle Checks. Das Scanner-Fundament
von PR #16 wird ohne Import des gesamten gestapelten Branches und ohne schwächere
Gates abgeglichen; #15 bleibt von #14 abhängig. Ein reiner Doku-Test-PR gegen main
prüft die negative Sperre und erst nach echter Abnahme positives natives Auto-Merge.
Fehlende Scanner/Reviewer sind Blocker und kein bestandener Ende-zu-Ende-Test.
Auto-Merge bleibt bis zum echten positiven Test unter wirksamem Schutz aus.

Aktuelle Ergebnisse/CI-Links stehen ausschließlich in HANDOFF.md. Der Pre-Push-
Prüfer isoliert Hook-lokale Git-Variablen von verschachtelten Test-Repositories und
erhält dabei den ursprünglichen LFS-Kontext.

Quellen: [natives Auto-Merge](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/automatically-merging-a-pull-request),
[CodeRabbit-Freigaben und eigene Prüfungen](https://docs.coderabbit.ai/reference/configuration).
