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
- Form entschieden: ein Export + ein Körper + NextClient-Features als Module. Ref A in Phase 1 nur gelesen.
- **Körper-Quelle offen** — siehe Gate vor Phase 3. „Körper neu schreiben“ ist keine stillschweigende Folge von Option A.
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

## Gate — Körper-Quelle (pflicht vor Phase 3)

Phase-1-Option A legt nur die **Form** fest (eine Client-Lib, Export, Körper, Features). Sie entscheidet **nicht**, woher der Körper kommt.

Der Körper ist der gesamte CS-1.6-`cl_dll`-Umfang: Waffen, Prediction, Entities, Input, Vanilla-HUD, Tempents. Das ist der größte Einzelposten im Projekt — größer als Engine- und Server-Arbeit. NextClient enthält ihn nicht.

Ref A (`refs/a-cs16-client/`, Velaron/cs16-client) ist genau das: ein fertiger, unter Xash laufender Client-Body, Lizenz **GPL-2.0-or-later + Valve-HL1-SDK-Ausnahme**. Die Engine ist bereits GPL-3; GPL-2+ führt keine neue Lizenz-Kategorie ein. Valve-SDK-Ausnahme und Attribution bleiben Pflicht. Bisherige Regel „Ref A kein Copy“ galt gegen stilles Mischen in Phase 0/1 — nicht als Lebenszeit-Verbot, nachdem der fehlende Körper feststeht.

**Vor dem ersten Phase-3-Commit eine der beiden Zeilen wählen und hier eintragen. Ohne Eintrag kein Körper-Code.**

| | Körper-Quelle | Folge |
|---|----------------|--------|
| **A0** | Komplett neu schreiben | `refs/a-cs16-client/` bleibt eingefroren, nur lesen. Monate Arbeit, keine Rollenänderung. |
| **A1** | Ref A nur als Körper befördern | Rolle von Ref A ändert sich: `cl_dll` / `pm_shared` / zugehörige Client-Header dürfen nach `client/body/` (Vendor + GPL-Attribution). **Nicht** YaPB, **nicht** ReGameDLL, **nicht** mainui. Ein Körper im Tree, dann NextClient-Features drauf. |

Gewählt: **(offen — Benny, vor Phase 3)**  
Datum: —  
Nicht still A0 annehmen.

YaPB/ReGameDLL aus Ref A nach `bots/`/`server/` bleiben in beiden Varianten verboten.

## Phase 3 — Minimal lauffähig

Erst nach dem Gate. Connect, Render, Input unter Xash — Körper laut A0 oder A1. Ohne Feinschliff, ohne Ref-B-Features.

## Phase 4 — Gezielte Ports

Nur Ref B, ein Feature pro Durchgang, Lizenz vorher, eigener Diff.
