#include "events.h"
#include "com_model.h"
#include "weapontype.h"

extern vec3_t v_origin;

namespace
{
const int kInfernoEvStart = 0;
const int kInfernoEvNode = 1;
const int kInfernoEvExtinguish = 2;
const int kInfernoMagic = 0x1F10;
// The intact model's 50 fps pullpin sequence lights the wick at source frame
// 23. View-model curstate.frame is not a reliable source-frame clock, so track
// elapsed time from the held attack input.
const float kMolotovWickDelay = 23.0f / 50.0f;

float g_infernoExtinguishUntil;
int g_infernoExtinguishId;

Vector g_molotovWickOrigin;
float g_molotovWickTime;
bool g_molotovWickValid;

void InfernoThink(tempent_s *te, float frametime, float currenttime)
{
	if (g_infernoExtinguishUntil > currenttime)
	{
		if (te->entity.curstate.iuser3 == g_infernoExtinguishId &&
			te->entity.curstate.fuser4 <= g_infernoExtinguishUntil - 0.25f)
		{
			te->die = currenttime;
			return;
		}
	}

	const float remaining = te->entity.curstate.fuser1 - currenttime;
	const float lifetime = te->entity.curstate.fuser2;
	if (lifetime > 0.05f)
	{
		const float ageScale = remaining / lifetime;
		if (ageScale < 0.0f)
			te->entity.curstate.scale = 0.15f;
		else
			te->entity.curstate.scale = 0.35f + 0.85f * ageScale;
	}

	if (te->entity.curstate.renderamt > 40 && remaining < 0.6f)
		te->entity.curstate.renderamt = (int)(255.0f * remaining / 0.6f);

	EV_CS16Client_KillEveryRound(te, frametime, currenttime);
}

model_s *LoadInfernoSprite(const char *path, int *modelIndex)
{
	int index = 0;
	model_s *model = gEngfuncs.CL_LoadModel(path, &index);
	if (!model)
	{
		const int sprite = gEngfuncs.pfnSPR_Load(path);
		model = (model_s *)gEngfuncs.GetSpritePointer(sprite);
		index = 0;
	}
	if (modelIndex)
		*modelIndex = index;
	return model;
}

void SpawnInfernoSprite(const Vector &origin, int weaponId, float remaining, float lifetime, bool column, int infernoId)
{
	const char *path = "sprites/grenade/molotov_fire_ground.spr";
	if (weaponId == WEAPON_INCGRENADE)
		path = "sprites/grenade/incendiary_fire_ground.spr";
	if (column)
		path = "sprites/grenade/molotov_fire_column.spr";

	int modelIndex = 0;
	model_s *model = LoadInfernoSprite(path, &modelIndex);
	if (!model && weaponId == WEAPON_INCGRENADE)
	{
		path = "sprites/grenade/molotov_fire_ground.spr";
		model = LoadInfernoSprite(path, &modelIndex);
	}
	if (!model && column)
	{
		path = "sprites/grenade/molotov_fire_ground.spr";
		model = LoadInfernoSprite(path, &modelIndex);
	}
	if (!model)
	{
		gEngfuncs.Con_DPrintf("CSRETRO_INFERNO_RENDER missing sprite=%s weapon=%d\n", path, weaponId);
		return;
	}

	Vector org = origin;
	org.z += column ? 8.0f : 2.0f;
	// High-priority so tracers/sparks cannot steal the tent pool and leave
	// a damaging inferno with no fire on screen.
	TEMPENTITY *pTemp = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, model);
	if (!pTemp)
		pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(org, model);
	if (!pTemp)
	{
		gEngfuncs.Con_DPrintf("CSRETRO_INFERNO_RENDER pool_full weapon=%d\n", weaponId);
		return;
	}

	pTemp->flags |= (FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP | FTENT_CLIENTCUSTOM | FTENT_PERSIST);
	pTemp->flags &= ~FTENT_NOMODEL;
	pTemp->callback = InfernoThink;
	const float spriteLifetime = column ? (remaining < 0.8f ? remaining : 0.8f) : remaining;
	pTemp->die = gEngfuncs.GetClientTime() + spriteLifetime;
	pTemp->frameMax = model->numframes > 1 ? model->numframes - 1 : 0;
	pTemp->entity.curstate.modelindex = modelIndex;
	pTemp->entity.curstate.framerate = 12.0f;
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = column ? 190 : 150;
	// Source sprites are 176x80 (ground) and 112x232 (column). Keep the
	// vertical sheet low enough to read as floor fire instead of a fire wall.
	pTemp->entity.curstate.scale = column ? 0.4f : 0.7f;
	pTemp->entity.curstate.fuser1 = gEngfuncs.GetClientTime() + spriteLifetime;
	pTemp->entity.curstate.fuser2 = column ? 0.8f : lifetime;
	// KillEveryRound compares this timestamp with the last round reset.
	// Leaving it at zero killed new fire immediately after the first reset.
	pTemp->entity.curstate.fuser4 = gEngfuncs.GetClientTime();
	pTemp->entity.curstate.iuser1 = kInfernoMagic;
	pTemp->entity.curstate.iuser2 = weaponId;
	pTemp->entity.curstate.iuser3 = infernoId;
}

void SpawnInfernoImpact(const Vector &origin, const Vector &normal, int weaponId, int infernoId)
{
	Vector effectOrigin = origin + normal * 2.0f;
	if (weaponId == WEAPON_INCGRENADE)
		gEngfuncs.pEfxAPI->R_SparkEffect(effectOrigin, 7, 45, 110);
	else
		gEngfuncs.pEfxAPI->R_ParticleBurst(effectOrigin, 18, 225, 0.35f);

	dlight_t *light = gEngfuncs.pEfxAPI->CL_AllocDlight(0x4D4F0000 | (infernoId & 0xFFFF));
	if (!light)
		return;

	light->origin = effectOrigin;
	light->radius = weaponId == WEAPON_INCGRENADE ? 104.0f : 92.0f;
	light->color.r = 255;
	light->color.g = weaponId == WEAPON_INCGRENADE ? 190 : 112;
	light->color.b = weaponId == WEAPON_INCGRENADE ? 64 : 18;
	light->die = gEngfuncs.GetClientTime() + 0.18f;
	light->decay = 360.0f;
}
}

void EV_ReadMolotovWickState(float origin[3], float *time, int *valid)
{
	if (origin)
	{
		origin[0] = g_molotovWickOrigin[0];
		origin[1] = g_molotovWickOrigin[1];
		origin[2] = g_molotovWickOrigin[2];
	}
	if (time)
		*time = g_molotovWickTime;
	if (valid)
		*valid = g_molotovWickValid ? 1 : 0;
}

void EV_CaptureMolotovWickOrigin(const float origin[3], cl_entity_s *entity)
{
	if (!origin)
		return;
	g_molotovWickOrigin[0] = origin[0];
	g_molotovWickOrigin[1] = origin[1];
	g_molotovWickOrigin[2] = origin[2];
	g_molotovWickTime = gEngfuncs.GetClientTime();
	g_molotovWickValid = true;
	if (entity)
	{
		for (int i = 0; i < 4; ++i)
		{
			entity->attachment[i][0] = origin[0];
			entity->attachment[i][1] = origin[1];
			entity->attachment[i][2] = origin[2];
		}
	}
}

static int s_molotov_held_advances;
static int s_molotov_held_captured;
static float s_molotov_held_age;
static int s_molotov_held_lit;
static int s_molotov_held_valid;
static int s_molotov_held_weapon;
static int s_molotov_held_logged;
static int s_molotov_held_state_logged;

int EV_MolotovHeldAdvances( void )
{
	return s_molotov_held_advances;
}

int EV_MolotovHeldWickCaptured( void )
{
	return s_molotov_held_captured;
}

float EV_MolotovHeldWickAge( void )
{
	return s_molotov_held_age;
}

int EV_MolotovHeldLit( void )
{
	return s_molotov_held_lit;
}

int EV_MolotovHeldWickValid( void )
{
	return s_molotov_held_valid;
}

int EV_MolotovHeldWeaponId( void )
{
	return s_molotov_held_weapon;
}

void EV_UpdateMolotovHeld()
{
	cl_entity_t *vm = gEngfuncs.GetViewModel();
	const int weaponId = HUD_GetWeapon();
	const float now = gEngfuncs.GetClientTime();
	s_molotov_held_advances++;
	static bool s_wasHeld = false;
	static bool s_ignited = false;
	static float s_pullStart = 0.0f;
	const bool held = vm && vm->model && weaponId == WEAPON_MOLOTOV
		&& (gHUD.m_iKeyBits & (IN_ATTACK | IN_ATTACK2));
	if (!s_molotov_held_state_logged && vm && vm->model
		&& strstr(vm->model->name, "v_molotov"))
	{
		gEngfuncs.Con_Printf(
			"CS Retro: viewmodel events held-state weapon=%i keys=%i held=%i lit=%i valid=%i age=%.3f\n",
			weaponId, gHUD.m_iKeyBits, held ? 1 : 0, s_ignited ? 1 : 0,
			g_molotovWickValid ? 1 : 0,
			g_molotovWickValid ? (now - g_molotovWickTime) : -1.0f );
		s_molotov_held_state_logged = 1;
	}
	if (held && !s_wasHeld)
	{
		s_pullStart = now;
		s_ignited = false;
	}
	if (held && now - s_pullStart >= kMolotovWickDelay)
		s_ignited = true;
	s_wasHeld = held;

	if (!held)
		s_ignited = false;
	const bool lit = s_ignited;
	s_molotov_held_weapon = weaponId;
	s_molotov_held_lit = lit ? 1 : 0;
	s_molotov_held_valid = g_molotovWickValid ? 1 : 0;
	s_molotov_held_age = g_molotovWickValid ? (now - g_molotovWickTime) : -1.0f;

	static TEMPENTITY *s_wick = nullptr;

	if (!lit)
	{
		if (s_wick)
		{
			s_wick->die = 0.0f;
			s_wick = nullptr;
		}
		return;
	}

	Vector org;
	if (g_molotovWickValid && (now - g_molotovWickTime) <= 0.08f)
	{
		org = g_molotovWickOrigin;
		s_molotov_held_captured = 1;
		if (!s_molotov_held_logged)
		{
			s_molotov_held_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: viewmodel events held-update source=captured age=%.3f advances=%i\n",
				s_molotov_held_age, s_molotov_held_advances );
		}
	}
	else
	{
		Vector angles, forward, right, up;
		gEngfuncs.GetViewAngles(angles);
		AngleVectors(angles, forward, right, up);
		org = Vector(v_origin) + forward * 16.0f + right * 5.5f + up * 1.6f;
		if (!s_molotov_held_logged)
		{
			s_molotov_held_logged = 1;
			gEngfuncs.Con_Printf(
				"CS Retro: viewmodel events held-update source=fallback age=%.3f valid=%i advances=%i\n",
				s_molotov_held_age, s_molotov_held_valid, s_molotov_held_advances );
		}
	}

	if (!s_wick)
	{
		const int sprite = gEngfuncs.pfnSPR_Load("sprites/grenade/molotov_wick.spr");
		const model_t *model = gEngfuncs.GetSpritePointer(sprite);
		if (!model)
			return;

		s_wick = gEngfuncs.pEfxAPI->CL_TempEntAllocHigh(org, (model_s *)model);
		if (!s_wick)
			s_wick = gEngfuncs.pEfxAPI->CL_TempEntAlloc(org, (model_s *)model);
		if (!s_wick)
			return;

		s_wick->flags |= (FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP | FTENT_CLIENTCUSTOM | FTENT_PERSIST);
		s_wick->flags &= ~FTENT_NOMODEL;
		s_wick->callback = nullptr;
		s_wick->entity.curstate.framerate = 16.0f;
		s_wick->entity.curstate.rendermode = kRenderTransAdd;
		s_wick->entity.curstate.renderamt = 220;
		s_wick->entity.curstate.scale = 0.04f;
		s_wick->frameMax = model->numframes > 1 ? model->numframes - 1 : 0;
	}

	s_wick->entity.origin = org;
	s_wick->die = gEngfuncs.GetClientTime() + 0.35f;
}

void EV_CreateInferno(event_args_s *args)
{
	const int mode = args->iparam1;
	const int weaponId = args->iparam2;
	if (mode == kInfernoEvStart)
		gEngfuncs.Con_DPrintf("CSRETRO_INFERNO_EVENT start weapon=%d origin=%.1f,%.1f,%.1f\n",
			weaponId, args->origin[0], args->origin[1], args->origin[2]);
	const float remaining = args->fparam1 > 0.1f ? args->fparam1 : 1.0f;
	const float lifetime = args->fparam2 > 0.1f ? args->fparam2 : remaining;

	if (mode == kInfernoEvExtinguish)
	{
		g_infernoExtinguishUntil = gEngfuncs.GetClientTime() + 0.25f;
		// Expiration/smoke affects this inferno, not every fire on the map.
		g_infernoExtinguishId = args->entindex;
		return;
	}

	SpawnInfernoSprite(args->origin, weaponId, remaining, lifetime, false, args->entindex);
	// One short ignition plume; spread nodes add only low ground fire. Spawning
	// a full-height column for every node turns the inferno into a bright wall.
	if (mode == kInfernoEvStart)
	{
		Vector impactNormal(args->angles);
		if (impactNormal.Length() < 0.1f)
			impactNormal = Vector(0, 0, 1);
		else
			impactNormal = impactNormal.Normalize();
		SpawnInfernoImpact(args->origin, impactNormal, weaponId, args->entindex);
		SpawnInfernoSprite(args->origin, weaponId, remaining, lifetime, true, args->entindex);
	}
}
