#include "csretro_render.h"
#include "render_backend.h"
#include "render_world.h"
#include "render_scene.h"
#include "render_sprite.h"
#include "render_studio.h"
#include "render_brush.h"
#include "render_bsp_mesh.h"
#include "render_decal.h"
#include "render_dlight.h"
#include "render_vis.h"
#include "render_trans.h"
#include "render_takeover.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "render_api.h"
#include "ref_params.h"
#include "camera.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

static_assert( offsetof( render_api_t, BuildSurfaceLightmapReadOnly ) == offsetof( render_api_t, DrawEFX ) + sizeof( void * ),
	"v37 prefix: BuildSurfaceLightmapReadOnly must follow DrawEFX" );
static_assert( offsetof( render_api_t, ResolveSurfaceTextureReadOnly ) == offsetof( render_api_t, BuildSurfaceLightmapReadOnly ) + sizeof( void * ),
	"v37 prefix: ResolveSurfaceTextureReadOnly must follow BuildSurfaceLightmapReadOnly" );
static_assert( offsetof( render_api_t, RunViewmodelEventsOnce ) == offsetof( render_api_t, ResolveSurfaceTextureReadOnly ) + sizeof( void * ),
	"v37 prefix: RunViewmodelEventsOnce must be the tail slot after ResolveSurfaceTextureReadOnly" );
static_assert( offsetof( render_api_t, PrepareCurrentFrameVis ) == offsetof( render_api_t, RunViewmodelEventsOnce ) + sizeof( void * ),
	"v37 prefix: PrepareCurrentFrameVis must be the tail slot after RunViewmodelEventsOnce" );
static_assert( offsetof( render_api_t, GetEntityRenderInfoReadOnly ) == offsetof( render_api_t, PrepareCurrentFrameVis ) + sizeof( void * ),
	"v37 prefix: GetEntityRenderInfoReadOnly must be the tail slot after PrepareCurrentFrameVis" );
static_assert( offsetof( render_api_t, PrepareCustomFrame ) == offsetof( render_api_t, GetEntityRenderInfoReadOnly ) + sizeof( void * ),
	"v37 prefix: PrepareCustomFrame must follow GetEntityRenderInfoReadOnly" );
static_assert( offsetof( render_api_t, FinalizeCustomFrame ) == offsetof( render_api_t, PrepareCustomFrame ) + sizeof( void * ),
	"v37 prefix: FinalizeCustomFrame must follow PrepareCustomFrame" );
static_assert( offsetof( render_api_t, CustomFrameFogPre ) == offsetof( render_api_t, FinalizeCustomFrame ) + sizeof( void * ),
	"v37 prefix: CustomFrameFogPre must follow FinalizeCustomFrame" );
static_assert( offsetof( render_api_t, CustomFrameFogPost ) == offsetof( render_api_t, CustomFrameFogPre ) + sizeof( void * ),
	"v37 prefix: CustomFrameFogPost must follow CustomFrameFogPre" );
static_assert( offsetof( render_api_t, CustomFrameExtraUpdate ) == offsetof( render_api_t, CustomFrameFogPost ) + sizeof( void * ),
	"v37 prefix: CustomFrameExtraUpdate must follow CustomFrameFogPost" );

static cvar_t *s_renderer = NULL;
static cvar_t *s_dump = NULL;
static cvar_t *s_probe_seq = NULL;
static cvar_t *s_force_fault = NULL;
static int s_inited = 0;
static int s_backend_ok = 0;
static int s_proof_logged = 0;
static int s_px6a_logged = 0;
static int s_px6a_last_fw = 0;
static int s_px6a_last_fh = 0;
static int s_sprite_logged = 0;
static int s_normal_crc_logged = 0;
static int s_tent_proof_logged = 0;
static int s_tent_seen = 0;
static int s_nodepth_try = 0;
static int s_studio_crc_logged = 0;
static int s_player_crc_logged = 0;
static int s_player_hash_logged = 0;
static int s_local_proof_logged = 0;
static int s_shadow_proof_logged = 0;
static int s_vm_crc_logged = 0;
static int s_vm_hash_logged = 0;
static int s_vm_depth_logged = 0;
static int s_vm_gate_logged = 0;
static int s_vm_molotov_logged = 0;
static int s_vm_cvar_logged = 0;
static int s_vm_third_logged = 0;
static int s_vm_dead_logged = 0;
static int s_vm_knife_logged = 0;
static int s_vm_event_claim_logged = 0;
static int s_vm_event_stress_logged = 0;
static int s_vm_event_reject_logged = 0;
static int s_vm_event_wick_logged = 0;
static char s_vm_last_model[64];
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
static int s_sky_crc_logged = 0;
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
static int s_rnd_inv_logged = 0;
static int s_rnd_px_logged = 0;
static int s_rnd_stable_logged = 0;
static unsigned int s_rnd_hash[3];
static float s_rnd_hash_t0 = 0.0f;
static int s_rnd_hash_n = 0;
static int s_rnd_mapchange_logged = 0;
static int s_rnd_map_serial = 0;
static char s_proof_map[64];
static float s_probe_start = 0.0f;
static int s_probe_step = 0;
static float s_probe_mark = 0.0f;
static int s_probe_ak = 0;

static void ResetSpriteProof( void )
{
	s_sprite_logged = 0;
	s_normal_crc_logged = 0;
	s_tent_proof_logged = 0;
	s_tent_seen = 0;
	s_nodepth_try = 0;
	s_studio_crc_logged = 0;
	s_player_crc_logged = 0;
	s_player_hash_logged = 0;
	s_local_proof_logged = 0;
	s_shadow_proof_logged = 0;
	s_follow_crc_logged = 0;
	CSRETRO_Studio_ResetPlayerProof();
	CSRETRO_Studio_ResetViewmodelProof();
	s_vm_crc_logged = 0;
	s_vm_hash_logged = 0;
	s_vm_depth_logged = 0;
	s_vm_gate_logged = 0;
	s_vm_molotov_logged = 0;
	s_vm_event_claim_logged = 0;
	s_vm_event_stress_logged = 0;
	s_vm_event_reject_logged = 0;
	s_vm_event_wick_logged = 0;
	s_vm_cvar_logged = 0;
	s_vm_third_logged = 0;
	s_vm_dead_logged = 0;
	s_vm_knife_logged = 0;
	s_vm_last_model[0] = '\0';
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
	s_rnd_inv_logged = 0;
	s_rnd_px_logged = 0;
	s_rnd_stable_logged = 0;
	s_rnd_hash[0] = s_rnd_hash[1] = s_rnd_hash[2] = 0;
	s_rnd_hash_t0 = 0.0f;
	s_rnd_hash_n = 0;
	s_rnd_mapchange_logged = 0;
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
	if( !s_force_fault )
		s_force_fault = CVAR_CREATE( "r_csretro_takeover_force_fault", "0", 0 );
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
	CSRETRO_DLight_Init();
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
	CSRETRO_DLight_OnVidInit();
	s_proof_logged = 0;
	ResetSpriteProof();
	gEngfuncs.Con_Printf( "CS Retro: renderer vidinit (FBO rebuilt on next probe)\n" );
}

void CSRETRO_Renderer_Shutdown( void )
{
	gHUD.m_Spectator.ForceOverviewGlClearRestore();
	CSRETRO_World_Release();
	CSRETRO_Brush_Release();
	CSRETRO_Scene_Clear();
	CSRETRO_DLight_Shutdown();
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
	CSRETRO_Backend_ResetGLErrorLog();
	CSRETRO_BspMesh_OnNewMap();
	CSRETRO_World_OnNewMap();
	CSRETRO_Brush_OnNewMap();
	CSRETRO_Decal_OnNewMap();
	CSRETRO_DLight_OnNewMap();
	CSRETRO_Vis_OnNewMap();
	CSRETRO_Backend_AllowDump();
	s_proof_logged = 0;
	ResetSpriteProof();
	{
		CSRETRO_WorldStats st;
		CSRETRO_World_GetStats( &st );
		s_rnd_map_serial++;
		gEngfuncs.Con_Printf( "CS Retro: R_NewMap captured %s surfaces=%i polys=%i tris=%i\n",
			st.map[0] ? st.map : "(none)", st.surfaces, st.polys, st.tris );
		if( s_rnd_map_serial > 1 )
			gEngfuncs.Con_Printf(
				"CS Retro: random tiled mapchange serial=%i stale_indices=0 mesh_rebuilt=1 map=%s\n",
				s_rnd_map_serial, st.map[0] ? st.map : "?" );
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
	if( CSRETRO_Vis_Valid() )
	{
		ctx->surf_mask = CSRETRO_Vis_SurfMask();
		ctx->surf_mask_bytes = CSRETRO_Vis_SurfMaskBytes();
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

static void LogRandomTiled( const CSRETRO_MeshDrawStats *ms )
{
	gEngfuncs.Con_Printf(
		"CS Retro: random tiled inventory candidates=%i resolved=%i fallback=%i distinct_selected_frames=%i selected_differs_from_texinfo_base=%i selection_hash=%08x variant0_count=%i variant1_count=%i variant2_count=%i variant3_count=%i variant4_count=%i helper=%i geom_unchanged=%i rebuilds_unchanged=%i verts=%i\n",
		ms->random_tile_candidates,
		ms->random_resolved,
		ms->random_fallback,
		ms->random_distinct_frames,
		ms->random_differs_from_base,
		ms->random_selection_hash,
		ms->random_variant[0],
		ms->random_variant[1],
		ms->random_variant[2],
		ms->random_variant[3],
		ms->random_variant[4],
		gRenderAPI.ResolveSurfaceTextureReadOnly ? 1 : 0,
		ms->geom_unchanged,
		ms->rebuilds_unchanged,
		ms->verts );
}

int CSRETRO_Renderer_Frame( const struct ref_viewpass_s *rvp )
{
	CSRETRO_WorldStats st;
	CSRETRO_OffscreenProof proof;
	int dump;
	int mode;
	int takeover = 0;
	int efx_draw_only = 1;
	int committed = 0;
	int present_ok = 0;
	int reject = CSRETRO_TAKEOVER_OK;
	CSRETRO_TakeoverProof tp;
	csretro_custom_frame_info_t cfi;

	memset( &tp, 0, sizeof( tp ) );
	memset( &cfi, 0, sizeof( cfi ) );

	if( !rvp )
		return 0;

	EnsureEngine();
	EnsureCvars();
	mode = CSRETRO_Renderer_Mode();
	tp.mode = mode;

	if( mode == 0 )
	{
		RunProbeSeq();
		return 0;
	}

	// Mode 1 diagnostic still requires RF_DRAW_WORLD like before.
	if( mode == 1 && !( rvp->flags & RF_DRAW_WORLD ) )
		return 0;

	CSRETRO_ClientTriangles_BeginFrame();
	CSRETRO_BspMesh_BeginFrame();
	CSRETRO_Decal_BeginFrame();
	CSRETRO_Takeover_ResetProof();

	if( mode == 2 )
	{
		if( !CSRETRO_Takeover_Eligible( rvp, &reject ) )
		{
			tp.eligible = 0;
			tp.reject_reason = reject;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			/* Latch means: this next frame is Xash; then allow Mode-2 retry. */
			if( reject == CSRETRO_TAKEOVER_REJECT_FAULT_LATCH )
				CSRETRO_Takeover_ClearFault();
			RunProbeSeq();
			return 0;
		}
		tp.eligible = 1;
		takeover = 1;
		efx_draw_only = 0;
	}

	// Events before BeginOffscreen. FBO failure still leaves the claimed
	// Xash event pass in place and returns to visible R_RenderScene.
	if( !( rvp->flags & RF_ONLY_CLIENTDRAW ) )
	{
		CSRETRO_StudioViewmodelProof evp;
		CSRETRO_Studio_ClaimViewmodelEvents( rvp );
		CSRETRO_Studio_GetViewmodelProof( &evp );
		if( evp.event_claimed && !s_vm_event_claim_logged && evp.event_first_rc == 1 )
		{
			s_vm_event_claim_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: viewmodel events claim client_claims=1 first_rc=1 event_impl_runs=1 currententity_restore=%i gl_restore=%i attach_a=%08x attach_b=%08x attach_c=%08x b_eq_c=%i\n",
				evp.event_currententity_restore, evp.event_gl_restore,
				evp.attach_hash_a, evp.attach_hash_b, evp.attach_hash_c, evp.attach_b_eq_c );
		}
		if( evp.event_claimed && !s_vm_event_stress_logged && evp.event_second_rc == -1 )
		{
			s_vm_event_stress_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: viewmodel events double-call first=%i second=-1 attach_b_eq_c=%i\n",
				evp.event_first_rc, evp.attach_b_eq_c );
		}
		if( evp.event_claimed && evp.event_first_rc == 0 && !s_vm_event_reject_logged )
		{
			s_vm_event_reject_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: viewmodel events client claim rejected first_rc=0 event_impl_runs=0 eligible=%i reason=%i\n",
				evp.event_eligible, evp.event_reject_reason );
		}
	}

	CSRETRO_Vis_Prepare( rvp );
	CSRETRO_Vis_FeedEfrags();
	CSRETRO_Trans_ClassifyScene( rvp->vieworigin );

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
			if( takeover )
			{
				tp.reject_reason = CSRETRO_TAKEOVER_REJECT_BACKEND;
				tp.return_code = 0;
				CSRETRO_Takeover_NoteProof( &tp );
			}
			RunProbeSeq();
			return 0;
		}
	}

	if( !CSRETRO_World_Ready() )
	{
		void *mod = GetModel( 1 );
		if( mod )
			CSRETRO_World_OnModel( (struct model_s *)mod, 1, NULL );
		CSRETRO_World_OnNewMap();
	}

	// --- PRE-COMMIT (Mode 2): no stateful advances yet ---
	if( takeover )
	{
		tp.viewport_w = rvp->viewport[2];
		tp.viewport_h = rvp->viewport[3];
		if( CSRETRO_Scene_HasAlias() )
		{
			tp.reject_reason = CSRETRO_TAKEOVER_REJECT_ALIAS;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			RunProbeSeq();
			return 0;
		}
		if( !CSRETRO_Backend_TakeoverPresentCapable() )
		{
			tp.reject_reason = CSRETRO_TAKEOVER_REJECT_PRESENT;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			RunProbeSeq();
			return 0;
		}
		if( !gRenderAPI.PrepareCustomFrame || !gRenderAPI.FinalizeCustomFrame )
		{
			tp.reject_reason = CSRETRO_TAKEOVER_REJECT_PREPARE;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			RunProbeSeq();
			return 0;
		}
		if( !CSRETRO_Backend_EnsureTakeoverTarget( tp.viewport_w, tp.viewport_h ) )
		{
			tp.reject_reason = CSRETRO_TAKEOVER_REJECT_FBO;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			RunProbeSeq();
			return 0;
		}
		CSRETRO_Backend_TakeoverSize( &tp.fbo_w, &tp.fbo_h );
		if( tp.fbo_w != tp.viewport_w || tp.fbo_h != tp.viewport_h )
		{
			tp.reject_reason = CSRETRO_TAKEOVER_REJECT_FBO;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			RunProbeSeq();
			return 0;
		}
		tp.preflight_ok = 1;
		/* Drain stale GL errors only when dumping — play path skips the sync. */
		if( s_dump && s_dump->value != 0.0f )
			(void)CSRETRO_Backend_CheckGL( "mode2-preflight" );

		// --- TAKEOVER COMMITTED: no same-frame Xash fallback ---
		if( !gRenderAPI.PrepareCustomFrame( rvp, &cfi ) )
		{
			tp.reject_reason = CSRETRO_TAKEOVER_REJECT_PREPARE;
			tp.return_code = 0;
			CSRETRO_Takeover_NoteProof( &tp );
			RunProbeSeq();
			return 0;
		}
		committed = 1;
		tp.committed = 1;
		tp.framecount_before = cfi.framecount_before;
		tp.framecount_after = cfi.framecount_after;
		tp.dlight_pushes = cfi.dlight_pushes;
		tp.player_light = cfi.player_light;
		tp.vis_consumed = cfi.vis_consumed;

		if( !CSRETRO_Backend_BeginTakeover( tp.viewport_w, tp.viewport_h ) )
		{
			CSRETRO_Takeover_LatchFault();
			if( gRenderAPI.FinalizeCustomFrame )
				gRenderAPI.FinalizeCustomFrame();
			tp.fault_latched = 1;
			tp.return_code = 1;
			CSRETRO_Takeover_NoteProof( &tp );
			gEngfuncs.Con_Printf( "CS Retro: PX6A BeginTakeover failed after commit — fault latched, return 1\n" );
			RunProbeSeq();
			return 1;
		}
		if( ( s_dump && s_dump->value != 0.0f )
			&& CSRETRO_Backend_CheckGL( "begin_takeover" ) )
		{
			CSRETRO_Takeover_LatchFault();
			tp.fault_latched = 1;
		}
	}
	else if( !CSRETRO_Backend_BeginOffscreen() )
	{
		if( !s_proof_logged )
		{
			s_proof_logged = 1;
			gEngfuncs.Con_Printf( "CS Retro: offscreen FBO failed — Xash fallback unchanged\n" );
		}
		RunProbeSeq();
		return 0;
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
		CSRETRO_DLight_BeginOffscreen();
		/* Full-FBO glReadPixels every frame = Mode-2 lag. Probes set dump=1. */
		CSRETRO_Backend_SetPixelProof( s_dump && s_dump->value != 0.0f );
		if( !s_rnd_px_logged && CSRETRO_Backend_PixelProofEnabled() )
		{
			CSRETRO_WorldStats wst;
			CSRETRO_World_GetStats( &wst );
			if( wst.random_tile_candidates > 0 )
			{
				CSRETRO_OffscreenProof crc_base;
				CSRETRO_OffscreenProof crc_res;
				CSRETRO_MeshDrawContext proof = world_ctx;

				proof.random_only = 1;
				proof.random_force_base = 1;
				proof.skip_fullbright = 1;
				proof.surf_mask = NULL;
				proof.surf_mask_bytes = 0;
				CSRETRO_Backend_PrepareImmediateDraw();
				CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y, &proof );
				memset( &crc_base, 0, sizeof( crc_base ) );
				CSRETRO_Backend_SampleProof( &crc_base );
				proof.random_force_base = 0;
				CSRETRO_Backend_PrepareImmediateDraw();
				CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y, &proof );
				memset( &crc_res, 0, sizeof( crc_res ) );
				CSRETRO_Backend_SampleProof( &crc_res );
				s_rnd_px_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: random tiled pixelproof base=%08x resolved=%08x differ=%i nonempty_base=%i nonempty_resolved=%i candidates=%i\n",
					crc_base.crc, crc_res.crc, crc_base.crc != crc_res.crc ? 1 : 0,
					crc_base.nonempty_pixels, crc_res.nonempty_pixels, wst.random_tile_candidates );
			}
		}
		{
			CSRETRO_OffscreenProof before_sky;
			CSRETRO_OffscreenProof after_sky;
			const csretro_frame_vis_t *vi = CSRETRO_Vis_Info();

			if( takeover && gRenderAPI.CustomFrameFogPre )
			{
				gRenderAPI.CustomFrameFogPre();
				tp.fog_pre++;
			}

			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			if( gXRGL.Disable )
				gXRGL.Disable( 0x0B44 ); /* GL_CULL_FACE — sky winding vs FBO cull */
			if( CSRETRO_Backend_PixelProofEnabled() )
			{
				memset( &before_sky, 0, sizeof( before_sky ) );
				CSRETRO_Backend_SampleProof( &before_sky );
			}
			CSRETRO_Vis_DrawSky();
			if( gXRGL.Enable )
				gXRGL.Enable( 0x0B44 );
			if( CSRETRO_Backend_PixelProofEnabled() )
			{
				CSRETRO_Backend_PrepareImmediateDraw();
				CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
				memset( &after_sky, 0, sizeof( after_sky ) );
				CSRETRO_Backend_SampleProof( &after_sky );
				if( vi && vi->sky_candidates > 0 && !s_sky_crc_logged )
				{
					int differ = before_sky.crc != after_sky.crc ? 1 : 0;
					gEngfuncs.Con_Printf(
						"CS Retro: sky pixelproof before=%08x after=%08x differ=%i candidates=%i drawn=%i nonempty=%i farclip=%.1f sides=%i applyview=1\n",
						before_sky.crc, after_sky.crc, differ,
						vi->sky_candidates, vi->sky_drawn, after_sky.nonempty_pixels,
						vi->farclip, vi->sky_sides_nonempty );
					if( differ && after_sky.nonempty_pixels > 0 && vi->farclip > 0.0f
						&& vi->sky_sides_nonempty > 0 )
					{
						s_sky_crc_logged = 1;
						CSRETRO_Backend_DumpPPM( "csretro_sky.ppm" );
					}
				}
			}
			else if( !s_sky_crc_logged )
				s_sky_crc_logged = -1; /* play path: never spam */
		}
		CSRETRO_World_Draw( org, ang, rvp->fov_x, rvp->fov_y, &world_ctx );
		if( takeover )
		{
			CSRETRO_Backend_SyncTextureUnits();
			(void)CSRETRO_Backend_CheckGL( "after_world" );
		}
		memset( &world_base_proof, 0, sizeof( world_base_proof ) );
		CSRETRO_Backend_SampleProof( &world_base_proof );
		CSRETRO_DLight_NoteWorldCrc( world_base_proof.crc, CSRETRO_DLight_PatchCount() );
		if( takeover && s_dump && s_dump->value != 0.0f
			&& CSRETRO_Backend_CheckGL( "world" ) )
		{
			CSRETRO_Takeover_LatchFault();
			tp.fault_latched = 1;
		}
		if( takeover && gRenderAPI.CustomFrameFogPost )
		{
			gRenderAPI.CustomFrameFogPost();
			tp.fog_post++;
		}
		if( takeover && gRenderAPI.CustomFrameExtraUpdate )
		{
			gRenderAPI.CustomFrameExtraUpdate();
			tp.extra_updates++;
		}
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
		if( takeover )
			CSRETRO_Backend_SyncTextureUnits();
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
		CSRETRO_Backend_SyncTextureUnits();
		CSRETRO_Studio_DrawList( &scene );
		{
			CSRETRO_OffscreenProof before_player;
			CSRETRO_OffscreenProof after_player;
			CSRETRO_StudioPlayerProof pp;
			memset( &before_player, 0, sizeof( before_player ) );
			CSRETRO_Backend_SampleProof( &before_player );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			CSRETRO_Backend_SyncTextureUnits();
			CSRETRO_Studio_DrawPlayers( &scene, rvp );
			memset( &after_player, 0, sizeof( after_player ) );
			CSRETRO_Backend_SampleProof( &after_player );
			CSRETRO_Studio_GetPlayerProof( &pp );
			if( CSRETRO_Backend_PixelProofEnabled()
				&& pp.drawn > 0 && s_player_crc_logged != 1 )
			{
				int differ = before_player.crc != after_player.crc ? 1 : 0;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen player proof before_crc=%08x after_crc=%08x differ=%i candidates=%i drawn=%i info_mutate=%i entity_mutate=%i events=%i shadow_side_draw=%i models_t=%i models_ct=%i distinct=%i a=%s b=%s\n",
					before_player.crc, after_player.crc, differ,
					pp.candidates, pp.drawn, pp.info_mutate, pp.entity_mutate,
					pp.events, pp.shadow_side_draw, pp.models_t, pp.models_ct,
					pp.distinct_models,
					pp.model_a[0] ? pp.model_a : "-",
					pp.model_b[0] ? pp.model_b : "-" );
				if( differ )
					s_player_crc_logged = 1;
			}
			else if( !CSRETRO_Backend_PixelProofEnabled() && s_player_crc_logged == 0 )
				s_player_crc_logged = -1;
			if( pp.candidates > 0 && !s_player_hash_logged && pp.info_before )
			{
				s_player_hash_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: player_info live before=%08x after_offscreen=%08x mutate=%i entity_live_before=%08x after_offscreen=%08x entity_mutate=%i snap_after=%08x\n",
					pp.info_before, pp.info_after_offscreen, pp.info_mutate,
					pp.entity_live_before, pp.entity_live_after_offscreen,
					pp.entity_mutate, pp.entity_snap_after );
			}
			if( !s_local_proof_logged && ( pp.local_hidden_viewentity > 0 || pp.local_drawn > 0 ) )
			{
				s_local_proof_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: local player proof mirrored=%i hidden=%i eligible=%i drawn=%i pixel=%i info_mutate=%i entity_mutate=%i thirdperson=%i firstperson=%i spectator=%i chase=%i ineye=%i\n",
					pp.local_mirrored, pp.local_hidden_viewentity, pp.local_eligible,
					pp.local_drawn, pp.local_pixel, pp.local_info_mutate, pp.local_entity_mutate,
					pp.local_thirdperson, pp.local_firstperson, pp.local_spectator,
					pp.local_chase, pp.local_ineye );
			}
			if( !s_shadow_proof_logged && ( pp.shadow_candidates > 0 || pp.shadow_drawn > 0 ) )
			{
				s_shadow_proof_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: player shadow proof candidates=%i drawn=%i rejected=%i side_draw=%i r_shadows=%i remote_pixel=%i local_pixel=%i follow_shadow=%i\n",
					pp.shadow_candidates, pp.shadow_drawn, pp.shadow_rejected_trace,
					pp.shadow_side_draw, pp.r_shadows_on, pp.remote_shadow_pixel,
					pp.local_shadow_pixel, pp.follow_player_shadow );
			}
		}
		CSRETRO_Backend_PrepareImmediateDraw();
		CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
		CSRETRO_Sprite_DrawSolid( org, ang, &scene );
		CSRETRO_Studio_DrawFollow( &scene );
		if( takeover )
			(void)CSRETRO_Backend_CheckGL( "after_solid_sprites" );
		{
			CSRETRO_OffscreenProof before_solid_efx;
			CSRETRO_OffscreenProof after_solid_efx;
			memset( &before_solid_efx, 0, sizeof( before_solid_efx ) );
			CSRETRO_Backend_SampleProof( &before_solid_efx );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			if( gRenderAPI.DrawEFX )
				gRenderAPI.DrawEFX( rvp, 0, efx_draw_only );
			if( takeover )
				tp.efx_solid++;
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
			if( takeover )
				CSRETRO_ClientTriangles_OwnedNormalPass();
			else
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
			CSRETRO_Sprite_SetNoDepth( s_nodepth_try == 1 );
			CSRETRO_Trans_Draw( org, ang, &scene, rvp );
			if( takeover )
				(void)CSRETRO_Backend_CheckGL( "after_trans" );
			CSRETRO_Sprite_SetNoDepth( 0 );
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
			if( ms.random_tile_candidates > 0 )
			{
				float now = (float)gEngfuncs.GetClientTime();

				if( !s_rnd_inv_logged )
				{
					s_rnd_inv_logged = 1;
					LogRandomTiled( &ms );
				}
				if( s_rnd_hash_n < 3 && ms.random_selection_hash )
				{
					if( s_rnd_hash_n == 0 )
					{
						s_rnd_hash[0] = ms.random_selection_hash;
						s_rnd_hash_t0 = now;
						s_rnd_hash_n = 1;
					}
					else if( s_rnd_hash_n == 1 && ( now - s_rnd_hash_t0 ) >= 0.05f )
					{
						s_rnd_hash[1] = ms.random_selection_hash;
						s_rnd_hash_n = 2;
					}
					else if( s_rnd_hash_n == 2 && ( now - s_rnd_hash_t0 ) >= 1.5f )
					{
						s_rnd_hash[2] = ms.random_selection_hash;
						s_rnd_hash_n = 3;
						s_rnd_stable_logged = 1;
						gEngfuncs.Con_Printf(
							"CS Retro: random tiled stability hash_a=%08x hash_b=%08x hash_c=%08x stable=%i time_delta=%.3f geom_unchanged=%i rebuilds_unchanged=%i\n",
							s_rnd_hash[0], s_rnd_hash[1], s_rnd_hash[2],
							( s_rnd_hash[0] == s_rnd_hash[1] && s_rnd_hash[1] == s_rnd_hash[2] ) ? 1 : 0,
							now - s_rnd_hash_t0, ms.geom_unchanged, ms.rebuilds_unchanged );
					}
				}
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
		CSRETRO_Sprite_SetNoDepth( 0 );
		{
			CSRETRO_OffscreenProof after_sprites;
			CSRETRO_OffscreenProof after_tri_t;
			CSRETRO_OffscreenProof after_trans_efx;
			memset( &after_sprites, 0, sizeof( after_sprites ) );
			CSRETRO_Backend_SampleProof( &after_sprites );
			CSRETRO_Backend_PrepareImmediateDraw();
			CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
			if( takeover )
				CSRETRO_ClientTriangles_OwnedTransparentPass();
			else
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
				gRenderAPI.DrawEFX( rvp, 1, efx_draw_only );
			if( takeover )
			{
				tp.efx_trans++;
				(void)CSRETRO_Backend_CheckGL( "after_efx_trans" );
			}
			memset( &after_trans_efx, 0, sizeof( after_trans_efx ) );
			CSRETRO_Backend_SampleProof( &after_trans_efx );
			if( s_efx_crc_logged != 1 && after_tri_t.crc != after_trans_efx.crc )
			{
				s_efx_crc_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen efx proof before_crc=%08x after_crc=%08x differ=1 pass=trans\n",
					after_tri_t.crc, after_trans_efx.crc );
			}
			{
				CSRETRO_OffscreenProof before_vm;
				CSRETRO_OffscreenProof after_vm;
				CSRETRO_StudioViewmodelProof vp;
				memset( &before_vm, 0, sizeof( before_vm ) );
				CSRETRO_Backend_SampleProof( &before_vm );
				CSRETRO_Backend_PrepareImmediateDraw();
				CSRETRO_Backend_ApplyView( org, ang, rvp->fov_x, rvp->fov_y );
				CSRETRO_Backend_SyncTextureUnits();
				CSRETRO_Studio_DrawViewmodel( rvp );
				memset( &after_vm, 0, sizeof( after_vm ) );
				CSRETRO_Backend_SampleProof( &after_vm );
				CSRETRO_Studio_GetViewmodelProof( &vp );
				if( !s_vm_hash_logged && vp.candidates )
				{
					s_vm_hash_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel live hash before=%08x after_offscreen=%08x live_mutate=%i snap_after=%08x events=%i righthand_mutate=%i\n",
						vp.live_hash_before, vp.live_hash_after, vp.live_mutate,
						vp.snap_hash_after, vp.events, vp.righthand_mutate );
				}
				if( !s_vm_depth_logged && vp.eligible )
				{
					s_vm_depth_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel depth before=%.5f/%.5f during=%.5f/%.5f after=%.5f/%.5f restore=%i gl_restore=%i\n",
						vp.depth_before[0], vp.depth_before[1],
						vp.depth_during[0], vp.depth_during[1],
						vp.depth_after[0], vp.depth_after[1],
						vp.depth_restore, vp.gl_restore );
				}
				if( CSRETRO_Backend_PixelProofEnabled()
					&& vp.drawn_frame > 0 && s_vm_crc_logged != 1 )
				{
					int differ = before_vm.crc != after_vm.crc ? 1 : 0;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen viewmodel proof before_crc=%08x after_crc=%08x differ=%i candidates=%i studio=%i drawn=%i events=%i live_mutate=%i\n",
						before_vm.crc, after_vm.crc, differ,
						vp.candidates, vp.studio_candidates, vp.drawn,
						vp.events, vp.live_mutate );
					if( differ )
						s_vm_crc_logged = 1;
				}
				else if( !CSRETRO_Backend_PixelProofEnabled() && s_vm_crc_logged == 0 )
					s_vm_crc_logged = -1;
				if( vp.model[0] && strncmp( s_vm_last_model, vp.model, sizeof( s_vm_last_model ) ) != 0 )
				{
					strncpy( s_vm_last_model, vp.model, sizeof( s_vm_last_model ) - 1 );
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel weapon model=%s drawn_frame=%i eligible=%i pistol=%i rifle=%i knife=%i he=%i smoke=%i flash=%i molotov=%i\n",
						vp.model, vp.drawn_frame, vp.eligible,
						vp.weapons_pistol, vp.weapons_rifle, vp.weapons_knife,
						vp.weapons_he, vp.weapons_smoke, vp.weapons_flash, vp.weapons_molotov );
				}
				if( !s_vm_gate_logged && vp.candidates )
				{
					s_vm_gate_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel eligibility candidates=%i studio=%i alias=%i eligible=%i drawn_frame=%i drawvm=%i thirdperson=%i health=%i cubemap=%i only_client=%i world=%i viewentity=%i local=%i model=%s\n",
						vp.candidates, vp.studio_candidates, vp.alias_seen, vp.eligible,
						vp.drawn_frame, vp.drawviewmodel, vp.thirdperson, vp.health,
						vp.cubemap, vp.only_clientdraw, vp.draw_world,
						vp.viewentity, vp.local_index,
						vp.model[0] ? vp.model : "-" );
				}
				if( vp.drawviewmodel == 0 && vp.candidates && !s_vm_cvar_logged )
				{
					s_vm_cvar_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel r_drawviewmodel 0 candidate=%i drawn_frame=%i\n",
						vp.candidates, vp.drawn_frame );
				}
				if( vp.drawviewmodel == 1 && vp.drawn_frame > 0 && s_vm_cvar_logged == 1 )
				{
					s_vm_cvar_logged = 2;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel r_drawviewmodel 1 drawn_frame=%i drawn=%i\n",
						vp.drawn_frame, vp.drawn );
				}
				if( vp.thirdperson && vp.candidates && !s_vm_third_logged )
				{
					s_vm_third_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel thirdperson candidate=%i drawn_frame=%i\n",
						vp.candidates, vp.drawn_frame );
				}
				if( vp.health <= 0 && vp.candidates && !s_vm_dead_logged )
				{
					s_vm_dead_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel dead-player candidate=%i drawn_frame=%i health=%i\n",
						vp.candidates, vp.drawn_frame, vp.health );
				}
				if( vp.weapons_knife && vp.drawn > 0 && !s_vm_knife_logged )
				{
					s_vm_knife_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel knife special_flip=%i righthand=%.0f righthand_mutate=%i\n",
						vp.special_flip, vp.righthand_after, vp.righthand_mutate );
				}
				if( vp.studio_candidates && !vp.alias_seen && s_vm_gate_logged == 1 )
				{
					s_vm_gate_logged = 2;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel alias not required by current product content studio=1 alias=0\n" );
				}
				if( !s_vm_molotov_logged && vp.wick_candidate && vp.drawn > 0 )
				{
					s_vm_molotov_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: viewmodel molotov candidate=%i drawn=%i events=%i wick_attempts=%i wick_captures=%i wick_mutate=%i live_mutate=%i\n",
						vp.wick_candidate, vp.drawn, vp.events,
						vp.wick_attempts, vp.wick_captures, vp.wick_mutate, vp.live_mutate );
				}
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
				CSRETRO_DLight_EndOffscreen();
				if( takeover )
				{
					if( gRenderAPI.CustomFrameExtraUpdate )
					{
						gRenderAPI.CustomFrameExtraUpdate();
						tp.extra_updates++;
					}
					CSRETRO_Backend_SampleProof( &proof );
					if( s_force_fault && s_force_fault->value != 0.0f )
					{
						present_ok = 0;
						gEngfuncs.Cvar_SetValue( "r_csretro_takeover_force_fault", 0.0f );
						gEngfuncs.Con_Printf(
							"CS Retro: PX6A postcommit force_fault=1 committed=1 same_frame_return=1\n" );
					}
					else
					{
						present_ok = CSRETRO_Backend_PresentTakeover(
							rvp->viewport[0], rvp->viewport[1],
							rvp->viewport[2], rvp->viewport[3] );
					}
					if( CSRETRO_Backend_CheckGL( "present" ) )
					{
						/* Cert/dump: latch. Play: log only — HE/dlight once
						 * triggered 0x500 and forced a one-frame Xash flash. */
						if( s_dump && s_dump->value != 0.0f )
						{
							CSRETRO_Takeover_LatchFault();
							tp.fault_latched = 1;
						}
					}
					tp.present_ok = present_ok;
					if( !present_ok )
					{
						CSRETRO_Takeover_LatchFault();
						tp.fault_latched = 1;
						gEngfuncs.Con_Printf( "CS Retro: PX6A present failed after commit — fault latched, return 1\n" );
					}
					CSRETRO_Backend_EndTakeover();
					if( gRenderAPI.FinalizeCustomFrame )
						gRenderAPI.FinalizeCustomFrame();
					if( !s_px6a_logged || tp.fbo_w != s_px6a_last_fw || tp.fbo_h != s_px6a_last_fh )
					{
						s_px6a_logged = 1;
						s_px6a_last_fw = tp.fbo_w;
						s_px6a_last_fh = tp.fbo_h;
						gEngfuncs.Con_Printf(
							"CS Retro: PX6A takeover present=%i fbo=%ix%i viewport=%ix%i framecount=%i→%i dlight=%i efx_s=%i efx_t=%i tri_n=%i tri_t=%i extra=%i fog_pre=%i fog_post=%i\n",
							present_ok, tp.fbo_w, tp.fbo_h, tp.viewport_w, tp.viewport_h,
							tp.framecount_before, tp.framecount_after, tp.dlight_pushes,
							tp.efx_solid, tp.efx_trans,
							CSRETRO_ClientTriangles_OwnedNormalCount(),
							CSRETRO_ClientTriangles_OwnedTransparentCount(),
							tp.extra_updates, tp.fog_pre, tp.fog_post );
					}
				}
				else
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
				"CS Retro: Studio classified: %i local: %i follow: %i viewmodel: %i preview: %i attempted: %i drawn: %i player_candidates=%i player_drawn=%i (events off, player=B isolated, viewmodel body after trans efx)\n",
				scene.studio, scene.studio_local, scene.studio_follow,
				scene.studio_viewmodel, scene.studio_preview,
				scene.studio_attempted, scene.studio_drawn,
				scene.studio_player, scene.studio_player_drawn );
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

	if( takeover && committed )
	{
		CSRETRO_StudioViewmodelProof evp_end;
		tp.tri_normal_owned = CSRETRO_ClientTriangles_OwnedNormalCount();
		tp.tri_trans_owned = CSRETRO_ClientTriangles_OwnedTransparentCount();
		tp.return_code = 1;
		CSRETRO_Takeover_NoteProof( &tp );
		CSRETRO_Studio_GetViewmodelProof( &evp_end );
		if( evp_end.event_eligible && evp_end.event_first_rc != 1 )
			CSRETRO_Studio_NoteLostEligibleEventFrame();
		return 1;
	}
	if( mode != 2 && CSRETRO_Takeover_FaultLatched() )
		CSRETRO_Takeover_ClearFault();
	return 0;
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
			int viscert = s_probe_seq->value >= 16.0f;
			int takeoverc = !viscert && s_probe_seq->value >= 15.0f;
			int visc = !viscert && !takeoverc && s_probe_seq->value >= 14.0f;
			int viewmodelc = !viscert && !takeoverc && !visc && s_probe_seq->value >= 12.0f;
			int playerc = !viscert && !takeoverc && !visc && !viewmodelc && s_probe_seq->value >= 11.0f;
			int randomc = !viewmodelc && !playerc && s_probe_seq->value >= 10.0f;
			int dlightc = !viewmodelc && !playerc && !randomc && s_probe_seq->value >= 9.0f;
			int decalc = !viewmodelc && !playerc && !dlightc && !randomc && s_probe_seq->value >= 8.0f;
			int waterb = !viewmodelc && !playerc && !randomc && !dlightc && !decalc && s_probe_seq->value >= 7.0f;
			int special = !viewmodelc && !playerc && !randomc && !dlightc && !decalc && !waterb && s_probe_seq->value >= 6.0f;
			int tri = !viewmodelc && !playerc && !randomc && !dlightc && !decalc && !waterb && !special && s_probe_seq->value >= 5.0f;
			int efx = !viewmodelc && !playerc && !randomc && !dlightc && !decalc && !waterb && !special && !tri && s_probe_seq->value >= 4.0f;
			int brush = !viewmodelc && !playerc && !randomc && !dlightc && !decalc && !waterb && !special && !efx && s_probe_seq->value >= 3.0f;
			int px3c = !viewmodelc && !playerc && !randomc && !dlightc && !decalc && !waterb && !special && !efx && !brush && s_probe_seq->value >= 2.0f;
			if( viscert )
			{
				/* Engine framebuffer shots (reliable under headless gamescope). */
#define PX6A1_SHOT( path ) gEngfuncs.pfnClientCmd( "screenshot " path "\n" )
				if( s_probe_step == 0 && elapsed >= 2.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert renderer 2 sha-ready\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 5.0f )
				{
					float ang[3] = { -20.0f, 130.0f, 0.0f };
					s_probe_step = 2;
					gEngfuncs.SetViewAngles( ang );
					PX6A1_SHOT( "scrshots/px6a1_02_sky.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert sky look\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 8.0f )
				{
					s_probe_step = 3;
					PX6A1_SHOT( "scrshots/px6a1_01_world_hud.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert hud world shot\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 10.0f )
				{
					s_probe_step = 4;
					gEngfuncs.pfnClientCmd( "toggleconsole\n" );
					PX6A1_SHOT( "scrshots/px6a1_03_console.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert console open\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 12.0f )
				{
					s_probe_step = 5;
					gEngfuncs.pfnClientCmd( "toggleconsole\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert console close\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 14.0f )
				{
					s_probe_step = 6;
					gEngfuncs.pfnClientCmd( "+showscores\n" );
					PX6A1_SHOT( "scrshots/px6a1_04_scoreboard.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert scoreboard open\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 16.0f )
				{
					s_probe_step = 7;
					gEngfuncs.pfnClientCmd( "-showscores\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert scoreboard close\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 18.0f )
				{
					s_probe_step = 8;
					gEngfuncs.pfnClientCmd( "chooseteam\n" );
					PX6A1_SHOT( "scrshots/px6a1_05_preview.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert preview chooseteam\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 22.0f )
				{
					s_probe_step = 9;
					gEngfuncs.pfnClientCmd( "jointeam 2; joinclass 1\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert preview leave\n" );
				}
				else if( s_probe_step == 9 && elapsed >= 26.0f )
				{
					s_probe_step = 10;
					gEngfuncs.pfnClientCmd( "give weapon_glock18; weapon_glock18\n" );
					PX6A1_SHOT( "scrshots/px6a1_06_pistol.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert viewmodel pistol\n" );
				}
				else if( s_probe_step == 10 && elapsed >= 28.0f )
				{
					s_probe_step = 11;
					gEngfuncs.pfnClientCmd( "give weapon_ak47; weapon_ak47; +attack\n" );
					PX6A1_SHOT( "scrshots/px6a1_07_ak.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert ak fire\n" );
				}
				else if( s_probe_step == 11 && elapsed >= 31.0f )
				{
					s_probe_step = 12;
					gEngfuncs.pfnClientCmd( "-attack; give weapon_knife; weapon_knife\n" );
					PX6A1_SHOT( "scrshots/px6a1_08_knife.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert knife\n" );
				}
				else if( s_probe_step == 12 && elapsed >= 33.0f )
				{
					s_probe_step = 13;
					gEngfuncs.pfnClientCmd( "give weapon_hegrenade; weapon_hegrenade; +attack; wait; -attack; +attack; wait; -attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert he throw\n" );
				}
				else if( s_probe_step == 13 && elapsed >= 38.0f )
				{
					s_probe_step = 14;
					PX6A1_SHOT( "scrshots/px6a1_09_he.png" );
					gEngfuncs.pfnClientCmd( "give weapon_smokegrenade; weapon_smokegrenade; +attack; wait; -attack; +attack; wait; -attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert smoke throw\n" );
				}
				else if( s_probe_step == 14 && elapsed >= 43.0f )
				{
					s_probe_step = 15;
					PX6A1_SHOT( "scrshots/px6a1_10_smoke.png" );
					gEngfuncs.pfnClientCmd( "give weapon_flashbang; weapon_flashbang; +attack; wait; -attack; +attack; wait; -attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert flash throw\n" );
				}
				else if( s_probe_step == 15 && elapsed >= 47.0f )
				{
					s_probe_step = 16;
					PX6A1_SHOT( "scrshots/px6a1_11_flash.png" );
					gEngfuncs.pfnClientCmd( "thirdperson\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert thirdperson\n" );
				}
				else if( s_probe_step == 16 && elapsed >= 50.0f )
				{
					s_probe_step = 17;
					PX6A1_SHOT( "scrshots/px6a1_12_thirdperson.png" );
					gEngfuncs.pfnClientCmd( "firstperson\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert firstperson\n" );
				}
				else if( s_probe_step == 17 && elapsed >= 52.0f )
				{
					s_probe_step = 18;
					gEngfuncs.Cvar_SetValue( "r_dynamic", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert r_dynamic 0\n" );
				}
				else if( s_probe_step == 18 && elapsed >= 54.0f )
				{
					s_probe_step = 19;
					gEngfuncs.Cvar_SetValue( "r_dynamic", 1.0f );
					gEngfuncs.pfnClientCmd( "give weapon_hegrenade; weapon_hegrenade; +attack; wait; -attack; +attack; wait; -attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert he r_dynamic 1\n" );
				}
				else if( s_probe_step == 19 && elapsed >= 58.0f )
				{
					s_probe_step = 20;
					gEngfuncs.pfnClientCmd( "dev_overview 1\n" );
					PX6A1_SHOT( "scrshots/px6a1_13_overview.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert overview 1\n" );
				}
				else if( s_probe_step == 20 && elapsed >= 61.0f )
				{
					s_probe_step = 21;
					gEngfuncs.pfnClientCmd( "dev_overview 0\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert overview 0\n" );
				}
				else if( s_probe_step == 21 && elapsed >= 63.0f )
				{
					s_probe_step = 22;
					gEngfuncs.Cvar_SetValue( "r_ripple", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert ripple 1\n" );
				}
				else if( s_probe_step == 22 && elapsed >= 66.0f )
				{
					s_probe_step = 23;
					gEngfuncs.Cvar_SetValue( "r_ripple", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert ripple 0\n" );
				}
				else if( s_probe_step == 23 && elapsed >= 68.0f )
				{
					s_probe_step = 24;
					gEngfuncs.Cvar_SetValue( "r_csretro_takeover_force_fault", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert force_fault arm\n" );
				}
				else if( s_probe_step == 24 && elapsed >= 70.0f )
				{
					s_probe_step = 25;
					CSRETRO_Takeover_ClearFault();
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert force_fault clear\n" );
				}
				else if( s_probe_step == 25 && elapsed >= 72.0f )
				{
					s_probe_step = 26;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert mode 2→0\n" );
				}
				else if( s_probe_step == 26 && elapsed >= 74.0f )
				{
					s_probe_step = 27;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert mode 0→2\n" );
				}
				else if( s_probe_step == 27 && elapsed >= 76.0f )
				{
					s_probe_step = 28;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert mode 2→1\n" );
				}
				else if( s_probe_step == 28 && elapsed >= 78.0f )
				{
					s_probe_step = 29;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert mode 1→2\n" );
				}
				else if( s_probe_step == 29 && elapsed >= 80.0f )
				{
					s_probe_step = 30;
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert map de_torn\n" );
				}
				else if( s_probe_step == 30 && elapsed >= 88.0f )
				{
					s_probe_step = 31;
					PX6A1_SHOT( "scrshots/px6a1_14_torn_water.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert water torn look\n" );
				}
				else if( s_probe_step == 31 && elapsed >= 92.0f )
				{
					s_probe_step = 32;
					gEngfuncs.pfnClientCmd( "map cs_assault\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert map cs_assault\n" );
				}
				else if( s_probe_step == 32 && elapsed >= 100.0f )
				{
					s_probe_step = 33;
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_enttools_enable 1; noclip; ent_fire 19 movehere\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert assault door here\n" );
				}
				else if( s_probe_step == 33 && elapsed >= 103.0f )
				{
					s_probe_step = 34;
					PX6A1_SHOT( "scrshots/px6a1_15_door_closed.png" );
					gEngfuncs.pfnClientCmd( "+use\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert assault door use\n" );
				}
				else if( s_probe_step == 34 && elapsed >= 106.0f )
				{
					s_probe_step = 35;
					PX6A1_SHOT( "scrshots/px6a1_15b_door_open.png" );
					gEngfuncs.pfnClientCmd( "-use; noclip\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert assault door done\n" );
				}
				else if( s_probe_step == 35 && elapsed >= 108.0f )
				{
					s_probe_step = 36;
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert map de_dust\n" );
				}
				else if( s_probe_step == 36 && elapsed >= 116.0f )
				{
					s_probe_step = 37;
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
					PX6A1_SHOT( "scrshots/px6a1_16_dust_vid.png" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 37 && elapsed >= 120.0f )
				{
					s_probe_step = 38;
					gEngfuncs.Con_Printf(
						"CS Retro: probe_seq PX6A1 cert lost_eligible_event_frames=%i\n",
						CSRETRO_Studio_LostEligibleEventFrames() );
					gEngfuncs.pfnClientCmd( "quit\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A1 cert quit\n" );
				}
#undef PX6A1_SHOT
			}
			else if( takeoverc )
			{
				if( s_probe_step == 0 && elapsed >= 2.0f )
				{
					s_probe_step = 1;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A renderer 2\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 5.0f )
				{
					s_probe_step = 2;
					gEngfuncs.pfnClientCmd( "give weapon_ak47; weapon_ak47; +attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A fire ak47\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 8.0f )
				{
					s_probe_step = 3;
					gEngfuncs.pfnClientCmd( "give weapon_hegrenade; weapon_hegrenade; +attack; wait; -attack; +attack; wait; -attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A throw he\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 12.0f )
				{
					s_probe_step = 4;
					gEngfuncs.pfnClientCmd( "dev_overview 1\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A dev_overview 1\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 15.0f )
				{
					s_probe_step = 5;
					gEngfuncs.pfnClientCmd( "dev_overview 0\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A dev_overview 0\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 17.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Cvar_SetValue( "r_ripple", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A r_ripple 1\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 20.0f )
				{
					s_probe_step = 7;
					gEngfuncs.Cvar_SetValue( "r_ripple", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A r_ripple 0\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 22.0f )
				{
					s_probe_step = 8;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A mode 2→0\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 24.0f )
				{
					s_probe_step = 9;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A mode 0→2\n" );
				}
				else if( s_probe_step == 9 && elapsed >= 26.0f )
				{
					s_probe_step = 10;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A mode 2→1\n" );
				}
				else if( s_probe_step == 10 && elapsed >= 28.0f )
				{
					s_probe_step = 11;
					gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 2.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A mode 1→2\n" );
				}
				else if( s_probe_step == 11 && elapsed >= 30.0f )
				{
					s_probe_step = 12;
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A map de_torn\n" );
				}
				else if( s_probe_step == 12 && elapsed >= 37.0f )
				{
					s_probe_step = 13;
					gEngfuncs.pfnClientCmd( "map cs_assault\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A map cs_assault\n" );
				}
				else if( s_probe_step == 13 && elapsed >= 44.0f )
				{
					s_probe_step = 14;
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A map de_dust\n" );
				}
				else if( s_probe_step == 14 && elapsed >= 51.0f )
				{
					s_probe_step = 15;
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 15 && elapsed >= 55.0f )
				{
					s_probe_step = 16;
					gEngfuncs.pfnClientCmd( "quit\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq PX6A quit\n" );
				}
			}
			else if( visc )
			{
				if( s_probe_step == 0 && elapsed >= 2.0f )
				{
					float ang[3] = { -42.0f, 130.0f, 0.0f };
					s_probe_step = 1;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq sky look pitch=-42 yaw=130\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 5.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vis ready\n" );
					gEngfuncs.pfnClientCmd( "r_novis 1\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_novis 1\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 8.0f )
				{
					s_probe_step = 3;
					gEngfuncs.pfnClientCmd( "r_novis 0\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_novis 0\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 10.0f )
				{
					s_probe_step = 4;
					gEngfuncs.pfnClientCmd( "r_lockpvs 1\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_lockpvs 1\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 12.0f )
				{
					s_probe_step = 5;
					gEngfuncs.pfnClientCmd( "r_lockpvs 0\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_lockpvs 0\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 14.0f )
				{
					s_probe_step = 6;
					gEngfuncs.pfnClientCmd( "dev_overview 1\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq dev_overview 1\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 18.0f )
				{
					s_probe_step = 7;
					gEngfuncs.pfnClientCmd( "dev_overview 0\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq dev_overview 0\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 25.0f )
				{
					s_probe_step = 8;
					gEngfuncs.pfnClientCmd( "map cs_assault\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 32.0f )
				{
					s_probe_step = 9;
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
				}
				else if( s_probe_step == 9 && elapsed >= 39.0f )
				{
					s_probe_step = 10;
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 10 && elapsed >= 43.0f )
				{
					s_probe_step = 11;
					gEngfuncs.pfnClientCmd( "quit\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
				}
			}
			else if( viewmodelc )
			{
				if( s_probe_step == 0 && elapsed >= 2.0f )
				{
					float ang[3] = { 16.0f, 90.0f, 0.0f };
					s_probe_step = 1;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Cvar_SetValue( "cl_righthand", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel glock righthand 1\n" );
					if( s_probe_seq->value >= 13.0f )
					{
						gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 0.0f );
						gEngfuncs.Con_Printf( "CS Retro: probe_seq events renderer 0\n" );
						gEngfuncs.pfnClientCmd( "give weapon_glock18; give weapon_usp; give weapon_ak47; weapon_ak47; +attack\n" );
					}
					else
						gEngfuncs.pfnClientCmd( "give weapon_glock18; give weapon_usp; weapon_glock18\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 6.0f )
				{
					s_probe_step = 2;
					if( s_probe_seq->value >= 13.0f )
					{
						gEngfuncs.pfnClientCmd( "-attack\n" );
						gEngfuncs.Cvar_SetValue( "r_csretro_renderer", 1.0f );
						gEngfuncs.Con_Printf( "CS Retro: probe_seq events renderer 1\n" );
					}
					gEngfuncs.Cvar_SetValue( "cl_righthand", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel righthand 0\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 9.0f )
				{
					s_probe_step = 3;
					gEngfuncs.Cvar_SetValue( "cl_righthand", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel knife righthand 0\n" );
					gEngfuncs.pfnClientCmd( "give weapon_knife; weapon_knife\n" );
					gEngfuncs.Cvar_SetValue( "cl_righthand", 0.0f );
				}
				else if( s_probe_step == 3 && elapsed >= 13.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Cvar_SetValue( "cl_righthand", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel knife righthand 1\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 16.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel ak47\n" );
					gEngfuncs.pfnClientCmd( "give weapon_ak47; weapon_ak47\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 19.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel fire\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 20.5f )
				{
					s_probe_step = 7;
					gEngfuncs.pfnClientCmd( "-attack; +reload\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel reload\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 23.0f )
				{
					s_probe_step = 8;
					gEngfuncs.pfnClientCmd( "-reload; give weapon_hegrenade; weapon_hegrenade; +attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel he pull\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 25.5f )
				{
					s_probe_step = 9;
					gEngfuncs.pfnClientCmd( "-attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel he throw\n" );
				}
				else if( s_probe_step == 9 && elapsed >= 28.0f )
				{
					s_probe_step = 10;
					gEngfuncs.pfnClientCmd( "give weapon_smokegrenade; weapon_smokegrenade; +attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel smoke pull\n" );
				}
				else if( s_probe_step == 10 && elapsed >= 30.5f )
				{
					s_probe_step = 11;
					gEngfuncs.pfnClientCmd( "-attack; give weapon_flashbang; weapon_flashbang; +attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel flash pull\n" );
				}
				else if( s_probe_step == 11 && elapsed >= 33.0f )
				{
					s_probe_step = 12;
					gEngfuncs.pfnClientCmd( "-attack; give weapon_molotov; weapon_molotov; +attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel molotov pull\n" );
				}
				else if( s_probe_step == 12 && elapsed >= 36.5f )
				{
					s_probe_step = 13;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel molotov wick\n" );
					CSRETRO_Studio_LogMolotovEventWick();
					s_vm_event_wick_logged = 1;
				}
				else if( s_probe_step == 13 && elapsed >= 38.5f )
				{
					s_probe_step = 14;
					gEngfuncs.pfnClientCmd( "-attack\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel molotov throw\n" );
				}
				else if( s_probe_step == 14 && elapsed >= 42.0f )
				{
					s_probe_step = 15;
					gEngfuncs.Cvar_SetValue( "r_drawviewmodel", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_drawviewmodel 0\n" );
				}
				else if( s_probe_step == 15 && elapsed >= 45.0f )
				{
					s_probe_step = 16;
					gEngfuncs.Cvar_SetValue( "r_drawviewmodel", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_drawviewmodel 1\n" );
				}
				else if( s_probe_step == 16 && elapsed >= 48.0f )
				{
					s_probe_step = 17;
					gEngfuncs.Cvar_SetValue( "cam_idealdist", 128.0f );
					cam_thirdperson = 1;
					gEngfuncs.pfnClientCmd( "thirdperson\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel thirdperson\n" );
				}
				else if( s_probe_step == 17 && elapsed >= 51.0f )
				{
					s_probe_step = 18;
					cam_thirdperson = 0;
					gEngfuncs.pfnClientCmd( "firstperson; give weapon_shield\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel shield try\n" );
				}
				else if( s_probe_step == 18 && elapsed >= 54.0f )
				{
					CSRETRO_StudioViewmodelProof vp;
					CSRETRO_Studio_GetViewmodelProof( &vp );
					s_probe_step = 19;
					if( vp.shield_detected )
						gEngfuncs.Con_Printf( "CS Retro: viewmodel shield detected=%i special_flip=%i\n",
							vp.shield_detected, vp.special_flip );
					else
						gEngfuncs.Con_Printf( "CS Retro: viewmodel shield implemented / runtime NOT REPRODUCIBLE WITH CURRENT GAME CONTENT\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel dead\n" );
					gEngfuncs.pfnClientCmd( "kill\n" );
				}
				else if( s_probe_step == 19 && elapsed >= 58.0f )
				{
					s_probe_step = 20;
					gEngfuncs.pfnClientCmd( "jointeam 1; joinclass 5\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq viewmodel restore team\n" );
				}
				else if( s_probe_step == 20 && elapsed >= 64.0f )
				{
					s_probe_step = 21;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
				}
				else if( s_probe_step == 21 && elapsed >= 72.0f )
				{
					s_probe_step = 22;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "map cs_assault\n" );
				}
				else if( s_probe_step == 22 && elapsed >= 80.0f )
				{
					s_probe_step = 23;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 23 && elapsed >= 88.0f )
				{
					s_probe_step = 24;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 24 && elapsed >= 92.0f )
				{
					s_probe_step = 25;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( playerc )
			{
				if( s_probe_step == 0 && elapsed >= 2.0f )
				{
					float target[3];
					float ang[3] = { 8.0f, 90.0f, 0.0f };
					int aimed = 0;
					if( CSRETRO_Studio_ProbeLookTarget( target ) )
					{
						cl_entity_t *lp = gEngfuncs.GetLocalPlayer();
						if( lp )
						{
							float dx = target[0] - lp->origin[0];
							float dy = target[1] - lp->origin[1];
							float dz = target[2] - lp->origin[2];
							float dist = (float)sqrt( (double)( dx * dx + dy * dy ) );
							ang[1] = (float)( atan2( (double)dy, (double)dx ) * 180.0 / 3.14159265358979323846 );
							if( dist > 1.0f )
								ang[0] = (float)( -atan2( (double)dz, (double)dist ) * 180.0 / 3.14159265358979323846 );
							aimed = 1;
						}
					}
					if( aimed || elapsed >= 6.0f )
					{
						s_probe_step = 1;
						gEngfuncs.SetViewAngles( ang );
						gEngfuncs.Con_Printf( "CS Retro: probe_seq player look aimed=%i\n", aimed );
						gEngfuncs.pfnClientCmd( "r_shadows 1\n" );
					}
				}
				else if( s_probe_step == 1 && elapsed >= 10.0f )
				{
					cvar_t *cv;
					s_probe_step = 2;
					cv = gEngfuncs.pfnGetCvarPointer( "r_shadows" );
					if( cv )
						cv->value = 0.0f;
					gEngfuncs.Cvar_SetValue( "r_shadows", 0.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_shadows 0 value=%.0f\n", cv ? cv->value : -1.0f );
				}
				else if( s_probe_step == 2 && elapsed >= 14.0f )
				{
					cvar_t *cv;
					s_probe_step = 3;
					cv = gEngfuncs.pfnGetCvarPointer( "r_shadows" );
					if( cv )
						cv->value = 1.0f;
					gEngfuncs.Cvar_SetValue( "r_shadows", 1.0f );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_shadows 1 value=%.0f\n", cv ? cv->value : -1.0f );
				}
				else if( s_probe_step == 3 && elapsed >= 18.0f )
				{
					float ang[3] = { 18.0f, 0.0f, 0.0f };
					s_probe_step = 4;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 18.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Cvar_SetValue( "cam_idealdist", 128.0f );
					gEngfuncs.Cvar_SetValue( "cam_idealpitch", 12.0f );
					cam_thirdperson = 1;
					gEngfuncs.pfnClientCmd( "thirdperson\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq thirdperson cam=%i\n", cam_thirdperson );
				}
				else if( s_probe_step == 4 && elapsed >= 24.0f )
				{
					s_probe_step = 5;
					cam_thirdperson = 0;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq spectator join\n" );
					gEngfuncs.pfnClientCmd( "firstperson; kill\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 28.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf(
						"CS Retro: probe_seq spectator ineye user1=%i user2=%i thirdperson=%i\n",
						g_iUser1, g_iUser2, CL_IsThirdPerson() );
					gEngfuncs.pfnClientCmd( "spec_mode 4; cmd specmode 4\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 32.0f )
				{
					s_probe_step = 7;
					gEngfuncs.Con_Printf(
						"CS Retro: probe_seq spectator chase user1=%i user2=%i thirdperson=%i\n",
						g_iUser1, g_iUser2, CL_IsThirdPerson() );
					gEngfuncs.pfnClientCmd( "spec_mode 2; cmd specmode 2\n" );
				}
				else if( s_probe_step == 7 && elapsed >= 36.0f )
				{
					s_probe_step = 8;
					cam_thirdperson = 0;
					gEngfuncs.pfnClientCmd( "firstperson; jointeam 1; joinclass 5\n" );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq restore team\n" );
				}
				else if( s_probe_step == 8 && elapsed >= 42.0f )
				{
					s_probe_step = 9;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
				}
				else if( s_probe_step == 9 && elapsed >= 50.0f )
				{
					s_probe_step = 10;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "map cs_assault\n" );
				}
				else if( s_probe_step == 10 && elapsed >= 58.0f )
				{
					s_probe_step = 11;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 11 && elapsed >= 66.0f )
				{
					s_probe_step = 12;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 12 && elapsed >= 70.0f )
				{
					s_probe_step = 13;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( randomc )
			{
				if( s_probe_step == 0 && elapsed >= 2.0f )
				{
					float ang[3] = { 12.0f, -40.0f, 0.0f };
					s_probe_step = 1;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 12.0f;
					ang[1] = -40.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq random tiled look\n" );
				}
				else if( s_probe_step == 1 && elapsed >= 8.0f )
				{
					s_probe_step = 2;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
				}
				else if( s_probe_step == 2 && elapsed >= 16.0f )
				{
					s_probe_step = 3;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "map cs_assault\n" );
				}
				else if( s_probe_step == 3 && elapsed >= 24.0f )
				{
					s_probe_step = 4;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 32.0f )
				{
					s_probe_step = 5;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 36.0f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( dlightc )
			{
				CSRETRO_DLightStats dlst;
				memset( &dlst, 0, sizeof( dlst ) );
				CSRETRO_DLight_GetStats( &dlst );
				if( s_probe_step == 0 && elapsed >= 3.0f )
				{
					float ang[3] = { 68.0f, 0.0f, 0.0f };
					s_probe_step = 1;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 68.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give ak47\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; give weapon_ak47; weapon_ak47\n" );
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
				else if( s_probe_step == 3 && elapsed >= 6.0f )
				{
					float ang[3] = { 72.0f, 0.0f, 0.0f };
					s_probe_step = 4;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 72.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give hegrenade\n" );
					gEngfuncs.pfnClientCmd( "give weapon_hegrenade; slot4; weapon_hegrenade\n" );
				}
				else if( s_probe_step == 4 && elapsed >= 6.8f )
				{
					s_probe_step = 5;
					gEngfuncs.pfnClientCmd( "weapon_hegrenade\n" );
				}
				else if( s_probe_step == 5 && elapsed >= 7.4f )
				{
					s_probe_step = 6;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq throw he\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 6 && elapsed >= 7.9f )
				{
					s_probe_step = 7;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 7 && ( dlst.inventory_logged || elapsed >= 13.0f ) )
				{
					s_probe_step = 8;
					s_probe_mark = elapsed;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_dynamic 0\n" );
					gEngfuncs.pfnClientCmd( "r_dynamic 0\n" );
				}
				else if( s_probe_step == 8 && elapsed >= s_probe_mark + 0.8f )
				{
					s_probe_step = 9;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq r_dynamic 1\n" );
					gEngfuncs.pfnClientCmd( "r_dynamic 1\n" );
				}
				else if( s_probe_step == 9 && elapsed >= s_probe_mark + 1.6f )
				{
					float ang[3] = { 72.0f, 0.0f, 0.0f };
					s_probe_step = 10;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 72.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give hegrenade rdyn1\n" );
					gEngfuncs.pfnClientCmd( "give weapon_hegrenade; slot4; weapon_hegrenade\n" );
				}
				else if( s_probe_step == 10 && elapsed >= s_probe_mark + 2.4f )
				{
					s_probe_step = 11;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq throw he rdyn1\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 11 && elapsed >= s_probe_mark + 2.9f )
				{
					s_probe_step = 12;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 12 && elapsed >= s_probe_mark + 5.5f )
				{
					s_probe_step = 13;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map cs_assault\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; map cs_assault\n" );
				}
				else if( s_probe_step == 13 && elapsed >= s_probe_mark + 13.5f )
				{
					s_probe_step = 14;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq assault door here\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_enttools_enable 1; noclip; ent_fire 19 movehere\n" );
				}
				else if( s_probe_step == 14 && elapsed >= s_probe_mark + 15.0f )
				{
					float ang[3] = { 28.0f, 0.0f, 0.0f };
					s_probe_step = 15;
					gEngfuncs.GetViewAngles( ang );
					ang[0] = 28.0f;
					gEngfuncs.SetViewAngles( ang );
					gEngfuncs.Con_Printf( "CS Retro: probe_seq give hegrenade door\n" );
					gEngfuncs.pfnClientCmd( "give weapon_hegrenade; slot4; weapon_hegrenade\n" );
				}
				else if( s_probe_step == 15 && elapsed >= s_probe_mark + 16.0f )
				{
					s_probe_step = 16;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq throw he door\n" );
					gEngfuncs.pfnClientCmd( "+attack\n" );
				}
				else if( s_probe_step == 16 && elapsed >= s_probe_mark + 16.5f )
				{
					s_probe_step = 17;
					gEngfuncs.pfnClientCmd( "-attack\n" );
				}
				else if( s_probe_step == 17 && elapsed >= s_probe_mark + 19.0f )
				{
					s_probe_step = 18;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq assault door use\n" );
					gEngfuncs.pfnClientCmd( "sv_cheats 1; sv_enttools_enable 1; ent_fire 19 use\n" );
				}
				else if( s_probe_step == 18 && elapsed >= s_probe_mark + 23.0f )
				{
					s_probe_step = 19;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_torn\n" );
					gEngfuncs.pfnClientCmd( "map de_torn\n" );
				}
				else if( s_probe_step == 19 && elapsed >= s_probe_mark + 33.0f )
				{
					s_probe_step = 20;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq map de_dust\n" );
					gEngfuncs.pfnClientCmd( "map de_dust\n" );
				}
				else if( s_probe_step == 20 && elapsed >= s_probe_mark + 43.0f )
				{
					s_probe_step = 21;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq vid_setmode 1024 768\n" );
					gEngfuncs.pfnClientCmd( "vid_setmode 1024 768\n" );
				}
				else if( s_probe_step == 21 && elapsed >= s_probe_mark + 47.0f )
				{
					s_probe_step = 22;
					gEngfuncs.Con_Printf( "CS Retro: probe_seq quit\n" );
					gEngfuncs.pfnClientCmd( "quit\n" );
				}
			}
			else if( decalc )
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
