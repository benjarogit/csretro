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

## Dedizierte Reviewer-App: Einrichtungsvorschlag, keine Freigabe

GitHubs [Review-API](https://docs.github.com/en/rest/pulls/reviews#create-a-review-for-a-pull-request)
unterstützt ausdrücklich Installationstoken, `APPROVE` und ein exaktes
`commit_id`. Der minimale Publisher benötigt **Pull requests: write** sowie
obligatorisches Metadata read. Er handelt als App-Installation, nicht mit einem
Benutzertoken des PR-Autors. Kein zweites persönliches GitHub-Konto ist vorgesehen.

Die Fähigkeiten bleiben getrennt:

- Der Prüfer liest Quellen und führt die freigegebene unabhängige Prüfung aus.
  Für private Quellen benötigt er Contents read und Pull requests read, aber
  weder App-Privatschlüssel noch Schreibtoken. Bereits installierte lokale
  Modelle bleiben optionale Anbieter mit festgelegter Laufzeit und echten Ausgaben.
- Der isolierte Publisher benötigt Pull requests write zum Veröffentlichen des
  geprüften Reviews. Contents read nur bei eigenem Quellenabruf, Checks read
  nur bei eigener Laufprüfung. Schutz-, CodeQL- und CI-Audit bleiben beim
  bestehenden Host. Contents write, Checks write, Administration write, Actions
  write, Bypass-Eintrag oder breitere Installation sind nicht erforderlich.
- Installation ausschließlich für CSRETRO. Jedes Installationstoken wird auf
  dieses Repository und die jeweils benötigten Rechte begrenzt. Es verfällt
  nach einer Stunde und wird nach Gebrauch frühzeitig widerrufen. Keine Token
  in Review-Artefakten speichern.

Siehe GitHubs [Installationstoken-Dokumentation](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/generating-an-installation-access-token-for-a-github-app).
Diese Untersuchung hat keine Registrierung, Installation, Schlüsselerzeugung,
Secret-Hinterlegung oder Rechtevergabe durchgeführt.

### Unabhängigkeit braucht eine getrennte Ausführungsgrenze

Ein Bot-Name allein schafft keine Unabhängigkeit. Der implementierende Agent darf
weder App-Privatschlüssel lesen noch Installationstoken erzeugen, Ausführerprofile
ändern, Ausführer-Signierschlüssel verwenden oder beliebige Freigabe-/Signieraufrufe
veranlassen. Ein Schlüssel im eigenen Checkout, zugänglichen lokalen Konto oder
gewöhnlichen Repository-Secret genügt nicht. GitHub weist darauf hin, dass
Repository-Schreiber über Workflows auf Repository-Secrets zugreifen können:
[Secure-use-Hinweise](https://docs.github.com/en/actions/reference/security/secure-use).

Bevorzugt wird ein separat administrierter Prüfer-/Publisher-Prozess außerhalb
des Dateisystems und der Zugangsdaten des Autors. Ein Auftrag darf Repository/PR
benennen, niemals eine vertrauenswürdige Entscheidung vorgeben. Der Prüfer holt
unveränderliche Quellen selbst, führt die festgelegte Prüfung aus und liefert
authentifizierte Ausführungsbelege. Der Publisher prüft diese gegen das freigegebene
unveränderliche Vertrauensprofil, jedes Manifestkriterium/jeden Pfad und ungelöste
negative Befunde und liest Head/Basis unmittelbar vor Veröffentlichung erneut.
Er protokolliert App-/Bot-ID, Review-ID, exakten Commit, Manifest-/Quellen-/Ein-/
Ausgabe-Digests, Ausführer/Laufzeit, Verifier-Version, Entscheidung und Belegreferenz.
Wiederholung oder Quellenbewegung brechen ab. Repository-Text und Modellausgaben
sind nicht vertrauenswürdige Daten, niemals Betriebsanweisungen.

Ein Actions-Betrieb müsste zusätzlich belegen, dass nur ein unabhängig freigegebener
unveränderlicher Workflow an beschränkte Umgebung und Zugangsdaten kommt; kein
PR-/Head-Code darf dort mit Secrets laufen. Ein Secret in einer vorhandenen
Umgebung allein beweist das nicht. Änderungen an Prüferkonfiguration, Workflow,
Schlüsseln oder Sicherheitspolicy behalten ausdrückliche Maintainer-Freigabe.
Ein alleiniger Administrator, der auch die App kontrolliert, kann die
Kontrollebene übersteuern; keine App verhindert kryptografisch neue Schlüssel
oder Regeländerungen durch diesen Administrator. Diese Vertrauensgrenze wird
benannt, nicht als technisch ausgeschlossen dargestellt.

### Freigabe und geschützter Test

1. Maintainer gibt Repository-Rechte, Betriebsisolation, Vertrauensregistrierung,
   Schlüsselverantwortung und Rotation/Widerruf gesondert frei. App-Privatschlüssel
   ausschließlich im freigegebenen isolierten Secret-Speicher halten. Kurzlebige
   JWTs erzeugen begrenzte Installationstoken; Schlüsselrotation/-widerruf folgt
   [GitHubs Verfahren](https://docs.github.com/en/apps/creating-github-apps/authenticating-with-a-github-app/managing-private-keys-for-github-apps).
2. Vertrauenswürdige Ausführungs-/Veröffentlichungsgrenze implementieren und
   unabhängig prüfen. Unveränderliche App-/Bot-IDs und Ausführer-Public-Keys erst
   nach Freigabe registrieren. Wirksame Route und Schutz bleiben bis zur Abnahme.
3. In gleich geschützter Sandbox echte unabhängige Ausführung und aktuelle
   App-`APPROVED`-Stimme fordern. Identität/Commit und natives `reviewDecision`
   zurücklesen; Last-Push-Freigabe, Veralten von Stimmen und alle sieben
   CI-/Sicherheitschecks nachweisen. Ein akzeptierter REST-Aufruf reicht nicht.
4. Gegenproben: Selbst-/Autor-Testate, erfundene oder fehlende Ausführung,
   fehlgeschlagener Review vor Kontingenterschöpfung, veralteter Head/Basis,
   geänderte Manifest-/Pfaddaten, unbekannte/widerrufene Schlüssel, Replay,
   widerrufene Stimme, offene Diskussion und fehlgeschlagene CI/CodeQL. Jeder
   Fall muss natives Auto-Merge sperren.
5. Erst nach separater Migrationsfreigabe und echtem positivem Sandbox-Ergebnis
   darf die anbieterunabhängige Route normale geschützte Merges unterstützen.
   Der Publisher mergt nicht; bestehender Host und native Regeln behalten dies.

GitHub beschreibt [Last-Push- und Stale-Review-Schutz](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches).
Die bisherige CodeRabbit-App-Stimme funktionierte in der vorhandenen Sandbox mit
gleichwertigen Regeln. Die Anerkennung der noch nicht installierten App bleibt
unter diesen Regeln unbewiesen. Für einen erfolgreichen Test werden weder
Review-Anzahl reduziert, App-Bypass eingerichtet noch Actions-Freigaben aktiviert.
Diese Untersuchung erteilt keine Rechte und verspricht keine unbegrenzte kostenlose
Rechenleistung; kein kostenpflichtiger Anbieter oder Hosting wird aktiviert.

Die auf Freigabe wartende App-Einrichtung wird unabhängig von regulärer G07-Arbeit
geparkt. Sie erzeugt keine projektweite Entwicklungsabhängigkeit und erlaubt keinen
sonst gesperrten Merge. NextClient-/Xash3D-Entwicklung läuft in der vorhandenen
Branch-/Aufgabenfolge weiter; alle aktuellen Review-/Sicherheitsgates bleiben bestehen.
