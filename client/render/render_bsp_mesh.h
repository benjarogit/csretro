// Shared BSP surface → triangle mesh. Used by world and brush-model cache.
// One builder, two destinations. Does not keep engine surface pointers.
#pragma once

#define XR_MESH_MAX_BATCH 4096

typedef struct CSRETRO_MeshVert_s
{
	float xyz[3];
	float st[2];
	float lm[2];
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
} CSRETRO_MeshBatch;

typedef struct CSRETRO_BspMesh_s
{
	void *model;
	int firstmodelsurface;
	int nummodelsurfaces;
	CSRETRO_MeshVert *verts;
	int vert_count;
	CSRETRO_MeshBatch *batches;
	int batch_count;
	int lightmap_pages;
	int flags_union;
	int surfaces;
	int polys;
	int skipped_turb;
	int skipped_sky;
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
} CSRETRO_MeshDrawStats;

void CSRETRO_BspMesh_Clear( CSRETRO_BspMesh *mesh );
int CSRETRO_BspMesh_Build( CSRETRO_BspMesh *mesh, void *mod );
void CSRETRO_BspMesh_Draw( const CSRETRO_BspMesh *mesh, const CSRETRO_MeshDrawContext *ctx );
void CSRETRO_BspMesh_OnNewMap( void );
void CSRETRO_BspMesh_BeginFrame( void );
void CSRETRO_BspMesh_GetDrawStats( CSRETRO_MeshDrawStats *out );
int CSRETRO_BspMesh_BuildCount( void );
