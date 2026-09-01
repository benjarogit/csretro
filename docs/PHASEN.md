# Phasen

Vor jeder Phase die Rollen bestätigen (Basis / Engine / Ref A / Ref B). Nicht mischen.

## Phase 0 — Rollen + Vendor (diese Session)

- Leerer Ordner, kein Alt-Monorepo vorhanden (nichts zu retten).
- Vier Bereiche + eingefrorene Refs vendort.
- Einheitliches CMake orchestriert; Engine weiter über Waf (nativ).
- NextClient-Upstream-CMake (MSVC/vcpkg) wird **nicht** als Default-Build verwendet.

## Phase 1 — Engine-Bindungs-Analyse (als Nächstes)

- `ncl-hl1-source-sdk` vs. Xash `cldll_func_t` / MainUI / vgui_support.
- Ref A **lesen**: CMake, `cdll_int.cpp`, Header — kein Copy.
- Ergebnis: Liste der Anpassungen, kein Feature-Port.

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
