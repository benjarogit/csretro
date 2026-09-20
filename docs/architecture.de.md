# Architektur

CS Retro baut auf vorhandenen Projekten auf und integriert sie zu einer eigenen Laufzeit.
[Upstream-Pins](upstream.md) und [Credits](credits.md) halten Herkunft und Rollen auseinander.

```text
CS Retro (eine Codebasis)
├── Engine: Xash3D-FWGS-derived — einzige Runtime, erweiterbar
├── Client: eine client_amd64.so
│   ├── CS-Körper (Velaron/NextClient-derived)
│   ├── Movement/Prediction (Vertrag mit der GameDLL)
│   └── PrimeXT-derived Technik (`client/render/`; PX3B World/BSP, PX3C Entity/Sprite, PX4A Non-Player Studio, PX4B.1 FOLLOW, #7 Brush + Engine-EFX + Client-Triangles draw-only)
├── Menü-Lib: CS-Retro-InGameUi (VGUI2 gültig; ImGui fest für Tools)
└── GameDLL: ReGameDLL-derived — Spielregeln, Waffen, Inferno, ZBot
```

Versorgung: Engine → Renderer → HUD/UI. Game State → UI. Commands ← UI.
PrimeXT ist Technik-Upstream im Client, keine zweite Runtime und kein paralleles `GetClientAPI`.
Renderer besitzt keine Spielregeln. Client und GameDLL implementieren denselben Movement-Vertrag.

| Pfad | Aufgabe |
| --- | --- |
| `engine/` | Xash3D-FWGS-derived Runtime; CS-Retro-Erweiterungen erlaubt, keine CS-Menülogik |
| `client/export/` | CS-Retro-Client-Export für die Engine |
| `client/body/` | Client-Laufzeit, HUD, Prediction und Shared-Waffen |
| `client/render/` | CS-Retro-Renderer (Lifecycle, Offscreen-GL, World/BSP, Brush-Entities, Sprite, Non-Player Studio). Eine `client_amd64.so`. |
| `client/menu/` | GameUI, Team-, Klassen- und Kaufoberflächen |
| `client/nextclient/`, weitere Client-Quellen | Funktionale Port-Quellen aus NextClient |
| `server/game/` | ReGameDLL-basierte Spiel- und Serverlogik |
| `bots/` | Bereich für Bot-Weiterentwicklung; aktueller ZBot liegt in der GameDLL |
| `refs/` | Vergleichsquellen, kein zweiter Produktclient |
| `data/` | Importmanifest und projektbezogene UI-Ressourcen |
| `scripts/`, `cmake/` | Build, Datenimport und Tests |

## Schnittstellen

Die Engine lädt eine Client-Lib über `GetClientAPI`; der Adapter liegt in
`client/export/csretro_cdll_export.cpp`. Client und Engine müssen dasselbe Tabellenlayout
verwenden. Die Menü-Lib bietet `GetMenuAPI` und den GameMenu-Interface-Vertrag.
Die GameDLL erhält die Engine-Funktionen über `GiveFnptrsToDll` und liefert die Entity-API.

64-Bit betrifft auch Pointerbreiten und Interface-Strukturen. Ändere bei Grenzflächen nicht
nur eine Seite. Client-Prediction und Server-Waffenlogik müssen dieselben Zustandsübergänge verstehen.

## Daten statt Steam-Laufzeit

Originale Spieldaten werden separat importiert. Die CS-Retro-Laufzeit verwendet keine
Steam-Engine als Ersatz. `XASH3D_RODIR` zeigt auf den vorbereiteten Datenbaum,
`XASH3D_BASEDIR` auf beschreibbare Konfigurationen und Laufzeitdaten.

## Änderungen beitragen

Upstream-Änderungen werden gezielt verglichen und integriert, nicht automatisch komplett gemergt.
Ein Port sollte Quelle, Commit, betroffene Dateien, Lizenzhinweise und Tests nennen.
Keine zweite parallele Implementierung der **Produktlogik**. Der Xash-World-Renderer darf als Diagnose-/A/B-Fallback bleiben (`r_csretro_renderer 0`).
PrimeXT-Updates gehen in die inzwischen eigene CS-Retro-Implementierung, nicht in wiederhergestellte Upstream-Schichten.
Verbindlicher Integrationsplan: PX0 (Verträge/Baselines) vor produktiver Renderer-Integration (PX2).
PX1-Research und Port-Matrix: [docs/research/px1-primext.md](research/px1-primext.md). PX2 (`17bd79f`) ist die `HUD_GetRenderInterface`-Brücke. PX3A: Strategie C. PX3B (`bbe418d`, visuell zertifiziert): World/Offscreen. PX3C (visuell zertifiziert, #6 geschlossen): Entity-Spiegel + Sprite-Offscreen. PX4A (visuell zertifiziert, #8 geschlossen): Non-Player Studio offscreen über GSMR `STUDIO_RENDER`. PX4B.1: Non-Player FOLLOW-Pfad vorhanden, auf Stock-CS-Maps nicht reproduzierbar. #7 Brush visuell zertifiziert; Engine-EFX draw-only hinter `GL_RenderFrame` 0; Client-Triangles VERIFIED draw-only (Spectator Map-Overview Cert); Sprite Completion (ANGLED implemented / runtime NOT REPRODUCIBLE, lerp VERIFIED, Xash sprite lighting VERIFIED). Brush-Sonderflächen bleiben [#7](https://github.com/benjarogit/csretro/issues/7). Rest vor return 1: restliches #7, Player [#9](https://github.com/benjarogit/csretro/issues/9), Viewmodel [#10](https://github.com/benjarogit/csretro/issues/10). Sichtbarer Takeover erst nach vollem Entity/EFX/Studio-Besitz.
