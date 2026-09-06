#include "TeamModelPreview.h"

#include "../src/menu_priv.h"

#include "cl_entity.h"
#include "entity_types.h"
#include "ref_params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace vgui2;

namespace
{
float Deg2Rad(float deg)
{
	return deg * static_cast<float>(M_PI) / 180.0f;
}

float Rad2Deg(float rad)
{
	return rad * 180.0f / static_cast<float>(M_PI);
}

float FovYFromX(int vw, int vh, float fovX)
{
	if (vw < 1 || vh < 1 || fovX <= 0.0f)
		return 0.0f;
	const float x = static_cast<float>(vw) / std::tan(Deg2Rad(fovX) * 0.5f);
	return Rad2Deg(std::atan(static_cast<float>(vh) / x)) * 2.0f;
}

float DistanceForHeight(float worldH, float fovY)
{
	if (fovY <= 0.0f)
		return 120.0f;
	return worldH * 0.5f / std::tan(Deg2Rad(fovY) * 0.5f);
}

void SetupStudio(cl_entity_t *ent, const char *path, int sequence, float yaw, float dist, float animStart, int index)
{
	memset(ent, 0, sizeof(*ent));
	gEng.pfnSetModel(ent, path);
	ent->index = index;
	ent->player = false;
	ent->curstate.number = index;
	ent->curstate.body = 0;
	ent->curstate.animtime = animStart;
	ent->curstate.sequence = sequence;
	ent->curstate.scale = 1.0f;
	ent->curstate.frame = 0.0f;
	ent->curstate.framerate = 1.0f;
	ent->curstate.effects |= EF_FULLBRIGHT;
	ent->curstate.rendermode = kRenderNormal;
	ent->curstate.renderamt = 255;
	for (int i = 0; i < 4; ++i)
	{
		ent->curstate.controller[i] = 127;
		ent->latched.prevcontroller[i] = 127;
	}
	// The CS rifle reference sequences are nine-way pitch blends.  Zero is
	// their upper edge (the model looks and aims into the air); 127 is the
	// neutral, camera-facing pose used by this preview.
	for (int i = 0; i < 2; ++i)
	{
		ent->curstate.blending[i] = 127;
		ent->latched.prevblending[i] = 127;
	}
	ent->origin[0] = ent->curstate.origin[0] = dist;
	ent->origin[2] = ent->curstate.origin[2] = 0.0f;
	ent->angles[1] = ent->curstate.angles[1] = yaw;
}
} // namespace

CTeamModelPreview::CTeamModelPreview(Panel *parent, const char *name)
	: BaseClass(parent, name)
{
	SetPaintBackgroundEnabled(false);
	SetPaintBorderEnabled(false);
	SetMouseInputEnabled(false);
	SetKeyBoardInputEnabled(false);
}

void CTeamModelPreview::SetPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence)
{
	m_path[0] = '\0';
	m_weapon[0] = '\0';
	if (modelPath)
		snprintf(m_path, sizeof(m_path), "%s", modelPath);
	if (weaponPath)
		snprintf(m_weapon, sizeof(m_weapon), "%s", weaponPath);
	m_yaw = yaw;
	m_sequence = sequence;
	m_animStart = 0.0f;
	m_logged = false;
}

void CTeamModelPreview::Paint()
{
	int w = 0, h = 0;
	GetSize(w, h);
	if (w < 16 || h < 16)
		return;

	int ax = 0, ay = 0;
	LocalToScreen(ax, ay);

	ref_viewpass_t rvp;
	memset(&rvp, 0, sizeof(rvp));
	rvp.viewport[0] = ax;
	rvp.viewport[1] = ay;
	rvp.viewport[2] = w;
	rvp.viewport[3] = h;
	rvp.fov_x = 26.0f;
	rvp.fov_y = FovYFromX(w, h, rvp.fov_x);
	if (rvp.fov_y <= 0.0f)
		return;
	rvp.vieworigin[2] = -5.0f;

	if (m_animStart <= 0.0f && gGlobals)
		m_animStart = gGlobals->time;

	const float distH = DistanceForHeight(82.0f, rvp.fov_y);
	const float distW = DistanceForHeight(50.0f, rvp.fov_x);
	const float dist = std::max(distH, distW) * 1.04f;
	const float now = gGlobals ? gGlobals->time : 0.0f;
	// Rifle aim references are intentionally almost static.  Keep the authored
	// pose and add only a restrained showroom idle, applied once to player and
	// bone-merged weapon so it costs no additional model or texture.
	const float idleYaw = std::sin(now * 0.85f) * 1.25f;
	const float idleLift = std::sin(now * 1.35f) * 0.22f;

	cl_entity_t *ent = gEng.pfnGetPlayerModel();
	gEng.pfnClearScene();
	SetupStudio(ent, m_path, m_sequence, m_yaw + idleYaw, dist, m_animStart, 1);
	ent->player = true;
	ent->origin[2] = ent->curstate.origin[2] = idleLift;

	cl_entity_t weapon;
	int weaponIndex = 0;
	if (m_weapon[0])
	{
		SetupStudio(&weapon, m_weapon, 0, m_yaw + idleYaw, dist, m_animStart, 2);
		if (weapon.curstate.modelindex > 0)
			weaponIndex = weapon.curstate.modelindex;
	}
	ent->curstate.weaponmodel = weaponIndex;
	const int playerAdded = gEng.CL_CreateVisibleEntity(ET_NORMAL, ent);

	gEng.pfnRenderScene(&rvp);

	if (!m_logged)
	{
		Menu_Con("CSRETRO_TEAM_MODEL path=%s weapon=%s player=%d weapon_index=%d seq=%d yaw=%.0f", m_path,
			m_weapon[0] ? m_weapon : "-", playerAdded, weaponIndex, m_sequence, m_yaw);
		m_logged = true;
	}
}
