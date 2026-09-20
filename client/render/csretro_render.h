// CS-Retro renderer public API. cdll_int.cpp stays the Xash bridge.
// Mode 0/1: GL_RenderFrame returns 0. Mode 2 (PX6A): may return 1 behind takeover gate.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ref_viewpass_s;
struct render_interface_s;
struct model_s;

void CSRETRO_Renderer_Init( void );
void CSRETRO_Renderer_VidInit( void );
void CSRETRO_Renderer_Shutdown( void );

// Mode 0: no-op return 0. Mode 1: offscreen diagnostic return 0.
// Mode 2: visible takeover candidate; return 1 only after successful commit+present.
int CSRETRO_Renderer_Frame( const struct ref_viewpass_s *rvp );

// Extra callbacks (documented in docs/research/px1-primext.md PX3B/PX3C).
void CSRETRO_Renderer_OnNewMap( void );
void CSRETRO_Renderer_OnLightmaps( void );
void CSRETRO_Renderer_OnModel( struct model_s *mod, int create, const unsigned char *buffer );
void CSRETRO_Renderer_ClearScene( void );
unsigned char *CSRETRO_Mod_GetCurrentVis( void );

// Mirror only. Does not change HUD_AddEntity's return value.
void CSRETRO_Renderer_AddEntity( int type, struct cl_entity_s *ent );

// Client-triangle ownership. Advance once per visible Xash frame. Draw-only has no simulation.
void CSRETRO_ClientTriangles_BeginFrame( void );
void CSRETRO_ClientTriangles_AdvanceNormal( void );
void CSRETRO_ClientTriangles_DrawNormalOnly( void );
void CSRETRO_ClientTriangles_AdvanceParticleMan( void );
void CSRETRO_ClientTriangles_RenderParticleMan( int update_pvs_cache );
void CSRETRO_ClientTriangles_AdvanceEnvironment( void );
void CSRETRO_ClientTriangles_AdvanceMolotovHeld( void );
void CSRETRO_ClientTriangles_DrawTransparentOnly( void );
void CSRETRO_ClientTriangles_OwnedNormalPass( void );
void CSRETRO_ClientTriangles_OwnedTransparentPass( void );
int CSRETRO_ClientTriangles_OwnedNormalCount( void );
int CSRETRO_ClientTriangles_OwnedTransparentCount( void );
int CSRETRO_ClientTriangles_ParticleCount( void );

#ifdef __cplusplus
}
#endif
