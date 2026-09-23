# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1–PX4B / #7: `docs/research/px1-primext.md`.

## Stand 2026-09-21 — Classic-CS-1.6-Movement-Restore

Aktiver PM-Pfad: `client/body/pm_shared` (Prediction) und
`server/game/regamedll/pm_shared` (Autoritaet). `client_mini` aus NextClient
ist in dieser Xash-Runtime kein alternativer PM-Pfad: `HUD_PlayerMove` wird
weitergeleitet; seine relevanten Unterschiede sind View-/Weapon-Feel.

| Mechanik | Current CS Retro | Velaron | NextClient | Entscheidung |
|---|---|---|---|---|
| Boden-, Luftbeschleunigung und Friction | Kernformeln gleich | Referenz | keine alternative PM | beibehalten |
| Post-Jump-Recovery `fuser2` | 450 ms, verkuerzte Drossel | 1315.789429 ms | keine PM-Alternative | auf 1315.789429 ms in Client und GameDLL restauriert |
| Auto-/Extended-Bhop | ReGame-CVars vorhanden, Defaults 0 | klassischer Ablauf | keine PM-Alternative | Defaults 0 belassen; bei Runtime-Test kontrollieren |
| ReGame-Stamina | optional, Default 0 | nicht aktiv | keine PM-Alternative | 0 belassen, da keine passende Client-Prediction existiert |
| Weapon lag | `cl_weaponlag 0` | klassisches View-Feel | aktiver Feel-Pfad | Default 1, persistente Mouse-Option hinzugefuegt |
| Bob | `cl_bobstyle 0/1` | Style 1 = alter Sway | Style 0 ohne zusaetzlichen Winkelsway | unabhaengige Feel-Option, kein Physics-Fix |

Die konkrete Fehlursache fuer den zu leichten Bunnyhop war die beiderseitige
Verkuerzung der im WalkMove wirksamen Recovery von 1315.789429 auf 450 ms. Der
Restore ist absichtlich lockstep: beide PM-Implementierungen setzen jetzt den
Velaron-Wert, und `scripts/movement-contract-gate.sh` erzwingt ihn.

Validiert: `./scripts/movement-contract-gate.sh`, `csretro_client`,
`csretro_menu` und `csretro_gamedll` bauen erfolgreich. Die GameDLL erzeugt
weiterhin vorhandene, nicht durch diesen Slice verursachte Compilerwarnungen.

Offener Gameplay-DoD, nicht aus Source-Paritaet schliessen: frischer
`./scripts/play.sh`-Lauf mit Standstill-Beschleunigung, Volltempo-Reversal,
A/D-Wechsel, Strafe mit Mausdrehung, Einzel-/Kettenjumps, Duck/Unduck und
Duck-Jump. Dabei serverseitig kontrollieren:
`sv_autobunnyhopping 0`, `sv_enablebunnyhopping 0`,
`mp_stamina_restore_rate 0`, `mp_unduck_method 0`, `mp_jump_height 45`.

## Stand 2026-09-20 — PX6A.2 Stabilization: #12 FAIL, #14+#15 OPEN

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
**#12 OPEN — Mode 2 nicht freigabefähig** (World sichtbar ≠ spielbar).
**#14 OPEN** Mode-2 Performance-Regression.
**#15 OPEN** Mode-2 Viewmodel-Parity falsch.
#1 #2 #3 #13 unverändert OPEN.
**Release: NONE.** Keine Promotion. Kein Default→2. Kein Fallback-Kill.

```
r_csretro_renderer 0 → normales Spielen (empfohlen)
r_csretro_renderer 1 → Diagnose offscreen + sichtbarer Xash
r_csretro_renderer 2 → Takeover — World ok-ish, Perf+#15 blockieren
```

### Befund (Manual Play)
- Fortschritt: World nicht mehr Clear-Color; Sky/HUD sichtbar.
- Blocker A (#14): extremes Laggen — Cert-Hotpath (SampleProof/`glReadPixels`,
  double World-Redraws, TMU-Cleanup×N, CheckGL-Spam).
- Blocker B (#15): Viewmodel dunkel/falsch — Xash `glState.activeTMU` nach
  Multitexture-World nicht synced; Studio band Skin auf falsche Unit.

### In-Tree Fixes (noch nicht DoD-geschlossen)
- SampleProof no-op wenn `r_csretro_offscreen_dump 0` (Play-Default).
- Random-tiled double World-Redraw nur bei dump=1.
- `CSRETRO_Backend_SyncTextureUnits()` vor Studio/Player/Viewmodel —
  **CleanUp(1) nicht CleanUp(0)** (0 disablte TMU0 → dunkles VM).
- PrepareImmediateDraw ohne CleanupTextures×20/Frame.
- CheckGL preflight/world/begin nur bei dump=1; Present behält Check.
- ShadeModel SMOOTH + TexGen aus.
- Play-Pfad: kein per-frame sky/viewmodel `Con_Printf` bei CRC=0
  (Log `play-20260920-202645`: ~1600 Spam-Zeilen in ~13s Mode 2).
- Sprite dump nur bei `r_csretro_offscreen_dump 1`; TempEnt-Bug
  (s_dump_left nie dekrementiert → Dauer-Spam) behoben.

### DoD zum Schließen (binär — nur interaktiver `play.sh`)

**Kein Schließen aus Codeanalyse oder altem Log.** Logs müssen die gebaute
Client-SO der Fixes tragen. `r_csretro_offscreen_dump` muss `0` sein.

**Optik-Erwartung:** Mode 2 soll sich **wie Mode 0 anfühlen/aussehen**
(Parität). Kein „schönerer Renderer“, kein Bloom/Shadowmap-Delta erwarten.

**#14 bleibt OPEN**, wenn Lag vs Mode 0 noch deutlich spürbar ist **oder**
der Runtime-Log permanent `sprite dump` / `pixelproof` spammt.
**#15 bleibt OPEN**, wenn eine einzige Waffe sichtbar abweicht.
Erst wenn **beides** im normalen `play.sh`-Lauf sauber ist → #14+#15 schließen;
danach #12 **neu bewerten**, nicht automatisch schließen.

#### Pflicht-Durchlauf (A/B Parität — stumpf)
1. `./scripts/build-client.sh && ./scripts/play.sh` · `de_dust`
2. Feste Kameraposition, **F9** Glock in der Hand
3. **F5** (Mode 0) → **F8** Screenshot
4. **Nicht bewegen:** **F7** (Mode 2) → **F8** Screenshot
5. Nur noch **F10** AK + **F11** Knife ebenso (0 dann 2, gleiche Pose)
6. Kurz HE / Smoke / Flash prüfen (liegen nach F11 bereit)
7. Ablage: neuer `play-*.log` + die 0/2-Paare
   (`CSRETRO_TEST renderer=` Zeilen im Log = Bestätigung der Umschaltung)

**Nur darauf schauen (nicht Schattenqualität/Bloom/„moderner“):**
- Waffe gleich hell/dunkel? gleiche Texturfarben?
- gleiche Größe/Position/Handedness? Knife gleiche Seite?
- keine schwarzen Flächen / fehlende Polygone?
- Animation normal?
- #14: Mode 2 fühlt sich ≈ so flüssig wie Mode 0?

**#15 bestätigt** (Manual 2026-09-21): Mode-2 Viewmodel falsch.
Saubere Paare: `de_dust_shot0004/0005` und `0006/0007` (F5→F8, F7→F8).
Alte 0000–0003 gelöscht.

**Freeze-Time-Bug (fix):** Create Game schrieb `listenserver.cfg`, hat sie aber
nie `exec`'t → GameDLL-Default `mp_freezetime 15`. Fix in `profile.cpp`
(`exec listenserver.cfg` vor `map`). Menü neu gebaut — nächster `play.sh`.

## Stand 2026-09-21 — Warteschlange (nicht nur HE/GL)

**Zielbild unverändert:** PrimeXT = selektive Ideenquelle. Mode 2 zuerst
**Parität zu Mode 0**, dann Features die Mode 0 nicht hat. Modes F5/F6/F7 =
Dev-Werkzeug, Endzustand nur Mode 2.

### Queue (Reihenfolge)

| Prio | Item | Stand |
|------|------|--------|
| 1 | **#15** Viewmodel-Parität Mode 2≈0 | Fix gebaut; DoD = Manual A/B F9–F11 |
| 2 | **#14** Mode-2 Perf | Manual: F6-Lag weg (0.1.54). DoD schließen wenn F7≈F5 Feel |
| 3 | **#12** Mode 2 freigabefähig? | Neu bewerten erst nach #14+#15 DoD |
| 4 | HE/Smoke/Flash Parität | Optisch ok unter F7; Rest-`0x500` Härten in 0.1.54 — **kein weiteres Tunneln** |
| 5 | PrimeXT-Selektiv (Studio/Licht/Sprites …) | **Erst nach** spielbarem Mode-2-DoD |
| — | #1 #2 #3 #13 | Unverändert OPEN, parallel möglich (#13) |

**Nicht:** Endlos an einem Effekt (HE/GL) bleiben, während die Queue steht.
Smoke-Dichte / „schöner als 1.6“ = später, nach Parität.

### Was ich jetzt tue
1. #15 DoD — A/B Glock/AK/Knife (F5↔F7, F8), Handoff schließen wenn ok
2. #14 DoD — wenn Feel ≈ Mode 0: schließen
3. #12 neu bewerten
4. Dann nächster PrimeXT-/Renderer-Slice aus Research (nicht Mikro-GL)

Default bleibt `r_csretro_renderer 0` bis #12 bewusst freigegeben.


### Deine Shots (behalten)
| Shot | Bedeutung |
|------|-----------|
| 0000 / 0001 | Glock Mode0 vs Mode2 — #15 |
| 0002 | AK Mode2 — gleiches VM-Problem |
| 0008–0017 | HE in Smoke Mode2 — Sprite/Partikel-Fehler |

### Was du getestet hast — bestätigt
- Alle Waffen-VMs Mode 2 falsch (nicht nur Glock)
- HE in Smoke: blockige/weiße Artefakte, Sortierung, kaputte Explosionssprites
- F8-Spam für kurze Blitze = sinnvoll

## Stand 2026-09-21 — Manual A/B + Freeze-Fix

**Shots (aktuell):** `de_dust_shot0004`/`0005` und `0006`/`0007`
(Log `play-20260921-091625`: F9→F5→F8→F7→F8, zweimal). 0000–0003 gelöscht.

**#15 OPEN bestätigt:** Mode 2 Viewmodel = schwarze Dreiecks-Artefakte;
World ≈ Mode 0. **#14** ohne Dump noch offen.

**Freeze 15s — Root Cause:** `Profile_Start` schrieb `listenserver.cfg`,
`exec`'te sie aber nicht vor `map` → GameDLL-Default 15. Fix + Menü-Rebuild.
Nächster Start: `./scripts/play.sh` (frisch). Freezetime 0 → ReGameDLL +2s Intro
( praktisch sofort beweglich, nicht 15).

## Stand 2026-09-20 — PX6A.2: Mode-2 Lag + Viewmodel (#12 OPEN)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
Issue #7/#9/#10/#11 geschlossen. **#12 OPEN** (Mode 2 noch nicht spielbar freigegeben).
#1 #2 #3 unverändert OPEN. **#13** Console Clipboard separat OPEN.
**Release: NONE.** Keine Promotion.

```
r_csretro_renderer 0 → normales Spielen (empfohlen)
r_csretro_renderer 1 → Diagnose offscreen + sichtbarer Xash
r_csretro_renderer 2 → Takeover — Recovery in Arbeit
```

World nach Cull/`GL_FRONT` wieder sichtbar. Offene Mode-2-Bugs:
- Lag: jedes Frame viele volle FBO-`glReadPixels` (SampleProof) → jetzt hinter
  `r_csretro_offscreen_dump` (Probes setzen 1, Play-Pfad 0).
- Viewmodel dunkel/falsch: `PrepareImmediateDraw` stellte TMU per Raw-GL um,
  Xash-`glState.activeTMU` blieb auf Lightmap-Unit → Studio-Bind falsch;
  plus `ShadeModel(GL_SMOOTH)`, TexGen aus.

Bitte: `./scripts/build-client.sh && ./scripts/play.sh` → `r_csretro_renderer 2`.

**Nicht:** Default→2, Mode-1 Promotion, Fallback entfernen, Release/Tag.

**Nächster Schritt:** Mode-2 spielbar (FPS + korrektes Viewmodel) → dann #12 Close erwägen.
#13 parallel möglich. #1/#2/#3 nicht schließen.

## Stand 2026-09-20 — PX6A.2 Recovery: Mode-2 World kaputt (#12 OPEN)

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
Issue #7/#9/#10/#11 geschlossen. **#12 wieder OPEN** (Manual-Repro de_dust).
#1 #2 #3 unverändert OPEN. **#13** Console Clipboard separat OPEN.
**Release: NONE.** Keine Promotion.

```
r_csretro_renderer 0 → normales Spielen (empfohlen)
r_csretro_renderer 1 → Diagnose offscreen + sichtbarer Xash
r_csretro_renderer 2 → Takeover — derzeit World = Clear-Color (10,10,31)
```

Manual-Repro (`b266384`, `./scripts/play.sh`, Mode 2): Sky/HUD/Viewmodel ok,
World = Backend-Clear. PX6A.1 Cert bleibt historischer Lauf, **nicht** Produktfreigabe.

Recovery-Stand: `SaveState`/Water/Decal TexEnv via `glGetTexEnviv`; Cull `GL_FRONT`
wie Xash `R_SetupGL` (war `GL_BACK` → World weggecullt); GL-Fence; Fault-Latch
gilt ein Xash-Frame, dann Retry. `px6a-takeover-probe` PASS ohne `GL_INVALID_ENUM`.

Bitte manuell: `./scripts/build-client.sh && ./scripts/play.sh` → `r_csretro_renderer 2`.

**Nicht:** Default→2, Mode-1 Promotion, Fallback entfernen, Release/Tag.

**Nächster Schritt:** Mode-2 World visuell spielbar → dann erst #12 wieder Close erwägen.
#13 parallel möglich. #1/#2/#3 nicht schließen.

## Stand 2026-09-20 — PX6A.1 Visual Cert PASS; #12 CLOSED

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
Issue #7/#9/#10/#11/#12 geschlossen. #1 #2 #3 unverändert OPEN.
**Release: NONE.** Unreleased belassen. Keine Promotion.

```
r_csretro_renderer 0 → Xash only, GL_RenderFrame = 0
r_csretro_renderer 1 → CS-Retro 512² offscreen + sichtbarer Xash, return 0
r_csretro_renderer 2 → CS-Retro visible takeover (PX6A, DoD VERIFIED)
```

Zertifiziert: Implementation `c5f8055`, Docs `800bd1b`.
Runner: `./scripts/px6a-visual-cert.sh` → `build/px6a-cert-shots/` (nicht committen).
`lost_eligible_event_frames=0`, `fog_pre=1 fog_post=1`, Fallbacks Preview/Overview/Ripple,
Postcommit-Fault-Latch, Mapchange, vid_setmode FBO==viewport, Movement Gate PASS.

**Nicht ohne neue Freigabe:** Mode 1 = visible custom; Default → 2; Xash-Fallback entfernen; Release.

**Nächster Schritt:** Promotion-Slice nur nach Freigabe. #1 #2 #3 nicht schließen.

## Stand 2026-09-20 — PX6A Mode-2 Takeover Gate; #12 OPEN

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
Issue #7/#9/#10/#11 geschlossen. #1 #2 #3 unverändert OPEN.
Issue #12 OPEN bis vollständiges DoD / visuelle Zertifizierung.

```
r_csretro_renderer 0 → Xash only, GL_RenderFrame = 0
r_csretro_renderer 1 → CS-Retro 512² offscreen + sichtbarer Xash, return 0
r_csretro_renderer 2 → CS-Retro visible takeover candidate (PX6A)
```

**Mode-2 Vertrag:** Pre-commit Eligibility → FBO/Present-Preflight → `PrepareCustomFrame`
(frametime, framecount++, PushDlights, Vis consume, PlayerLight) → Draw (EFX/Triangles
owned advance) → Present/Blit → `FinalizeCustomFrame` → `return 1`.
Nach Commit kein Xash-Fallback im selben Frame. Default bleibt 0. Mode 1 unverändert.

```
REF_API_VERSION 25: PrepareCustomFrame / FinalizeCustomFrame /
  CustomFrameFogPre / CustomFrameFogPost / CustomFrameExtraUpdate
Takeover-FBO = viewport-sized (nicht 512)
Present = glBlitFramebuffer
Overview / Cubemap / Preview / r_ripple≠0 / Alias → return 0 pre-commit
```

Probe: `./scripts/px6a-takeover-probe.sh` PASS.
Mode-1 Smoke PASS (return 0, kein Takeover).
Log: `build/run-gate/px6a/merged-px6a.log`.
vid_setmode: FBO folgt Viewport (Gamescope clampte 1024×768 → 640×480; recreate VERIFIED).

**Nicht:** Mode 1 = visible custom; Default auf 2; Xash-Fallback entfernen; #12 schließen ohne DoD.

**Nächster Schritt:** visuelle Zertifizierung / DoD-Rest für #12; danach Freigabe für Promotion.
#1 #2 #3 nicht schließen.

## Stand 2026-09-20 — PX5.1 Sky + Trans; #11 CLOSED

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
`GL_RenderFrame` bleibt 0. Issue #7/#9/#10/#11 geschlossen. #1 #2 #3 unverändert OPEN.
`return 1` nicht starten.

```
r_csretro_renderer 0 → Xash sichtbar
r_csretro_renderer 1 → CS-Retro offscreen + sichtbarer Xash-Fallback
GL_RenderFrame        → immer 0
```

**Vertrag:** `PrepareCurrentFrameVis` ruft `R_PrepareViewState` → `R_SetupGL(false)` → `R_MarkLeaves`.
`R_SetupGL(false)` setzt nur `RI.worldviewMatrix` / `projectionMatrix` / `worldviewProjectionMatrix` / `farClip`.
Kein sichtbarer GL-State. `R_RenderScene` reused Prepare+MarkLeaves und führt weiter `R_SetupGL(true)` aus.
Tail-API `GetEntityRenderInfoReadOnly` (`REF_API_VERSION` 24): effective rendermode, opaque/renderfx, read-only FxBlend, bbox-center distance.

```
current-frame Frustum/PVS/Mod_GetCurrentVis: VERIFIED
world PVS+frustum selection: VERIFIED
sky: farclip>0 sides>0 candidates/drawn>0 nonempty>0 isolated CRC differ=1 visual PASS
trans: Xash resolver, Player included, duplicate_scene_draws=0, comparator proof PASS
backface: CONFIRMED code parity / runtime reject NOT REPRODUCIBLE
efrag: implemented safety contract / runtime NOT REPRODUCIBLE
alias: KIND_OTHER, visible Xash R_DrawAliasModel / runtime NOT REPRODUCIBLE
overview: vis overview=1 prepared=1 während dev_overview
fog: visible Xash R_DrawFog/R_CheckFog
ripple: visible Xash R_AnimateRipples
```

Probe: `./scripts/px5.1-sky-trans-probe.sh` PASS. `./scripts/px5-vis-probe.sh` PASS.
Shots `build/px5-vis-shots/` (nicht committed).
Pflicht-Gates PASS: movement, px7-tri-overview, sprite, brush A/B/C/D, random-tiled, px4b2, px4c1, px4c2.
Issue: https://github.com/benjarogit/csretro/issues/11 CLOSED

**Nächster Schritt:** nur nach neuer Freigabe. `return 1` nicht starten.
#1 #2 #3 nicht schließen. #7 #9 #10 #11 nicht wieder öffnen.

## Stand 2026-09-20 — PX5 Vis-Seam hinter return 0; #11 bleibt OPEN

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
`GL_RenderFrame` bleibt 0. Issue #7/#9/#10 geschlossen. Issue #11 offen bis komplettes DoD.
#1 #2 #3 unverändert OPEN. `return 1` nicht starten.

```
r_csretro_renderer 0 → Xash sichtbar
r_csretro_renderer 1 → CS-Retro offscreen + sichtbarer Xash-Fallback
GL_RenderFrame        → immer 0
```

**Vertrag:** eine Tail-API `PrepareCurrentFrameVis` (`REF_API_VERSION` 23). Xash berechnet
Frustum/Viewleaf/PVS (`R_SetupFrustum` / `R_FindViewLeaf` / `R_MarkLeaves`). Client kopiert
PVS in einen eigenen Frame-Puffer. `Mod_GetCurrentVis` gibt genau diesen Puffer zurück.
`R_RenderScene` nach return 0 reused denselben Zustand (kein zweites MarkLeaves).
World-Draw nutzt eine Surface-Maske auf `CSRETRO_BspMesh`. Keine zweite PVS-Formel.

```
current-frame Frustum/PVS/Mod_GetCurrentVis: VERIFIED (pvs_match=1, reused=1)
world PVS+frustum selection: VERIFIED (drawn≠all, selection_hash ändert sich)
efrag: implemented safety contract / runtime NOT REPRODUCIBLE (count=0 auf aztec/torn/assault/dust)
sky: implemented (candidates/drawn>0); FBO pixelproof differ=0 nonempty=0 — verification pending
trans order: implemented (eine Liste, Brush/Sprite/Studio-Dispatch); overlapping Teilfälle N/R
fog: explicit safe contract — visible Xash R_DrawFog/R_CheckFog; client triangle fog = draw-only
ripple: explicit safe contract — visible Xash R_AnimateRipples
alias: NOT REPRODUCIBLE WITH CURRENT GAME CONTENT
```

Probe: `./scripts/px5-vis-probe.sh` PASS. Shots `build/px5-vis-shots/` (nicht committed).
Pflicht-Gates PASS: movement, px7-tri-overview, sprite, brush A/B/C/D, random-tiled, px4b2, px4c1, px4c2.
Issue: https://github.com/benjarogit/csretro/issues/11

**Nächster Schritt:** nur nach neuer Freigabe. `return 1` nicht starten.
#1 #2 #3 nicht schließen. #7 #9 #10 nicht wieder öffnen. #11 nicht schließen ohne DoD.

## Stand 2026-09-20 — PX4C.2 Event-Ownership VERIFIED; #10 CLOSED

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
`GL_RenderFrame` bleibt 0. Issue #7 geschlossen. Issue #9 geschlossen. Issue #10 geschlossen.
Bekannter Blocker vor erstem sichtbaren Takeover: **Vis**. Vis nicht gestartet. Kein return 1.

```
Viewmodel studio body: VERIFIED offscreen (PX4C.1 unverändert)
Event ownership: VERIFIED exactly-one (PX4C.2)
  renderer 0: Xash default first claim, event_impl_runs=1, client_claims=0
  renderer 1: Client claim first, event_impl_runs=1, default_duplicate_skips=1
  double-call: first=1, second=-1, attach B==C
Attachments: exactly-once VERIFIED
Muzzle / ELight: once VERIFIED
Studio deliveries: VERIFIED (pass once; mehrere Events im Modell erlaubt)
Molotov: event_wick_capture>0, offscreen_body_wick_capture=0, visible_body_wick_capture=legacy Xash
EV_UpdateMolotovHeld: once per visible frame; source=captured NOT REPRODUCIBLE
  (HUD_GetWeapon=9 / SMOKE vs WEAPON_MOLOTOV=32 — bestehende Held-Semantik, nicht #2)
GL_RenderFrame: always 0
```

**Produktpfad Events:** eine Impl `R_RunViewmodelEventsImpl`. Wrapper `R_RunViewmodelEventsOnce` besitzt das Frame-Claim-Gate. Client (`r_csretro_renderer 1`) claimt früh in `CSRETRO_Renderer_Frame` vor BeginOffscreen; CurrentEntity/Model save/restore. Xash-Default nach `GL_RenderFrame` ruft dieselbe Once-Funktion und wird zum No-Op. Eligibility bleibt Xash. LIVE Viewmodel (keine Snapshot-Isolation). Body weiter Snapshot + `STUDIO_RENDER` only.

**GetViewInfo / Vis:** `R_SetupRefParams` kopiert `rvp` vor dem Callback. `RI.vforward/vright/vup` kommen erst in `R_SetupFrustum` innerhalb von `R_RenderScene`. Events laufen wie bisher vor Vis. Vor return 1 muss der Vis-Slice Frustum/GL/MarkLeaves setzen. PX4C.2 ruft diese Helper nicht.

Probe: `./scripts/px4c2-viewmodel-events-probe.sh` PASS. PX4C.1 `./scripts/px4c1-viewmodel-body-probe.sh` PASS. Movement-Gate PASS. Shots `build/px4c2-viewmodel-shots/` (nicht committed).

**Nächster Schritt:** nur nach neuer Freigabe. Vis / `return 1` nicht starten.
#1 #2 #3 #7 #9 nicht schließen. #7 #9 #10 nicht wieder öffnen ohne konkretes Bug-Issue.

## Stand 2026-09-20 — PX4C.1 Viewmodel BODY VERIFIED; #10 bleibt OPEN

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
`GL_RenderFrame` bleibt 0. Issue #7 geschlossen. Issue #9 geschlossen. Issue #10 offen.

```
Viewmodel studio body: VERIFIED offscreen
Eligibility: VERIFIED
DepthRange: VERIFIED (before 0/1, during 0/0.3, restore=1)
Handedness: VERIFIED (XOR, cl_righthand mutate=0; Knife special_flip)
Offscreen STUDIO_EVENTS: 0 VERIFIED
Additional Wick capture: 0 VERIFIED (attempts>0, captures=0)
Visible Xash events/wick: still owner
Event ownership for return 1: PENDING
```

Vor return 1 offen: **#10 Event-Ownership**, Vis. PX4C.2 nur Research, kein Code.
PX4C.1 = nur Viewmodel BODY unter Strategie C. Kein Event-Handoff, kein Vis, kein return 1.

**Produktpfad Viewmodel:** `GetViewModel()` → lokale `cl_entity_t`-Kopie → `R_SetCurrentEntity(&snapshot)` → `StudioDrawViewmodelOffscreen(STUDIO_RENDER)` → restore. Eligibility = Xash-World-Pass. DepthRange save / 0.3-span / restore. GSMR-Knochen-Cache nach Player/FOLLOW bewusst überschrieben; sichtbares Xash setzt den Studio-Kontext neu. Kein persistenter Bone-Pointer.

Probe: `./scripts/px4c1-viewmodel-body-probe.sh` PASS. Movement-Gate PASS. Mapfolge aztec/torn/assault/dust + `vid_setmode`. Shots `build/px4c1-viewmodel-shots/` (nicht committed). Shield implemented / Stock-CS **NOT REPRODUCIBLE**. Alias nicht im Produktcontent.

**Nächster Schritt:** nur nach neuer Freigabe. Vis / Event-Handoff / `return 1` nicht starten.
#1 #2 #3 #7 #9 nicht schließen. #10 nicht schließen. #7 #9 nicht wieder öffnen ohne konkretes Bug-Issue.

## Stand 2026-09-20 — PX4B.3 Local Eligibility + Player Shadows VERIFIED; #9 CLOSED

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
`GL_RenderFrame` bleibt 0. Issue #7 geschlossen. Issue #9 geschlossen.

```
Remote Player isolated B: VERIFIED (unverändert)
Local Xash eligibility: CL_IsThirdPerson() || index != rvp->viewentity
  First-person hidden VERIFIED (mirrored=1, hidden_viewentity>0, local_drawn=0, remotes drawn)
  Third-person visible VERIFIED (eligible>0, local_drawn>0, pixel CRC differ, live mutate=0)
  Spectator CHASE VERIFIED (OBS_CHASE_FREE, user2==local → CL_IsThirdPerson=1, local eligible)
  Spectator IN_EYE: Xash-Regel implementiert; Death-Cam spec_mode 4 ließ iuser1=2 (CHASE_FREE)
Player Shadows: shared StudioDrawPlayerShadow (Bip01 Spine3 → StudioDrawShadow)
  Remote pixel VERIFIED, local thirdperson pixel VERIFIED
  r_shadows 0 drawn=0 / r_shadows 1 drawn>0 VERIFIED
  shadow_side_draw=0 (kein B-Core Side-Draw)
Player-parent FOLLOW: implemented / NOT REPRODUCIBLE WITH CURRENT GAME CONTENT
m_bLocal=false, SetupClientAnimation inactive (sichtbares Xash/GSMR)
```

**Produktpfad Player:** `StudioDrawPlayer` (sichtbar, Live-`PlayerInfo`, `cl_shadows` → Helper) und `StudioDrawPlayerOffscreen` (lokale `player_info_t`, keine Events, kein Save/Restore, kein Shadow) teilen `_StudioDrawPlayer`. Nach erfolgreichem Offscreen-`STUDIO_RENDER` entscheidet CS Retro über `r_shadows` + `StudioDrawPlayerShadow`. Scene-Mirror = Quelle. Kein Live-Writeback. Local nur bei Xash-Eligibility.

Probe: `./scripts/px4b2-player-probe.sh` PASS. Movement-Gate PASS. Mapfolge aztec/torn/assault/dust + `vid_setmode`. Shots `build/px4b2-player-shots/` (nicht committed).

**Nächster Schritt:** nur nach neuer Freigabe. #10 Viewmodel / Vis / `return 1` nicht starten. #1 #2 #3 #7 #10 nicht anfassen. #7 #9 nicht wieder öffnen ohne konkretes Bug-Issue.

## Stand 2026-09-20 — PX4B.2 Player Studio Isolation (Variante B) VERIFIED; #9 bleibt OPEN

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
`GL_RenderFrame` bleibt 0. Issue #7 geschlossen. Issue #9 offen (Local Player deferred, Player-Shadows Takeover-Blocker, Player-parent FOLLOW N/R).

```
Remote Player isolated B: VERIFIED
  live player_info BEFORE == AFTER_OFFSCREEN, AFTER_VISIBLE != BEFORE
  live cl_entity mutate=0, snapshot copy changes
  STUDIO_EVENTS=0, shadow_side_draw=0
  pixels VERIFIED (aztec/torn/dust CRC differ=1)
  models: gsg9/sas/gign (CT) + arctic (T)
Local Player: consciously deferred (first-person world-studio exists when mirrored=1; SetupClientAnimation tot, m_bLocal=0)
Player-parent FOLLOW: implemented / NOT REPRODUCIBLE WITH CURRENT GAME CONTENT
Player shadows offscreen: not drawn (Takeover-Blocker, in #9 belassen)
```

**Produktpfad Player:** `StudioDrawPlayer` (sichtbar, Live-`PlayerInfo`) und `StudioDrawPlayerOffscreen` (lokale `player_info_t`, keine Events, kein Save/Restore, kein `r_shadows`) teilen `_StudioDrawPlayer` über `ResolvePlayerInfo`. Kein zweiter GSMR-Core. Scene-Mirror = Quelle. Kein Live-Writeback.

Probe: `./scripts/px4b2-player-probe.sh` PASS. Movement-Gate PASS. Mapfolge aztec/torn/assault/dust + `vid_setmode`. Shots `build/px4b2-player-shots/` (nicht committed).

**Nächster Schritt:** nur nach neuer Freigabe. #10 Viewmodel / Vis / `return 1` nicht starten. #9 nicht schließen, solange Local deferred oder Shadows für Takeover fehlen. #1 #2 #3 #7 #10 nicht anfassen.

## Stand 2026-09-20 — #7 Brush Special E (Random Tiled) VERIFIED; Issue CLOSED

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX3B: `bbe418d` / v0.1.12, Cert `8443d0d` / v0.1.13, Issue #5 geschlossen.
PX3C: `d3de222` / v0.1.14, Cert `9c7e7ae` / v0.1.15, Issue #6 geschlossen.
PX4A: `29f5a3c` / v0.1.16, Visual `b42359a` / v0.1.18, Issue #8 geschlossen.
PX4B.1: `344bf73` / v0.1.19. PX4B.2: `d531884` / v0.1.39. PX4B.3: dieser Stand / v0.1.40. Issue #9 geschlossen.
#7 Brush visuell: `59f4921` / v0.1.20, Cert `fba5b54` / v0.1.21.
#7 Engine-EFX Split: `41ee26f` / v0.1.22. Offscreen `c365cda` / v0.1.23. Docs `ba75509` / v0.1.24.
#7 Client-Triangles Split: `362f1dc` / v0.1.26. Offscreen `191651b` / v0.1.27. Docs `fede75a` / v0.1.28.
Spectator-Overview Cert: `e917629` / v0.1.29. Sprite Completion: `ec42664` / v0.1.30.
#7 Brush Special A: `b4e4bcd` / v0.1.31.
#7 Brush Special B: `251491a` / v0.1.32.
#7 Brush Special C: `42bb6df` / v0.1.33.
#7 Brush Special D: `0dd936b` / v0.1.35. Docs v0.1.36.
#7 Brush Special E: `9bc0194` / v0.1.37. Docs dieser Stand / v0.1.38.
`GL_RenderFrame` bleibt 0. Issue #7 geschlossen.

```
Brush entities: VERIFIED
Engine EFX: VERIFIED draw-only ownership
Client triangles: VERIFIED draw-only ownership
Sprite modes:
    SPR_ANGLED: implemented, runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT
    frame lerp: VERIFIED
    sprite lighting: VERIFIED (Xash sprite lighting / lightmap-style pass)
Brush special:
    texture animation: VERIFIED (cs_assault +0/+1 chain, tex_changed, pixel CRC differ, mesh/rebuild unchanged)
    conveyor: VERIFIED (de_torn func_conveyor, UV offset, geom unchanged)
    fullbright: implemented, runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT
    water/turb: qualified (capture+warp+wave implemented; brush water VERIFIED on de_torn; world opaque drawn, spawn FBO pixel N/R; transparent late / alpha_cap=0 N/R)
    decals: world VERIFIED (bullet-hole pixel CRC differ); brush/moving-brush implemented / runtime NOT REPRODUCIBLE; fallback implemented / N/R; transparent/stencil N/R; premultiplied implemented / runtime N/R
    dlights: world VERIFIED (HE TE_EXPLOSION, pixel CRC differ); brush/moving-brush implemented / runtime NOT REPRODUCIBLE; dynamic litwater implemented / runtime N/R
    random tiled: VERIFIED (de_aztec candidates=1955 resolved=1955 fallback=0, two variants, pixel CRC differ, selection zeitstabil)
```

**PrimeXT-Pin:** Tag `continious`, SHA `46fb05b41e58ed887718649e1720313baaac9a35` (2026-08-23).
Rolle: nur lesen. Clone ohne Submodule nach `refs/primext/` (gitignored).

**Produktpfad**
- Lifecycle / Frame: `client/render/render_core.cpp`
- GL/Offscreen: `client/render/render_backend.cpp` (FBO, Depth-Range, Polygon, Shade, Texenv, TMU, Poly-Offset, Fog Push/Pop, Restore)
- Shared BSP-Mesh: `client/render/render_bsp_mesh.cpp` — ein Builder für World und Brush-Cache; normale Batches + Water-Batches
- World/BSP: `client/render/render_world.cpp`
- Surface-DLights: `client/render/render_dlight.cpp` — Snapshot `GetDynamicLight`, transienter Atlas `*csretro_dlight_atlas`, keine live dlight/surface writes
- Brush offscreen: `client/render/render_brush.cpp` — Cache nach `model_t*`, GoldSrc-Transform, opaque + trans + Brush-Water
- Entity-Spiegel: `client/render/render_scene.cpp` — volle `cl_entity_t`-Kopie inkl. latched, kein Steal
- Sprite offscreen: `client/render/render_sprite.cpp` — eine Pipeline; SPR_ANGLED, Frame-Lerp und Xash sprite-lighting (LightAtPoint + Modulationspass, kein zweiter BSP-Atlas)
- Studio offscreen: `client/render/render_studio.cpp` — Non-Player `StudioDrawModel(STUDIO_RENDER)`; Remote + eligible Local `StudioDrawPlayerOffscreen` auf lokaler `player_info_t`; expliziter `StudioDrawPlayerShadow` nach Body wenn `r_shadows`; FOLLOW non-player `StudioDrawModel(0)`, player-parent `StudioDrawPlayerOffscreen(0)` ohne Shadow; Viewmodel-Events `RunViewmodelEventsOnce` (LIVE, vor Offscreen); Viewmodel-Body `StudioDrawViewmodelOffscreen(STUDIO_RENDER)` auf Snapshot nach Trans-EFX / vor Late Water; CurrentEntity save/restore
- Engine-EFX: `gRenderAPI.DrawEFX(rvp, trans, draw_only)` — CS-Retro-Extension am Ende von `render_api_t` (v37-Prefix eingefroren). Intern Ref `REF_API_VERSION` 22
- Surface-DLights: `gRenderAPI.BuildSurfaceLightmapReadOnly(...)` — Tail-Slot nach DrawEFX. Xash evaluiert die Lightmap read-only; CS Retro besitzt transienten Atlas und Draw. Kein `R_PushDlights` offscreen.
- Random tiled: `gRenderAPI.ResolveSurfaceTextureReadOnly(surface, entity_frame)` — Tail-Slot nach BuildSurfaceLightmapReadOnly. Eine Implementierung `R_ResolveSurfaceTexture`. Sichtbares `R_TextureAnimation` ist Wrapper. Keine Client-RNG, keine rtable-Export.
- Viewmodel-Events: `gRenderAPI.RunViewmodelEventsOnce()` — Tail-Slot nach ResolveSurfaceTextureReadOnly. Eine Impl `R_RunViewmodelEventsImpl`. Frame-Claim-Gate. Client und Xash-Default teilen denselben Wrapper.
- Client-Triangles: interne API `CSRETRO_ClientTriangles_*` in derselben `client_amd64.so` (kein neuer `render_api_t`-Slot). Xash-Exports `HUD_DrawNormalTriangles` / `HUD_DrawTransparentTriangles` bleiben Advance+Draw
- Brücke: `cdll_int.cpp` — `GL_RenderFrame` void + `return 0`; `HUD_AddEntity` spiegelt und behält Return
- Water-Alpha: `PARM_WATER_ALPHA` = Map-Capability 0/1. `PARM_WATER_ALPHA_VALUE` = IEEE-754-Bits der effective wateralpha (1.0 ohne Capability). `PARM_MAP_HAS_LITWATER` 0/1. Kein neuer Funktionsslot.

**CVar:** `r_csretro_renderer` 0 = Xash-only (Xash bleibt Event-Owner). 1 = Offscreen World+Brush+Sprites+Non-Player-Studio+Remote/eligible-Local-Player-Studio (B)+explizite Player-Shadows+FOLLOW+Viewmodel-Events (Claim)+Viewmodel-Body+draw-only EFX+draw-only Client-Triangles + sichtbarer Xash-Fallback. Keine eigene Viewmodel-CVar; `r_drawviewmodel` steuert Offscreen wie Xash.
Probes: `./scripts/px3b-offscreen-probe.sh`, `./scripts/px3c-offscreen-probe.sh`, `./scripts/px4a-offscreen-probe.sh`, `./scripts/px4b1-offscreen-probe.sh`, `./scripts/px4b2-player-probe.sh`, `./scripts/px4c1-viewmodel-body-probe.sh`, `./scripts/px4c2-viewmodel-events-probe.sh`, `./scripts/px7-brush-offscreen-probe.sh`, `./scripts/px7-efx-xash-gate.sh`, `./scripts/px7-efx-offscreen-probe.sh`, `./scripts/px7-tri-xash-gate.sh`, `./scripts/px7-tri-offscreen-probe.sh`, `./scripts/px7-tri-overview-cert.sh`, `./scripts/px7-sprite-completion-probe.sh`, `./scripts/px7-brush-special-a-probe.sh`, `./scripts/px7-brush-special-b-probe.sh`, `./scripts/px7-brush-special-c-probe.sh`, `./scripts/px7-brush-special-d-dlights-probe.sh`, `./scripts/px7-random-tiled-probe.sh`.
Visual: `./scripts/px3c-visual-cert.sh` → `build/px3c-cert-shots/`; `./scripts/px4a-visual-cert.sh` → `build/px4a-cert-shots/`; `./scripts/px7-brush-visual-cert.sh` → `build/px7-brush-cert-shots/` (nicht committed).

**Callbacks an:** `Mod_ProcessUserData`, `R_NewMap`, `GL_BuildLightmaps`, `R_ClearScene` (additiv, nur CS-Retro-Liste).
**Callbacks NULL:** `Mod_GetCurrentVis`, `R_ProcessEntData`, Studio-Decals, `CL_UpdateLatchedVars`.

**#7 Brush (VERIFIED 2026-09-20)**
- Shared mesh builder: BSP-Surfaces → gleiche Triangulation für World-Mesh und Brush-Cache. World-Mesh wird nicht von Brush überschrieben.
- Cache: `model_t*`, first/num modelsurfaces, verts, batches, textures, LM-pages, flags. Mapchange/unload gibt frei. Keine Surface-Pointer über Map-Leben.
- Nur `CSRETRO_KIND_BRUSH` aus `HUD_AddEntity`. Worldmodel nicht doppelt. Kein `GetEntityByIndex` als Renderliste.
- Transform: GoldSrc `R_RotateForEntity` / `R_TranslateForEntity` (origin, yaw, −pitch, roll).
- Opaque `kRenderNormal` + TransTexture/Color/Alpha/Add. Lightmaps über `PARM_TEX_LIGHTMAP`.
- Probe PASS + Visual `./scripts/px7-brush-visual-cert.sh` PASS: aztec Welt/Viewmodel/HUD; assault `func_door_rotating *11` index 19 origin 696 2236 48 sichtbar geschlossen → Kante → offen. Mapchange dust + `vid_setmode`. Movement-Gate PASS. `GL_RenderFrame` immer 0.
- Sonderflächen: Texture-Anim VERIFIED, Conveyor VERIFIED, Fullbright implemented / runtime N/R. Water/Turb: Capture + Warp + Wave im shared Mesh, Brush-Water VERIFIED (`de_torn` `*4`). World-opaque gezeichnet, Spawn-FBO-Pixel N/R. Transparent late / `alpha_cap=0` N/R. World-Decals VERIFIED. Brush-Decals implemented / N/R. World-DLights VERIFIED. Brush-DLights implemented / N/R. Random tiled VERIFIED.

**#7 Engine-EFX (VERIFIED draw-only ownership 2026-09-20)**
- Vertrag: `docs/research/px1-primext.md`. `GL_DrawParticles` bleibt unsicher (Advance). Produktpfad ist `DrawEFX(..., draw_only=1)`.
- Intern: Particles/Tracers ohne Think; Beams `copy=*live`; Dead-list nur im Advance.
- Xash-only Gate PASS (AK-Schuss particles+tracers, kein draw-only).
- Offscreen: draw-only `mutate=0` auf Trans-Pass; CRC differ=1; Xash `advanced=1` dieselbe Frame. Beams Stock-CS NOT REPRODUCIBLE.
- Reihenfolge: World → opaque Brush → Studio → FOLLOW → solid EFX draw-only → Normal Client-Triangles draw-only → trans Brush → Sprites → Transparent Client-Triangles draw-only → trans EFX draw-only → restore → return 0.
- Player: Remote B VERIFIED, Local Xash-Eligibility VERIFIED, Shadows VERIFIED (#9 CLOSED).

**#7 Client-Triangles (VERIFIED draw-only ownership 2026-09-20)**
- Interne API, kein neuer Engine-ABI-Slot. Sichtbar: Advance einmal + Draw. Offscreen: Draw-only, return 0.
- Overview: `AdvanceOverviewState` (gl_clear + CheckOverviewEntities) vs `DrawOverviewReadOnly` (Layer+Entities, kein Listen-Kill, kein CVar, HUD-PlayerPos nur sichtbar). FPS-Pfad Draw-only no-op.
- ParticleMan: `Advance` (Forces, Think, Die/Delete, `g_flOldTime`) vs `Render` (Frustum, lokale Visibility/Distanz/Sort, Draw). Live-Liste unsortiert. PVS-Cache nur sichtbar. FacePlayer-Winkel lokal.
- Environment.Update / EV_UpdateMolotovHeld: offscreen 0×, sichtbares Xash 1×. Fog: Render-State, Push/Pop im FBO.
- Xash-only Gate PASS. Offscreen: 150 Wetterpartikel `mutate=0`, CRC differ=1 pass=trans, Xash `advanced=1`.
- Spectator Map-Overview **VERIFIED**: `./scripts/px7-tri-overview-cert.sh` PASS. `user1=5` (OBS_MAP_FREE) Layer+Entities, Liste `count=37/37 mutate=0`, `gl_clear before=0 after=0 mutate=0`, Restore beim Verlassen. Shots `build/px7-tri-overview-cert-shots/` (nicht committed).
- `GL_RenderFrame` immer 0. Movement-Gate PASS. Mapchange dust + `vid_setmode`.

**#7 Sprite Completion (2026-09-20)**
- Eine Pipeline: `client/render/render_sprite.cpp`. Snapshot → lokale Latch-Kopie → Xash-Interpolation → Kopie verwerfen. Live `mutate=0`.
- `SPR_ANGLED` implementiert (8 Richtungsframes, Xash-Formel). Runtime: **NOT REPRODUCIBLE WITH CURRENT GAME CONTENT**.
- Frame-Lerp: `r_sprite_lerping`, SPR_ADDITIVE, zwei Pässe wenn old≠current, `* 11.0`. HE-Tent `candidates=1 drawn=1 old_ne_current=1`.
- Lighting: `Xash sprite lighting / lightmap-style pass` via `gEngfuncs.pTriAPI->LightAtPoint`, renderer-owned White-Texture, DepthFunc EQUAL + Restore. aztec SPR_ALPHTEST `candidates=14 lightatpoint=14 pass=14`.
- Probe: `./scripts/px7-sprite-completion-probe.sh` PASS. HE/Smoke/Flash, `r_sprite_lerping`/`r_sprite_lighting` 0/1, aztec→dust→assault, `vid_setmode`. Movement-Gate PASS.

**#7 Brush Special A (2026-09-20)**
- Ein Draw-Pfad: `client/render/render_bsp_mesh.cpp` für World und Brush. Animation/Conveyor/Fullbright zur Draw-Zeit, Mesh-Cache bleibt. Kein zweiter Brush-Renderer.
- Texture-Animation: Xash `R_TextureAnimation` (Alternate bei Snapshot-`frame != 0`, 10 fps bei `PARM_TEX_FLAGS`/`TF_QUAKEPAL` sonst 20). Batch-Key = Basis-Textur + Lightmap + Flags. **VERIFIED** `cs_assault` `+0/+1` chain `candidates=19 tex_changed=1` pixel `crc_a=c6874c8c crc_b=9e35736d`, verts/rebuild/geom unchanged.
- Alternate: implementiert. Runtime **NOT REPRODUCIBLE** (`alternate_used=0`, kein Entity-Frame ≠ 0).
- Random tiled (`-`): Special E. VERIFIED über gemeinsamen Xash-Resolver.
- Conveyor: UV-Offset nur beim Vertex-Output, `xr_texture_t.width`, Speed aus Snapshot-`rendercolor`. **VERIFIED** `de_torn` `func_conveyor` `candidates=20 uv_changed=1 geom_unchanged=1`.
- Fullbright: zweiter Pass ONE,ONE, DepthMask off, Fog Push/Pop. Runtime **NOT REPRODUCIBLE** (`fb_texturenum=0` auf aztec/torn/dust/assault).
- Water/Turb: Special B. Keine Decals, keine DLights. `PARM_TEX_LIGHTMAP` unverändert.
- Probe: `./scripts/px7-brush-special-a-probe.sh` PASS. aztec→torn→dust→assault, `vid_setmode`. Movement-Gate PASS. `GL_RenderFrame` immer 0.

**#7 Brush Special B (2026-09-20)**
- Shared `CSRETRO_BspMesh`: normale Batches + Water-Batches. Builder kopiert vorbereitete Xash-Turb-Polys, kein eigenes Subdivide, kein stiller Flat-Fan. aztec `turb_surfaces=12 turb_polys=76 turb_verts=444 skipped_no_polys=0`.
- UV-Warp + Vertex-Wave zur Draw-Zeit (`EmitWaterPolys` / `warpsin.h`). TextureAnimation geteilt. `fb_texturenum` auf Turb = Ripple-Handle, nicht Fullbright. `R_UploadRipples` nicht offscreen.
- World opaque: `wateralpha>=1` im World-Pass (`opaque=12 late=0 base=12`). Spawn-FBO `differ=0` (Water nicht im Blick). Transparent late: implementiert; Stock-CS `alpha_cap=0` auf aztec/torn/dust/assault → effective=1, Late **N/R**.
- Brush-Water im Entity-Pass. **VERIFIED** `de_torn` `*4` liquid rendermode=2 pixelproof `ea9a9ed2≠7573f906` pass=brush drawn=4. Aztec `func_water` `*14/*15/*77` captured.
- Water-Sides: ohne `EF_WATERSIDES` nicht gezeichnet (Xash). Stock-CS hat das Bit nicht. top water VERIFIED (brush), water sides **NOT REPRODUCIBLE**.
- Static litwater: Capture hält LM-UV/Page. `litwater=0` Stock-CS. DLights/Decals unangetastet. Cache/live mutate=0. GL restore=1.
- Probe: `./scripts/px7-brush-special-b-probe.sh` PASS. Special A weiter Anim/Conveyor PASS. vid_setmode + Movement-Gate PASS. `GL_RenderFrame` immer 0.

**#7 Brush Special C (2026-09-20)**
- Ein Draw-Pfad: `client/render/render_decal.cpp`. Xash bleibt Owner (creation/pool/linkage/lifetime). CS Retro: read-only `pdecals` → `polys`. Keine Decal-Pointer im BSP-Mesh.
- ABI: `xr_decal_t` 88 Byte, Offsets gegen `engine/common/com_model.h` (amd64). `TF_PREMULTIPLIED` über `PARM_TEX_FLAGS`, kein neuer Slot.
- World: Base → Decals → Fullbright. **VERIFIED** `de_aztec` AK-Löcher `before=31301f88 after=8fc0c012 differ=1` surfaces=6 decals=9 drawn=9. live mutate=0, gl_restore=1, stored_ptrs=0.
- Brush / moving door `*11`: implementiert. Runtime **NOT REPRODUCIBLE** (Tür-Schuss erzeugte keine `entityIndex`-Decals in der Probe).
- Fallback `R_DecalSetupVerts` nur auf lokaler Kopie: implementiert, Stock-CS `polys!=NULL` → **N/R**.
- Transparent/stencil: inventarisiert (`transparent_decals=0`). **NOT REPRODUCIBLE**. Kein künstliches FBO-Stencil.
- Premultiplied: Code-Pfad vorhanden, Runtime `premult=0` / std blend → **N/R**.
- Special A weiter Anim/Conveyor PASS. Water capture weiter. Map aztec→assault→torn→dust, `vid_setmode`, Movement-Gate PASS.
- Probe: `./scripts/px7-brush-special-c-probe.sh` PASS.

**#7 Brush Special D (2026-09-20)**
- Eine API: Xash besitzt Allocation/Decay/`die`/sichtbaren Dynamic-LM-Pass. CS Retro snapshotet `gRenderAPI.GetDynamicLight(i)` read-only. Kein `R_PushDlights` / `R_MarkLights` offscreen, kein live `dlightframe`/`dlightbits`, kein `tr.dlightTexture`, kein zweiter statischer BSP-LM-Atlas, kein Blob/Vertex-Light.
- Engine-Helper `BuildSurfaceLightmapReadOnly` (Tail nach DrawEFX, `REF_API_VERSION` 20). Dieselbe Mathematik wie sichtbares `R_BuildLightMap` / `R_AddDynamicLights`, lokale Bits/Origins. `Mod_SampleSizeForFace`. `dlight.dark` wird wie der klassische GL-Pfad ignoriert (additiv).
- Client: transienter Atlas `*csretro_dlight_atlas` (Shelf + `glTexSubImage2D`), nur `dynamic==1`. Mesh-Cache unverändert. `r_dynamic 0` → keine Patches.
- Inventur Stock-CS: AK-Muzzle ist `EF_MUZZLEFLASH` (ELight), kein Surface-DLight. Reale `cl_dlights`: HE `TE_EXPLOSION` (`TE_EXPLFLAG_NONE`).
- World **VERIFIED** `de_aztec` HE: `active_dlights=4` `affected_world_surfaces=48` `patches=48` pixel `87aaaff8≠a2026c1c` `dlight_mutate=0` `surface_mutate=0` `mesh_mutate=0`. Expiration stellt den statischen Engine-LM-Pfad wieder her.
- Brush / moving door: Transform (yaw, −pitch, roll) implementiert. Runtime **NOT REPRODUCIBLE** in der Probe (Tür-HE ohne `brush dlight affected>0`).
- Dynamic litwater: Pfad vorhanden, Stock-CS `litwater=0` → **N/R**. Ripple bleibt Xash.
- `r_dynamic 0` disables / `r_dynamic 1` enables. Visible Xash PASS. Mapchange aztec→assault→torn→dust + `vid_setmode` PASS. Movement-Gate PASS. `GL_RenderFrame` immer 0.
- Probe: `./scripts/px7-brush-special-d-dlights-probe.sh` PASS. PrimeXT-Shader-Lights nicht übernommen.

**#7 Brush Special E (2026-09-20, Issue CLOSED)**
- Eine Implementierung: Engine `R_ResolveSurfaceTexture(surface, entity_frame)` in `ref_context.c`. `R_TextureAnimation` ist Wrapper. Client-API `ResolveSurfaceTextureReadOnly` ruft denselben Resolver. `rtable` bleibt Ref-Init (`COM_RandomLong` + Seed zurück). Keine Client-RNG, keine zweite Tabelle.
- ABI: Tail nach `BuildSurfaceLightmapReadOnly`. `REF_API_VERSION` 21. v37-Prefix unverändert. Soft-Ref teilt denselben Resolver.
- Draw: per `CSRETRO_SurfaceSpan`, nicht per Batch. `+0/+1` bleibt der lokale Special-A-Pfad. Random Tile nur TMU0; DLight weiter TMU1.
- Fullbright vom resolved Frame. SURF_DRAWTURB mit `-` denselben Resolver. Alternate: entity.frame → alternate_anims → random (Xash-Reihenfolge).
- **VERIFIED** `de_aztec` `candidates=1955 resolved=1955 fallback=0` `distinct=2` `differs=926` `variant0=933 variant1=1022` pixel `921ad40d≠f9063657` `stable=1` hash `42335c1c`. Mesh/rebuild/geom after first frame unchanged.
- Mapchange aztec→torn→assault→dust `stale_indices=0`. `vid_setmode` auf dust: dieselbe `selection_hash`. Anim + Conveyor weiter PASS. Movement-Gate PASS. `GL_RenderFrame` immer 0.
- Probe: `./scripts/px7-random-tiled-probe.sh` PASS. Shots `build/px7-random-tiled-shots/` (nicht committed).

**PX4B.3** (dieser Stand)
- Local: Xash `viewentity`/`CL_IsThirdPerson()` in `CSRETRO_Studio_DrawPlayers(&scene, rvp)`. First-Person hidden VERIFIED. Third-Person Pixel VERIFIED. `m_bLocal` false, `SetupClientAnimation` inactive.
- Shadows: gemeinsamer Helper, explizit nach Body. Remote+Local Pixel VERIFIED. `r_shadows` 0/1 VERIFIED. Kein B-Core Side-Draw.
- PX4B.2 Remote B unverändert. Player-parent FOLLOW implemented / N/R.

**PX4B.2**
- Variante B isoliert: Live `player_info_t` einmal lesen → lokale Kopie → Offscreen mutiert nur die Kopie → verwerfen → sichtbares Xash advanced Live genau einmal.
- Remote Player VERIFIED. PX4B.1 Non-Player-FOLLOW unverändert, Stock-CS weiter N/R.

**Offen vor return 1**
- [#7](https://github.com/benjarogit/csretro/issues/7): **CLOSED**. Brush Special A/B/C/D/E complete. Qualifizierte N/R sind Content-Limits, kein offener Produkt-Unterpunkt.
- Player-Studio ([#9](https://github.com/benjarogit/csretro/issues/9)) — **CLOSED** (Remote B, Local Eligibility, Shadows VERIFIED; Player-parent FOLLOW implemented / runtime N/R)
- Viewmodel ([#10](https://github.com/benjarogit/csretro/issues/10)) — **CLOSED** (PX4C.1 Body VERIFIED, PX4C.2 Event-Ownership VERIFIED)
- Vis — bekannter Blocker vor return 1

**Nächster Schritt:** nur nach neuer Freigabe. Vis / `return 1` nicht starten. #1 #2 #3 #7 #9 nicht schließen. #7 #9 #10 nicht wieder öffnen ohne konkretes Bug-Issue.

**PX0 bleibt offen**
- #1 Movement Replay: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- #3 Erster M1 Granaten: https://github.com/benjarogit/csretro/issues/3
- Gate `./scripts/movement-contract-gate.sh` PASS. Gate ≠ Replay.

**Team / Klasse / Buy** — nach PX2, PX3B und PX3C visuell CONFIRMED.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**Play-Test-Regeln** (jeder `play.sh`-Start): `$16000`, `mp_buytime -1`,
`mp_freezetime 0`, `mp_buy_anywhere 1`, `mp_round_infinite 1`, `sv_cheats 1`.
Quelle: `scripts/play-test-rules.cfg` → `listenserver.cfg`, Create-Game-Defaults in
`data/ui-overrides/cstrike/settings.scr`.

**F-Tasten** (`scripts/play-test-client.cfg` → `csretro_play_test.cfg`, **F4** = Legende):
| Taste | Aktion |
|-------|--------|
| F1 | max money |
| F2 / F3 | god / noclip |
| F5 / F6 / F7 | renderer 0 / 1 / 2 |
| F8 | screenshot (A/B: Pose halten → F5→F8, F7→F8) |
| F9 / F10 / F11 | Glock / AK47 / Knife+Nades |
| KP− / KP+ / KP_Enter | FPS / dump OFF / dump ON (lag!) |
| F12 | Engine-Snapshot (unverändert) |

A/B-DoD ohne Konsole: F9 → Pose → **F5→F8** → **F7→F8** (dann F10/F11 ebenso).

**Kein `GL_RenderFrame → 1`.** Locks: Inferno/Zippo-Gameplay, CS2-Waffen, `pm_shared`, ImGui, PhysX, HDR/PBR, Entity-Steal, sichtbarer Custom-Frame, PrimeXT-Studio parallel.
