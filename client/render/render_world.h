#pragma once

struct CSRETRO_OffscreenProof_s;

typedef struct CSRETRO_WorldStats_s
{
	char map[64];
	int surfaces;
	int polys;
	int tris;
	int verts;
	int lightmap_pages;
	int captured;
	int empty_mesh;
} CSRETRO_WorldStats;

typedef struct CSRETRO_WorldEngine_s
{
	void *( *get_model )( int index );
	void ( *gl_bind )( int tmu, unsigned int texnum );
	void ( *gl_cleanup )( int last );
	long ( *get_parm )( int parm, int arg );
	void ( *print )( const char *msg );
	unsigned int ( *crc32 )( const void *buffer, int length );
	int ( *save_file )( const char *filename, const void *data, int len );
} CSRETRO_WorldEngine;

void CSRETRO_World_SetEngine( const CSRETRO_WorldEngine *engine );
void CSRETRO_World_OnModel( void *mod, int create, const unsigned char *buffer );
void CSRETRO_World_OnNewMap( void );
void CSRETRO_World_OnLightmaps( void );
void CSRETRO_World_Release( void );
int CSRETRO_World_Ready( void );
void CSRETRO_World_GetStats( CSRETRO_WorldStats *out );
void CSRETRO_World_Draw( const float *vieworg, const float *viewangles, float fov_x, float fov_y );
