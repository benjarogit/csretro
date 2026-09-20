// Shared BSP surface → triangle mesh. Used by world and brush-model cache.
// One builder, two destinations. Does not keep engine surface pointers.
// Normal batches and water/turb batches live in the same CSRETRO_BspMesh.
#pragma once

#define XR_MESH_MAX_BATCH 4096

enum
{
	CSRETRO_WATER_SKIP = 0,
	CSRETRO_WATER_OPAQUE = 1,
	CSRETRO_WATER_LATE = 2,
	CSRETRO_WATER_BRUSH = 3
};

typedef struct CSRETRO_MeshVert_s
{
	float xyz[3];
	float st[2];
	float lm[2];
	float poly_z0;
	int surface_index;
} CSRETRO_MeshVert;

typedef struct CSRETRO_MeshBatch_s
{
	unsigned int tex;
	unsigned int lightmap;
	int first_tri;
	int tri_count;
	int flags;
	void *anim_source; // xr_texture_t*, map-owned, valid until mesh clear
	unsigned int tex_width;
	unsigned int tex_height;
	int random_tile;
	int plane_type;
	float plane_dist;
	int surface_index;
	float surface_z0;
} CSRETRO_MeshBatch;

typedef struct CSRETRO_SurfaceSpan_s
{
	int surface_index;
	int first_tri;
	int tri_count;
	int batch_index;
	int light_s;
	int light_t;
	unsigned int lightmap;
	int flags;
} CSRETRO_SurfaceSpan;

typedef struct CSRETRO_BspMesh_s
{
	void *model;
	int firstmodelsurface;
	int nummodelsurfaces;
	CSRETRO_MeshVert *verts;
	int vert_count;
	CSRETRO_MeshBatch *batches;
	int batch_count;
	CSRETRO_SurfaceSpan *spans;
	int span_count;
	int span_cap;
	CSRETRO_MeshVert *water_verts;
	int water_vert_count;
	CSRETRO_MeshBatch *water_batches;
	int water_batch_count;
	int lightmap_pages;
	int flags_union;
	int surfaces;
	int polys;
	int skipped_turb;
	int skipped_sky;
	int turb_surfaces;
	int turb_polys;
	int turb_verts;
	int waterside_candidates;
	int liquid_model;
	int captured;
	int anim_candidates;
	int conveyor_candidates;
	int fullbright_candidates;
	int alternate_candidates;
	int random_tile_candidates;
	unsigned int geom_hash;
	int build_serial;
} CSRETRO_BspMesh;

typedef struct CSRETRO_MeshDrawContext_s
{
	float time;
	float entity_frame;
	unsigned char rendercolor[3];
	int rendermode;
	long ( *get_parm )( int parm, int arg );
	int bind_textures;
	int skip_base;
	int skip_fullbright;
	float vieworg[3];
	float wave_scale;
	int effects;
	float water_alpha;
	int water_pass;
	int is_brush;
	float entity_mins[3];
	int random_force_base;
	int random_only;
} CSRETRO_MeshDrawContext;

typedef struct CSRETRO_MeshDrawStats_s
{
	int anim_candidates;
	int anim_tex_changed;
	unsigned int anim_tex_first;
	unsigned int anim_tex_last;
	int alternate_candidates;
	int alternate_used;
	int random_tile_candidates;
	int random_resolved;
	int random_fallback;
	int random_distinct_frames;
	int random_differs_from_base;
	unsigned int random_selection_hash;
	int random_variant[10];
	int conveyor_candidates;
	int conveyor_uv_changed;
	float conveyor_s;
	float conveyor_t;
	int fullbright_candidates;
	int fullbright_drawn;
	int verts;
	int build_serial;
	int build_serial_first;
	int rebuilds_unchanged;
	unsigned int geom_hash_before;
	unsigned int geom_hash_after;
	int geom_unchanged;
	int skipped_turb;
	int turb_surfaces;
	int turb_polys;
	int turb_verts;
	int world_water_opaque;
	int world_water_late;
	int world_water_base;
	int brush_turb_candidates;
	int brush_turb_drawn;
	int waterside_candidates;
	int waterside_drawn;
	int liquid_models;
	float effective_alpha;
	int alpha_capability;
	int litwater;
	int water_cache_mutate;
	int gl_restore_ok;
} CSRETRO_MeshDrawStats;

void CSRETRO_BspMesh_Clear( CSRETRO_BspMesh *mesh );
int CSRETRO_BspMesh_Build( CSRETRO_BspMesh *mesh, void *mod );
void CSRETRO_BspMesh_Draw( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshDrawContext *ctx );
void CSRETRO_BspMesh_DrawWater( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshDrawContext *ctx );
void CSRETRO_BspMesh_OnNewMap( void );
void CSRETRO_BspMesh_BeginFrame( void );
void CSRETRO_BspMesh_GetDrawStats( CSRETRO_MeshDrawStats *out );
int CSRETRO_BspMesh_BuildCount( void );
float CSRETRO_DecodeWaterAlpha( long bits );
