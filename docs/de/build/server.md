# Server-API bauen

Die Server-API ist ein AMX-Mod-X-Modul. ReHLDS lädt es über Metamod, und Plugins
nutzen seine Natives über `next_client_api.inc`. Es ist ein **32-Bit-i386-Shared-
Object**, wie jedes GoldSrc-Servermodul.

Voraussetzungen: [Build-Voraussetzungen](prerequisites.md).

## Schnellstart

```bash
scripts/doctor.sh          # Host prüfen, inklusive -m32
scripts/build-server.sh    # Toolchain holen, konfigurieren, bauen, testen, Manifest
```

Das Skript lädt beim ersten Lauf die Pawn-Toolchain, konfiguriert mit Tests,
baut, führt CTest aus und schreibt ein Manifest. Ein zweiter Lauf ist ein No-op.

## Pawn-Toolchain

Zwei Upstream-Pakete werden gebraucht, beide sind nicht vendort:

| Paket | Version | Zweck |
|---|---|---|
| AMX Mod X Base | `1.10.0.5487` | Compiler `amxxpc` und die Standard-Includes |
| ReAPI | `5.29.0.358` | `reapi.inc` und das ReAPI-Modul |

Beide URLs und ihre SHA-256-Digests stehen in `versions.lock.json` unter
`server_toolchain`. `scripts/fetch-server-toolchain.sh` lädt sie nach
`.cache/downloads/`, prüft jeden Digest, entpackt sie nach `.cache/toolchain/`
und führt die beiden Include-Verzeichnisse zu einem zusammen.

`amxxpc` öffnet `amxxpc32.so` per `dlopen()` über den Namen, läuft also nur, wenn
sein eigenes Verzeichnis in `LD_LIBRARY_PATH` liegt. Das Skript erzeugt dafür
einen kleinen Wrapper; Aufrufer behandeln ihn wie einen normalen Compiler:

```bash
eval "$(scripts/fetch-server-toolchain.sh)"
"$AMXXPC" plugin.sma -i"$PAWN_INCLUDE_DIR" -oplugin.amxx
```

`.cache/` ist per gitignore ausgeschlossen. `scripts/clean.sh` entfernt es.

## Konfiguration

`scripts/build-server.sh` ruft CMake direkt auf; ein Preset gibt es nicht, weil
das Modul nur eine unterstützte Konfiguration hat.

| Einstellung | Wert |
|---|---|
| CMake-Minimum | 3.21 |
| C++-Standard | C++17 |
| Ziel-ABI | `-m32`, Suffix `_i386.so` |
| Linken | `-static-libstdc++ -static-libgcc` |
| Build-Verzeichnis | `server/api/build/linux` |

| Option | Standard | Bedeutung |
|---|---|---|
| `NEXTCLIENTAPI_BUILD_TESTS` | `OFF` | Testsuite bauen und registrieren |
| `NEXTCLIENTAPI_PAWN_COMPILER` | leer | `amxxpc`-Wrapper |
| `NEXTCLIENTAPI_PAWN_INCLUDE_DIR` | leer | zusammengeführte AMXX- und ReAPI-Includes |

Das Build-Skript setzt alle drei, sobald Tests aktiv sind. Nur eine der beiden
Pawn-Variablen zu setzen ist ein harter Konfigurationsfehler.

Die Abhängigkeiten `kangaru` (v4.3.2), `sigslot` (v1.2.0) und `magic_enum`
(v0.9.7) holt `server/api/cmake/Dependencies.cmake` zur Konfigurationszeit,
gepinnt per Tag oder Commit. Die erste Konfiguration braucht daher Netz.

## Targets und Artefakte

| Target | Artefakt | Typ | Zweck |
|---|---|---|---|
| `nextclientapi_amxx` | `nextclientapi_amxx_i386.so` | ELF32 | das Modul |
| `rsa_keygen` | `deps/rsa_keygen/rsa_keygen` | ELF32 | Entwicklungswerkzeug für Schlüssel |
| `material-visibility-tests` | Host-Binary | ELF64 | native Policy-Tests |
| `entity-sync-packets-tests` | Host-Binary | ELF64 | native Paket-Tests |
| `material-entity-sync-tests` | Host-Binary | ELF64 | native Szenario-Tests |

Die Tests sind Host-Programme: Sie prüfen engine-freien Policy-Code und werden
für die Host-ABI übersetzt. Nur Modul und `rsa_keygen` müssen 32 Bit sein; das
Skript bricht ab, wenn das Modul nicht `ELF32` ist.

## Tests

`scripts/build-server.sh` führt die komplette Suite aus. Referenzlauf:
**34/34 bestanden in etwa einer Sekunde**.

| Gruppe | Anzahl | Inhalt |
|---|---|---|
| `pawn-material-plugin` | 1 | kompiliert `amxx_test/.../test_nextclient_api.sma` mit `amxxpc` und scheitert bei jeder Warnung |
| `material-visibility-policy` | 1 | wer ein Material sehen darf |
| `entity-sync-packets` | 1 | Paketlayout |
| `material-*`-Szenarien | 31 | Binden, Zulassung, Eigentum, Wiederverwendung, Unbind |

Der Pawn-Test läuft nur, wenn beide Pawn-Variablen gesetzt sind; das Build-Skript
setzt sie.

## Modul-Metadaten

Die Metadaten stammen aus `server/api/CMakeLists.txt` und landen im Binary:

| Feld | Wert |
|---|---|
| Name | `Next Client API` |
| Version | `1.6.0` (aus `project(... VERSION ...)`) |
| Log-Tag | `nclapi` |

`scripts/build-server.sh` liest die erwarteten Werte aus dieser Datei und bricht
ab, wenn das gebaute Modul sie nicht enthält. Außerdem stehen die `NEEDED`-
Bibliotheken im Manifest; das Modul linkt nur `libm`, `libc` und den Loader,
weil libstdc++ und libgcc statisch sind.

## Referenzbuild (G05, 8. November 2026, Release)

| Artefakt | Größe | Typ |
|---|---:|---|
| `nextclientapi_amxx_i386.so` | 8.863.316 | ELF32 |
| `deps/rsa_keygen/rsa_keygen` | 4.437.032 | ELF32 |
| `material-entity-sync-tests` | 672.760 | ELF64 |
| `entity-sync-packets-tests` | 136.040 | ELF64 |
| `material-visibility-tests` | 22.944 | ELF64 |

| Schritt | Zeit |
|---|---:|
| Konfiguration inklusive Dependency-Fetch | 8 s |
| Build | 17 s |
| CTest, alle 34 Tests | 1 s |
| Kompletter Clean-Rebuild | 27 s |

## Upstream-Patches

Zwei Portabilitäts-Fixes sind in `server/api/deps/rsa_keygen/` angewendet und in
[`patches/README.md`](https://github.com/benjarogit/csretro/blob/main/patches/README.md) dokumentiert. Sie betreffen nur
ein Entwicklungswerkzeug, nicht das Modul.

## Bekannte Warnungen

- `cmake_minimum_required` unter 3.10 in den vendorten `kangaru` und `sigslot` —
  Upstream, nur eine Warnung, verschwindet beim Aktualisieren der Pins.
- `creating DT_TEXTREL` beim Linken von `rsa_keygen` gegen das statische OpenSSL —
  Upstream, für ein Entwicklungswerkzeug unkritisch.

## Fehlersuche

| Symptom | Ursache | Lösung |
|---|---|---|
| `compiler failed to instantiate: amxxpc32.so` | `amxxpc` ohne sein Verzeichnis in `LD_LIBRARY_PATH` | `$AMXXPC` aus `scripts/fetch-server-toolchain.sh` verwenden |
| `SHA-256 mismatch` | beschädigter oder geänderter Download | Datei in `.cache/downloads/` löschen und erneut ausführen |
| `Missing PAWN_COMPILER` | nur eine der beiden Pawn-Variablen gesetzt | beide setzen oder keine |
| `void main must return int` | ungepatchtes Upstream-`rsa_keygen` | `patches/server-api/0001-rsa-keygen-portability.patch` anwenden |
| `i386 architecture ... incompatible with i386:x86-64` | ein Host-Werkzeug linkt die 32-Bit-OpenSSL-Archive | derselbe Patch |
| `could not find baseline` oder Fetch-Fehler | kein Netz bei der ersten Konfiguration | einmal verbinden oder `server/api/build/linux/_deps` vorbefüllen |
| Modul ist `ELF64` | `-m32`-Flags entfernt | `NAPI_TARGET_PROPS` unverändert lassen |
| `material-*`-Test schlägt fehl | Policy-Regression | Policy korrigieren, nie die Assertion |
