# Handoff

Lebender Arbeitsstand. Öffentliche Docs: `docs/status.de.md`, `docs/architecture.de.md`.
PX1–PX4B / #7: `docs/research/px1-primext.md`.

## Stand 2026-09-20 — #7 Brush Special E (Random Tiled) VERIFIED; Issue CLOSED

Verbindlich: eine CS-Retro-Codebasis. Xash = einzige Runtime. Eine `client_amd64.so`.
PX3B: `bbe418d` / v0.1.12, Cert `8443d0d` / v0.1.13, Issue #5 geschlossen.
PX3C: `d3de222` / v0.1.14, Cert `9c7e7ae` / v0.1.15, Issue #6 geschlossen.
PX4A: `29f5a3c` / v0.1.16, Visual `b42359a` / v0.1.18, Issue #8 geschlossen.
PX4B.1: `344bf73` / v0.1.19. Issue #9 offen (ruhend).
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
- Studio offscreen: `client/render/render_studio.cpp` — GSMR `STUDIO_RENDER` only, Snapshot, CurrentEntity save/restore; FOLLOW child nur bei Non-Player-Parent in der Mirror-Liste
- Engine-EFX: `gRenderAPI.DrawEFX(rvp, trans, draw_only)` — CS-Retro-Extension am Ende von `render_api_t` (v37-Prefix eingefroren). Intern Ref `REF_API_VERSION` 21
- Surface-DLights: `gRenderAPI.BuildSurfaceLightmapReadOnly(...)` — Tail-Slot nach DrawEFX. Xash evaluiert die Lightmap read-only; CS Retro besitzt transienten Atlas und Draw. Kein `R_PushDlights` offscreen.
- Random tiled: `gRenderAPI.ResolveSurfaceTextureReadOnly(surface, entity_frame)` — Tail-Slot nach BuildSurfaceLightmapReadOnly. Eine Implementierung `R_ResolveSurfaceTexture`. Sichtbares `R_TextureAnimation` ist Wrapper. Keine Client-RNG, keine rtable-Export.
- Client-Triangles: interne API `CSRETRO_ClientTriangles_*` in derselben `client_amd64.so` (kein neuer `render_api_t`-Slot). Xash-Exports `HUD_DrawNormalTriangles` / `HUD_DrawTransparentTriangles` bleiben Advance+Draw
- Brücke: `cdll_int.cpp` — `GL_RenderFrame` void + `return 0`; `HUD_AddEntity` spiegelt und behält Return
- Water-Alpha: `PARM_WATER_ALPHA` = Map-Capability 0/1. `PARM_WATER_ALPHA_VALUE` = IEEE-754-Bits der effective wateralpha (1.0 ohne Capability). `PARM_MAP_HAS_LITWATER` 0/1. Kein neuer Funktionsslot.

**CVar:** `r_csretro_renderer` 0 = Xash-only. 1 = Offscreen World+Brush+Sprites+Non-Player-Studio+FOLLOW+draw-only EFX+draw-only Client-Triangles + sichtbarer Xash-Fallback.
Probes: `./scripts/px3b-offscreen-probe.sh`, `./scripts/px3c-offscreen-probe.sh`, `./scripts/px4a-offscreen-probe.sh`, `./scripts/px4b1-offscreen-probe.sh`, `./scripts/px7-brush-offscreen-probe.sh`, `./scripts/px7-efx-xash-gate.sh`, `./scripts/px7-efx-offscreen-probe.sh`, `./scripts/px7-tri-xash-gate.sh`, `./scripts/px7-tri-offscreen-probe.sh`, `./scripts/px7-tri-overview-cert.sh`, `./scripts/px7-sprite-completion-probe.sh`, `./scripts/px7-brush-special-a-probe.sh`, `./scripts/px7-brush-special-b-probe.sh`, `./scripts/px7-brush-special-c-probe.sh`, `./scripts/px7-brush-special-d-dlights-probe.sh`, `./scripts/px7-random-tiled-probe.sh`.
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
- Player bleibt C.

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

**PX4B.1** (ruhend)
- FOLLOW parent graph implementiert. Player-parent deferred. Stock-CS: **NOT REPRODUCIBLE WITH CURRENT GAME CONTENT**.
- Player bleibt C. Issue #9 offen, ruht bis A/B-Beweis. Kein Player-Produktcode in diesem Slice.
- Read-only: Variante B kann `gait`/`player_info_t` nicht isolieren ohne GSMR- oder `PlayerInfo`-Änderung (`IEngineStudio.PlayerInfo()` ist Live-State).

**Offen vor return 1**
- [#7](https://github.com/benjarogit/csretro/issues/7): **CLOSED**. Brush Special A/B/C/D/E complete. Qualifizierte N/R (SPR_ANGLED, Fullbright, World-water special, Brush decals, premult/stencil, Brush DLight, dynamic litwater) sind Content-Limits, kein offener Produkt-Unterpunkt.
- Player-Studio ([#9](https://github.com/benjarogit/csretro/issues/9)) — Variante C, Blocker vor `return 1`
- Player-parent FOLLOW (Slice in #9, hängt an Player-Safety)
- Viewmodel ([#10](https://github.com/benjarogit/csretro/issues/10))
- Vis

**Nächster Schritt:** nur nach neuer Freigabe. Empfohlen: Player-Safety #9. Player / Viewmodel / Vis / `return 1` nicht starten. #9/#10/#1/#2/#3 nicht anfassen. #7 nicht wieder öffnen ohne konkretes Bug-Issue.

**PX0 bleibt offen**
- #1 Movement Replay: https://github.com/benjarogit/csretro/issues/1
- #2 Incendiary In-Game: https://github.com/benjarogit/csretro/issues/2
- #3 Erster M1 Granaten: https://github.com/benjarogit/csretro/issues/3
- Gate `./scripts/movement-contract-gate.sh` PASS. Gate ≠ Replay.

**Team / Klasse / Buy** — nach PX2, PX3B und PX3C visuell CONFIRMED.

**Start:** nur `./scripts/play.sh`. Root-`play.sh` nicht committen. Keine Zweitinstanz.
Build-Client: `./scripts/build-client.sh` → `build/client-cmake/client/client_amd64.so`.

**Kein `GL_RenderFrame → 1`.** Locks: Inferno/Zippo-Gameplay, CS2-Waffen, `pm_shared`, ImGui, PhysX, HDR/PBR, Entity-Steal, sichtbarer Custom-Frame, PrimeXT-Studio parallel.
