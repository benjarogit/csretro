#include "render_takeover.h"
#include "render_scene.h"
#include "render_xash_sprite.h"

#include "hud.h"
#include "cl_util.h"
#include "ref_params.h"

#include <string.h>

static CSRETRO_TakeoverProof s_proof;
static int s_fault_latched = 0;
static unsigned int s_reject_logged_mask = 0;

int CSRETRO_Renderer_Mode( void )
{
	float v = CVAR_GET_FLOAT( "r_csretro_renderer" );
	if( v >= 1.5f )
		return 2;
	if( v >= 0.5f )
		return 1;
	return 0;
}

int CSRETRO_Scene_HasAlias( void )
{
	int i, n = CSRETRO_Scene_Count();
	for( i = 0; i < n; i++ )
	{
		const CSRETRO_EntCopy *e = CSRETRO_Scene_Get( i );
		if( e && e->model_type == XR_MOD_ALIAS )
			return 1;
	}
	return 0;
}

int CSRETRO_Takeover_FaultLatched( void )
{
	return s_fault_latched;
}

void CSRETRO_Takeover_LatchFault( void )
{
	s_fault_latched = 1;
	s_proof.fault_latched = 1;
}

void CSRETRO_Takeover_ClearFault( void )
{
	s_fault_latched = 0;
	s_proof.fault_latched = 0;
}

void CSRETRO_Takeover_ResetProof( void )
{
	memset( &s_proof, 0, sizeof( s_proof ) );
	s_proof.fault_latched = s_fault_latched;
}

void CSRETRO_Takeover_GetProof( CSRETRO_TakeoverProof *out )
{
	if( out )
		*out = s_proof;
}

void CSRETRO_Takeover_NoteProof( const CSRETRO_TakeoverProof *src )
{
	if( !src )
		return;
	s_proof = *src;
	s_proof.fault_latched = s_fault_latched;
}

int CSRETRO_Takeover_Eligible( const struct ref_viewpass_s *rvp, int *reason_out )
{
	int reason = CSRETRO_TAKEOVER_OK;
	float ripple;

	if( s_fault_latched )
		reason = CSRETRO_TAKEOVER_REJECT_FAULT_LATCH;
	else if( !rvp )
		reason = CSRETRO_TAKEOVER_REJECT_FLAGS;
	else if( !( rvp->flags & RF_DRAW_WORLD ) )
		reason = CSRETRO_TAKEOVER_REJECT_FLAGS;
	else if( rvp->flags & RF_ONLY_CLIENTDRAW )
		reason = CSRETRO_TAKEOVER_REJECT_CLIENTDRAW;
	else if( rvp->flags & RF_DRAW_CUBEMAP )
		reason = CSRETRO_TAKEOVER_REJECT_CUBEMAP;
	else if( rvp->flags & RF_DRAW_OVERVIEW )
		reason = CSRETRO_TAKEOVER_REJECT_OVERVIEW;
	else if( rvp->viewport[2] <= 0 || rvp->viewport[3] <= 0 )
		reason = CSRETRO_TAKEOVER_REJECT_VIEWPORT;
	else
	{
		ripple = CVAR_GET_FLOAT( "r_ripple" );
		if( ripple != 0.0f )
			reason = CSRETRO_TAKEOVER_REJECT_RIPPLE;
		else if( CSRETRO_Scene_HasAlias() )
			reason = CSRETRO_TAKEOVER_REJECT_ALIAS;
	}

	if( reason_out )
		*reason_out = reason;
	if( reason != CSRETRO_TAKEOVER_OK )
	{
		unsigned int bit = 1u << (unsigned)reason;
		if( !( s_reject_logged_mask & bit ) )
		{
			s_reject_logged_mask |= bit;
			if( reason == CSRETRO_TAKEOVER_REJECT_FLAGS && rvp && !( rvp->flags & RF_DRAW_WORLD ) )
				gEngfuncs.Con_Printf(
					"CS Retro: PX6A takeover reject reason=%i pass=preview/non-world (pre-commit, advances=0)\n",
					reason );
			else
				gEngfuncs.Con_Printf( "CS Retro: PX6A takeover reject reason=%i (pre-commit, advances=0)\n", reason );
		}
	}
	return reason == CSRETRO_TAKEOVER_OK ? 1 : 0;
}
