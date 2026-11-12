# Build-Voraussetzungen

CSRETRO baut auf einem **Linux-x86_64-Host**. Der Client ist **x86_64**.
Das Server-Modul bleibt **i386**, weil ReHLDS es so lädt. CachyOS/Arch ist die
Referenzdistribution.

Das Retail-Spiel wird **nicht** zum Bauen benötigt. Es wird nur zum Ausführen des
Clients gebraucht und bleibt unangetastet in der Steam-Bibliothek.

## Mindestversionen

| Komponente | Minimum | Verifiziert auf dem Referenzhost |
|---|---|---|
| Linux-Kernel | 6.1 | 6.x |
| glibc | 2.43 | 2.44 |
| GCC / G++ | 15 | 16.2.1 |
| CMake | 3.21 | 4.4.5 |
| Ninja | 1.11 | 1.13.2 |
| Python | 3.12 | 3.14.9 |
| Git | 2.30 | 2.56.0 |
| Git LFS | 3.0 | 3.8.1 |
| Freier Speicherplatz | 20 GB | 636 GB |

`uv` ist optional und wird nur für Python-Tool-Umgebungen verwendet.

## Pakete auf Arch / CachyOS

```bash
sudo pacman -S --needed \
  base-devel cmake ninja python uv git git-lfs shellcheck \
  curl zip unzip tar patch pkgconf perl nasm \
  lib32-glibc lib32-gcc-libs lib32-mesa lib32-libglvnd \
  lib32-libx11 lib32-libxext lib32-libxcursor lib32-libxi \
  lib32-libxrandr lib32-libxinerama lib32-libxss lib32-libxfixes lib32-libxrender \
  mesa libglvnd libx11 libxext libxcursor libxi libxrandr libxinerama \
  libxss libxfixes libxrender
```

Oder das Bootstrap genau diese Menge installieren lassen:

```bash
scripts/bootstrap.sh --install-deps
```

## Warum x86_64

Der Produktprozess ist x86_64. Die Entscheidung steht in
[Plan, Abschnitt 3.7](https://github.com/benjarogit/csretro/blob/main/docs/plan/csretro-plan.md). Der Client nutzt das
vcpkg-Triplet `x64-linux`. Das Server-Modul bleibt i386 und braucht dafür
weiter die 32-Bit-Toolchain.

## Was der Doctor prüft

`scripts/doctor.sh` ist read-only und endet mit einem Fehlercode, wenn eine harte
Voraussetzung fehlt. Geprüft wird:

- Host-Architektur, Distribution, glibc, GCC, CMake, Ninja, Python, Git, Git LFS
- Build-Helfer: `curl`, `zip`, `unzip`, `tar`, `patch`, `make`, `pkg-config`, `perl`
- einen echten x86_64-Compile-and-Link-Probe für C und C++
- natives `libGL` und die X11-Bibliotheken `X11`, `Xext`, `Xcursor`, `Xi`, `Xrandr`,
  `Xinerama`, `Xss`, `Xfixes`, `Xrender`
- einen Mesa-`swrast`-Treiber für Offscreen-Rendering-Tests
- vcpkg-Submodul, vcpkg-Binary und das Triplet `x64-linux`
- die 32-Bit-Toolchain für das Server-Modul (`-m32`, ELF32)
- initialisierte Submodule und mindestens 20 GB freien Speicherplatz

Die Steam-Runtime erscheint nur als Warnung: Sie wird zum Ausführen des
Retail-Spiels gebraucht, nicht zum Bauen.

## Bewusst nicht erforderlich

- **System-SDL2.** vcpkg baut SDL2 und der Client linkt diese Kopie. Die
  Retail-`hw.so` teilt ihre SDL nicht mehr mit dem Prozess.
- **Windows-Werkzeuge.** Windows ist kein Ziel.
- **CEF.** Die Browser-Oberfläche bleibt unter Linux deaktiviert.

## Prüfen und vorbereiten

```bash
scripts/doctor.sh          # read-only Voraussetzungsprüfung
scripts/bootstrap.sh       # Hooks, LFS, Submodule, vcpkg, Verifikation
```

`bootstrap.sh` ist idempotent. Nach einem Branchwechsel oder Submodul-Update
erneut ausführen.

## Fehlersuche

| Symptom | Ursache | Lösung |
|---|---|---|
| `cannot find -lX11` im Probe | native X11-Entwicklungsdateien fehlen | `libx11` installieren |
| Server-Probe `-m32` scheitert | 32-Bit-libc oder libstdc++ fehlt | `lib32-glibc lib32-gcc-libs` installieren |
| `GL/gl.h` nicht gefunden | native OpenGL-Header oder -Libs fehlen | `mesa libglvnd` installieren |
| `no swrast driver` | Mesa-Runtime fehlt | `mesa` installieren |
| `vcpkg binary: not bootstrapped` | Submodul da, Tool nicht gebaut | `scripts/bootstrap.sh` ausführen |
| `glibc ... is too old` | Distribution älter als das Minimum | neuere Distribution oder Container |
| CMake bleibt auf dem alten Triplet | Cache aus `x86-linux` | `scripts/build-client.sh --clean` |

CSRETRO niemals in den Steam-Spielordner installieren. Siehe
[Sicherheitsregeln](https://github.com/benjarogit/csretro/blob/main/AGENTS.md#10-security).
