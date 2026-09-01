# Phasen

Vor jeder Phase die Rollen bestätigen (Basis / Engine / Ref A / Ref B). Nicht mischen.

## Phase 0 — Rollen + Vendor (abgeschlossen 2026-09-01)

- Lokaler Ordner war leer. GitHub `benjarogit/csretro` hatte noch das alte cs16-client-Monorepo — `main` am 2026-09-01 ersetzt.
- Vier Bereiche + eingefrorene Refs vendort.
- Einheitliches CMake orchestriert; Engine weiter über Waf (nativ).
- NextClient-Upstream-CMake (MSVC/vcpkg) wird **nicht** als Default-Build verwendet.

## Phase 1 — Engine-Bindungs-Analyse (abgeschlossen 2026-09-01)

- NextClient ist Overlay auf Steam-`client.dll`, kein `GetClientAPI`-Körper.
- Xash: alle Namen in `cdll_exports[]` Pflicht; Menü separat über `GetMenuAPI`.
- Entscheidung: eigener Export + eigener Körper + NextClient-Features als Module. Ref A nur gelesen.
- Server AMXX bleibt Phase-3-Lücke, kein ReGameDLL.
- Text: `docs/PHASE1-ARCHITEKTUR.md`. Kein Feature-Port, kein Code-Copy.

## Phase 2 — Steam raus

Identifizieren und entfernen oder ersetzen:

- `client/nextclient/steam_api_proxy/`
- Steam-Master (`platform/config/MasterServer.vdf`, tsarvar/hl1master)
- Protector, soweit Steam-spezifisch
- `tier2/steam_api.cpp` im SDK
- NitroApi Address-Provider 8684 Windows
- CEF/Steam-Pfade im Launcher

## Phase 3 — Minimal lauffähig

Connect, Render, Input unter Xash — ohne Feinschliff, ohne Ref-B-Features.

## Phase 4 — Gezielte Ports

Nur Ref B, ein Feature pro Durchgang, Lizenz vorher, eigener Diff.
