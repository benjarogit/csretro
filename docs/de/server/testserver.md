# Testserver

Der Testserver ist ein reproduzierbarer GoldSrc-Server für End-to-End-Tests. Er
ist **kein Produktionsserver**: Er läuft nur lokal oder in CI, hört auf `sv_lan 1`
und ist mit `-insecure` gestartet.

## Stack

| Komponente | Version | Rolle |
|---|---|---|
| [ReHLDS](https://github.com/dreamstalker/rehlds) | `3.15.0.896` | Engine |
| [ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) | `5.30.0.814` | Spiellogik |
| [Metamod-R](https://github.com/rehlds/Metamod-R) | `1.3.0.149` | Plugin-Loader |
| [AMX Mod X](https://github.com/alliedmodders/amxmodx) | `1.10.0.5487` | Plugin-Laufzeit |
| [ReAPI](https://github.com/rehlds/ReAPI) | `5.29.0.358` | Modul-API |
| CSRETRO-Modul | `1.6.0` | `nextclientapi_amxx_i386.so` |
| Testplugin | – | `test_nextclient_api.amxx` |

Alle Versionen und SHA-256-Digests stehen in `versions.lock.json`. Die
HLDS-Basisdateien kommen als App 90 über SteamCMD.

## Aufbau

| Pfad im Image | Inhalt |
|---|---|
| `/server/hlds_linux` | ReHLDS-Engine plus App-90-Basisdateien |
| `/server/cstrike/dlls/cs.so` | ReGameDLL_CS |
| `/server/addons/metamod/metamod_i386.so` | Metamod-R |
| `/server/addons/plugins.ini` | Metamod-Pluginliste, lädt AMXX |
| `/server/cstrike/addons/amxmodx` | AMX Mod X, Module und Plugins |
| `/server/cstrike/models`, `sound`, `sprites`, `materials` | Assets des Testplugins |
| `/server/libcsretro_steamcompat.so` | Steam-Stub, siehe unten |
| `/entrypoint.sh`, `/healthcheck.py` | Start und Healthcheck |

Das Image wird aus `server/pack/` gebaut. Der Build-Kontext enthält zusätzlich
die vendorten Runtime-Archive aus `server/vendor/dist/`, das gebaute Modul,
das kompilierte Testplugin und die Testassets. `tests/e2e/run.sh` stellt
diesen Kontext zusammen; von Hand erledigt das `scripts/build-server.sh --stage`.

## Schnellstart

```bash
tests/e2e/run.sh
```

Das Skript

1. baut Modul und Testplugin über `scripts/build-server.sh --stage`,
2. prüft die vendorten Runtime-Archive aus `server/vendor/dist/` per SHA-256,
3. baut das Image und startet es mit Docker Compose,
4. wartet auf den Healthcheck,
5. prüft Log und RCON mit `tests/e2e/test_server.py`,
6. entfernt Container und Volumes in jedem Fall.

Voraussetzungen: Docker mit Compose v2 und Netzwerk beim ersten Lauf, weil
SteamCMD App 90 herunterlädt. Die Server-Binaries kommen aus `server/vendor/dist/`.

| Variable | Standard | Bedeutung |
|---|---|---|
| `CSR_E2E_HOST_PORT` | `27015` | veröffentlichter UDP-Port |
| `CSR_E2E_RCON_PASSWORD` | `csretro-e2e` | RCON-Passwort |
| `CSR_E2E_MAP` | `de_dust2` | Startmap |
| `CSR_E2E_PROJECT` | `csretro-e2e` | Compose-Projektname |

Im Container setzt `entrypoint.sh` `CSR_MAP`, `CSR_MAXPLAYERS`, `CSR_PORT` und
`CSR_RCON_PASSWORD`; alle haben einen Default, damit das Image auch ohne Compose
startet.

## Healthcheck und RCON

Der Healthcheck fragt per UDP `getchallenge` ab und schickt dann
`rcon <challenge> <passwort> status`. Das ist das klassische GoldSrc-Verfahren:
Ohne vorherige Challenge antwortet der Server mit `Bad rcon_password.`, auch
wenn das Passwort stimmt. Eine erfolgreiche Antwort beginnt mit
`\xff\xff\xff\xffl` und enthält `hostname`, `version`, `map` und `players`.

`tests/e2e/test_server.py` prüft zusätzlich:

- `Metamod-r v1.3.0.149`, AMXX `1.10.0.5487` und
  `[nclapi] Next Client API Successfully loaded, version 1.6.0` im Log,
- `Started map "de_dust2"`,
- `nclapi_version` ist `1.6.0`,
- `amxx plugins` meldet `Test NCL API` als `running`,
- keine `FATAL ERROR`, `Fencepost`, `Segmentation fault`, kein Dump und kein
  `[AMXX] Run time error`.

## Steam-Stub

ReHLDS 3.15 ist gegen die Steamworks-ABI vor dem 25-jährigen Jubiläum gebaut: Es
importiert `SteamGameServer_Init`, `SteamGameServer` und `SteamApps` und erwartet
`ISteamGameServer011`. Das Default-Public-App-90-Paket liefert nur
`SteamInternal_GameServer_Init` und `SteamGameServer015`. Die Vtable-Layouts sind
nicht austauschbar; ein direktes Laden der neuen Bibliothek stürzt in
`CSteam3Server::Activate()` ab oder flutet das Log mit Interface-Assertions.

Der Testserver antwortet diese ABI deshalb selbst. `server/pack/steamcompat/steamstub.cpp`
wird als `libcsretro_steamcompat.so` gebaut und per `LD_PRELOAD` in den
Engine-Prozess geladen:

- `SteamGameServer_Init` meldet Erfolg, `SteamGameServer()` liefert eine
  No-op-Implementierung von `ISteamGameServer011`.
- `SteamAPI_Init` meldet `false`, also den Pfad, den ReGameDLL ohne Steam-Client
  ohnehin behandelt.

Der Stub authentifiziert niemanden und ist ausschließlich für den lokalen,
unsicheren Testserver gedacht. Ein Produktionsserver braucht eine echte
Steam-Anbindung; das ist nicht Teil von G06.

## Kein A2S mit dem Stub

ReHLDS beantwortet Connectionless-Queries nicht mehr selbst: `SVC_Info` und
`ValidInfoChallenge` sind `NOXREF`, und `SV_ReadPackets` reicht jedes
Connectionless-Paket an `Steam_HandleIncomingPacket` weiter. Die Antworten holt
sich die Engine über `GetNextOutgoingPacket` aus der Steam-Gameserver-Schicht.
Mit dem Stub gibt es diese Schicht nicht, also antwortet der Server **nicht** auf
`A2S_INFO`.

Deshalb nutzt der Healthcheck RCON, das die Engine selbst verarbeitet. Wie A2S
unter Default Public wieder verfügbar wird, ist eine offene Entscheidung für G07:
entweder ein Adapter von `ISteamGameServer011` auf die 015-Schnittstelle oder ein
eigener Query-Responder.

## Runtime-Basis

Das Runtime-Image ist `i386/debian:trixie-slim` (glibc 2.41). Das CSRETRO-Modul
verlangt `GLIBC_ABI_GNU_TLS` und `GLIBC_2.38`; Debian 12 (glibc 2.36) kann es
nicht laden. Die Build-Stages laufen auf `debian:bookworm-slim`, weil dort nur
entpackt und mit `g++-multilib` der Stub übersetzt wird.

## Fehlersuche

| Symptom | Ursache | Lösung |
|---|---|---|
| `CSR_STEAMCMD_IMAGE` fehlt | Compose ohne Digest gestartet | `tests/e2e/run.sh` nutzen oder die Variable aus `versions.lock.json` setzen |
| Image-Build scheitert bei App 90 | SteamCMD-Netzfehler | erneut ausführen; der Build wiederholt den Download viermal |
| Healthcheck bleibt `unhealthy` | Map lädt nicht oder RCON-Passwort leer | `docker compose logs server` prüfen |
| `Bad rcon_password.` trotz richtigem Passwort | Challenge-Schritt fehlt | erst `getchallenge`, dann `rcon <challenge> <passwort> <befehl>` |
| `GLIBC_ABI_GNU_TLS not found` | Runtime älter als der Build-Host | Runtime-Image nicht auf bookworm zurückstellen |
| `Module ... failed to load` | Modul fehlt oder falsche AMXX-Version | Staging-Manifest und `versions.lock.json` vergleichen |
| `Mod_LoadModel: ... not found` | Testassets fehlen im Kontext | `scripts/build-server.sh --stage` neu ausführen |
| `Not loading plugin ... already loaded` | Modul doppelt in `modules.ini` | `modules.ini` leer lassen, AMXX erkennt Module selbst |
| Kein `A2S_INFO` | Steam-Stub ohne Gameserver-Schicht | erwartet; RCON für Healthchecks nutzen |

## Nachweis (G06, 8. November 2026)

```text
ok   log: Metamod-r v1.3.0.149
ok   log: AMX Mod X version 1.10.0.5487
ok   log: [nclapi] Next Client API Successfully loaded, version 1.6.0
ok   log: Started map "de_dust2"
ok   rcon: map     :  de_dust2 at: 0 x, 0 y, 0 z
ok   rcon: "nclapi_version" is "1.6.0"
ok   rcon: amxx plugins: Test NCL API running
all e2e checks passed
```
