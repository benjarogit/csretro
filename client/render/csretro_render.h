// CS-Retro renderer public API. cdll_int.cpp stays the Xash bridge.
// GL_RenderFrame is registered only in cdll_int and always returns 0.
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

// Offscreen probe when r_csretro_renderer is 1. Never decides the visible frame.
void CSRETRO_Renderer_Frame( const struct ref_viewpass_s *rvp );

// Extra callbacks (documented in docs/research/px1-primext.md PX3B/PX3C).
void CSRETRO_Renderer_OnNewMap( void );
void CSRETRO_Renderer_OnLightmaps( void );
void CSRETRO_Renderer_OnModel( struct model_s *mod, int create, const unsigned char *buffer );
void CSRETRO_Renderer_ClearScene( void );

// Mirror only. Does not change HUD_AddEntity's return value.
void CSRETRO_Renderer_AddEntity( int type, struct cl_entity_s *ent );

#ifdef __cplusplus
}
#endif
