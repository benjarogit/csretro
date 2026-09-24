#pragma once

const int INFERNO_MAX_FLAMES = 16;
const float INFERNO_SLOPE_MIN_Z = 0.8660254f; // cos(30°)
const float INFERNO_SMOKE_RADIUS = 115.0f;
const float INFERNO_CYLINDER_BELOW = 64.0f;
const float INFERNO_CYLINDER_ABOVE = 120.0f;
const float INFERNO_AIR_TRANSFER = 128.0f;
// CS:GO-compatible maximum flight time. At expiry the projectile transfers
// the ignition to a suitable floor below instead of creating fire in mid-air.
const float INFERNO_AIR_FUSE = 2.0f;
const float INFERNO_FUSE_EXTENSION = 0.7f;
const float INFERNO_STILL_TIME = 0.5f;
// CS:GO uses ~5 u/s. Stay well below GoldSrc apex speed so a lob does not
// ignite in mid-air on a floor 128u below.
const float INFERNO_STILL_SPEED = 30.0f;
// Accept stairs/ramps when the nade has already settled (cos 45°).
const float INFERNO_SETTLE_SLOPE_MIN_Z = 0.70710678f;
const float INFERNO_NEAR_FLOOR = 48.0f;

enum InfernoEventMode
{
	INFERNO_EV_START = 0,
	INFERNO_EV_NODE = 1,
	INFERNO_EV_EXTINGUISH = 2,
};

struct InfernoConfig
{
	float damagePerSecond;
	float damageInterval;
	float flameLifetime;
	float maxRange;
	float spreadSpeedMult;
	int maxFlames;
	float flameSpacing;
	int maxChildDepth;
	float spawnAngle;
};

struct InfernoNode
{
	Vector origin;
	int depth;
	bool active;
};

class CInferno: public CBaseEntity
{
public:
	virtual void Spawn();
	virtual void Precache();
	virtual int ObjectCaps() { return FCAP_DONT_SAVE; }

	void EXPORT InfernoThink();

	static CInferno *CreateInferno(entvars_t *pevOwner, const Vector &origin, const Vector &impactNormal, int weaponId, unsigned short usEvent);
	static const InfernoConfig &ConfigForWeapon(int weaponId);
	static bool IsWalkableNormal(const Vector &normal);
	static bool PointInActiveSmoke(const Vector &origin);
	static int CountActiveSmokeCoverage(const InfernoNode *nodes, int count);

private:
	void SpreadTick();
	void DamageTick();
	void Playback(int mode, const Vector &origin, const Vector &impactNormal = g_vecZero);
	void Extinguish();
	bool TryAddChild(const InfernoNode &parent, const Vector &dir);
	bool PlayerInNode(CBasePlayer *pPlayer, const InfernoNode &node) const;
	bool NodeCanBurnPlayer(const InfernoNode &node, CBasePlayer *pPlayer) const;

public:
	InfernoConfig m_config;
	InfernoNode m_nodes[INFERNO_MAX_FLAMES];
	int m_iNodeCount;
	int m_iWeaponId;
	unsigned short m_usEvent;
	float m_flSpawnTime;
	float m_flNextSpread;
	float m_flNextDamage;
	float m_flExpireTime;
};

bool Inferno_IsActiveSmokeGrenade(CGrenade *pGrenade);
