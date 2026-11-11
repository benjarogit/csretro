# Spieleridentität

Es gibt einen Slot. `ResolvePlayerIdentity` bekommt den Spielnamen aus dem
`name`-Setinfo und optional eine Steam-Persona.

Ist niemand eingeloggt, bleibt der Spielname und es gibt kein Portrait. Ist
die Persona eingeloggt und hat einen Namen, ist das der angezeigte Name. Das
Portrait sind die Pixel, die der Aufruf schon mitbringt. Die Funktion lädt
kein Bild, speichert keine Zugangsdaten und startet Steam nicht.

Die Entscheidung steht in [Plan, Abschnitt 3.7](../../plan/csretro-plan.md).

## Test

```bash
cd client
cmake --preset linux-tests
cmake --build --preset linux-tests-release --target player_identity-tests
./build/linux-tests/out/bin/Release/player_identity-tests
```

Der Test spricht kein Netz an und startet das Spiel nicht.
