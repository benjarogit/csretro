# Client bauen

Der Client ist ein x86_64-Build. Valves i386-`hw.so` wird nicht geladen.
Die Entscheidung steht in [Plan, Abschnitt 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md).
Nichts davon berührt die Steam-Installation.

Voraussetzungen: [Build-Voraussetzungen](prerequisites.md).

## Schnellstart

```bash
scripts/doctor.sh          # Host prüfen
scripts/bootstrap.sh       # Hooks, LFS, Submodule, vcpkg
scripts/build-client.sh    # konfiguriert bei Bedarf, baut, schreibt ein Manifest
```

## Pfade mit Leerzeichen

Mehrere Dependency-Builds, besonders OpenSSLs `Configure`, brechen ab, wenn der
Build-Pfad Leerzeichen enthält. Dieses Repository kann unter einem solchen Pfad
liegen.

`scripts/build-client.sh` erkennt das und baut über einen space-freien Symlink:

```text
/tmp/csretro-src -> <Repository>
```

Die Dateien bleiben im Repository; nur der Pfad, den der Compiler sieht, ist ein
anderer. Wer CMake manuell aus einem Pfad mit Leerzeichen aufruft, bekommt den
OpenSSL-Fehler. Also das Skript verwenden oder aus einem space-freien Checkout
konfigurieren.

Ein vorhandenes `build/linux` nicht zwischen echtem Pfad und Symlink verschieben:
CMake speichert absolute Pfade. Nach einem solchen Wechsel
`scripts/build-client.sh --clean` ausführen.

## Konfiguration

Das Preset `linux` ist ein Ninja-Multi-Config-Preset:

| Einstellung | Wert |
|---|---|
| Generator | Ninja Multi-Config |
| Triplet | `x64-linux` (`client/vcpkg/triplets/x64-linux.cmake`) |
| C/C++-Flags | Host-Standard, ohne `-m32` |
| Build-Verzeichnis | `client/build/linux` |
| Ausgabe | `client/build/linux/out/bin/<config>/` |

## Targets

`scripts/build-client.sh` baut in Abhängigkeitsreihenfolge:

| Target | Artefakt | Zweck |
|---|---|---|
| `client_mini` | `client_mini.dll` | HUD, View, Waffen, Radar |
| `engine_mini` | `next_engine_mini.dll` | Laufzeit-Detours und Engine-Patches |
| `nitro_api` | `nitro_api2.dll` | Engine-Adressen und Hook-Schicht |
| `gameui` | `cstrike/cl_dlls/gameui.so` | native VGUI2-Menüs |
| `cstrike.exe` | `cstrike.exe` | Launcher und Prozesseinstieg |
| `BUILD_ALL` | alle Shipping-Module | zusätzlich `FileSystem_Proxy.dll`, `steam_api.dll`, `vgui2.so` |

Ein einzelnes Target bauen:

```bash
scripts/build-client.sh --target client_mini
```

Weitere Optionen: `--config Debug`, `--jobs 8`, `--reconfigure`, `--clean`,
`--install-dir <dir>`, `--manifest <file>`.

## Referenzbuild (G04, 8. November 2026, damaliger 32-Bit-Stand)

Die Tabelle ist der abgeschlossene G04-Nachweis. Der Produkt-Build ist seit
dem x86_64-Wechsel ELF64.

| Artefakt | Größe | Typ |
|---|---:|---|
| `client_mini.dll` | 5.482.420 | ELF32 |
| `next_engine_mini.dll` | 26.130.800 | ELF32 |
| `nitro_api2.dll` | 15.000.184 | ELF32 |
| `cstrike/cl_dlls/gameui.so` | 8.785.140 | ELF32 |
| `cstrike.exe` | 19.974.812 | ELF32 |
| `FileSystem_Proxy.dll` | 2.117.508 | ELF32 |
| `steam_api.dll` | 1.748.084 | ELF32 |
| `vgui2.so` | 2.597.024 | ELF32 |

Gemessen auf dem Referenzhost:

| Schritt | Zeit |
|---|---:|
| Konfiguration inklusive aller vcpkg-Ports | 122 s |
| `client_mini` | 32 s |
| `engine_mini` | 56 s |
| `nitro_api` | 104 s |
| `gameui` | 18 s |
| `cstrike.exe` | 9 s |
| `BUILD_ALL` | 3 s |

Speicher: `client/build/linux` etwa 282 MB, `client/vcpkg/downloads` etwa 90 MB.

Das Skript schreibt ein Manifest mit Version, Commit, Konfiguration, Buildzeit und
einer Zeile pro Artefakt:

```text
# size_bytes  sha256  elf  path
5482420  <sha256>  ELF64  client_mini.dll
```

Der Build bricht ab, wenn ein Artefakt nicht `ELF64` ist.

## Bekannte Warnungen

- `creating DT_TEXTREL in a shared object` — Upstream-Code, erwartet, nicht fatal.
- `cmake_minimum_required` unter 3.10 im vendorten SDK — Upstream, wird beim
  Rebranding angegangen.
- `FetchContent_Populate is deprecated` — Upstream `dep/funchook`.

Keine davon verändert das Ergebnis.

## Fehlersuche

| Symptom | Ursache | Lösung |
|---|---|---|
| `is a Git LFS pointer file` beim Konfigurieren | LFS-Objekte nicht geladen | `git lfs pull` |
| `could not find baseline version` von vcpkg | vcpkg-Submodul ist ein Shallow-Clone | `git -C client/vcpkg fetch --unshallow` |
| OpenSSLs `Configure` scheitert an Leerzeichen | Build-Pfad enthält Leerzeichen | `scripts/build-client.sh` verwenden, es baut über einen space-freien Symlink |
| Artefakt ist `ELF32` | altes Triplet oder `-m32` noch aktiv | Preset `linux` und Triplet `x64-linux` behalten |
| CMake findet keine Bibliothek | vcpkg-Cache vom alten Triplet | `scripts/build-client.sh --clean` |
| vcpkg-Install scheitert nach Toolchain-Wechsel | veralteter Cache oder Build-Trees | `scripts/build-client.sh --clean` |
