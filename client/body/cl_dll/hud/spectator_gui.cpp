/*
spectator_gui.cpp - HUD Overlays
Copyright (C) 2015 a1batross

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

In addition, as a special exception, the author gives permission to
link the code of this program with the Half-Life Game Engine ("HL
Engine") and Modified Game Libraries ("MODs") developed by Valve,
L.L.C ("Valve").  You must obey the GNU General Public License in all
respects for all of the code used other than the HL Engine and MODs
from Valve.  If you modify this file, you may extend this exception
to your version of the file, but you are not obligated to do so.  If
you do not wish to do so, delete this exception statement from your
version.
*/

#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include "vgui_parser.h"
#include "triangleapi.h"
#include "draw_util.h"

/*
 * We will draw all elements inside a box. It's size 16x10.
 */

#define XPOS( x ) ( (x) / 16.0f )
#define YPOS( y ) ( (y) / 10.0f  )

#define INT_XPOS(x) int(XPOS(x) * ScreenWidth)
#define INT_YPOS(y) int(YPOS(y) * ScreenHeight)

int CHudSpectatorGui::Init()
{
	if( !g_iXash )
		return 1;

	HOOK_MESSAGE( gHUD.m_SpectatorGui, SpecHealth );
	HOOK_MESSAGE( gHUD.m_SpectatorGui, SpecHealth2 );

	HOOK_COMMAND_FUNC( "_spec_find_next_player_reverse", gHUD.m_Spectator.FindNextPlayer, true );
	HOOK_COMMAND_FUNC( "_spec_find_next_player", gHUD.m_Spectator.FindNextPlayer, false );

	gHUD.AddHudElem(this);
	m_iFlags = HUD_DRAW;
	m_hTimerTexture = 0;
	return 1;
}

int CHudSpectatorGui::VidInit()
{
	if( !g_iXash )
	{
		ConsolePrint("Warning: CHudSpectatorGui is disabled! Dude, are you running me on old GoldSrc?\n");
		m_iFlags = 0;
		return 0;
	}

	m_hTimerTexture = gRenderAPI.GL_LoadTexture("gfx/vgui/timer.tga", NULL, 0, TF_NEAREST |TF_NOMIPMAP|TF_CLAMP );
	return 1;
}

void CHudSpectatorGui::Shutdown()
{
	gRenderAPI.GL_FreeTexture( m_hTimerTexture );
}

int CHudSpectatorGui::Draw( float flTime )
{
	if( !g_iUser1 )
	{
		if( g_pMenu )
		{
			SpectatorHudState s;
			memset( &s, 0, sizeof( s ) );
			g_pMenu->SetSpectatorHud( &s );
		}
		return 1;
	}

	CalcAllNeededData( );

	if( g_pMenu )
	{
		SpectatorHudState s;
		memset( &s, 0, sizeof( s ) );
		s.observerMode = g_iUser1;
		s.targetIndex = g_iUser2;
		s.tScore = label.m_iTerrorists;
		s.ctScore = label.m_iCounterTerrorists;
		if( g_iUser2 > 0 && g_iUser2 < MAX_PLAYERS )
		{
			s.playerTeam = g_PlayerExtraInfo[g_iUser2].teamnumber;
			hud_player_info_t info;
			GetPlayerInfo( g_iUser2, &info );
			if( info.name )
				strncpy( s.player, info.name, sizeof( s.player ) - 1 );
			s.health = g_PlayerExtraInfo[g_iUser2].sb_health > 255
				? g_PlayerExtraInfo[g_iUser2].sb_health
				: g_PlayerExtraInfo[g_iUser2].health;
		}
		strncpy( s.timer, label.m_szTimer, sizeof( s.timer ) - 1 );
		const char *map = label.m_szMap;
		if( !strncmp( map, "Map: ", 5 ) )
			map += 5;
		strncpy( s.map, map, sizeof( s.map ) - 1 );
		gHUD.m_Scoreboard.GetAllPlayersInfo();
		for( int i = 1; i < MAX_PLAYERS && s.playerCount < CSRETRO_SCOREBOARD_PLAYERS; ++i )
		{
			if( !g_PlayerInfoList[i].name || !g_PlayerInfoList[i].name[0] )
				continue;
			ScoreboardPlayerRow &row = s.players[s.playerCount++];
			strncpy( row.name, g_PlayerInfoList[i].name, sizeof( row.name ) - 1 );
			row.frags = g_PlayerExtraInfo[i].frags;
			row.deaths = g_PlayerExtraInfo[i].deaths;
			row.ping = g_PlayerInfoList[i].ping;
			row.thisPlayer = g_PlayerInfoList[i].thisplayer ? 1 : 0;
			row.dead = g_PlayerExtraInfo[i].dead ? 1 : 0;
			row.team = g_PlayerExtraInfo[i].teamnumber;
			const char *bot = gEngfuncs.PlayerInfo_ValueForKey( i, "*bot" );
			row.bot = ( bot && atoi( bot ) > 0 ) ? 1 : 0;
		}
		g_pMenu->SetSpectatorHud( &s );
		if( gHUD.m_Spectator.m_pip )
			gHUD.m_Spectator.m_pip->value = INSET_OFF;
		return 1;
	}

	int r = 255, g = 140, b = 0;

	// at first, draw these silly black bars
	int startpos = 0;
	if( gHUD.m_Spectator.m_pip->value != INSET_OFF ) // pip adjust
	{
		startpos = XRES(gHUD.m_Spectator.m_OverviewData.insetWindowWidth) + XRES(gHUD.m_Spectator.m_OverviewData.insetWindowX);
		startpos *= ScreenWidth / TrueWidth; // hud_scale adjust
	}
	FillRGBABlend(startpos, 0, ScreenWidth - startpos, INT_YPOS(2), 0, 0, 0, 153);
	FillRGBABlend(0, ScreenHeight - INT_YPOS(2), ScreenWidth, INT_YPOS(2), 0, 0, 0, 153);

	if ( gHUD.m_Spectator.m_drawstatus && gHUD.m_Spectator.m_drawstatus->value )
	{
		// divider
		{
			int divX = INT_XPOS(12.5);
			int divTop = INT_YPOS(2) * 0.25;
			int divBottom = INT_YPOS(2) * 0.5 + gHUD.GetCharHeight();
			int divH = divBottom - divTop;
			if (divH < gHUD.GetCharHeight()) divH = gHUD.GetCharHeight();

			int pad = (gHUD.GetCharHeight() * 2) / 3;
			if (pad < 1) pad = 1;

			int drawTop = divTop - pad;
			if (drawTop < 0) drawTop = 0;
			int drawH = divH + pad * 2;
			if (drawTop + drawH > ScreenHeight) drawH = ScreenHeight - drawTop;

			FillRGBABlend(divX, drawTop, 1, drawH, r, g, b, 255);
		}

		{ // mapname. extradata
			DrawUtils::DrawHudString( INT_XPOS(12.5) + 10, INT_YPOS(2) * 0.25, ScreenWidth, label.m_szMap, r, g, b );

			if( !m_bBombPlanted ) // timer remaining
			{
				if( m_hTimerTexture )
				{
					gRenderAPI.GL_SelectTexture( 0 );
					gRenderAPI.GL_Bind(0, m_hTimerTexture);
					gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
					gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );

					float quadX = INT_XPOS(12.5) + 10;
					float quadY = INT_YPOS(2) * 0.5f;
					int uploadW = (int)gRenderAPI.RenderGetParm( PARM_TEX_WIDTH, m_hTimerTexture );
					int uploadH = (int)gRenderAPI.RenderGetParm( PARM_TEX_HEIGHT, m_hTimerTexture );

					// gEngfuncs.pTriAPI->Begin( TRI_QUADS );
					DrawUtils::Draw2DQuad( quadX * gHUD.m_flScale,
										quadY * gHUD.m_flScale,
										(quadX + (float)uploadW) * gHUD.m_flScale,
										(quadY + (float)uploadH) * gHUD.m_flScale );
					// gEngfuncs.pTriAPI->End();
				}
				DrawUtils::DrawHudString( INT_XPOS(12.5) + gHUD.GetCharHeight() * 1.5 + gHUD.GetCharWidth('M') , INT_YPOS(2) * 0.5, ScreenWidth,
										label.m_szTimer, r, g, b );
			}
		}


		{ // draw team here
			int iLen = DrawUtils::HudStringLen("Counter-Terrorists:" );

			DrawUtils::DrawHudString( INT_XPOS(12.5) - iLen - 50 , INT_YPOS(2) * 0.25, INT_XPOS(12.5) - 50, "Counter-Terrorists:", r, g, b );
			DrawUtils::DrawHudString( INT_XPOS(12.5) - iLen - 50, INT_YPOS(2) * 0.5, INT_XPOS(12.5) - 50, "Terrorists:", r, g, b );
			// count
			DrawUtils::DrawHudNumberString( INT_XPOS(12.5) - 10, INT_YPOS(2) * 0.25, INT_XPOS(12.5) - 50, label.m_iCounterTerrorists, r, g, b );
			DrawUtils::DrawHudNumberString( INT_XPOS(12.5) - 10, INT_YPOS(2) * 0.5,  INT_XPOS(12.5) - 50, label.m_iTerrorists,        r, g, b );
		}
	}

	//if( !label.m_szNameAndHealth[0] )
	//{
		int iLen = DrawUtils::HudStringLen( label.m_szNameAndHealth );
		GetTeamColor( r, g, b, g_PlayerExtraInfo[ g_iUser2 ].teamnumber );
		DrawUtils::DrawHudString( ScreenWidth * 0.5 - iLen * 0.5, INT_YPOS(9) - gHUD.GetCharHeight() * 0.5 , ScreenWidth,
								  label.m_szNameAndHealth, r, g, b );
	//}

	return 1;
}

void CHudSpectatorGui::CalcAllNeededData( )
{
	// mapname
	if( !label.m_szMap[0] )
	{
		static char szMapNameStripped[55];
		const char *szMapName = gEngfuncs.pfnGetLevelName(); //  "maps/%s.bsp"
		strncpy( szMapNameStripped, szMapName + 5, sizeof( szMapNameStripped ) );
		szMapNameStripped[strlen(szMapNameStripped) - 4] = '\0';
		snprintf( label.m_szMap, sizeof( label.m_szMap ), "Map: %s", szMapNameStripped );
	}

	// team
	/*label.m_iTerrorists        = 0;
	label.m_iCounterTerrorists = 0;
	for( int i = 0; i < MAX_PLAYERS; i++ )
	{
		if( g_PlayerExtraInfo[i].dead )
			continue; // show remaining

		switch( g_PlayerExtraInfo[i].teamnumber )
		{
		case TEAM_CT:
			label.m_iCounterTerrorists++;
		case TEAM_TERRORIST:
			label.m_iTerrorists++;
		}
	}*/

	label.m_iCounterTerrorists = 0;
	label.m_iTerrorists = 0;
	for( int i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; i++ )
	{
		switch( g_TeamInfo[i].teamnumber )
		{
		case TEAM_CT:
			label.m_iCounterTerrorists = g_TeamInfo[i].frags;
			break;
		case TEAM_TERRORIST:
			label.m_iTerrorists = g_TeamInfo[i].frags;
			break;
		}
	}

	// timer
	// time must be positive
	if( !m_bBombPlanted )
	{
		int iMinutes = max( 0, (int)( gHUD.m_Timer.m_iTime + gHUD.m_Timer.m_fStartTime - gHUD.m_flTime ) / 60);
		int iSeconds = max( 0, (int)( gHUD.m_Timer.m_iTime + gHUD.m_Timer.m_fStartTime - gHUD.m_flTime ) - (iMinutes * 60));

		sprintf( label.m_szTimer, "%i:%02i", iMinutes, iSeconds );
	}

	// player name
	if( g_iUser2 > 0 && g_iUser2 < MAX_PLAYERS )
	{
		hud_player_info_t sInfo;
		GetPlayerInfo( g_iUser2, &sInfo );

		int iHealth = g_PlayerExtraInfo[g_iUser2].sb_health > 255 ? g_PlayerExtraInfo[g_iUser2].sb_health : g_PlayerExtraInfo[g_iUser2].health;

		snprintf( label.m_szNameAndHealth, sizeof( label.m_szNameAndHealth ),
				  "%s (%i)",  sInfo.name, iHealth );
	}
	else label.m_szNameAndHealth[0] = '\0';
}

void CHudSpectatorGui::InitHUDData()
{
	m_bBombPlanted = false;
	label.m_szMap[0] = '\0';
}

void CHudSpectatorGui::Reset()
{
	m_bBombPlanted = false;
}

int CHudSpectatorGui::MsgFunc_SpecHealth(const char *pszName, int iSize, void *buf)
{
	BufferReader reader( pszName, buf, iSize );

	int health = reader.ReadByte();

	g_PlayerExtraInfo[g_iUser2].health = health;
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;

	return 1;
}

int CHudSpectatorGui::MsgFunc_SpecHealth2(const char *pszName, int iSize, void *buf)
{
	BufferReader reader( pszName, buf, iSize );

	int health = reader.ReadByte();
	int client = reader.ReadByte();

	g_PlayerExtraInfo[client].health = health;
	gHUD.m_Health.m_iPlayerLastPointedAt = g_iUser2;

	return 1;
}
