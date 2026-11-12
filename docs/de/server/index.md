# Server

Der Client ist noch nicht spielbar. Der Testserver-Stack ist getrennt von der
Plugin-Anleitung und dient dem reproduzierbaren Gate-Nachweis.

## Stack

ReHLDS, ReGameDLL_CS, Metamod-R, AMXX und die Server-API sind in
`versions.lock.json` gepinnt. Kein Git-Cherry-Pick: neue Versionen sind Lock-
Einträge mit geprüfter SHA-256.

## Seiten

| Thema | Seite |
|---|---|
| Testserver betreiben | [Testserver](testserver.md) |
| Server-API bauen | [Build](../build/server.md) |
| Security | [SECURITY](https://github.com/benjarogit/csretro/blob/main/SECURITY.md) |

Produkt-Server-Management (`csretro-server`) steht im Backlog nach 1.0.0.
