# Bedienung

Die Tastenbelegung ist anpassbar. Maßgeblich sind deine Einstellungen, nicht eine starre
Liste aus einer anderen Counter-Strike-Version. Die folgenden Befehle helfen bei der Orientierung.

| Aktion | Konsolenbefehl | Übliche Taste |
| --- | --- | --- |
| Bewegen | `+forward`, `+back`, `+moveleft`, `+moveright` | W, S, A, D |
| Springen / Ducken | `+jump` / `+duck` | Leertaste / Strg |
| Primär- / Sekundäraktion | `+attack` / `+attack2` | M1 / M2 |
| Nachladen | `+reload` | R |
| Kaufen | `buy` | B |
| Teamwahl | `chooseteam` | M |
| Granatengruppe | `slot4` | 4 |
| Waffenwechsel | `invnext` / `invprev` | Mausrad |
| Punktestand | `+showscores` | Tab |

## Team, Klasse und Kaufen

Nach dem Beitritt wählst du ein Team und eine Klasse. Das Kaufmenü bietet die für dein Team
verfügbaren Gegenstände. Guthaben, Kaufzone und Kaufzeit bestimmen, was gekauft werden kann.
Die Menüs sind weiterhin in Überarbeitung; [Screenshots](screenshots.md) zeigen einen Entwicklungsstand.

## Granaten

Neben HE, Flash und Smoke sind Molotov für T und Incendiary für CT nativ im Spielcode vorhanden.
Die vorgesehene Bedienung ist: M1 für den normalen Wurf, M2 für einen kurzen Wurf,
beide für die mittlere Variante; halten bereitet vor, loslassen wirft.
Die Granaten gehören zur Slot-4-Gruppe.

!!! warning "Bekannte Einschränkung"
    Der erste M1-Druck reagiert in gemeldeten Abläufen noch nicht zuverlässig.
    Bei Incendiary wurde Schaden ohne sichtbares Feuer gemeldet.
    Diese Anleitung beschreibt das Zielverhalten, keine Bestätigung einer Fehlerbehebung.
    Siehe [Projektstatus](status.md).

Für reproduzierbare Berichte bitte Kauf per Maus oder Tastatur, Auswahl per Slot oder Mausrad,
Freeze-Time, gedrückte Taste und Loslassen getrennt nennen.
