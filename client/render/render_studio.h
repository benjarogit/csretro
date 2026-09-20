#pragma once

struct CSRETRO_SceneStats_s;

// Offscreen non-player studio via GSMR STUDIO_RENDER only.
// Never STUDIO_EVENTS. Visible Xash path is unchanged.
int CSRETRO_Studio_DrawList( struct CSRETRO_SceneStats_s *stats );

// MOVETYPE_FOLLOW children with a non-player studio parent in the same-frame mirror list.
// Player-parent FOLLOW is counted and deferred (no StudioDrawPlayer).
int CSRETRO_Studio_DrawFollow( struct CSRETRO_SceneStats_s *stats );
