#pragma once

struct CSRETRO_SceneStats_s;

// Offscreen non-player studio via GSMR STUDIO_RENDER only.
// Never STUDIO_EVENTS. Visible Xash path is unchanged.
int CSRETRO_Studio_DrawList( struct CSRETRO_SceneStats_s *stats );
