# Rollen (bindend)

CS Retro wechselt **nicht** von NextClient auf cs16-client.
NextClient bleibt die funktionale Zielbasis. Ref A liefert nur den fehlenden Xash-Unterbau.

| Rolle | Was | Pfad | Darf |
|--------|-----|------|------|
| **CS Retro** | Produkt-Codebase / Worktree | Repo-Wurzel | hier wird entwickelt |
| **Xash3D-FWGS** | einzige Engine | `engine/` | Bindung, Build, Interface |
| **NextClient** | funktionales Zielverhalten des Clients | `client/nextclient/` (+ NitroApi/SDK als Port-Quelle) | Features herauslösen, auf Xash umbauen |
| **Ref A** (Velaron/cs16-client) | Xash-kompatible **Client-Body-Quelle** | `refs/a-cs16-client/` | nur A1-Manifest, siehe unten |
| **Ref B** (FuryBaM) | bedingte UI-/Menü-Referenz | `refs/b-cs16-goldsrc/` | erst Phase 4, ein Feature |
| **Server** | GameDLL + optionale Module | `server/` · geplant `server/game/` | eigene Entscheidung, siehe `docs/SERVER.md` |
| **Bots** | eigener Bereich | `bots/` | leer; Interface zur GameDLL; kein YaPB-Copy |

64-Bit-Desktop: `docs/PLATTFORMEN.md`.

## Stapel (eine Client-Lib, eine GameDLL)

```
Xash3D-FWGS
  → CS-Retro Client-Export
    → CS-Client-Body (A1, client/body/)
      → darauf integrierte NextClient-Funktionen

Xash3D-FWGS
  → CS-Retro GameDLL
      → optional Servermodule
      → separat: bots/
```

Am Ende: **eine** `client_*` und **eine** `cs_*` pro Plattform. Kein zweiter Client. Ref A wird nicht weiterentwickelt.

## A1-Body-Manifest (einzige Allowlist)

Pin: `refs/a-cs16-client/` `bb60674`. Vendort nach `client/body/`. Referenz bleibt, wird nicht gebaut.

Identisch in `docs/PHASE3-BODY.md`. Bei Abweichung gilt **diese** Tabelle.

| Pfad in `client/body/` | Rolle |
|------------------------|--------|
| `cl_dll/` | Client-Körper (ohne die entfernten Leichen, siehe unten) |
| `pm_shared/` | Prediction / Movement |
| `common/` | Shared GoldSrc-Header + `interface.cpp` |
| `public/` | `cl_dll/IGame*.h`, `strl*`, `utflib`, `build.h` — **ohne** `steam/` |
| `public/mainui/font/FontRenderer.h` | ein Typ-Header für `IGameMenuExports` (`HFont` / `CFontBuilder`) |
| `game_shared/` | Voice-HUD-Hilfen (Client). `voice_gamemgr.cpp` entfernt |
| `dlls/*.h` | 23 Waffen-/Entity-Header für `cs_weapons` + `wpn_shared` |
| `dlls/wpn_shared/*.cpp` | Shared-Waffen |
| `miniutl/` | UTL für Parser / Saytext / Draw (kein Menu) |
| `engine/*.h` | 16 Body-Engine-Header (`progs.h`, `eiface.h`, …). Kein `.cpp`. Nicht Xash-`engine/` als `-I` |

CS-Retro, nicht aus Ref A: `client/export/` (`GetClientAPI`).

**Nicht im Manifest:** YaPB, ReGameDLL, `mainui` / `mainui_cpp`, `public/steam/`, Ref-A-Engine-`.cpp`, restliches `3rdparty/`, Ref B.

Entfernte Body-Leichen (waren ungebaut oder inkompatibel): `F()`, `voice_gamemgr.cpp`, `ev_hldm.cpp`, `inputw32.cpp`, `input/input_sdl.cpp`, `cs_wpn/cs_baseentity.cpp`, Ref-A-`cl_dll/CMakeLists.txt`, `cl_dll.dsp`.

`voice_gamemgr.h` bleibt: `gamerules.h` braucht den Typ; die Server-Implementierung gehört in die GameDLL.

A1 heißt: den Unterbau holen, den NextClient aus Steam-`client.dll` vorausgesetzt hat. Danach NextClient-Funktionen auf diesen Unterbau / Xash setzen.

## Redundanz

Wenn A und B dieselbe Funktion haben:

1. Verhalten beider Varianten vollständig erfassen.
2. Stärken und Extra-Optionen benennen.
3. Gewünschtes NextClient-/CS-Retro-Verhalten festlegen.
4. Eine saubere CS-Retro-Implementierung bauen.
5. Alte Varianten entfernen.

Gilt für Funktionen, Klassen, Module, Build, Konfiguration, später Server/Bots.

## Keine Code-Leichen

Ungenutzter Code wird **entfernt**, nicht deaktiviert. Keine dauerhaften `#if 0`, auskommentierten Alts, Steam-/8684-Stubs, `nullptr`-Interface-Friedhöfe, ungebauten `.cpp` ohne Zweck, parallelen Alt+Neu.

Einzige temporäre Ausnahme: NextClient-Quellen als Port-Quelle, bis das Feature verifiziert in CS Retro liegt. Dann Hook-/NitroApi-Weg und Duplikat löschen.

## Ref-A-Regel

Ref A bleibt Referenz. **Ausnahme nur** dieses Manifest. Sonst kein Copy. `refs/a-cs16-client/` nicht als zweiten Client bauen. Dessen ReGameDLL nicht still als Server nutzen — Server-Body: `docs/SERVER.md`.
