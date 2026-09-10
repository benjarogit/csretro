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

#include "stdafx.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

LINK_ENTITY_TO_CLASS(weapon_smokegrenade, CSmokeGrenade)

void CSmokeGrenade::Spawn(void)
{
	pev->classname = MAKE_STRING("weapon_smokegrenade");

	Precache();

	m_iId = WEAPON_SMOKEGRENADE;
	SET_MODEL(edict(), "models/w_smokegrenade.mdl");

	pev->dmg = 4;

	m_iDefaultAmmo = SMOKEGRENADE_DEFAULT_GIVE;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	m_flThrowStrength = 1.0f;

	// get ready to fall down.
	FallInit();
}

void CSmokeGrenade::Precache()
{
	PRECACHE_MODEL("models/v_smokegrenade.mdl");

	PRECACHE_SOUND("weapons/pinpull.wav");
	PRECACHE_SOUND("weapons/sg_explode.wav");

	m_usCreateSmoke = PRECACHE_EVENT(1, "events/createsmoke.sc");
}

int CSmokeGrenade::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "SmokeGrenade";

	p->iMaxAmmo1 = MAX_AMMO_SMOKEGRENADE;
	p->iMaxClip = WEAPON_NOCLIP;

	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iSlot = 3;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_SMOKEGRENADE;
	p->iWeight = SMOKEGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CSmokeGrenade::Deploy()
{
	m_flReleaseThrow = -1;
	m_flThrowStrength = 1.0f;
	m_fMaxSpeed = SMOKEGRENADE_MAX_SPEED;

	return DefaultDeploy("models/v_smokegrenade.mdl", "models/p_smokegrenade.mdl", SMOKEGRENADE_DRAW, "grenade", UseDecrement() != FALSE);
}

BOOL CSmokeGrenade::CanHolster()
{
	return CanHolsterGrenadeThrow();
}

void CSmokeGrenade::Holster(int skiplocal)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		// no more smokegrenades!
		// clear the smokegrenade of bits for HUD
		m_pPlayer->pev->weapons &= ~(1 << WEAPON_SMOKEGRENADE);
		DestroyItem();
	}

	m_flStartThrow = 0;
	m_flReleaseThrow = -1;
	m_flThrowStrength = 1.0f;
}

void CSmokeGrenade::PrimaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK2) ? 0.5f : 1.0f);
}

void CSmokeGrenade::SecondaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK) ? 0.5f : 0.0f);
}

void CSmokeGrenade::StartThrow(float strength)
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		m_flThrowStrength = strength;
		m_flReleaseThrow = 0;
		m_flStartThrow = gpGlobals->time;

		SendWeaponAnim(SMOKEGRENADE_PINPULL, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;
	}
}

void CSmokeGrenade::WeaponIdle()
{
	if (m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2))
		return;

	if (m_flReleaseThrow == 0)
		m_flReleaseThrow = gpGlobals->time;

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	if (m_flStartThrow)
	{
		if (!CanCommitGrenadeThrow())
			return;

		m_pPlayer->Radio("%!MRAD_FIREINHOLE", "#Fire_in_the_hole");

		Vector vecSrc, vecThrow;
		ComputeGrenadeThrow(vecSrc, vecThrow);

		CGrenade::ShootSmokeGrenade(m_pPlayer->pev, vecSrc, vecThrow, 1.5, m_usCreateSmoke);

		SendWeaponAnim(SMOKEGRENADE_THROW, UseDecrement() != FALSE);

#ifndef CLIENT_DLL
		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
#endif
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75f;

		if (--m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			// ensure that the animation can finish playing
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		}
	}
	else if (m_flReleaseThrow > 0)
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			SendWeaponAnim(SMOKEGRENADE_DRAW, UseDecrement() != FALSE);
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
		m_flReleaseThrow = -1;
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);

		if (flRand <= 0.75)
		{
			iAnim = SMOKEGRENADE_IDLE;

			// how long till we do this again.
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
		}
		else
		{
			iAnim = SMOKEGRENADE_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 75.0f / 30.0f;
		}

		SendWeaponAnim(iAnim, UseDecrement() != FALSE);
	}
}

BOOL CSmokeGrenade::CanDeploy()
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
