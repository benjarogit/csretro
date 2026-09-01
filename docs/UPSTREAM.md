# Upstream-Pins (Vendor, kein Submodule)

**Unser Origin:** https://github.com/benjarogit/csretro (`main`, privat).
Import 2026-09-01, shallow clone, `.git` entfernt. Kein `git submodule`.

Altes Remote-`main` (Xash+cs16-client bis `8ece11c`, Tag `v0.2.0`) ist kein Upstream mehr.

Danksagung: `CREDITS.md`. Lizenzen: `docs/LIZENZEN.md`.

## Upstream-Policy (verbindlich)

CS Retro ist **nicht** von seinen Upstreams abhängig und folgt keinem Upstream automatisch.

Alle relevanten Quellen werden vendort und danach als Bestandteil dieses Worktrees gepflegt.

Upstreams bleiben dokumentiert und werden als Entwicklungs-/Ideenquelle beobachtet. NextClient bleibt die **funktionale Zielbasis des Clients**. Diese Policy ändert die Rollen in `docs/ROLLEN.md` nicht.

### Nicht automatisch

Kein automatisches Merge, Rebase, Vendor-Refresh, Submodule-Update, „immer HEAD ziehen“.

### Selektiver Port (kein Cherry-Pick im Git-Sinn)

Vendorte Trees haben keine `.git`-Historie. „Cherry-pick“ heißt hier: selektiver Upstream-Port/Backport.

Vorgehen:

1. Upstream-Änderung entdecken.
2. Prüfen, ob sie für CS Retro relevant ist.
3. Mit unserer Implementierung vergleichen.
4. Nur das Sinnvolle übernehmen.
5. An Architektur, 64-Bit und Cross-Platform anpassen.
6. Testen.
7. Herkunft hier dokumentieren (Tabelle unten).
8. Keine alte/doppelte Implementierung zurücklassen.

Ist unsere Lösung bereits besser oder vollständiger: nichts übernehmen.

Ein Upstream darf temporär gefetcht werden, um einen einzelnen Patch zu lesen. Daraus entsteht keine dauerhafte Remote-/Submodule-Abhängigkeit.

**Beobachten → Bestes auswählen → in CS Retro integrieren → CS Retro bleibt eigenständig.**

Wenn Upstream A, B und CS Retro dieselbe Funktion haben: alle Varianten vergleichen, eine CS-Retro-Implementierung behalten. Beobachtung darf keine parallelen Kopien erzeugen.

## Quellen

| Pfad | Repo | Rolle | Pin | Import |
|------|------|-------|-----|--------|
| `client/` | https://github.com/CS-NextClient/NextClient | funktionale Client-Zielbasis | `f5addc2ba0276d30d26c40eeb8d51fb9fc416102` | 2026-08-02 |
| `client/dep/NclNitroApi/` | https://github.com/CS-NextClient/NclNitroApi | Port-Quelle / Typen | `f73fc1a7592ba413aa382b5b3857e4a40b1b545e` | mit NextClient (nicht HEAD) |
| `client/dep/NclNitroApi/dep/ncl-hl1-source-sdk/` | https://github.com/CS-NextClient/ncl-hl1-source-sdk | HL1-SDK-Typen | `46c310389d669685ec953b8c23f651bd60465e19` | 2026-06-24 |
| `server/` | https://github.com/CS-NextClient/NextClientServerApi | NCLM/Protokoll-Herkunft, nicht GameDLL | `1c7e5c61191f1b949a28cade96dc1d821eedb335` | 2026-07-01 |
| `engine/` | https://github.com/FWGS/xash3d-fwgs | einzige Engine | `1442d14a69093780389104dcb7369aa3685945cf` | 2026-08-27 |
| `client/body/` | https://github.com/Velaron/cs16-client | A1-Client-Body (Manifest) | `bb60674c120ae9bf8fa7854018bea8a77e71c17f` | 2026-09-01 (Vendor) |
| `refs/a-cs16-client/` | https://github.com/Velaron/cs16-client | Body-Referenz, nicht gebaut | derselbe Pin | 2026-08-24 |
| `refs/b-cs16-goldsrc/` | https://github.com/FuryBaM/cs16-goldsrc-client | Menü-/VGUI-Referenz bereits in Phase 3M; gezielte zusätzliche Feature-Ports später | `b662acca3ce74c2c9851cc842592c58661d95799` | 2026-08-27 |
| `server/game/` | https://github.com/rehlds/ReGameDLL_CS | GameDLL-Körper | `b0889847fe6d03898be88acc9e366660efb40ab5` | 2026-09-01 |

Zuletzt geprüft (Clone/Vergleich, kein Sync): ReGameDLL_CS 2026-09-01 = Pin.

Engine-3rdparty (mitimportiert, kein Submodule): MultiEmulator, bzip2, xash-extras, gl-wes-v2, gl4es, libbacktrace, libogg, library_suffix, maintui, mainui (+ miniutl), mbedtls, nanogl, opus, opusfile, vgui_support (+ vgui-dev), vorbis.

### Beobachtet, nicht vendort

| Repo | Rolle |
|------|--------|
| https://github.com/yapb/yapb | Bot-Ideenquelle; später mit ZBot und weiteren vergleichen |
| https://github.com/dreamstalker/rehlds | ReGameDLL-Grundlage; nicht unsere Engine |
| `microsoft/vcpkg` | Windows-Package-Manager, nicht im Tree |

### VGUI2 Research References (Phase 3M)

Dokumentations- und Vergleichsquellen. **Vendort ≠ Produkt-Build ≠ Runtime-Abhängigkeit.** Research-Quelle darf bei Bedarf reproduzierbar in den Worktree vendort werden (Analyse/Port) — ohne sie als Runtime-Abhängigkeit oder stillschweigende Classic-Baseline zu übernehmen.

Prüfdatum aller Pins unten: **2026-09-01**, sofern nicht anders angegeben.

| Upstream | URL | Commit / Stand | Rolle | Konkrete Erkenntnis |
|----------|-----|----------------|-------|---------------------|
| Valve Developer Community — VGUI Documentation | https://developer.valvesoftware.com/wiki/VGUI_Documentation | Wiki, geprüft 2026-09-01; **oldid** wegen Bot-Schutz der History-API nicht abrufbar — vor nächstem Cite erneut History speichern | Hersteller-Doku: Panel-Hierarchie, Lifecycle, Scheme/Fonts, Loc, Proportionality, Build Mode (**Ctrl+Shift+Alt+B**) | Allgemeine VGUI2-Semantik. **Nicht** ungeprüft Source-only (`PANEL_CLIENTDLL`, `SourceScheme`, BaseViewport) übernehmen. Bei Widerspruch: Original-CS-1.6. |
| Understanding VGUI2 Resource Files | https://developer.valvesoftware.com/wiki/Understanding_VGUI2_Resource_Files | Wiki, geprüft 2026-09-01 (oldid s. o.) | `.res`-Struktur, Position/Size, Parent, AutoResize/Pinning | Resource-Semantik für Options-Seiten |
| VGUI2: Creating a panel | https://developer.valvesoftware.com/wiki/VGUI2:_Creating_a_panel | Wiki, geprüft 2026-09-01 (oldid s. o.) | Panel, EditablePanel, LoadControlSettings, SetScheme, SetProportional, Build Mode | Bestätigt Build-Mode-Shortcut |
| NextClient GameUI | Pin `client/` oben | funktional Options/BasePanel | `COptionsDialog` **545×406**, `SetTabWidth(84)` — NextClient-Layout; Funktion behalten, Optik nicht automatisch = Valve |
| Ref B / FuryBaM | `refs/b-cs16-goldsrc/` Pin oben | Menü-/VGUI; später Team/Buy | Steam-`vgui2` nicht als Runtime |
| kungfulon/fwgs-vgui2-support | https://github.com/kungfulon/fwgs-vgui2-support | `91868378f21ebb39aefd255db5ae6e21b74a4a3b` (2019-01-21, HEAD zum Prüfdatum) | historische Xash + Steam-`vgui2`-Forschung | Analyse only; deprecated zugunsten kungfulon/xash3d-fwgs; **nicht** Produktgrundlage |
| CKFDevPowered/CKF3Alpha | https://github.com/CKFDevPowered/CKF3Alpha | `4e1ee1bdb2aeebe3548eb57b104f2a9cf4dccf97` (2020-02-04) | klassische rekonstruierte GoldSrc-GameUI (BasePanel, GameMenu, Options, …) | `OptionsDialog.cpp`: `SetBounds(0,0,512,406)` + `SetTabWidth(84)` — Rekonstruktion; konvergent mit Golden-5971 **rendered** 512×406, nicht Binary-Beweis |
| Counter-Strike-16/OpenGoldSrc | https://github.com/Counter-Strike-16/OpenGoldSrc | `9f7bbee933a1ea337758d8e03dfdd2c0515bb41c` (2017-07-25) | GoldSrc-/GameUI-Rekonstruktion, VGUI2/BaseUI | `ogs/gameui/default/OptionsDialog.cpp`: ebenfalls **512×406** + `SetTabWidth(84)` |
| Golden 5971 Screenshot Artifact | `docs/research/golden-5971/` | SHA-256 `0f73bd7b45c3980a780c8abfe6f25e779c0893b8935723fff5697199c6f12cfd` | visuelle Classic-Referenz + screenshot-derived metrics | OptionsDialog **512×406** rendered (logisch 1366×768); HD/prop-State **unknown** |
| hzqst/MetaHookSv (VGUI2Extension) | https://github.com/hzqst/MetaHookSv | `bb5f7833cd6908638ab48c7315f6b0240c6caafb` (2026-08-30); Doku `docs/VGUI2Extension.md`, `include/Interface/VGUI/ISurface.h` | Scheme/Resource-Injection; HiDPI; HL25 Surface | `ISurface_HL25`: `GetHDProportionalBase`/`SetHDProportionalBase` (HL25-added). HiDPI-Modus: bewusst alle Panels proportional — **Vergleich**, nicht Classic-Ziel. Current Steam 2024 = HL25-era HD reference; **5971 proportional/HD = unknown** |
| Ref A | `refs/a-cs16-client/` Pin oben | Desktop-/Client-Erkenntnisse | kein Ref-A-mainui als Produkt |

Golden Visual vs Current Steam Resources / Metrics: `docs/PHASE3M-METRICS-DIAGNOSIS.md`.

Spielinhalte `valve/` / `cstrike/`: externe Runtime-Datenquelle. Steam CS 1.6 (AppID 10) wird gelesen, nie geschrieben, und nicht als RODIR benutzt. Materialisiert: `gamedata/` (`docs/GAMEDATA.md`). Nicht im Git.
Ref-A-`3rdparty/ReGameDLL_CS/`, Ref-A-YaPB, Ref-A-mainui: nicht die Produktquelle.

Weitere Repos hier eintragen, sobald daraus Wissen, Code oder Verhalten tatsächlich verwendet wird.

## Übernommene Upstream-Ports

Konkrete Fixes/Commits, nicht jede Idee.

| Quelle | Upstream-Commit/PR | Was übernommen | CS-Retro-Commit |
|--------|--------------------|----------------|-----------------|
| — | ncl-hl1-source-sdk | 64-Bit-VGUI-Patches: `VPANEL`→`uintptr_t`, Bitfield-Swap, mempool/threadtools | 2026-09-01 |

## NextClient-Vendor nach Phase 2

Lokaler Schnitt gegenüber dem Pin: `steam_api_proxy/` weg, 8684-Provider weg, `MatchmakingSteamComp` weg. Ein späterer NextClient-Port muss das wiederholen oder bewusst lassen.

GameDLL-Produkt-Build: `cmake/CsretroGameDll.cmake`. Nicht Ref A. Details: `server/game/ATTRIBUTION.md`.
