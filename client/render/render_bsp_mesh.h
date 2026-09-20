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
} CSRETRO_BspMesh;

void CSRETRO_BspMesh_Clear( CSRETRO_BspMesh *mesh );
int CSRETRO_BspMesh_Build( CSRETRO_BspMesh *mesh, void *mod );
void CSRETRO_BspMesh_Draw( const CSRETRO_BspMesh *mesh, long ( *get_parm )( int parm, int arg ), int bind_textures );
