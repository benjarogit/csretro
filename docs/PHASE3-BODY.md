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
| 3C Baseline | **unvollständig** — Load/Init ja; In-Game nein (keine eigene 64-Bit-GameDLL) |
| 3D NextClient-Features | **nicht begonnen** |

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

## 3C — was fehlt

Xash lädt die Lib (`CL_LoadProgs: found single callback export`). `Init: CS16Client ver. 3959`. RenderAPI/MobilityAPI ok. MainUI lädt. `+quit` sauber.

Map / Rendering / Input / Movement / Prediction / HUD / Waffen / Connect: **nicht verifiziert**. Steam-RODIR-`cs_amd64.so` scheitert (`executable stack`). Gate: `docs/SERVER.md`.

## 3D

Erst nach vollständiger 3C (Listen-Server, Map läuft). Ein Feature pro Durchgang.
