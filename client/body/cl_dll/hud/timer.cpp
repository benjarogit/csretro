/*
timer.cpp -- HUD timer, progress bars, etc
Copyright (C) 2015-2016 a1batross
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

#include "stdio.h"
#include "stdlib.h"
#include "math.h"

#include "hud.h"
#include "cl_util.h"
#include "keydefs.h"
#include "cdll_dll.h"
#include "parsemsg.h"
#include "vgui_parser.h"
#include <string.h>
#include "draw_util.h"
#include "pm_shared.h"
#include "com_weapons.h"

int CHudTimer::Init()
{
	HOOK_MESSAGE( gHUD.m_Timer, RoundTime );
	HOOK_MESSAGE( gHUD.m_Timer, ShowTimer );
	HOOK_MESSAGE( gHUD.m_Timer, Warmup );
	m_iFlags = 0;
	m_bPanicColorChange = false;
	InitHUDData();
	gHUD.AddHudElem(this);
	return 1;
}

void CHudTimer::InitHUDData()
{
	m_bWarmup = false;
	m_iWarmupReady = 0;
	m_iWarmupNeed = 0;
	m_bAnnouncerCountdown = true;
	m_bAnnouncerMinute = true;
	m_bAnnouncerWasFreeze = false;
	m_bAnnouncerPreparePlayed = false;
	m_bAnnouncerBeginPlayed = false;
	m_iAnnouncerLastSpoken = -1;
	m_iAnnouncerPrevRemain = -1;
	m_bAnnouncerMinutePlayed = false;
}

int CHudTimer::VidInit()
{
	m_HUD_timer = gHUD.GetSpriteIndex( "stopwatch" );
	return 1;
}

bool CHudTimer::HandleReadyKey(int keynum)
{
	if (!m_bWarmup || keynum != K_F12)
		return false;
	ClientCmd("ready");
	return true;
}

int CHudTimer::GetTimeRemaining() const
{
	return max( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) );
}

void CHudTimer::TickAnnouncer(int remain, bool freeze)
{
	if (m_bAnnouncerCountdown)
	{
		if (freeze && !m_bAnnouncerWasFreeze)
		{
			m_iAnnouncerLastSpoken = -1;
			m_bAnnouncerMinutePlayed = false;
			m_bAnnouncerPreparePlayed = false;
			m_bAnnouncerBeginPlayed = false;
		}

		// At 10s remaining so the ~1.6s clip finishes before 5-4-3-2-1.
		if (freeze && !m_bAnnouncerPreparePlayed && remain <= 10 && remain >= 8)
		{
			PlaySound("announcer/prepareforbattle.wav", 1);
			m_bAnnouncerPreparePlayed = true;
		}

		if (freeze && remain >= 1 && remain <= 5 && remain != m_iAnnouncerLastSpoken)
		{
			char path[64];
			snprintf(path, sizeof(path), "announcer/%d.wav", remain);
			PlaySound(path, 1);
			m_iAnnouncerLastSpoken = remain;
		}

		if (!freeze && m_bAnnouncerWasFreeze && !m_bAnnouncerBeginPlayed)
		{
			PlaySound("announcer/begin.wav", 1);
			m_bAnnouncerBeginPlayed = true;
		}
	}

	if (m_bAnnouncerMinute && !freeze && !m_bWarmup
		&& m_iAnnouncerPrevRemain > 60 && remain <= 60 && remain >= 55
		&& !m_bAnnouncerMinutePlayed)
	{
		PlaySound("announcer/1minuteremains.wav", 1);
		m_bAnnouncerMinutePlayed = true;
	}

	m_bAnnouncerWasFreeze = freeze;
	m_iAnnouncerPrevRemain = remain;
}

int CHudTimer::Draw( float fTime )
{
	if (gHUD.m_iHideHUDDisplay & HIDEHUD_ALL)
		return 1;

	const int remain = GetTimeRemaining();
	const bool inRound = (g_iTeamNumber == TEAM_TERRORIST || g_iTeamNumber == TEAM_CT)
		&& (gHUD.m_iWeaponBits & (1 << WEAPON_SUIT)) && !g_iUser1;
	const bool freeze = inRound && !g_iFreezeTimeOver && remain > 0 && remain <= 20 && !m_bWarmup;
	if (inRound)
		TickAnnouncer(remain, freeze);

	if( gHUD.m_iHideHUDDisplay & HIDEHUD_TIMER )
		return 1;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return 1;

	if( g_iUser1 == OBS_IN_EYE )
		return 1;

	int r, g, b;
	// time must be positive
	int minutes = max( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) / 60);
	int seconds = max( 0, (int)( m_iTime + m_fStartTime - gHUD.m_flTime ) - (minutes * 60));

	if( minutes * 60 + seconds > 20 )
	{
		DrawUtils::UnpackRGB( r, g, b, gHUD.m_iDefaultHUDColor );
	}
	else
	{
		m_flPanicTime += gHUD.m_flTimeDelta;
		// add 0.1 sec, so it's not flicker fast
		if( m_flPanicTime > ((float)seconds / 40.0f) + 0.1f)
		{
			m_flPanicTime = 0;
			m_bPanicColorChange = !m_bPanicColorChange;
		}
		DrawUtils::UnpackRGB( r, g, b, m_bPanicColorChange ? gHUD.m_iDefaultHUDColor : RGB_REDISH );
	}

	DrawUtils::ScaleColors( r, g, b, MIN_ALPHA );

	if (m_bWarmup)
	{
		const char *title = "WARMUP";
		const char *hint = "F12  READY";
		const float titleScale = 1.35f;
		const float hintScale = 0.85f;
		int tr, tg, tb;
		int hr, hg, hb;
		DrawUtils::UnpackRGB(tr, tg, tb, RGB_YELLOWISH);
		DrawUtils::UnpackRGB(hr, hg, hb, RGB_WHITE);
		DrawUtils::ScaleColors(hr, hg, hb, 170);
		const int titleW = static_cast<int>(DrawUtils::HudStringLen(title) * titleScale);
		const int hintW = static_cast<int>(DrawUtils::HudStringLen(hint) * hintScale);
		const int titleY = gHUD.m_iFontHeight;
		const int hintY = titleY + static_cast<int>(gHUD.m_iFontHeight * titleScale) + 2;
		DrawUtils::DrawHudString((ScreenWidth - titleW) / 2, titleY, ScreenWidth,
			title, tr, tg, tb, titleScale);
		DrawUtils::DrawHudString((ScreenWidth - hintW) / 2, hintY, ScreenWidth,
			hint, hr, hg, hb, hintScale);
	}
	else if (freeze && m_bAnnouncerCountdown && remain <= 10 && remain > 5)
	{
		const char *prepare = "PREPARE FOR BATTLE";
		const int textW = DrawUtils::HudStringLen(prepare);
		DrawUtils::DrawHudString((ScreenWidth - textW) / 2, gHUD.m_iFontHeight, ScreenWidth,
			prepare, r, g, b);
	}

	int iWatchWidth = gHUD.GetSpriteRect(m_HUD_timer).Width();
	int iDigitWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).Width();
	int iColonWidth = iDigitWidth / 2;

	// Always reserve space for 2 digits for minutes to keep layout consistent
	int totalWidth = iWatchWidth + 2 * iDigitWidth + iColonWidth + 2 * iDigitWidth;

	int x = (ScreenWidth - totalWidth) / 2;
	int y = ScreenHeight - 1.5 * gHUD.m_iFontHeight;

	SPR_Set(gHUD.GetSprite(m_HUD_timer), r, g, b);
	SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_timer));
	x += iWatchWidth;

	if (minutes < 10)
		// Shift x to the right by one digit width to reserve space for the leading zero, then draw 1 digit without the leading zero
		x = DrawUtils::DrawHudNumber2(x + iDigitWidth, y, false, 1, minutes, r, g, b);
	else
		// Draw 2 digits, including the leading zero if needed
		x = DrawUtils::DrawHudNumber2(x, y, true, 2, minutes, r, g, b);

	// Draw colon (":")
	FillRGBA(x + iColonWidth / 2, y + gHUD.m_iFontHeight / 4, 2, 2, r, g, b, 100);
	FillRGBA(x + iColonWidth / 2, y + gHUD.m_iFontHeight - gHUD.m_iFontHeight / 4, 2, 2, r, g, b, 100);
	x += iColonWidth;

	m_right = DrawUtils::DrawHudNumber2(x, y, true, 2, seconds, r, g, b);

	return 1;
}

int CHudTimer::MsgFunc_RoundTime(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );
	m_iTime = reader.ReadShort();
	m_fStartTime = gHUD.m_flTime;
	m_iFlags = HUD_DRAW;
	return 1;
}

int CHudTimer::MsgFunc_ShowTimer(const char *pszName, int iSize, void *pbuf)
{
	m_iFlags = HUD_DRAW;
	return 1;
}

int CHudTimer::MsgFunc_Warmup(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader(pszName, pbuf, iSize);
	m_bWarmup = reader.ReadByte() != 0;
	m_iWarmupReady = reader.ReadByte();
	m_iWarmupNeed = reader.ReadByte();
	if (iSize >= 5)
	{
		m_bAnnouncerCountdown = reader.ReadByte() != 0;
		m_bAnnouncerMinute = reader.ReadByte() != 0;
	}
	m_iFlags = HUD_DRAW;
	return 1;
}

#define UPDATE_BOTPROGRESS 0
#define CREATE_BOTPROGRESS 1
#define REMOVE_BOTPROGRESS 2

int CHudProgressBar::Init()
{
	HOOK_MESSAGE( gHUD.m_ProgressBar, BarTime );
	HOOK_MESSAGE( gHUD.m_ProgressBar, BarTime2 );
	HOOK_MESSAGE( gHUD.m_ProgressBar, BotProgress );
	Reset( );
	gHUD.AddHudElem(this);
	return 1;
}

int CHudProgressBar::VidInit()
{
	return 1;
}

void CHudProgressBar::Reset( void )
{
	m_iFlags = 0;
	m_szLocalizedHeader = NULL;
	m_szHeader[0] = '\0';
	m_fStartTime = m_fPercent = 0.0f;
}

int CHudProgressBar::Draw( float flTime )
{
	// allow only 0.0..1.0
	if( (m_fPercent < 0.0f) || (m_fPercent > 1.0f) )
	{
		m_iFlags = 0;
		m_fPercent = 0.0f;
		return 1;
	}

	if( m_szLocalizedHeader && m_szLocalizedHeader[0] )
	{
		int r, g, b;
		DrawUtils::UnpackRGB( r, g, b, gHUD.m_iDefaultHUDColor );
		DrawUtils::DrawHudString( ScreenWidth / 4, ScreenHeight / 2, ScreenWidth, (char*)m_szLocalizedHeader, r, g, b );

		DrawUtils::DrawRectangle( ScreenWidth/ 4, ScreenHeight / 2 + gHUD.GetCharHeight(), ScreenWidth/2, ScreenHeight/30 );
		FillRGBA( ScreenWidth/4+2, ScreenHeight/2 + gHUD.GetCharHeight() + 2, m_fPercent * (ScreenWidth/2-4), ScreenHeight/30-4, 255, 140, 0, 255 );
		return 1;
	}

	// prevent SIGFPE
	if( m_iDuration != 0.0f )
	{
		m_fPercent = ((flTime - m_fStartTime) / m_iDuration);
	}
	else
	{
		m_fPercent = 0.0f;
		m_iFlags = 0;
		return 1;
	}

	DrawUtils::DrawRectangle( ScreenWidth/4, ScreenHeight*2/3, ScreenWidth/2, 10 );
	FillRGBA( ScreenWidth/4+2, ScreenHeight*2/3+2, m_fPercent * (ScreenWidth/2-4), 6, 255, 140, 0, 255 );

	return 1;
}

int CHudProgressBar::MsgFunc_BarTime(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );

	m_iDuration = reader.ReadShort();
	m_fPercent = 0.0f;

	m_fStartTime = gHUD.m_flTime;

	m_iFlags = HUD_DRAW;
	return 1;
}

int CHudProgressBar::MsgFunc_BarTime2(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );

	m_iDuration = reader.ReadShort();
	m_fPercent = m_iDuration * (float)reader.ReadShort() / 100.0f;

	m_fStartTime = gHUD.m_flTime;

	m_iFlags = HUD_DRAW;
	return 1;
}

int CHudProgressBar::MsgFunc_BotProgress(const char *pszName, int iSize, void *pbuf)
{
	BufferReader reader( pszName, pbuf, iSize );
	m_iDuration = 0.0f; // don't update our progress bar
	m_iFlags = HUD_DRAW;

	float fNewPercent;
	int flag = reader.ReadByte();
	switch( flag )
	{
	case CREATE_BOTPROGRESS:
		m_fPercent = 0.0f;
		break;
	case UPDATE_BOTPROGRESS:
		fNewPercent = (float)reader.ReadByte() / 100.0f;
		// cs behavior:
		// just don't decrease percent values
		if( m_fPercent < fNewPercent )
		{
			m_fPercent = fNewPercent;
		}
		strncpy(m_szHeader, reader.ReadString(), sizeof(m_szHeader));
		m_szHeader[sizeof(m_szHeader)-1] = 0;

		if( m_szHeader[0] == '#' )
			m_szLocalizedHeader = Localize(m_szHeader + 1);
		else
			m_szLocalizedHeader = m_szHeader;
		break;
	case REMOVE_BOTPROGRESS:
	default:
		m_fPercent = 0.0f;
		m_szHeader[0] = '\0';
		m_iFlags = 0;
		m_szLocalizedHeader = NULL;
		break;
	}

	return 1;
}
