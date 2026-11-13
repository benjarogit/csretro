# Host und Surface-Integration

Der Host ist die Integrationsumgebung für eine VGUI2-Surface, das gemountete Inhaltsdateisystem, die zusammengehörigen Client-Erweiterungs-/VGUI-Bibliotheken und GameUI. Er ist noch keine laufende Xash3D-Spielengine. Die [Laufzeit-Abnahme](../maintainers/runtime-readiness.md) trennt ein offenes Fenster von einem erfolgreichen Port.

Die Surface-Factory besitzt über diese Module hinweg eine Identität. Eingaben erreichen die Surface über den Host; Zeichnen und Dateizugriffe nutzen die gemountete Inhaltskopie. Fehlende Module, Schnittstellen oder Dateien müssen fehlschlagen, statt einen Null-Zeiger aufzurufen. Retail-`hw.so` gehört nicht in den x64-Prozess.

## Build und Tests

Baue `xash_host` mit dem Preset `linux` und `xash_host-tests` mit `linux-tests`. Die Tests prüfen Factories, Inhaltszugriff, Laden der Oberfläche und Fenster-/Eingabeverhalten in ihrer eigenen Offscreen- oder Xvfb-Umgebung. Sie beweisen keine laufende CS-Spielrunde. Siehe [Client bauen](../build/client.md).

Die Surface-Zeichnung hat eine eigene [Adapter-Anleitung](xash-surface.md). Das Zusammensetzen des Starts gehört zur [Launcher-Anleitung](xash-launcher.md). Aktuelle Testzahlen, Quellstand, CI und offene Integrationsarbeit stehen in [HANDOFF](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md), nicht als Kopie auf jeder Komponentenseite.
