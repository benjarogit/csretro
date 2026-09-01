# Server / GameDLL

Lebende Serverarchitektur. Plugin-Infrastruktur nicht bauen, bevor der 64-Bit-GameDLL-Stack unter Xash läuft.

## Zielstapel

```
Xash3D-FWGS
  → CS-Retro GameDLL          (Spiellogik, Listen + Dedicated)
      → optional: Server-Modul-Schicht (Metamod / AMXX / …)
      → separat über Interface: bots/
```

`NextClientServerApi` unter `server/` ist **nicht** die Counter-Strike-GameDLL. Es bleibt Herkunft für NCLM/Protokoll, bis die Funktionen in die CS-Retro-GameDLL wandern.

## Listen- und Dedicated-Server

Eine GameDLL, eine Spiellogik. Unterschied nur Host-Modus, Konfiguration, optionale Module.

Xash hat das bereits:

- Dedicated: `Host_IsDedicated()`, CVar `servercfgfile` (Default `server.cfg`)
- Listen: CVar `lservercfgfile` (Default `listenserver.cfg`)
- Einstieg: `GiveFnptrsToDll` + `GetEntityAPI` / `GetEntityAPI2` (`engine/engine/server/sv_game.c`)

Kein zweites paralleles Konfigsystem. CS-Retro-Profile dürfen darüberliegen (Vanilla, Test, Module, Bots), jede Einstellung hat **eine** Quelle.

Der Client startet ein LAN-Spiel über Xash-MainUI. Der Listen-Server muss für Entwicklung allein reichen — kein Pflicht-Dedicated für jeden Test.

## Lokaler Admin (Core, kein Plugin)

Listen-Server: der lokale Host ist automatisch Administrator. Kein AMXX, kein Fake-Account, kein Steam.

Der Server weiß, dass er Listen ist und welcher Client der Host ist. Rechte über eine interne Server-Schnittstelle.

Dedicated: kein automatisches Host-Admin. Administration über `server.cfg`, Konsole, später definierte Remote-/Modulwege.

## Optionale Servermodule

Beim **Start** festlegen, nicht während einer laufenden Map hot-loaden:

| Profil | Ladung |
|--------|--------|
| Vanilla / Clean | nur CS-Retro-GameDLL |
| Server Modules | konfigurierte Modulschicht + GameDLL |

AMXX / Metamod sind **optional**. Sie dürfen nie Voraussetzung sein für: Serverstart, LAN, Bots, lokalen Admin, CS-Spiellogik.

Offizielle AMX Mod X-Builds sind 32-Bit und eingestellt (`is_amd64_server` deprecated). Metamod-FWGS existiert für 64-Bit-Xash; das ist Community, nicht macOS-Garantie. **macOS hängt nicht an AMXX.** Fehlende Modul-Unterstützung auf einem Ziel blockiert den Core nicht.

## Bots

`bots/` bleibt eigener Bereich. Interface zur GameDLL, kein Copy der ReGameDLL-ZBot-Quellen in den Core. Testfunktionen (Bot add/remove) sind Core-Perspektive, Implementierung später.

## Redundanz

Wenn AMXX, ReGameDLL-API, NextClientServerApi und CS Retro dieselbe Serverfunktion anbieten: vergleichen → eine CS-Retro-Lösung → interne Duplikate entfernen. Optionale Plugin-APIs dürfen dieselbe Funktion *aufrufen*, nicht eine zweite Spiellogik implementieren.

## GameDLL-Gate (Entscheidung 2026-09-01, noch nicht vendort)

Alte Regel „kein ReGameDLL still aus Ref A“ bleibt gültig. Das hier ist eine **neue, bewusste** Server-Body-Wahl.

### Kandidaten

| Quelle | Eignung |
|--------|---------|
| Steam-`dlls/cs.so` / `cs_amd64.so` | nein — 32-Bit bzw. hier `executable stack`; nicht unser Code |
| Ref-A-`3rdparty/ReGameDLL_CS/` | nein — alter Pin, ReHLDS-lastig, Bots/YaPB-Nähe, nicht die aktuelle Xash-64-Linie |
| `NextClientServerApi` | nein — AMXX-Modul, keine GameDLL |
| **rehlds/ReGameDLL_CS** (aktuell) | **empfohlen** als Server-Body-Quelle |

### Warum rehlds/ReGameDLL_CS

- Expliziter 64-Bit-Xash-Support: PR #1053 (a1batross), Release 5.30.0.814 (2026-05-18)
- CMake-Option `XASH_COMPAT`: ohne sie erzwingt das Upstream-CMake `-m32`; mit ihr `-DXASH_64BIT` und Xash-LibraryNaming (`cs_amd64.so`)
- `string_t` bleibt 32-Bit über den Engine-String-Pool (gleiche ABI-Idee wie der Client-Body)
- MIT; CS-1.6-kompatible Spiellogik
- Geprüfter HEAD (Analyse-Clone, nicht im Repo): `b088984` (2026-08-28)

### Was nicht automatisch mitkommt

- ZBot / `dlls/bot/` → bleibt hinter `bots/`, nicht still Core
- ReAPI / Hookchains → keine Pflicht für den Vanilla-Listen-Server
- ReHLDS-Annahme → Xash ist die Engine
- AMXX/Metamod → optionale Schicht danach

### Plattform-Lücken (vor Vendor)

| Ziel | Stand |
|------|--------|
| Linux x86_64 + `XASH_COMPAT=ON` | Configure ok (`cs_amd64.so`, `XASH_AMD64`). CMake 4.4: `CMAKE_POLICY_VERSION_MINIMUM=3.5`. **Link noch nicht:** Clang 22, `saverestore.cpp:429` `NAME_FOR_FUNCTION((uint32)*data)` — Pointer→`uint32`. Beim Vendor gezielt auf Xash-64-Funktionsnamen umstellen. |
| Windows x86_64 | Root-CMake: `FATAL_ERROR` „Windows isn't supported, use msvc/ReGameDLL.sln“ — das SlN ist historisch 32-Bit. Eigenes 64-Bit-Windows-Gate nötig |
| macOS ARM64 / x86_64 | `XASH_APPLE` in LibraryNaming vorhanden, kein nachgewiesener CI-Build |

Probe 2026-09-01: Clone `b088984` nach `/tmp` (nicht im Repo). Kein `cs_amd64.so` entstanden. Nicht still aus Ref A ziehen.

### Nächster Schritt (nicht in diesem Stand)

1. Aktuelles `rehlds/ReGameDLL_CS` separat nach `server/game/` vendorn (nicht Ref A kopieren).
2. Nur GameDLL/Server-Unterbau, Bots nicht als CS-Retro-Bots übernehmen.
3. Linux x86_64 mit `XASH_COMPAT` bauen, von Xash laden, Map im Listen-Server starten → **dann 3C zuende**.
4. Erst danach 3D (NextClient-Features).
5. NextClientServerApi-Funktionen einzeln in diese GameDLL ziehen.

Analysewerkzeug: Ghidra, wenn Original-`cs.so`/Exports/Structs unklar sind. Keine Artefakte committen.
