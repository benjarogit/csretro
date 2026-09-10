#include "stdafx.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"

#ifndef MOLOTOV_MAX_SPEED
#define MOLOTOV_MAX_SPEED 245.0f
#endif
#ifndef MOLOTOV_PIN_TIME
#define MOLOTOV_PIN_TIME 1.0f
#endif

enum molotov_e
{
	MOLOTOV_IDLE,
	MOLOTOV_PINPULL,
	MOLOTOV_THROW,
	MOLOTOV_DRAW,
};

LINK_ENTITY_TO_CLASS(weapon_molotov, CMolotov)

void CMolotov::Spawn(void)
{
	Precache();

	m_iId = WEAPON_MOLOTOV;
	SET_MODEL(edict(), "models/w_molotov.mdl");

	pev->dmg = 2;
	m_iDefaultAmmo = MOLOTOV_DEFAULT_GIVE;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bHeldIdle = false;
	FallInit();
}

void CMolotov::Precache(void)
{
	PRECACHE_MODEL("models/v_molotov.mdl");
	PRECACHE_SOUND("weapons/pinpull.wav");
	PRECACHE_SOUND("weapons/molotov_light.wav");
	m_usCreateInferno = PRECACHE_EVENT(1, "events/createinferno.sc");
}

int CMolotov::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Molotov";
	p->iMaxAmmo1 = MAX_AMMO_MOLOTOV;
	p->iMaxClip = WEAPON_NOCLIP;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iSlot = 3;
	p->iPosition = 4;
	p->iId = m_iId = WEAPON_MOLOTOV;
	p->iWeight = MOLOTOV_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;
	return 1;
}

BOOL CMolotov::Deploy(void)
{
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bHeldIdle = false;
	m_fMaxSpeed = MOLOTOV_MAX_SPEED;
	return DefaultDeploy("models/v_molotov.mdl", "models/p_molotov.mdl", MOLOTOV_DRAW, "grenade", UseDecrement() != FALSE);
}

void CMolotov::Holster(int skiplocal)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		DestroyItem();

	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bHeldIdle = false;
}

void CMolotov::ItemPostFrame(void)
{
	if (m_flStartThrow && (m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2)))
	{
		UpdateGrenadeCookStrength();

		if (!m_bHeldIdle && m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
		{
			SendWeaponAnim(MOLOTOV_IDLE, UseDecrement() != FALSE);
			m_bHeldIdle = true;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CMolotov::PrimaryAttack(void)
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK2) ? 0.5f : 1.0f);
}

void CMolotov::SecondaryAttack(void)
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK) ? 0.5f : 0.0f);
}

void CMolotov::StartThrow(float strength)
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		m_flThrowStrength = strength;
		m_flReleaseThrow = 0;
		m_flStartThrow = gpGlobals->time;
		m_bHeldIdle = false;
		SendWeaponAnim(MOLOTOV_PINPULL, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + MOLOTOV_PIN_TIME;
	}
}

BOOL CMolotov::CanHolster(void)
{
	return CanHolsterGrenadeThrow();
}

void CMolotov::WeaponIdle(void)
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
		CGrenade::ShootFireGrenade(m_pPlayer->pev, vecSrc, vecThrow, WEAPON_MOLOTOV, m_usCreateInferno);
		SendWeaponAnim(MOLOTOV_THROW, UseDecrement() != FALSE);

#ifndef CLIENT_DLL
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);
#endif
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.75f;

		if (--m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
	}
	else if (m_flReleaseThrow > 0)
	{
		m_flStartThrow = 0;
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			SendWeaponAnim(MOLOTOV_DRAW, UseDecrement() != FALSE);
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
		SendWeaponAnim(MOLOTOV_IDLE, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
	}
}

BOOL CMolotov::CanDeploy(void)
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
