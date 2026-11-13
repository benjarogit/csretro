# Client-Laufzeit

CSRETRO läuft nur auf dem **Default-Public-Branch** von Counter-Strike 1.6.
Die Engine des Produktprozesses ist Xash3D. Die Entscheidung steht in
[Plan, Abschnitt 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).

## Produktgrenze

Zuerst wird das bestehende NextClient-Verhalten mit architekturgleicher Engine, Spielbibliothek und Erweiterungen auf Xash3D portiert. Ausschließlich x64 und gemessene Spielleistung folgen danach. Der bisherige Host prüft Menü, Dateisystem und Surface; er startet die Spielengine nicht. `client_mini` erweitert einen vorhandenen Spielclient; der erste Port kann wie der i386-Spike die lokal bereitgestellte Original-Spielbibliothek nutzen. Damit ist die spätere x64-Spielbibliothek noch nicht gelöst. Eine geladene Factory oder gespeicherte Engine-Callbacks beweisen kein Gameplay.

Die [Laufzeit-Abnahme](../maintainers/runtime-readiness.md) beschreibt Exportprüfung, Integration und den manuellen Spielnachweis. Aktuelle Ergebnisse und der nächste Task stehen nur in [HANDOFF](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md). Der Retail-Resolver unten dient der Offline-Kompatibilitätsprüfung und ersetzt keine Xash-Integration.

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
