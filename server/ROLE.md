# Rolle: Server

- **Heute:** `NextClientServerApi` — AMXX-Herkunft für NCLM/Protokoll, nicht die CS-GameDLL.
- **Ziel:** eine CS-Retro-GameDLL unter `server/game/` für Listen- und Dedicated-Server. Siehe [docs/architecture.de.md](../docs/architecture.de.md).
- **Nicht:** Ref-A-ReGameDLL still kopieren. Keine Bots in diesen Baum. AMXX/Metamod nicht in die GameDLL backen.
- **64-Bit:** dieselbe Architektur wie Engine und Client.
- **Movement:** dieselbe deterministische `pm_shared`-Baseline wie der Client. Renderer/UI besitzen den Vertrag nicht.
