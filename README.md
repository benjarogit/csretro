# CS Retro

**Repo:** https://github.com/benjarogit/csretro (`main`, privat)

NextClient-Basis, Laufzeit **Xash3D-FWGS**. Kein Steam-GoldSrc.

Der aktuelle Projektstand steht in [`docs/HANDOFF.md`](docs/HANDOFF.md) — dort zuerst lesen.
Änderungen gehören ins [`CHANGELOG.md`](CHANGELOG.md).

> Dieses Repo ist **nicht** mehr das alte Xash+cs16-client-Monorepo (bis 2026-08-31, Tag `v0.2.0`).
> Jene Historie wurde am 2026-09-01 von `main` entfernt. cs16-client liegt nur noch eingefroren unter `refs/a-cs16-client/`.

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

| Datei | Inhalt |
|-------|--------|
| `docs/HANDOFF.md` | **Lebender Stand** — Phase, Remote, offene Arbeit |
| `docs/PHASEN.md` | Phasen 0–4 |
| `docs/SCHNITTSTELLEN.md` | Engine/Client/Server/Bots |
| `docs/UPSTREAM.md` | Vendor-Pins |
| `docs/LIZENZEN.md` | Lizenzen vor jedem Import |
| `CHANGELOG.md` | Versionen / Releases |

## Lizenzhinweis

Xash3D: GPL-3. NextClient: keine LICENSE. SDK: Valve Source SDK (nur kostenlos).
Repo bleibt privat. Kein öffentlicher Release, solange NextClient ungeklärt ist. Siehe `docs/LIZENZEN.md`.
