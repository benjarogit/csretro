// Offscreen studio. GSMR stays the CS renderer.
// Remote + eligible local: local player_info_t copy only. Live advances on visible Xash.
// Local world-draw = CL_IsThirdPerson() || index != rvp->viewentity.
// Explicit StudioDrawPlayerShadow after STUDIO_RENDER; never inside Offscreen.
// STUDIO_EVENTS never. No live entity across frames — local snapshot only.
#include "render_studio.h"
#include "render_scene.h"
#include "render_backend.h"

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "com_model.h"
#include "const.h"
#include "entity_types.h"
#include "r_studioint.h"
#include "GameStudioModelRenderer.h"
#include "render_api.h"
#include "ref_params.h"
#include "camera.h"
#include "cdll_dll.h"
#include "pm_shared.h"

#include <string.h>
#include <math.h>

extern engine_studio_api_t IEngineStudio;
extern int g_iUser1;
extern int g_iUser2;

static CSRETRO_StudioPlayerProof s_player_proof;
static int s_player_logged = 0;
static int s_local_logged = 0;
static int s_pending_visible = 0;
static int s_pending_index = -1;
static unsigned int s_pending_before = 0;
static unsigned int s_pending_after_off = 0;
static unsigned int s_pending_local = 0;
static unsigned int s_pending_gait_local = 0;

static unsigned int MixU32( unsigned int h, unsigned int v )
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int MixFloat( unsigned int h, float f )
{
	unsigned int bits = 0;
	memcpy( &bits, &f, sizeof( bits ) );
	return MixU32( h, bits );
}

static unsigned int HashBytes( unsigned int h, const void *p, size_t n )
{
	const unsigned char *b = (const unsigned char *)p;
	size_t i;
	for( i = 0; i < n; i++ )
		h = MixU32( h, b[i] );
	return h;
}

static unsigned int HashPlayerInfo( const player_info_t *info )
{
	unsigned int h = 2166136261u;
	if( !info )
		return 0;
	h = MixU32( h, (unsigned int)info->userid );
	h = HashBytes( h, info->name, sizeof( info->name ) );
	h = HashBytes( h, info->model, sizeof( info->model ) );
	h = MixU32( h, (unsigned int)info->topcolor );
	h = MixU32( h, (unsigned int)info->bottomcolor );
	h = MixU32( h, (unsigned int)info->renderframe );
	h = MixU32( h, (unsigned int)info->gaitsequence );
	h = MixFloat( h, info->gaitframe );
	h = MixFloat( h, info->gaityaw );
	h = MixFloat( h, info->prevgaitorigin[0] );
	h = MixFloat( h, info->prevgaitorigin[1] );
	h = MixFloat( h, info->prevgaitorigin[2] );
	return h;
}

static unsigned int HashPlayerGait( const player_info_t *info )
{
	unsigned int h = 2166136261u;
	if( !info )
		return 0;
	h = MixU32( h, (unsigned int)info->gaitsequence );
	h = MixFloat( h, info->gaitframe );
	h = MixFloat( h, info->gaityaw );
	h = MixFloat( h, info->prevgaitorigin[0] );
	h = MixFloat( h, info->prevgaitorigin[1] );
	h = MixFloat( h, info->prevgaitorigin[2] );
	h = MixU32( h, (unsigned int)info->renderframe );
	return h;
}

static unsigned int HashEntityMut( const cl_entity_t *ent )
{
	unsigned int h = 2166136261u;
	int i;
	if( !ent )
		return 0;
	h = MixFloat( h, ent->angles[0] );
	h = MixFloat( h, ent->angles[1] );
	h = MixFloat( h, ent->angles[2] );
	h = MixFloat( h, ent->origin[0] );
	h = MixFloat( h, ent->origin[1] );
	h = MixFloat( h, ent->origin[2] );
	h = MixU32( h, (unsigned int)ent->curstate.sequence );
	h = MixU32( h, (unsigned int)ent->curstate.gaitsequence );
	h = MixFloat( h, ent->curstate.frame );
	h = MixFloat( h, ent->curstate.animtime );
	h = MixFloat( h, ent->curstate.framerate );
	for( i = 0; i < 4; i++ )
		h = MixU32( h, ent->curstate.controller[i] );
	for( i = 0; i < 2; i++ )
		h = MixU32( h, ent->curstate.blending[i] );
	h = MixFloat( h, ent->latched.prevanimtime );
	h = MixFloat( h, ent->latched.sequencetime );
	h = MixU32( h, (unsigned int)ent->latched.prevsequence );
	h = MixFloat( h, ent->latched.prevframe );
	for( i = 0; i < 4; i++ )
		h = MixU32( h, ent->latched.prevcontroller[i] );
	for( i = 0; i < 2; i++ )
		h = MixU32( h, ent->latched.prevblending[i] );
	for( i = 0; i < 2; i++ )
		h = MixU32( h, ent->latched.prevseqblending[i] );
	return h;
}

static player_info_t *LivePlayerInfo( const cl_entity_t *snap )
{
	int idx;
	if( !snap || !IEngineStudio.PlayerInfo )
		return NULL;
	idx = snap->curstate.number - 1;
	if( idx < 0 )
		idx = snap->index - 1;
	if( idx < 0 || idx >= gEngfuncs.GetMaxClients() )
		return NULL;
	return IEngineStudio.PlayerInfo( idx );
}

static int Eligible( const CSRETRO_EntCopy *e )
{
	if( !e || e->kind != CSRETRO_KIND_STUDIO )
		return 0;
	if( e->type != ET_NORMAL )
		return 0;
	if( e->player || e->is_viewmodel || e->is_follow || e->is_preview )
		return 0;
	if( e->snap_index < 0 )
		return 0;
	return 1;
}

static int EligibleRemotePlayer( const CSRETRO_EntCopy *e )
{
	if( !e || e->kind != CSRETRO_KIND_STUDIO )
		return 0;
	if( !e->player )
		return 0;
	if( e->type != ET_NORMAL && e->type != ET_PLAYER )
		return 0;
	if( e->is_viewmodel || e->is_follow || e->is_preview )
		return 0;
	if( e->snap_index < 0 )
		return 0;
	return 1;
}

static int EligibleLocalPlayer( const CSRETRO_EntCopy *e )
{
	if( !e || e->kind != CSRETRO_KIND_STUDIO_LOCAL )
		return 0;
	if( !e->player )
		return 0;
	if( e->type != ET_NORMAL && e->type != ET_PLAYER )
		return 0;
	if( e->is_viewmodel || e->is_follow || e->is_preview )
		return 0;
	if( e->snap_index < 0 )
		return 0;
	return 1;
}

static int LocalWorldDrawEligible( const CSRETRO_EntCopy *e, const ref_viewpass_t *rvp )
{
	int viewentity;

	if( !e )
		return 0;
	if( CL_IsThirdPerson() )
		return 1;
	viewentity = rvp ? rvp->viewentity : 0;
	return e->index != viewentity;
}

static int ShadowsCvarOn( void )
{
	cvar_t *cv = gEngfuncs.pfnGetCvarPointer( "r_shadows" );
	return ( cv && cv->value != 0.0f ) ? 1 : 0;
}

static unsigned int SampleFboCrc( void )
{
	CSRETRO_OffscreenProof p;
	memset( &p, 0, sizeof( p ) );
	CSRETRO_Backend_SampleProof( &p );
	return p.crc;
}

static void ClassifyLocalPlayer( const ref_viewpass_t *rvp )
{
	int i, n;
	int mirrored = 0;
	int firstperson = 0;
	int spectator = 0;
	int chase = 0;
	int ineye = 0;
	int thirdperson = 0;
	int local_index = 0;
	int viewentity = rvp ? rvp->viewentity : 0;
	cl_entity_t *lp = gEngfuncs.GetLocalPlayer();

	if( lp )
		local_index = lp->index;

	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		if( e && e->kind == CSRETRO_KIND_STUDIO_LOCAL )
			mirrored = 1;
	}

	if( g_iUser1 )
	{
		spectator = 1;
		if( g_iUser1 == OBS_CHASE_LOCKED || g_iUser1 == OBS_CHASE_FREE || g_iUser1 == OBS_MAP_CHASE )
			chase = 1;
		if( g_iUser1 == OBS_IN_EYE )
			ineye = 1;
	}
	else if( !CL_IsThirdPerson() )
		firstperson = 1;

	if( CL_IsThirdPerson() )
		thirdperson = 1;

	s_player_proof.local_mirrored = mirrored;
	s_player_proof.local_firstperson = firstperson;
	s_player_proof.local_spectator = spectator;
	s_player_proof.local_chase = chase;
	s_player_proof.local_ineye = ineye;
	s_player_proof.local_thirdperson = thirdperson;
	s_player_proof.local_deferred = 0;
	s_player_proof.local_index = local_index;
	s_player_proof.viewentity = viewentity;
	s_player_proof.r_shadows_on = ShadowsCvarOn();

	if( !s_local_logged )
	{
		s_local_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: local player class firstperson=%i spectator=%i chase=%i ineye=%i thirdperson=%i mirrored=%i viewentity=%i local_index=%i user2=%i setupclientanim=0 (m_bLocal=0) offscreen=xash-eligibility\n",
			firstperson, spectator, chase, ineye, thirdperson, mirrored,
			viewentity, local_index, g_iUser2 );
	}
}

static void NotePlayerModel( const player_info_t *info, int team )
{
	const char *name;
	if( !info || !info->model[0] )
		return;
	name = info->model;
	if( team == TEAM_TERRORIST )
		s_player_proof.models_t = 1;
	else if( team == TEAM_CT )
		s_player_proof.models_ct = 1;
	if( !s_player_proof.model_a[0] )
	{
		strncpy( s_player_proof.model_a, name, sizeof( s_player_proof.model_a ) - 1 );
		s_player_proof.distinct_models = 1;
	}
	else if( strcmp( s_player_proof.model_a, name ) != 0 && !s_player_proof.model_b[0] )
	{
		strncpy( s_player_proof.model_b, name, sizeof( s_player_proof.model_b ) - 1 );
		s_player_proof.distinct_models = 2;
	}
}

static void FinishPendingVisible( void )
{
	player_info_t *live;
	unsigned int after_vis;
	unsigned int gait_vis;

	if( !s_pending_visible || s_pending_index < 0 || !IEngineStudio.PlayerInfo )
		return;
	if( s_pending_index >= gEngfuncs.GetMaxClients() )
		return;

	live = IEngineStudio.PlayerInfo( s_pending_index );
	if( !live )
		return;

	after_vis = HashPlayerInfo( live );
	gait_vis = HashPlayerGait( live );
	s_player_proof.info_after_visible = after_vis;
	s_player_proof.info_visible_final = after_vis;
	s_player_proof.gait_visible_final = gait_vis;
	s_player_proof.after_visible_ne_before = ( after_vis != s_pending_before ) ? 1 : 0;
	s_player_proof.local_eq_visible = ( s_pending_local == after_vis ) ? 1 : 0;
	s_player_proof.gait_local_eq_visible = ( s_pending_gait_local == gait_vis ) ? 1 : 0;

	if( !s_player_logged )
	{
		gEngfuncs.Con_Printf(
			"CS Retro: player_info hash BEFORE=%08x AFTER_OFFSCREEN=%08x AFTER_VISIBLE=%08x live_mutate=%i visible_advanced=%i\n",
			s_pending_before, s_pending_after_off, after_vis,
			( s_pending_before != s_pending_after_off ) ? 1 : 0,
			s_player_proof.after_visible_ne_before );
		gEngfuncs.Con_Printf(
			"CS Retro: player_info offscreen_local_final=%08x visible_live_final=%08x equal=%i gait_local=%08x gait_visible=%08x gait_equal=%i\n",
			s_pending_local, after_vis, s_player_proof.local_eq_visible,
			s_pending_gait_local, gait_vis, s_player_proof.gait_local_eq_visible );
		s_player_logged = 1;
	}

	s_pending_visible = 0;
}

int CSRETRO_Studio_DrawList( CSRETRO_SceneStats *stats )
{
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	int i, n, drawn = 0;
	static int s_logged_model = 0;

	if( !gRenderAPI.R_SetCurrentEntity )
		return 0;

	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		cl_entity_t *snap;
		int ok;

		if( !Eligible( e ) )
			continue;
		snap = CSRETRO_Scene_StudioSnap( e->snap_index );
		if( !snap || !snap->model )
			continue;
		if( snap->curstate.renderfx == kRenderFxDeadPlayer )
			continue;

		CSRETRO_Scene_NoteAttempted();
		gRenderAPI.R_SetCurrentEntity( snap );
		// STUDIO_RENDER only. Never STUDIO_EVENTS (no sounds, muzzle, attachment writeback).
		ok = g_StudioRenderer.StudioDrawModel( STUDIO_RENDER );
		if( ok )
		{
			CSRETRO_Scene_NoteDrawn( CSRETRO_KIND_STUDIO );
			drawn++;
			if( !s_logged_model && snap->model->name[0] )
			{
				s_logged_model = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen studio draw model=%s index=%i events=0\n",
					snap->model->name, snap->index );
			}
		}
	}

	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );

	if( stats )
		CSRETRO_Scene_GetStats( stats );
	return drawn;
}

static int MaybeExplicitShadow( int is_follow_bones )
{
	int shadows_before;
	int result;

	if( is_follow_bones )
	{
		s_player_proof.follow_player_shadow = 0;
		return 0;
	}
	if( !ShadowsCvarOn() )
		return 0;

	s_player_proof.shadow_candidates++;
	shadows_before = g_StudioRenderer.OffscreenShadowsDrawn();
	result = g_StudioRenderer.StudioDrawPlayerShadow();
	if( g_StudioRenderer.OffscreenShadowsDrawn() != shadows_before )
		s_player_proof.shadow_side_draw = 1;
	if( result < 0 )
		return 0;
	s_player_proof.shadow_trace_attempts++;
	if( result > 0 )
	{
		s_player_proof.shadow_drawn++;
		CSRETRO_Backend_PrepareImmediateDraw();
		return 1;
	}
	s_player_proof.shadow_rejected_trace++;
	CSRETRO_Backend_PrepareImmediateDraw();
	return 0;
}

static int DrawIsolatedPlayer( const CSRETRO_EntCopy *e, int is_local, int want_shadow )
{
	cl_entity_t *snap;
	cl_entity_t *live;
	player_info_t *live_info;
	player_info_t local_info;
	entity_state_t pplayer;
	unsigned int h_before, h_after, h_local, h_gait;
	unsigned int e_before, e_after, e_snap;
	int events_before, shadows_before;
	int ok;
	int team = 0;

	snap = CSRETRO_Scene_StudioSnap( e->snap_index );
	if( !snap || !snap->model )
		return 0;
	if( snap->curstate.renderfx == kRenderFxDeadPlayer )
		return 0;

	if( !is_local )
		s_player_proof.candidates++;
	CSRETRO_Scene_NotePlayerAttempted();
	CSRETRO_Scene_NoteAttempted();

	live = e->live;
	live_info = LivePlayerInfo( snap );
	if( !live_info )
		return 0;

	h_before = HashPlayerInfo( live_info );
	e_before = HashEntityMut( live );
	local_info = *live_info;
	pplayer = snap->curstate;
	if( pplayer.number <= 0 )
		pplayer.number = snap->index;

	if( pplayer.number > 0 && pplayer.number <= MAX_PLAYERS )
		team = g_PlayerExtraInfo[pplayer.number].teamnumber;
	NotePlayerModel( live_info, team );

	events_before = g_StudioRenderer.OffscreenEventsFired();
	shadows_before = g_StudioRenderer.OffscreenShadowsDrawn();

	gRenderAPI.R_SetCurrentEntity( snap );
	ok = g_StudioRenderer.StudioDrawPlayerOffscreen( STUDIO_RENDER, &pplayer, &local_info );

	h_after = HashPlayerInfo( live_info );
	h_local = HashPlayerInfo( &local_info );
	h_gait = HashPlayerGait( &local_info );
	e_after = HashEntityMut( live );
	e_snap = HashEntityMut( snap );

	if( h_after != h_before )
	{
		if( is_local )
			s_player_proof.local_info_mutate = 1;
		else
			s_player_proof.info_mutate = 1;
	}
	if( e_after != e_before )
	{
		if( is_local )
			s_player_proof.local_entity_mutate = 1;
		else
			s_player_proof.entity_mutate = 1;
	}
	if( g_StudioRenderer.OffscreenEventsFired() != events_before )
		s_player_proof.events = 1;
	if( g_StudioRenderer.OffscreenShadowsDrawn() != shadows_before )
		s_player_proof.shadow_side_draw = 1;

	if( is_local )
	{
		s_player_proof.local_info_before = h_before;
		s_player_proof.local_info_after = h_after;
		s_player_proof.local_entity_before = e_before;
		s_player_proof.local_entity_after = e_after;
	}
	else
	{
		s_player_proof.info_before = h_before;
		s_player_proof.info_after_offscreen = h_after;
		s_player_proof.info_local_final = h_local;
		s_player_proof.gait_local_final = h_gait;
		s_player_proof.before_eq_after_offscreen = ( h_before == h_after ) ? 1 : 0;
		s_player_proof.entity_live_before = e_before;
		s_player_proof.entity_live_after_offscreen = e_after;
		s_player_proof.entity_snap_after = e_snap;
		s_player_proof.live_entity_eq = ( e_before == e_after ) ? 1 : 0;

		if( !s_pending_visible )
		{
			s_pending_visible = 1;
			s_pending_index = pplayer.number - 1;
			s_pending_before = h_before;
			s_pending_after_off = h_after;
			s_pending_local = h_local;
			s_pending_gait_local = h_gait;
		}
	}

	if( !ok )
		return 0;

	CSRETRO_Scene_NoteDrawn( CSRETRO_KIND_STUDIO );
	CSRETRO_Scene_NotePlayerDrawn();
	if( !is_local )
		s_player_proof.drawn++;
	else
		s_player_proof.local_drawn++;

	if( !s_player_proof.look_ready )
	{
		s_player_proof.look_origin[0] = snap->origin[0];
		s_player_proof.look_origin[1] = snap->origin[1];
		s_player_proof.look_origin[2] = snap->origin[2];
		s_player_proof.look_ready = 1;
	}

	if( !is_local && !s_player_proof.crc_after_player_body )
		s_player_proof.crc_after_player_body = SampleFboCrc();
	else if( is_local && !s_player_proof.crc_after_local )
		s_player_proof.crc_after_local = SampleFboCrc();

	if( want_shadow )
	{
		unsigned int crc_body = SampleFboCrc();
		if( MaybeExplicitShadow( 0 ) )
		{
			unsigned int crc_shadow = SampleFboCrc();
			if( !is_local )
			{
				if( crc_shadow != crc_body )
				{
					s_player_proof.crc_after_player_body = crc_body;
					s_player_proof.crc_after_shadow = crc_shadow;
					s_player_proof.remote_shadow_pixel = 1;
				}
				else if( !s_player_proof.crc_after_shadow )
				{
					s_player_proof.crc_after_player_body = crc_body;
					s_player_proof.crc_after_shadow = crc_shadow;
				}
			}
			else
			{
				s_player_proof.crc_after_local = crc_body;
				s_player_proof.crc_after_local_shadow = crc_shadow;
				if( crc_shadow != crc_body )
					s_player_proof.local_shadow_pixel = 1;
			}
		}
	}

	return 1;
}

int CSRETRO_Studio_DrawPlayers( CSRETRO_SceneStats *stats, const ref_viewpass_t *rvp )
{
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	int i, n, drawn = 0;
	int shadow_drawn_start;
	int remote_drawn_start;
	static int s_fp_logged = 0;
	static int s_tp_logged = 0;
	static int s_spec_logged = 0;
	static int s_shadow_logged = 0;
	static int s_shadow_off_logged = 0;

	ClassifyLocalPlayer( rvp );
	FinishPendingVisible();

	if( !gRenderAPI.R_SetCurrentEntity )
		return 0;

	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	shadow_drawn_start = s_player_proof.shadow_drawn;
	remote_drawn_start = s_player_proof.drawn;

	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );

		if( !EligibleRemotePlayer( e ) )
			continue;
		if( DrawIsolatedPlayer( e, 0, 1 ) )
			drawn++;
	}

	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );

		if( !EligibleLocalPlayer( e ) )
			continue;
		if( !LocalWorldDrawEligible( e, rvp ) )
		{
			s_player_proof.local_hidden_viewentity++;
			continue;
		}
		s_player_proof.local_eligible++;
		if( !s_player_proof.crc_before_local )
			s_player_proof.crc_before_local = SampleFboCrc();
		if( DrawIsolatedPlayer( e, 1, 1 ) )
		{
			drawn++;
			if( s_player_proof.crc_after_local && s_player_proof.crc_before_local
				&& s_player_proof.crc_after_local != s_player_proof.crc_before_local )
				s_player_proof.local_pixel = 1;
		}
	}

	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );

	if( !s_fp_logged && s_player_proof.local_mirrored && s_player_proof.local_hidden_viewentity > 0
		&& s_player_proof.local_drawn == 0 && s_player_proof.drawn > 0 )
	{
		s_fp_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: local firstperson hidden mirrored=%i hidden_viewentity=%i local_drawn=%i remote_drawn=%i viewentity=%i local_index=%i thirdperson=%i eligible=%i\n",
			s_player_proof.local_mirrored, s_player_proof.local_hidden_viewentity,
			s_player_proof.local_drawn, s_player_proof.drawn,
			s_player_proof.viewentity, s_player_proof.local_index,
			s_player_proof.local_thirdperson, s_player_proof.local_eligible );
	}
	if( !s_tp_logged && s_player_proof.local_thirdperson && s_player_proof.local_eligible > 0
		&& s_player_proof.local_drawn > 0 )
	{
		s_tp_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: local thirdperson drawn eligible=%i local_drawn=%i pixel=%i before_crc=%08x after_crc=%08x info_mutate=%i entity_mutate=%i shadow_cand=%i shadow_drawn=%i local_shadow_pixel=%i\n",
			s_player_proof.local_eligible, s_player_proof.local_drawn,
			s_player_proof.local_pixel, s_player_proof.crc_before_local,
			s_player_proof.crc_after_local, s_player_proof.local_info_mutate,
			s_player_proof.local_entity_mutate, s_player_proof.shadow_candidates,
			s_player_proof.shadow_drawn, s_player_proof.local_shadow_pixel );
	}
	if( !s_spec_logged && s_player_proof.local_spectator )
	{
		s_spec_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: spectator class chase=%i ineye=%i viewentity=%i local_index=%i thirdperson=%i eligible=%i hidden=%i local_drawn=%i user2=%i\n",
			s_player_proof.local_chase, s_player_proof.local_ineye,
			s_player_proof.viewentity, s_player_proof.local_index,
			s_player_proof.local_thirdperson, s_player_proof.local_eligible,
			s_player_proof.local_hidden_viewentity, s_player_proof.local_drawn, g_iUser2 );
	}
	if( !s_shadow_logged && s_player_proof.shadow_drawn > 0 && s_player_proof.r_shadows_on
		&& s_player_proof.remote_shadow_pixel )
	{
		s_shadow_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: explicit shadow r_shadows=1 candidates=%i drawn=%i rejected_trace=%i attempts=%i side_draw=%i after_body=%08x after_shadow=%08x differ=1\n",
			s_player_proof.shadow_candidates, s_player_proof.shadow_drawn,
			s_player_proof.shadow_rejected_trace, s_player_proof.shadow_trace_attempts,
			s_player_proof.shadow_side_draw, s_player_proof.crc_after_player_body,
			s_player_proof.crc_after_shadow );
	}
	{
		int frame_shadow = s_player_proof.shadow_drawn - shadow_drawn_start;
		(void)remote_drawn_start;
		if( !s_shadow_off_logged && !s_player_proof.r_shadows_on && s_player_proof.drawn > 0 )
		{
			s_shadow_off_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: explicit shadow r_shadows=0 drawn=%i candidates=%i body_drawn=%i\n",
				frame_shadow, s_player_proof.shadow_candidates, s_player_proof.drawn );
		}
	}

	if( stats )
		CSRETRO_Scene_GetStats( stats );
	return drawn;
}

static int FollowChildEligible( const CSRETRO_EntCopy *e )
{
	if( !e || !e->is_follow )
		return 0;
	if( e->kind != CSRETRO_KIND_STUDIO )
		return 0;
	if( e->type != ET_NORMAL )
		return 0;
	if( e->player || e->is_viewmodel || e->is_preview )
		return 0;
	if( e->snap_index < 0 )
		return 0;
	return 1;
}

static int ParentIsPlayer( const CSRETRO_EntCopy *parent, const cl_entity_t *psnap )
{
	if( !parent )
		return 0;
	if( parent->player || parent->kind == CSRETRO_KIND_STUDIO_LOCAL )
		return 1;
	if( psnap && psnap->player )
		return 1;
	return 0;
}

static int ParentUsableNonplayer( const CSRETRO_EntCopy *parent, const cl_entity_t *psnap )
{
	if( !parent || !psnap || !psnap->model )
		return 0;
	if( parent->kind != CSRETRO_KIND_STUDIO )
		return 0;
	if( ParentIsPlayer( parent, psnap ) )
		return 0;
	if( parent->is_follow || parent->is_viewmodel || parent->is_preview )
		return 0;
	if( parent->snap_index < 0 )
		return 0;
	if( psnap->curstate.renderfx == kRenderFxDeadPlayer )
		return 0;
	return 1;
}

static int DrawPlayerParentBones( cl_entity_t *psnap, cl_entity_t *live )
{
	player_info_t *live_info;
	player_info_t local_info;
	entity_state_t pplayer;
	unsigned int h_before, h_after, e_before, e_after;
	int ok;

	if( !psnap || !psnap->model )
		return 0;

	live_info = LivePlayerInfo( psnap );
	if( !live_info )
		return 0;

	h_before = HashPlayerInfo( live_info );
	e_before = HashEntityMut( live );
	local_info = *live_info;
	pplayer = psnap->curstate;
	if( pplayer.number <= 0 )
		pplayer.number = psnap->index;

	gRenderAPI.R_SetCurrentEntity( psnap );
	ok = g_StudioRenderer.StudioDrawPlayerOffscreen( 0, &pplayer, &local_info );

	h_after = HashPlayerInfo( live_info );
	e_after = HashEntityMut( live );
	if( h_after != h_before )
		s_player_proof.info_mutate = 1;
	if( live && e_after != e_before )
		s_player_proof.entity_mutate = 1;
	if( g_StudioRenderer.OffscreenEventsFired() )
		s_player_proof.events = 1;
	if( g_StudioRenderer.OffscreenShadowsDrawn() )
		s_player_proof.shadow_side_draw = 1;

	return ok;
}

int CSRETRO_Studio_DrawFollow( CSRETRO_SceneStats *stats )
{
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	int i, n, drawn = 0;
	static int s_logged = 0;

	if( !gRenderAPI.R_SetCurrentEntity )
		return 0;

	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		const CSRETRO_EntCopy *parent;
		cl_entity_t *child;
		cl_entity_t *psnap;
		int ok;

		if( !FollowChildEligible( e ) )
			continue;

		child = CSRETRO_Scene_StudioSnap( e->snap_index );
		if( !child || !child->model )
		{
			CSRETRO_Scene_NoteFollowMissing();
			continue;
		}
		if( child->curstate.renderfx == kRenderFxDeadPlayer )
		{
			CSRETRO_Scene_NoteFollowMissing();
			continue;
		}

		parent = CSRETRO_Scene_FindByIndex( e->aiment );
		psnap = ( parent && parent->snap_index >= 0 ) ? CSRETRO_Scene_StudioSnap( parent->snap_index ) : NULL;

		if( !parent || !psnap )
		{
			CSRETRO_Scene_NoteFollowMissing();
			continue;
		}

		if( ParentIsPlayer( parent, psnap ) )
		{
			CSRETRO_Scene_NoteFollowParent( 1 );
			s_player_proof.follow_player_parent++;
			if( !DrawPlayerParentBones( psnap, parent->live ) )
				continue;
			s_player_proof.follow_player_shadow = 0;
			VectorCopy( psnap->origin, child->origin );
			VectorCopy( psnap->curstate.origin, child->curstate.origin );
			gRenderAPI.R_SetCurrentEntity( child );
			ok = g_StudioRenderer.StudioDrawModel( STUDIO_RENDER );
			if( ok )
			{
				CSRETRO_Scene_NoteDrawn( CSRETRO_KIND_STUDIO );
				CSRETRO_Scene_NoteFollowDrawn();
				s_player_proof.follow_player_drawn++;
				drawn++;
				if( !s_logged && child->model->name[0] )
				{
					s_logged = 1;
					gEngfuncs.Con_Printf(
						"CS Retro: offscreen FOLLOW player-parent child=%s index=%i parent=%i events=0\n",
						child->model->name, child->index, parent->index );
				}
			}
			continue;
		}

		if( !ParentUsableNonplayer( parent, psnap ) )
		{
			CSRETRO_Scene_NoteFollowMissing();
			continue;
		}

		CSRETRO_Scene_NoteFollowParent( 0 );

		// Bone cache only. Never STUDIO_RENDER / STUDIO_EVENTS. Snapshot only.
		gRenderAPI.R_SetCurrentEntity( psnap );
		if( !g_StudioRenderer.StudioDrawModel( 0 ) )
			continue;

		VectorCopy( psnap->origin, child->origin );
		VectorCopy( psnap->curstate.origin, child->curstate.origin );

		gRenderAPI.R_SetCurrentEntity( child );
		ok = g_StudioRenderer.StudioDrawModel( STUDIO_RENDER );
		if( ok )
		{
			CSRETRO_Scene_NoteDrawn( CSRETRO_KIND_STUDIO );
			CSRETRO_Scene_NoteFollowDrawn();
			drawn++;
			if( !s_logged && child->model->name[0] )
			{
				s_logged = 1;
				gEngfuncs.Con_Printf(
					"CS Retro: offscreen FOLLOW draw child=%s index=%i parent=%i events=0\n",
					child->model->name, child->index, parent->index );
			}
		}
	}

	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );

	if( stats )
		CSRETRO_Scene_GetStats( stats );
	return drawn;
}

void CSRETRO_Studio_GetPlayerProof( CSRETRO_StudioPlayerProof *out )
{
	if( !out )
		return;
	*out = s_player_proof;
}

int CSRETRO_Studio_ProbeLookTarget( float *origin )
{
	if( !origin || !s_player_proof.look_ready )
		return 0;
	origin[0] = s_player_proof.look_origin[0];
	origin[1] = s_player_proof.look_origin[1];
	origin[2] = s_player_proof.look_origin[2];
	return 1;
}

void CSRETRO_Studio_ResetPlayerProof( void )
{
	memset( &s_player_proof, 0, sizeof( s_player_proof ) );
	s_player_logged = 0;
	s_local_logged = 0;
	s_pending_visible = 0;
	s_pending_index = -1;
}
