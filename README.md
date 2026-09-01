# CS Retro

NextClient-Basis, Laufzeit **Xash3D-FWGS**. Kein Steam-GoldSrc.

## Rollen (bindend)

| Bereich | Rolle | Quelle |
|---------|--------|--------|
| `engine/` | Ziel-Engine | Xash3D-FWGS |
| `client/` | Basis (entwickeln) | NextClient + NitroApi + ncl-hl1-source-sdk |
| `server/` | Basis (entwickeln) | NextClientServerApi |
| `bots/` | eigener Bereich | noch leer |
| `refs/a-cs16-client/` | Referenz A, eingefroren | Velaron/cs16-client — nur Engine-Bindung verstehen |
| `refs/b-cs16-goldsrc/` | Referenz B, eingefroren | FuryBaM — erst Phase 4, ein Feature |

Kein Merge aus den Refs. Ein Feature, ein Schritt. Lizenzen: `docs/LIZENZEN.md`.

## Build

Aktuelles CMake + Clang. Engine nativ per Waf, orchestriert vom Root-CMake.

```bash
export CMAKE_ROOT=/usr/share/cmake   # Workaround defektes CMake-Paket
export CC=clang CXX=clang++
./scripts/build-engine.sh
```

Presets: `linux-clang-x86_64` (aktiv), `linux-clang-i686`, `linux-clang-aarch64` (Toolchains liegen, Cross-Compiler lokal noch nicht).

## Dokumentation

- `docs/HANDOFF.md` — anderer Rechner
- `docs/PHASEN.md` — 0–4
- `docs/SCHNITTSTELLEN.md` — Engine/Client/Server/Bots
- `docs/UPSTREAM.md` — Vendor-Pins
- `CHANGELOG.md`

## Lizenzhinweis

Xash3D: GPL-3. NextClient: keine LICENSE. SDK: Valve Source SDK (nur kostenlos).
Kein öffentlicher Release, solange das ungeklärt ist. Siehe `docs/LIZENZEN.md`.
