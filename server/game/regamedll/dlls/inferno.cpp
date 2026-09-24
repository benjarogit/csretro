#include "precompiled.h"

namespace
{
const InfernoConfig kMolotovConfig = {
	32.0f, 0.20f, 7.0f, 150.0f, 1.0f, INFERNO_MAX_FLAMES, 42.0f, 4, 45.0f
};

const InfernoConfig kIncendiaryConfig = {
	32.0f, 0.20f, 5.5f, 110.0f, 10.0f, INFERNO_MAX_FLAMES, 42.0f, 4, 45.0f
};

int g_iMolotovGroundSpr;
int g_iIncGroundSpr;

void InfernoTempSprite(const Vector &origin, int modelIndex, int scaleTenths)
{
	if (modelIndex <= 0)
		return;
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
		WRITE_BYTE(TE_SPRITE);
		WRITE_COORD(origin.x);
		WRITE_COORD(origin.y);
		WRITE_COORD(origin.z + 2.0f);
		WRITE_SHORT(modelIndex);
		WRITE_BYTE(scaleTenths);
		WRITE_BYTE(180);
	MESSAGE_END();
}

void InfernoTempLight(const Vector &origin)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
		WRITE_BYTE(TE_DLIGHT);
		WRITE_COORD(origin.x);
		WRITE_COORD(origin.y);
		WRITE_COORD(origin.z + 8.0f);
		WRITE_BYTE(16);
		WRITE_BYTE(255);
		WRITE_BYTE(110);
		WRITE_BYTE(35);
		WRITE_BYTE(10);
		WRITE_BYTE(12);
	MESSAGE_END();
}
}

bool Inferno_IsActiveSmokeGrenade(CGrenade *pGrenade)
{
	if (!pGrenade || pGrenade->m_bIsC4 || !pGrenade->m_bDetonated)
		return false;

	return pGrenade->m_SGSmoke >= 1 && pGrenade->m_SGSmoke <= 20;
}

const InfernoConfig &CInferno::ConfigForWeapon(int weaponId)
{
	if (weaponId == WEAPON_INCGRENADE)
		return kIncendiaryConfig;

	return kMolotovConfig;
}

bool CInferno::IsWalkableNormal(const Vector &normal)
{
	return normal.z >= INFERNO_SLOPE_MIN_Z;
}

bool CInferno::PointInActiveSmoke(const Vector &origin)
{
	edict_t *pentFind = nullptr;
	while ((pentFind = FIND_ENTITY_BY_CLASSNAME(pentFind, "grenade")))
	{
		if (FNullEnt(pentFind))
			break;

		CGrenade *pGrenade = (CGrenade *)CBaseEntity::Instance(pentFind);
		if (!Inferno_IsActiveSmokeGrenade(pGrenade))
			continue;

		if ((pGrenade->m_vSmokeDetonate - origin).Length() <= INFERNO_SMOKE_RADIUS)
			return true;
	}

	return false;
}

int CInferno::CountActiveSmokeCoverage(const InfernoNode *nodes, int count)
{
	int covered = 0;
	for (int i = 0; i < count; i++)
	{
		if (!nodes[i].active)
			continue;
		if (PointInActiveSmoke(nodes[i].origin))
			covered++;
	}

	return covered;
}

LINK_ENTITY_TO_CLASS(inferno, CInferno, CCSInferno)

void CInferno::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW;
	pev->takedamage = DAMAGE_NO;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
}

void CInferno::Precache()
{
	PRECACHE_SOUND("weapons/grenade/molotov_explode.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_extinguish.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_fire_fadeout.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_fire_ground.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_idle_loop.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_hit.wav");
	PRECACHE_SOUND("weapons/grenade/molotov_gibs.wav");
	g_iMolotovGroundSpr = PRECACHE_MODEL("sprites/grenade/molotov_fire_ground.spr");
	g_iIncGroundSpr = PRECACHE_MODEL("sprites/grenade/incendiary_fire_ground.spr");
	PRECACHE_MODEL("sprites/grenade/molotov_fire_column.spr");
}

CInferno *CInferno::CreateInferno(entvars_t *pevOwner, const Vector &origin, const Vector &impactNormal, int weaponId, unsigned short usEvent)
{
	if (PointInActiveSmoke(origin))
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE inferno_rejected reason=active_smoke origin=(%.1f %.1f %.1f)\n", origin.x, origin.y, origin.z);
		return nullptr;
	}

	if (csretro_fire_debug.value != 0.0f)
		ALERT(at_console, "CSRETRO_FIRE inferno_created weapon=%d origin=(%.1f %.1f %.1f) normal=(%.2f %.2f %.2f)\n",
			weaponId, origin.x, origin.y, origin.z, impactNormal.x, impactNormal.y, impactNormal.z);

	CInferno *pInferno = GetClassPtr<CCSInferno>((CInferno *)nullptr);
	pInferno->Spawn();
	pInferno->Precache();

	UTIL_SetOrigin(pInferno->pev, origin);
	pInferno->pev->owner = pevOwner ? ENT(pevOwner) : nullptr;
	pInferno->m_iWeaponId = weaponId;
	pInferno->m_usEvent = usEvent;
	pInferno->m_config = ConfigForWeapon(weaponId);
	pInferno->m_iNodeCount = 0;
	Q_memset(pInferno->m_nodes, 0, sizeof(pInferno->m_nodes));

	pInferno->m_nodes[0].origin = origin;
	pInferno->m_nodes[0].depth = 0;
	pInferno->m_nodes[0].active = true;
	pInferno->m_iNodeCount = 1;

	pInferno->m_flSpawnTime = gpGlobals->time;
	pInferno->m_flExpireTime = gpGlobals->time + pInferno->m_config.flameLifetime;
	pInferno->m_flNextSpread = gpGlobals->time + (0.12f / pInferno->m_config.spreadSpeedMult);
	pInferno->m_flNextDamage = gpGlobals->time + pInferno->m_config.damageInterval;

	if (weaponId == WEAPON_INCGRENADE)
		pInferno->pev->classname = MAKE_STRING("weapon_incgrenade");
	else
		pInferno->pev->classname = MAKE_STRING("weapon_molotov");

	EMIT_SOUND(ENT(pInferno->pev), CHAN_WEAPON, "weapons/grenade/molotov_explode.wav", VOL_NORM, ATTN_NORM);
	EMIT_SOUND(ENT(pInferno->pev), CHAN_STATIC, "weapons/grenade/molotov_idle_loop.wav", 0.55f, ATTN_NORM);

	pInferno->Playback(INFERNO_EV_START, origin, impactNormal);
	pInferno->SetThink(&CInferno::InfernoThink);
	pInferno->pev->nextthink = gpGlobals->time + 0.05f;
	return pInferno;
}

void CInferno::Playback(int mode, const Vector &origin, const Vector &impactNormal)
{
	if (!m_usEvent)
		return;

	Vector eventOrigin = origin;
	PLAYBACK_EVENT_FULL(FEV_RELIABLE | FEV_GLOBAL, edict(), m_usEvent, 0,
		(float *)&eventOrigin, (float *)&impactNormal,
		Q_max(m_flExpireTime - gpGlobals->time, 0.1f), m_config.flameLifetime,
		mode, m_iWeaponId, FALSE, FALSE);

	// GoldSrc floor mark only on ignition. Per-node TE_SPRITE plus the
	// reliable event doubled the tent load; Incendiary's 10x spread then
	// filled the pool and left damage without visible fire.
	if (mode == INFERNO_EV_START)
	{
		const int spr = (m_iWeaponId == WEAPON_INCGRENADE && g_iIncGroundSpr > 0)
			? g_iIncGroundSpr : g_iMolotovGroundSpr;
		InfernoTempSprite(origin, spr, 10);
		InfernoTempLight(origin);
	}
}

void CInferno::Extinguish()
{
	STOP_SOUND(ENT(pev), CHAN_STATIC, "weapons/grenade/molotov_idle_loop.wav");
	EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/grenade/molotov_extinguish.wav", VOL_NORM, ATTN_NORM);
	Playback(INFERNO_EV_EXTINGUISH, pev->origin);
	UTIL_Remove(this);
}

void CInferno::InfernoThink()
{
	if (gpGlobals->time >= m_flExpireTime)
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, "weapons/grenade/molotov_idle_loop.wav");
		EMIT_SOUND(ENT(pev), CHAN_WEAPON, "weapons/grenade/molotov_fire_fadeout.wav", VOL_NORM, ATTN_NORM);
		Playback(INFERNO_EV_EXTINGUISH, pev->origin);
		UTIL_Remove(this);
		return;
	}

	int active = 0;
	for (int i = 0; i < m_iNodeCount; i++)
	{
		if (m_nodes[i].active)
			active++;
	}

	if (active > 0 && CountActiveSmokeCoverage(m_nodes, m_iNodeCount) * 3 > active)
	{
		Extinguish();
		return;
	}

	if (gpGlobals->time >= m_flNextSpread)
	{
		SpreadTick();
		m_flNextSpread = gpGlobals->time + (0.12f / m_config.spreadSpeedMult);
	}

	if (gpGlobals->time >= m_flNextDamage)
	{
		DamageTick();
		m_flNextDamage = gpGlobals->time + m_config.damageInterval;
	}

	pev->nextthink = gpGlobals->time + 0.05f;
}

bool CInferno::TryAddChild(const InfernoNode &parent, const Vector &dir)
{
	if (m_iNodeCount >= m_config.maxFlames)
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE node_rejected reason=max_flames\n");
		return false;
	}

	Vector dest = parent.origin + dir * m_config.flameSpacing;
	if ((dest - m_nodes[0].origin).Length() > m_config.maxRange)
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE node_rejected reason=max_range dest=(%.1f %.1f %.1f)\n", dest.x, dest.y, dest.z);
		return false;
	}

	if (PointInActiveSmoke(dest))
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE node_rejected reason=smoke dest=(%.1f %.1f %.1f)\n", dest.x, dest.y, dest.z);
		return false;
	}

	for (int i = 0; i < m_iNodeCount; i++)
	{
		if ((m_nodes[i].origin - dest).Length() < 20.0f)
		{
			if (csretro_fire_debug.value != 0.0f)
				ALERT(at_console, "CSRETRO_FIRE node_rejected reason=duplicate dest=(%.1f %.1f %.1f)\n", dest.x, dest.y, dest.z);
			return false;
		}
	}

	TraceResult tr;
	UTIL_TraceLine(parent.origin + Vector(0, 0, 8), dest + Vector(0, 0, 8), ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction < 1.0f)
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE node_rejected reason=blocked fraction=%.2f dest=(%.1f %.1f %.1f)\n", tr.flFraction, dest.x, dest.y, dest.z);
		return false;
	}

	UTIL_TraceLine(dest + Vector(0, 0, 32), dest + Vector(0, 0, -72), ignore_monsters, ENT(pev), &tr);
	if (tr.flFraction >= 1.0f || !IsWalkableNormal(tr.vecPlaneNormal))
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE node_rejected reason=no_walkable_support fraction=%.2f normal=(%.2f %.2f %.2f) dest=(%.1f %.1f %.1f)\n",
				tr.flFraction, tr.vecPlaneNormal.x, tr.vecPlaneNormal.y, tr.vecPlaneNormal.z, dest.x, dest.y, dest.z);
		return false;
	}

	if (PointInActiveSmoke(tr.vecEndPos))
	{
		if (csretro_fire_debug.value != 0.0f)
			ALERT(at_console, "CSRETRO_FIRE node_rejected reason=smoke_support origin=(%.1f %.1f %.1f)\n", tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z);
		return false;
	}

	InfernoNode &child = m_nodes[m_iNodeCount++];
	child.origin = tr.vecEndPos;
	child.depth = parent.depth + 1;
	child.active = true;
	if (csretro_fire_debug.value != 0.0f)
		ALERT(at_console, "CSRETRO_FIRE node index=%d depth=%d origin=(%.1f %.1f %.1f)\n",
			m_iNodeCount - 1, child.depth, child.origin.x, child.origin.y, child.origin.z);
	Playback(INFERNO_EV_NODE, child.origin);
	return true;
}

void CInferno::SpreadTick()
{
	if (m_iNodeCount >= m_config.maxFlames)
		return;

	const int existing = m_iNodeCount;
	const float step = m_config.spawnAngle;
	for (int i = 0; i < existing && m_iNodeCount < m_config.maxFlames; i++)
	{
		if (!m_nodes[i].active || m_nodes[i].depth >= m_config.maxChildDepth)
			continue;

		for (float yaw = 0.0f; yaw < 360.0f && m_iNodeCount < m_config.maxFlames; yaw += step)
		{
			const float rad = yaw * (3.14159265f / 180.0f);
			TryAddChild(m_nodes[i], Vector(Q_cos(rad), Q_sin(rad), 0));
		}
	}
}

bool CInferno::PlayerInNode(CBasePlayer *pPlayer, const InfernoNode &node) const
{
	if (!pPlayer || !node.active)
		return false;

	Vector delta = pPlayer->pev->origin - node.origin;
	delta.z = 0;
	if (delta.Length() > m_config.flameSpacing * 0.65f)
		return false;

	const float dz = pPlayer->pev->origin.z - node.origin.z;
	return dz >= -INFERNO_CYLINDER_BELOW && dz <= INFERNO_CYLINDER_ABOVE;
}

bool CInferno::NodeCanBurnPlayer(const InfernoNode &node, CBasePlayer *pPlayer) const
{
	if (!PlayerInNode(pPlayer, node))
		return false;

	TraceResult tr;
	UTIL_TraceLine(node.origin + Vector(0, 0, 16), pPlayer->pev->origin, ignore_monsters, ENT(pev), &tr);
	return tr.flFraction >= 1.0f || tr.pHit == pPlayer->edict();
}

void CInferno::DamageTick()
{
	const float age = gpGlobals->time - m_flSpawnTime;
	const float tickDamage = Q_min(1.0f + (float)(int)(age / m_config.damageInterval),
		m_config.damagePerSecond * m_config.damageInterval);

	CBaseEntity *pEntity = nullptr;
	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "player")))
	{
		if (FNullEnt(pEntity->edict()) || !pEntity->IsPlayer() || !pEntity->IsAlive())
			continue;

		CBasePlayer *pPlayer = static_cast<CBasePlayer *>(pEntity);
		bool hit = false;
		for (int i = 0; i < m_iNodeCount; i++)
		{
			if (NodeCanBurnPlayer(m_nodes[i], pPlayer))
			{
				hit = true;
				break;
			}
		}

		if (!hit)
			continue;

		entvars_t *pevAttacker = pev->owner ? VARS(pev->owner) : pev;
		pPlayer->TakeDamage(pev, pevAttacker, (float)tickDamage, DMG_BURN);
	}
}
