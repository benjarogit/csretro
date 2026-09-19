# PX1 — PrimeXT Research

Stand: 2026-09-19. Gegen CS Retro `76ee12f` (v0.1.6). Kein Produktcode.
Status-Wörter: **CONFIRMED** (im Baum/Remote nachgeprüft), **INFERRED** (folgt aus Code, nicht runtime-geprüft), **UNKNOWN** (nicht belegt), **DEFERRED** (bewusst später).

Issues: [#1 Movement Replay](https://github.com/benjarogit/csretro/issues/1), [#2 Incendiary In-Game](https://github.com/benjarogit/csretro/issues/2), [#3 erster M1 Granaten](https://github.com/benjarogit/csretro/issues/3) bleiben offen. [#4 PX2-Brücke](https://github.com/benjarogit/csretro/issues/4) visuell verifiziert 2026-09-20.

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
