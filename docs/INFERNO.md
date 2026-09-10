# Native Molotov / Incendiary

Zwei **zusätzliche** Granaten. HE, Flash und Smoke bleiben. Kein AMXX, keine geklauten Glock-/Smoke-IDs. Stock-`delta.lst` bleibt; nur `m_iId` von 5 auf **6 Bit** (IDs 32/33 passen sonst nicht, First-Person bleibt leer).

Katalog-Ausnahme: nur diese zwei Fire-Nades. Restliches „keine CS2-Waffen erfinden“ gilt weiter. Siehe `docs/MENUS.md`.

## IDs

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

Gemeinsam: 40 DPS, Tick 0.2 s, Rampe 1…8 nach Inferno-Alter, max. 16 Nodes, Abstand 42, Tiefe 4, Winkel 45°, Slope ≤ 30° (nach Stillstand auch 45°), maximal 2 s Flugzeit mit Bodentransfer statt Feuer in der Luft, Transfer < 128 u plus 8u-Offsets, Boden < 48 u nach Wandhit, Stillstand 0,5 s unter 30 u/s, Impact ~2 / ~1 mit Armor, kein Slow, Werfer nicht immun, FF folgt `mp_friendlyfire`, Infernos stapeln, eine Edict, Smoke löscht bei >1/3 Node-Coverage (Radius 115). Feuer durch Wände trifft nicht (LOS vom Node). Feuer blockiert die Sicht nicht.

## Wurf (alle fünf Granaten)

CS2-Input, GoldSrc-Physik. HE, Flash, Smoke, Molotov und Incendiary teilen denselben Layer. Drei diskrete Stärken, kein analoges Lerp: M1 = voll (1.0), M2 = Underhand (0.0), beide Tasten = mittel (0.5). Taste drücken = Pin/Zippo, halten = bereit, Loslassen = Wurf. Freeze: Pin ist erlaubt, der Wurf wartet auf `m_bCanShoot` und das Ende der Freeze-Zeit. Waffenwechsel während des Haltens bricht ab (Nadel wieder rein, Granate bleibt); nach dem Loslassen ist der Wurf committed und `CanHolster` blockiert. Startvelocity = `forward * speed + playerVelocity`, Origin = `origin + view_ofs`, 22/6-Trace, Z-Drop bei schwachem Wurf. Molotov-Docht-Loop (ab 0,46 s) und Incendiary-Held-Idle bleiben feuer-spezifisch. Inferno-Zahlen oben bleiben der CS2-Stand.

## Assets

Modelle/Sprites/Sounds liegen nur lokal in `gamedata/` (nicht im Git).

```bash
python3 ./scripts/bootstrap-gamedata.py
./scripts/import-molotov-assets.sh
```

Standardquelle: `/home/benny/Downloads/Molotov Incendiary Grenade.rar`. Anderer Pfad als erstes Argument. Das vollständige GoldSrc-`v_incendiarygrenade.mdl` aus diesem Paket wird direkt als `v_incgrenade.mdl` übernommen; Hand, Dose, Bügel, Pin und die vier Granatensequenzen bleiben dadurch aus einem gemeinsamen Rig. Der Import deaktiviert nur drei eingebettete Soundevents für nicht mitgelieferte WAV-Dateien und lässt `pinpull.wav` aktiv. Der frühere Neubau aus Smoke-Händen plus automatisch eingepasster `p_inc`-Dose ist entfernt, weil er Hand und Dose sichtbar ineinanderschob. Liegt `/home/benny/Downloads/molotov_cocktail-3.30_cstrike.zip` da, nimmt der Import außerdem das intakte animierte 2008-`v_molotov`, `p_molotov` und `w_broke_molotov`. Keine AMXX-Dateien. Lokale `delta.lst`: `m_iId` 6 Bit. Nach Game-Data-Refresh Import erneut.

Der Start des gehaltenen Docht-Effekts folgt der Angriffstaste nach 0,46 s
(Quellframe 23 bei 50 fps). Der Studio-Renderer berechnet Viewmodel-Attachments
erst nach `HUD_CreateEntities`; dort gelesene Attachments gehoeren daher zum
falschen Transform/Frame. Die kleine Lunte wird stattdessen stabil im
View-Space am Flaschenhals verankert. Incendiary erhaelt bewusst keine Lunte.

Repo-seitig: `data/ui-overrides/cstrike/sprites/weapon_*.txt`, `events/createinferno.sc`, Buy-`.res`.

## Animationen

Abgeguckt aus `TEMP_EXTRA/cstrike15_src` (`CBaseCSGrenade` + `CMolotovGrenade`), nativ auf GoldSrc übertragen: Draw zeigt die Flasche; `ACT_VM_PULLPIN` muss durchlaufen; das 2008-Modell liefert Zippo-Events und `molotov_light.wav`, der gehaltene Molotov spielt den Idle-Loop, der beim Wurf endet. GoldSrc-Mapping: dieselben vier Sequenzen wie HE (`idle` / `pullpin` / `throw` / `draw`), Pin-Wartezeit = Sequenzlänge. Dritte Person bleibt `"grenade"`. Prediction und `weapondata[64]` erreichen IDs 32/33.

| | Molotov | Incendiary |
|--|--|--|
| Herkunft | intaktes 2008-Flaschen-/Hand-/Zippo-Rig; kein Cross-Skeleton-Retarget | vollständiges Fire-Pack-GoldSrc-Rig; kein Smoke-Hand-/Dose-Splice |
| Draw | 2008 Deploy + Zippo-Idle | 24 Frames / 0,8 s |
| Idle | 2008 Idle | 32 Frames |
| Pin | 50 Frames / 1,0 s + Zippo 5001/5011 + `molotov_light.wav` | 24 Frames / 0,8 s + `pinpull.wav` |
| Halten | Angriffstaste gehalten = kein Wurf; nach der Pin-Zeit Idle-Pose; Docht-Loop ab Frame 23 | nach der Pin-Zeit Idle-Pose, ohne Feuerloop |
| Wurf | Loslassen nach Pin; 2008 Throw | Loslassen nach Pin; vollständige 36 Frames / 1,2 s |
| Bruch | `w_broke_molotov` + `TE_BREAKMODEL` + `molotov_gibs.wav` | dasselbe |
| 3rd | 2008 `p_molotov` | Fire-Pack `p_incgrenade` |
| Flug | Fire-Pack `w_molotov` Toss 3–6 | Fire-Pack `w_incendiarygrenade` |

`material_348` bleibt draußen.

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
