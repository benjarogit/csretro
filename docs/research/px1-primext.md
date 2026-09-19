# PX1 — PrimeXT Research

Stand: 2026-09-19. Gegen CS Retro `76ee12f` (v0.1.6). Kein Produktcode.
Status-Wörter: **CONFIRMED** (im Baum/Remote nachgeprüft), **INFERRED** (folgt aus Code, nicht runtime-geprüft), **UNKNOWN** (nicht belegt), **DEFERRED** (bewusst später).

Issues: [#1 Movement Replay](https://github.com/benjarogit/csretro/issues/1), [#2 Incendiary In-Game](https://github.com/benjarogit/csretro/issues/2), [#3 erster M1 Granaten](https://github.com/benjarogit/csretro/issues/3) bleiben offen. [#4 PX2-Brücke](https://github.com/benjarogit/csretro/issues/4) visuell verifiziert 2026-09-20. [#5 PX3B](https://github.com/benjarogit/csretro/issues/5) visuell zertifiziert 2026-09-20, geschlossen.

## Pin

| Feld | Wert | Status |
| --- | --- | --- |
| Upstream | [SNMetamorph/PrimeXT](https://github.com/SNMetamorph/PrimeXT) | CONFIRMED |
| Tag | `continious` (einziger Release-Tag, Pre-Release) | CONFIRMED |
| SHA | `46fb05b41e58ed887718649e1720313baaac9a35` | CONFIRMED |
| Datum | 2026-08-23 (`2026-08-24 00:07:29 +0400`) | CONFIRMED |
| Rolle | Vergleichsbaum, nicht Produkt-Build, nicht zweite Runtime, kein Submodule | CONFIRMED |
| Lokal | `refs/primext/` (gitignored, ohne Submodule/vcpkg) | CONFIRMED |

Bewusste Wahl: letzter veröffentlichter Release-Tag, nicht floating `master`. Der Tag zeigte an diesem Tag auf denselben SHA wie `master`.

Wiederherstellen (kein Submodule, kein `--recursive`, kein vcpkg/PhysX):

```bash
git clone --no-recurse-submodules \
  https://github.com/SNMetamorph/PrimeXT.git refs/primext
git -C refs/primext checkout --detach 46fb05b41e58ed887718649e1720313baaac9a35
git -C refs/primext rev-parse HEAD
# erwartet: 46fb05b41e58ed887718649e1720313baaac9a35
```

Flacher Snapshot desselben Pins: `git clone --depth 1 --branch continious --single-branch --no-recurse-submodules https://github.com/SNMetamorph/PrimeXT.git refs/primext` — SHA danach prüfen, nicht `master`/`latest` ziehen.

## Lead-Schlussfolgerung

PX2 ist nur die Render-API-Brücke. `HUD_GetRenderInterface` setzt `render_interface_t`; `GL_RenderFrame` gibt 0 zurück, bis ein Custom-Pfad wirklich existiert. Xash bleibt Fallback. PrimeXT-Source bleibt in `refs/primext/`. ImGui, PhysX, Studio-Ersatz, Shader/HDR und Inferno-Optik gehören nicht nach PX2.

## Dependency Map

### Renderer (Agent A)

Lifecycle PrimeXT: `HUD_GetRenderInterface` setzt `g_fRenderInitialized` → `HUD_Init` ruft `GL_Init` → `HUD_VidInit` ruft `R_VidInit` → Frame `HUD_RenderFrame` → `GL_Shutdown` in `HUD_Shutdown`.

| Teilstück | PrimeXT-Pfad | Interne Deps | Xash-APIs | Common/Thirdparty | HL-Annahme | CS-Retro-Ziel | Vorstufe | Risiko |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Entry | `client/render/gl_rmain.cpp` `HUD_GetRenderInterface` | `gRenderfuncs`, `tr` | `render_api_t` 37 | `common/render_api.h` | Xash, nicht GoldSrc | `client/body/cl_dll/cdll_int.cpp` | PX2 | Callbacks aktivieren ändert Framepfad |
| GL-Init | `gl_export.cpp` `GL_Init` | Extensions, Shader, Textures | `GL_GetProcAddress` | eigene GL-Loader | OpenGL, kein `soft` | neuer Client-Renderpfad | PX3 | Soft-Renderer wird von PrimeXT abgelehnt |
| World/BSP | `gl_world_new.cpp`, `gl_rsurf.cpp`, `gl_scene.cpp` | Materials, Lightmaps, vis | `Mod_ProcessUserData`, `R_FatPVS` | `game_shared/meshdesc*` | HL1-BSP + Deluxe | Client-Renderer | PX3 | Mapdata/CRC, nicht CS-Buy |
| vis | `Mod_GetCurrentVis` in `gl_rmain.cpp` | World-Userdata | `R_FatPVS` | — | Engine-PVS | PX3 | PX3 | NULL-Callback ist heute sicher |
| Lightmaps | `gl_lightmap.cpp`, `HUD_BuildLightmaps` | Gamma-Table, Grass-Update | `GL_BuildLightmaps` | — | Lightstyles | PX3 | PX3 | Gamma/`vid_restart` |
| Sprites | `gl_sprite.cpp`, `g_SpriteRenderer` | Studio-Init | Studio-API | — | HL-Sprites | PX3 | PX3 | Tempents/Inferno-Tents |
| Transparenz | `gl_rsurf.cpp`, `gl_backend.cpp` | Sort/Pass | Triangle-API | Shader | GoldSrc-Sort | PX3 | PX3 | Reihenfolge vs HUD |
| Studio | `gl_studio_*.cpp` | Bone-Setup, VBO, Decals | `HUD_GetStudioModelInterface` | `game_shared/bone_setup` | HL-Studio | **nicht** PrimeXT in PX2 | PX4 | Konflikt mit `EF_CSRETRO_*` |
| Particles | `gl_rpart.cpp` | — | `GL_DrawParticles` | — | Engine-Particles | PX3/PX9 | PX3 | Inferno-Tents getrennt halten |
| Licht/Schatten | `gl_dlight.cpp`, `gl_shadows.cpp`, `gl_shadowmap.cpp`, `gl_slight.cpp` | FBO, Shader | dlights | GLSL `game_dir/glsl` | HL-dlights + eigene Lights | PX4–6 | PX3 World zuerst | HDR-Voraussetzung |
| HDR/PostFX | `gl_postprocess.cpp`, `gl_framebuffer.cpp`, `client/postfx_*` | FBO, MSAA | — | GLSL `postfx/` | PrimeXT-Maps | PX6 / PX10 | PX3 spielbar ohne HDR | Architektur: PX3 vor Bloom |
| Materials | `gl_material.h`, `game_shared/material.cpp` | Scripts | Texture-API | `game_dir/scripts` | eigene Materialsprache | PX3–4 | PX3 | Lizenz/Assets |

### ImGui (Agent B)

Lifecycle: `GL_Init` → `CImGuiManager::Initialize` → `VidInitialize` in `HUD_VidInit` → `NewFrame` am Ende von `GL_BackendEndFrame` (`gl_backend.cpp`) → `Terminate` in `GL_Shutdown`.

| Teil | Portierbar allein? | Abhängigkeit |
| --- | --- | --- |
| `CImGuiManager` Keys/Text/Maus/Clipboard | Teilweise | VGUI-Support-API (`vgui_support_int`), nicht nur Xash-Key |
| Cursor | Nein ohne VGUI-API | `g_VguiApiFuncs->CursorSelect` |
| Backend `CImGuiBackend` | Nein | PrimeXT-GL, Shader, `GL_GetProcAddress` |
| Windows (Material-Editor, PostFX-Menü) | Nein | Renderer + Materials |
| Spieler-UI | DO NOT PORT | Architektur: ImGui fest Tools, VGUI2 bleibt |

PX2/PX3: kein ImGui. PX7: Tools, nach spielbarem Renderpfad.

### Render API (Agent C)

Version überall 37: Engine `engine/common/render_api.h`, Client `client/body/common/render_api.h`, PrimeXT `refs/primext/common/render_api.h`.

`render_interface_t`-Callbacks (Reihenfolge identisch):

1. `version`
2. `GL_RenderFrame` — 0 Engine / 1 Client. CONFIRMED `engine/ref/gl/gl_rmain.c`.
3. `GL_BuildLightmaps`
4. `GL_OrthoBounds`
5. `R_CreateStudioDecalList`
6. `R_ClearStudioDecals`
7. `R_SpeedsMessage`
8. `Mod_ProcessUserData`
9. `R_ProcessEntData`
10. `Mod_GetCurrentVis`
11. `R_NewMap`
12. `R_ClearScene`
13. `CL_UpdateLatchedVars`

Xash prüft jeden `drawFuncs`-Zeiger auf NULL. PX2 darf nur `GL_RenderFrame` setzen.

CS Retro heute: `GetClientAPI` exportiert `HUD_GetRenderInterface`; Body kopiert `gRenderAPI`, setzt `*callback` **nicht**.

Studio bleibt CS Retro: `HUD_GetStudioModelInterface` → `GameStudioModelRenderer`. Engine-GL/Soft ehren `EF_CSRETRO_ITEM` / `EF_CSRETRO_PREVIEW`. PrimeXT-Studio nicht in PX2.

Client-`render_api.h` hat `AVI_Reserved0/1` statt Engine-`AVI_Think`/`AVI_SetParm`. Gleiche Slot-Anzahl. ABI-Quelle für PX2: `engine/common/render_api.h`.

### Build / PhysX (Agent D)

PrimeXT: CMake 3.19, vcpkg, Features `client` (imgui ≥ 1.89.9), `server`, `utils`, `physx` (PhysX 4.1.2#6). `ENABLE_PHYSX` default ON, aber nur Server (`server/physx/`). Client-CMake linkt imgui + fmt, nicht PhysX.

PhysX: **DO NOT PORT** für PX2/PX3. Kein Gegenbeweis, dass der Renderer PhysX braucht.

PX2-Min-Deps: keine neuen Thirdparty.

PX3-Min-Deps: OpenGL-Funktionen über vorhandenes Xash-`GL_GetProcAddress`; Shader/Material-Assets aus `refs/primext/game_dir/glsl` und `scripts` nur nach Lizenzprüfung. `game_shared` PrimeXT (math/material/meshdesc) ADAPT, nicht Blanket-Copy. fmt/imgui/vcpkg nicht für PX3 nötig.

## Port-Matrix

| Subsystem | PrimeXT-Pfade | CS-Retro heute | Xash-Abh. | zus. Deps | Entscheidung | Zielphase | Risiken | Tests |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Render-API-Brücke | `gl_rmain.cpp` `HUD_GetRenderInterface` | `cdll_int.cpp` Callbacks aus | `CL_RENDER_INTERFACE_VERSION` 37 | — | ADAPT | PX2 | Fallback vergessen | Frame = Xash; Gate |
| Custom Frame | `HUD_RenderFrame` | Xash `R_RenderScene` | `ref_viewpass_s` | GL | DEFER voll, PX2 nur return 0 | PX2/PX3 | return 1 ohne Welt | `r_csretro_renderer` |
| World/BSP | `gl_world_new.cpp`, `gl_rsurf.cpp` | Xash-GL | PVS, model_t | GLSL, materials | ADAPT | PX3 | HL-only Maps | Mapchange, vis |
| Lightmaps | `gl_lightmap.cpp` | Xash | `GL_BuildLightmaps` | — | ADAPT | PX3 | Gamma | `vid_restart` |
| Sprites/Tents | `gl_sprite.cpp` | Xash + Inferno-Tents | Tent-API | — | REFERENCE ONLY zuerst | PX3/PX9 | Tent-Pool | Incendiary #2 |
| Studio | `gl_studio_*.cpp` | `GameStudioModelRenderer` | Studio-API 1 | bone_setup | DEFER | PX4 | Preview-Flags | Team/Klasse/Buy |
| Particles | `gl_rpart.cpp` | Engine-Particles | `GL_DrawParticles` | — | DEFER | PX9 | Inferno | — |
| Licht/Schatten | `gl_dlight.cpp`, `gl_shadow*` | Xash-dlights | dlight_t | FBO | DEFER | PX4–6 | Perf | — |
| HDR/PostFX | `gl_postprocess.cpp` | — | — | FBO, GLSL | DEFER | PX6/PX10 | PX3-First | — |
| Materials/Shader | `game_shared/material.cpp`, `game_dir/glsl` | Xash-Texturen | Texture-API | Assets | ADAPT (Lizenz) | PX3 | Provenance | Asset-Gate |
| ImGui-Tools | `client/ui/*`, `gl_imgui_backend.cpp` | VGUI2 GameUI | VGUI-API | imgui | DEFER | PX7 | Renderer-Kopplung | — |
| Spieler-UI | ImGui-Windows | VGUI2 | — | imgui | DO NOT PORT | PX8 eval | Doppel-UI | — |
| PhysX | `server/physx/*` | ReGameDLL-Physik | — | PhysX 4.1 | DO NOT PORT | — | Movement-Vertrag | — |
| PrimeXT-Server/Waffen | `refs/primext/server/` | ReGameDLL | — | — | DO NOT PORT | — | CS-Logik | — |
| Utils (pxbsp/pxrad) | `utils/` | — | — | miniz | REFERENCE ONLY | später | nicht Runtime | — |
| Engine-Fork PrimeXT | `refs/primext/engine/` | Xash-FWGS | — | — | DO NOT PORT | — | zweite Engine | — |

## PX2-Minimalschnitt (Issue #4)

Dateien: `client/body/cl_dll/cdll_int.cpp` (`HUD_GetRenderInterface`). Export unverändert (`GetClientAPI` setzt den Pointer schon).

Verhalten: `*callback` = statische CS-Retro-`render_interface_t` mit `version = 37` und `GL_RenderFrame` → immer 0. Rest NULL. CVar `r_csretro_renderer` 0 = Xash-Fallback, 1 = Custom angefordert, ebenfalls return 0 (kein PX3).

Nicht: PrimeXT-Copy, ImGui, Studio, PhysX, Shader, Inferno.

### PX2-Nachweis (2026-09-19)

| Punkt | Status | Beleg |
| --- | --- | --- |
| Interface v37 angenommen | CONFIRMED | Log `HUD_GetRenderInterface accepted v37` |
| Callbacks registriert | CONFIRMED | Log `CS-Retro render callbacks registered`; nur `GL_RenderFrame` gesetzt |
| `GL_RenderFrame` erreicht, return 0 | CONFIRMED | Log `GL_RenderFrame callback reached` + `Xash fallback selected` |
| Start / Mapload / Connect | CONFIRMED | `+map de_dust` und Probe-CFG: `level loaded`, `client connected` |
| Mapchange | CONFIRMED | `de_dust` → `de_aztec` → `de_dust`, kein Crash |
| Disconnect / Reconnect | CONFIRMED | `Host_EndGame: disconnected` danach erneut `Spawn Server: de_dust` |
| Video-Reinit | CONFIRMED | Xash hat kein `vid_restart`; `vid_setmode 1024 768` / `1280 720` ohne Crash. Handshake läuft nur beim Client-Load, Callbacks bleiben. |
| Shutdown | CONFIRMED | Probe endete mit `quit`, Exit 0 |
| Team / Klasse / Buy-Vorschau | CONFIRMED | Isolierter Lauf 2026-09-20, `build/px2-cert-shots/`. Team T+CT-Modelle+Embleme; Klasse 4×T und 4×CT; Buy-Raster + Player-Preview beider Teams |
| Pixelgleichheit vs. vor PX2 | INFERRED | return 0 → Xash `R_RenderScene`; kein Pixeldiff, keine offensichtliche Regression |
| Join als Teamspieler + Viewmodel | CONFIRMED | T-Spawn Glock + HUD; CT-Spawn USP + HUD; Welt `de_dust` sichtbar |
| HE / Smoke / Flash sichtbar | CONFIRMED | HE-Viewmodel + Explosion; Smoke-Viewmodel + Wolke; Flash-Viewmodel + Wurf-Entity. Nicht #2-DoD |

#1–#3 bleiben PX0-Verifikation. #4 visuell PASS 2026-09-20. PX3-Produktcode nicht gestartet. `GL_RenderFrame` bleibt 0.

## Movement-Review (76ee12f, kein neues Contract-Issue)

CONFIRMED gleich: Header-Konstanten inkl. `BUNNYJUMP_MAX_SPEED_FACTOR` 1.2; `fuser2 = 450` Client `pm_shared.cpp:2498` und GameDLL `pm_shared.cpp:2822`; AirAccel Wishspd-Cap 30; Friction-Formel; Jump-Default `sqrt(2*800*45)`.

CONFIRMED Server-only (`REGAMEDLL_ADD` in `cmake/CsretroGameDll.cmake`): `mp_stamina_restore_rate` Default 0; `IN_RUN`/`fuser3`; `mp_jump_height` Default 45. Bei Defaults kein Predictionsbruch.

Der alte ~7k-Diff auf `server/.../pm_shared.cpp` in `76ee12f` ist Format/Lockstep-Restore, kein offener Vertrag. Replay bleibt #1.

## Docs-Links

MkDocs-i18n: `architecture.md` / `upstream.md` in der Site sind keine toten Links. Tote internen ROLE-Pfade (`docs/BOTS.md`, `docs/SERVER.md`, `docs/ROLLEN.md`) wurden in bestehenden Dateien nachgezogen. HUD/Konsole: `76ee12f` enthält einen gezielten Console-Fix; ohne aktuelle Repro kein Issue.

## PX3A — Frame-Kompositionsvertrag (2026-09-20)

Kein Produktcode. `GL_RenderFrame` bleibt 0. Leitzaun: **`return 1` erst wenn klar ist, wer jeden notwendigen sichtbaren Pass besitzt.**

### Xash frame coverage

CONFIRMED `engine/engine/client/cl_view.c` `V_RenderView` → `GL_RenderFrame` (`cl_view.c:418`) → `ref.dllFuncs.GL_RenderFrame` → `engine/ref/gl/gl_rmain.c` `R_RenderFrame` (`gl_rmain.c:1079`).

Normaler World-Frame **nur wenn** Client-`GL_RenderFrame` fehlt oder **0** zurückgibt:

```text
R_RenderFrame
  R_SetupRefParams
  [Client GL_RenderFrame]          → 0
  R_RunViewmodelEvents             (wenn nicht RF_ONLY_CLIENTDRAW)
  R_RenderScene                    gl_rmain.c:947
    R_DrawWorld
    R_DrawEntitiesOnList           gl_rmain.c:794
      solid brush/alias/studio
      solid sprites
      CL_DrawEFX(..., false)       gl_rmain.c:864
      HUD_DrawNormalTriangles      gl_rmain.c:870
      translucent brush/alias/studio/sprite
      HUD_DrawTransparentTriangles gl_rmain.c:917
      CL_DrawEFX(..., true)        gl_rmain.c:925
      R_DrawViewModel              gl_rmain.c:934
    R_DrawWaterSurfaces
```

`CL_EmitEntities` (`cl_frame.c:1287`) füllt `tr.draw_list` **vor** dem Draw. `return 1` stoppt nur das Zeichnen, nicht Allokation/Simulation.

HUD-2D/VGUI liegen **nicht** in `R_RenderScene`. CONFIRMED `V_PostRender` (`cl_view.c:526`): `CL_DrawHUD` / `HUD_Redraw` und `VGui_Paint` laufen nach dem 3D-Pass weiter. 3D-Team/Klasse/Buy-Previews und das Menü-`pfnRenderScene` (`cl_gameui.c:875`) gehen durch dieselbe `GL_RenderFrame`-Naht und sterben bei `return 1`, wenn der Client sie nicht selbst zeichnet.

`RF_ONLY_CLIENTDRAW` ist ein **anderer** Pfad als `return 1`: `R_RenderScene` läuft, World/Entities/EFX/Viewmodel nicht, HUD-Triangles schon. `return 1` streicht auch die HUD-Triangles.

### What return 1 suppresses

CONFIRMED `gl_rmain.c:1091–1101`: bei gesetztem Callback und Rückgabe 1 setzt Xash `tr.fCustomRendering`, ruft `R_GatherPlayerLight(tr.viewent)`, erhöht `tr.realframecount`, setzt `tr.fResetVis`, **return** — **kein** `R_RenderScene`.

Damit entfallen in diesem Frame: World/BSP, Brush-Entities, Studio-Entities (inkl. Spieler), Sprites inkl. TempEnt-Sprites, beide `CL_DrawEFX`-Pässe (Beams / Particles / Tracer — **nicht** TempEnt-Sprites), Client-Triangles, Viewmodel-Events, Viewmodel, Water, `R_PushDlights`.

Was weiterläuft: `R_SetupRefParams`, der Takeover-Zweig oben, Entity-Link/Tent-Think, später `V_PostRender` / `R_EndFrame` / Swap. Soft-Renderer hat denselben Takeover (`engine/ref/soft/r_main.c:1154`).

### PrimeXT frame coverage

CONFIRMED Pin `46fb05b` `refs/primext/client/render/gl_rmain.cpp`:

- `HUD_RenderFrame` (`gl_rmain.cpp:1003`) ist der volle Takeover. Kommentar: return 1 = Client zeichnet **alles**; return 0 z. B. wenn `GL_BackendStartFrame` scheitert oder Preview nicht vom Client kommen soll.
- Erfolgreicher Pfad: `R_RenderScene` + `GL_BackendEndFrame`, dann **return 1** (`gl_rmain.cpp:1053–1059`).
- PrimeXT-`R_RenderScene` (`gl_rmain.cpp:944`): Sky, solid Brush, **solid Studio**, `HUD_DrawNormalTriangles`, Particles, Trans-Liste (sortiert `R_SortTransMeshes`), Particles trans, Weather, `HUD_DrawTransparentTriangles`.
- Viewmodel: `GL_BackendEndFrame` → `R_DrawViewModel` (`gl_backend.cpp:475`).
- PrimeXT setzt **alle** `render_interface_t`-Slots (`gl_rmain.cpp:1134–1149`), nicht nur `GL_RenderFrame`.

Was PrimeXT selbst besitzen muss, weil Xash bei return 1 nicht mehr zeichnet: World/Brush, Studio, Sprites/Quads in der Trans-Liste, Engine-EFX via `GL_DrawParticles`, Client-Triangles, Viewmodel, Map-Userdata/Lightmaps.

Zusätzlich CONFIRMED: PrimeXT **stiehlt** sichtbare Ents in `HUD_AddEntity` (`refs/primext/client/entity.cpp:43`) — `R_AddEntity` in die eigene Liste, return 0, damit Xash sie nicht in `tr.draw_list` legt. `ET_BEAM` return 1, damit `CL_DrawBeams` die Engine-Beam-Liste behält. `R_ClearScene` leert die PrimeXT-Liste jedes Frame. Viewmodel liegt **nicht** in `tr.draw_entities`. PrimeXT-`tri.cpp`-Stubs sind leer; CS-Retro-`tri.cpp` ist es nicht (Overview, Fog, ParticleMan/Wetter, `EV_UpdateMolotovHeld`).

### Existing CS Studio reusable?

**POSSIBLE** — Studio-Interface ist unabhängig von `render_interface_t`.

CONFIRMED: `HUD_GetStudioModelInterface` (`GameStudioModelRenderer.cpp:1220`) liefert `R_StudioDrawModel` / `R_StudioDrawPlayer`. Xash ruft das nur aus `R_StudioDrawModelInternal` (`gl_studio.c:3366`) auf, und nur wenn `RF_DRAW_WORLD` und nicht `r_studio_builtin_renderer`. Aufrufer: `R_DrawStudioModel` in der Entity-Liste und `R_DrawViewModel` (`gl_studio.c:3482`).

Ein Custom-Frame kann `g_StudioRenderer` weiter als CS-Was nutzen, **wenn** er Entity/Model-Kontext setzt (`IEngineStudio`, `gRenderAPI.R_SetCurrentEntity` in `engine/ref/gl/gl_context.c:253`) und `StudioDrawModel` / `StudioDrawPlayer` selbst aufruft. PrimeXT-Studio muss dafür nicht übernommen werden.

**NOT PRACTICAL** ohne diesen expliziten Aufruf: `return 1` allein ruft `GameStudioModelRenderer` nicht.

`EF_CSRETRO_PREVIEW` sitzt in GSMR (Client-Was). `EF_CSRETRO_ITEM` sitzt nur in Engine-`R_StudioDrawPoints` — GSMR prüft das Bit nicht. PrimeXT ersetzt `pStudioDraw` **nicht** (`gl_studio_init.cpp`); deren VBO-Studio ist ein paralleler Pfad ohne CS-Was.

Viewmodel-Strategie: entweder weiter Engine `R_DrawViewModel` (nur bei return 0 oder Hybrid-B) oder Custom-Frame zeichnet `GetViewModel()` inkl. `STUDIO_EVENTS` (Wick-Knochen) und `STUDIO_RENDER`. `GetViewInfo` / Frustum kommen heute erst in `R_SetupFrustum` **nach** dem Custom-Hook — der Custom-Frame muss View-Vektoren selbst setzen.

### Entity / Sprite / EFX / TempEnt

| Teilstück | Heute | Bei return 1 | Öffentliche API |
| --- | --- | --- | --- |
| Brush ents | `R_DrawBrushModel` in Entity-Liste | Liste voll, niemand zeichnet | intern `ref/gl`; `HUD_AddEntity` sieht sie nur |
| Studio ents | `R_DrawStudioModel` → GSMR | weg | Studio-Interface, siehe oben |
| Sprites | `R_DrawSpriteModel` in derselben Liste | weg | intern `ref/gl`; `SPR_*` ist HUD-2D |
| TempEnt-Sprites | Tent → `CL_AddVisibleEntity` → **`R_DrawSpriteModel`** | Sim/Think weiter, **Pixel weg** | Alloc über `pEfxAPI`; Draw fehlt |
| Engine-EFX | `CL_DrawEFX` (Beams / Particles / Tracer) | weg | `gRenderAPI.GL_DrawParticles` zweimal (false/true) |
| ParticleMan | nur Wetter in `HUD_DrawTransparentTriangles` | weg, wenn Hook fehlt | direkt aufrufbar; **keine Granaten** |
| Inferno/Smoke TE_SPRITE | Event/TE allokiert; Draw = Trans-Sprite | **unsichtbar** — Härte #2 | Sprite-Listen-Draw, nicht `GL_DrawParticles` |
| Client-Triangles / Wick | Xash nach Entities | weg | CS-Retro-`tri.cpp` selbst rufen (`EV_UpdateMolotovHeld`) |
| Viewmodel | `R_DrawViewModel` + vorher Events | weg | `IEngineStudio` + `GetViewModel()` |

TempEnt-Strategie: **nicht** `return 1`, solange niemand `R_DrawSpriteModel` bzw. die Trans-Sprite-Liste zeichnet. `GL_DrawParticles` ersetzt das **nicht**. Sonst ist #2 (Incendiary) strukturell unsichtbar — gleicher sichtbarer Ausfall wie ein voller Tent-Pool, nur diesmal durch den Renderer.

### Required callbacks (nicht aktivieren)

Xash NULL-prüft jeden Slot. Bewertung für einen späteren Custom-Frame, nicht für jetzt:

| Slot | Aufrufer | Bewertung |
| --- | --- | --- |
| `GL_BuildLightmaps` | `gl_rsurf.c:3898`, `gl_rmain.c:988` | needed for world preparation |
| `GL_OrthoBounds` | `gl_rsurf.c:121` | needed only later (overview) |
| `R_CreateStudioDecalList` / `R_ClearStudioDecals` | `gl_decals.c` | needed only later |
| `R_SpeedsMessage` | `gl_backend.c:30` | not needed yet |
| `Mod_ProcessUserData` | `gl_context.c:133/156` (Modell load/unload) | needed for world preparation |
| `R_ProcessEntData` | `gl_context.c:305` (nur GL) | needed only later (PrimeXT Studio-Instances; erster World-Nachweis ohne) |
| `Mod_GetCurrentVis` | `gl_rsurf.c:114` nur wenn `tr.fCustomRendering` | needed before return 1 |
| `R_NewMap` | `gl_context.c:436` | needed for world preparation |
| `R_ClearScene` | `gl_rmain.c:224` | needed before return 1 |
| `CL_UpdateLatchedVars` | `cl_frame.c:228/285` | needed only later (Studio-Lerp), evtl. before return 1 wenn Custom-Studio |

Keine Callback-Funktion nur deshalb setzen, weil PrimeXT sie besitzt.

### Required engine extensions

Für Strategie B dauerhaft denkbar: `R_RenderScene` so teilen, dass World clientseitig und Entity/EFX/Viewmodel engine-seitig bleiben. Nur wenn die Naht bleibt, keine Wegwerf-API.

Für Strategie A ohne Engine-Änderung: Client braucht eigenen **Sprite-Listen-Draw** (TempEnts) und muss `GL_DrawParticles` plus CS-Retro-`tri.cpp` selbst rufen. `render_api_t` v37 ist eingefroren; neue Slots nur am Ende. `R_DrawEntitiesOnList` / `R_DrawSpriteModel` sind intern.

### Recommended strategy: C

**C — offscreen/diagnostisch.** Kein Rückschritt.

Begründung: Ein sichtbares `return 1` ohne Studio-, Sprite-, EFX- und Viewmodel-Besitz erzeugt einen halben Frame (Welt ohne Waffen/FX/Previews). Das verletzt den Leitzaun. Strategie A wäre PrimeXT-vollständig und zieht Studio/EFX in PX3 vor. Strategie B ist nur sinnvoll, wenn CS Retro eine **dauerhafte** World-vs-Rest-Naht in Xash will — derzeit nicht nötig, um World-Technik zu lernen.

Strategie C: World/Lightmap-Technik hinter `GL_RenderFrame → 0` aufbauen und nachweisen (Offscreen/Debug). Sichtbarer Takeover erst, wenn Komposition (Entities, Sprites, `CL_DrawEFX` oder Ersatz, Viewmodel, CS-Studio, Previews) einen Besitzer hat.

### Updated phase proposal

```text
PX3A  Frame-Kompositionsvertrag (diese Research) — fertig
PX3B  Renderer-Core + World offscreen; GL_RenderFrame bleibt 0
PX3C  Entity / Sprite / EFX-Komposition (inkl. TempEnt/#2)
PX4   Studio-Fusion: bestehender GameStudioModelRenderer im Custom Frame
PX4 Exit → erster vollständiger return 1
```

Alte Matrix „World = PX3, Studio = PX4“ als **sofort sichtbarer** Custom-Frame ist falsch. Als Lernreihenfolge hinter return 0 bleibt sie gültig.

### Files expected to change (erst nach Freigabe, nicht jetzt)

PX3B (nach neuer Freigabe): neuer Client-Renderpfad neben `cdll_int.cpp`, evtl. Diagnose-FBO; Docs. Kein `return 1`.

### Files explicitly untouched

`pm_shared`, Inferno-Gameplay, Zippo, ReGameDLL-Regeln, PhysX, ImGui, Spieler-UI, `GameStudioModelRenderer` Ersatz, `EF_CSRETRO_*`, PBR/HDR/PostFX, CS2-Waffen, PrimeXT-Copy nach `client/body/`.

### PX3A RESULT

```text
Xash frame coverage:          CONFIRMED siehe oben
PrimeXT frame coverage:       CONFIRMED voller Takeover + eigene Studio/EFX/VM-Pässe
What return 1 suppresses:     CONFIRMED gesamtes R_RenderScene
World dependencies:           Lightmaps, Mod_ProcessUserData, R_NewMap, vis
Entity dependencies:          R_DrawEntitiesOnList oder eigener Pass
Sprite/EFX dependencies:      TempEnt-Sprites = R_DrawSpriteModel; CL_DrawEFX = Beams/Particles via GL_DrawParticles
Existing CS Studio reusable?: POSSIBLE (expliziter GSMR-Aufruf); NOT PRACTICAL bei nacktem return 1
Viewmodel strategy:           Engine-Pass behalten bis Takeover vollständig; Events für Wick
TempEnt strategy:             Sprite-Listen-Draw behalten oder ersetzen; GL_DrawParticles reicht nicht (#2)
Required callbacks:           siehe Tabelle; jetzt keine aktivieren
Required engine extensions:   keine für C; B nur als dauerhafte Naht; A braucht EFX-Hook
Recommended strategy A/B/C:   C
Files expected to change:     erst PX3B nach Freigabe
Files explicitly untouched:   Locks oben
New issues:                   [#5 PX3B World offscreen](https://github.com/benjarogit/csretro/issues/5) (Implementation; offen bis DoD)
Updated phase proposal:       PX3A done → PX3B offscreen → PX3C EFX → PX4 Studio → return 1
```

## PX3B — World offscreen (2026-09-20)

Erste gezielte PrimeXT-derived Produktintegration. **Kein Blanket-Import.**
`GL_RenderFrame` bleibt unter allen Umständen 0. Sichtbarer Frame = Xash.

„Kein PrimeXT-Produktimport“ heißt nicht „keinen PrimeXT-Code adaptieren“.
Erlaubt: untersuchen → Teil wählen → an CS Retro anpassen → `client/render/` → Herkunft dokumentieren.
Nicht erlaubt: gesamten PrimeXT-`client/render/`-Baum kopieren, zweite Runtime, PrimeXT-Layer, ungeprüftes `game_shared/`.

### Produktpfad

| Verantwortung | CS-Retro-Datei |
| --- | --- |
| Lifecycle | `client/render/render_core.cpp` + Brücke `cdll_int.cpp` |
| GL / Offscreen | `client/render/render_backend.cpp` |
| World/BSP + Draw | `client/render/render_world.cpp` + `render_xash_brush.h` |

Eine `client_amd64.so`. `cdll_int.cpp` bleibt Bridge: `GL_RenderFrame` ruft `CSRETRO_Renderer_Frame` (void) und **return 0**.

### CVar

| CVar | Default | Bedeutung |
| --- | --- | --- |
| `r_csretro_renderer` | 0 | 0 = Xash-only, kein Extra-Cost. 1 = Offscreen-Probe + sichtbarer Xash-Fallback. Kein sichtbarer Custom-Renderer. |
| `r_csretro_offscreen_dump` | 0 | Diagnose; einmaliger PPM-Dump passiert ohnehin beim ersten nicht-leeren Probe-Pass. |
| `r_csretro_probe_seq` | 0 | Diagnose: dust → aztec → dust → `vid_setmode` → quit. |

### Callbacks

Aktiviert, weil der Nachweis sie braucht (additiv, ersetzen Xash nicht):

| Slot | Warum | Daten | Bleibt Xash | Unload / Map / Vid |
| --- | --- | --- | --- | --- |
| `Mod_ProcessUserData` | World-Load/Unload erkennen | `model_s*`, create-Flag | Textur-/Modell-Load | create=false gibt Mesh frei |
| `R_NewMap` | Nach Engine-Lightmaps/Polys Mesh kopieren | `pfnGetModel(1)` | sichtbares Draw | Recapture, alte Handles weg |
| `GL_BuildLightmaps` | Nach Engine-LM-Rebuild UVs/Pages refreshen | aktuelles Worldmesh | Engine baut LM zuerst (`gl_rsurf.c`) | Recapture |

Absichtlich NULL: `Mod_GetCurrentVis`, `R_ClearScene`, `R_ProcessEntData`, Studio-Decals, `GL_OrthoBounds`, `R_SpeedsMessage`, `CL_UpdateLatchedVars`.

### Provenance (gezielt)

| Stück | Upstream | Pin | Original | CS-Retro | Reason |
| --- | --- | --- | --- | --- | --- |
| Lifecycle-Naht | SNMetamorph/PrimeXT | `46fb05b` / continious | `client/render/gl_rmain.cpp` HUD_* + `HUD_GetRenderInterface` | `render_core.cpp`, `cdll_int.cpp` | Init/Vid/Shutdown/Map ohne Takeover |
| GL-Proc-Load | PrimeXT | derselbe | `client/render/gl_export.cpp` `GL_GetProcAddress` | `render_backend.cpp` | Nur nötige Procs, Xash-Wrapper für Bind/CreateTexture |
| FBO-Idee | PrimeXT | derselbe | `client/render/gl_framebuffer.cpp` | `render_backend.cpp` (eigenes FBO, 512²) | Offscreen-Nachweis, Default aus |
| World-Mesh aus Surfaces | PrimeXT | derselbe | `client/render/gl_world_new.cpp` `Mod_CreateBufferObject` | `render_world.cpp` | Echte BSP-Surfaces/Polys, Kopie statt Engine-Pointer |
| Lightmap-Pages | PrimeXT / Xash | derselbe | `gl_lightmap.cpp` + Engine `PARM_TEX_LIGHTMAP` | `render_world.cpp` bindet Engine-LM | Baseline-LM, kein eigener Atlas |
| View-Matrix | GoldSrc/Xash | — | `R_SetupGL` / Quake-Rotate | `render_backend.cpp` `ApplyView` | Offscreen-Kamera aus `ref_viewpass` |

Nicht übernommen: Studio, Sprites, Particles, Weather, ImGui, PostFX, HDR, Shadows, PBR, PhysX, PrimeXT-Server, volles Materialsystem, Shader-VBO-Pfad.

### Probe

`./scripts/px3b-offscreen-probe.sh` — erwartet dust + aztec Offscreen-Proof, `empty=0`, `GL_RenderFrame` nie 1.

### PX3B-Nachweis (2026-09-20)

Gegen `bbe418d` / v0.1.12. Sichtbarer Lauf mit `r_csretro_renderer 1` (sonst nur Xash ohne Offscreen-Pass). Shots: `build/px3b-cert-shots/` (nicht committed). Automatische Probe: `./scripts/px3b-offscreen-probe.sh` PASS.

| Punkt | Status | Beleg |
| --- | --- | --- |
| Interface v37 + Callbacks | CONFIRMED | `HUD_GetRenderInterface accepted v37`; `GL_RenderFrame always 0` |
| `GL_RenderFrame` erreicht, return 0 | CONFIRMED | `GL_RenderFrame callback reached` + `r_csretro_renderer 1 offscreen probe, visible frame stays Xash` |
| Offscreen dust nonempty | CONFIRMED | `map=maps/de_dust.bsp … empty=0 crc=24f7c85e pixels=45285` |
| Offscreen aztec nonempty | CONFIRMED | `map=maps/de_aztec.bsp … empty=0 crc=b574ac9e pixels=128406` |
| Mapchange zurück | CONFIRMED | Probe `de_dust` → `de_aztec` → `de_dust`, gleiche Counts; visuell Team-Menü nach Mapchange sauber |
| `vid_setmode` | CONFIRMED | Probe `vid_setmode 1024 768` → neuer `crc=6a698541 pixels=22380`, weiterhin `empty=0`. Kein `vid_restart`. |
| T/CT Join, World, VM, HUD | CONFIRMED | Isolierter Lauf: T-Glock + HUD, CT-USP + HUD, `de_dust` Welt |
| Team / Klasse / Buy | CONFIRMED | T+CT-Previews, Klassen-Lineups, Buy-Raster + Player-Preview beider Teams |
| HE / Smoke / Flash | CONFIRMED | HE-Explosion; Smoke-Wolke; Flash-Viewmodel + Wurf. Nicht #2-DoD |
| Folgerahmen / Artefakte | CONFIRMED | Folge-Spawn ohne kaputten Frame; kein sichtbares FBO/Blend/Depth/Viewport/Textur-Leak |
| GL state isolation | CONFIRMED | Save/Restore vor return 0; sichtbares Xash nach Offscreen-Pass unverändert. Pixelidentität nicht verlangt |
| Movement-Gate | CONFIRMED | `./scripts/movement-contract-gate.sh` PASS |

#1–#3 bleiben PX0-Verifikation. #5 visuell PASS 2026-09-20. `GL_RenderFrame` bleibt 0.

### TempEnt-Sichtbarkeit (PX3C-Pfad, CONFIRMED)

CS Retro sieht TempEnt-`cl_entity_t` bereits, bevor Xash sie in die sichtbare Ref-Liste legt. Keine neue Engine-ABI.

```text
HUD_TempEntUpdate
  → Callback_AddVisibleEntity          (= CL_TempEntAddEntity, cl_tent.c)
    → CL_AddVisibleEntity(..., ET_TEMPENTITY)
      → HUD_AddEntity(ET_TEMPENTITY, ...)   // Spectatorfilter; sonst return 1
        → ref R_AddEntity
          → R_DrawSpriteModel                 // später in R_DrawEntitiesOnList
```

Belege: `client/body/cl_dll/entity.cpp` `HUD_TempEntUpdate` / `HUD_AddEntity`; `engine/engine/client/cl_tent.c` `CL_TempEntAddEntity`; `engine/engine/client/cl_frame.c` `CL_AddVisibleEntity`; `engine/ref/gl/gl_rmain.c` `R_AddEntity` / `R_DrawSpriteModel`.

`GL_DrawParticles` rendert **keine** TempEnt-Sprites (unverändert: Beams/Particles/Tracer only).

`R_ClearScene` (`gl_rmain.c`): Xash leert die eigene `tr.draw_list`, danach optional den Client-Callback. PX3C setzt diesen Callback **additiv**: nur die CS-Retro-Spiegelliste wird geleert. Xash-Liste bleibt. Spiegel ist kein Ownership.

### PX3C — Entity / Sprite offscreen (2026-09-20)

Issue [#6](https://github.com/benjarogit/csretro/issues/6). `GL_RenderFrame` bleibt 0. Kein sichtbarer Custom-Frame. Kein PrimeXT-Steal (`HUD_AddEntity` behält Spectatorfilter/return 1).

| Teilstück | Status | Beleg |
| --- | --- | --- |
| Entity-Spiegel | CONFIRMED | Kopie pro Frame in `render_scene.cpp`. Keine TempEnt-Pointer über Frames. |
| `R_ClearScene` additiv | CONFIRMED | Log `R_ClearScene additive`; Xash-Liste unverändert |
| TempEnt + `mod_sprite` | CONFIRMED | `TempEnt sprite mirrored: N drawn: N`; echter 8×8-HE-Spark `tex=553` an Welt-Origin |
| `ET_NORMAL` + `mod_sprite` | CONFIRMED | aztec `Normal sprite mirrored: 16 drawn: 16` |
| Studio | CONFIRMED nicht gezeichnet | `Studio classified: N local: N (not drawn)` |
| Brush-Entities | DEFERRED | gezählt (`brush: N strategy=deferred`); World-Mesh ist nur Worldmodel. Türen/transparente Brushes = eigener Pass |
| World+Sprite CRC | UNKNOWN | 512²-Readback oft identisch bei kleinen Tents (Depth/near). Pixelidentität nicht verlangt |
| Engine-EFX `GL_DrawParticles` | DEFERRED | `CL_DrawParticles` ruft `CL_ThinkParticle` — ändert Sim-State. Doppelaufruf offscreen unsicher |
| Client-Triangles | DEFERRED | `HUD_DrawTransparentTriangles` macht `ParticleMan::Update`, Fog, `EV_UpdateMolotovHeld` — nicht rein zeichnend |
| GL isolation (Sprite) | CONFIRMED soweit sichtbar | blend/alpha-test/depth-mask/cull/texenv/TMU/color/matrices Save+Restore; sichtbares Xash ohne Artefakte |
| Fehlende Sprite-Modi | DEFERRED | `SPR_ANGLED`; Frame-Lerp; Sprite-Lightmap |

Provenance Sprite-Draw: PrimeXT `46fb05b` `client/render/gl_sprite.cpp` (Frame, Quad, Orientierung, Rendermode/color/amt), an CS-Retro-Kopien + Xash-`msprite_t`-View angepasst.

`return 1` bleibt gesperrt. Studio/Viewmodel bleiben Xash/PX4. #1 #2 #3 #5 nicht angefasst.
