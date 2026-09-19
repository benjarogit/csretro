# Rolle: Ziel-Engine

- **Quelle:** Xash3D-FWGS (https://github.com/FWGS/xash3d-fwgs)
- **Funktion:** einzige Laufzeitumgebung von CS Retro
- **Nicht:** Steam-GoldSrc, nicht zweite Engine, nicht PrimeXT als Runtime
- **Anpassungen:** CS Retro darf die Engine erweitern, wenn Client-, Renderer-, UI- oder Plattformintegration das sinnvoll erfordern. Engine kennt keine CS-Spiel- oder Menülogik (kein Buy, kein Inferno-Gameplay).
- **Architektur:** immer 64-Bit (Waf `-8`). Eine orchestrierte Produkt-Build-Pipeline; intern bleibt Waf für die Engine zulässig.
- **Versorgung:** Engine → Client Renderer → Client HUD/UI
