# VGUI-Surface auf Xash3D

CSRETRO zeichnet VGUI2 über eine eigene `VGUI_Surface026`. Xash3D liefert diese
Schnittstelle nicht, und die mitgelieferte `vgui2.so` zeichnet nicht selbst:
sie holt die Surface aus der Factory-Liste des Hosts und reicht jeden Aufruf
weiter.

## Modul

Das Modul ist `client/nextclient/xash_surface/`. Der Build erzeugt
`xash_surface.so`. `CreateInterface("VGUI_Surface026")` liefert die Instanz,
die `csr_surface_create` angelegt hat.

Der Host übergibt Bildschirmgröße, die absolute Panelposition, optionale
Insets und das absolute Clip-Rechteck. Zeichenkoordinaten bleiben relativ zum
Panel. VGUI-Alpha 0 ist opak; die Umrechnung passiert erst am OpenGL-Aufruf.
Jeder Draw stellt den OpenGL-Zustand wieder her, den er angefasst hat.

Text ist in diesem Stand eine Zelle pro Zeichen. Sie belegt Position, Farbe
und Laufweite. Die Glyphenformen kommen mit dem Font-Atlas. `PaintTraverse`
läuft den Panelbaum nicht ab, und Mausereignisse werden nur gespeichert.

## Bauen und Testen

Die Bibliothek hängt am Preset `linux` und damit an `BUILD_ALL`. Sie wird noch
nicht ins Spielverzeichnis installiert, weil der Launcher sie noch nicht lädt.

Die Tests laufen über das Preset `linux-tests`:

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target xash_surface-tests
./build/linux-tests/out/bin/Release/xash_surface-tests
```

Der Offscreen-Lauf setzt `LIBGL_ALWAYS_SOFTWARE=1` und öffnet einen
EGL-Pbuffer. Er startet das Spiel nicht. Der Spike mit dem externen Stub
bleibt der Beweis für den echten VGUI2-Baum, bis der Launcher diese Bibliothek
registriert.

Die Entscheidung und der Umfang stehen in
[Plan, Abschnitt 3.7](../../plan/csretro-plan.md).
