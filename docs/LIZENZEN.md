# Lizenzen

Kein Architektur-, Phasen- oder Entwicklungs-Gate.
A1 ist dokumentiert, nicht als vollständig geklärt markiert.
Vor öffentlicher Distribution: vollständiger Audit. Repo bis dahin privat.

## Produktiv

| Bereich | Upstream | SPDX / Text | Einschränkung |
|---------|----------|-------------|---------------|
| Engine | FWGS/xash3d-fwgs | **GPL-3.0-or-later** | Ableitungen bleiben GPL-3 |
| Features / Zielverhalten | CS-NextClient/NextClient | **keine LICENSE** | ungeklärt — privater Fork |
| Client-SDK (NextClient) | ncl-hl1-source-sdk | **Source 1 SDK** (Valve) | nur kostenlos; Notices behalten |
| NitroApi | NclNitroApi | keine Root-LICENSE | an NextClient gebunden |
| Server-Protokoll | NextClientServerApi | **MIT** | Notice behalten |
| Body (A1) | Velaron/cs16-client Manifest | **GPL-2.0-or-later** + Valve HL1-SDK-Ausnahme | `client/body/LICENSE`, `ATTRIBUTION.md` |
| GameDLL (geplant) | rehlds/ReGameDLL_CS | **MIT** + Valve-SDK in den Quellen | erst nach Vendor; nicht Ref-A-Kopie |

## Referenzen

| Pfad | Lizenz | Nutzung |
|------|--------|---------|
| `refs/a-cs16-client/` | GPL-2+ + Valve-Ausnahme | Body-Quelle A1; Rest nur lesen |
| `refs/b-cs16-goldsrc/` | GPL-3.0 | Phase 4, Feature für Feature |

## Was das heißt (nicht „geklärt“)

- Engine GPL-3 + Body GPL-2+ führt keine neue Kategorie ein, ersetzt keinen Audit.
- Valve-HL1-SDK-Ausnahme: nur kostenlose Weitergabe; LICENSE der übernommenen Dateien behalten.
- NextClient ohne LICENSE blockiert ein öffentliches Repo.
- Kombination Engine + Body + NextClient + Server ist **nicht** pauschal verteilsicher.
- Ref-A-YaPB/ReGameDLL: nicht Teil von A1.

## Third-Party Engine

Opus, Ogg, Vorbis, mbedTLS, bzip2, mainui_cpp, vgui_support — Notices in den 3rdparty-Ordnern behalten.

vcpkg nicht vendort.
