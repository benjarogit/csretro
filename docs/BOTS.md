# Bots — Zielbild „CS Retro Bot“

**Status: Planung. Nicht in Arbeit.** Der Produktivstand bleibt bis auf Weiteres der
**ReGameDLL-ZBot**, unverändert, in `server/game/`. Dieses Dokument hält das Zielbild fest,
damit es später nicht neu erfunden werden muss — es ist **kein** Auftrag, jetzt etwas zu bauen.

Betriebsgrenze und Verzeichnisregeln: `bots/ROLE.md`. Lizenzlage: `docs/LIZENZEN.md`.

## Ziel

Ein **eigener** Bot („CS Retro Bot“), der aus den bekannten GoldSrc-Bots das jeweils Beste
übernimmt, modernisiert und unter einer Konfiguration vereinheitlicht. Kein Vendoring eines
Fremdbots, kein zweiter Bot-Stack neben dem ersten.

Bis dahin gilt: ZBot bleibt wie er ist. Kein Teilumbau „nebenbei“.

## Basis

| Rolle | Projekt |
|---|---|
| Funktionale Ausgangsbasis, heute produktiv | ReGameDLL ZBot (`server/game/regamedll/dlls/bot/`) |
| Zweite Hauptquelle | `yapb/yapb` |

## Quellen für Mechaniken und Features

Vor jedem Port gilt der Ablauf aus `.cursor/rules/upstream-strategie.mdc`:
entdecken → Relevanz prüfen → mit unserer Implementierung vergleichen → nur das Sinnvolle
übernehmen → an 64-Bit/Cross-Platform anpassen → testen → dokumentieren.

**Arbeitsweise: interaktiv, nicht im Alleingang.** Weil aus vielen Bots zusammengeführt wird,
ist die Feature-Auswahl eine laufende Produktentscheidung, keine Implementierungsdetailfrage.
Vor Übernahme wird gefragt: Ergibt dieses Feature für CS Retro Sinn, in welcher Variante, und
was passiert mit dem, was wir schon haben? Kein stilles Mergen „weil es im Quellbot stand“.

**Gleiche Engine-Linie (GoldSrc) — Code grundsätzlich portierbar:**

- `Bots-United/HPB-bot`
- `Bots-United/joebot`
- `CCNHsK-Dev/SyPB`
- `EfeDursun125/CS-EBOT-LEGACY`
- `Fundynamic/RealBot`
- `MuxaJlbl4/Condition-Zero-Coop`
- `rcbotCheeseh/RCBotSven5` (Sven Co-op, GoldSrc-Linie)
- ImpactBots (CS 1.6 Metamod; Thread https://cs-bg.info/forum/viewtopic.php?t=179393) — Rollen, Team-Taktik, JSON-Profile, Graph-Dateien. Öffentlicher Quellcode im Thread nicht genannt; Zuordnung vor erstem Port bestätigen. Später, vor/beim Port: ohne veröffentlichten Code ist Ghidra auf die lokalen ImpactBots-Binaries der Weg, um Mechaniken zu verstehen — analog zur Projektlinie in `docs/MENUS.md` (Ghidra auf Original-Binaries nur wenn 1–5 nicht reicht); Lizenz bleibt kein Gate, Zuordnung vor dem ersten Port bestätigen.

**Andere Engine — nur Konzepte und Verhalten, kein Codeport:**

- `APGRoboCop/rcbot2` (Source 1)
- `manicogaming/CSGOBetterBots` (CS:GO)
- `ed0ard/CS2-Bot-Improver`, `XBribo/CS2-Bot-Hider` (Source 2)

Die Engine-Zuordnung ist nach bestem Wissen notiert und **vor** dem ersten Port je Projekt zu
bestätigen — zusammen mit der Lizenz (siehe unten).

## Pflichtfunktionen des Zielbots

**Navigation**

- Automatische Nav-/Waypoint-Erzeugung. Heute die härteste Lücke: von 25 Maps im Gamedata-Baum
  hat genau eine (`de_dust`) ein `.nav`. Deshalb ist der Bot-Block der Create-Game-Server-Seite
  auf Maps ohne Mesh gesperrt (`docs/PHASE3M.md`, Schritt 3).
- Manuelles Bearbeiten und Speichern von Waypoints.
- Bessere Performance als die Vorlagen, nicht nur Feature-Gleichstand.

**Konfiguration**

- Bot-Konfiguration bereits **beim Erstellen** eines Spiels: LAN, Offline und Dedicated —
  über dieselbe Quelle, das `ServerProfile` (`docs/MENUS.md`, Abschnitt Serverprofil).
  Keine UI-Sonderlösung, keine zweite Konfigurationsdatei.
- **In-Game-Menü nur für Bots**, mit sofort wirksamen Änderungen während des Spiels.
- Schwierigkeit und Verhalten („was der Bot können soll“) als Teil dieser Konfiguration.
- Umfang beim Spielerstellen, über den heutigen ZBot-Block hinaus: Bots **ob**, **welche**
  (sobald es mehr als eine Implementierung gibt), **wie viele**, **welches Team**, sowie
  **Waffenfreigaben** — welche Waffen ein Bot benutzen darf und benutzt. Das sprengt eine
  einzelne Seite, gehört also in eigene Registerkarten oder Unterseiten des Create-Game-
  Dialogs, nicht in den Bot-Block der Server-Seite.
- **Voreinstellungen müssen ohne Zutun gut sein.** Ausgeliefert wird eine sinnvolle
  Bot-Konfiguration, nicht ein Satz Nullwerte, den der Nutzer erst brauchbar machen muss.

## Abhängigkeiten zur UI

Beide Oberflächen entstehen erst, wenn der Bot dahinter existiert — Feature-UI ohne Backend
ist ausgeschlossen (`docs/PHASE3M.md`):

| Oberfläche | Wo | Status |
|---|---|---|
| Bot-Block beim Spiel erstellen | Create-Game-Server-Seite | vorhanden, auf ZBot-Umfang begrenzt, ohne Nav gesperrt |
| Eigene Bot-Registerkarten (Auswahl, Anzahl, Team, Waffenfreigaben) | Create-Game-Dialog | offen, erst mit CS Retro Bot |
| In-Game-Bot-Menü | In-Game-UI | offen, erst mit CS Retro Bot |

## Lizenzen

**Kein Gate.** Wie überall im Projekt (`docs/LIZENZEN.md`): Lizenzen halten die Entwicklung
nicht auf, das Repo ist privat, ein vollständiger Audit steht vor öffentlicher Distribution an.
Die Feature-Auswahl entscheidet der Produktnutzen, nicht die Lizenz.

Nur zur Kenntnis für den späteren Audit: Die POD-Bot-Linie (YaPB, SyPB, E-BOT, RealBot, joebot)
ist überwiegend GPL, die ZBot-Basis aus ReGameDLL ist MIT. Herkunft übernommener Teile wandert
wie gewohnt nach `CREDITS.md`.

## Nicht jetzt

- ZBot umbauen, ersetzen oder löschen.
- YaPB oder einen anderen Fremdbot nach `bots/` vendorn.
- Bot-UI bauen, die mehr verspricht, als ZBot kann.
