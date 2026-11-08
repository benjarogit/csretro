# Build-Voraussetzungen

CSRETRO baut auf einem **Linux-x86_64-Host** und erzeugt **32-Bit-x86-Binaries**.
CachyOS/Arch ist die Referenzdistribution; die Prüfungen sind generisch und
funktionieren auf jeder Distribution mit 32-Bit-Toolchain und X11-Bibliotheken.

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
  lib32-libxrandr lib32-libxinerama lib32-libxss lib32-libxfixes lib32-libxrender
```

Oder das Bootstrap genau diese Menge installieren lassen:

```bash
scripts/bootstrap.sh --install-deps
```

## Warum 32 Bit

GoldSrc ist eine 32-Bit-Engine. Jedes Client-Modul muss zu ihrer ABI passen, deshalb
kompiliert das Projekt mit `-m32 -march=pentium4` und vcpkg nutzt das eigene
Triplet `client/custom-vcpkg-triplets/x86-linux.cmake`. Ohne Multilib lässt sich
der Client nicht linken, egal wie modern der Host ist.

## Was der Doctor prüft

`scripts/doctor.sh` ist read-only und endet mit einem Fehlercode, wenn eine harte
Voraussetzung fehlt. Geprüft wird:

- Host-Architektur, Distribution, glibc, GCC, CMake, Ninja, Python, Git, Git LFS
- Build-Helfer: `curl`, `zip`, `unzip`, `tar`, `patch`, `make`, `pkg-config`, `perl`
- ein echter 32-Bit-Compile-and-Link-Probe für C und C++ mit `-m32 -march=pentium4`
- 32-Bit-`libGL` und die X11-Bibliotheken `X11`, `Xext`, `Xcursor`, `Xi`, `Xrandr`,
  `Xinerama`, `Xss`, `Xfixes`, `Xrender`
- ein 32-Bit-Mesa-`swrast`-Treiber für Offscreen-Rendering-Tests
- vcpkg-Submodul, vcpkg-Binary und das Triplet `x86-linux`
- initialisierte Submodule und mindestens 20 GB freien Speicherplatz

Die Steam-Runtime erscheint nur als Warnung: Sie wird zum Ausführen des
Retail-Spiels gebraucht, nicht zum Bauen.

## Bewusst nicht erforderlich

- **System-SDL2 für 32 Bit.** vcpkg baut SDL2 aus dem Quellcode und linkt es
  dynamisch gegen die SONAME der Engine. Die Header kommen aus der vendorten Kopie
  in `client/dep/SDL2/include`.
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
| `cannot find -lX11` im Probe | 32-Bit-X11-Entwicklungsdateien fehlen | `lib32-libx11` installieren |
| `-m32` scheitert vor dem Linken | 32-Bit-libc oder libstdc++ fehlt | `lib32-glibc lib32-gcc-libs` installieren |
| `GL/gl.h` nicht gefunden | 32-Bit-OpenGL-Header oder -Libs fehlen | `lib32-mesa lib32-libglvnd` installieren |
| `no 32-bit swrast driver` | 32-Bit-Mesa-Runtime fehlt | `lib32-mesa` installieren |
| `vcpkg binary: not bootstrapped` | Submodul da, Tool nicht gebaut | `scripts/bootstrap.sh` ausführen |
| `glibc ... is too old` | Distribution älter als das Minimum | neuere Distribution oder Container |
| CMake konfiguriert 64-Bit-Bibliotheken | Multilib nicht aktiv oder Triplet überschrieben | Multilib aktivieren, `x86-linux`-Triplet behalten |

CSRETRO niemals in den Steam-Spielordner installieren. Siehe
[Sicherheitsregeln](../../../AGENTS.md#10-security).
