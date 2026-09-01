# Phase 3 — A1-Body (Arbeitsdokument)

Nach Abschluss von Phase 3 wird diese Datei konsolidiert und entfernt. Lebende Wahrheit: `docs/ROLLEN.md`, `docs/HANDOFF.md`, `docs/PLATTFORMEN.md`, `docs/SERVER.md`.

**A1-Body-Manifest:** identisch mit `docs/ROLLEN.md` (Abschnitt „A1-Body-Manifest“). Keine zweite Allowlist.

Herkunft: `refs/a-cs16-client/` Pin `bb60674`. Referenz bleibt, wird nicht gebaut.

## Manifest (Kopie von ROLLEN.md)

| Pfad in `client/body/` | Rolle |
|------------------------|--------|
| `cl_dll/` | Client-Körper (ohne entfernte Leichen) |
| `pm_shared/` | Prediction / Movement |
| `common/` | Shared GoldSrc-Header + `interface.cpp` |
| `public/` | `cl_dll/IGame*.h`, `strl*`, `utflib`, `build.h` — **ohne** `steam/` |
| `public/mainui/font/FontRenderer.h` | ein Typ-Header für `IGameMenuExports` |
| `game_shared/` | Voice-HUD-Hilfen. `voice_gamemgr.cpp` entfernt |
| `dlls/*.h` | 23 Waffen-/Entity-Header |
| `dlls/wpn_shared/*.cpp` | Shared-Waffen |
| `miniutl/` | UTL (kein Menu) |
| `engine/*.h` | 16 Body-Engine-Header, kein `.cpp` |

Plus CS-Retro `client/export/` (`GetClientAPI`).

## Stand

| Teil | Status |
|------|--------|
| 3A Vendor | erledigt |
| 3B eine Client-Lib | erledigt — `GetClientAPI`, lädt unter Xash |
| 3C Baseline | **abgenommen** — interaktiver Listen-Lauf `de_dust` |
| 3D NextClient-Features | **nicht automatisch beginnen** |

## 3B — eine Lib

- Target `csretro_client` → `client_amd64.so` (Linux x86_64)
- Pflicht-Exports: `GetClientAPI`, `Initialize`, `HUD_*`, `IN_*`, `CL_*`
- `F()` entfernt (Ref-A-Layout ≠ Xash-`cldll_func_t`)
- `ldd`: libm, libstdc++, libgcc_s, libc
- kein steam_api / NitroApi / 8684 zur Laufzeit

Build: `./scripts/build-client.sh`

## Body-Anpassungen (CS Retro)

| Datei | Änderung | Warum |
|-------|----------|--------|
| `engine/cdll_int.h` | `steam/steamtypes.h` → `archtypes.h` | kein Steam im aktiven Build |
| `cl_dll/cdll_int.cpp` | Versionscheck nur wenn `g_iXash > 0` | Waf-`Q_buildnum()` ist `-1` |
| `cl_dll/cdll_int.cpp` | `F()` entfernt | Xash nutzt `GetClientAPI`; altes Layout falsch |
| `cl_dll/cdll_int.cpp` | kein Modal bei fehlendem `GameMenuExports001` | 3C: MainUI ohne Factory. 3M: `client/menu/` exportiert `GameMenuExports001` |
| `cl_dll/menu.cpp` | 3C: `ShowMenu`. 3M: VGUI wenn `g_pMenu` | Touch-`exec touch/*.cfg` entfernt. `ShowMenu` bleibt Legacy |
| `cl_dll/hud.cpp` | `_vgui_menus` 1 wenn `g_pMenu` | 3C-Baseline war 0; 3M schaltet den GameDLL-VGUI-Pfad wieder ein |

## 3C — Stand

GameDLL `cs_amd64.so` unter Xash. Smoke: `./scripts/smoke-gamedll.sh dedicated|listen`.
Interaktiv: `./scripts/interactive-3c.sh` (`de_dust`, 640×480, xdotool, Fenstertitel `CS Retro`).
Menüs ohne Auto-Join: `./scripts/interactive-menus.sh`.

Nachweis im `engine.log`: Auto-Join CT, ZBot T, `Game_Commencing` / `Round_Start`, Movement/Duck/Jump, Slot 1–3, Attack, Reload, `give weapon_ak47`, Host-`quit`.
Listen-Admin soweit vorhanden = Host-Konsole/Binds (`give`, `bot_add_t`, `sv_cheats`), kein AMXX-Admin.
Prediction: `cl_showerror 1`, kein `prediction error:` im Log (Notify-Overlay nicht mitgeschnitten).
HUD: `CS16Client` init; Warnung `sprites/hud.txt` 215 vs 190 (Daten, kein Crash).

ZBot-Testdaten nur in `build/run/` (nicht Git): Profile aus ReGameDLL `bot_profiles.zip`, `de_dust.nav` lokal. Kein zweites Bot-System.

`XASH3D_RODIR` = `gamedata/` nach `python3 ./scripts/bootstrap-gamedata.py`. Steam-HL nicht als RODIR.
3C interaktiv 2026-09-01 ohne Steam-RODIR: Team/Spawn/Movement/Waffen/Round/Shutdown bestanden. strace: 0 Opens unter `steamapps/common/Half-Life`.
Immer `-dll` / `-clientlib` auf unsere Libs. Listen-`+map`: `.rc` mit `stuffcmds`.

## 3D

3C-Gate erfüllt. **Nicht vor Abschluss von 3M** (`docs/PHASE3M.md`). Ein Feature pro Durchgang, erstes wäre FOV.
