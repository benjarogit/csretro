#pragma once

struct CSRETRO_EntCopy_s;
struct CSRETRO_SceneStats_s;

// Offscreen sprite pass. Visible Xash path is unchanged.
int CSRETRO_Sprite_DrawList( const float *vieworg, const float *viewangles,
	struct CSRETRO_SceneStats_s *stats );
int CSRETRO_Sprite_DrawOne( int scene_index, const float *vieworg, const float *viewangles,
	struct CSRETRO_SceneStats_s *stats );
void CSRETRO_Sprite_ResetDump( void );
void CSRETRO_Sprite_SetNoDepth( int enabled );
