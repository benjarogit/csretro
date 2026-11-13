# Was CSRETRO bietet

Der Client ist noch nicht spielbar. CSRETRO baut auf dem vendorten NextClient-Quelltext auf und wird zuerst unter Erhalt des bestehenden CS-Verhaltens auf Xash3D portiert. Ausschließlich Linux x86_64 und gemessene Spielleistung folgen danach; zusätzliche Funktionen kommen später. Originalinhalte von Counter-Strike 1.6 bleiben erforderlich. Vorhandener Quelltext bedeutet nicht, dass eine Funktion die Xash-Laufzeitprüfung bestanden hat.

## HUD und Radar

Der übernommene Client enthält Erweiterungen für Gesundheit, Schadensrichtung, Schadenssymbole, Killfeed, Fadenkreuz und Radar. Das erweiterte Radar unterstützt erzeugte Kartenbilder, Etagenwahl und Markierungen bekannter Spielerpositionen. Unbekannte Positionen dürfen dadurch nicht sichtbar werden. Diese Funktionen müssen noch in einer echten x64-Spielrunde geprüft werden.

## Waffen und Kamera

Im Quelltext liegen Waffeninspektion, Einstellungen für Position und Sichtfeld der Waffenansicht, Kamerabewegung und Waffenmaterialien. Rendering, Animation, Prediction und Waffenereignisse müssen beim Port zusammen geprüft werden; ein funktionierendes Einstellungsmenü beweist keine funktionierenden Waffen.

## Menü, Server und Downloads

NextClient bringt native VGUI2-Menüs, Serverbrowser, Ressourcendownloads und Schutz vor unerwünschten Serverbefehlen mit. CSRETRO behält die native Oberfläche bei. Browser-MOTD, Steam-Anmeldung, Downloads und Serververbindungen gelten nicht allein deshalb als funktionsfähig, weil der Host das Menü zeigt.

## Leistungswerkzeuge

Der Quelltext enthält CPU-/GPU-Framemessung, Netzwerkdiagnose, Karteninspektion und Renderer-Aufnahmen. Das sind Entwicklungswerkzeuge. Produktziel sind mindestens 60 FPS ohne Begrenzung auf 60 FPS; der Nachweis braucht eine echte Spielszene und keinen reinen Menü-Benchmark.

## Vom Server gesteuerte Funktionen

Die vendorte Server-API unterstützt kontrollierte Cvar-Änderungen, HUD-Sprites, Viewmodel-Effekte, Waffen-Soundersatz und Materialsynchronisation. Ein Plugin muss auf Client-Bereitschaft warten und unterstützte Fähigkeiten abfragen. Siehe [Plugin-Entwicklung](../plugins/index.md).

## Ideen aus anderen Clients

Kaufvoreinstellungen, weitere Fadenkreuz- und HUD-Optionen, Prediction-Verbesserungen und Zuschauer-Politur stehen im [maßgeblichen Backlog](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md#15-backlog-nach-100). Eine Funktion eines Referenzprojekts wird erst mit Implementierung, Tests und Dokumentation zur CSRETRO-Funktion. Herkunft und Lizenzgrenzen stehen in [CREDITS](https://github.com/benjarogit/csretro/blob/main/CREDITS.md).

## Weiter

- [Separate Kopie der Spielinhalte vorbereiten](../client/install.md).
- [Einstellungen und Befehle verstehen](settings.md).
- [Aktuelle Nachweise und verbleibende Arbeit lesen](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
