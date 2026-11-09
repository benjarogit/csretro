# Client-Laufzeit

CSRETRO läuft nur auf dem **Default-Public-Branch** von Counter-Strike 1.6. Der
Client erkennt den installierten Build, bevor er Engine- oder Client-Funktionen
hookt.

## Build-Erkennung

Der Build wird über den Inhalt der beiden retail Binärdateien bestimmt, nicht
über Dateigröße oder eine Versionszeichenkette:

| Datei | Rolle |
|---|---|
| `hw.so` | Engine (Linux) |
| `cstrike/cl_dlls/client.so` | Spiel-Client |

Beim Start berechnet der Client von beiden Dateien den SHA-256 und vergleicht das
Paar mit der Build-Registry in `client/nextclient/nitro_api/src/build_registry.cpp`.
Nur ein exakt passendes Paar ergibt eine Build-Identität. Die Registry enthält
derzeit:

| ID | Plattform | Steam-Build |
|---|---|---|
| `public-linux-1.0.1.4` | Linux | 12934623 |

## Unbekannter Build

Passt das Paar auf keinen Eintrag, protokolliert der Client beide Digests und
schaltet die Hooks ab. Es gibt keinen Rückfall auf einen anderen Build und keine
Schätzung über Dateigröße. Ein unbekannter Build bedeutet: CSRETRO funktioniert
nicht, bis die Digests geprüft und ein Eintrag ergänzt wurde.

## Adressauflösung

Der Public-Provider liegt in `EngineAddressProviderPublicLinux.cpp` und
`ClientAddressProviderPublicLinux.cpp`. Jeder Eintrag wird dreistufig aufgelöst:

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

Die beiden generierten Namen sind an genau diese Binärdatei gebunden. Der Build
ist über SHA-256 gepinnt; ein anderer Build erhält einen eigenen Provider, und
das Werkzeug meldet jede Abweichung, bevor der Client startet.

`EngineMsgBase` ist die Tabelle `cl_parsefuncs`; ihr Aufbau
`{opcode, pszname, pfnParse}` ist layoutgleich zu `goldsrc::EngineMsg`.

## Neuen Build aufnehmen

1. Die beiden Dateien aus einer unveränderten Installation nehmen.
2. `sha256sum hw.so cstrike/cl_dlls/client.so` ausführen.
3. In `build_registry.cpp` einen Eintrag mit ID, Steam-Build-Nummer, Plattform
   und beiden Digests ergänzen.
4. `nitro_api-tests` laufen lassen; die Registry-Tests decken unbekannte,
   unvollständige und plattformfremde Fingerabdrücke ab.

Ein neuer Build braucht außerdem einen eigenen Address-Provider, dessen
Symbolnamen gegen die neuen Binärdateien geprüft werden. Der Resolver selbst
bleibt unverändert.

## Erster Start

Die manuelle Prüfung nach einem Client-Build steht in
[Erster Client-Start](first-start.md).
