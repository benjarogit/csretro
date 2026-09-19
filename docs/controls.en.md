# Controls

Bindings are configurable. Your settings are authoritative, not a fixed list from another
Counter-Strike version. These commands provide a starting point.

| Action | Console command | Usual binding |
| --- | --- | --- |
| Movement | `+forward`, `+back`, `+moveleft`, `+moveright` | W, S, A, D |
| Jump / crouch | `+jump` / `+duck` | Space / Ctrl |
| Primary / secondary action | `+attack` / `+attack2` | M1 / M2 |
| Reload | `+reload` | R |
| Buy | `buy` | B |
| Team selection | `chooseteam` | M |
| Grenade group | `slot4` | 4 |
| Weapon selection | `invnext` / `invprev` | Mouse wheel |
| Scoreboard | `+showscores` | Tab |

## Team, class and buying

After joining, choose a team and class. The buy menu offers items for your team.
Money, buy zone and buy time determine what can be purchased.
The menus are still evolving; [screenshots](screenshots.md) show a development snapshot.

## Grenades

Alongside HE, flash and smoke, T Molotovs and CT Incendiaries are implemented natively in game code.
The intended controls are M1 for a normal throw, M2 for a short throw and both for an intermediate
throw; hold to prepare, release to throw. Grenades belong to slot 4.

!!! warning "Known limitation"
    The first M1 press is still unreliable in reported sequences.
    Incendiary damage without visible flames has also been reported.
    This guide describes intended behavior, not a verified fix.
    See [project status](status.md).

For useful reports, distinguish mouse/keyboard buying, slot/wheel selection, freeze time,
button press and release.
