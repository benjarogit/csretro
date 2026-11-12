# Xash-Launcher

Der Xash-Launcher setzt die Stücke zusammen, die bereits gebaut sind:
`xash_host`, die eine `VGUI_Surface026`, `client_mini`, `vgui2`, `gameui`,
die `VFileSystem009`-Wurzel und die ELF64-`libsteam_api.so`. Er fügt keine
zweite Surface, keine zweite Factory und keinen zweiten Identitätsslot hinzu.

Der Aufrufer übergibt die Inhaltswurzel. Fehlt das Verzeichnis, ist das ein
Fehlschlag. Der Launcher sucht keine Steam-Bibliothek und erfindet keinen
Pfad. Originaldaten, Maps, Modelle, Sounds und `valve/resource` bleiben
Voraussetzung und liegen nicht im Repository.

Derselbe Prozess öffnet diese Liste über den Host: die eine Surface, dann
`client_mini` und `vgui2`, dann `gameui`, dann die Dateisystem-Wurzel, dann
`IVGui::Init` auf dieser Liste. Eine Scheme-Datei in der Wurzel wird dort
geladen. Fehlt sie, bleibt der Host offen. Danach folgt
`IGameUI::Initialize`. Dann `libsteam_api.so`.
`SteamAPI_IsSteamRunning` bleibt falsch. Der geplante
Befehl wird nicht ausgeführt. Der Launcher startet das Spiel nicht, führt
Xash3D nicht aus, ruft `RunFrame` nicht auf und startet Steam nicht. Ein
fehlgeschlagenes Öffnen lässt den Host geschlossen.

`nextclient.sh` und [Erster Client-Start](first-start.md) bleiben der
GoldSrc-Weg, bis dieser Pfad sie ersetzt.
