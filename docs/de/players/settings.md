# Einstellungen und Konsolenbefehle

Der Client ist noch nicht spielbar. Diese Anleitung erklärt die Pflege der Einstellungen und ihre bisherigen Quellen; sie behauptet nicht, dass die Spieleinstellungen unter Xash bereits funktionieren.

## Nach Aufgabe auswählen

Nutze das native Optionsmenü für Eingabe, Bild, Ton, HUD, Radar, Waffen und Messwerkzeuge, sobald die jeweilige portierte Funktion verfügbar ist. Ändere eine Einstellung auf einmal und vergleiche dieselbe Karte, Position und Szene. Ein Menüelement ohne seinen Spiel-Callback ist noch keine funktionierende Einstellung.

Unterscheide bei der Konsole zwischen einem Cvar (gespeicherter Wert) und einem Befehl (Aktion). Diagnosebefehle können eine geladene Karte oder einen initialisierten Renderer benötigen. Übernimm keine komplette Upstream-Konfiguration blind: alte Engine-Versionen, Windows-Browserfunktionen und fremde Onlinedienste gehören nicht zum Produktziel.

## Namen, Werte und Voreinstellungen

Maßgeblich sind die Cvar-Registrierung und die gemeinsamen Defaults im Code. Generierte Befehls- und Cvar-Referenzen müssen daraus entstehen; diese Anleitung pflegt keine zweite Default-Tabelle. Die technischen Upstream-Anleitungen erklären [Radarsteuerung](https://github.com/benjarogit/csretro/blob/main/client/docs/radar-layers.md) und [Leistungswerkzeuge](https://github.com/benjarogit/csretro/blob/main/client/docs/performance-monitor.md), beschreiben aber die geerbte Implementierung und keine abgenommene x64-Version.

## Ein Einstellungsproblem melden

Notiere CSRETRO-Revision, Karte, Funktion, angeforderten Wert, tatsächliches Ergebnis und ob das Problem nach Neuverbindung oder Neustart bleibt. Persönliche Konfigurationspfade und Logs bleiben lokal. Reiche nur den relevanten Ausschnitt ohne private Daten ein. Mitentwickler nutzen die [Laufzeit-Abnahme](../maintainers/runtime-readiness.md).
