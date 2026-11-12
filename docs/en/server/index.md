# Servers

The client is not playable yet. The test-server stack is separate from the
plugin guide and exists for reproducible gate evidence.

## Stack

ReHLDS, ReGameDLL_CS, Metamod-R, AMXX and the server API are pinned in
`versions.lock.json`. These are not git cherry-picks: a new version is a lock
entry with a verified SHA-256.

## Pages

| Topic | Page |
|---|---|
| Run the test server | [Test server](testserver.md) |
| Build the server API | [Build](../build/server.md) |
| Security | [SECURITY](https://github.com/benjarogit/csretro/blob/main/SECURITY.md) |

Product server management (`csretro-server`) is backlog after 1.0.0.
