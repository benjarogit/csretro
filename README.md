# CS Retro

**Repo:** https://github.com/benjarogit/csretro (`main`, privat)

NextClient-Verhalten, Laufzeit **Xash3D-FWGS**. Kein Steam-GoldSrc, kein Wechsel auf cs16-client als Produkt.

Stand: [`docs/HANDOFF.md`](docs/HANDOFF.md) · Rollen: [`docs/ROLLEN.md`](docs/ROLLEN.md)

```
Xash3D-FWGS → CS-Retro-Export → Client-Body (Ref A) → NextClient-Funktionen
```

Eine Client-Library. Ref A ist der fehlende Xash-Unterbau, nicht die Zielidee.

## Rollen

| | Rolle |
|---|--------|
| CS Retro | Produkt-Worktree |
| `engine/` | Xash3D-FWGS |
| NextClient | funktionales Ziel |
| Ref A | Body-Quelle (A1-Allowlist) |
| Ref B | bedingte Menü-Referenz, Phase 4 |
| `server/` · `bots/` | getrennte Bereiche |

## Build

```bash
export CMAKE_ROOT=/usr/share/cmake
export CC=clang CXX=clang++
./scripts/build-engine.sh
```

## Lizenz

Xash GPL-3 · NextClient ohne LICENSE · Body (A1) GPL-2+ / Valve-SDK-Ausnahme.
Repo privat. A1 nicht als vollständig geklärt markiert — Audit vor öffentlicher Distribution. [`docs/LIZENZEN.md`](docs/LIZENZEN.md)
