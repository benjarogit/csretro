#include "precompiled.h"

LINK_ENTITY_TO_CLASS(weapon_incgrenade, CIncendiary, CCSIncendiary)

void CIncendiary::Spawn()
{
	Precache();

	m_iId = WEAPON_INCGRENADE;
	SET_MODEL(edict(), "models/w_incgrenade.mdl");

	pev->dmg = 2;

	m_iDefaultAmmo = INCGRENADE_DEFAULT_GIVE;
	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bHeldIdle = false;

	FallInit();
	CBasePlayerWeapon::Spawn();
}

void CIncendiary::Precache()
{
	PRECACHE_MODEL("models/v_incgrenade.mdl");
	PRECACHE_MODEL("models/p_incgrenade.mdl");
	PRECACHE_MODEL("models/w_incgrenade.mdl");
	PRECACHE_SOUND("weapons/pinpull.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_hit.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_explode.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_gibs.wav");
	PRECACHE_MODEL("models/w_broke_molotov.mdl");
	UTIL_PrecacheOther("inferno");

	m_usCreateInferno = PRECACHE_EVENT(1, "events/createinferno.sc");
}

int CIncendiary::GetItemInfo(ItemInfo *p)
{
	auto info = GetWeaponInfo(WEAPON_INCGRENADE);

	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Incgrenade";
	p->iMaxAmmo1 = info ? info->maxRounds : MAX_AMMO_INCGRENADE;
	p->iMaxClip = info ? info->gunClipSize : WEAPON_NOCLIP;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iSlot = 3;
	p->iPosition = 5;
	p->iId = m_iId = WEAPON_INCGRENADE;
	p->iWeight = INCGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

BOOL CIncendiary::Deploy()
{
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bHeldIdle = false;
	m_fMaxSpeed = INCGRENADE_MAX_SPEED;
	return DefaultDeploy("models/v_incgrenade.mdl", "models/p_incgrenade.mdl", INCGRENADE_DRAW, "grenade", UseDecrement() != FALSE);
}

void CIncendiary::Holster(int skiplocal)
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5f;

	if (!m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
		DestroyItem();

	m_flStartThrow = 0;
	m_flReleaseThrow = -1.0f;
	m_flThrowStrength = 1.0f;
	m_bHeldIdle = false;
}

void CIncendiary::ItemPostFrame()
{
	if (m_flStartThrow && (m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2)))
	{
		UpdateGrenadeCookStrength();

		if (!m_bHeldIdle && m_flTimeWeaponIdle <= UTIL_WeaponTimeBase())
		{
			// Keep the last pinpull frame (pin out, spoon held). Idle is the
			// rest pose and looks like nothing is prepared.
			m_bHeldIdle = true;
		}
	}

	CBasePlayerWeapon::ItemPostFrame();
}

void CIncendiary::PrimaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK2) ? 0.5f : 1.0f);
}

void CIncendiary::SecondaryAttack()
{
	StartThrow((m_pPlayer->pev->button & IN_ATTACK) ? 0.5f : 0.0f);
}

void CIncendiary::StartThrow(float strength)
{
	if (!m_flStartThrow && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)
	{
		m_flThrowStrength = strength;
		m_flReleaseThrow = 0;
		m_flStartThrow = gpGlobals->time;
		m_bHeldIdle = false;
		SendWeaponAnim(INCGRENADE_PINPULL, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + INCGRENADE_PIN_TIME;
	}
}

void CIncendiary::WeaponIdle()
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

		m_pPlayer->ThrowGrenade(this, vecSrc, vecThrow, 0, m_usCreateInferno);
		SendWeaponAnim(INCGRENADE_THROW, UseDecrement() != FALSE);
		m_pPlayer->SetAnimation(PLAYER_ATTACK1);

		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + INCGRENADE_THROW_TIME;

		if (--m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
	}
	else if (m_flReleaseThrow > 0)
	{
		m_flStartThrow = 0;
		if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType])
			SendWeaponAnim(INCGRENADE_DRAW, UseDecrement() != FALSE);
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
		SendWeaponAnim(INCGRENADE_IDLE, UseDecrement() != FALSE);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT(10, 15);
	}
}

LINK_HOOK_CLASS_CHAIN3(BOOL, CBasePlayerWeapon, CIncendiary, CanDeploy)

BOOL EXT_FUNC CIncendiary::__API_HOOK(CanDeploy)()
{
	return m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0;
}
