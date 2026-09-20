// Offscreen classic Xash/GoldSrc surface dlights.
// Xash owns allocation, decay, die/radius, visible dynamic LM pass.
// CS Retro: snapshot + read-only evaluator + transient atlas. One API.
#pragma once

struct CSRETRO_BspMesh_s;
struct cl_entity_s;

#define CSRETRO_MAX_DLIGHTS 32

typedef struct CSRETRO_DLightPatch_s
{
	void *model;
	int surface_index;
	int atlas_x;
	int atlas_y;
	int width;
	int height;
	int light_s;
	int light_t;
	int used;
} CSRETRO_DLightPatch;

typedef struct CSRETRO_DLightStats_s
{
	int active;
	int helper_ok;
	int r_dynamic;
	int affected_world;
	int affected_brush;
	int patches;
	int transform_used;
	int dlight_mutate;
	int surface_mutate;
	int mesh_mutate;
	int gl_restore;
	int expired_static;
	int r_dynamic_off_patches;
	int r_dynamic_on_patches;
	unsigned int crc_before;
	unsigned int crc_after;
	int pixel_differ;
	int inventory_logged;
	int key0;
	float radius0;
	unsigned char color0[3];
	float die0;
} CSRETRO_DLightStats;

void CSRETRO_DLight_Init( void );
void CSRETRO_DLight_Shutdown( void );
void CSRETRO_DLight_OnNewMap( void );
void CSRETRO_DLight_OnVidInit( void );
void CSRETRO_DLight_BeginOffscreen( void );
void CSRETRO_DLight_PrepareMesh( const struct CSRETRO_BspMesh_s *mesh, const struct cl_entity_s *entity_or_null, int is_world );
void CSRETRO_DLight_EndOffscreen( void );
void CSRETRO_DLight_NoteWorldCrc( unsigned int crc, int patches );
int CSRETRO_DLight_Lookup( void *model, int surface_index, CSRETRO_DLightPatch *out );
unsigned int CSRETRO_DLight_AtlasTexnum( void );
int CSRETRO_DLight_BlockSize( void );
int CSRETRO_DLight_AtlasSize( void );
void CSRETRO_DLight_GetStats( CSRETRO_DLightStats *out );
int CSRETRO_DLight_PatchCount( void );
