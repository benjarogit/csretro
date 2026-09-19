# refs/

Kein `add_subdirectory` aus dem Root-Build. Kein Blanket-Import. Kein zweiter Produktclient.

| Pfad | Rolle |
|------|--------|
| `a-cs16-client/` | Body-Quelle A1. Sonst nur lesen. Nicht als zweiten Client bauen. |
| `b-cs16-goldsrc/` | Ref B — Vergleich, ein Feature |
| `primext/` | PrimeXT-Vergleichsbaum, gitignored. Pin: Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23). Nicht bauen. Siehe `docs/research/px1-primext.md`. |

Herkunft: `docs/upstream.de.md`. Lizenzen: `docs/licenses.de.md`. Laufzeitrollen: `docs/architecture.de.md`.
