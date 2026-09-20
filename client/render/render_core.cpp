#include "csretro_render.h"
#include "render_backend.h"
#include "render_world.h"
#include "render_scene.h"
#include "render_sprite.h"
#include "render_studio.h"

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
static int s_sprite_logged = 0;
static int s_normal_crc_logged = 0;
static int s_tent_proof_logged = 0;
static int s_tent_seen = 0;
static int s_nodepth_try = 0;
static int s_studio_crc_logged = 0;
static int s_follow_crc_logged = 0;
static int s_follow_detail_logged = 0;
static char s_proof_map[64];
static float s_probe_start = 0.0f;
static int s_probe_step = 0;
static int s_probe_ak = 0;

static void ResetSpriteProof( void )
{
	s_sprite_logged = 0;
	s_normal_crc_logged = 0;
	s_tent_proof_logged = 0;
	s_tent_seen = 0;
	s_nodepth_try = 0;
	s_studio_crc_logged = 0;
	s_follow_crc_logged = 0;
	s_follow_detail_logged = 0;
	CSRETRO_Sprite_ResetDump();
	CSRETRO_Sprite_SetNoDepth( 0 );
}

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
	ResetSpriteProof();
	s_proof_map[0] = 0;
	CSRETRO_Scene_Clear();
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
	ResetSpriteProof();
	gEngfuncs.Con_Printf( "CS Retro: renderer vidinit (FBO rebuilt on next probe)\n" );
}

void CSRETRO_Renderer_Shutdown( void )
{
	CSRETRO_World_Release();
	CSRETRO_Scene_Clear();
	CSRETRO_Backend_Shutdown();
	s_backend_ok = 0;
	s_inited = 0;
	s_proof_logged = 0;
	ResetSpriteProof();
	gEngfuncs.Con_Printf( "CS Retro: renderer shutdown\n" );
}

void CSRETRO_Renderer_OnNewMap( void )
{
	EnsureEngine();
	CSRETRO_World_OnNewMap();
	CSRETRO_Backend_AllowDump();
	s_proof_logged = 0;
	ResetSpriteProof();
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

void CSRETRO_Renderer_ClearScene( void )
{
	// Additive: Xash already emptied tr.draw_list. Mirror has no ownership.
	CSRETRO_Scene_Clear();
}

void CSRETRO_Renderer_AddEntity( int type, struct cl_entity_s *ent )
{
	if( !ProbeEnabled() )
		return;
	CSRETRO_Scene_Add( type, ent );
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
		CSRETRO_OffscreenProof world_proof;
		CSRETRO_SceneStats scene;
		CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y );
		memset( &world_proof, 0, sizeof( world_proof ) );
		CSRETRO_Backend_SampleProof( &world_proof );
		CSRETRO_Backend_PrepareImmediateDraw();
		CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
		CSRETRO_Sprite_SetNoDepth( s_nodepth_try == 1 );
		CSRETRO_Sprite_DrawList( org, ang, &scene );
		CSRETRO_Sprite_SetNoDepth( 0 );
		{
			CSRETRO_OffscreenProof after_sprites;
			memset( &after_sprites, 0, sizeof( after_sprites ) );
			CSRETRO_Backend_SampleProof( &after_sprites );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			CSRETRO_Studio_DrawList( &scene );
			{
				CSRETRO_OffscreenProof after_studio;
				memset( &after_studio, 0, sizeof( after_studio ) );
				CSRETRO_Backend_SampleProof( &after_studio );
				CSRETRO_Backend_PrepareImmediateDraw();
				CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
				CSRETRO_Studio_DrawFollow( &scene );
				CSRETRO_World_GetStats( &st );
				dump = s_dump && s_dump->value != 0.0f;
				memset( &proof, 0, sizeof( proof ) );
				CSRETRO_Backend_EndOffscreen( &proof, 1 );
				if( scene.follow_drawn > 0 && s_follow_crc_logged != 1 )
				{
					int fdiffer = after_studio.crc != proof.crc ? 1 : 0;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen FOLLOW crc before=%08x after=%08x differ=%i drawn=%i\n",
						after_studio.crc, proof.crc, fdiffer, scene.follow_drawn );
					if( fdiffer )
					{
						s_follow_crc_logged = 1;
						gEngfuncs.Con_Printf(
							"CS Retro: offscreen FOLLOW proof before_crc=%08x after_crc=%08x differ=1 drawn=%i\n",
							after_studio.crc, proof.crc, scene.follow_drawn );
					}
					else if( s_follow_crc_logged == 0 )
						s_follow_crc_logged = -1;
				}
			}
			if( scene.studio_attempted > 0 && s_studio_crc_logged != 1 )
			{
				int differ = after_sprites.crc != proof.crc ? 1 : 0;
				if( s_studio_crc_logged == 0 )
				{
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen studio crc sprites=%08x full=%08x differ=%i attempted=%i drawn=%i\n",
						after_sprites.crc, proof.crc, differ,
						scene.studio_attempted, scene.studio_drawn );
					s_studio_crc_logged = differ ? 1 : -1;
					if( differ )
						gEngfuncs.Con_Printf(
							"CS Retro: offscreen studio proof sprites_crc=%08x full_crc=%08x differ=1 drawn=%i\n",
							after_sprites.crc, proof.crc, scene.studio_drawn );
				}
				else if( differ )
				{
					s_studio_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen studio proof sprites_crc=%08x full=%08x differ=1 drawn=%i\n",
						after_sprites.crc, proof.crc, scene.studio_drawn );
				}
			}
		}
		proof.draw_executed = st.captured && st.tris > 0;

		if( !s_proof_logged || strncmp( s_proof_map, st.map, sizeof( s_proof_map ) ) != 0 )
		{
			strncpy( s_proof_map, st.map, sizeof( s_proof_map ) - 1 );
			s_proof_logged = 1;
			s_sprite_logged = 0;
			LogProof( &st, &proof );
			if( dump && gRenderAPI.pfnSaveFile && proof.target_ok )
			{
				gEngfuncs.Con_Printf( "CS Retro: offscreen dump requested (crc=%08x, nonempty=%i). Manual: r_csretro_offscreen_dump 1\n",
					proof.crc, proof.nonempty_pixels );
			}
		}
		if( !s_sprite_logged )
		{
			s_sprite_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: TempEnt sprite mirrored: %i drawn: %i\n",
				scene.tent_sprite, scene.tent_drawn );
			gEngfuncs.Con_Printf(
				"CS Retro: Normal sprite mirrored: %i drawn: %i\n",
				scene.normal_sprite, scene.normal_drawn );
			gEngfuncs.Con_Printf(
				"CS Retro: Studio classified: %i local: %i follow: %i viewmodel: %i preview: %i attempted: %i drawn: %i (events off, player=C)\n",
				scene.studio, scene.studio_local, scene.studio_follow,
				scene.studio_viewmodel, scene.studio_preview,
				scene.studio_attempted, scene.studio_drawn );
			gEngfuncs.Con_Printf(
				"CS Retro: FOLLOW nonplayer_parent=%i player_parent=%i missing_parent=%i drawn=%i deferred_player=%i\n",
				scene.follow_nonplayer_parent, scene.follow_player_parent,
				scene.follow_missing_parent, scene.follow_drawn, scene.follow_deferred_player );
			gEngfuncs.Con_Printf(
				"CS Retro: brush: %i strategy=deferred\n",
				scene.brush );
		}
		if( !s_follow_detail_logged
			&& ( scene.studio_follow > 0 || scene.follow_nonplayer_parent > 0
				|| scene.follow_player_parent > 0 || scene.follow_missing_parent > 0
				|| scene.follow_drawn > 0 || scene.follow_deferred_player > 0 ) )
		{
			s_follow_detail_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: FOLLOW nonplayer_parent=%i player_parent=%i missing_parent=%i drawn=%i deferred_player=%i\n",
				scene.follow_nonplayer_parent, scene.follow_player_parent,
				scene.follow_missing_parent, scene.follow_drawn, scene.follow_deferred_player );
		}
		if( scene.normal_drawn > 0 && s_normal_crc_logged != 1 )
		{
			int differ = world_proof.crc != proof.crc ? 1 : 0;
			if( s_normal_crc_logged == 0 )
			{
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen sprite crc world=%08x full=%08x differ=%i view=%.0f %.0f %.0f\n",
					world_proof.crc, proof.crc, differ,
					org[0], org[1], org[2] );
				if( differ )
				{
					s_normal_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen sprite proof world_crc=%08x full_crc=%08x differ=1 normal_drawn=%i\n",
						world_proof.crc, proof.crc, scene.normal_drawn );
				}
				else
				{
					s_normal_crc_logged = -1;
					s_nodepth_try = 1;
				}
			}
			else if( s_nodepth_try == 1 )
			{
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen sprite crc world=%08x full=%08x differ=%i nodepth=1\n",
					world_proof.crc, proof.crc, differ );
				s_nodepth_try = 2;
				if( differ )
				{
					s_normal_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen sprite proof world_crc=%08x full_crc=%08x differ=1 normal_drawn=%i nodepth=1\n",
						world_proof.crc, proof.crc, scene.normal_drawn );
				}
			}
			else if( differ )
			{
				s_normal_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen sprite proof world_crc=%08x full_crc=%08x differ=1 normal_drawn=%i\n",
					world_proof.crc, proof.crc, scene.normal_drawn );
			}
		}
		if( !s_tent_seen && scene.tent_sprite > 0 )
		{
			s_tent_seen = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: TempEnt sprite mirrored: %i drawn: %i\n",
				scene.tent_sprite, scene.tent_drawn );
		}
		if( !s_tent_proof_logged && scene.tent_drawn > 0 && world_proof.crc != proof.crc )
		{
			s_tent_proof_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: TempEnt sprite mirrored: %i drawn: %i\n",
				scene.tent_sprite, scene.tent_drawn );
			gEngfuncs.Con_Printf(
				"CS Retro: offscreen sprite proof world_crc=%08x full_crc=%08x differ=1 tent_drawn=%i\n",
				world_proof.crc, proof.crc, scene.tent_drawn );
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
			int px3c = s_probe_seq->value >= 2.0f;
			if( px3c )
			{
				if( s_probe_ak == 0 && elapsed >= 3.0f )
				{
					float ang[3] = { 48.0f, 0.0f, 0.0f };
					s_probe_ak = 1;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 48.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give ak47\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; give weapon_ak47\n" );
				}
				else if( s_probe_ak == 1 && elapsed >= 3.8f )
				{
					s_probe_ak = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq drop ak47\n" );
					gEngfuncs.pfnClientCmd( "drop\n" );
				}
				if( s_probe_step == 0 && elapsed >= 4.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give he\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; give weapon_hegrenade\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 5.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq throw he\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 6.5f )
				{
					s_probe_step = 3;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 8.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give smoke\n" );
					gEngfuncs.pfnClientCmd( "give weapon_smokegrenade\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 9.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq throw smoke\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 10.5f )
				{
					s_probe_step = 6;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 18.0f )
				{
					s_probe_step = 7;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 30.0f )
				{
					s_probe_step = 8;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 34.0f )
				{
					s_probe_step = 9;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( s_probe_step == 0 && elapsed >= 8.0f )
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
