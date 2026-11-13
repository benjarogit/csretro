# Launcher-Integration

Der Launcher verbindet Host, Surface, Client-Erweiterungs-/VGUI-Paar, GameUI, lokalen Steam-API-Shim, Engine-Erweiterungen und NitroApi mit einer separaten Inhaltswurzel. Sein bisheriges Programm zeigt ein Menü und nimmt Beenden sowie Eingaben an. Es startet noch kein Xash3D-Gameplay.

## Inhalte und Startverhalten

Bereite Inhalte mit dem [Inhaltsinstaller](install.md) vor. Die Inhaltswurzel ist erforderlich und muss außerhalb der Steam-Bibliothek liegen. Einen angegebenen Kartennamen kann die Testumgebung untersuchen; das ist etwas anderes als eine echte Engine, die eine Karte lädt, Bewegung vorhersagt und eine Spielrunde ausführt. Ein gezeichnetes Menü oder aufgezeichnete Welt-Vertices beweisen keine gerenderte Spielwelt.

Der lokale Steam-API-Shim meldet den Benutzer als abgemeldet. Er startet keinen Steam-Prozess und liefert keine produktive Authentifizierung. Öffentliche Serveranmeldung braucht eine eigene Implementierung mit Nachweis.

## Build und Tests

Baue `xash_launcher` mit `linux` und `xash_launcher-tests` mit `linux-tests`; siehe [Client bauen](../build/client.md). Die Launcher-Tests prüfen Eingabevalidierung, Inhaltsfehler und den Host-Lebenszyklus. Der tatsächliche Spielstart bleibt nach der [Laufzeit-Abnahme](../maintainers/runtime-readiness.md) eine menschliche Aktion.

Die [Host-Anleitung](xash-host.md) beschreibt Factory- und Dateisystemverantwortung. Fortschritt, reproduzierbare Ergebnisse und nächste Arbeit stehen in [HANDOFF](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
