// Offscreen studio. GSMR stays the CS renderer.
// Remote + eligible local: local player_info_t copy only. Live advances on visible Xash.
// Local world-draw = CL_IsThirdPerson() || index != rvp->viewentity.
// Explicit StudioDrawPlayerShadow after STUDIO_RENDER; never inside Offscreen.
// STUDIO_EVENTS never. No live entity across frames — local snapshot only.
#include "render_studio.h"
#include "render_scene.h"
#include "render_backend.h"
#include "render_trans.h"

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
#include "events.h"

#include <string.h>
#include <math.h>

extern engine_studio_api_t IEngineStudio;
extern int g_iUser1;
extern int g_iUser2;

#ifndef GL_DEPTH_RANGE
#define GL_DEPTH_RANGE 0x0B70
#endif
#ifndef GL_DEPTH_TEST
#define GL_DEPTH_TEST 0x0B71
#endif
#ifndef GL_BLEND
#define GL_BLEND 0x0BE2
#endif
#ifndef GL_DEPTH_WRITEMASK
#define GL_DEPTH_WRITEMASK 0x0B73
#endif
#ifndef GL_VIEWPORT
#define GL_VIEWPORT 0x0BA2
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_CULL_FACE
#define GL_CULL_FACE 0x0B44
#endif

static CSRETRO_StudioPlayerProof s_player_proof;
static CSRETRO_StudioViewmodelProof s_vm_proof;
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

static unsigned int HashViewModel( const cl_entity_t *ent )
{
	unsigned int h = 2166136261u;
	int i;

	if( !ent )
		return 0;
	h = MixFloat( h, ent->origin[0] );
	h = MixFloat( h, ent->origin[1] );
	h = MixFloat( h, ent->origin[2] );
	h = MixFloat( h, ent->angles[0] );
	h = MixFloat( h, ent->angles[1] );
	h = MixFloat( h, ent->angles[2] );
	h = MixU32( h, (unsigned int)ent->curstate.sequence );
	h = MixFloat( h, ent->curstate.frame );
	h = MixFloat( h, ent->curstate.animtime );
	for( i = 0; i < 4; i++ )
		h = MixU32( h, ent->curstate.controller[i] );
	for( i = 0; i < 4; i++ )
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
	h = MixU32( h, (unsigned int)ent->curstate.body );
	h = MixU32( h, (unsigned int)ent->curstate.rendermode );
	h = MixU32( h, (unsigned int)ent->curstate.renderfx );
	h = MixU32( h, (unsigned int)ent->curstate.renderamt );
	for( i = 0; i < 4; i++ )
	{
		h = MixFloat( h, ent->attachment[i][0] );
		h = MixFloat( h, ent->attachment[i][1] );
		h = MixFloat( h, ent->attachment[i][2] );
	}
	return h;
}

static unsigned int HashAttachments( const cl_entity_t *ent )
{
	unsigned int h = 2166136261u;
	int i;

	if( !ent )
		return 0;
	for( i = 0; i < 4; i++ )
	{
		h = MixFloat( h, ent->attachment[i][0] );
		h = MixFloat( h, ent->attachment[i][1] );
		h = MixFloat( h, ent->attachment[i][2] );
	}
	return h;
}

static unsigned int HashWickState( void )
{
	unsigned int h = 2166136261u;
	float org[3];
	float time = 0.0f;
	int valid = 0;

	memset( org, 0, sizeof( org ) );
	EV_ReadMolotovWickState( org, &time, &valid );
	h = MixFloat( h, org[0] );
	h = MixFloat( h, org[1] );
	h = MixFloat( h, org[2] );
	h = MixFloat( h, time );
	h = MixU32( h, (unsigned int)valid );
	return h;
}

static void NoteViewmodelWeapon( const char *name )
{
	if( !name || !name[0] )
		return;
	if( strstr( name, "v_glock" ) || strstr( name, "v_usp" ) || strstr( name, "v_deagle" ) )
		s_vm_proof.weapons_pistol = 1;
	if( strstr( name, "v_ak47" ) || strstr( name, "v_m4a1" ) || strstr( name, "v_aug" ) || strstr( name, "v_galil" ) || strstr( name, "v_famas" ) )
		s_vm_proof.weapons_rifle = 1;
	if( strstr( name, "v_knife" ) )
		s_vm_proof.weapons_knife = 1;
	if( strstr( name, "v_hegrenade" ) )
		s_vm_proof.weapons_he = 1;
	if( strstr( name, "v_smokegrenade" ) )
		s_vm_proof.weapons_smoke = 1;
	if( strstr( name, "v_flashbang" ) )
		s_vm_proof.weapons_flash = 1;
	if( strstr( name, "v_molotov" ) )
		s_vm_proof.weapons_molotov = 1;
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

static int DrawStudioRange( int only_index, CSRETRO_SceneStats *stats )
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

		if( only_index >= 0 && i != only_index )
			continue;
		if( !Eligible( e ) )
			continue;
		if( only_index < 0 && CSRETRO_Trans_DrawClass( i ) != CSRETRO_DRAW_OPAQUE )
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
			CSRETRO_Trans_NoteDrawn( i );
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

int CSRETRO_Studio_DrawList( CSRETRO_SceneStats *stats )
{
	return DrawStudioRange( -1, stats );
}

int CSRETRO_Studio_DrawOne( int scene_index, CSRETRO_SceneStats *stats )
{
	return DrawStudioRange( scene_index, stats );
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
		if( CSRETRO_Trans_DrawClass( i ) != CSRETRO_DRAW_OPAQUE )
			continue;
		if( DrawIsolatedPlayer( e, 0, 1 ) )
		{
			CSRETRO_Trans_NoteDrawn( i );
			drawn++;
		}
	}

	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );

		if( !EligibleLocalPlayer( e ) )
			continue;
		if( CSRETRO_Trans_DrawClass( i ) != CSRETRO_DRAW_OPAQUE )
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
			CSRETRO_Trans_NoteDrawn( i );
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

int CSRETRO_Studio_DrawPlayerOne( int scene_index, CSRETRO_SceneStats *stats, const ref_viewpass_t *rvp )
{
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	const CSRETRO_EntCopy *e;
	int ok = 0;

	e = CSRETRO_Scene_Get( scene_index );
	if( !e || !gRenderAPI.R_SetCurrentEntity )
		return 0;

	ClassifyLocalPlayer( rvp );
	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	if( EligibleRemotePlayer( e ) )
	{
		ok = DrawIsolatedPlayer( e, 0, 1 );
		if( ok )
			CSRETRO_Trans_NoteDrawn( scene_index );
	}
	else if( EligibleLocalPlayer( e ) )
	{
		if( !LocalWorldDrawEligible( e, rvp ) )
		{
			s_player_proof.local_hidden_viewentity++;
			ok = 0;
		}
		else
		{
			s_player_proof.local_eligible++;
			ok = DrawIsolatedPlayer( e, 1, 1 );
			if( ok )
				CSRETRO_Trans_NoteDrawn( scene_index );
		}
	}

	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );
	if( stats )
		CSRETRO_Scene_GetStats( stats );
	return ok;
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

void CSRETRO_Studio_GetViewmodelProof( CSRETRO_StudioViewmodelProof *out )
{
	if( !out )
		return;
	*out = s_vm_proof;
}

void CSRETRO_Studio_ResetViewmodelProof( void )
{
	memset( &s_vm_proof, 0, sizeof( s_vm_proof ) );
	g_StudioRenderer.ResetOffscreenViewmodelProof();
}

int CSRETRO_Studio_DrawViewmodel( const ref_viewpass_t *rvp )
{
	cl_entity_t *live_vm;
	cl_entity_t vm_snapshot;
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	cl_entity_t *local;
	cvar_t *drawvm;
	cvar_t *righthand;
	float depth_saved[2];
	float depth_during[2];
	float depth_after[2];
	unsigned char depth_test_before = 0, depth_test_after = 0;
	unsigned char depth_mask_before = 0, depth_mask_after = 0;
	unsigned char blend_before = 0, blend_after = 0;
	int viewport_before[4], viewport_after[4];
	int fbo_before = 0, fbo_after = 0;
	int flags;
	int thirdperson = 0;
	int health = 0;
	int ok;
	int events_before;
	int wick_attempts_before;
	int wick_captures_before;

	if( !gRenderAPI.R_SetCurrentEntity )
		return 0;

	s_vm_proof.drawn_frame = 0;
	s_vm_proof.eligible = 0;

	live_vm = gEngfuncs.GetViewModel();
	if( live_vm && live_vm->model )
	{
		s_vm_proof.candidates = 1;
		if( live_vm->model->type == mod_studio )
			s_vm_proof.studio_candidates = 1;
		else
			s_vm_proof.alias_seen = 1;
		strncpy( s_vm_proof.model, live_vm->model->name, sizeof( s_vm_proof.model ) - 1 );
		NoteViewmodelWeapon( live_vm->model->name );
		if( strstr( live_vm->model->name, "v_molotov" ) )
			s_vm_proof.wick_candidate = 1;
	}

	flags = rvp ? rvp->flags : 0;
	s_vm_proof.draw_world = ( flags & RF_DRAW_WORLD ) ? 1 : 0;
	s_vm_proof.only_clientdraw = ( flags & RF_ONLY_CLIENTDRAW ) ? 1 : 0;
	s_vm_proof.cubemap = ( flags & RF_DRAW_CUBEMAP ) ? 1 : 0;
	if( gRenderAPI.RenderGetParm )
	{
		thirdperson = (int)gRenderAPI.RenderGetParm( PARM_THIRDPERSON, 0 );
		health = (int)gRenderAPI.RenderGetParm( PARM_LOCAL_HEALTH, 0 );
	}
	s_vm_proof.thirdperson = thirdperson;
	s_vm_proof.health = health;
	drawvm = gEngfuncs.pfnGetCvarPointer( "r_drawviewmodel" );
	s_vm_proof.drawviewmodel = ( drawvm && drawvm->value != 0.0f ) ? 1 : 0;
	local = gEngfuncs.GetLocalPlayer();
	s_vm_proof.local_index = local ? local->index : 0;
	s_vm_proof.viewentity = rvp ? rvp->viewentity : 0;

	if( !( flags & RF_DRAW_WORLD ) )
		return 0;
	if( flags & RF_ONLY_CLIENTDRAW )
		return 0;
	if( !s_vm_proof.drawviewmodel )
		return 0;
	if( thirdperson )
		return 0;
	if( flags & RF_DRAW_CUBEMAP )
		return 0;
	if( health <= 0 )
		return 0;
	if( !local || !rvp || rvp->viewentity != local->index )
		return 0;
	if( !live_vm || !live_vm->model || live_vm->model->type != mod_studio )
		return 0;

	s_vm_proof.eligible = 1;
	vm_snapshot = *live_vm;
	s_vm_proof.live_hash_before = HashViewModel( live_vm );
	s_vm_proof.wick_hash_before = HashWickState();
	s_vm_proof.sequence = live_vm->curstate.sequence;
	s_vm_proof.frame = live_vm->curstate.frame;
	righthand = gHUD.cl_righthand;
	s_vm_proof.righthand_before = righthand ? righthand->value : 0.0f;

	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	memset( depth_saved, 0, sizeof( depth_saved ) );
	memset( depth_during, 0, sizeof( depth_during ) );
	memset( depth_after, 0, sizeof( depth_after ) );
	memset( viewport_before, 0, sizeof( viewport_before ) );
	memset( viewport_after, 0, sizeof( viewport_after ) );
	if( gXRGL.GetFloatv )
		gXRGL.GetFloatv( GL_DEPTH_RANGE, depth_saved );
	if( gXRGL.IsEnabled )
	{
		depth_test_before = gXRGL.IsEnabled( GL_DEPTH_TEST );
		blend_before = gXRGL.IsEnabled( GL_BLEND );
	}
	if( gXRGL.GetBooleanv )
		gXRGL.GetBooleanv( GL_DEPTH_WRITEMASK, &depth_mask_before );
	if( gXRGL.GetIntegerv )
	{
		gXRGL.GetIntegerv( GL_VIEWPORT, viewport_before );
		gXRGL.GetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo_before );
	}

	if( gXRGL.DepthRange )
		gXRGL.DepthRange( (double)depth_saved[0], (double)( depth_saved[0] + 0.3f * ( depth_saved[1] - depth_saved[0] ) ) );
	if( gXRGL.GetFloatv )
		gXRGL.GetFloatv( GL_DEPTH_RANGE, depth_during );

	events_before = g_StudioRenderer.OffscreenViewmodelEvents();
	wick_attempts_before = g_StudioRenderer.OffscreenWickAttempts();
	wick_captures_before = g_StudioRenderer.OffscreenWickCaptures();

	gRenderAPI.R_SetCurrentEntity( &vm_snapshot );
	// Snapshot only. Never live. STUDIO_RENDER only. GSMR bone cache is overwritten
	// here after Player/FOLLOW; visible Xash rebuilds its own studio context.
	ok = g_StudioRenderer.StudioDrawViewmodelOffscreen( STUDIO_RENDER );

	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );

	if( gXRGL.DepthRange )
		gXRGL.DepthRange( (double)depth_saved[0], (double)depth_saved[1] );
	if( gXRGL.GetFloatv )
		gXRGL.GetFloatv( GL_DEPTH_RANGE, depth_after );

	s_vm_proof.depth_before[0] = depth_saved[0];
	s_vm_proof.depth_before[1] = depth_saved[1];
	s_vm_proof.depth_during[0] = depth_during[0];
	s_vm_proof.depth_during[1] = depth_during[1];
	s_vm_proof.depth_after[0] = depth_after[0];
	s_vm_proof.depth_after[1] = depth_after[1];
	s_vm_proof.depth_restore = ( depth_after[0] == depth_saved[0] && depth_after[1] == depth_saved[1] ) ? 1 : 0;

	if( gXRGL.IsEnabled )
	{
		depth_test_after = gXRGL.IsEnabled( GL_DEPTH_TEST );
		blend_after = gXRGL.IsEnabled( GL_BLEND );
	}
	if( gXRGL.GetBooleanv )
		gXRGL.GetBooleanv( GL_DEPTH_WRITEMASK, &depth_mask_after );
	if( gXRGL.GetIntegerv )
	{
		gXRGL.GetIntegerv( GL_VIEWPORT, viewport_after );
		gXRGL.GetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo_after );
	}
	s_vm_proof.gl_restore = ( depth_test_before == depth_test_after
		&& depth_mask_before == depth_mask_after
		&& blend_before == blend_after
		&& viewport_before[0] == viewport_after[0]
		&& viewport_before[1] == viewport_after[1]
		&& viewport_before[2] == viewport_after[2]
		&& viewport_before[3] == viewport_after[3]
		&& fbo_before == fbo_after ) ? 1 : 0;

	s_vm_proof.live_hash_after = HashViewModel( live_vm );
	s_vm_proof.snap_hash_after = HashViewModel( &vm_snapshot );
	s_vm_proof.wick_hash_after = HashWickState();
	s_vm_proof.live_mutate = ( s_vm_proof.live_hash_before != s_vm_proof.live_hash_after ) ? 1 : 0;
	s_vm_proof.wick_mutate = ( s_vm_proof.wick_hash_before != s_vm_proof.wick_hash_after ) ? 1 : 0;
	s_vm_proof.righthand_after = righthand ? righthand->value : 0.0f;
	s_vm_proof.righthand_mutate = ( s_vm_proof.righthand_before != s_vm_proof.righthand_after ) ? 1 : 0;
	if( g_StudioRenderer.OffscreenViewmodelEvents() != events_before )
		s_vm_proof.events = 1;
	s_vm_proof.wick_attempts = g_StudioRenderer.OffscreenWickAttempts();
	s_vm_proof.wick_captures = g_StudioRenderer.OffscreenWickCaptures();
	if( g_StudioRenderer.OffscreenWickCaptures() != wick_captures_before )
		s_vm_proof.wick_captures = g_StudioRenderer.OffscreenWickCaptures();
	(void)wick_attempts_before;
	s_vm_proof.special_flip = g_StudioRenderer.ViewmodelSpecialFlip();
	s_vm_proof.shield_detected = g_StudioRenderer.ViewmodelShieldDetected();
	s_vm_proof.event_wick_captures = g_StudioRenderer.EventWickCaptures();
	s_vm_proof.visible_body_wick_captures = g_StudioRenderer.VisibleBodyWickCaptures();
	s_vm_proof.studio_event_deliveries = CSRETRO_ViewmodelStudioEventsDelivered();
	s_vm_proof.molotov_held_advances = EV_MolotovHeldAdvances();
	s_vm_proof.wick_source_captured = EV_MolotovHeldWickCaptured();
	s_vm_proof.wick_age = EV_MolotovHeldWickAge();

	if( ok )
	{
		s_vm_proof.drawn++;
		s_vm_proof.drawn_frame = 1;
	}

	return ok ? 1 : 0;
}

int CSRETRO_Studio_ClaimViewmodelEvents( const ref_viewpass_t *rvp )
{
	cl_entity_t *saved_ent = NULL;
	struct model_s *saved_model = NULL;
	cl_entity_t *after_ent = NULL;
	cl_entity_t *live_vm;
	float depth_before[2], depth_after[2];
	unsigned char blend_before = 0, blend_after = 0;
	unsigned char cull_before = 0, cull_after = 0;
	int fbo_before = 0, fbo_after = 0;
	int flags;
	int first_rc;
	int second_rc = -2;
	int eligible = 0;
	int reason = CSRETRO_VM_EVENT_OK;
	static int s_stress_done = 0;
	static unsigned int s_reject_logged_mask = 0;

	if( !gRenderAPI.RunViewmodelEventsOnce )
		return 0;

	flags = rvp ? rvp->flags : 0;
	if( flags & RF_ONLY_CLIENTDRAW )
		return 0;

	eligible = CSRETRO_Studio_EventEligible( rvp, &reason );

	live_vm = gEngfuncs.GetViewModel();
	if( IEngineStudio.GetCurrentEntity )
		saved_ent = IEngineStudio.GetCurrentEntity();
	if( saved_ent )
		saved_model = saved_ent->model;

	memset( depth_before, 0, sizeof( depth_before ) );
	memset( depth_after, 0, sizeof( depth_after ) );
	if( gXRGL.GetFloatv )
		gXRGL.GetFloatv( GL_DEPTH_RANGE, depth_before );
	if( gXRGL.IsEnabled )
	{
		blend_before = gXRGL.IsEnabled( GL_BLEND );
		cull_before = gXRGL.IsEnabled( GL_CULL_FACE );
	}
	if( gXRGL.GetIntegerv )
		gXRGL.GetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo_before );

	s_vm_proof.attach_hash_a = HashAttachments( live_vm );
	first_rc = gRenderAPI.RunViewmodelEventsOnce();
	s_vm_proof.attach_hash_b = HashAttachments( live_vm );
	if( !s_stress_done )
	{
		second_rc = gRenderAPI.RunViewmodelEventsOnce();
		s_vm_proof.attach_hash_c = HashAttachments( live_vm );
		s_vm_proof.event_second_rc = second_rc;
		s_stress_done = 1;
	}
	else
	{
		s_vm_proof.attach_hash_c = s_vm_proof.attach_hash_b;
		s_vm_proof.event_second_rc = -1;
	}

	if( IEngineStudio.GetCurrentEntity )
		after_ent = IEngineStudio.GetCurrentEntity();
	gRenderAPI.R_SetCurrentEntity( saved_ent );
	if( gRenderAPI.R_SetCurrentModel )
		gRenderAPI.R_SetCurrentModel( saved_model );

	if( gXRGL.GetFloatv )
		gXRGL.GetFloatv( GL_DEPTH_RANGE, depth_after );
	if( gXRGL.IsEnabled )
	{
		blend_after = gXRGL.IsEnabled( GL_BLEND );
		cull_after = gXRGL.IsEnabled( GL_CULL_FACE );
	}
	if( gXRGL.GetIntegerv )
		gXRGL.GetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo_after );

	s_vm_proof.event_claimed = 1;
	s_vm_proof.event_first_rc = first_rc;
	s_vm_proof.event_impl_ran = ( first_rc == 1 ) ? 1 : 0;
	s_vm_proof.event_eligible = eligible;
	s_vm_proof.event_reject_reason = reason;
	s_vm_proof.attach_b_eq_c = ( s_vm_proof.attach_hash_b == s_vm_proof.attach_hash_c ) ? 1 : 0;
	s_vm_proof.event_currententity_restore = ( !IEngineStudio.GetCurrentEntity
		|| IEngineStudio.GetCurrentEntity() == saved_ent ) ? 1 : 0;
	(void)after_ent;
	s_vm_proof.event_gl_restore = ( depth_before[0] == depth_after[0]
		&& depth_before[1] == depth_after[1]
		&& blend_before == blend_after
		&& cull_before == cull_after
		&& fbo_before == fbo_after ) ? 1 : 0;
	s_vm_proof.event_wick_captures = g_StudioRenderer.EventWickCaptures();
	s_vm_proof.visible_body_wick_captures = g_StudioRenderer.VisibleBodyWickCaptures();
	s_vm_proof.studio_event_deliveries = CSRETRO_ViewmodelStudioEventsDelivered();

	if( first_rc == 0 )
	{
		unsigned int bit = 1u << (unsigned)reason;
		if( !( s_reject_logged_mask & bit ) )
		{
			cvar_t *drawvm = gEngfuncs.pfnGetCvarPointer( "r_drawviewmodel" );
			cl_entity_t *local = gEngfuncs.GetLocalPlayer();
			s_reject_logged_mask |= bit;
			gEngfuncs.Con_Printf(
				"CS Retro: viewmodel events reject reason=%i eligible=%i draw_world=%i only_clientdraw=%i cubemap=%i thirdperson=%i health=%i viewentity=%i local=%i viewmodel=%i model_type=%i r_drawviewmodel=%.0f event_impl_runs=0\n",
				reason, eligible,
				( flags & RF_DRAW_WORLD ) ? 1 : 0,
				( flags & RF_ONLY_CLIENTDRAW ) ? 1 : 0,
				( flags & RF_DRAW_CUBEMAP ) ? 1 : 0,
				gRenderAPI.RenderGetParm ? (int)gRenderAPI.RenderGetParm( PARM_THIRDPERSON, 0 ) : -1,
				gRenderAPI.RenderGetParm ? (int)gRenderAPI.RenderGetParm( PARM_LOCAL_HEALTH, 0 ) : -1,
				rvp ? rvp->viewentity : 0,
				local ? local->index : 0,
				live_vm ? 1 : 0,
				( live_vm && live_vm->model ) ? live_vm->model->type : -1,
				drawvm ? drawvm->value : -1.0f );
		}
	}

	(void)second_rc;
	return first_rc;
}

static int s_lost_eligible_event_frames = 0;

int CSRETRO_Studio_EventEligible( const ref_viewpass_t *rvp, int *reason_out )
{
	int flags = rvp ? rvp->flags : 0;
	int thirdperson = 0;
	int health = 0;
	cvar_t *drawvm;
	cl_entity_t *local;
	cl_entity_t *live_vm;
	int reason = CSRETRO_VM_EVENT_OK;

	/* Mirrors R_RunViewmodelEventsImpl (+ client-only ONLY_CLIENTDRAW skip). */
	if( flags & RF_ONLY_CLIENTDRAW )
		reason = CSRETRO_VM_EVENT_REJECT_CLIENTDRAW;
	else if( flags & RF_DRAW_CUBEMAP )
		reason = CSRETRO_VM_EVENT_REJECT_CUBEMAP;
	else
	{
		if( gRenderAPI.RenderGetParm )
		{
			thirdperson = (int)gRenderAPI.RenderGetParm( PARM_THIRDPERSON, 0 );
			health = (int)gRenderAPI.RenderGetParm( PARM_LOCAL_HEALTH, 0 );
		}
		drawvm = gEngfuncs.pfnGetCvarPointer( "r_drawviewmodel" );
		local = gEngfuncs.GetLocalPlayer();
		live_vm = gEngfuncs.GetViewModel();
		if( thirdperson )
			reason = CSRETRO_VM_EVENT_REJECT_THIRDPERSON;
		else if( health <= 0 )
			reason = CSRETRO_VM_EVENT_REJECT_HEALTH;
		else if( !local || !rvp || rvp->viewentity != local->index )
			reason = CSRETRO_VM_EVENT_REJECT_VIEWENTITY;
		else if( !drawvm || drawvm->value == 0.0f )
			reason = CSRETRO_VM_EVENT_REJECT_DRAWVIEWMODEL;
		else if( !live_vm )
			reason = CSRETRO_VM_EVENT_REJECT_NO_VIEWMODEL;
		else if( !live_vm->model || live_vm->model->type != mod_studio )
			reason = CSRETRO_VM_EVENT_REJECT_MODEL_TYPE;
	}

	if( reason_out )
		*reason_out = reason;
	return reason == CSRETRO_VM_EVENT_OK ? 1 : 0;
}

int CSRETRO_Studio_LostEligibleEventFrames( void )
{
	return s_lost_eligible_event_frames;
}

void CSRETRO_Studio_NoteLostEligibleEventFrame( void )
{
	s_lost_eligible_event_frames++;
	gEngfuncs.Con_Printf(
		"CS Retro: viewmodel events LOST eligible frame lost_eligible_event_frames=%i (return 1 without event_impl)\n",
		s_lost_eligible_event_frames );
}

void CSRETRO_Studio_LogMolotovEventWick( void )
{
	int captured = EV_MolotovHeldWickCaptured();
	gEngfuncs.Con_Printf(
		"CS Retro: viewmodel events wick event_wick_capture=%i offscreen_body_wick_capture=%i visible_body_wick_capture=%i held_advances=%i lit=%i valid=%i weapon=%i source=%s age=%.3f delivered=%i\n",
		g_StudioRenderer.EventWickCaptures(),
		g_StudioRenderer.OffscreenWickCaptures(),
		g_StudioRenderer.VisibleBodyWickCaptures(),
		EV_MolotovHeldAdvances(),
		EV_MolotovHeldLit(),
		EV_MolotovHeldWickValid(),
		EV_MolotovHeldWeaponId(),
		captured ? "captured" : "fallback",
		EV_MolotovHeldWickAge(),
		CSRETRO_ViewmodelStudioEventsDelivered() );
}
