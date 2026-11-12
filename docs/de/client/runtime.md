# Client-Laufzeit

CSRETRO läuft nur auf dem **Default-Public-Branch** von Counter-Strike 1.6.
Die Engine des Produktprozesses ist Xash3D. Die Entscheidung steht in
[Plan, Abschnitt 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Build-Erkennung

Die Produktidentität ist `xash` (`BuildFamily::Xash`). Der Start öffnet,
hasht und hookt `hw.so` nicht und liest `client.so` nicht. Fehlende
Engine-Adressen sind keine Hooks. Ein Aufruf springt nicht durch einen
Null-Trampolin.

`vgui2_surface`, `VGUI2_ResetCurrentTexture` und `VGUI2_Draw_SetTextColor`
gehen über `xash_engine` an die eine Surface. `VGUI2_Draw_Init` hat dort
keinen Einstieg. Ein fehlender Funktionszeiger wird nicht aufgerufen.
`csr_xash_engine_argv`, `csr_xash_engine_mode`, `csr_xash_engine_init_game`,
`csr_xash_engine_shutdown` und `csr_xash_engine_error` nehmen Argumente,
Videomodus, Spielstart, Herunterfahren und einen fatalen Fehler entgegen.
Liegt der passende Retail-Hook vor, bleibt dieser Aufruf. Fehlt er, zeichnet
`engine_mini` die Angaben auf und merkt sich, dass die Engine Xash ist.
Der Host lädt danach `next_engine_mini.dll` und `nitro_api2.dll`, ruft
`EngineMini::Init` und dann argv, Modus und Init. Eine fehlende Bibliothek
oder ein fehlender Export ist ein Fehlschlag und kein Aufruf. Ein
erfolgreiches Öffnen ruft Shutdown und den fatalen Fehler nicht; beide
bleiben gebunden. `COM_InitArgv` läuft dabei. `OnGameInitializing` liest eine
Zeigertabelle aus diesem Prozess. Ein Null-Funktionszeiger ist kein Aufruf,
und `Sys_Error` wird nicht gerufen. `Con_Init` läuft danach. Es liest
`-condebug` über `COM_CheckParm` und ruft ein Null-Dateisystem nicht auf.
`pfnGetGameDir` schreibt danach den Namen des gemounteten Verzeichnisses.
Ein leeres Mount lässt diesen Zeiger null, und `SetCStrikeFlags` wird nicht
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
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`Mod_Extradata` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher werden keine Extradaten gelesen, kein Modell geladen, keine
Modelldatei gelesen, keine CRC berechnet, needload bleibt unverändert und
`Sys_Error` läuft nicht. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `SPR_Init` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleibt eine leere Sprite-Liste unalloziert, kein Sprite wird
geladen, keine Datei gelesen und kein Socket geöffnet. Ein Null-Zeiger ist
kein Aufruf. Ein Null-Hook ist kein Aufruf. `SPR_Shutdown` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert
den Zeiger und ruft ihn nicht auf, daher bleibt eine leere Sprite-Liste
unalloziert, eine allozierte Sprite-Liste bleibt alloziert, keine
Sprite-Textur wird entladen, kein Sprite wird geladen, keine Datei gelesen
und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `SPR_Shutdown_NoModelFree` ist danach die Funktion in
diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den
Zeiger und ruft ihn nicht auf, daher bleibt eine leere Sprite-Liste
unalloziert, eine allozierte Sprite-Liste bleibt am selben Zeiger, der
Sprite-Name bleibt am selben Zeiger, keine Sprite-Textur wird entladen,
kein Sprite-Name wird freigegeben, kein Sprite wird geladen, keine Datei
gelesen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `SPR_Load` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und
ruft ihn nicht auf, daher bleibt eine leere Sprite-Liste unalloziert, eine
allozierte Sprite-Liste bleibt am selben Zeiger, die Anzahl bleibt gleich,
der Sprite-Name bleibt am selben Zeiger, kein Sprite wird geladen, kein
Sprite-Name wird alloziert, `Mod_ForName` läuft nicht, `Sys_Error` läuft
nicht, keine Datei wird gelesen und kein Socket geöffnet. Ein Null-Zeiger
ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `SPR_Draw` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher bleibt das aktuelle
Sprite am selben Zeiger, kein Sprite-Frame wird gezeichnet,
`Draw_SpriteFrame` läuft nicht, kein Sprite wird geladen und kein Socket
wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `SPR_DrawAdditive` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleibt das aktuelle Sprite am selben Zeiger, kein Sprite-Frame
wird additiv gezeichnet, `Draw_SpriteFrameAdditive` läuft nicht, kein
Sprite wird geladen und kein Socket wird geöffnet. Ein Null-Zeiger ist kein
Aufruf. Ein Null-Hook ist kein Aufruf. `SPR_DrawHoles` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher bleibt das aktuelle
Sprite am selben Zeiger, kein Sprite-Frame wird mit Löchern gezeichnet,
`Draw_SpriteFrameHoles` läuft nicht, kein Sprite wird geladen und kein
Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist
kein Aufruf. `SPR_DrawGeneric` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher bleibt das aktuelle
Sprite am selben Zeiger, kein Sprite-Frame wird mit dem Blend des Aufrufers
gezeichnet, `Draw_SpriteFrameGeneric` läuft nicht, kein Sprite wird geladen
und kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `SPR_EnableScissor` ist danach die Funktion in
diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den
Zeiger und ruft ihn nicht auf, daher bleibt ein gesetztes Scissor-Rechteck
unverändert, `EnableScissorTest` läuft nicht, kein Sprite wird geladen und
kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `SPR_DisableScissor` ist danach die Funktion in
diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den
Zeiger und ruft ihn nicht auf, daher bleibt ein gesetztes Scissor-Rechteck
unverändert, `DisableScissorTest` läuft nicht, kein Sprite wird geladen und
kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `SPR_Set` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleibt das aktuelle Sprite am selben Zeiger mit derselben Farbe
und demselben Scale, `qglColor4f` läuft nicht, kein Sprite wird geladen und
kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook
ist kein Aufruf. `SPR_Width` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleiben gesetzte Sprite-Abmessungen unverändert, keine Sprite-Breite
wird gelesen, kein Sprite wird geladen und kein Socket wird geöffnet. Ein
Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `SPR_Height` ist
danach die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher bleiben gesetzte
Sprite-Abmessungen unverändert, keine Sprite-Höhe wird gelesen, kein Sprite
wird geladen und kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf.
Ein Null-Hook ist kein Aufruf. `SPR_Frames` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und
ruft ihn nicht auf, daher bleiben gesetzte Sprite-Frame- und Abmessungsfelder
unverändert, keine Sprite-Framezahl wird gelesen, kein Sprite wird geladen und
kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist
kein Aufruf. `SPR_GetModelPointer` ist danach die Funktion in diesem Prozess,
wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht
auf, daher bleiben gesetzte Sprite- und Model-Felder unverändert, kein Model-
Zeiger wird aufgelöst, kein Sprite wird geladen und kein Socket wird geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `R_StudioReloadSkin`
ist danach die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher bleiben gesetzte Studio- und
Skin-Felder unverändert, keine Studio-Skin wird neu geladen, keine Textur wird
berührt, kein Sprite wird geladen und kein Socket wird geöffnet. Ein Null-Zeiger
ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `SetCrosshair` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den
Zeiger und ruft ihn nicht auf, daher bleiben gesetztes Crosshair-Sprite, Farbe und
Rechteck unverändert, kein Crosshair wird gesetzt, kein Sprite wird geladen und
kein Socket wird geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `DrawCrosshair` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf, daher
bleiben gesetztes Crosshair-Sprite, Farbe und Rechteck unverändert, kein Crosshair
wird gezeichnet, kein Sprite wird geladen und kein Socket wird geöffnet. Ein
Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `R_RenderView` ist
danach die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher wird keine View gerendert, kein
GL-Kontext aus diesem Init geöffnet, kein Sprite geladen und kein Socket geöffnet.
Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `GL_Init` ist
danach die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher wird GL nicht initialisiert,
kein GL-Kontext aus diesem Init geöffnet, kein Sprite geladen und kein Socket
geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `GL_Config` ist
danach die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher wird keine GL-Config gelesen
oder geschrieben, kein GL-Kontext aus diesem Init geöffnet, kein Sprite geladen
und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `GL_SelectTexture` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf, daher
wird keine Textur-Einheit gewählt, kein GL-Kontext aus diesem Init geöffnet, kein
Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `CL_ConnectClient` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft
ihn nicht auf, daher wird kein Client verbunden, kein Netzwerkpfad aus diesem
Init geöffnet, kein Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist
kein Aufruf. Ein Null-Hook ist kein Aufruf. `CL_ClearClientState` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den
Zeiger und ruft ihn nicht auf, daher wird kein Client-Zustand gelöscht, kein
Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `GL_LoadTexture` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft
ihn nicht auf, daher wird keine Textur geladen, kein GL-Speicher alloziert,
keine Texturdatei geöffnet, kein GL-Kontext aus diesem Init geöffnet, kein
Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `GL_LoadTexture2` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft
ihn nicht auf, daher wird keine Textur geladen, kein Filter angewendet, kein
GL-Speicher alloziert, keine Texturdatei geöffnet, kein GL-Kontext aus diesem
Init geöffnet, kein Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger
ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `GL_UnloadTextures` ist danach
die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init
speichert den Zeiger und ruft ihn nicht auf, daher werden keine Texturen
entladen, kein Textur-Session-Zustand freigegeben, kein GL-Speicher
freigegeben, kein GL-Kontext aus diesem Init geöffnet, kein Sprite geladen und
kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein
Aufruf. `GL_UnloadTexture` ist danach die Funktion in diesem Prozess, wenn
dieser Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf,
daher wird keine Textur entladen, kein GL-Speicher freigegeben, kein
GL-Kontext aus diesem Init geöffnet, kein Sprite geladen und kein Socket
geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`GL_Bind` ist danach die Funktion in diesem Prozess, wenn dieser Retail-Hook
fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf, daher wird keine
Textur gebunden, kein GL-Texturzustand geändert, kein GL-Kontext aus diesem
Init geöffnet, kein Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger
ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `Draw_Init` ist danach die
Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert
den Zeiger und ruft ihn nicht auf, daher wird kein Draw-Zustand initialisiert,
keine Draw-Ressourcen geladen, kein GL-Kontext aus diesem Init geöffnet, kein
Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `Draw_Shutdown` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und
ruft ihn nicht auf, daher wird kein Draw-Zustand heruntergefahren, keine
Draw-Ressourcen freigegeben, kein GL-Kontext aus diesem Init geöffnet, kein
Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. `Draw_PicFromWad` ist danach die Funktion in diesem
Prozess, wenn dieser Retail-Hook fehlt. Der Init speichert den Zeiger und
ruft ihn nicht auf, daher wird kein WAD-Bild geladen, keine WAD-Datei geöffnet,
kein GL-Kontext aus diesem Init geöffnet, kein Sprite geladen und kein Socket
geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`V_UpdatePalette` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf, daher
wird die Palette nicht aktualisiert, keine Gamma-Tabellen geschrieben, kein
GL-Kontext aus diesem Init geöffnet, kein Sprite geladen und kein Socket
geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf.
`V_CheckGamma` ist danach die Funktion in diesem Prozess, wenn dieser
Retail-Hook fehlt. Der Init speichert den Zeiger und ruft ihn nicht auf, daher
wird Gamma nicht geprüft, keine Gamma-Tabellen neu gebaut, kein GL-Kontext aus
diesem Init geöffnet, kein Sprite geladen und kein Socket geöffnet. Ein
Null-Zeiger ist kein Aufruf. Ein Null-Hook ist kein Aufruf. `BuildGammaTable`
ist danach die Funktion in diesem Prozess, wenn dieser Retail-Hook fehlt. Der
Init speichert den Zeiger und ruft ihn nicht auf, daher werden keine
Gamma-Tabellen neu gebaut, kein GL-Kontext aus diesem Init geöffnet, kein
Sprite geladen und kein Socket geöffnet. Ein Null-Zeiger ist kein Aufruf. Ein
Null-Hook ist kein Aufruf. Der nächste Retail-Hook ist `DT_SetRenderState`.
Dieser Pfad kehrt davor zurück.
`csr_host_frame` ist ein späterer direkter Aufruf und verlängert diese Liste nicht.
Ein Aufruf öffnet das X11-Display, wählt eine EGL-Config, deren natives Visual
ein Kind des Root-Fensters sein kann, erzeugt mit `eglCreateWindowSurface` eine
EGL-Fensterfläche, zeichnet ein GameUI-Bild und ruft `eglSwapBuffers`.
Ein Visual mit der Tiefe des Root-Fensters gewinnt, und ein Border-Pixel wird
gesetzt, damit ein tieferes Visual kein BadMatch ist. `csr_host_pump` wiederholt
diesen Frame auf demselben gemappten Fenster, gibt Bewegungs- und
Tastenereignisse an `csr_host_input_move` und `csr_host_input_button` und
zerstört danach das Fenster. Nach N Frames ist die Swap-Zahl N, und die
Host-Zeit ist um N mal das Delta vorgerückt. `csr_host_run` hält dieses Fenster gemappt und wiederholt den Frame, bis der aufgezeichnete Client-Befehl `quit` ist. `QuitNoConfirm` schreibt diesen Befehl über `pfnClientCmd` und führt ihn nicht aus. Bewegungs- und Tastenereignisse erreichen weiter `csr_host_input_move` und `csr_host_input_button`. Der Aufruf zerstört danach das Fenster. Die Swap-Zahl ist die Zahl der Frames, und die Host-Zeit rückt um diese Zahl mal das Delta vor. `PresentXashLauncher` zeichnet ein Bild und kehrt zurück. Es ruft diesen Loop nicht auf. `RunXashLauncher` öffnet den Host wie `OpenXashLauncher` und ruft danach `csr_host_run`. Der menschliche Prozess ist `client/build/linux/out/bin/Release/xash_launcher`. Sein `main` ruft `RunXashLauncher`. Die Inhaltswurzel ist das erste Argument und der Mapname das zweite. Ist ein Mapname angegeben, liest `RunXashLauncher` vor dem Quit-Loop `cstrike/maps/<name>.bsp` aus dieser Wurzel und schreibt den angeforderten Namen und eine Prüfsumme der gelesenen Bytes in den Client-Zustand. Die Datei muss ein BSP der Version 30 sein, dessen Lumps in der Datei liegen. Eine fehlende Datei, eine zu kurze Datei oder eine andere Version schlägt fehl und ändert den Client-Zustand nicht, daher bleibt ein null-`worldmodel` null und der bisherige Mapname bleibt. Ist die Datei gültig, baut dieser Ladevorgang vor dem Quit-Loop aus den geprüften Bytes ein Brush-Worldmodel und schreibt es nach `client_state->worldmodel`. Der Modelname ist der angeforderte Mapname. Ein nicht leerer Entities-Lump wird auf dieses Model kopiert. Die Lumps für Vertices, Edges, Surfedges und Faces werden auf das Worldmodel kopiert. Ein Face wird eine Surface, `numsurfaces` ist danach mindestens 1, und die Vertex-Bytes stimmen mit dem Fixture überein. Texinfo und Texturen werden nicht kopiert. Lighting-Bytes sind keine Lightmap-Surfaces, und `GL_BuildLightmaps` läuft nicht. Der 128-Byte-Fixture mit der Prüfsumme `0xa4ebf95d` hat weiter leere Face-, Texinfo-, Textur- und Lighting-Lumps, daher bleibt `numsurfaces` dort 0. Nach einem gültigen Laden zeigt `r_worldentity->model` auf dieses Worldmodel. Eine direkte Zeichenfunktion in diesem Prozess läuft das Face über die Surfedges ab und reicht die Vertices als ein Primitive weiter. `RunXashLauncher` ruft sie nach einem erfolgreichen Binden einmal auf. Ist ein Worldmodel gebunden, ruft der menschliche Frame-Loop `csr_host_run` diese Zeichenfunktion in jedem Frame einmal auf. Sie speichert das Primitive und schreibt den Farbpuffer nicht, daher bleibt das getauschte Fenster das Menü. Ohne gebundenes Worldmodel bleibt der Frame das Menü und die Zeichenzahl bleibt 0. `PresentXashLauncher` kehrt weiter nach einem Bild zurück und reicht kein Welt-Primitive weiter. Ein fehlgeschlagener Ladevorgang zeichnet nicht und ersetzt einen bisherigen Renderer-World-Zeiger nicht. `eng()->R_DrawWorld` wird nicht installiert und nicht gerufen. `eng()->R_NewMap` wird nicht installiert und nicht gerufen. `eng()->Mod_LoadBrushModel` ist dieser Aufruf nicht. `Host_Map_f` ist dieser Aufruf nicht. Ohne Mapname lädt der Aufruf keine Map. Der Aufruf spielt kein Counter-Strike. `CVGui::RunFrame` bleibt ungerufen
und der Launcher-Befehl wird nicht ausgeführt. `ISurface::SwapBuffers` bleibt leer. Die Surface-Tests behalten ihr EGL-Pbuffer.
`OnGameInitialized` wird nicht betreten.
Die Retail-Hook-Kette läuft dabei nicht. `hw.so` wird nicht geöffnet. `RunFrame` wird
nicht gerufen. Die übrigen `engine_mini`-Weiterleitungen bleiben am
Retail-Hook.

`client_mini` und `vgui2` kommen aus derselben SDK-Revision. Beide sind ELF64
und linken `libtier0.so`, `libvstdlib.so` und `libsteam_api.so` aus dem
Host. Beide tragen `VGUI_Scheme009`. Valves i386-`client.so` wird nicht
geöffnet und nicht gelinkt.

`xash_host` öffnet dieses Paar, nachdem die Surface-Factory steht. Er sieht
`csr_vgui_dispatch_messages` und `VGUI_Scheme009`. Eine fehlende Bibliothek
ist ein Fehlschlag und kein Aufruf. Der Host startet das Spiel nicht.

`gameui` ist ELF64 aus derselben SDK-Revision und linkt dieselbe Host-Laufzeit.
Es trägt `GameUI007` und `GameConsole003` und keine eigene Surface. Der Host
öffnet es nach der Surface-Factory. Dieser Ladeaufruf ruft die
Spieloberfläche nicht auf und hängt keine zweite Factory an.
`IGameUI::Initialize` folgt dem Mount.

Dieselbe Factory-Liste beantwortet `VFileSystem009`, nachdem der Host eine
Wurzel gemountet hat. Die Surface bleibt der erste Eintrag. Der Proxy liest
nur Dateien unter dieser Wurzel. Eine fehlende Datei ist ein Fehltreffer.
`hw.so` wird dabei nicht geöffnet. Nach diesem Mount nimmt `IVGui::Init`
diese Liste, sodass `vgui2` die eine Surface hält. Eine Scheme-Datei unter
dieser Wurzel lädt derselbe Manager. Eine fehlende Datei lässt den Host
offen. `IGameUI::Initialize` hängt das Basis-Panel danach an die Host-Wurzel. Liegt `resource/schemes/TrackerScheme_ClassicPlus.res` vor, wird das Scheme daraus gelesen und die Serverliste ein Kind des Basis-Panels. `csr_host_paint_gameui` setzt danach die Größe dieser Wurzel und zeichnet den Baum. Liegt die Scheme-Datei vor, liest dieses Paint `resource/BackgroundLayout.txt` und lädt einen Texturnamen ohne Endung aus der `.tga` daneben. Das Hintergrundbild wird mit Uhrzeit 0 gezeichnet. `csr_host_start_gameui` ruft danach `IGameUI::Start` auf einer Host-Engine-Tabelle, die `hw.so` nicht öffnet. Das folgende Paint baut die Optionsseiten. Ein fehlendes
Engine-Modul wird nicht aufgerufen. `RunFrame` ist dieser Aufruf nicht. Der Host startet das Spiel nicht.

Die eine `libsteam_api.so` ist ELF64 aus dem Host. Sie lädt Valves
i386-Bibliothek nicht und startet Steam nicht. Ohne bereits laufendes,
eingeloggtes Steam meldet sie abgemeldet. Ein fehlender Schnittstellenname
wird nicht aufgerufen. Der Name bleibt `ResolvePlayerIdentity` und der
`name`-Setinfo, ohne Portrait.

Der Xash-Launcher setzt diese Stücke zu einem Befehl zusammen. Die
Installation sucht Steam und kopiert die gemounteten Inhalte außerhalb der
Bibliothek; das Öffnen bekommt diese Kopie als Inhaltswurzel. Fehlt sie,
startet nichts. Der Launcher wird nicht ausgeführt, startet Steam nicht und
braucht die Originaldaten. Siehe [Spielinhalte installieren](install.md).
`nextclient.sh` und der First-Start bleiben der GoldSrc-Weg.

Die Build-Registry in `client/nextclient/nitro_api/src/build_registry.cpp`
bleibt die Offline-Quelle für den Public-Linux-Fingerabdruck. Sie ist nicht
der Startpfad.

| ID | Plattform | Steam-Build | Rolle |
|---|---|---|---|
| `xash` | Linux | – | Produktprozess |
| `public-linux-1.0.1.4` | Linux | 12934623 | Offline-Fingerabdruck der Retail-Dateien |

## Adressauflösung

Der Produkt-Provider ist `EngineAddressProviderXash`. Er enthält keine
Retail-Symboltabelle. Die Public-Provider in
`EngineAddressProviderPublicLinux.cpp` und
`ClientAddressProviderPublicLinux.cpp` bleiben der Katalog der Retail-Namen
für die Offline-Prüfung. Der Produktprozess verwendet sie nicht. Jeder
Retail-Eintrag wird dreistufig aufgelöst:

1. **dynsym** – `dlsym` über den dynamischen Symboltabellen-Eintrag.
2. **symtab** – vollständige ELF-Symboltabelle der Datei auf der Platte.
3. **Pattern** – Byte-Muster als letzter Rückfall.

Beim Public-Build liegen die Engine-Funktionen nur in `symtab`; `dynsym` enthält
im Wesentlichen C++-Bibliotheken. Stufe 2 trägt daher die Auflösung. Die
Provider-Tabellen decken 327 Engine-Einträge und 81 Client-Einträge ab; alle
Namen wurden gegen die retail Binärdateien geprüft.

## Provider prüfen

`tools/check_address_providers.py` vergleicht beide Provider mit einer
Installation:

```bash
python3 tools/check_address_providers.py --game-dir /pfad/zu/Half-Life
```

Das Werkzeug prüft jeden Symbolnamen gegen `dynsym` und `symtab` und zählt jedes
String-Literal in der Binärdatei. Ein fehlendes Symbol, ein nicht eindeutiger
String oder ein nicht unterstützter Suchtyp lassen es mit Exit-Code 1 enden.
Das ist der Nachweis nach einem Steam-Update und vor dem Anlegen eines neuen
Providers.

Drei Einträge weichen vom Regelfall ab:

| Eintrag | Typ | Begründung |
|---|---|---|
| `isFogEnabled.26640` | Symbol | Compiler-generierter Name eines statischen Locals; im Public-Build nur so vorhanden |
| `R_ForceCVars.part.3` | Symbol | Compiler-generierter Teil einer Funktion; im Public-Build nur so vorhanden |
| `GL_ARB_multitexture ` | String | kommt in `hw.so` genau einmal vor |

Die beiden generierten Namen sind an genau diese Binärdatei gebunden. Der
Retail-Katalog ist über SHA-256 gepinnt. Das Werkzeug meldet jede Abweichung.
Der Produktprozess lädt diesen Katalog nicht.

`EngineMsgBase` ist die Tabelle `cl_parsefuncs`; ihr Aufbau
`{opcode, pszname, pfnParse}` ist layoutgleich zu `goldsrc::EngineMsg`.

## Neuen Build aufnehmen

1. Die beiden Dateien aus einer unveränderten Installation nehmen.
2. `sha256sum hw.so cstrike/cl_dlls/client.so` ausführen.
3. In `build_registry.cpp` einen Eintrag mit ID, Steam-Build-Nummer, Plattform
   und beiden Digests ergänzen.
4. `nitro_api-tests` laufen lassen; die Registry-Tests decken unbekannte,
   unvollständige und plattformfremde Fingerabdrücke ab.

Ein weiterer Retail-Katalog braucht einen eigenen Address-Provider, dessen
Symbolnamen gegen die neuen Binärdateien geprüft werden. Der Resolver selbst
bleibt unverändert. Der Produktpfad bleibt `EngineAddressProviderXash`.

## Erster Start

Die manuelle Prüfung nach einem Client-Build steht in
[Erster Client-Start](first-start.md).
