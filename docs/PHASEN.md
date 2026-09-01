# Phasen

Vor jeder Phase die Rollen in `docs/ROLLEN.md` bestätigen. Nicht mischen.

## Phase 0 — Rollen + Vendor (abgeschlossen)

Vier Bereiche + eingefrorene Refs. CMake orchestriert; Engine über Waf. NextClient-MSVC/vcpkg ist nicht der Produkt-Build.

## Phase 1 — Bindungs-Analyse (abgeschlossen)

NextClient ist Overlay auf Steam-`client.dll`, kein `GetClientAPI`-Körper. Xash verlangt `GetClientAPI` + `Initialize(&gEngfuncs)`. Form: eine Client-Lib = Export + Körper + NextClient-Module. Körper-Quelle **A1**. Menü = zweiter Ladeweg (`GetMenuAPI`).

## Phase 2 — Steam-/Hook-Schnitt (abgeschlossen)

Entfernt: `steam_api_proxy/`, 8684-Address-Provider, Steam-Master `hl1master`, `MatchmakingSteamComp`, Launcher-`steam_api.dll`. Feature-Quellen (`client_mini`, `engine_mini` NCLM/HTTP-Master, GameUI) bleiben als Port-Quelle. Exportvertrag unter `client/export/`.

## Gate A1 (2026-09-01)

Ref-A-Allowlist als Client-Body. NextClient bleibt Zielbasis. Manifest: `docs/ROLLEN.md`.

## Phase 3 — Minimal lauffähig (offen)

Arbeitsdokument: `docs/PHASE3-BODY.md`. Plattform: `docs/PLATTFORMEN.md`. Server: `docs/SERVER.md`.

| Teil | Status |
|------|--------|
| 3A A1-Body vendorn | abgenommen |
| 3B nackte Client-Lib unter Xash | abgenommen |
| 3C In-Game-Baseline | **abgenommen** — Listen interaktiv `de_dust` (Team/Spawn/Movement/Waffen/Round/Shutdown) |
| GameDLL-Gate | vendort `server/game/` Pin `b088984`, Target `csretro_gamedll` |
| **3M VGUI2 / Desktop-UI** | **in Arbeit** — V1 bestanden; `COptionsSubMouse` abgenommen; Audio noch nicht. `docs/PHASE3M.md` |
| 3D NextClient-Features | **nach 3M** — erstes Feature FOV, nur nach Freigabe |

3C vollständig heißt: Listen-Server, Map, Rendering, Input, Movement, Prediction, Vanilla-HUD, Waffen, Connect, Shutdown — alles auf derselben 64-Bit-Architektur. 3C wird für 3M **nicht** neu aufgerollt. `ShowMenu` bleibt Legacy-Kompatibilität.

## Phase 4 — Gezielte Ports

Ref B ist **bereits in Phase 3M** Menü-/VGUI-Referenz (In-Game-Verhalten / `.res`-Mapping). Phase 4 ist nur für gezielte zusätzliche Feature-Ports, die nach 3M übrig bleiben — nicht der erste Ref-B-Einstieg. NextClient-GameUI-Ports laufen in **3M**, nicht als zweiter Stapel.
