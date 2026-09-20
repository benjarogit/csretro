#pragma once

struct CSRETRO_SceneStats_s;
struct ref_viewpass_s;

enum
{
	CSRETRO_DRAW_SKIP = 0,
	CSRETRO_DRAW_OPAQUE,
	CSRETRO_DRAW_TRANS
};

void CSRETRO_Trans_ClassifyScene( const float *vieworg );
int CSRETRO_Trans_DrawClass( int scene_index );
void CSRETRO_Trans_NoteDrawn( int scene_index );
int CSRETRO_Trans_DuplicateDraws( void );

int CSRETRO_Trans_Draw( const float *vieworg, const float *viewangles,
	struct CSRETRO_SceneStats_s *stats, const struct ref_viewpass_s *rvp );
