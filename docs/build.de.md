# Build

## Umfang

Dieser Weg beschreibt den bestehenden Linux-x86_64-Entwicklungsbuild.
Er ist keine fertige Installation und keine geprüfte Windows-/macOS-Anleitung.
Für den Start gelten zusätzlich die [Spieldaten-Voraussetzungen](getting-started.md).

Benötigt werden Git, Clang/Clang++, CMake, Ninja, Python 3, pkg-config sowie SDL2-
und OpenGL-Entwicklungsdateien. Systemabhängig kommen Bibliotheks-/Headerpakete hinzu;
die Waf-Konfiguration nennt fehlende Abhängigkeiten. Die Engine bringt mehrere Codec-Quellen mit.
Es gibt noch kein hier getestetes Installationsrezept für jede Distribution.

## Quellen und Komponenten

```bash
git clone https://github.com/benjarogit/csretro.git
cd csretro
export CC=clang CXX=clang++

./scripts/build-engine.sh
./scripts/build-client.sh
./scripts/build-gamedll.sh
./scripts/build-menu.sh
```

Die Quellen sind vendort; ein rekursives Submodule-Update ist nicht erforderlich.
Client und Menü verwenden denselben CMake-Buildbaum: diese beiden Skripte nacheinander ausführen.

| Komponente | Typische Ausgabe |
| --- | --- |
| Engine | `build/engine/game_launch/xash3d` |
| Client | `build/client-cmake/client/client_amd64.so` |
| Server | `build/gamedll-cmake/cs_amd64.so` |
| Menü | `build/client-cmake/menu/menu_amd64.so` |

Alle geladenen Bibliotheken müssen zur gleichen Architektur und zum selben Interface-Stand passen.
Nicht einzelne Libraries aus fremden Downloads hineinmischen.

## Daten und Start

```bash
python3 scripts/bootstrap-gamedata.py
python3 scripts/bootstrap-gamedata.py --status
# Erst starten, wenn auch die zusätzlichen Assets vollständig vorhanden sind:
./scripts/play.sh
```

Die zusätzlichen Granaten-Assets sind eine bekannte Einstiegshürde, kein Bestandteil des Steam-Imports.
Siehe [Einstieg](getting-started.md). Die Website lässt sich unabhängig davon bauen.

## Prüfen

```bash
git diff --check
./scripts/smoke-gamedll.sh dedicated
```

Der Smoke-Test benötigt vorbereitete Spieldaten. Ein erfolgreicher Link- oder Serverstart prüft
keine Mausinteraktion, Animation oder Menüoptik. Berichte getrennt über Build, automatisierte
Tests und manuelle Spieltests; gib Commit und lokale Abweichungen an.

## Häufige Stolperstellen

- Fehlende `client_amd64.so`, `cs_amd64.so` oder Menü-Lib: passende Komponente bauen.
- Fehlende Modelle/Sprites: Datenproblem nicht durch beliebige Downloads verdecken.
- CMake-Modulpfade: `CMAKE_ROOT` nicht auf einen fremden Systempfad festlegen;
  die installierte CMake-Version und ihr Modulverzeichnis müssen zusammenpassen.
- Plattformport: vorhandene CMake-Presets sind kein Beleg für einen vollständig getesteten Port.
