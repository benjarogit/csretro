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

## Neuen Build aufnehmen

1. Die beiden Dateien aus einer unveränderten Installation nehmen.
2. `sha256sum hw.so cstrike/cl_dlls/client.so` ausführen.
3. In `build_registry.cpp` einen Eintrag mit ID, Steam-Build-Nummer, Plattform
   und beiden Digests ergänzen.
4. `nitro_api-tests` laufen lassen; die Registry-Tests decken unbekannte,
   unvollständige und plattformfremde Fingerabdrücke ab.

Die Adress-Provider für den Public-Build entstehen in G07b und G07c. Bis dahin
meldet der Client die erkannte Build-ID, lässt die Hooks aber deaktiviert.
