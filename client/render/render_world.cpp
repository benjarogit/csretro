#include "render_world.h"
#include "render_backend.h"
#include "render_bsp_mesh.h"
#include "render_xash_brush.h"

#include <string.h>

static CSRETRO_WorldEngine s_eng;
static CSRETRO_BspMesh s_world;
static void *s_pending = NULL;
static CSRETRO_WorldStats s_stats;
static int s_logged_empty = 0;

static int IsWorldModel( const xr_model_t *m )
{
	if( !m || m->type != 0 )
		return 0;
	if( m->flags & XR_MODEL_WORLD )
		return 1;
	if( m->name[0] == 'm' && strstr( m->name, ".bsp" ) )
		return 1;
	return 0;
}

static void Log( const char *msg )
{
	if( s_eng.print )
		s_eng.print( msg );
}

static void FillStats( const xr_model_t *mod )
{
	memset( &s_stats, 0, sizeof( s_stats ) );
	if( mod )
		strncpy( s_stats.map, mod->name, sizeof( s_stats.map ) - 1 );
	s_stats.surfaces = s_world.surfaces;
	s_stats.polys = s_world.polys;
	s_stats.tris = s_world.vert_count / 3;
	s_stats.verts = s_world.vert_count;
	s_stats.lightmap_pages = s_world.lightmap_pages;
	s_stats.captured = s_world.captured;
	s_stats.empty_mesh = !s_world.captured;
	s_logged_empty = 0;
}

void CSRETRO_World_SetEngine( const CSRETRO_WorldEngine *engine )
{
	if( engine )
		s_eng = *engine;
	else
		memset( &s_eng, 0, sizeof( s_eng ) );
}

void CSRETRO_World_Release( void )
{
	CSRETRO_BspMesh_Clear( &s_world );
	s_pending = NULL;
	memset( &s_stats, 0, sizeof( s_stats ) );
	s_logged_empty = 0;
}

static int CaptureWorld( xr_model_t *mod )
{
	if( !CSRETRO_BspMesh_Build( &s_world, mod ) )
	{
		FillStats( mod );
		return 0;
	}
	FillStats( mod );
	return s_stats.captured;
}

void CSRETRO_World_OnModel( void *mod, int create, const unsigned char *buffer )
{
	xr_model_t *m = (xr_model_t *)mod;
	(void)buffer;
	if( !IsWorldModel( m ) )
		return;
	if( !create )
	{
		if( s_world.model == mod || s_pending == mod )
			CSRETRO_World_Release();
		s_pending = NULL;
		return;
	}
	s_pending = mod;
}

void CSRETRO_World_OnNewMap( void )
{
	void *mod = s_pending;
	if( !mod && s_eng.get_model )
		mod = s_eng.get_model( 1 );
	if( mod )
		CaptureWorld( (xr_model_t *)mod );
}

void CSRETRO_World_OnLightmaps( void )
{
	if( s_world.model )
		CaptureWorld( (xr_model_t *)s_world.model );
	else if( s_pending )
		CaptureWorld( (xr_model_t *)s_pending );
}

int CSRETRO_World_Ready( void )
{
	return s_stats.captured && s_world.vert_count >= 3;
}

void CSRETRO_World_GetStats( CSRETRO_WorldStats *out )
{
	if( out )
		*out = s_stats;
}

void *CSRETRO_World_Model( void )
{
	return s_world.model;
}

void CSRETRO_World_Draw( const float *vieworg, const float *viewangles, float fov_x, float fov_y )
{
	if( !CSRETRO_World_Ready() || !gXRGL.Begin )
	{
		if( !s_logged_empty )
		{
			s_logged_empty = 1;
			Log( "CS Retro: offscreen world not ready (no captured BSP mesh)\n" );
		}
		return;
	}

	CSRETRO_Backend_ApplyView( vieworg, viewangles, fov_x, fov_y );
	if( gXRGL.Color4f )
		gXRGL.Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	CSRETRO_BspMesh_Draw( &s_world, s_eng.get_parm, 1 );
}
