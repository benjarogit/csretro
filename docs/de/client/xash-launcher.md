# Xash-Launcher

Der Xash-Launcher setzt die Stücke zusammen, die bereits gebaut sind:
`xash_host`, die eine `VGUI_Surface026`, `client_mini`, `vgui2`, `gameui`,
die `VFileSystem009`-Wurzel, die ELF64-`libsteam_api.so`,
`next_engine_mini.dll` und `nitro_api2.dll`. Er fügt keine
zweite Surface, keine zweite Factory und keinen zweiten Identitätsslot hinzu.

Die Installation sucht die Steam-Bibliothek und kopiert die gemounteten
Inhalte nach außerhalb. Siehe [Spielinhalte installieren](install.md). Das
Öffnen bekommt diese Kopie als Inhaltswurzel. Fehlt das Verzeichnis, ist das
ein Fehlschlag. Die Kopie erfindet keinen Pfad, und dieses Öffnen startet
Steam nicht. Originaldaten, Maps, Modelle, Sounds und `valve/resource` bleiben
Voraussetzung und liegen nicht im Repository.

Derselbe Prozess öffnet diese Liste über den Host: die eine Surface, dann
`client_mini` und `vgui2`, dann `gameui`, dann die Dateisystem-Wurzel, dann
`IVGui::Init` auf dieser Liste. Eine Scheme-Datei in der Wurzel wird dort
geladen. Fehlt sie, bleibt der Host offen. Danach folgt
`IGameUI::Initialize`. Dann `libsteam_api.so`.
`SteamAPI_IsSteamRunning` bleibt falsch. Nach `IGameUI::Start` lädt dasselbe
Öffnen `engine_mini`, ruft `EngineMini::Init` und danach argv, Modus und
Init. `COM_InitArgv` läuft. `OnGameInitializing` liest eine Zeigertabelle aus
diesem Prozess, ruft `Con_Init` und ruft `Sys_Error` nicht. `-condebug` ruft
ein Null-Dateisystem nicht auf. `pfnGetGameDir` schreibt den Namen des
gemounteten Verzeichnisses. Ein leeres Mount lässt diesen Zeiger null, und
`SetCStrikeFlags` wird nicht aufgerufen. Ein vorhandener Name lässt
`SetCStrikeFlags` laufen. `NET_SendPacket` ist die Funktion in diesem Prozess,
wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn
nicht auf, daher wird kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf.
Ein Null-Hook ist kein Aufruf. `Netchan_CopyFileFragments` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher wird keine Fragmentdatei
kopiert und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `SVC_TimeScale` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Diese Funktion ist `CL_Parse_Timescale`.
Der Init speichert den Zeiger und ruft ihn nicht auf, daher bleibt die
Timescale unverändert und es wird kein Socket geöffnet. Ein Null-Zeiger ist
kein Aufruf. Ein Null-Hook ist kein Aufruf. `SVC_SendCvarValue` ist danach die Funktion in diesem Prozess,
wenn dieser Retail-Hook fehlt. Diese Funktion ist `CL_Send_CvarValue`. Der
Init speichert den Zeiger und ruft ihn nicht auf, daher wird kein Cvar
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
kein Aufruf. Ein Null-Hook ist kein Aufruf. Der nächste Retail-Hook ist
`CL_ClearState`. Dieser Pfad kehrt davor zurück.
`OnGameInitialized` wird nicht betreten.
Shutdown und der fatale Fehler
bleiben gebunden und werden nicht gerufen. Eine fehlende Bibliothek oder ein
fehlender
Export lässt das Öffnen scheitern und ist kein Aufruf. Der geplante
Befehl wird nicht ausgeführt. Der Launcher startet das Spiel nicht, führt
Xash3D nicht aus, ruft `RunFrame` nicht auf und startet Steam nicht. Ein
fehlgeschlagenes Öffnen lässt den Host geschlossen. `PresentXashLauncher`
ruft danach `ActivateGameUI` und zeichnet dieses Menü in der angegebenen
Größe. `Quit` wird nicht ausgeführt. `setpause` wird nur aufgezeichnet.
Fehlt das Scheme, bleiben die Optionsseiten ungebaut, und ein
Serverbrowser ohne Dialog wird nicht geöffnet.

`nextclient.sh` und [Erster Client-Start](first-start.md) bleiben der
GoldSrc-Weg, bis dieser Pfad sie ersetzt.
