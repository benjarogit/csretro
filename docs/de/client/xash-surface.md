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

Text kommt aus FreeType. Der erste Glyph-Set eines Handles legt die Höhe fest,
weitere Sets decken nur fehlende Zeichen ab. Laufweite und Ascent stammen aus
der Schriftdatei, nicht aus einer festen Zelle. Gezeichnet wird sofort, in
Aufrufreihenfolge, aus einem gemeinsamen Atlas (weißes RGB, Deckung im Alpha).

Mitgeliefert werden Ersatzschriften, weil Linux weder Arial noch Verdana noch
Tahoma hat: Arial wird Liberation Sans, Verdana und Tahoma werden DejaVu Sans.
Das ändert Glyphenform und Breite gegenüber Retail. Marlett liegt nur im
Retail-`valve/resource` und wird nicht mitgeliefert; der Host reicht den Pfad
über `resolve_font`. Unschärfe, Scanlines und Umriss sind noch offen.
`SolveTraverse` läuft den sichtbaren Baum über die Panel-Funktionen des Hosts.
`PaintTraverse` ruft die Paint-Methode des Panels einmal mit Repaint und
Allow-Force auf; die Kinder läuft VGUI2 selbst. Popups lässt
`ShouldPaintChildPanel` aus und die Surface malt sie danach. Ohne die
Funktionstabelle passiert nichts. Mausereignisse gehen an die Input-Funktionen
des Hosts: Cursor, Druck oder Loslassen, danach genau eine Zustellung.
Taste 0 ist die linke Maustaste. `RunFrame` tut nichts, damit kein
Fokus-Reset auf die letzte Cursorposition den Klick überschreibt.
`SurfaceSetCursorPos` speichert die Position und löst keine Eingabe aus.

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
EGL-Pbuffer. Er startet das Spiel nicht. `xash_host` veröffentlicht diese
Bibliothek als erste Factory. Der Spike mit dem externen Stub
bleibt der Beweis für den echten VGUI2-Baum, bis der Prozess diesen Host lädt.

Die Entscheidung und der Umfang stehen in
[Plan, Abschnitt 3.7](../../plan/csretro-plan.md).
