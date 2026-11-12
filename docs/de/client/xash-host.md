# Host-Factory für die VGUI-Surface

`xash_host.so` lädt `xash_surface.so` und stellt die Factory-Liste bereit, die
`vgui2` nach `VGUI_Surface026` fragt. Die Surface bleibt eine eigene Bibliothek.
Sie wird nicht in `vgui2.so` eingebunden, Xash3D wird nicht verändert, und
`LD_PRELOAD` bleibt ein Spike-Werkzeug.

Der erste Eintrag der Liste beantwortet nur `VGUI_Surface026`. Ist eine Wurzel
gemountet, folgt `VFileSystem009` auf derselben Liste. Der Proxy liest nur
Dateien unter dieser Wurzel. Eine fehlende Datei oder ein Pfad außerhalb ist
ein Fehltreffer. Sind das Paar geladen und die Wurzel gemountet, nimmt
`IVGui::Init` diese Liste. `vgui2` hält danach die eine Surface. Eine
Scheme-Datei unter dieser Wurzel lädt derselbe Manager. Eine fehlende Datei
lässt den Host offen. Danach ruft der Host `IGameUI::Initialize`. Ein
fehlendes Engine-Modul wird nicht aufgerufen. Das Basis-Panel ist ein Kind der Host-Wurzel. Die Serverliste wartet, bis ein
Scheme geladen ist. `Start` und `RunFrame` sind dieser Aufruf nicht. Weitere Factories
werden für den Surface-Namen nicht gefragt.

Mausbewegung, Druck und Loslassen gehen an die drei Input-Aufrufe und danach
genau einmal an die Nachrichtenzustellung. `RunFrame` wird nicht aufgerufen,
damit der Fokus nicht auf eine gespeicherte Cursorposition zurückspringt.
`SurfaceSetCursorPos` speichert nur die Position.

Die aus dem SDK gebaute `vgui2.so` exportiert `csr_vgui_dispatch_messages`.
Der Host bekommt Funktionszeiger. Prozess, Steam und Bildrate stehen nur im
[Plan, Abschnitt 3.7](../../plan/csretro-plan.md).

## Bauen und Testen

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target xash_host-tests
./build/linux-tests/out/bin/Release/xash_host-tests
```

Die Tests laden die gebaute Surface und starten das Spiel nicht.
