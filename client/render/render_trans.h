#pragma once

struct CSRETRO_SceneStats_s;
struct ref_viewpass_s;

int CSRETRO_Trans_Draw( const float *vieworg, const float *viewangles,
	struct CSRETRO_SceneStats_s *stats, const struct ref_viewpass_s *rvp );
