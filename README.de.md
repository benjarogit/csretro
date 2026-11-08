# CSRETRO

Counter-Strike 1.6, neu aufgebaut als eigenständiges Projekt.

CSRETRO ist ein vollständig vendorter CS-1.6-Client plus AMXX-Server-API. Ziel ist der
**Default-Public-Branch** des Retail-Spiels. CSRETRO spricht ein eigenes Protokoll,
hat eine eigene Identität und Infrastruktur und ist zweisprachig dokumentiert.

> Status: frühe Entwicklung (`0.1.0-dev`). Gate G01 (Repository und Harness) läuft.
> Noch ist nichts installierbar.

## Was das ist

| Teil | Beschreibung |
|---|---|
| `client/` | Vendorter NextClient-Client, ab G02/G08 als CSRETRO gebrandet |
| `server/api/` | Vendorte NextClientServerApi, AMXX-Modul für CSRETRO-Server |
| `docs/` | Zweisprachige Dokumentation, einzige Quelle für das GitHub-Wiki |
| `scripts/` | Einstiegspunkte für Build, Installation, Prüfung und Cleanup |
| `tools/` | Repository-Prüfungen und Generatoren |

## Nicht-Ziele

- Kein `steam_legacy`-Branch und kein 8684-Fallback. Nur Default Public.
- Keine Abwärtskompatibilität zu NextClient. CSRETRO-Server akzeptieren CSRETRO-Clients.
- Keine Vendorung von GPL-Referenzprojekten. Ideen werden credited, Code nicht kopiert.
- Windows ist kein Ziel. Linux x86 (32 Bit) im 64-Bit-Host ist das Ziel.

## Voraussetzungen

Siehe den [Dokumentationsindex](docs/de/index.md) für die aktuellen Kapitel. G03
ergänzt `docs/de/build/prerequisites.md` mit der vollständigen Paketliste. Kurzfassung:
Linux x86_64, Multilib, GCC, CMake, Ninja, Python, Git LFS.

## Dokumentation

- [Projektplan](docs/plan/csretro-plan.md) (Deutsch, maßgeblich)
- [Dokumentationsindex, Deutsch](docs/de/index.md)
- [Documentation index, English](docs/en/index.md)
- [Credits](CREDITS.md)

## Arbeitsregeln

Der Harness-Vertrag steht in [`AGENTS.md`](AGENTS.md). Jede Aufgabe wird umgesetzt,
verifiziert, dokumentiert, committet und gepusht, bevor die nächste beginnt.

## Lizenz

Es ist noch keine Lizenz gewählt. Das Repository ist privat. Vendorte Bestandteile
behalten ihre Upstream-Lizenzen; siehe [`CREDITS.md`](CREDITS.md) und die Lizenzdateien
in den vendorten Bäumen.
