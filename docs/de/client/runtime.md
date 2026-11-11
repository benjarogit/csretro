# Client-Laufzeit

CSRETRO läuft nur auf dem **Default-Public-Branch** von Counter-Strike 1.6.
Die Engine des Produktprozesses ist Xash3D. Die Entscheidung steht in
[Plan, Abschnitt 3.7](../../plan/csretro-plan.md).

## Build-Erkennung

Die Produktidentität ist `xash` (`BuildFamily::Xash`). Der Start öffnet,
hasht und hookt `hw.so` nicht und liest `client.so` nicht. Fehlende
Engine-Adressen sind keine Hooks. Ein Aufruf springt nicht durch einen
Null-Trampolin.

`vgui2_surface`, `VGUI2_ResetCurrentTexture` und `VGUI2_Draw_SetTextColor`
gehen über `xash_engine` an die eine Surface. `VGUI2_Draw_Init` hat dort
keinen Einstieg. Ein fehlender Funktionszeiger wird nicht aufgerufen. Die
übrigen `engine_mini`-Weiterleitungen bleiben am Retail-Hook.

`client_mini` und `vgui2` kommen aus derselben SDK-Revision. Beide sind ELF64
und linken `libtier0.so`, `libvstdlib.so` und `libsteam_api.so` aus dem
Host. Beide tragen `VGUI_Scheme009`. Valves i386-`client.so` wird nicht
geöffnet und nicht gelinkt.

`xash_host` öffnet dieses Paar, nachdem die Surface-Factory steht. Er sieht
`csr_vgui_dispatch_messages` und `VGUI_Scheme009`. Eine fehlende Bibliothek
ist ein Fehlschlag und kein Aufruf. Der Host startet das Spiel nicht.

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
