#include "csretro_render.h"
#include "render_backend.h"
#include "render_world.h"
#include "render_scene.h"
#include "render_sprite.h"
#include "render_studio.h"
#include "render_brush.h"
#include "render_bsp_mesh.h"
#include "render_decal.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
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
static int s_brush_crc_logged = 0;
static int s_brush_move_crc_logged = 0;
static int s_efx_crc_logged = 0;
static int s_tri_crc_logged = 0;
static int s_brush_logged = 0;
static int s_special_logged = 0;
static int s_anim_proof_logged = 0;
static int s_anim_crc_logged = 0;
static int s_conv_proof_logged = 0;
static int s_fb_proof_logged = 0;
static int s_water_logged = 0;
static int s_water_crc_logged = 0;
static int s_water_opaque_crc_logged = 0;
static int s_water_alpha_logged = 0;
static int s_water_wave_logged = 0;
static int s_decal_logged = 0;
static int s_decal_drawn_logged = 0;
static int s_decal_crc_logged = 0;
static int s_decal_brush_logged = 0;
static int s_decal_mapchange_logged = 0;
static unsigned int s_decal_brush_xyz_hash = 0;
static int s_decal_brush_xyz_have = 0;
static unsigned int s_water_crc_a = 0;
static unsigned int s_water_alpha_crc_a = 0;
static unsigned int s_water_wave_crc_a = 0;
static float s_water_alpha_seen = -1.0f;
static unsigned int s_anim_crc_a = 0;
static float s_anim_crc_time = 0.0f;
static unsigned int s_brush_rest_crc = 0;
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
	s_brush_crc_logged = 0;
	s_brush_move_crc_logged = 0;
	s_efx_crc_logged = 0;
	s_tri_crc_logged = 0;
	s_brush_logged = 0;
	s_special_logged = 0;
	s_anim_proof_logged = 0;
	s_anim_crc_logged = 0;
	s_conv_proof_logged = 0;
	s_fb_proof_logged = 0;
	s_water_logged = 0;
	s_decal_logged = 0;
	s_decal_drawn_logged = 0;
	s_decal_crc_logged = 0;
	s_decal_brush_logged = 0;
	s_decal_mapchange_logged = 0;
	s_decal_brush_xyz_hash = 0;
	s_decal_brush_xyz_have = 0;
	s_water_crc_logged = 0;
	s_water_opaque_crc_logged = 0;
	s_water_alpha_logged = 0;
	s_water_wave_logged = 0;
	s_water_crc_a = 0;
	s_water_alpha_crc_a = 0;
	s_water_wave_crc_a = 0;
	s_water_alpha_seen = -1.0f;
	s_anim_crc_a = 0;
	s_anim_crc_time = 0.0f;
	s_brush_rest_crc = 0;
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
	CSRETRO_Brush_Release();
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
	CSRETRO_BspMesh_OnNewMap();
	CSRETRO_World_OnNewMap();
	CSRETRO_Brush_OnNewMap();
	CSRETRO_Decal_OnNewMap();
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
	CSRETRO_Brush_OnLightmaps();
}

void CSRETRO_Renderer_OnModel( struct model_s *mod, int create, const unsigned char *buffer )
{
	EnsureEngine();
	CSRETRO_World_OnModel( mod, create, buffer );
	CSRETRO_Brush_OnModel( mod, create );
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

static void RunProbeSeq( void );

static float EffectiveWaterAlpha( void )
{
	if( !GetParm( PARM_WATER_ALPHA, 0 ) )
		return 1.0f;
	return CSRETRO_DecodeWaterAlpha( GetParm( PARM_WATER_ALPHA_VALUE, 0 ) );
}

static void FillWorldMeshContext( CSRETRO_MeshDrawContext *ctx, const float *vieworg )
{
	cl_entity_t *live;

	memset( ctx, 0, sizeof( *ctx ) );
	ctx->time = (float)gEngfuncs.GetClientTime();
	ctx->get_parm = GetParm;
	ctx->bind_textures = 1;
	ctx->water_alpha = EffectiveWaterAlpha();
	if( vieworg )
	{
		ctx->vieworg[0] = vieworg[0];
		ctx->vieworg[1] = vieworg[1];
		ctx->vieworg[2] = vieworg[2];
	}
	live = gEngfuncs.GetEntityByIndex( 0 );
	if( live )
	{
		cl_entity_t snap = *live;
		ctx->entity_frame = snap.curstate.frame;
		ctx->rendercolor[0] = snap.curstate.rendercolor.r;
		ctx->rendercolor[1] = snap.curstate.rendercolor.g;
		ctx->rendercolor[2] = snap.curstate.rendercolor.b;
		ctx->rendermode = snap.curstate.rendermode;
		ctx->wave_scale = snap.curstate.scale;
		ctx->effects = snap.curstate.effects;
	}
}

static void LogBrushSpecial( const CSRETRO_MeshDrawStats *ms )
{
	gEngfuncs.Con_Printf(
		"CS Retro: brush special anim_candidates=%i tex_first=%u tex_last=%u tex_changed=%i alternate_candidates=%i alternate_used=%i random_tiled=%i conveyor_candidates=%i uv_s=%.5f uv_t=%.5f uv_changed=%i fullbright_candidates=%i fullbright_drawn=%i verts=%i builds=%i rebuilds_unchanged=%i geom_unchanged=%i skipped_turb=%i turb_surfaces=%i turb_polys=%i turb_verts=%i\n",
		ms->anim_candidates,
		ms->anim_tex_first,
		ms->anim_tex_last,
		ms->anim_tex_changed,
		ms->alternate_candidates,
		ms->alternate_used,
		ms->random_tile_candidates,
		ms->conveyor_candidates,
		ms->conveyor_s,
		ms->conveyor_t,
		ms->conveyor_uv_changed,
		ms->fullbright_candidates,
		ms->fullbright_drawn,
		ms->verts,
		CSRETRO_BspMesh_BuildCount(),
		ms->rebuilds_unchanged,
		ms->geom_unchanged,
		ms->skipped_turb,
		ms->turb_surfaces,
		ms->turb_polys,
		ms->turb_verts );
}

void CSRETRO_Renderer_Frame( const struct ref_viewpass_s *rvp )
{
	CSRETRO_WorldStats st;
	CSRETRO_OffscreenProof proof;
	int dump;

	if( !rvp )
		return;
	if( !( rvp->flags & RF_DRAW_WORLD ) )
		return;

	EnsureEngine();
	EnsureCvars();
	CSRETRO_ClientTriangles_BeginFrame();
	CSRETRO_BspMesh_BeginFrame();
	CSRETRO_Decal_BeginFrame();
	if( !ProbeEnabled() )
	{
		RunProbeSeq();
		return;
	}

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
		CSRETRO_OffscreenProof world_base_proof;
		CSRETRO_SceneStats scene;
		CSRETRO_MeshDrawContext world_ctx;
		cl_entity_t *world_live;
		float world_scale_before = 0.0f;
		FillWorldMeshContext( &world_ctx, org );
		world_live = gEngfuncs.GetEntityByIndex( 0 );
		if( world_live )
			world_scale_before = world_live->curstate.scale;
		world_ctx.skip_fullbright = 1;
		CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y, &world_ctx );
		memset( &world_base_proof, 0, sizeof( world_base_proof ) );
		CSRETRO_Backend_SampleProof( &world_base_proof );
		if( CSRETRO_World_HasWater() && world_ctx.water_alpha >= 1.0f )
		{
			CSRETRO_OffscreenProof after_water;
			CSRETRO_MeshDrawStats wms;
			world_ctx.water_pass = CSRETRO_WATER_OPAQUE;
			CSRETRO_World_DrawWater( &world_ctx );
			memset( &after_water, 0, sizeof( after_water ) );
			CSRETRO_Backend_SampleProof( &after_water );
			CSRETRO_BspMesh_GetDrawStats( &wms );
			{
				int differ = after_water.crc != world_base_proof.crc ? 1 : 0;
				if( !s_water_opaque_crc_logged )
				{
					s_water_opaque_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: water crc before=%08x after=%08x differ=%i pass=opaque nonempty=%i drawn=%i\n",
						world_base_proof.crc, after_water.crc, differ, after_water.nonempty_pixels, wms.world_water_opaque );
				}
				if( s_water_crc_logged != 1 && differ )
				{
					s_water_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: water pixelproof before=%08x after=%08x differ=1 pass=opaque nonempty=%i\n",
						world_base_proof.crc, after_water.crc, after_water.nonempty_pixels );
				}
			}
			if( s_water_alpha_logged != 1 )
			{
				if( s_water_alpha_seen < 0.0f )
				{
					s_water_alpha_crc_a = after_water.crc;
					s_water_alpha_seen = world_ctx.water_alpha;
				}
			}
			if( s_water_wave_logged != 1 )
			{
				if( !s_water_wave_crc_a && world_ctx.wave_scale == 0.0f )
					s_water_wave_crc_a = after_water.crc;
				else if( s_water_wave_crc_a && world_ctx.wave_scale != 0.0f && after_water.crc != s_water_wave_crc_a && wms.geom_unchanged )
				{
					s_water_wave_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: water wave proof amp0=%08x amp1=%08x differ=1 geom_unchanged=1 scale=%.3f\n",
						s_water_wave_crc_a, after_water.crc, world_ctx.wave_scale );
				}
			}
			s_water_crc_a = after_water.crc;
		}
		else if( CSRETRO_World_HasWater() )
			world_ctx.water_pass = CSRETRO_WATER_SKIP;
		{
			CSRETRO_OffscreenProof before_decals;
			CSRETRO_OffscreenProof after_decals;
			CSRETRO_DecalStats ds;
			unsigned int world_hash_before;
			void *wmod;

			memset( &before_decals, 0, sizeof( before_decals ) );
			CSRETRO_Backend_SampleProof( &before_decals );
			wmod = CSRETRO_World_Model();
			world_hash_before = CSRETRO_Decal_HashSurfaces( wmod );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			CSRETRO_Decal_DrawSurfaces( wmod, world_ctx.rendermode, 0, 0 );
			memset( &after_decals, 0, sizeof( after_decals ) );
			CSRETRO_Backend_SampleProof( &after_decals );
			CSRETRO_Decal_GetStats( &ds );
			if( world_hash_before != CSRETRO_Decal_HashSurfaces( wmod ) )
				ds.live_mutate = 1;
			if( s_decal_crc_logged != 1 && after_decals.crc != before_decals.crc && ds.world_decals_drawn > 0 )
			{
				s_decal_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: world decal pixelproof before=%08x after=%08x differ=1 surfaces=%i decals=%i drawn=%i nonempty=%i\n",
					before_decals.crc, after_decals.crc,
					ds.world_decal_surfaces, ds.world_decals, ds.world_decals_drawn,
					after_decals.nonempty_pixels );
			}
			if( !s_decal_drawn_logged && ds.world_decals_drawn > 0 )
			{
				s_decal_drawn_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: world decals drawn surfaces=%i decals=%i drawn=%i polys=%i fallback=%i mutate=%i dxdy_hashed=1\n",
					ds.world_decal_surfaces, ds.world_decals, ds.world_decals_drawn,
					ds.world_decals_polys, ds.world_fallback, ds.live_mutate );
			}
			if( !s_decal_logged )
			{
				s_decal_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: decal inventory world_decal_surfaces=%i world_decals=%i world_decals_drawn=%i polys=%i fallback=%i spawn=%i transparent_surfaces=%i transparent_decals=%i skipped=%i stale_skipped=%i premult=%i std_blend=%i mutate=%i gl_restore=%i stored_ptrs=%i serial=%i\n",
					ds.world_decal_surfaces, ds.world_decals, ds.world_decals_drawn,
					ds.world_decals_polys, ds.world_fallback, ds.spawn_world_decals,
					ds.transparent_surfaces, ds.transparent_decals, ds.transparent_skipped,
					ds.stale_skipped,
					ds.premultiplied_drawn, ds.standard_blend_drawn,
					ds.live_mutate, ds.gl_restore_ok, ds.stored_decal_ptrs, ds.map_serial );
				if( ds.transparent_decals == 0 )
					gEngfuncs.Con_Printf( "CS Retro: transparent/stencil decals runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT\n" );
			}
			if( !s_decal_mapchange_logged && ds.map_serial > 1 )
			{
				CSRETRO_WorldStats mapst;
				CSRETRO_World_GetStats( &mapst );
				s_decal_mapchange_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: decal mapchange stale_ptrs=0 stored_ptrs=0 serial=%i spawn_decals=%i map=%s\n",
					ds.map_serial, ds.spawn_world_decals, mapst.map[0] ? mapst.map : "?" );
			}
		}
		world_ctx.skip_base = 1;
		world_ctx.skip_fullbright = 0;
		CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y, &world_ctx );
		memset( &world_proof, 0, sizeof( world_proof ) );
		CSRETRO_Backend_SampleProof( &world_proof );
		if( s_fb_proof_logged != 1 && world_base_proof.crc != world_proof.crc )
		{
			CSRETRO_MeshDrawStats ms;
			CSRETRO_BspMesh_GetDrawStats( &ms );
			if( ms.fullbright_drawn > 0 )
			{
				s_fb_proof_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: fullbright proof base_crc=%08x plus_crc=%08x differ=1 drawn=%i candidates=%i\n",
					world_base_proof.crc, world_proof.crc, ms.fullbright_drawn, ms.fullbright_candidates );
			}
		}
		memset( &world_proof, 0, sizeof( world_proof ) );
		CSRETRO_Backend_SampleProof( &world_proof );
		CSRETRO_Backend_PrepareImmediateDraw();
		CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
		CSRETRO_Brush_DrawPass( 1, &scene );
		{
			CSRETRO_OffscreenProof after_opaque;
			CSRETRO_MeshDrawStats ms;
			CSRETRO_DecalStats ds;
			const CSRETRO_BrushMove *mv;
			memset( &after_opaque, 0, sizeof( after_opaque ) );
			CSRETRO_Backend_SampleProof( &after_opaque );
			CSRETRO_BspMesh_GetDrawStats( &ms );
			CSRETRO_Decal_GetStats( &ds );
			mv = CSRETRO_Brush_LastMove();
			if( !s_anim_crc_a && ms.anim_candidates > 0 )
			{
				s_anim_crc_a = after_opaque.crc;
				s_anim_crc_time = (float)gEngfuncs.GetClientTime();
			}
			else if( s_anim_crc_logged != 1 && ms.anim_tex_changed && ms.anim_candidates > 0
				&& after_opaque.crc != s_anim_crc_a
				&& ( (float)gEngfuncs.GetClientTime() - s_anim_crc_time ) >= 0.45f )
			{
				s_anim_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: texture animation pixelproof crc_a=%08x crc_b=%08x differ=1\n",
					s_anim_crc_a, after_opaque.crc );
			}
			if( ds.brush_decals_drawn > 0 && !s_decal_brush_xyz_have )
			{
				s_decal_brush_xyz_hash = ds.hash_after;
				s_decal_brush_xyz_have = 1;
			}
			if( !s_decal_brush_logged && ds.brush_decals_drawn > 0 )
			{
				int moved = mv && mv->happened;
				int xyz_unchanged = 1;
				if( s_decal_brush_xyz_have && ds.hash_after != s_decal_brush_xyz_hash && moved )
					xyz_unchanged = ( ds.live_mutate == 0 );
				gEngfuncs.Con_Printf(
					"CS Retro: brush decal model=%s index=%i exists=1 drawn=%i surfaces=%i decals=%i polys=%i fallback=%i transform_changed=%i xyz_unchanged=%i mutate=%i\n",
					ds.brush_model[0] ? ds.brush_model : "?",
					ds.brush_entity_index, ds.brush_decals_drawn,
					ds.brush_decal_surfaces, ds.brush_decals, ds.brush_decals_polys,
					ds.brush_fallback, moved ? 1 : 0, xyz_unchanged, ds.live_mutate );
				if( moved )
					s_decal_brush_logged = 1;
			}
		}
		CSRETRO_Backend_PrepareImmediateDraw();
		CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
		CSRETRO_Studio_DrawList( &scene );
		CSRETRO_Studio_DrawFollow( &scene );
		{
			CSRETRO_OffscreenProof before_solid_efx;
			CSRETRO_OffscreenProof after_solid_efx;
			memset( &before_solid_efx, 0, sizeof( before_solid_efx ) );
			CSRETRO_Backend_SampleProof( &before_solid_efx );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			if( gRenderAPI.DrawEFX )
				gRenderAPI.DrawEFX( rvp, 0, 1 );
			memset( &after_solid_efx, 0, sizeof( after_solid_efx ) );
			CSRETRO_Backend_SampleProof( &after_solid_efx );
			if( s_efx_crc_logged != 1 && before_solid_efx.crc != after_solid_efx.crc )
			{
				s_efx_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen efx proof before_crc=%08x after_crc=%08x differ=1 pass=solid\n",
					before_solid_efx.crc, after_solid_efx.crc );
			}
		}
		{
			CSRETRO_OffscreenProof before_tri_n;
			CSRETRO_OffscreenProof after_tri_n;
			memset( &before_tri_n, 0, sizeof( before_tri_n ) );
			CSRETRO_Backend_SampleProof( &before_tri_n );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			CSRETRO_ClientTriangles_DrawNormalOnly();
			memset( &after_tri_n, 0, sizeof( after_tri_n ) );
			CSRETRO_Backend_SampleProof( &after_tri_n );
			if( s_tri_crc_logged != 1 && before_tri_n.crc != after_tri_n.crc
				&& gHUD.m_Spectator.OverviewShouldDraw() )
			{
				s_tri_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen tri proof before_crc=%08x after_crc=%08x differ=1 pass=normal\n",
					before_tri_n.crc, after_tri_n.crc );
			}
		}
		CSRETRO_Backend_PrepareImmediateDraw();
		CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
		{
			CSRETRO_OffscreenProof before_trans_brush;
			memset( &before_trans_brush, 0, sizeof( before_trans_brush ) );
			CSRETRO_Backend_SampleProof( &before_trans_brush );
			CSRETRO_Brush_DrawPass( 0, &scene );
			if( s_water_crc_logged != 1 )
			{
				CSRETRO_OffscreenProof after_trans_w;
				CSRETRO_MeshDrawStats wms;
				memset( &after_trans_w, 0, sizeof( after_trans_w ) );
				CSRETRO_Backend_SampleProof( &after_trans_w );
				CSRETRO_BspMesh_GetDrawStats( &wms );
				if( wms.brush_turb_drawn > 0 && after_trans_w.crc != before_trans_brush.crc )
				{
					s_water_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: water pixelproof before=%08x after=%08x differ=1 pass=brush nonempty=%i drawn=%i\n",
						before_trans_brush.crc, after_trans_w.crc, after_trans_w.nonempty_pixels, wms.brush_turb_drawn );
				}
			}
		}
		{
			CSRETRO_MeshDrawStats ms;
			CSRETRO_BspMesh_GetDrawStats( &ms );
			if( !s_special_logged && ( ms.anim_candidates || ms.conveyor_candidates || ms.fullbright_candidates || ms.random_tile_candidates ) )
			{
				s_special_logged = 1;
				LogBrushSpecial( &ms );
			}
			else if( s_special_logged == 1 && ( ms.anim_tex_changed || ms.conveyor_uv_changed || ms.rebuilds_unchanged ) )
			{
				s_special_logged = 2;
				LogBrushSpecial( &ms );
			}
			if( s_anim_proof_logged != 1 && ms.anim_candidates > 0 && ms.anim_tex_changed
				&& ms.geom_unchanged && ms.rebuilds_unchanged )
			{
				s_anim_proof_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: texture animation proof candidates=%i tex_first=%u tex_last=%u changed=1 verts=%i rebuilds_unchanged=1 geom_unchanged=1\n",
					ms.anim_candidates, ms.anim_tex_first, ms.anim_tex_last, ms.verts );
			}
			if( s_conv_proof_logged != 1 && ms.conveyor_candidates > 0 && ms.conveyor_uv_changed && ms.geom_unchanged )
			{
				s_conv_proof_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: conveyor proof candidates=%i uv_s=%.5f uv_t=%.5f uv_changed=1 geom_unchanged=1 verts=%i\n",
					ms.conveyor_candidates, ms.conveyor_s, ms.conveyor_t, ms.verts );
			}
		}
		{
			CSRETRO_OffscreenProof after_brush;
			const CSRETRO_BrushMove *bmove;
			memset( &after_brush, 0, sizeof( after_brush ) );
			CSRETRO_Backend_SampleProof( &after_brush );
			CSRETRO_Scene_GetStats( &scene );
			bmove = CSRETRO_Brush_LastMove();
			if( ( scene.brush_drawn > 0 || scene.brush > 0 ) && s_brush_crc_logged != 1 )
			{
				int bdiffer = world_proof.crc != after_brush.crc ? 1 : 0;
				if( s_brush_crc_logged == 0 )
				{
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen brush crc world=%08x after=%08x differ=%i mirrored=%i drawn=%i opaque=%i trans=%i\n",
						world_proof.crc, after_brush.crc, bdiffer,
						scene.brush, scene.brush_drawn,
						scene.brush_opaque_drawn, scene.brush_trans_drawn );
					s_brush_crc_logged = bdiffer && scene.brush_drawn > 0 ? 1 : -1;
					if( s_brush_crc_logged == 1 )
						gEngfuncs.Con_Printf(
							"CS Retro: offscreen brush proof world_crc=%08x after_crc=%08x differ=1 drawn=%i\n",
							world_proof.crc, after_brush.crc, scene.brush_drawn );
				}
				else if( bdiffer && scene.brush_drawn > 0 )
				{
					s_brush_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen brush proof world_crc=%08x after_crc=%08x differ=1 drawn=%i\n",
						world_proof.crc, after_brush.crc, scene.brush_drawn );
				}
			}
			if( bmove && bmove->happened )
			{
				if( s_brush_move_crc_logged == 0 )
				{
					gEngfuncs.Con_Printf(
						"CS Retro: brush moving index=%i model=%s origin_before=%.1f %.1f %.1f origin_after=%.1f %.1f %.1f angles_before=%.1f %.1f %.1f angles_after=%.1f %.1f %.1f\n",
						bmove->index, bmove->model[0] ? bmove->model : "?",
						bmove->origin_before[0], bmove->origin_before[1], bmove->origin_before[2],
						bmove->origin_after[0], bmove->origin_after[1], bmove->origin_after[2],
						bmove->angles_before[0], bmove->angles_before[1], bmove->angles_before[2],
						bmove->angles_after[0], bmove->angles_after[1], bmove->angles_after[2] );
					s_brush_move_crc_logged = -1;
				}
				if( s_brush_rest_crc && s_brush_rest_crc != after_brush.crc && s_brush_move_crc_logged != 1 )
				{
					s_brush_move_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen brush move proof closed_crc=%08x open_crc=%08x differ=1 index=%i drawn=%i\n",
						s_brush_rest_crc, after_brush.crc, bmove->index, scene.brush_drawn );
				}
			}
			else if( scene.brush_drawn > 0 && !s_brush_rest_crc )
				s_brush_rest_crc = after_brush.crc;
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
		}
		CSRETRO_Sprite_SetNoDepth( s_nodepth_try == 1 );
		CSRETRO_Sprite_DrawList( org, ang, &scene );
		CSRETRO_Sprite_SetNoDepth( 0 );
		{
			CSRETRO_OffscreenProof after_sprites;
			CSRETRO_OffscreenProof after_tri_t;
			CSRETRO_OffscreenProof after_trans_efx;
			memset( &after_sprites, 0, sizeof( after_sprites ) );
			CSRETRO_Backend_SampleProof( &after_sprites );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			CSRETRO_ClientTriangles_DrawTransparentOnly();
			memset( &after_tri_t, 0, sizeof( after_tri_t ) );
			CSRETRO_Backend_SampleProof( &after_tri_t );
			if( s_tri_crc_logged != 1 && after_sprites.crc != after_tri_t.crc
				&& CSRETRO_ClientTriangles_ParticleCount() > 0 )
			{
				s_tri_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen tri proof before_crc=%08x after_crc=%08x differ=1 pass=trans count=%i\n",
					after_sprites.crc, after_tri_t.crc, CSRETRO_ClientTriangles_ParticleCount() );
			}
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			if( gRenderAPI.DrawEFX )
				gRenderAPI.DrawEFX( rvp, 1, 1 );
			memset( &after_trans_efx, 0, sizeof( after_trans_efx ) );
			CSRETRO_Backend_SampleProof( &after_trans_efx );
			if( s_efx_crc_logged != 1 && after_tri_t.crc != after_trans_efx.crc )
			{
				s_efx_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen efx proof before_crc=%08x after_crc=%08x differ=1 pass=trans\n",
					after_tri_t.crc, after_trans_efx.crc );
			}
			if( CSRETRO_World_HasWater() && world_ctx.water_alpha < 1.0f )
			{
				CSRETRO_OffscreenProof before_late;
				CSRETRO_OffscreenProof after_late;
				CSRETRO_MeshDrawStats wms;
				CSRETRO_Backend_PrepareImmediateDraw();
				CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
				FillWorldMeshContext( &world_ctx, org );
				world_ctx.water_pass = CSRETRO_WATER_LATE;
				memset( &before_late, 0, sizeof( before_late ) );
				CSRETRO_Backend_SampleProof( &before_late );
				CSRETRO_World_DrawWater( &world_ctx );
				memset( &after_late, 0, sizeof( after_late ) );
				CSRETRO_Backend_SampleProof( &after_late );
				CSRETRO_BspMesh_GetDrawStats( &wms );
				if( s_water_crc_logged != 1 && after_late.crc != before_late.crc )
				{
					s_water_crc_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: water pixelproof before=%08x after=%08x differ=1 pass=late nonempty=%i\n",
						before_late.crc, after_late.crc, after_late.nonempty_pixels );
				}
				if( s_water_alpha_logged != 1 && s_water_alpha_crc_a && after_late.crc != s_water_alpha_crc_a && wms.geom_unchanged )
				{
					s_water_alpha_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: water alpha pixelproof a=%08x b=%08x differ=1 geom_unchanged=1 alpha=%.3f opaque=%i late=%i base=%i\n",
						s_water_alpha_crc_a, after_late.crc, world_ctx.water_alpha,
						wms.world_water_opaque, wms.world_water_late, wms.world_water_base );
				}
				after_trans_efx = after_late;
			}
			{
				CSRETRO_OffscreenProof after_studio;
				memset( &after_studio, 0, sizeof( after_studio ) );
				after_studio = after_trans_efx;
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
			s_water_logged = 0;
			s_water_crc_logged = 0;
			s_water_opaque_crc_logged = 0;
			s_water_alpha_logged = 0;
			s_water_wave_logged = 0;
			s_water_crc_a = 0;
			s_water_alpha_crc_a = 0;
			s_water_wave_crc_a = 0;
			s_water_alpha_seen = -1.0f;
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
				"CS Retro: brush mirrored: %i opaque_drawn: %i trans_drawn: %i drawn: %i\n",
				scene.brush, scene.brush_opaque_drawn, scene.brush_trans_drawn, scene.brush_drawn );
			s_brush_logged = 1;
		}
		if( !s_water_logged )
		{
			CSRETRO_MeshDrawStats wms;
			cl_entity_t *live_now;
			float scale_after = world_scale_before;
			int live_mutate = 0;
			CSRETRO_World_GetStats( &st );
			CSRETRO_BspMesh_GetDrawStats( &wms );
			live_now = gEngfuncs.GetEntityByIndex( 0 );
			if( live_now )
			{
				scale_after = live_now->curstate.scale;
				if( scale_after != world_scale_before )
					live_mutate = 1;
			}
			s_water_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: water capture map=%s turb_surfaces=%i turb_polys=%i turb_verts=%i skipped_no_polys=%i\n",
				st.map[0] ? st.map : "(none)", st.turb_surfaces, st.turb_polys, st.turb_verts, st.skipped_turb );
			gEngfuncs.Con_Printf(
				"CS Retro: water inventory world_turb=%i brush_turb=%i liquid=%i sides=%i alpha_cap=%i effective=%.3f litwater=%i opaque=%i late=%i base=%i brush_drawn=%i\n",
				st.turb_surfaces, wms.brush_turb_candidates, wms.liquid_models, wms.waterside_candidates,
				wms.alpha_capability, world_ctx.water_alpha, wms.litwater,
				wms.world_water_opaque, wms.world_water_late, wms.world_water_base, wms.brush_turb_drawn );
			gEngfuncs.Con_Printf(
				"CS Retro: water live mutate=%i scale_before=%.3f scale_after=%.3f cache_mutate=%i gl_restore=%i\n",
				live_mutate, world_scale_before, scale_after, wms.water_cache_mutate, wms.gl_restore_ok );
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
	RunProbeSeq();
}

static void RunProbeSeq( void )
{
	if( s_probe_seq && s_probe_seq->value != 0.0f )
	{
		float now = gRenderAPI.pfnTime ? gRenderAPI.pfnTime() : (float)gEngfuncs.GetClientTime();
		if( s_probe_start <= 0.0f )
			s_probe_start = now;
		{
			float elapsed = now - s_probe_start;
			int decalc = s_probe_seq->value >= 8.0f;
			int waterb = !decalc && s_probe_seq->value >= 7.0f;
			int special = !decalc && !waterb && s_probe_seq->value >= 6.0f;
			int tri = !decalc && !waterb && !special && s_probe_seq->value >= 5.0f;
			int efx = !decalc && !waterb && !special && !tri && s_probe_seq->value >= 4.0f;
			int brush = !decalc && !waterb && !special && !efx && s_probe_seq->value >= 3.0f;
			int px3c = !decalc && !waterb && !special && !efx && !brush && s_probe_seq->value >= 2.0f;
			if( decalc )
			{
				if( s_probe_step == 0 && elapsed >= 3.0f )
				{
					float ang[3] = { 18.0f, 0.0f, 0.0f };
					s_probe_step = 1;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 18.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give ak47\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; give weapon_ak47\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 4.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq fire ak47 world\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 5.2f )
				{
					s_probe_step = 3;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 10.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; map cs_assault\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 18.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq assault door here\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_enttools_enable 1; noclip; ent_fire 19 movehere\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 19.5f )
				{
					float ang[3] = { 25.0f, 0.0f, 0.0f };
					s_probe_step = 6;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 25.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give ak47 door\n" );
					gEngfuncs.pfnClientCmd( "give weapon_ak47\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 20.5f )
				{
					s_probe_step = 7;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq fire ak47 door\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 21.6f )
				{
					s_probe_step = 8;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 23.5f )
				{
					s_probe_step = 9;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq assault door use\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_enttools_enable 1; ent_fire 19 use\n" );
				}
				else if( s_probe_step == 9 && elapsed >= 28.0f )
				{
					s_probe_step = 10;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
				}
				else if( s_probe_step == 10 && elapsed >= 38.0f )
				{
					s_probe_step = 11;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 11 && elapsed >= 48.0f )
				{
					s_probe_step = 12;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 12 && elapsed >= 52.0f )
				{
					s_probe_step = 13;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( waterb )
			{
				if( s_probe_step == 0 && elapsed >= 4.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq sv_wateralpha 0.5\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_wateralpha 0.5\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 8.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq sv_wateralpha 1 sv_wateramp 1\n" );
					gEngfuncs.pfnClientCmd( "sv_wateralpha 1; sv_wateramp 1\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 12.0f )
				{
					s_probe_step = 3;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "sv_wateramp 0; map de_torn\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 22.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 32.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; map cs_assault\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 42.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 46.0f )
				{
					s_probe_step = 7;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( special )
			{
				if( s_probe_step == 0 && elapsed >= 3.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 12.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 22.0f )
				{
					s_probe_step = 3;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; map cs_assault\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 32.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 36.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( tri )
			{
				if( s_probe_step == 0 && elapsed >= 6.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 16.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 20.0f )
				{
					s_probe_step = 3;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( efx )
			{
				if( s_probe_step == 0 && elapsed >= 3.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give ak47\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; give weapon_ak47; give weapon_hegrenade\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 4.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq fire ak47\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 4.6f )
				{
					s_probe_step = 3;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 5.5f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq throw he\n" );
					gEngfuncs.pfnClientCmd( "slot4; wait 10; +attack\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 6.5f )
				{
					s_probe_step = 5;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 12.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 20.0f )
				{
					s_probe_step = 7;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 24.0f )
				{
					s_probe_step = 8;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( brush )
			{
				if( s_probe_step == 0 && elapsed >= 8.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; map cs_assault\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 18.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq assault door use\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_enttools_enable 1; ent_fire func_door_rotating use; ent_fire func_door use\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 24.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 36.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 40.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( px3c )
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
