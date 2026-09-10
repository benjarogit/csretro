#include "precompiled.h"

LINK_ENTITY_TO_CLASS(weapon_hegrenade, CHEGrenade, CCSHEGrenade)

void CHEGrenade::Spawn()
{
	Precache();

	m_iId = WEAPON_HEGRENADE;
	SET_MODEL(edict(), "models/w_hegrenade.mdl");

	pev->dmg = 4;

	m_iDefaultAmmo = HEGRENADE_DEFAULT_GIVE;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;

	// Get ready to fall down
	FallInit();

	// extend
	CBasePlayerWeapon::Spawn();
}

void CHEGrenade::Precache()
{
	PRECACHE_MODEL("models/v_hegrenade.mdl");

	PRECACHE_SOUND("weapons/hegrenade-1.wav");
	PRECACHE_SOUND("weapons/hegrenade-2.wav");
	PRECACHE_SOUND("weapons/he_bounce-1.wav");
	PRECACHE_SOUND("weapons/pinpull.wav");

	m_usCreateExplosion = PRECACHE_EVENT(1, "events/createexplo.sc");
}

int CHEGrenade::GetItemInfo(ItemInfo *p)
{
	auto info = GetWeaponInfo(WEAPON_HEGRENADE);

	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "HEGrenade";

	p->iMaxAmmo1 = info ? info->maxRounds : MAX_AMMO_HEGRENADE;
	p->iMaxClip = info ? info->gunClipSize : WEAPON_NOCLIP;

	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_HEGRENADE;
	p->iWeight = HEGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CHEGrenade::Deploy()
{
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_fMaxSpeed = HEGRENADE_MAX_SPEED;

	return DefaultDeploy("models/v_hegrenade.mdl", "models/p_hegrenade.mdl", HEGRENADE_DRAW, "grenade", UseDecrement() != FALSE);
}

void CHEGrenade::Holster(int skiplocal)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
#ifndef REGAMEDLL_FIXES
		// Moved to DestroyItem()
		m_pPlayer->pev->weapons &= ~(1 << WEAPON_HEGRENADE);
#endif

		DestroyItem();
	}

	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
}

void CHEGrenade::PrimaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK2) ? 0.5f : 1.0f);
}

void CHEGrenade::SecondaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK) ? 0.5f : 0.0f);
}

void CHEGrenade::StartThrow(float strength)
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		m_flThrowStrength = strength;
		m_flReleaseThrow = 0;
		m_flStartThrow = gpGlobals->time;

		SendWeaponAnim(HEGRENADE_PULLPIN, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5f;
	}
}

void CHEGrenade::WeaponIdle()
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

		m_pPlayer->ThrowGrenade(this, vecSrc, vecThrow, 1.5, m_usCreateExplosion);

		SendWeaponAnim(HEGRENADE_THROW, UseDecrement() != FALSE);

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

		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		{
			SendWeaponAnim(HEGRENADE_DRAW, UseDecrement() != FALSE);
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
		m_flReleaseThrow = -1.0f;
	}
	else if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
	{
		SendWeaponAnim(HEGRENADE_IDLE, UseDecrement() != FALSE);

		// how long till we do this again.
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
	}
}

LINK_HOOK_CLASS_CHAIN3(BOOL, CBasePlayerWeapon, CHEGrenade, CanDeploy)

BOOL EXT_FUNC CHEGrenade::__API_HOOK(CanDeploy)()
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
