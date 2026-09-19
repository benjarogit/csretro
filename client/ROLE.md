# Rolle: CS-Retro-Client (Produkt)

- **Eine Lib:** `export/GetClientAPI` → eine 64-Bit-`client_amd64.so`. Kein paralleles PrimeXT-`GetClientAPI`, keine zweite Client-DLL.
- **CS-Körper:** Velaron/cs16-client-derived `body/`, NextClient-Funktionen als Port-Quelle bis sie in CS Retro liegen.
- **Technik-Upstream:** PrimeXT für Renderer, Materialien, Licht, PostFX und ImGui-Tools. Produktcode ist CS Retro und darf umgebaut werden. Erste produktive Integration erst PX2, nach Movement-Gate.
- **Movement:** Client implementiert denselben deterministischen Vertrag wie die GameDLL. Renderer und UI besitzen Movement nicht.
- **UI:** VGUI2 bleibt gültig. ImGui fest für Tools; Spieler-UI nur nach Evaluation je Panel.
- **Nicht:** YaPB, ReGameDLL, PrimeXT-`client.so` als Runtime. Kein 32-Bit. Kein Steam-Laufzeitbind.
