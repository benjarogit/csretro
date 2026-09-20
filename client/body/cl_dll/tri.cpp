//========= Copyright ? 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any
#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "particleman.h"
#include "IParticleMan_Active.h"
#include "environment.h"
#include "events.h"
#include "csretro_render.h"
#include "render_backend.h"

extern int g_iWaterLevel;

FogParameters g_FogParameters;

static int s_tri_normal_export;
static int s_tri_trans_export;
static int s_tri_pman_adv;
static int s_tri_pman_render;
static int s_tri_env_adv;
static int s_tri_molotov_adv;
static int s_tri_drawonly_normal;
static int s_tri_drawonly_trans;
static int s_tri_xash_logged;
static int s_tri_pman_drawonly_logged;
static int s_tri_pman_advance_logged;
static int s_tri_overview_drawonly_logged;
static unsigned int s_tri_pman_drawonly_hash;
static unsigned int s_tri_overview_drawonly_hash;
static int s_tri_frame_noted;

static IParticleMan_Active *ActiveParticleMan( void )
{
	return static_cast<IParticleMan_Active *>( g_pParticleMan );
}

static void NoteExport( int trans )
{
	if ( trans )
		s_tri_trans_export++;
	else
		s_tri_normal_export++;
}

void CSRETRO_ClientTriangles_BeginFrame( void )
{
	s_tri_normal_export = 0;
	s_tri_trans_export = 0;
	s_tri_pman_adv = 0;
	s_tri_pman_render = 0;
	s_tri_env_adv = 0;
	s_tri_molotov_adv = 0;
	s_tri_drawonly_normal = 0;
	s_tri_drawonly_trans = 0;
	s_tri_frame_noted = 1;
}

void RenderFog()
{
	FogParameters fog;

	fog = g_FogParameters;

	if( cl_fog_density )
		fog.density = cl_fog_density->value;

	if( cl_fog_r )
		fog.color[0] = cl_fog_r->value;

	if( cl_fog_g )
		fog.color[1] = cl_fog_g->value;

	if( cl_fog_b )
		fog.color[2] = cl_fog_b->value;
	
	gEngfuncs.pTriAPI->FogParams( fog.density, fog.affectsSkyBox );
	gEngfuncs.pTriAPI->Fog( fog.color, 100.0f, 2000.0f, g_iWaterLevel <= 1 ? fog.density > 0.0f : 0 );
}

void CSRETRO_ClientTriangles_AdvanceNormal( void )
{
	gHUD.m_Spectator.AdvanceOverviewState();
}

void CSRETRO_ClientTriangles_DrawNormalOnly( void )
{
	unsigned int before = 0, after = 0;
	const int wants = gHUD.m_Spectator.OverviewShouldDraw() ? 1 : 0;

	s_tri_drawonly_normal++;
	if ( wants && !s_tri_overview_drawonly_logged )
		before = gHUD.m_Spectator.OverviewStateHash();

	gHUD.m_Spectator.DrawOverviewReadOnly( false );

	if ( wants && !s_tri_overview_drawonly_logged )
	{
		after = gHUD.m_Spectator.OverviewStateHash();
		s_tri_overview_drawonly_logged = 1;
		s_tri_overview_drawonly_hash = after;
		gEngfuncs.Con_Printf( "CS Retro: tri overview draw-only reached\n" );
		gEngfuncs.Con_Printf(
			"CS Retro: tri overview state before=%08x after=%08x mutate=%i\n",
			before, after, before != after ? 1 : 0 );
	}
}

void CSRETRO_ClientTriangles_AdvanceParticleMan( void )
{
	IParticleMan_Active *pman = ActiveParticleMan();
	unsigned int after;

	if ( !pman )
		return;

	s_tri_pman_adv++;
	pman->Advance();

	if ( !s_tri_pman_advance_logged )
	{
		s_tri_pman_advance_logged = 1;
		after = pman->StateHash();
		gEngfuncs.Con_Printf(
			"CS Retro: tri pman advance reached count=%i hash=%08x vs_drawonly=%08x advanced=%i\n",
			pman->ParticleCount(), after, s_tri_pman_drawonly_hash,
			s_tri_pman_drawonly_hash ? ( after != s_tri_pman_drawonly_hash ? 1 : 0 ) : 1 );
	}
}

void CSRETRO_ClientTriangles_RenderParticleMan( int update_pvs_cache )
{
	IParticleMan_Active *pman = ActiveParticleMan();

	if ( !pman )
		return;

	s_tri_pman_render++;
	pman->Render( update_pvs_cache != 0 );
}

void CSRETRO_ClientTriangles_AdvanceEnvironment( void )
{
	s_tri_env_adv++;
	g_Environment.Update();
}

void CSRETRO_ClientTriangles_AdvanceMolotovHeld( void )
{
	s_tri_molotov_adv++;
	EV_UpdateMolotovHeld();
}

void CSRETRO_ClientTriangles_DrawTransparentOnly( void )
{
	IParticleMan_Active *pman = ActiveParticleMan();
	unsigned int before = 0, after = 0;
	int count = pman ? pman->ParticleCount() : 0;

	s_tri_drawonly_trans++;
	if ( s_tri_drawonly_trans == 1 )
		gEngfuncs.Con_Printf( "CS Retro: tri draw-only reached trans=1\n" );

	if ( !s_tri_pman_drawonly_logged && pman )
		before = pman->StateHash();

	CSRETRO_Backend_PushFog();
	RenderFog();
	CSRETRO_ClientTriangles_RenderParticleMan( 0 );
	CSRETRO_Backend_PopFog();

	if ( !s_tri_pman_drawonly_logged && pman )
	{
		after = pman->StateHash();
		s_tri_pman_drawonly_logged = 1;
		s_tri_pman_drawonly_hash = after;
		gEngfuncs.Con_Printf( "CS Retro: tri pman draw-only reached count=%i\n", count );
		gEngfuncs.Con_Printf(
			"CS Retro: tri pman state before=%08x after=%08x mutate=%i count=%i\n",
			before, after, before != after ? 1 : 0, count );
	}
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles( void )
{
	NoteExport( 0 );
	CSRETRO_ClientTriangles_AdvanceNormal();
	gHUD.m_Spectator.DrawOverviewReadOnly( true );
}

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
	NoteExport( 1 );
	RenderFog();

	if ( g_pParticleMan )
	{
		CSRETRO_ClientTriangles_AdvanceParticleMan();
		CSRETRO_ClientTriangles_RenderParticleMan( 1 );
		CSRETRO_ClientTriangles_AdvanceEnvironment();
	}

	CSRETRO_ClientTriangles_AdvanceMolotovHeld();

	if ( !s_tri_xash_logged && s_tri_drawonly_trans == 0 )
	{
		s_tri_xash_logged = 1;
		gEngfuncs.Con_Printf(
			"CS Retro: tri xash export reached normal=%i trans=%i pman_adv=%i env=%i molotov=%i\n",
			s_tri_normal_export, s_tri_trans_export, s_tri_pman_adv, s_tri_env_adv, s_tri_molotov_adv );
		if ( s_tri_normal_export > 1 || s_tri_trans_export > 1 || s_tri_pman_adv > 1
			|| s_tri_env_adv > 1 || s_tri_molotov_adv > 1 )
			gEngfuncs.Con_Printf(
				"CS Retro: tri xash export extra normal=%i trans=%i pman_adv=%i env=%i molotov=%i\n",
				s_tri_normal_export, s_tri_trans_export, s_tri_pman_adv, s_tri_env_adv, s_tri_molotov_adv );
	}
}
