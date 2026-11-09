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
