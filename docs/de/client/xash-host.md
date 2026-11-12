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
liest eine Zeigertabelle aus diesem Prozess, ruft `Con_Init` und ruft
`Sys_Error` nicht. `-condebug` ruft ein Null-Dateisystem nicht auf.
`pfnGetGameDir` schreibt den Namen des gemounteten Verzeichnisses. Ein
leeres Mount lässt diesen Zeiger null, und `SetCStrikeFlags` wird nicht
aufgerufen. Ein vorhandener Name lässt `SetCStrikeFlags` laufen.
`NET_SendPacket` ist die Funktion in diesem Prozess, wenn dieser Retail-Hook
fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf, daher wird kein
Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `Netchan_CopyFileFragments` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und
ruft ihn nicht auf, daher wird keine Fragmentdatei kopiert und kein Socket
geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`SVC_TimeScale` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Diese Funktion ist `CL_Parse_Timescale`. Der Init speichert
den Zeiger und ruft ihn nicht auf, daher bleibt die Timescale unverändert und
es wird kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `SVC_SendCvarValue` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Diese Funktion ist `CL_Send_CvarValue`.
Der Init speichert den Zeiger und ruft ihn nicht auf, daher wird kein Cvar
gesendet, Cvar-Werte bleiben unverändert und es wird kein Socket geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`CL_HTTPUpdate` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird kein HTTP-Request ausgeführt und kein Socket geöffnet. Ein
Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`CL_HTTPStop_f` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird kein Download gestoppt und kein Socket geöffnet. Ein
Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`CL_StartResourceDownloading` ist danach die Funktion in diesem Prozess,
wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn
nicht auf, daher wird kein Download gestartet und kein Socket geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`CL_ReadPackets` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird kein Paket gelesen und kein Socket geöffnet. Ein Null-Zeiger
ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`CL_RequestMissingResources` ist danach die Funktion in diesem Prozess,
wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn
nicht auf, daher wird keine Ressource angefordert und kein Socket geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`CL_Disconnect` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird nicht getrennt und kein Socket geöffnet. Ein Null-Zeiger ist
kein Aufruf. Ein Null-Hook ist kein Aufruf. `CL_ClearState` ist danach
die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher bleibt der
Client-Zustand unverändert und kein Socket geöffnet. Ein Null-Zeiger ist
kein Aufruf. Ein Null-Hook ist kein Aufruf. `Host_Map_f` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird keine Map geladen, der Levelname bleibt unverändert und kein
Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `Host_FilterTime` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleiben die Host-Zeit und das Frame-Intervall unverändert und
kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `Mod_ClearAll` ist danach die Funktion in diesem Prozess,
wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn
nicht auf, daher bleibt die Modellliste unverändert und kein Socket wird
geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`Mod_FindName` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird kein Modell gesucht oder geladen und kein Socket geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`Mod_Print` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird die Modellliste nicht gedruckt und kein Socket geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`Mod_ValidateCRC` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird keine CRC geprüft, kein Modell gesucht oder abgelehnt und kein
Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `Mod_NeedCRC` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleibt das Need-CRC-Flag unverändert, kein Modell wird gesucht
und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `Mod_LoadModel` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher wird kein Modell geladen, keine Modelldatei gelesen, keine CRC
berechnet und das Need-CRC-Flag bleibt unverändert. Ein Null-Zeiger ist kein
Aufruf. Ein Null-Hook ist kein Aufruf. `Mod_ForName` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher wird kein Modell gesucht oder geladen, keine Modelldatei gelesen,
kein Modellslot belegt, keine CRC berechnet und needload bleibt unverändert.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. Der nächste
Retail-Hook ist `Mod_Extradata`. Dieser Pfad kehrt davor zurück.
`csr_host_frame` ist ein späterer direkter Aufruf und verlängert diese Liste nicht.
`OnGameInitialized` wird nicht betreten.
Shutdown und der fatale Fehler bleiben gebunden und dieses Öffnen ruft sie
nicht. Eine fehlende Bibliothek oder ein fehlender Export lässt das Laden
scheitern und
ist kein Aufruf. Der Ladevorgang öffnet `hw.so` nicht und ruft `RunFrame`
nicht. Das Basis-Panel ist ein Kind der Host-Wurzel. Liegt
`resource/schemes/TrackerScheme_ClassicPlus.res` auf dieser Wurzel, liest der Host das Scheme aus der Quelldatei
und die Serverliste wird ein Kind des Basis-Panels. Fehlt die Datei, bleibt die Liste ungebaut. `csr_host_paint_gameui` setzt danach die Größe der Wurzel und ihrer Nicht-Popup-Kinder und zeichnet den Baum. Ein Panel, das zeichnet, ruft `PushMakeCurrent`. Liegt diese Scheme-Datei auf der Wurzel, wendet dasselbe Paint die Scheme-Einstellungen an. Das liest `resource/BackgroundLayout.txt`. Ein Texturname ohne Endung ist die `.tga`-Datei daneben. Eine fehlende Datei wird keine Textur. `csr_host_start_gameui` ruft `IGameUI::Start` mit einer Host-Engine-Tabelle. Sie öffnet `hw.so` nicht. Cvar-Lesen bleibt leer, die Uhr bleibt bei 0, und aufgezeichnete Befehle werden nicht ausgeführt. Fehlt die Scheme-Datei, wird der Serverbrowser nicht aufgerufen. Das Hintergrundbild wird davor mit Uhrzeit 0 gezeichnet. Das Paint danach baut die Optionsseiten. `RunFrame` ist dieser Aufruf nicht. Weitere Factories
werden für den Surface-Namen nicht gefragt.

`csr_host_frame` zeichnet ein GameUI-Bild über denselben Present-Pfad
(`ActivateGameUI`, dann das Paint oben) und addiert das übergebene Delta
auf die Host-Zeit. Es schreibt `realtime`, `host_frametime` und
`oldrealtime` direkt. `Host_FilterTime` bleibt ungerufen, und es kommt kein
Retail-Hook dazu. `CVGui::RunFrame` bleibt ungerufen. `hw.so` bleibt
ungemappt, kein Steam-Client startet, und der geplante Launcher-Befehl
wird nicht ausgeführt. Der Frame öffnet das X11-Display und wählt eine
EGL-Config, deren natives Visual ein Kind des Root-Fensters sein kann.
Ein Visual mit der Tiefe des Root-Fensters gewinnt. Ein Border-Pixel wird
gesetzt, damit ein tieferes Visual kein BadMatch ist. Das Fenster bekommt
dieses Visual, nicht das des Parents, dann `eglCreateWindowSurface`, das
Paint und `eglSwapBuffers`. Schlägt ein Aufruf fehl, gibt es keinen
Swap und die Host-Zeit bleibt stehen. `csr_host_pump` ruft diesen Frame
mit einer Framezahl auf demselben gemappten Fenster auf, gibt Bewegungs-
und Tastenereignisse an `csr_host_input_move` und `csr_host_input_button`
und zerstört danach das Fenster. Nach N Frames ist die Swap-Zahl N, und
die Host-Zeit ist um N mal das Delta vorgerückt. `csr_host_run` hält dieses Fenster gemappt und wiederholt den Frame, bis der aufgezeichnete Client-Befehl `quit` ist. `QuitNoConfirm` schreibt diesen Befehl über `pfnClientCmd` und führt ihn nicht aus. Bewegungs- und Tastenereignisse erreichen weiter `csr_host_input_move` und `csr_host_input_button`. Der Aufruf zerstört danach das Fenster. Die Swap-Zahl ist die Zahl der Frames, und die Host-Zeit rückt um diese Zahl mal das Delta vor. `PresentXashLauncher` zeichnet ein Bild und kehrt zurück. Es ruft diesen Loop nicht auf. Die X11-Verbindung ist der
Socket des Fensters. Ein Spiel-Socket geht nicht auf.
`ISurface::SwapBuffers` bleibt leer. Die Surface-Tests behalten ihr
EGL-Pbuffer. Die Uhr der GameUI-Engine-Tabelle bleibt bei 0.
`OnGameInitialized` bleibt außen, weil dieser Einstieg Materialdateien
lädt, `vgui2` auflöst und in die übrige Retail-Initialisierung weitergeht.

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
