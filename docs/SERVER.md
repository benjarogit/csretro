# Server / GameDLL

Lebende Serverarchitektur. Plugin-Infrastruktur nicht bauen, bevor der 64-Bit-GameDLL-Stack unter Xash läuft.

## Zielstapel

```
Xash3D-FWGS
  → CS-Retro GameDLL          (Spiellogik, Listen + Dedicated)
      → optional: Server-Modul-Schicht (Metamod / AMXX / …)
      → später über Interface: bots/
```

`NextClientServerApi` unter `server/` ist **nicht** die Counter-Strike-GameDLL. Es bleibt Herkunft für NCLM/Protokoll, bis die Funktionen in die CS-Retro-GameDLL wandern.

## Vendort (2026-09-01)

| Feld | Wert |
|------|------|
| Quelle | https://github.com/rehlds/ReGameDLL_CS |
| Pin | `b0889847fe6d03898be88acc9e366660efb40ab5` (2026-08-28) |
| Baum | `server/game/` |
| Manifest | `server/game/MANIFEST.md` |
| Target | `csretro_gamedll` — `cmake/CsretroGameDll.cmake` |
| Artefakt Linux x86_64 | `cs_amd64.so` |
| Build | `./scripts/build-gamedll.sh` |
| Smoke | `./scripts/smoke-gamedll.sh dedicated\|listen` |

Nicht Ref-A-ReGameDLL. Nicht der Upstream-CMake/SLN. Kein `CMAKE_POLICY_VERSION_MINIMUM=3.5`.

## Listen- und Dedicated-Server

Eine GameDLL, eine Spiellogik. Unterschied nur Host-Modus, Konfiguration, optionale Module.

Xash:

- Dedicated: `Host_IsDedicated()`, CVar `servercfgfile`
- Listen: CVar `lservercfgfile` (Default `listenserver.cfg`)
- Einstieg: `GiveFnptrsToDll` + `GetEntityAPI` (`GetEntityAPI2` ist im Vendor `NOXREF`, Xash fällt auf `GetEntityAPI` zurück)
- Override: `-dll /pfad/cs_amd64.so` (umgeht Steam-`cs_amd64.so` mit executable stack)

## Lokaler Admin (Core, kein Plugin)

Listen-Server: der lokale Host ist automatisch Administrator. Kein AMXX, kein Fake-Account, kein Steam.

Dedicated: kein automatisches Host-Admin.

## Optionale Servermodule

Beim **Start** festlegen. AMXX / Metamod sind **optional**, kein Core-Blocker.

AMX Mod X wird weiterhin entwickelt. Der aktuelle Upstream-Build und große Teile des Ökosystems sind **stark auf klassische 32-Bit-HL1-Server** ausgerichtet. 64-Bit separat untersuchen. **macOS hängt nicht an AMXX.**

## Bots (Migration)

Endzustand: `bots/` = Bot-System, `server/game/` = Spiellogik.

**Jetzt:** ZBot vollständig in der GameDLL mitgebaut (`dlls/bot/*`, `game_shared/bot/*`). Hostage-Nav teilt sich diesen Code. Nicht amputieren. Vorübergehende Redundanz ist dokumentierte Migration.

Runtime-Testdaten (nicht im Git): `BotProfile.db` / `BotChatter.db` aus Upstream `regamedll/extra/zBot/bot_profiles.zip`; Map-`.nav` unter `build/run/cstrike/maps/`. Kein YaPB als Ersatz-Bot.

Später: alle Bot-Quellen vergleichen → eigener CS-Retro-Bot → erst dann alte Implementierungen entfernen.

## 64-Bit-Audit (aktives Manifest)

| Thema | Urteil |
|-------|--------|
| `XASH_64BIT` + `build.h` | Pflicht. Ohne sie ist `MAKE_STRING` ein Pointer→`uint32`-Cast |
| `string_t` / `QString` | **32-Bit behalten** — Xash-String-Pool |
| `MAKE_STRING` | 64-Bit-Pfad: Offset wenn im Pool, sonst `ALLOC_STRING` |
| `pfnFunctionFromName` / `pfnNameForFunction` | Xash-Vertrag: `unsigned long` (nicht `uint32`) |
| Save `FIELD_FUNCTION` | Name serialisiert; Restore in `uintptr_t` |
| Save `FIELD_POINTER` | Dateiformat bleibt 32-Bit-`int` (GoldSrc-Savegame) |
| `GiveFnptrsToDll` | `memcpy` der **GameDLL**-`enginefuncs_t` (kürzer als Xash; Extra-Feld am Ende, sicher) |
| `func_t` (`unsigned int`) | unbenutzt im aktiven Build |
| POSIX-`DWORD` = `unsigned long` | nur tot (`assert_dialog.cpp`, nicht im Target) |
| Pointer-Warnungen | nicht global abgeschaltet |

## Plattform-Status

| Ziel | Stand |
|------|--------|
| Linux x86_64 | **gebaut und geladen.** Dedicated- + Listen-Smoke `de_dust`. ASan+UBSan-Build + Dedicated-Smoke ohne Report |
| Windows x86_64 | CMake-Modell vorbereitet (`platform_win32.cpp`, `.dll`). Compile-Gate auf diesem Host nicht gelaufen |
| macOS ARM64 | CMake-Modell (`cs_arm64.dylib`, kein SSE-File). Compile-Gate nicht gelaufen |
| macOS Intel x86_64 | CMake-Modell (`cs_amd64.dylib`). Compile-Gate nicht gelaufen |

## Redundanz

Wenn AMXX, ReGameDLL-API, NextClientServerApi und CS Retro dieselbe Serverfunktion anbieten: vergleichen → eine CS-Retro-Lösung → interne Duplikate entfernen. Optionale Plugin-APIs dürfen dieselbe Funktion *aufrufen*, nicht eine zweite Spiellogik implementieren.
