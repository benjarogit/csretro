// Current-frame vis owned by Xash, copied into a CS-Retro buffer.
// No second PVS implementation. GL_RenderFrame stays 0.
#pragma once

struct ref_viewpass_s;
struct csretro_frame_vis_s;
struct csretro_efrag_info_s;

#define CSRETRO_VIS_PVS_MAX 16384
#define CSRETRO_VIS_SURF_MAX_BYTES 8192
#define CSRETRO_VIS_EFRAG_MAX 256

int CSRETRO_Vis_Prepare( const struct ref_viewpass_s *rvp );
unsigned char *CSRETRO_Vis_CurrentBuffer( void );
int CSRETRO_Vis_Valid( void );
const struct csretro_frame_vis_s *CSRETRO_Vis_Info( void );
const unsigned char *CSRETRO_Vis_SurfMask( void );
int CSRETRO_Vis_SurfMaskBytes( void );
int CSRETRO_Vis_EfragCount( void );
const struct csretro_efrag_info_s *CSRETRO_Vis_Efrag( int index );
void CSRETRO_Vis_FeedEfrags( void );
int CSRETRO_Vis_DrawSky( void );
void CSRETRO_Vis_OnNewMap( void );
int CSRETRO_Vis_SurfaceVisible( int surface_index );
