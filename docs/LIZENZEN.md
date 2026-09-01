# Lizenzen (Stand Phase 0, 2026-09-01)

Geprüft **bevor** irgendetwas aus Referenz A/B in die Basis wandert.
Kein Code aus A/B ist bisher übernommen.

## Produktiv (wird entwickelt)

| Bereich | Upstream | SPDX / Text | Einschränkung |
|---------|----------|-------------|---------------|
| Engine | FWGS/xash3d-fwgs | **GPL-3.0-or-later** (Header in `Documentation/gpl_copyright_header.h`; GitHub-API ohne SPDX) | Ableitungen der Engine bleiben GPL-3 |
| Client | CS-NextClient/NextClient | **keine LICENSE-Datei**, GitHub `license: null` | rechtlich ungeklärt — nur privater Fork, keine Weitergabe ohne Klärung |
| Client-SDK | CS-NextClient/ncl-hl1-source-sdk | **Source 1 SDK License** (Valve) | nur kostenlos verteilbar; LICENSE + `thirdpartylegalnotices.txt` mitliefern |
| NitroApi | CS-NextClient/NclNitroApi | keine eigene LICENSE im Root | an NextClient gebunden |
| Server | CS-NextClient/NextClientServerApi | **MIT** (Copyright 2022 Next21 Team) | MIT-Notice behalten |

## Eingefroren (nur lesen)

| Pfad | Upstream | Lizenz | Hinweis |
|------|----------|--------|---------|
| `refs/a-cs16-client/` | Velaron/cs16-client | **GPL-2.0-or-later** + Valve HL1-SDK-Ausnahme | plus eingebettetes HL1-SDK |
| `refs/b-cs16-goldsrc/` | FuryBaM/cs16-goldsrc-client | **GPL-3.0** (`LICENSE.txt`) | Phase 4: vor jedem Port prüfen, ob der Nachbau GPL-kompatibel bleibt |

## Konflikt (nicht wegbügeln)

1. Xash3D ist GPL-3. Eine verteilte Binärkombination mit Valve-SDK-Teilen und NextClient (ohne Lizenz) ist **nicht sauber klärbar**.
2. Valve Source/HL1-SDK: nur kostenlose Weitergabe, LICENSE-Datei Pflicht.
3. NextClient ohne Lizenz: **kein öffentlicher GitHub-Release**, solange das ungeklärt ist.
4. Server-MIT allein ist unproblematisch; die Kopplung an das Client-Protokoll ändert nichts an (1)–(3).

## Third-Party im Engine-Tree (Auszug)

Opus, Ogg, Vorbis, mbedTLS, bzip2, mainui_cpp, vgui_support, gl4es, … — jeweils eigene Dateien im jeweiligen 3rdparty-Ordner. Nicht entfernen.

## vcpkg

Microsoft vcpkg wurde **nicht** vendort (Package-Manager, keine Projektquelle).
