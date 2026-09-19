# Projektstatus

**Stand dieser Übersicht: 11. September 2026.** CS Retro ist in aktiver Entwicklung.
Ein Screenshot oder erfolgreicher Compile-Test bedeutet nicht, dass das gesamte Spiel fehlerfrei ist.

| Bereich | Einordnung |
| --- | --- |
| Linux x86_64 | Lokale Builds und Spieltests vorhanden; kein allgemeines Supportversprechen |
| Windows / macOS | 64-Bit-Ziele, vollständige Build-/Runtime-Matrix noch offen |
| Team- und Klassenmenüs | Spielbar; Live-Teamvorschau zuletzt im Spiel positiv zurückgemeldet |
| Kaufmenü | Vorhanden, Darstellung und Bedienung werden weiter verfeinert |
| Molotov / Incendiary | Native Implementierung vorhanden; Eingabe, Effekte und Animationen noch fehleranfällig |
| Bots | ZBot-Code in der GameDLL; vollständige neue-Granaten-Kompetenz nicht als getestet bestätigt |
| Distribution | Kein aktuelles vollständiges Spielepaket; Asset-Beschaffung und Lizenzprüfung offen |
| Website | Spieler- und Entwicklerdokumentation auf Deutsch und Englisch |

## Aktuell bekannte Einschränkungen

- Erster M1-Druck bei Granaten wird in bestimmten Abläufen nicht wirksam ([#3](https://github.com/benjarogit/csretro/issues/3)).
- Incendiary-Feuer kann laut Spielberichten unsichtbar bleiben, obwohl Schaden entsteht ([#2](https://github.com/benjarogit/csretro/issues/2)). Movement-Replay fehlt noch ([#1](https://github.com/benjarogit/csretro/issues/1)).
- Viewmodels, Halte-/Wurfanimationen und das Bewegungsgefühl benötigen weitere Prüfung.
- Kaufmenü-Abstände, Lesbarkeit und Gegenstandsdarstellung sind noch nicht endgültig.
- Konsole und HUD wurden mit Darstellungs-/Bedienproblemen gemeldet.
- Ein vollständiger neuer Build braucht zusätzliche Assets, die noch nicht allgemein bereitgestellt sind.

Die Liste beschreibt bekannte Problemfelder, nicht zwingend jeden aktuellen Fehler.
[Issues](https://github.com/benjarogit/csretro/issues) dienen als öffentlicher Ort für Reproduktionen
und Diskussionen. Einen Fehler schließen wir nicht allein aufgrund eines erfolgreichen Builds.

## Richtung

Wir wollen das klassische Spielgefühl erhalten, die Bedienung verbessern und passende moderne
Ideen gezielt integrieren. Gute Vorschläge beschreiben das Problem, die gewünschte Spielerfahrung
und mögliche Nachteile. Es gibt hier keine zugesagten Liefertermine.

[Mitgestalten](contributing.md) · [Änderungen](https://github.com/benjarogit/csretro/blob/main/CHANGELOG.md)
