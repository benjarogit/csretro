# Phasen

Vor jeder Phase die Rollen in `docs/ROLLEN.md` bestätigen. Nicht mischen.

## Phase 0 — Rollen + Vendor (abgeschlossen 2026-09-01)

- Lokaler Ordner war leer. GitHub `benjarogit/csretro` hatte noch das alte cs16-client-Monorepo — `main` am 2026-09-01 ersetzt.
- Vier Bereiche + eingefrorene Refs vendort.
- Einheitliches CMake orchestriert; Engine weiter über Waf (nativ).
- NextClient-Upstream-CMake (MSVC/vcpkg) wird **nicht** als Default-Build verwendet.

## Phase 1 — Engine-Bindungs-Analyse (abgeschlossen 2026-09-01)

- NextClient ist Overlay auf Steam-`client.dll`, kein `GetClientAPI`-Körper.
- Xash: alle Namen in `cdll_exports[]` Pflicht; Menü separat über `GetMenuAPI`.
- Form entschieden: ein Export + ein Körper + NextClient-Features als Module.
- Körper-Quelle: Gate **A1** (2026-09-01) — siehe unten.
- Server AMXX bleibt Phase-3-Lücke, kein ReGameDLL.
- Text: `docs/PHASE1-ARCHITEKTUR.md`. In Phase 1 noch kein Feature-Port und kein Body-Vendor.

## Phase 2 — Steam-/Hook-/Proxy-Schnitt (als Nächstes)

Kein Körper-Code. Keine Architekturänderung. Linux und Xash-Plattformen mitdenken.

- Steam-Abhängigkeiten identifizieren und entfernen/ersetzen
- NitroApi- / Build-8684-Abhängigkeiten entfernen
- Steam-Master- / Steam-Pfade entfernen
- direkte Xash-Anbindung vorbereiten (Inventar, nicht Body-Vendor)

Konkret u. a.: `steam_api_proxy/`, Master/Tsarvar, Protector soweit Steam, `tier2/steam_api.cpp`, 8684-Address-Provider, CEF/Steam-Launcher-Pfade.

## Gate — Körper-Quelle (pflicht vor Phase 3)

Phase-1-Option A legt nur die **Form** fest (eine Client-Lib, Export, Körper, Features). Sie entscheidet **nicht**, woher der Körper kommt.

Der Körper ist der gesamte CS-1.6-`cl_dll`-Umfang: Waffen, Prediction, Entities, Input, Vanilla-HUD, Tempents. Das ist der größte Einzelposten im Projekt — größer als Engine- und Server-Arbeit. NextClient enthält ihn nicht.

Ref A (`refs/a-cs16-client/`, Velaron/cs16-client) ist genau das: ein fertiger, unter Xash laufender Client-Body, Lizenz **GPL-2.0-or-later + Valve-HL1-SDK-Ausnahme**. Die Engine ist bereits GPL-3; GPL-2+ führt keine neue Lizenz-Kategorie ein. Valve-SDK-Ausnahme und Attribution bleiben Pflicht. Bisherige Regel „Ref A kein Copy“ galt gegen stilles Mischen in Phase 0/1 — nicht als Lebenszeit-Verbot, nachdem der fehlende Körper feststeht.

**Gewählt: A1 — Ref A als Client-Body**  
**Datum: 2026-09-01**

NextClient bleibt die funktionale Zielbasis. Ref A ersetzt sie nicht.
Allowlist und Stapel: `docs/ROLLEN.md`. Lizenz: `docs/LIZENZEN.md` (A1 dokumentiert, nicht als vollständig geklärt markiert).

Kein Phase-3-Body-Code, bevor diese Doku steht (erledigt mit diesem Eintrag). Body-Vendor erst in Phase 3.

YaPB / ReGameDLL / mainui aus Ref A bleiben verboten.

## Phase 3 — Minimal lauffähig

Erst nach Phase 2. Body laut A1 vendorn, dann **eine** Client-Lib. Connect, Render, Input unter Xash. NextClient-Features auf den Unterbau, nicht cs16-client weiterentwickeln. Ohne Ref-B-Features.

## Phase 4 — Gezielte Ports

Nur Ref B, ein Feature pro Durchgang, Lizenz vorher, eigener Diff.
