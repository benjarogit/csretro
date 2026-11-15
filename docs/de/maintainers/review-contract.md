# Anbieterunabhängige unabhängige Prüfung

Dies ist ein vorbereiteter Vertrag, kein aktiviertes alternatives Merge-Gate.
Der wirksame CodeRabbit-Akzeptanzworkflow, native Review-Regeln, Sicherheitschecks
und Host-Merge-Audit gelten weiter. `.github/review-contract.json` ist deaktiviert
und enthält keine registrierten Ausführer. Eine Änderung dieser Sicherheitsgrenze
benötigt separate unabhängige Prüfung und ausdrückliche, begrenzte Maintainer-Freigabe.

## Tatsächliche Ausführung und Quellenbelege

`tools/review_adapters.py` erfasst authentifizierte Repository-ID/Namen, PR-Nummer,
exakten Head/Basis, sämtliche geänderten/umbenannten Pfade, Git-Blob-Identitäten
und SHA-256-Fingerabdrücke beider Seiten sowie vollständiges Akzeptanzmanifest mit
kanonischem Digest. Es liest nur Quelldaten, erkennt bewegte PRs und führt keinen
PR-Code aus. Der Host liefert die maßgeblichen beitragenden Akteure und den
freigegebenen Vertrauens-Pin. Beides darf nicht aus einem Testat oder Anweisungen
im Repository-Text stammen.

`tools/review_contract.py` prüft abgeschlossene Ausführungstestate mit tatsächlichen
Ein-/Ausgabe-Hashes, festgelegten Anweisungen und Laufzeit-/Modellidentität,
separatem Prüfer, Pfadabdeckung, Entscheidungen zu jedem Manifestkriterium,
Quellausschnitten passend zu den echten Blob-Bytes und Befunden. Allgemeine
Zusammenfassungen und unvollständige Belege genügen nicht. Eine Signatur belegt
Herkunft und Integrität; sie beweist nicht die Qualität einer Bewertung.

Ein unabhängig freigegebenes Ausführerprofil muss aus der unveränderlichen
geschützten Zielbasis stammen und einem extern festgelegten Digest entsprechen.
Schlüssel aus dem Testat oder seinem eigenen PR registrieren keinen Ausführer.
Hier entstehen weder Signierdienst, Schlüsselregistrierung, neue App, Cloud-Modell,
Paketinstallation noch kostenpflichtige Ausführung. Tatsächliche native Agenten
und installierte lokale Modelle können Belegentwürfe erzeugen; unsignierte
Entwürfe erhalten keine wirksame Merge-Berechtigung. Ein selbst signierender
Implementierer ist kein unabhängiger Prüfer. Testschlüssel sind flüchtige Fixtures.

## Auswahl und formale Freigabe

Ein vertrauenswürdiger negativer Review sperrt auch bei positivem anderem Anbieter.
Nichtverfügbarkeit ist von einem fachlichen Fehler getrennt. Der CodeRabbit-Adapter
erkennt Kontingent-Nichtverfügbarkeit nur aus einem authentifizierten Check des
exakten Heads mit ausdrücklichem Kontingent-Titel oder erfasst das Fehlen eines
ausgeführten Checks. Unklare Custom-Ergebnisse, widerrufene Reviews und ungültige
Belege werden keine Kontingent-Ausnahme. Der bestehende Validator echter
inhaltlicher CodeRabbit-Prüfung wird weiterverwendet. Alle gewählten Ergebnisse
müssen zum selben extern erwarteten Quellen-Digest passen.

Inhaltliche Akzeptanz ersetzt keine native GitHub-Freigabe. Die erforderliche
aktuelle Stimme muss von einer anderen berechtigten GitHub-Identität stammen;
ungelöste sperrende Stimmen bleiben nach einem Push wirksam. Der bestehende Host
prüft weiterhin sämtliche CI-/Sicherheitsbelege, tatsächlichen Schutz, frische
Basis, gelöste Diskussionen, sensible Änderungsfreigaben und menschliche
Laufzeitabnahme. Auswahlfunktion und CLI allein erlauben keinen Merge und
bescheinigen kein Gameplay.

## Prüfung und Einführung

`python3 -m unittest discover -s tests/review_contract -v` prüft echte
Ed25519-Fixture-Signaturen sowie positive/negative Fälle für Quellen, Identität,
Manipulation, Nichtverfügbarkeit gegenüber Fehlern, veraltete Reviews und
Selbstregistrierung. Bestehende Merge-Tests und Repository-Prüfung bleiben Pflicht.
Gezielte enthaltene CodeRabbit-Reviews ersetzen automatische inkrementelle
Anfragen in der Kandidatenkonfiguration; keine kostenpflichtige Ausweichroute.

Als Nächstes Vertrag und Vertrauensmodell unabhängig freigeben, ausschließlich
freigegebene bestehende Ausführungsidentitäten registrieren und danach typisierte
Ergebnisse in einem separat geprüften Schritt an schreibgeschützten Akzeptanzjob
und anbieterunabhängigen Host-Audit anbinden. Vor produktiver Nutzung den
Sandbox-Test für diese Route wiederholen. Der erfolgreiche PR-#18-Sandbox-Test
belegt ausschließlich die bestehende CodeRabbit-Route. Belege und Blocker stehen
im `HANDOFF.md`, nicht in einem zweiten Statusjournal hier.
Wiederverwendbare Testat-, Adapter- und Vertrauensgrenzen sind Orqen-Kandidaten;
kein Orqen-Repository wird verändert.

[CodeRabbit configuration reference](https://docs.coderabbit.ai/reference/configuration)
