# Native Molotov / Incendiary

Zwei **zusätzliche** Granaten. HE, Flash und Smoke bleiben. Zielbild für deren Modernisierung (nach 3M, kein Code jetzt): `docs/UTILITY.md`. Smoke-Zellen dürfen die Löschregel unten **nicht** ersetzen. Kein AMXX, keine geklauten Glock-/Smoke-IDs. Stock-`delta.lst` bleibt; nur `m_iId` von 5 auf **6 Bit** (IDs 32/33 passen sonst nicht, First-Person bleibt leer).

Katalog-Ausnahme: nur diese zwei Fire-Nades. Restliches „keine CS2-Waffen erfinden“ gilt weiter. Siehe `docs/MENUS.md`.

## IDs

Nachzug 2026-09-11: Maximaler Schaden auf Inhaberwunsch **32 DPS** (vorher 40), tatsächlicher Tick-Cap aus `damagePerSecond * damageInterval`. Historische 40-DPS-Angaben unten beschreiben den vorherigen Stand. Neue Sprites setzen `fuser4=GetClientTime()` für `KillEveryRound`; ohne diesen Zeitstempel verschwanden sie nach dem ersten Round-Reset sofort. `invnext`/`invprev` beachten nun ebenfalls `hud_fastswitch`. First-Person: **Zippo-Molotov** (`grenades_09_2`) und Fire-Pack-Inc. languagelawyer-HE-Hände wurden 09:05 vom Inhaber zurückgewiesen; Skripte bleiben im Repo, Import baut sie nicht.

`WEAPON_SUIT` belegt Bit 31 in `pev->weapons`. Deshalb:

| | ID |
|--|--|
| Molotov | `WEAPON_MOLOTOV = 32` |
| Incendiary | `WEAPON_INCGRENADE = 33` |

`ITEM_MOLOTOV` / `ITEM_INCGRENADE` hängen hinten an `items.h`. Bits ab 32 liegen in `CCSPlayer::m_iWeaponBits2`. `MAX_WEAPONS = 64`.

Auswahl wie HE/Flash/Smoke: Slot 3 / Taste **4** (Gruppe). `WeapPickup` legt sie in denselben Slot; IDs 32/33 zusätzlich über `WpnBits2`. Optionen: `weapon_molotov` / `weapon_incgrenade` in `kb_act_overlay.lst` (neben HE/Flash/Smoke). Bots kaufen team-gated und werfen wie andere Granaten.

Kauf nur über die Aliase `molotov` / `incgrenade` (wie `hegren`/`sgren`). Nicht über `BuyWeaponByWeaponID` — das würde Primär/Sekundär droppen.

## Werte (CS2-Stand, GoldSrc-Nodes)

| | T Molotov | CT Incendiary |
|--|--|--|
| Preis | $400 | $500 |
| Lebensdauer | 7.0 s | 5.5 s |
| max. Reichweite | 150 u | 110 u |
| Spread-Mult | 1 | 10 |

Gemeinsam: **32 DPS**, Tick 0.2 s, Rampe 1…8 nach Inferno-Alter, max. 16 Nodes, Abstand 42, Tiefe 4, Winkel 45°, Slope ≤ 30° (nach Stillstand auch 45°), maximal 2 s Flugzeit mit Bodentransfer statt Feuer in der Luft, Transfer < 128 u plus 8u-Offsets, Boden < 48 u nach Wandhit, Stillstand 0,5 s unter 30 u/s, Impact ~2 / ~1 mit Armor, **kein Slow/kein Flinch** (`DMG_BURN` setzt `m_flVelocityModifier` nicht herunter), Werfer nicht immun, FF folgt `mp_friendlyfire`, Infernos stapeln, eine Edict, Smoke löscht bei >1/3 Node-Coverage (Radius 115). Feuer durch Wände trifft nicht (LOS vom Node). Feuer blockiert die Sicht nicht.

## Wurf (alle fünf Granaten)

Nachzug 2026-09-11 11:03: Inc-Wurf spielt die Throw-Sequenz, wartet danach aber wie HE/Smoke (0,75 s, letzte Nade 0,5 s) statt der vollen 1,2 s Smoke-Recovery. Sonst kommt der linke Arm noch einmal als DRAW. Molotov war schon 0,75/0,5. Pin-Zeiten unverändert. Inferno-Zahlen unverändert.

Nachzug 2026-09-11 08:43/08:44: erster `+attack` **PASS** (Inhaber) auf Smoke, Molotov, Inc, HE. HUD rüstet HE/Flash/Smoke/Molotov/Inc bei Slot 4 und Mausrad immer aus (`hud_fastswitch` gilt weiter nur für Waffen). Restliches `gpActiveSel` auf einer Granate frisst M1 nicht. Overlay-Mausgrab (08:23) bleibt zusätzlich. Nicht erneut NextAttack.

Nachzug 2026-09-10 nach 18:58: Der neue Fix liegt in der HUD-Waffenauswahl, nicht erneut in NextAttack. Bei `hud_fastswitch=1` funktioniert direkter Wechsel auch mit mehreren Waffen im Slot. Isolierter CT-Test: HE+Inc kaufen, zweimal Slot 4, ein Angriff/Loslassen zeigt Pin, Wurf und Feuer. Inc-Unsichtbarkeit wurde nicht reproduziert; weder Event-/Sprite-Ursache noch allgemeine Behebung behauptet. `CSRETRO_INFERNO_EVENT` und `CSRETRO_INFERNO_RENDER` liefern zusätzliche Developer-Diagnose. Inferno-Werte unverändert; Inhaber-Recheck bleibt offen.

CS2-Input, GoldSrc-Physik. HE, Flash, Smoke, Molotov und Incendiary teilen denselben Layer. Drei diskrete Stärken: M1 = voll (1.0), M2 = Underhand (0.0), beide = mittel (0.5). **Playtest 2026-09-10 18:58:** M2/Underhand **OK** auf allen fünf. Erster `+attack` damals FAIL, 08:37 noch FAIL, **08:43/08:44 Inhaber-PASS**. Freeze-Pin und Hold-Pose bleiben Soll. Inferno-Zahlen oben nicht anfassen.

## Assets

Modelle/Sprites/Sounds liegen nur lokal in `gamedata/` (nicht im Git).

```bash
python3 ./scripts/bootstrap-gamedata.py
./scripts/import-molotov-assets.sh
```

Standardquelle: `/home/benny/Downloads/Molotov Incendiary Grenade.rar` (Sprites, Sounds, `p_`/`w_incgrenade`). Zippo/`grenades_09_2` werden als First-Person-Molotov übernommen. **Kein** HE-Hände-Rebuild beim Import (Inhaber 09:05). `CSRETRO_HE_HANDS=1` wäre der optionale languagelawyer-Pfad. Keine AMXX-Dateien. Lokale `delta.lst`: `m_iId` 6 Bit. Nach Game-Data-Refresh Import erneut.

Der Start des gehaltenen Docht-Effekts folgt der Angriffstaste nach 0,46 s
(Quellframe 23 bei 50 fps). Nach dem Pin bleibt die letzte `pullpin`-Pose
(beleuchteter Lappen, Zippo offen). Die kleine Lunte sitzt auf
dem `ragslave2`-Knochen (`HUD_DrawTransparentTriangles`). Incendiary erhält bewusst keine Lunte.

Repo-seitig: `data/ui-overrides/cstrike/sprites/weapon_*.txt`, `events/createinferno.sc`, Buy-`.res`.

## Animationen

Abgeguckt aus `TEMP_EXTRA/cstrike15_src` (`CBaseCSGrenade` + `CMolotovGrenade`) für den CS2-Input-Layer, nativ auf GoldSrc. Viewmodels: Zippo-Molotov und Fire-Pack-Inc (Inhaber 09:05: nicht durch languagelawyer-HE-Hände ersetzen). Draw zeigt die Granate; `ACT_VM_PULLPIN` muss durchlaufen. Dritte Person bleibt `"grenade"`. Prediction und `weapondata[64]` erreichen IDs 32/33.

| | Molotov | Incendiary |
|--|--|--|
| Herkunft | 2008-Flaschen-/Zippo-Rig (`grenades_09_2`) | Fire-Pack GoldSrc-Rig |
| Draw | 2008 Deploy | 24 Frames / 0,8 s |
| Idle | 2008 Idle | 32 Frames |
| Pin | 50 Frames / 1,0 s + Zippo + `molotov_light.wav` | 24 Frames / 0,8 s + `pinpull.wav` |
| Halten | letzte Pin-Pose; Docht-Loop ab Frame 23 | letzte Pin-Pose, ohne Feuerloop |
| Wurf | 2008 Throw; Idle-Wait 0,75 s / letzte 0,5 s wie HE | Clip 36 Frames / 1,2 s; Idle-Wait wie HE/Smoke (0,75 / 0,5), sonst DRAW nach dem Wurf (zweiter linker Arm) |
| Bruch | `w_broke_molotov` + `TE_BREAKMODEL` + `molotov_gibs.wav` | dasselbe |
| 3rd | 2008 `p_molotov` | Fire-Pack `p_incgrenade` |
| Flug | Fire-Pack `w_molotov` | Fire-Pack `w_incgrenade` |

`material_348` bleibt draußen. `scripts/build_molotov_models.py` liegt im Repo, wird nicht auf die Runtime-MDLs angewandt.

## Native Sichtpruefung

Die Pruefung laeuft in einem isolierten BASEDIR und veraendert keine
Benutzerkonfiguration:

```bash
./scripts/fire-grenade-gate.sh
CSRETRO_FIRE_TEAM=ct ./scripts/fire-grenade-gate.sh
```

Aufnahmen von Deploy, Pinziehen, Halten, Wurf und Ergebnis landen unter
`build/fire-shots/`. Beim Aufprall wird nur einmal eine kurze Zuendflamme
gezeichnet; die ausgebreiteten Nodes erzeugen ausschliesslich niedriges
Bodenfeuer, damit keine geschlossene Feuerwand entsteht.
