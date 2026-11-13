# Client bauen

Baue die vorhandenen NextClient-Komponenten in der für die Aufgabe benötigten Architektur. Der erste Port läuft auf Xash3D mit passenden lokal bereitgestellten Original-Spielbibliotheken; ausschließliches x64-Gameplay folgt danach. Die Reihenfolge steht in [Plan, Abschnitt 3.8](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md). Ein erfolgreicher Komponentenbau belegt keinen spielbaren Client. Diese Befehle starten kein Spiel und ändern die Steam-Installation nicht.

Voraussetzungen: [Build-Voraussetzungen](prerequisites.md).

## Profil wählen

- `linux` (Standard): x64-Komponenten, vcpkg `x64-linux`, Host-Compilerflags.
- `linux-port`: i386-Komponenten für den ersten Port, vcpkg `x86-linux`, `-m32` für C, C++ und Assembler. Benötigt passende 32-Bit-OpenGL- und FreeType-Bibliotheken; unter Arch liegen diese in `/usr/lib32`.
- `linux-tests` und `linux-port-tests`: passende Profile mit Testtargets. Testkonfigurationen erzeugen keine auslieferbare Engine-Bibliothek; zum Bereitstellen das Profil ohne Tests verwenden.

Alle Profile verwenden Ninja Multi-Config und getrennte Verzeichnisse unter `client/build/<preset>`. Shared-Artefakte liegen dort unter `out/bin/<config>/`. Binärdateien verschiedener Architekturen oder Konfigurationen nicht mischen.

## Komponenten bauen

```bash
scripts/doctor.sh
scripts/bootstrap.sh
scripts/build-client.sh --target xash_surface
scripts/build-client.sh --preset linux-port \
  --target client_mini --target nitro_api --target engine_mini --target gameui
```

Wiederholte `--target`-Optionen bauen diese Targets nacheinander. Ohne sie baut das Skript den vorhandenen Auslieferungssatz einschließlich Launcher und `BUILD_ALL`. Ein Installationsverzeichnis fordert die bestehende Installationsroutine an; sie stellt nicht automatisch eine funktionierende Xash3D-Laufzeit zusammen.

Weitere Optionen: `--config Debug`, `--jobs 8`, `--reconfigure`, `--clean`, `--install-dir <dir>`, `--manifest <file>`. Standardkonfiguration ist Release. Nach Presetänderungen oder neuen per CMake-Glob erfassten Quellen erneut konfigurieren.

Das Manifest enthält Version, Quellcommit, Preset, Konfiguration, Buildzeit, Größe, SHA-256, ELF-Klasse und relativen Artefaktpfad. CMake leitet die erwartete ELF-Klasse aus der Compiler-ABI ab. Leere Shared-Ausgabe oder eine abweichende Klasse lassen den Build scheitern. Das Manifest erfasst das Ausgabeverzeichnis; es bestätigt weder Callbacks noch Movement, Leistung oder Spielbarkeit.

## Komponenten ohne Spielstart prüfen

Nach dem Konfigurieren aus einem Client-Quellpfad ohne Leerzeichen:

```bash
cmake --preset linux-port-tests
cmake --build --preset linux-port-tests-release --target xash_surface-tests
LIBGL_ALWAYS_SOFTWARE=1 build/linux-port-tests/out/bin/Release/xash_surface-tests
cmake --build --preset linux-port-release --target verify-nitro-api-factory goldsrc_abi_golden
```

Die Surface-Tests verwenden einen eigenen Offscreen-Kontext. Der Factory-Probe lädt unseren Provider, das Golden-Target kompiliert ABI-Assertions. Keiner davon startet ein Spiel. Die Buildskript-Regressionen laufen mit `python3 -m unittest discover -s tests/build_scripts -v` aus dem Repository-Hauptverzeichnis und auch in CI. Spielnachweise folgen der [Laufzeit-Abnahme](../maintainers/runtime-readiness.md).

## Pfade und Bereinigung

Abhängigkeitsbuilds wie OpenSSL scheitern an Quellpfaden mit Leerzeichen. Der Wrapper verwendet den Symlink `${TMPDIR:-/tmp}/csretro-src` ohne Leerzeichen; die Dateien bleiben im Repository. Für manuelle CMake-Befehle dessen `client`-Verzeichnis betreten. Einen konfigurierten Build nicht zwischen echtem Pfad und Symlink verschieben: CMake speichert absolute Pfade.

`scripts/build-client.sh --preset linux-port --clean` baut nur dieses Profil neu. `scripts/clean.sh --client-build linux-port-tests` entfernt nur dessen Testbuild, optional mit `--dry-run`. Andere Profile bleiben erhalten. Die vollständige Bereinigung generierter Daten erfolgt weiterhin über `scripts/clean.sh`.

## Fehlersuche

- Git-LFS-Pointer beim Konfigurieren: `git lfs pull` ausführen.
- Fehlende vcpkg-Baseline: gepinntes Submodul initialisieren; bei einem flachen Klon die vollständige Historie holen.
- Falsche ELF-Klasse oder unpassende Systembibliothek: Profil und passende Systemabhängigkeiten prüfen, dann nur dieses Profil bereinigen und neu konfigurieren. Für `linux-port` ist ELF32 richtig.
- i386-Assemblerfehler bei Registern/Operanden: mit dem getrackten Port-Preset neu konfigurieren, damit auch Assemblerabhängigkeiten `CMAKE_ASM_FLAGS=-m32` erhalten.
- Fehlendes 32-Bit-GL oder FreeType: passende Multilib-Pakete aus den Voraussetzungen installieren. Eine gecachte x64-Bibliothek funktioniert nicht im i386-Build.

Historische Buildmessungen und ihre Commits stehen in [HANDOFF.md](https://github.com/benjarogit/csretro/blob/main/HANDOFF.md).
