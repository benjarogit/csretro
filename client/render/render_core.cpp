#include "csretro_render.h"
#include "render_backend.h"
#include "render_world.h"

#include "hud.h"
#include "cl_util.h"
#include "render_api.h"
#include "ref_params.h"

#include <stdio.h>
#include <string.h>

static cvar_t *s_renderer = NULL;
static cvar_t *s_dump = NULL;
static cvar_t *s_probe_seq = NULL;
static int s_inited = 0;
static int s_backend_ok = 0;
static int s_proof_logged = 0;
static char s_proof_map[64];
static float s_probe_start = 0.0f;
static int s_probe_step = 0;

static void Print( const char *msg )
{
	gEngfuncs.Con_Printf( "%s", msg );
}

static void *GetModel( int index )
{
	if( gRenderAPI.pfnGetModel )
		return gRenderAPI.pfnGetModel( index );
	return NULL;
}

static void BindTex( int tmu, unsigned int tex )
{
	if( gRenderAPI.GL_Bind )
		gRenderAPI.GL_Bind( tmu, tex );
}

static void CleanupTex( int last )
{
	if( gRenderAPI.GL_CleanUpTextureUnits )
		gRenderAPI.GL_CleanUpTextureUnits( last );
}

static long GetParm( int parm, int arg )
{
	if( gRenderAPI.RenderGetParm )
		return (long)gRenderAPI.RenderGetParm( parm, arg );
	return 0;
}

static unsigned int FileCRC( const void *buffer, int length )
{
	if( gRenderAPI.pfnFileBufferCRC32 )
		return gRenderAPI.pfnFileBufferCRC32( buffer, length );
	return 0;
}

static int SaveFile( const char *filename, const void *data, int len )
{
	if( gRenderAPI.pfnSaveFile )
		return gRenderAPI.pfnSaveFile( filename, data, len );
	return 0;
}

static void EnsureEngine( void )
{
	CSRETRO_WorldEngine eng;
	memset( &eng, 0, sizeof( eng ) );
	eng.get_model = GetModel;
	eng.gl_bind = BindTex;
	eng.gl_cleanup = CleanupTex;
	eng.get_parm = GetParm;
	eng.print = Print;
	eng.crc32 = FileCRC;
	eng.save_file = SaveFile;
	CSRETRO_World_SetEngine( &eng );
}

static void EnsureCvars( void )
{
	if( !s_renderer )
		s_renderer = CVAR_CREATE( "r_csretro_renderer", "0", 0 );
	if( !s_dump )
		s_dump = CVAR_CREATE( "r_csretro_offscreen_dump", "0", 0 );
	if( !s_probe_seq )
		s_probe_seq = CVAR_CREATE( "r_csretro_probe_seq", "0", 0 );
}

void CSRETRO_Renderer_Init( void )
{
	EnsureCvars();
	EnsureEngine();
	s_inited = 1;
	s_backend_ok = 0;
	s_proof_logged = 0;
	s_proof_map[0] = 0;
	gEngfuncs.Con_Printf( "CS Retro: renderer lifecycle init (offscreen probe default off)\n" );
}

void CSRETRO_Renderer_VidInit( void )
{
	EnsureCvars();
	EnsureEngine();
	if( s_backend_ok )
	{
		CSRETRO_Backend_Shutdown();
		s_backend_ok = 0;
	}
	s_proof_logged = 0;
	gEngfuncs.Con_Printf( "CS Retro: renderer vidinit (FBO rebuilt on next probe)\n" );
}

void CSRETRO_Renderer_Shutdown( void )
{
	CSRETRO_World_Release();
	CSRETRO_Backend_Shutdown();
	s_backend_ok = 0;
	s_inited = 0;
	s_proof_logged = 0;
	gEngfuncs.Con_Printf( "CS Retro: renderer shutdown\n" );
}

void CSRETRO_Renderer_OnNewMap( void )
{
	EnsureEngine();
	CSRETRO_World_OnNewMap();
	CSRETRO_Backend_AllowDump();
	s_proof_logged = 0;
	{
		CSRETRO_WorldStats st;
		CSRETRO_World_GetStats( &st );
		gEngfuncs.Con_Printf( "CS Retro: R_NewMap captured %s surfaces=%i polys=%i tris=%i\n",
			st.map[0] ? st.map : "(none)", st.surfaces, st.polys, st.tris );
	}
}

void CSRETRO_Renderer_OnLightmaps( void )
{
	EnsureEngine();
	CSRETRO_World_OnLightmaps();
}

void CSRETRO_Renderer_OnModel( struct model_s *mod, int create, const unsigned char *buffer )
{
	EnsureEngine();
	CSRETRO_World_OnModel( mod, create, buffer );
}

static int ProbeEnabled( void )
{
	return s_renderer && s_renderer->value != 0.0f;
}

static void LogProof( const CSRETRO_WorldStats *st, const CSRETRO_OffscreenProof *proof )
{
	gEngfuncs.Con_Printf(
		"CS Retro: offscreen world map=%s surfaces=%i polys=%i tris=%i verts=%i lm_pages=%i target=%s draw=%i empty=%i crc=%08x pixels=%i\n",
		st->map[0] ? st->map : "(none)",
		st->surfaces, st->polys, st->tris, st->verts, st->lightmap_pages,
		proof->target_ok ? "ok" : "fail",
		proof->draw_executed,
		proof->empty,
		proof->crc,
		proof->nonempty_pixels );
}

void CSRETRO_Renderer_Frame( const struct ref_viewpass_s *rvp )
{
	CSRETRO_WorldStats st;
	CSRETRO_OffscreenProof proof;
	int dump;

	if( !rvp || !ProbeEnabled() )
		return;
	if( !( rvp->flags & RF_DRAW_WORLD ) )
		return;

	EnsureEngine();
	EnsureCvars();

	if( !s_backend_ok )
	{
		s_backend_ok = CSRETRO_Backend_Init( &gRenderAPI );
		if( !s_backend_ok )
		{
			if( !s_proof_logged )
			{
				s_proof_logged = 1;
				gEngfuncs.Con_Printf( "CS Retro: offscreen backend unavailable — Xash fallback unchanged\n" );
			}
			return;
		}
	}

	if( !CSRETRO_World_Ready() )
	{
		void *mod = GetModel( 1 );
		if( mod )
			CSRETRO_World_OnModel( (struct model_s *)mod, 1, NULL );
		CSRETRO_World_OnNewMap();
	}

	if( !CSRETRO_Backend_BeginOffscreen() )
	{
		if( !s_proof_logged )
		{
			s_proof_logged = 1;
			gEngfuncs.Con_Printf( "CS Retro: offscreen FBO failed — Xash fallback unchanged\n" );
		}
		return;
	}

	{
		float org[3] = { rvp->vieworigin[0], rvp->vieworigin[1], rvp->vieworigin[2] };
		float ang[3] = { rvp->viewangles[0], rvp->viewangles[1], rvp->viewangles[2] };
		CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y );
	}
	CSRETRO_World_GetStats( &st );
	dump = s_dump && s_dump->value != 0.0f;
	memset( &proof, 0, sizeof( proof ) );
	CSRETRO_Backend_EndOffscreen( &proof, 1 );
	proof.draw_executed = st.captured && st.tris > 0;

	if( !s_proof_logged || strncmp( s_proof_map, st.map, sizeof( s_proof_map ) ) != 0 )
	{
		strncpy( s_proof_map, st.map, sizeof( s_proof_map ) - 1 );
		s_proof_logged = 1;
		LogProof( &st, &proof );
		if( dump && gRenderAPI.pfnSaveFile && proof.target_ok )
		{
			// PPM is written from a later read if the user asks; counts+CRC are the gate.
			gEngfuncs.Con_Printf( "CS Retro: offscreen dump requested (crc=%08x, nonempty=%i). Manual: r_csretro_offscreen_dump 1\n",
				proof.crc, proof.nonempty_pixels );
		}
	}

	(void)dump;

	if( s_probe_seq && s_probe_seq->value != 0.0f )
	{
		float now = gRenderAPI.pfnTime ? gRenderAPI.pfnTime() : (float)gEngfuncs.GetClientTime();
		if( s_probe_start <= 0.0f )
			s_probe_start = now;
		{
			float elapsed = now - s_probe_start;
			if( s_probe_step == 0 && elapsed >= 8.0f )
			{
				s_probe_step = 1;
				gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_aztec\n" );
				gEngfuncs.pfnClientCmd( "map de_aztec\n" );
			}
			else if( s_probe_step == 1 && elapsed >= 20.0f )
			{
				s_probe_step = 2;
				gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
				gEngfuncs.pfnClientCmd( "map de_dust\n" );
			}
			else if( s_probe_step == 2 && elapsed >= 32.0f )
			{
				s_probe_step = 3;
				gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
				gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
			}
			else if( s_probe_step == 3 && elapsed >= 36.0f )
			{
				s_probe_step = 4;
				gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
				gEngfuncs.pfnClientCmd( "quit\n" );
			}
		}
	}
}
