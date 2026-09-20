// Offscreen brush-entity draw. Mirror snapshots only. No Xash R_DrawBrushModel.
#pragma once

struct CSRETRO_SceneStats_s;

typedef struct CSRETRO_BrushMove_s
{
	int index;
	char model[64];
	float origin_before[3];
	float origin_after[3];
	float angles_before[3];
	float angles_after[3];
	int happened;
} CSRETRO_BrushMove;

typedef struct CSRETRO_BrushStats_s
{
	int cache_models;
	int cache_verts;
	int opaque_drawn;
	int trans_drawn;
	int skipped_world;
	int skipped_empty;
	int turb_candidates;
	int turb_drawn;
	int waterside_candidates;
	int liquid_models;
} CSRETRO_BrushStats;

void CSRETRO_Brush_OnModel( void *mod, int create );
void CSRETRO_Brush_OnNewMap( void );
void CSRETRO_Brush_OnLightmaps( void );
void CSRETRO_Brush_Release( void );
void CSRETRO_Brush_GetStats( CSRETRO_BrushStats *out );
const CSRETRO_BrushMove *CSRETRO_Brush_LastMove( void );

// opaque_only: 1 = kRenderNormal, 0 = trans modes. Classification is sortable.
int CSRETRO_Brush_DrawPass( int opaque_only, struct CSRETRO_SceneStats_s *stats );
int CSRETRO_Brush_DrawOne( int scene_index, struct CSRETRO_SceneStats_s *stats );
