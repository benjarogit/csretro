# Utility — Zielbild (kein Code in 3M)

Methode: Quellen vergleichen (1.6, CS:GO-Dump, CS2-Produktgefühl, unser Inferno),
**das Beste nehmen**, nativ in GoldSrc umsetzen. Kein Source-2-Volumen, kein Merge
fremder Engines. Wurf-Layer und Inferno sind gebaut und gesperrt. HE/Flash/Smoke:
Ziel festgehalten 2026-09-11, Umsetzung **nach** 3M (Buy/Inc/Konsole-Recheck + Feinschliff).

Keine neuen Waffen. Kein Decoy. Keine `CModernGrenadeWeapon`-Hierarchie
(Throw sitzt auf `CBasePlayerWeapon`). languagelawyer-Feuer-Sim nicht übernehmen.

Zahlen und Nodes: `docs/INFERNO.md`. Phasen: `docs/PHASEN.md`.

## Schon da — nicht anfassen

| | Stand |
|--|--|
| Wurf | M1 voll / M2 Underhand / beide 50 %. Halten = Pin, Loslassen = Wurf. Velocity erbt Jump-/Runthrow. Ducken senkt den Ursprung. |
| Inferno | Zellen/Nodes, Boden/Slope, T/CT asymmetrisch. **32 DPS**, kein Fire-Slow. |
| Smoke löscht Feuer | **>1/3** Inferno-Nodes in **115 u** um `m_vSmokeDetonate`. Bleibt so, auch wenn Smoke später Zellen hat. |
| Smoke-Zündung | Fuse, dann **Boden** (`FL_ONGROUND`) — nicht in der Luft. Nicht auf CS2-Landing-only umbauen. |

## Nach 3M — Reihenfolge

1. **HE** — Deckung + Tagging (kein Zellensystem).
2. **Flash** — stufenloser Winkel + Audio-Dämpfung.
3. **Smoke-Zellen** — Sicht/HE-Loch/Kugelkanal; Grafik bleibt Sprites.

### HE

- Trace vom Explosionszentrum: freie Linie = Schaden, Wand **blockiert oder stark reduziert**.
- Schadenswerte **1.6**: `pev->dmg` 100, linearer Falloff, Armor wie jetzt. Kein CS2-Cap (~98).
- Tagging **proportional** zum HE-Schaden, kurz (5 dmg kaum, 70 dmg spürbarer Peek-Hit). Kein pauschales Slow. Inferno bleibt ohne Slow.

### Flash

- `RadiusFlash` behalten. Distanz + Trace + **Dot Product** stufenlos (vorne voll, Seite mittel, Rücken schwach) — nicht nur vorne/hinten.
- Starke Flash: Weiß + Fade + gedämpfte Umgebung/Tinnitus per Client-Message. Schwache Flash: wenig Audio. Keine volle Taubheit über die Hold-Zeit. Kein Mixer-Umbau.

### Smoke (Zellen, nicht Fluid)

Server: Zellen (Ursprung, Radius, Dichte, Sperre bis). Client: Sprites, in der Zelle extra abdunkeln.

- Geometrie **wie Inferno**: Trace, Wand dicht, Tür/Gang durch. Kein Raum-Flood-Fill, keine 1.6-Kugel durch Wände.
- Sicht nur Client. Spieler-Entities bleiben für Hitreg / Spectator / Bots.
- HE: Zellen im Radius **kurz aus**, Smoke füllt nach. Entity nicht löschen, Loch nicht bis Ablauf dauerhaft.
- Kugeln: lokale Dichte kurz senken (kleiner Kanal). Kein Spray-wie-HE.
- Mehrere Smokes: überlappende Felder, keine Merge-Simulation in v1.
- **Nicht:** Source-2-volumetrische Fluidsimulation.

## Quellen vergleichen — Bestes nehmen

`TEMP_EXTRA/cstrike15_src` (CS:GO, Depot 730 vor Hydra 2017, **nicht** CS2) ist eine
Ideenquelle wie 1.6 und unser Inferno. Lesen, vergleichen, das Beste nativ bauen.
Nichts vendorn, keine zweite Engine (`docs/UPSTREAM.md`).

Bisherige Wahl (gesperrt, bis Inhaber anders entscheidet): Wurf/Nodes aus CS:GO nativ;
DPS **32** (besser als CS:GO 40 für uns); HE-Zahlen **1.6**; Smoke-Zündung **Boden**;
Löschen **>1/3 in 115 u**; Grafik Sprites, nicht Partikel/Scaleform/Volumen.

| Thema | Kandidaten | Bisher beste Wahl | Warum nicht 1:1 der Rest |
|-------|------------|-------------------|--------------------------|
| Wurf | CS:GO `weapon_basecsgrenade.cpp` | Pin / Loslassen / M2 — gebaut | Klassenbaum der Source-Waffe |
| Inferno | CS:GO `Effects/inferno.cpp`, CS2-Feeling | Nodes/Spread — gebaut, 32 DPS | Source-Partikel, 40 DPS |
| HE-Deckung | CS:GO `GetExplosionDamageAdjustment` / `GetAmountOfEntityVisible`, 1.6-Radius | CS:GO-Traces + **1.6-Schaden** | VPhysics-Dichte, Partikel, CS2-Cap ~98 |
| Flash | CS:GO `PercentageOfFlashForPlayer` + `RadiusFlash` Dot, 1.6 vorne/hinten | CS:GO-LOS/Umwege + stufenloser Winkel | volle `Deafen`, Scaleform, Anim-Tags |
| Smoke | CS:GO Partikel + Bounce-Löschen, CS2 Volumen, unser Inferno-Zellen | Inferno-Geometrie, Fuse+Boden, Löschregel wie jetzt | Partikelwolke, Fluid, Velocity-Detonate |

Client-HUD-Flash (`hud_flashbang.cpp`): Effekt-Idee; Audio bei uns per Message.

## Nicht in diesem Ziel

Economy-MR15, restliche CS2-Waffen, 3D/FOV, Inferno-Zahlen, Viewmodels der Fire-Nades.
