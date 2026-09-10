#include "precompiled.h"

LINK_ENTITY_TO_CLASS(weapon_flashbang, CFlashbang, CCSFlashbang)

void CFlashbang::Spawn()
{
	Precache();

	m_iId = WEAPON_FLASHBANG;
	SET_MODEL(edict(), "models/w_flashbang.mdl");

	pev->dmg = 4;

	m_iDefaultAmmo = FLASHBANG_DEFAULT_GIVE;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;

	// Get ready to fall down
	FallInit();

	// extend
	CBasePlayerWeapon::Spawn();
}

void CFlashbang::Precache()
{
	PRECACHE_MODEL("models/v_flashbang.mdl");

	PRECACHE_SOUND("weapons/flashbang-1.wav");
	PRECACHE_SOUND("weapons/flashbang-2.wav");
	PRECACHE_SOUND("weapons/pinpull.wav");
}

int CFlashbang::GetItemInfo(ItemInfo *p)
{
	auto info = GetWeaponInfo(WEAPON_FLASHBANG);

	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Flashbang";

	p->iMaxAmmo1 = info ? info->maxRounds : MAX_AMMO_FLASHBANG;
	p->iMaxClip = info ? info->gunClipSize : WEAPON_NOCLIP;

	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iSlot = 3;
	p->iPosition = 2;
	p->iId = m_iId = WEAPON_FLASHBANG;
	p->iWeight = FLASHBANG_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CFlashbang::Deploy()
{
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_fMaxSpeed = FLASHBANG_MAX_SPEED;

	return DefaultDeploy("models/v_flashbang.mdl", "models/p_flashbang.mdl", FLASHBANG_DRAW, "grenade", UseDecrement() != FALSE);
}

void CFlashbang::Holster(int skiplocal)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
#ifndef REGAMEDLL_FIXES
		// Moved to DestroyItem()
		m_pPlayer->pev->weapons &= ~(1 << WEAPON_FLASHBANG);
#endif
		DestroyItem();
	}

	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
}

void CFlashbang::PrimaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK2) ? 0.5f : 1.0f);
}

void CFlashbang::SecondaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK) ? 0.5f : 0.0f);
}

void CFlashbang::StartThrow(float strength)
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		m_flThrowStrength = strength;
		m_flReleaseThrow = 0;
		m_flStartThrow = gpGlobals->time;

		SendWeaponAnim(FLASHBANG_PULLPIN, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;
	}
}

void CFlashbang::WeaponIdle()
{
	if (m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2))
		return;

	if (m_flReleaseThrow == 0 && m_flStartThrow != 0.0f)
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

		m_pPlayer->ThrowGrenade(this, vecSrc, vecThrow, 1.5);

		SendWeaponAnim(FLASHBANG_THROW, UseDecrement() != FALSE);

		// player "shoot" animation
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

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
		RetireWeapon();
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1);

		if (flRand <= 0.75)
		{
			iAnim = FLASHBANG_IDLE;

			// how long till we do this again.
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
		}
		else
		{
		#ifdef REGAMEDLL_FIXES
			iAnim = FLASHBANG_IDLE;
		#else
			// TODO: This is a bug?
			iAnim = *(int *)&flRand;
		#endif
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 75.0f / 30.0f;
		}

		SendWeaponAnim(iAnim, UseDecrement() != FALSE);
	}
}

LINK_HOOK_CLASS_CHAIN3(BOOL, CBasePlayerWeapon, CFlashbang, CanDeploy)

BOOL EXT_FUNC CFlashbang::__API_HOOK(CanDeploy)()
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
