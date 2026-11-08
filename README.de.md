# CSRETRO

Counter-Strike 1.6, neu aufgebaut als eigenständiges Projekt.

CSRETRO ist ein vollständig vendorter CS-1.6-Client plus AMXX-Server-API. Ziel ist der
**Default-Public-Branch** des Retail-Spiels. CSRETRO spricht ein eigenes Protokoll,
hat eine eigene Identität und Infrastruktur und ist zweisprachig dokumentiert.

> Status: frühe Entwicklung (`0.1.0-dev`). Die Gates G01–G04 sind abgeschlossen:
> Harness, vendorte Upstreams, Toolchain und ein vollständiger 32-Bit-Client-Build.
> Noch ist nichts installierbar.

## Was das ist

| Teil | Beschreibung |
|---|---|
| `client/` | Vendorter NextClient-Client mit voller Historie, ab G08 als CSRETRO gebrandet |
| `server/api/` | Vendorte NextClientServerApi mit voller Historie, AMXX-Modul |
| `docs/` | Zweisprachige Dokumentation, einzige Quelle für das GitHub-Wiki |
| `scripts/` | Einstiegspunkte für Build, Installation, Prüfung und Cleanup |
| `tools/` | Repository-Prüfungen und Generatoren |

## Erster Clone

```bash
git clone https://github.com/benjarogit/csretro.git
cd csretro
./scripts/bootstrap.sh
```

`bootstrap.sh` installiert den versionierten Push-Hook, initialisiert Git LFS,
checkt die gepinnten Submodule aus und führt die Verifikation aus.

## Nicht-Ziele

- Kein `steam_legacy`-Branch und kein 8684-Fallback. Nur Default Public.
- Keine Abwärtskompatibilität zu NextClient. CSRETRO-Server akzeptieren CSRETRO-Clients.
- Keine Vendorung von GPL-Referenzprojekten. Ideen werden credited, Code nicht kopiert.
- Windows ist kein Ziel. Linux x86 (32 Bit) im 64-Bit-Host ist das Ziel.

## Voraussetzungen

Siehe [`docs/de/build/prerequisites.md`](docs/de/build/prerequisites.md) für die
vollständige Liste, oder `scripts/doctor.sh` für die Prüfung des Hosts. Kurzfassung:
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
