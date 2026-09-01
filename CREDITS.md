# Credits

CS Retro baut auf jahrelanger Arbeit vieler Entwickler und Projekte auf.

Wir haben diese Arbeit **nicht** selbst geschaffen. CS Retro ist unsere integrierte Weiterentwicklung: vendorte Quellen, eigene Architektur, 64-Bit und Cross-Platform. Der Dank gilt den Menschen und Projekten, ohne die das nicht möglich wäre.

Diese Datei bleibt dauerhaft. Sie ist keine Phasendoku und wird nach einem Phasenabschluss nicht gelöscht.

**Diese Liste ergänzt die Credits der Upstream-Projekte.** Vorhandene AUTHORS, CREDITS, README-Acknowledgements und Contributor-Listen der verwendeten Quellen bleiben die maßgebliche Herkunft. Hier nicht kürzen oder ersetzen.

Vor einer späteren öffentlichen Veröffentlichung die dann tatsächlich verwendeten Pins noch einmal durchgehen und namentliche Contributors/Acknowledgements der jeweiligen Projekte hier nachziehen.

Technische Pins: `docs/UPSTREAM.md`. Lizenzen: `docs/LIZENZEN.md`.

## Spiel und Ursprung

- Valve und die ursprünglichen Half-Life- und Counter-Strike-Entwickler
- GoldSrc / Half-Life SDK
- Counter-Strike 1.6
- Steam als ursprüngliche Bezugs- und Distributionsplattform des Spiels

NextClient dankt Valve ausdrücklich für Counter-Strike 1.6 und die Haltung zur Modder-Community (`client/README.md`, Abschnitt Thanks).

## Client-Zielbasis — CS NextClient

Projekt: [CS-NextClient/NextClient](https://github.com/CS-NextClient/NextClient) und Contributors.

Zugehörig:

- [NextClientServerApi](https://github.com/CS-NextClient/NextClientServerApi)
- [NclNitroApi](https://github.com/CS-NextClient/NclNitroApi)
- [ncl-hl1-source-sdk](https://github.com/CS-NextClient/ncl-hl1-source-sdk)

NextClient bleibt die funktionale Zielbasis des Clients.

### Thanks aus NextClient (Quelle: `client/README.md`)

Nicht ersetzt, hier mitgeführt:

- [Nordic Warrior](https://github.com/Nord1cWarr1or) — Feedback und Fehlerberichte
- [fl0werD](https://github.com/fl0werD) — Sprite API
- [Mikko Kokko](https://github.com/mikkokko) — [csldr](https://github.com/mikkokko/csldr)
- [Felipe](https://github.com/LAGonauta) — [MetaAudio](https://github.com/LAGonauta/MetaAudio)
- [MoeMod](https://github.com/MoeMod) — [Thanatos-Launcher](https://github.com/MoeMod/Thanatos-Launcher) (GameUI/VGUI2)
- [tmp64](https://github.com/tmp64) — [hl1_source_sdk](https://github.com/tmp64/hl1_source_sdk)
- [TsarVar](https://tsarvar.com) — Idee einer JS-API für GameUI
- [s1lent](https://github.com/s1lentq) — Rat, Fixes, [avatarid-spec](https://github.com/goldclient-plus/avatarid-spec)
- [lozatto](https://github.com/lozatto) — HWID
- [SanyaSho](https://github.com/SanyaSho) — [BarsTech_goldsrc_compatible_public](https://github.com/SanyaSho/BarsTech_goldsrc_compatible_public) (VGUI2-Fonts)
- Valve — Counter-Strike 1.6
- alle, die mit Berichten, Vorschlägen und Unterstützung beitragen

## Engine — Xash3D

- Ursprüngliches [Xash3D](https://www.moddb.com/engines/xash3d-engine) von Unkle Mike
- [Xash3D FWGS](https://github.com/FWGS/xash3d-fwgs) und Contributors
- Flying With Gauss / XashXT Group (Engine-Credits: `engine/3rdparty/mainui/menus/Credits.cpp`)

Namentlich aus der FWGS-Dokumentation (`engine/Documentation/donate.md`, `engine/README.md`), unvollständig gegenüber der vollen Contributor-Historie:

- [a1batross](https://github.com/a1batross) — SDL2/Linux-Port, Engine-Maintainer, Flying With Gauss
- [nekonomicon](https://github.com/nekonomicon) — hlsdk-portable, Ports, PNG
- [Velaron](https://github.com/Velaron) — Android, Voice, cs16-client
- [SNMetamorph](https://github.com/SNMetamorph) — Windows-Port, Voice
- [$_Vladislav](https://github.com/Vladislav4KZ) — Tests und Beiträge

Volle Historie: GitHub-Contributors von FWGS/xash3d-fwgs und Notices in `engine/3rdparty/`.

## Client-Body — Velaron/cs16-client

Projekt: [Velaron/cs16-client](https://github.com/Velaron/cs16-client).

Wichtige Contributors laut deren README (`refs/a-cs16-client/README.md`):

- [a1batross](https://github.com/a1batross) — ursprünglicher Autor und Maintainer
- [jeefo](https://github.com/jeefo) — [YaPB](https://github.com/yapb/yapb)
- die Entwickler von [ReGameDLL_CS](https://github.com/rehlds/ReGameDLL_CS)
- [Vladislav4KZ](https://github.com/Vladislav4KZ)
- [SNMetamorph](https://github.com/SNMetamorph) — PS Vita
- [Alprnn357](https://github.com/Alprnn357) — Touch-Menüs
- [wh1tesh1t](https://github.com/wh1tesh1t), [pwd491](https://github.com/pwd491), [Elinsrc](https://github.com/Elinsrc), [xiaodo1337](https://github.com/xiaodo1337), [nekonomicon](https://github.com/nekonomicon), [lewa-j](https://github.com/lewa-j) und weitere

## GameDLL — ReHLDS / ReGameDLL_CS

- [ReGameDLL_CS](https://github.com/rehlds/ReGameDLL_CS) und Contributors
- [ReHLDS](https://github.com/dreamstalker/rehlds) — Grundlage, der ReGameDLL-README explizit dankt (`server/game/README.md`)

Lizenzwechsel MIT (Juli 2025), namentlich in `server/game/LICENSE-TRANSITION.md`:

- s1lent
- Vaqtincha
- In-line
- wopox
- WPMGPRoSToTeMa

sowie alle weiteren signifikanten Contributors des Projekts.

## Weitere Referenzen

- [FuryBaM/cs16-goldsrc-client](https://github.com/FuryBaM/cs16-goldsrc-client) — Ref B, bedingte Menü-Referenz
- [YaPB](https://github.com/yapb/yapb) (jeefo und Contributors) — beobachtet als Bot-Quelle; noch nicht vendort
- ZBot in ReGameDLL — mitvendort, später mit YaPB und weiteren vergleichen

Jedes weitere Projekt, aus dem wir später Code oder Verhalten übernehmen, hier und in `docs/UPSTREAM.md` nachtragen.
