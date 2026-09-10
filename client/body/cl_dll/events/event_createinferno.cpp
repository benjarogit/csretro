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

Vector g_infernoExtinguishOrigin;
float g_infernoExtinguishUntil;
bool g_infernoExtinguishAll;

void InfernoThink(tempent_s *te, float frametime, float currenttime)
{
	if (g_infernoExtinguishUntil > currenttime)
	{
		if (g_infernoExtinguishAll ||
			(te->entity.origin - g_infernoExtinguishOrigin).Length() < 180.0f)
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

void SpawnInfernoSprite(const Vector &origin, int weaponId, float remaining, float lifetime, bool column)
{
	const char *path = "sprites/grenade/molotov_fire_ground.spr";
	if (weaponId == WEAPON_INCGRENADE)
		path = "sprites/grenade/incendiary_fire_ground.spr";
	if (column)
		path = "sprites/grenade/molotov_fire_column.spr";

	const int sprite = gEngfuncs.pfnSPR_Load(path);
	const model_t *model = gEngfuncs.GetSpritePointer(sprite);
	if (!model)
		return;

	Vector org = origin;
	TEMPENTITY *pTemp = gEngfuncs.pEfxAPI->CL_TempEntAlloc(org, (model_s *)model);
	if (!pTemp)
		return;

	pTemp->flags |= (FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP | FTENT_CLIENTCUSTOM | FTENT_PERSIST);
	pTemp->flags &= ~FTENT_NOMODEL;
	pTemp->callback = InfernoThink;
	const float spriteLifetime = column ? (remaining < 0.8f ? remaining : 0.8f) : remaining;
	pTemp->die = gEngfuncs.GetClientTime() + spriteLifetime;
	pTemp->frameMax = model->numframes > 1 ? model->numframes - 1 : 0;
	pTemp->entity.curstate.framerate = 12.0f;
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = column ? 190 : 150;
	// Source sprites are 176x80 (ground) and 112x232 (column). Keep the
	// vertical sheet low enough to read as floor fire instead of a fire wall.
	pTemp->entity.curstate.scale = column ? 0.35f : 0.45f;
	pTemp->entity.curstate.fuser1 = gEngfuncs.GetClientTime() + spriteLifetime;
	pTemp->entity.curstate.fuser2 = column ? 0.8f : lifetime;
	pTemp->entity.curstate.iuser1 = kInfernoMagic;
	pTemp->entity.curstate.iuser2 = weaponId;
	if (column)
		pTemp->entity.origin.z += 8.0f;
}
}

void EV_UpdateMolotovHeld()
{
	cl_entity_t *vm = gEngfuncs.GetViewModel();
	const int weaponId = HUD_GetWeapon();
	const float now = gEngfuncs.GetClientTime();
	static bool s_wasHeld = false;
	static bool s_ignited = false;
	static float s_pullStart = 0.0f;
	const bool held = vm && vm->model && weaponId == WEAPON_MOLOTOV
		&& (gHUD.m_iKeyBits & (IN_ATTACK | IN_ATTACK2));
	if (held && !s_wasHeld)
	{
		s_pullStart = now;
		s_ignited = false;
	}
	if (held && now - s_pullStart >= kMolotovWickDelay)
		s_ignited = true;
	s_wasHeld = held;

	// ItemPostFrame switches to the idle sequence while attack remains held.
	// The input state is the reliable lifecycle here: view-model sequences may
	// already have advanced by the time HUD_CreateEntities runs.
	if (!held)
		s_ignited = false;
	const bool lit = s_ignited;

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

	// View-model attachments are filled by the studio draw, after
	// HUD_CreateEntities. Reading them here returns the previous frame in a
	// different transform and can put the flame meters away. Anchor the wick
	// in view space at the bottle neck instead.
	Vector angles, forward, right, up;
	gEngfuncs.GetViewAngles(angles);
	AngleVectors(angles, forward, right, up);
	Vector org = Vector(v_origin) + forward * 20.0f + right * 8.0f + up * 1.1f;

	if (!s_wick)
	{
		const int sprite = gEngfuncs.pfnSPR_Load("sprites/grenade/molotov_wick.spr");
		const model_t *model = gEngfuncs.GetSpritePointer(sprite);
		if (!model)
			return;

		s_wick = gEngfuncs.pEfxAPI->CL_TempEntAlloc(org, (model_s *)model);
		if (!s_wick)
			return;

		s_wick->flags |= (FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP | FTENT_CLIENTCUSTOM | FTENT_PERSIST);
		s_wick->flags &= ~FTENT_NOMODEL;
		s_wick->callback = nullptr;
		s_wick->entity.curstate.framerate = 16.0f;
		s_wick->entity.curstate.rendermode = kRenderTransAdd;
		s_wick->entity.curstate.renderamt = 220;
		s_wick->entity.curstate.scale = 0.014f;
		s_wick->frameMax = model->numframes > 1 ? model->numframes - 1 : 0;
	}

	s_wick->entity.origin = org;
	s_wick->die = gEngfuncs.GetClientTime() + 0.35f;
}

void EV_CreateInferno(event_args_s *args)
{
	const int mode = args->iparam1;
	const int weaponId = args->iparam2;
	const float remaining = args->fparam1 > 0.1f ? args->fparam1 : 1.0f;
	const float lifetime = args->fparam2 > 0.1f ? args->fparam2 : remaining;

	if (mode == kInfernoEvExtinguish)
	{
		g_infernoExtinguishOrigin = args->origin;
		g_infernoExtinguishUntil = gEngfuncs.GetClientTime() + 0.25f;
		g_infernoExtinguishAll = true;
		return;
	}

	g_infernoExtinguishAll = false;
	SpawnInfernoSprite(args->origin, weaponId, remaining, lifetime, false);
	// One short ignition plume; spread nodes add only low ground fire. Spawning
	// a full-height column for every node turns the inferno into a bright wall.
	if (mode == kInfernoEvStart)
		SpawnInfernoSprite(args->origin, weaponId, remaining, lifetime, true);
}
