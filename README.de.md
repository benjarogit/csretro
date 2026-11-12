# CSRETRO

Counter-Strike 1.6, neu aufgebaut als eigenes Produkt.

Der Client ist **noch nicht spielbar**. Für Originalinhalte brauchst du später
eine retail Counter-Strike-1.6-Installation. CSRETRO nie in den Steam-Spieleordner
legen und nie mit einem modifizierten Client auf einen VAC-Server.

CSRETRO ist ein vollständig vendorter CS-1.6-Client plus AMXX-Server-API. Ziel ist
der **Default-Public-Branch** des Retail-Spiels, eigenes Protokoll, eigene Identität
und Infrastruktur, Dokumentation auf Deutsch und Englisch.

> Status: frühe Entwicklung (`1.0.0-dev`). Produktprozess ist Linux **x86_64**.
> Die Gates G01–G05 sind abgeschlossen. G07 (Default-Public-Laufzeit auf Xash3D)
> läuft.

## Was das ist

| Teil | Beschreibung |
|---|---|
| `client/` | Vendorter NextClient-Client mit voller Historie, Rebrand ab G08 |
| `server/api/` | Vendorte NextClientServerApi mit voller Historie, AMXX-Modul |
| `docs/` | Zweisprachige Dokumentation, einzige Quelle für die GitHub Page |
| `scripts/` | Einstiege für Build, Install, Verify und Clean |
| `tools/` | Repository-Checks und Generatoren |

## Erster Clone

```bash
git clone https://github.com/benjarogit/csretro.git
cd csretro
./scripts/bootstrap.sh
```

`bootstrap.sh` setzt den getrackten Push-Hook, initialisiert Git LFS, checkt die
gepinnten Submodule aus und führt die Verifikation aus.

## Nicht-Ziele

- Kein `steam_legacy`-Branch und kein 8684-Fallback. Nur Default Public.
- Keine Abwärtskompatibilität zu NextClient. CSRETRO-Server akzeptieren CSRETRO-Clients.
- Kein Vendoring von GPL-Referenzprojekten. Ideen werden credited, Code nicht kopiert.
- Windows ist kein Ziel. Der Produktprozess ist Linux x86_64.

## Voraussetzungen

Siehe [`docs/de/build/prerequisites.md`](docs/de/build/prerequisites.md) oder
`scripts/doctor.sh`. Kurz: Linux x86_64, Multilib, GCC, CMake, Ninja, Python, Git LFS.

## Dokumentation

Veröffentlichte Seite (aus `docs/de` und `docs/en`):
https://benjarogit.github.io/csretro/

- [Projektplan](docs/plan/csretro-plan.md) (Deutsch, maßgeblich)
- [Dokumentationsindex, Deutsch](docs/de/index.md)
- [Documentation index, English](docs/en/index.md)
- [Credits](CREDITS.md)
- [Security-Richtlinie](SECURITY.md)

## Arbeitsregeln

Der Harness-Vertrag steht in [`AGENTS.md`](AGENTS.md). Jede Aufgabe wird
umgesetzt, verifiziert, dokumentiert, committet und gepusht, bevor die nächste
beginnt. Zusammenarbeit läuft über Issues und Pull Requests. GitHub Actions
führen nur Befehle aus.

## Lizenz

Für den CSRETRO-Kleber ist noch keine Lizenz gewählt. Das Repository ist öffentlich.
Vendored Komponenten behalten ihre Upstream-Lizenzen; siehe [`CREDITS.md`](CREDITS.md)
und die Lizenzdateien in den vendorten Bäumen. Ohne Kleber-Lizenz werden keine
fremden Patches angenommen.
