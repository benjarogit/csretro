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
// menu.cpp — Desktop-In-Game-Menüs (GoldSrc ShowMenu / titles.txt)
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "com_weapons.h"

#include <string.h>
#include <stdio.h>
#include "draw_util.h"
#include "strl.h"

#define MAX_MENU_STRING	512

char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

int KB_ConvertString( char *in, char **ppout );

static int menu_r = 255, menu_g = 255, menu_b = 255, menu_x = 20, menu_ralign = 0;

static const char *ParseEscapeToken( const char *token )
{
	if( *token != '\\' )
		return token;

	token++;
	switch( *token )
	{
	case '\0':
		return token;
	case 'w':
		menu_r = 255; menu_g = 255; menu_b = 255;
		break;
	case 'd':
		menu_r = 100; menu_g = 100; menu_b = 100;
		break;
	case 'y':
		menu_r = 255; menu_g = 210; menu_b = 64;
		break;
	case 'r':
		menu_r = 210; menu_g = 24; menu_b = 0;
		break;
	case 'R':
		menu_x = ScreenWidth / 2;
		menu_ralign = TRUE;
		break;
	}
	return ++token;
}

static int PlayerTeamNumber( void )
{
	const int idx = gHUD.m_Scoreboard.m_iPlayerNum;
	if( idx >= 1 && idx <= MAX_PLAYERS )
		return g_PlayerExtraInfo[idx].teamnumber;
	return g_iTeamNumber;
}

static const char *TeamMenuTitle( int bits )
{
	const bool spec = ( bits & MENU_KEY_6 ) != 0;
	const bool exitKey = ( bits & MENU_KEY_0 ) != 0;
	const bool vip = ( bits & MENU_KEY_3 ) != 0;
	if( vip && spec )
		return "#IG_VIP_Team_Select_Spect";
	if( vip )
		return "#IG_VIP_Team_Select";
	if( spec && exitKey )
		return "#IG_Team_Select_Spect";
	if( spec )
		return "#Team_Select_Spect";
	if( exitKey )
		return "#IG_Team_Select";
	return "#Team_Select";
}

static const char *VguiMenuTitle( int menuType, int bits )
{
	const int team = PlayerTeamNumber();
	const bool asMap = IsASMapType();

	switch( menuType )
	{
	case MENU_TEAM:
		return TeamMenuTitle( bits );
	case MENU_CLASS_T:
		return "#Terrorist_Select";
	case MENU_CLASS_CT:
		return "#CT_Select";
	case MENU_BUY:
		return "#Buy";
	case MENU_BUY_PISTOL:
		return ( team == TEAM_TERRORIST ) ? "#T_BuyPistol" : "#CT_BuyPistol";
	case MENU_BUY_SHOTGUN:
		return asMap ? "#AS_BuyShotgun" : "#BuyShotgun";
	case MENU_BUY_RIFLE:
		if( asMap )
			return ( team == TEAM_CT ) ? "#AS_CT_BuyRifle" : "#AS_T_BuyRifle";
		return ( team == TEAM_TERRORIST ) ? "#T_BuyRifle" : "#CT_BuyRifle";
	case MENU_BUY_SUBMACHINEGUN:
		if( asMap )
			return ( team == TEAM_CT ) ? "#AS_CT_BuySubMachineGun" : "#AS_T_BuySubMachineGun";
		return ( team == TEAM_TERRORIST ) ? "#T_BuySubMachineGun" : "#CT_BuySubMachineGun";
	case MENU_BUY_MACHINEGUN:
		return asMap ? "#AS_T_BuyMachineGun" : "#BuyMachineGun";
	case MENU_BUY_ITEM:
		return ( team == TEAM_CT ) ? "#CT_BuyItem" : "#T_BuyItem";
	case MENU_RADIOA:
		return "#RadioA";
	case MENU_RADIOB:
		return "#RadioB";
	case MENU_RADIOC:
		return "#RadioC";
	default:
		return NULL;
	}
}

int CHudMenu::Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( gHUD.m_Menu, ShowMenu );
	HOOK_MESSAGE( gHUD.m_Menu, VGUIMenu );
	HOOK_MESSAGE( gHUD.m_Menu, BuyClose );
	HOOK_MESSAGE( gHUD.m_Menu, AllowSpec );
	HOOK_COMMAND( gHUD.m_Menu, "client_buy_open", OldStyleMenuOpen );
	HOOK_COMMAND( gHUD.m_Menu, "client_buy_close", OldStyleMenuClose );
	HOOK_COMMAND( gHUD.m_Menu, "showvguimenu", ShowVGUIMenu );

	InitHUDData();
	m_bAllowSpec = true;
	return 1;
}

void CHudMenu::InitHUDData( void )
{
	m_fMenuDisplayed = 0;
	m_bitsValidSlots = 0;
	Reset();
}

void CHudMenu::Reset( void )
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = FALSE;
}

int CHudMenu::VidInit( void )
{
	return 1;
}

void CHudMenu::Close( void )
{
	m_fMenuDisplayed = 0;
	m_iFlags &= ~HUD_DRAW;
	m_flShutoffTime = -1;
}

int CHudMenu::Draw( float flTime )
{
	if( m_flShutoffTime > 0 && m_flShutoffTime <= gHUD.m_flTime )
	{
		Close();
		return 1;
	}

	int nlc = 0;
	for( int i = 0; i < MAX_MENU_STRING && g_szMenuString[i] != '\0'; i++ )
	{
		if( g_szMenuString[i] == '\n' )
			nlc++;
	}

	int y = ( ScreenHeight / 2 ) - ( ( nlc / 2 ) * 12 ) - 40;
	const char *sptr = g_szMenuString;

	menu_r = 255;
	menu_g = 255;
	menu_b = 255;

	while( *sptr )
	{
		if( *sptr == '\n' )
		{
			sptr++;
			y += 12;
			menu_x = 20;
			menu_ralign = FALSE;
			menu_r = 255;
			menu_g = 255;
			menu_b = 255;
			continue;
		}

		if( *sptr == '\\' )
		{
			sptr = ParseEscapeToken( sptr );
			continue;
		}

		char menubuf[80];
		const char *ptr = sptr;
		while( *sptr && *sptr != '\n' && *sptr != '\\' )
			sptr++;

		const int n = (int)( sptr - ptr );
		const int copy = n < (int)sizeof( menubuf ) - 1 ? n : (int)sizeof( menubuf ) - 1;
		memcpy( menubuf, ptr, copy );
		menubuf[copy] = '\0';

		if( menu_ralign )
			menu_x = DrawUtils::DrawHudStringReverse( menu_x, y, 0, menubuf, menu_r, menu_g, menu_b );
		else
			menu_x = DrawUtils::DrawHudString( menu_x, y, ScreenWidth / 2, menubuf, menu_r, menu_g, menu_b );
	}

	return 1;
}

void CHudMenu::SelectMenuItem( int menu_item )
{
	if( ( menu_item > 0 ) && ( m_bitsValidSlots & ( 1 << ( menu_item - 1 ) ) ) )
	{
		char szbuf[32];
		sprintf( szbuf, "menuselect %d\n", menu_item );
		ClientCmd( szbuf );
		Close();
	}
}

bool CHudMenu::HandleEscape( void )
{
	if( !m_fMenuDisplayed )
		return false;

	if( m_bitsValidSlots & MENU_KEY_0 )
		SelectMenuItem( 10 );
	else
		Close();
	return true;
}

void CHudMenu::OpenLocalized( const char *titleKey, int bitsValidSlots, int displayTime )
{
	char *temp = NULL;

	m_bitsValidSlots = bitsValidSlots;
	if( displayTime > 0 )
		m_flShutoffTime = displayTime + gHUD.m_flTime;
	else
		m_flShutoffTime = -1;

	if( !m_bitsValidSlots )
	{
		Close();
		return;
	}

	strlcpy( g_szPrelocalisedMenuString, titleKey, sizeof( g_szPrelocalisedMenuString ) );
	strlcpy( g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString( titleKey ), sizeof( g_szMenuString ) );
	if( KB_ConvertString( g_szMenuString, &temp ) )
	{
		strlcpy( g_szMenuString, temp, sizeof( g_szMenuString ) );
		free( temp );
	}

	m_fMenuDisplayed = 1;
	m_iFlags |= HUD_DRAW;
	m_fWaitingForMore = FALSE;
	gEngfuncs.Con_Printf( "CSRetro-Menu: %s\n", titleKey );
}

int CHudMenu::MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf )
{
	char *temp = NULL, *menustring;
	BufferReader reader( pszName, pbuf, iSize );

	m_bitsValidSlots = reader.ReadShort();
	const int DisplayTime = reader.ReadChar();
	const int NeedMore = reader.ReadByte();

	if( DisplayTime > 0 )
		m_flShutoffTime = DisplayTime + gHUD.m_flTime;
	else
		m_flShutoffTime = -1;

	if( !m_bitsValidSlots )
	{
		Close();
		return 1;
	}

	menustring = reader.ReadString();

	if( !m_fWaitingForMore )
		strlcpy( g_szPrelocalisedMenuString, menustring, sizeof( g_szPrelocalisedMenuString ) );
	else
		strlcat( g_szPrelocalisedMenuString, menustring, sizeof( g_szPrelocalisedMenuString ) );

	if( !NeedMore )
	{
		strlcpy( g_szMenuString, gHUD.m_TextMessage.BufferedLocaliseTextString( g_szPrelocalisedMenuString ), sizeof( g_szMenuString ) );
		if( KB_ConvertString( g_szMenuString, &temp ) )
		{
			strlcpy( g_szMenuString, temp, sizeof( g_szMenuString ) );
			free( temp );
		}
		gEngfuncs.Con_Printf( "CSRetro-Menu: %s\n", g_szPrelocalisedMenuString );
	}

	m_fMenuDisplayed = 1;
	m_iFlags |= HUD_DRAW;
	m_fWaitingForMore = NeedMore;
	return 1;
}

int CHudMenu::MsgFunc_VGUIMenu( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );
	const int menuType = reader.ReadByte();
	m_bitsValidSlots = reader.ReadShort();

	const char *title = VguiMenuTitle( menuType, m_bitsValidSlots );
	if( !title )
	{
		gEngfuncs.Con_Printf( "CSRetro-Menu: unbekannter VGUIMenu %d\n", menuType );
		return 1;
	}

	OpenLocalized( title, m_bitsValidSlots, -1 );
	return 1;
}

int CHudMenu::MsgFunc_BuyClose( const char *pszName, int iSize, void *pbuf )
{
	Close();
	if( g_pMenu )
		g_pMenu->HideVGUIMenu();
	return 1;
}

int CHudMenu::MsgFunc_AllowSpec( const char *pszName, int iSize, void *pbuf )
{
	BufferReader reader( pszName, pbuf, iSize );
	m_bAllowSpec = (bool)reader.ReadByte();
	return 1;
}

void CHudMenu::UserCmd_OldStyleMenuOpen()
{
	OpenLocalized( "#Buy",
		MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_3 | MENU_KEY_4 | MENU_KEY_5 |
		MENU_KEY_6 | MENU_KEY_7 | MENU_KEY_8 | MENU_KEY_0, -1 );
}

void CHudMenu::UserCmd_OldStyleMenuClose()
{
	Close();
}

void CHudMenu::ShowVGUIMenu( int menuType )
{
	const char *title = VguiMenuTitle( menuType, m_bitsValidSlots ? m_bitsValidSlots : 0x3FF );
	if( !title )
		return;
	OpenLocalized( title, m_bitsValidSlots ? m_bitsValidSlots : 0x3FF, -1 );
}

void CHudMenu::UserCmd_ShowVGUIMenu()
{
	if( gEngfuncs.Cmd_Argc() < 2 )
	{
		ConsolePrint( "usage: showvguimenu <menuType>\n" );
		return;
	}
	ShowVGUIMenu( atoi( gEngfuncs.Cmd_Argv( 1 ) ) );
}
