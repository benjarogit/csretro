# CS-Retro-GameDLL — Herkunft

Vendor aus https://github.com/rehlds/ReGameDLL_CS
Commit `b0889847fe6d03898be88acc9e366660efb40ab5` (2026-08-28).

Lizenz: `LICENSE` (MIT) plus Valve-HL1-SDK-Anteile in den Quellen.

Dieser Baum ist der CS-Retro-Server-Unterbau, nicht ReGameDLL-Weiterentwicklung und nicht der Ref-A-Pin.
`refs/a-cs16-client/3rdparty/ReGameDLL_CS/` bleibt unberührt.

ZBot liegt in `regamedll/dlls/bot/` und `regamedll/game_shared/bot/` und wird **mitgebaut** (Migration).
Später: Analyse, dann schrittweise nach `bots/`. Keine Amputation vor dem ersten lauffähigen Server.

Produkt-Build: `cmake/CsretroGameDll.cmake` → Target `csretro_gamedll`.
Upstream-CMake/SLN/CI sind nicht übernommen.
