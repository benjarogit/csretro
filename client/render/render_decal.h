// Offscreen world/brush surface-decal draw. Xash owns creation, pool,
// surface linkage, overlap, lifetime, save/restore and removal.
// CS Retro: read-only walk of msurface_t::pdecals and precomputed polys.
#pragma once

typedef struct CSRETRO_DecalStats_s
{
	int world_decal_surfaces;
	int world_decals;
	int world_decals_drawn;
	int world_decals_polys;
	int world_fallback;
	int brush_decal_surfaces;
	int brush_decals;
	int brush_decals_drawn;
	int brush_decals_polys;
	int brush_fallback;
	int brush_entity_index;
	char brush_model[64];
	int transparent_surfaces;
	int transparent_decals;
	int transparent_skipped;
	int premultiplied_drawn;
	int standard_blend_drawn;
	int live_mutate;
	unsigned int hash_before;
	unsigned int hash_after;
	int gl_restore_ok;
	int map_serial;
	int stored_decal_ptrs;
	int spawn_world_decals;
	int stale_skipped;
} CSRETRO_DecalStats;

void CSRETRO_Decal_BeginFrame( void );
void CSRETRO_Decal_OnNewMap( void );
unsigned int CSRETRO_Decal_HashSurfaces( void *mod );
void CSRETRO_Decal_DrawSurfaces( void *mod, int rendermode, int is_brush, int entity_index );
void CSRETRO_Decal_GetStats( CSRETRO_DecalStats *out );
