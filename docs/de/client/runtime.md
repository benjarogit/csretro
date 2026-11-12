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
Aufruf. Der nächste Retail-Hook ist `Mod_NeedCRC`. Dieser Pfad kehrt davor
zurück.
`csr_host_frame` ist ein späterer direkter Aufruf und verlängert diese Liste nicht.
Ein Aufruf öffnet das X11-Display, erzeugt mit `eglCreateWindowSurface` eine
EGL-Fensterfläche, zeichnet ein GameUI-Bild und ruft `eglSwapBuffers`.
`ISurface::SwapBuffers` bleibt leer. Die Surface-Tests behalten ihr EGL-Pbuffer.
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
