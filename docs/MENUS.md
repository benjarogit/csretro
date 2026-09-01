# Menüs

Lebende Architektur. Rollen: `docs/ROLLEN.md`. Schnittstelle: `docs/SCHNITTSTELLEN.md`.

CS Retro ist **64-Bit-Desktop** (Linux x86_64, Windows 10+ x86_64, macOS ARM64/x86_64). Kein Android, iOS, Switch, Vita, keine Touch-/Mobile-UI.

## Entscheidung

**Eine** langfristige Menü-Library ist das Ziel:

```
Xash → GetMenuAPI → CS-Retro-Hauptmenü
Client → Xash MenuFactory → CreateInterface → GameMenuExports001 → In-Game-Menüs
```

Xash stellt `MenuFactory` als plattformübergreifendes Native Object bereit (CreateInterface-Pointer der geladenen Menü-Lib). Das ist kein Android-API.

Die aktuell geladene Phase-3-Lib (`libmenu.so` / Xash-MainUI) exportiert `GetMenuAPI`, aber **kein** `CreateInterface` / `GameMenuExports001`. Deshalb ist `IGameMenuExports` im A1-Body **optional**: kein modaler `pfnSys_Warn`. Das ist ein Kompatibilitätsfallback, keine Aussage „CS Retro nutzt GameMenuExports nie“.

Ref-A-mainui wird nicht übernommen. Ref B bleibt Menü-Referenz für das spätere klassische VGUI-Erscheinungsbild. NextClient bleibt funktionale Zielbasis.

## Jetzt (Phase-3-Desktop)

| Fläche | Implementierung |
|--------|-----------------|
| Hauptmenü (New Game, Browser, Options, Quit, Escape außerhalb einer Map) | Xash-MainUI über `GetMenuAPI` |
| Team / Klasse / Buy / Radio | GoldSrc-`ShowMenu` im Client (`CHudMenu`), Texte aus `cstrike/titles.txt` |
| Escape während eines In-Game-Menüs | schließt das HUD-Menü (`menuselect 0` wenn Slot 0 gilt), öffnet nicht sofort MainUI |

Die GameDLL sendet `ShowMenu` mit `#Team_Select`, `#Buy`, `#RadioA`, … sobald Userinfo `_vgui_menus` 0 ist. Das ist der originale CS-1.6-Pfad für Clients ohne VGUI. `VGUIMenu` wird, falls es trotzdem ankommt, auf dieselben `titles.txt`-Schlüssel abgebildet.

Kein `exec touch/*.cfg`. Keine Touch-Buttons.

## Quellen

1. Original-CS / ReGameDLL: `ShowVGUIMenu` → `ShowMenu`, wenn `!m_bVGUIMenus` (`server/game/regamedll/dlls/client.cpp`). Ghidra nicht nötig, der Vendor ist eindeutig.
2. `titles.txt` aus dem Game-Data-Baum: klassische Nummerntafeln inkl. `\y`/`\w`/`\R`.
3. Ref B: Escape-Reihenfolge und ShowMenu-Zeichnung (Escape-Tokens) — übernommen als Desktop-Verhalten, nicht deren VGUI2-Viewport.
4. NextClient-GameUI: späteres Zielbild, nicht Phase-3-Lader.

## Nicht

- Touch-CFG als Desktop-Fallback
- Ref-A-mainui pauschal
- parallele Menüsysteme für dieselbe Funktion
