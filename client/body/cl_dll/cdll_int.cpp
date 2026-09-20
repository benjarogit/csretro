/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include "hud.h"
#include "netadr.h"
#include "pmtrace.h"

#include "pm_shared.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <dlfcn.h>
#include <link.h>
#endif
#include "interface.h"
#include "render_api.h"
#include "vgui_parser.h"
#include "cl_dll/IGameMenuExports.h"
#include "particleman.h"
#include "IParticleMan_Active.h"
#include "CMiniMem.h"
#include "environment.h"

#include "cl_util.h"
#include "csretro_render.h"

cl_enginefunc_t		gEngfuncs  = { };
render_api_t		gRenderAPI = { };
CHud gHUD;
int g_iXash = 0; // indicates a buildnum

IGameMenuExports *g_pMenu = nullptr;
IParticleMan *g_pParticleMan = NULL;

#if !defined(_WIN32)
static IGameMenuExports *ExportsFromHandle( void *handle )
{
	if( !handle )
		return nullptr;

	using GetExportsFn = IGameMenuExports *(*)( void );
	if( auto get = reinterpret_cast<GetExportsFn>( dlsym( handle, "Csretro_GetGameMenuExports" ) ) )
	{
		if( IGameMenuExports *p = get() )
			return p;
	}

	CreateInterfaceFn factory = reinterpret_cast<CreateInterfaceFn>( dlsym( handle, "CreateInterface" ) );
	if( factory )
		return static_cast<IGameMenuExports *>( factory( GAMEMENUEXPORTS_INTERFACE_VERSION, NULL ) );
	return nullptr;
}

struct MenuSoSearch
{
	char path[512];
};

static int VisitLoadedMenu( struct dl_phdr_info *info, size_t, void *data )
{
	auto *out = static_cast<MenuSoSearch *>( data );
	if( !info->dlpi_name || !info->dlpi_name[0] )
		return 0;
	if( !strstr( info->dlpi_name, "menu_amd64.so" ) && !strstr( info->dlpi_name, "libmenu.so" ) )
		return 0;
	snprintf( out->path, sizeof( out->path ), "%s", info->dlpi_name );
	return 1;
}
#endif

static IGameMenuExports *GetNativeMenuExports( void )
{
	if( gEngfuncs.pfnGetNativeObject )
	{
		if( void *direct = gEngfuncs.pfnGetNativeObject( "GameMenuExports" ) )
			return static_cast<IGameMenuExports *>( direct );

		if( void *nativeFactory = gEngfuncs.pfnGetNativeObject( "MenuFactory" ) )
		{
			CreateInterfaceFn menuFactory = reinterpret_cast<CreateInterfaceFn>( nativeFactory );
			if( IGameMenuExports *p = static_cast<IGameMenuExports *>( menuFactory( GAMEMENUEXPORTS_INTERFACE_VERSION, NULL ) ) )
				return p;
		}
	}

#if !defined(_WIN32)
	const char *candidates[] = {
		getenv( "CSRETRO_MENU_SO" ),
		"menu_amd64.so",
		"libmenu.so",
	};
	for( const char *path : candidates )
	{
		if( !path || !path[0] )
			continue;
		if( void *handle = dlopen( path, RTLD_NOW | RTLD_NOLOAD ) )
		{
			if( IGameMenuExports *p = ExportsFromHandle( handle ) )
				return p;
		}
	}

	MenuSoSearch found = {};
	if( dl_iterate_phdr( VisitLoadedMenu, &found ) && found.path[0] )
	{
		if( void *handle = dlopen( found.path, RTLD_NOW | RTLD_NOLOAD ) )
		{
			if( IGameMenuExports *p = ExportsFromHandle( handle ) )
				return p;
		}
	}
#endif
	return nullptr;
}

void Menu_EnsureExports( void )
{
	if( g_pMenu )
		return;

	g_pMenu = GetNativeMenuExports();
	if( g_pMenu )
		gEngfuncs.Con_Printf( "CSRetro-Menu: GameMenuExports001 bereit\n" );
	else
	{
		void *factory = ( gEngfuncs.pfnGetNativeObject ) ? gEngfuncs.pfnGetNativeObject( "MenuFactory" ) : nullptr;
		gEngfuncs.Con_Printf( "CSRetro-Menu: GameMenuExports001 fehlt (GetNativeObject=%p factory=%p)\n",
			reinterpret_cast<void *>( gEngfuncs.pfnGetNativeObject ), factory );
	}
}

static void LoadMenuInterface( void )
{
	Menu_EnsureExports();
}

void InitInput (void);
void Game_HookEvents( void );
void IN_Commands( void );
void Input_Shutdown (void);
void CL_LoadParticleMan();
void CL_UnloadParticleMan();

/*
========================== 
    Initialize

Called when the DLL is first loaded.
==========================
*/
int DLLEXPORT Initialize( cl_enginefunc_t *pEnginefuncs, int iVersion )
{
	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	gEngfuncs = *pEnginefuncs;

	sscanf( CVAR_GET_STRING( "host_ver" ), "%d", &g_iXash );

	Game_HookEvents();
	CL_LoadParticleMan();

	return 1;
}


/*
=============
HUD_Shutdown

=============
*/
void DLLEXPORT HUD_Shutdown( void )
{
	CSRETRO_Renderer_Shutdown();
	gHUD.Shutdown();
	Input_Shutdown();
	Localize_Free();
	g_Environment.Clear();
	if (g_pParticleMan)
	{
		CL_UnloadParticleMan();
	}
	auto miniMem = CMiniMem::Instance();
	if (miniMem)
	{
		miniMem->Reset();
		miniMem->Shutdown();
	}
}


/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int DLLEXPORT HUD_GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	int iret = 0;

	switch ( hullnumber )
	{
	case 0:				// Normal player
		Vector(-16, -16, -36).CopyToArray(mins);
		Vector(16, 16, 36).CopyToArray(maxs);
		iret = 1;
		break;
	case 1:				// Crouched player
		Vector(-16, -16, -18).CopyToArray(mins);
		Vector(16, 16, 18).CopyToArray(maxs);
		iret = 1;
		break;
	case 2:				// Point based hull
		Vector(0, 0, 0).CopyToArray(mins);
		Vector(0, 0, 0).CopyToArray(maxs);
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int	DLLEXPORT HUD_ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	// Parse stuff from args
	// int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void DLLEXPORT HUD_PlayerMoveInit( struct playermove_s *ppmove )
{
	PM_Init( ppmove );
}

char DLLEXPORT HUD_PlayerMoveTexture( char *name )
{
	return PM_FindTextureType( name );
}

void DLLEXPORT HUD_PlayerMove( struct playermove_s *ppmove, int server )
{
	PM_Move( ppmove, server );
}

#ifdef _CS16CLIENT_ENABLE_GSRC_SUPPORT
/*
=================
HUD_GetRect

VGui stub
=================
*/
int *HUD_GetRect( void )
{
	static int extent[4];

	extent[0] = gEngfuncs.GetWindowCenterX() - ScreenWidth / 2;
	extent[1] = gEngfuncs.GetWindowCenterY() - ScreenHeight / 2;
	extent[2] = gEngfuncs.GetWindowCenterX() + ScreenWidth / 2;
	extent[3] = gEngfuncs.GetWindowCenterY() + ScreenHeight / 2;

	return extent;
}
#endif

/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

bool isLoaded = false;

int DLLEXPORT HUD_VidInit( void )
{
	CSRETRO_Renderer_VidInit();
	gHUD.VidInit();

	isLoaded = true;

	if (g_pParticleMan)
	{
		g_pParticleMan->ResetParticles();
		g_Environment.Reset();
		g_Environment.RestoreWeather();
	}

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all 
the hud variables.
==========================
*/

void DLLEXPORT HUD_Init( void )
{
	LoadMenuInterface();
	InitInput();
	gHUD.Init();
	CSRETRO_Renderer_Init();
	
	// Initialize menu if it's loaded
	if( g_pMenu && !g_pMenu->Initialize( Sys_GetFactoryThis() ) )
	{
		gEngfuncs.Con_Printf( "Warning: Menu initialization failed\n" );
	}
	//Scheme_Init();
}


/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int DLLEXPORT HUD_Redraw( float time, int intermission )
{
	gHUD.Redraw( time, intermission );

	return 1;
}


/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int DLLEXPORT HUD_UpdateClientData(client_data_t *pcldata, float flTime )
{
	IN_Commands();

	return gHUD.UpdateClientData(pcldata, flTime );
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void DLLEXPORT HUD_Reset( void )
{
	gHUD.Reset();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void DLLEXPORT HUD_Frame( double time )
{
#ifdef _CS16CLIENT_ENABLE_GSRC_SUPPORT
	gEngfuncs.VGui_ViewportPaintBackground(HUD_GetRect());
#endif

	// Handle menu input and mouse movement
	if( g_pMenu )
	{
		int x = 0, y = 0;
		if( g_pMenu->IsActive() )
		{
			gEngfuncs.GetMousePosition( &x, &y );
			g_pMenu->MouseMove( x, y );
		}
	}

	GetClientVoice()->Frame( time );
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	// gHUD.m_Radio.Voice( entindex, bTalking );
	if ( entindex > 0 && entindex <= gEngfuncs.GetMaxClients() )
	{
		if ( bTalking )
		{
			g_PlayerExtraInfo[entindex].radarflashtime = gHUD.m_flTime;
			g_PlayerExtraInfo[entindex].radarflashes = 99999;
		}
		else
		{
			g_PlayerExtraInfo[entindex].radarflashtime = -1.0f;
			g_PlayerExtraInfo[entindex].radarflashes = 0;
		}
	}

	GetClientVoice()->UpdateSpeakerStatus( entindex, bTalking );
}

/*
==========================
HUD_DirectorEvent

Called when a director event message was received
==========================
*/

void DLLEXPORT HUD_DirectorMessage( int iSize, void *pbuf )
{
	 gHUD.m_Spectator.DirectorMessage( iSize, pbuf );
}

/*
==========================
HUD_GetRenderInterface

PX2/PX3B/PX6A: own render_interface_t. Mode 0/1 return 0.
Mode 2 may return 1 behind the PX6A takeover lifecycle gate.
==========================
*/

static cvar_t *r_csretro_renderer = NULL;
static int s_glRenderFrameLogged = 0;

static int CSRETRO_GL_RenderFrame( const struct ref_viewpass_s *rvp )
{
	int rc = CSRETRO_Renderer_Frame( rvp );
	int mode = 0;

	if( r_csretro_renderer )
	{
		if( r_csretro_renderer->value >= 1.5f )
			mode = 2;
		else if( r_csretro_renderer->value >= 0.5f )
			mode = 1;
	}

	if( !s_glRenderFrameLogged )
	{
		s_glRenderFrameLogged = 1;
		gEngfuncs.Con_Printf( "CS Retro: GL_RenderFrame callback reached\n" );
		if( mode == 2 )
			gEngfuncs.Con_Printf( "CS Retro: r_csretro_renderer 2 takeover candidate (return=%i)\n", rc );
		else if( mode == 1 )
			gEngfuncs.Con_Printf( "CS Retro: r_csretro_renderer 1 offscreen probe, visible frame stays Xash\n" );
		else
			gEngfuncs.Con_Printf( "CS Retro: Xash fallback selected\n" );
	}

	return rc;
}

static void CSRETRO_GL_BuildLightmaps( void )
{
	// Additive: engine already rebuilt lightmaps (gl_rsurf.c). Visible Xash path unchanged.
	CSRETRO_Renderer_OnLightmaps();
}

static void CSRETRO_Mod_ProcessUserData( struct model_s *mod, qboolean create, const byte *buffer )
{
	CSRETRO_Renderer_OnModel( mod, create ? 1 : 0, buffer );
}

static void CSRETRO_R_NewMap( void )
{
	CSRETRO_Renderer_OnNewMap();
}

static void CSRETRO_R_ClearScene( void )
{
	CSRETRO_Renderer_ClearScene();
}

static render_interface_t gCSRetroRenderInterface = {
	CL_RENDER_INTERFACE_VERSION,
	CSRETRO_GL_RenderFrame,
	CSRETRO_GL_BuildLightmaps,
	NULL, // GL_OrthoBounds — overview only
	NULL, // R_CreateStudioDecalList
	NULL, // R_ClearStudioDecals
	NULL, // R_SpeedsMessage
	CSRETRO_Mod_ProcessUserData,
	NULL, // R_ProcessEntData — PX3C/PX4
	CSRETRO_Mod_GetCurrentVis,
	CSRETRO_R_NewMap,
	CSRETRO_R_ClearScene, // additiv: nur CS-Retro-Spiegelliste
	NULL  // CL_UpdateLatchedVars — Studio-Lerp, später
};

int DLLEXPORT HUD_GetRenderInterface( int version, render_api_t *renderfuncs, render_interface_t *callback )
{
	if( version != CL_RENDER_INTERFACE_VERSION || !renderfuncs || !callback )
		return false;

	gRenderAPI = *renderfuncs;
	*callback = gCSRetroRenderInterface;

	if( !r_csretro_renderer )
		r_csretro_renderer = CVAR_CREATE( "r_csretro_renderer", "0", 0 );

	// host_ver = "Q_buildnum() XASH_VERSION os arch commit". Dev-Waf ohne Datum → -1.
	if( g_iXash > 0 && g_iXash < MIN_XASH_VERSION )
	{
		gRenderAPI.Host_Error("Xash3D FWGS version check failed!\nPlease update your Xash3D FWGS!\n");
	}

	s_glRenderFrameLogged = 0;
	gEngfuncs.Con_Printf( "CS Retro: HUD_GetRenderInterface accepted v%i\n", CL_RENDER_INTERFACE_VERSION );
	gEngfuncs.Con_Printf( "CS Retro: CS-Retro render callbacks registered (GL_RenderFrame mode0/1=0, mode2=takeover gate, R_ClearScene additive, Mod_GetCurrentVis active)\n" );

	return true;
}

extern "C" void DLLEXPORT HUD_ChatInputPosition( int *x, int *y )
{
}

extern "C" int DLLEXPORT HUD_GetPlayerTeam(int iplayer)
{
	// original seems to return team_id, but I'm not sure it's even set somewhere
	if ( iplayer >= 1 && iplayer <= MAX_PLAYERS )
		return g_PlayerExtraInfo[iplayer].teamnumber;
	return 0;
}

void CL_UnloadParticleMan()
{
	if (g_pParticleMan)
	{
		delete g_pParticleMan;
		g_pParticleMan = NULL;
	}
}

void CL_LoadParticleMan()
{
	g_pParticleMan = new IParticleMan_Active();
	if (g_pParticleMan)
	{
		g_pParticleMan->SetUp(&gEngfuncs);
	}
}

#include "cl_dll/IGameClientExports.h"

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	const char *GetServerHostName() override
	{
		return gHUD.m_szServerName;
	}

	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted( int playerIndex ) override
	{
		if ( GetClientVoice() )
			return GetClientVoice()->IsPlayerBlocked( playerIndex );

		return false;
	}

	void MutePlayerGameVoice( int playerIndex ) override
	{
		if ( GetClientVoice() )
		{
			GetClientVoice()->SetPlayerBlockedState( playerIndex, true );
		}
	}

	void UnmutePlayerGameVoice( int playerIndex ) override
	{
		if ( GetClientVoice() )
		{
			GetClientVoice()->SetPlayerBlockedState( playerIndex, false );
		}
	}

	const char *GetLevelName( void ) override
	{
		const char *fullname = gEngfuncs.pfnGetLevelName();
		if( fullname[0] )
		{
			strncpy( mapname, fullname + 5, sizeof( mapname ));
			mapname[strlen(mapname) - 4] = '\0';
		}
		else mapname[0] = 0;

		return mapname;
	}

	int GetLocalPlayerTeam() override
	{
		return g_PlayerExtraInfo[gHUD.m_Scoreboard.m_iPlayerNum].teamnumber;
	}
private:
	char mapname[64];
};

EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION)
