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
fehlendes Engine-Modul wird nicht aufgerufen. Nach `IGameUI::Start` lädt der
Host `next_engine_mini.dll` und `nitro_api2.dll`, ruft `EngineMini::Init`
und danach argv, Modus und Init. `COM_InitArgv` läuft. `OnGameInitializing`
liest eine Zeigertabelle aus diesem Prozess und ruft `Sys_Error` nicht.
Shutdown und der fatale Fehler bleiben gebunden und dieses Öffnen ruft sie
nicht. Eine fehlende Bibliothek oder ein fehlender Export lässt das Laden
scheitern und
ist kein Aufruf. Der Ladevorgang öffnet `hw.so` nicht und ruft `RunFrame`
nicht. Das Basis-Panel ist ein Kind der Host-Wurzel. Liegt
`resource/schemes/TrackerScheme_ClassicPlus.res` auf dieser Wurzel, liest der Host das Scheme aus der Quelldatei
und die Serverliste wird ein Kind des Basis-Panels. Fehlt die Datei, bleibt die Liste ungebaut. `csr_host_paint_gameui` setzt danach die Größe der Wurzel und ihrer Nicht-Popup-Kinder und zeichnet den Baum. Ein Panel, das zeichnet, ruft `PushMakeCurrent`. Liegt diese Scheme-Datei auf der Wurzel, wendet dasselbe Paint die Scheme-Einstellungen an. Das liest `resource/BackgroundLayout.txt`. Ein Texturname ohne Endung ist die `.tga`-Datei daneben. Eine fehlende Datei wird keine Textur. `csr_host_start_gameui` ruft `IGameUI::Start` mit einer Host-Engine-Tabelle. Sie öffnet `hw.so` nicht. Cvar-Lesen bleibt leer, die Uhr bleibt bei 0, und aufgezeichnete Befehle werden nicht ausgeführt. Fehlt die Scheme-Datei, wird der Serverbrowser nicht aufgerufen. Das Hintergrundbild wird davor mit Uhrzeit 0 gezeichnet. Das Paint danach baut die Optionsseiten. `RunFrame` ist dieser Aufruf nicht. Weitere Factories
werden für den Surface-Namen nicht gefragt.

`csr_host_load_pair` bindet Mausbewegung, Druck und Loslassen an das
`VGUI_InputInternal001` des Paars. Jedes Ereignis endet mit genau einem
`csr_vgui_dispatch_messages`. `IInputInternal::RunFrame` und `CVGui::RunFrame`
werden nicht aufgerufen, damit der Fokus nicht auf eine gespeicherte
Cursorposition zurückspringt. `SurfaceSetCursorPos` speichert nur die Position.
Ein manueller `csr_host_bind_input` bleibt für Tests mit Fake-Callbacks. Die
Host-Wurzel trifft ihre Kinder. Paint verknüpft diese Wurzel mit dem
Standard-Input-Kontext, damit der Treffertest den eingebetteten Baum läuft.
Eine Menü-Hover-Nachricht trägt das Panel-Handle, weil ein `VPANEL` auf LP64
nicht in ein KeyValues-Int passt. `csr_host_activate_gameui` ruft nach `Start`
`IGameUI::ActivateGameUI` und ruft `RunFrame` nicht auf. Ein Klick auf einen
gelegten Eintrag aus `GameMenu.res` läuft über diese Input-Bindung.
`QuitNoConfirm` schreibt `quit` über `pfnClientCmd` und führt ihn nicht aus.
LP64-Änderungen am vendorten SDK, einschließlich dieser Menü-Handle-Nachricht,
stehen in den Dateien unter `client/dep/ncl-hl1-source-sdk/`. Der Commit in
`versions.lock.json` ist die Upstream-Basis, von der dieser Baum ausgeht.

Die aus dem SDK gebaute `vgui2.so` exportiert `csr_vgui_dispatch_messages`.
Der Host bekommt Funktionszeiger. Prozess, Steam und Bildrate stehen nur im
[Plan, Abschnitt 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Bauen und Testen

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target xash_host-tests
./build/linux-tests/out/bin/Release/xash_host-tests
```

Die Tests laden die gebaute Surface und starten das Spiel nicht.
