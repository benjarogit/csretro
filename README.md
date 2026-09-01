# CS Retro

**Repo:** https://github.com/benjarogit/csretro (`main`, privat)

NextClient-Verhalten, Laufzeit **Xash3D-FWGS**. Kein Steam-GoldSrc, kein Wechsel auf cs16-client als Produkt.
**64-Bit only** — Linux x86_64, Windows 10+ x86_64, macOS ARM64 (und Intel x86_64 soweit sinnvoll).

Stand: [`docs/HANDOFF.md`](docs/HANDOFF.md) · Rollen: [`docs/ROLLEN.md`](docs/ROLLEN.md) · Plattformen: [`docs/PLATTFORMEN.md`](docs/PLATTFORMEN.md)

```
Xash3D-FWGS → CS-Retro-Export → A1-Body → NextClient-Funktionen
Xash3D-FWGS → CS-Retro-GameDLL → optional Module → separat Bots
```

## Build

```bash
export CMAKE_ROOT=/usr/share/cmake   # CachyOS/CMake 4.4
export CC=clang CXX=clang++
./scripts/build-engine.sh
./scripts/build-client.sh
```

## Lizenz

Xash GPL-3 · NextClient ohne LICENSE · Body (A1) GPL-2+ / Valve-SDK-Ausnahme.
Repo privat. [`docs/LIZENZEN.md`](docs/LIZENZEN.md)
