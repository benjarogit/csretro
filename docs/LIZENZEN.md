# Lizenzen

Geprüft, bevor Code wandert. **Kein Architektur-, Phasen- oder Entwicklungs-Gate.**
**A1 (2026-09-01) ist dokumentiert, nicht als vollständig geklärt markiert.**
Vor einer öffentlichen Distribution: vollständiger Lizenz-Audit. Repo bis dahin privat.

Kein Ref-A-Code ist bisher in `client/body/` (Phase 3). Attribution/Lizenztexte beim späteren Vendor **mitkopieren**, nicht ersetzen.

## Produktiv

| Bereich | Upstream | SPDX / Text | Einschränkung |
|---------|----------|-------------|---------------|
| Engine | FWGS/xash3d-fwgs | **GPL-3.0-or-later** | Ableitungen bleiben GPL-3 |
| Features / Zielverhalten | CS-NextClient/NextClient | **keine LICENSE** | ungeklärt — privater Fork |
| Client-SDK (NextClient) | ncl-hl1-source-sdk | **Source 1 SDK** (Valve) | nur kostenlos; LICENSE + thirdparty notices |
| NitroApi | NclNitroApi | keine Root-LICENSE | an NextClient gebunden |
| Server | NextClientServerApi | **MIT** | Notice behalten |
| Body (A1, ab Phase 3) | Velaron/cs16-client Allowlist | **GPL-2.0-or-later** + Valve HL1-SDK-Ausnahme | Attribution Pflicht; YaPB/ReGameDLL/mainui nicht übernommen |

## Referenzen

| Pfad | Lizenz | Nutzung |
|------|--------|---------|
| `refs/a-cs16-client/` | GPL-2+ + Valve-Ausnahme | Body-Quelle A1; Rest nur lesen |
| `refs/b-cs16-goldsrc/` | GPL-3.0 | Phase 4, Feature für Feature |

## A1 — was das lizenzrechtlich heißt (nicht „geklärt“)

- Engine ist schon GPL-3. GPL-2+ am Body führt keine neue Kategorie ein, ersetzt aber keinen Audit.
- Valve-HL1-SDK-Ausnahme: nur kostenlose Weitergabe; LICENSE der übernommenen Dateien behalten.
- NextClient ohne LICENSE bleibt der Blocker für ein öffentliches Repo — unabhängig von A1.
- Kombination Engine (GPL-3) + Body (GPL-2+/Valve) + NextClient (keine Lizenz) + Server (MIT) ist **nicht** pauschal verteilsicher.
- YaPB/ReGameDLL aus Ref A: nicht Teil von A1, eigene Lizenzen, bleiben draußen.

## Konflikt (unverändert)

1. NextClient ohne Lizenz + Valve-SDK + GPL-Engine = keine saubere öffentliche Binärdistribution.
2. Interne Releases auf dem privaten Repo sind in Ordnung.
3. Server-MIT allein ist unproblematisch.

## Third-Party Engine

Opus, Ogg, Vorbis, mbedTLS, bzip2, mainui_cpp, vgui_support, … — Notices in den 3rdparty-Ordnern behalten.

vcpkg nicht vendort.
