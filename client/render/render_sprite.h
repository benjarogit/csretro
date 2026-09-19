#pragma once

struct CSRETRO_EntCopy_s;
struct CSRETRO_SceneStats_s;

// Offscreen sprite pass. Visible Xash path is unchanged.
int CSRETRO_Sprite_DrawList( const float *vieworg, const float *viewangles,
	struct CSRETRO_SceneStats_s *stats );
