#include "precompiled.h"

LINK_ENTITY_TO_CLASS(weapon_molotov, CMolotov, CCSMolotov)

void CMolotov::Spawn()
{
	Precache();

	m_iId = WEAPON_MOLOTOV;
	SET_MODEL(edict(), "models/w_molotov.mdl");

	pev->dmg = 2;

	m_iDefaultAmmo = MOLOTOV_DEFAULT_GIVE;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bCookLoop = false;
	m_bHeldIdle = false;

	FallInit();
	CBasePlayerWeapon::Spawn();
}

void CMolotov::Precache()
{
	PRECACHE_MODEL("models/v_molotov.mdl");
	PRECACHE_MODEL("models/p_molotov.mdl");
	PRECACHE_MODEL("models/w_molotov.mdl");
	PRECACHE_SOUND("weapons/pinpull.wav");
	PRECACHE_SOUND("weapons/molotov_light.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_hit.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_explode.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_idle_loop.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_gibs.wav");
	PRECACHE_MODEL("models/w_broke_molotov.mdl");
	UTIL_PrecacheOther("inferno");

	m_usCreateInferno = PRECACHE_EVENT(1, "events/createinferno.sc");
}

int CMolotov::GetItemInfo(ItemInfo *p)
{
	auto info = GetWeaponInfo(WEAPON_MOLOTOV);

	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Molotov";
	p->iMaxAmmo1 = info ? info->maxRounds : MAX_AMMO_MOLOTOV;
	p->iMaxClip = info ? info->gunClipSize : WEAPON_NOCLIP;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iSlot = 3;
	p->iPosition = 4;
	p->iId = m_iId = WEAPON_MOLOTOV;
	p->iWeight = MOLOTOV_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CMolotov::Deploy()
{
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bCookLoop = false;
	m_bHeldIdle = false;
	m_fMaxSpeed = MOLOTOV_MAX_SPEED;
	return DefaultDeploy("models/v_molotov.mdl", "models/p_molotov.mdl", MOLOTOV_DRAW, "grenade", UseDecrement() != FALSE);
}

void CMolotov::Holster(int skiplocal)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;
	STOP_SOUND(m_pPlayer->edict(), CHAN_STATIC, "weapons/grenade/molotov_idle_loop.wav");

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		DestroyItem();

	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bCookLoop = false;
	m_bHeldIdle = false;
}

void CMolotov::ItemPostFrame()
{
	// Hold cooks; release throws. Do not move TimeWeaponIdle — HE leaves the
	// pin wait alone, and pushing it to +0.25s made a one-frame release
	// (bots, tap-hold) miss the throw.
	if (m_flStartThrow && (m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2)))
	{
		UpdateGrenadeCookStrength();

		if (!m_bCookLoop && gpGlobals->time >= m_flStartThrow + MOLOTOV_WICK_TIME)
		{
			EMIT_SOUND(m_pPlayer->edict(), CHAN_STATIC, "weapons/grenade/molotov_idle_loop.wav", 0.45f, ATTN_NORM);
			m_bCookLoop = true;
		}

		if (!m_bHeldIdle && m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
		{
			// Keep the last pullpin frame (lit rag + open Zippo). Idle is the
			// unlit rest pose and extinguishes the wick visually.
			m_bHeldIdle = true;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CMolotov::PrimaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK2) ? 0.5f : 1.0f);
}

void CMolotov::SecondaryAttack()
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
		m_bCookLoop = false;
		m_bHeldIdle = false;
		SendWeaponAnim(MOLOTOV_PINPULL, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + MOLOTOV_PIN_TIME;
	}
}

void CMolotov::WeaponIdle()
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

		STOP_SOUND(m_pPlayer->edict(), CHAN_STATIC, "weapons/grenade/molotov_idle_loop.wav");
		m_bCookLoop = false;
		m_pPlayer->ThrowGrenade(this, vecSrc, vecThrow, 0, m_usCreateInferno);
		SendWeaponAnim(MOLOTOV_THROW, UseDecrement() != FALSE);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

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

LINK_HOOK_CLASS_CHAIN3(BOOL, CBasePlayerWeapon, CMolotov, CanDeploy)

BOOL EXT_FUNC CMolotov::__API_HOOK(CanDeploy)()
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
