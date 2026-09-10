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

## Eigenes Netzwerkprotokoll (Richtungsentscheidung)

CS Retro bekommt ein **eigenes** Netzwerkprotokoll. Das ist das Zielbild, keine Notlösung.

Spielkompatibilität mit Steam-Counter-Strike-1.6 oder anderen GoldSrc-Upstreams ist **kein** Ziel. Fremde Server sind keine Spielpartner: eine Zeile im Browser, die beim Verbinden nur scheitern kann, gehört nicht in die Liste. GoldSrc-/Steam-Antworten (`gs`) und fremde `gamedir` lehnt der LAN-Browser deshalb ab.

Der Steam-Master ist kein Ziel. Eine Internet-Serverliste für CS-Retro-Server ist ein **eigenes, noch nicht begonnenes** Vorhaben — getrennt vom LAN-Browser, getrennt vom historischen Master-Query der Engine (`internetservers` / `NET_MasterQuery`).

LAN-Discovery bleibt sinnvoll: eigene CS-Retro-Server im lokalen Netz finden. Die Engine trägt das vollständig (`localservers` broadcastet `A2A_INFO`, Antworten kommen über `UI_AddServerToList`). Der Menü-Browser löst die Adresse mit `pfnAdrToString` auf und zeigt sie. Die gemessene „Ping“-Spalte ist die Zeit vom Broadcast bis zur Antwort, keine Leitungsmessung.

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

Native Molotov/Incendiary: `docs/INFERNO.md`. Kein AMXX in der GameDLL.

## Listen- und Dedicated-Server

Eine GameDLL, eine Spiellogik. Unterschied nur Host-Modus, Konfiguration, optionale Module.

Xash:

- Dedicated: `Host_IsDedicated()`, CVar `servercfgfile`
- Listen: CVar `lservercfgfile` (Default `listenserver.cfg`)
- Einstieg: `GiveFnptrsToDll` + `GetEntityAPI` (`GetEntityAPI2` ist im Vendor `NOXREF`, Xash fällt auf `GetEntityAPI` zurück)
- Override: `-dll /pfad/cs_amd64.so` (umgeht Steam-`cs_amd64.so` mit executable stack)

## Dedicated vom laufenden Client (Zielbild)

Dedicated ist ein **Startweg aus dem gestarteten Client** — kein Extra-Tool und keine
zweite Binary von Hand. Dieselbe `ServerProfile`-Konfiguration wie Create Game
(`docs/MENUS.md`). Kein zweiter Wizard, keine zweite Config-Quelle.

Listen/LAN und Dedicated unterscheiden sich nur im Host-Modus (siehe oben). Die
Dedicated-UI kommt, wenn das Backend steht (Xash dedicated / vorhandene Engine-Wege).
**Nicht jetzt bauen.**

Zum Profil gehören später auch **Online vs. offline** (LAN/Internet-Sichtbarkeit,
`sv_lan` bzw. eigenes Netz — an das eigene Protokoll oben koppeln, kein Steam-Master)
und **RCON / Server-Zugriff** (Passwort, ob RCON an, die üblichen Zugangs-Einstellungen)
beim Dedicated und Create. Zielbild, nicht Auftrag jetzt. Auswahl über den
CVar-Katalog, nicht über eine einzelne `.scr` — `docs/MENUS.md`.

## Lokaler Admin (Core, kein Plugin)

Listen-Server: der lokale Host ist automatisch Administrator. Kein AMXX, kein Fake-Account, kein Steam.

Dedicated: kein automatisches Host-Admin.

## Optionale Servermodule

Beim **Start** festlegen. AMXX / Metamod sind **optional**, kein Core-Blocker.

AMX Mod X wird weiterhin entwickelt. Der aktuelle Upstream-Build und große Teile des Ökosystems sind **stark auf klassische 32-Bit-HL1-Server** ausgerichtet. 64-Bit separat untersuchen. **macOS hängt nicht an AMXX.**

Das ist der Ist-Stand, nicht das Ziel. Zielzustand ist eine **native CS-Retro-Plugin-Schnittstelle**: die Plugin-Idee bleibt, damit Dritte liefern können, die Zwischenschicht verschwindet (`docs/PHASEN.md`, Phase 7). AMXX-Code im Baum ist Fork wie alles andere und darf umgeschrieben und verschmolzen werden.

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

## Netz / Anbindungen

Eigenes Protokoll bleibt. Geplante Anbindung (nur wenn kostenlos nutzbar): Faceit —
`docs/PHASEN.md`, Phase 8. Kein Ersatz für unser Netz, keine Steam-Kompatibilität
durch die Hintertür.
