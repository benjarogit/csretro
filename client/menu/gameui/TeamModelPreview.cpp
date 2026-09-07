#include "TeamModelPreview.h"

#include "../src/menu_priv.h"

#include "cl_entity.h"
#include "const.h"
#include "entity_types.h"
#include "ref_params.h"

#include <vgui/ISurfaceNext.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace vgui2;

namespace
{
// Stable leading part of Xash model_t through radius. Keeping this local
// avoids importing the engine math headers into the Source-style VGUI build.
struct PreviewModelBounds
{
	char name[64];
	int needload;
	int type;
	int numframes;
	std::uint32_t mempool;
	int flags;
	float mins[3];
	float maxs[3];
	float radius;
};

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
	m_item = false;
	m_stageBackdrop = false;
	SetPaintBackgroundEnabled(false);
	ClearPreviews(50.0f);
	AddPreview(modelPath, weaponPath, yaw, sequence, 0.0f);
}

void CTeamModelPreview::SetItemPreview(const char *modelPath, float worldWidth)
{
	ClearPreviews(worldWidth > 1.0f ? worldWidth : 24.0f);
	m_item = true;
	m_stageBackdrop = false;
	SetPaintBackgroundEnabled(false);
	AddPreview(modelPath, nullptr, 18.0f, 0, 0.0f);
}

void CTeamModelPreview::SetStageBackdrop(bool enabled)
{
	m_stageBackdrop = enabled;
	SetPaintBackgroundEnabled(enabled);
}

void CTeamModelPreview::PaintBackground()
{
	if (!m_stageBackdrop)
		return;
	int w = 0, h = 0;
	GetSize(w, h);
	if (!surface() || w < 1 || h < 1)
		return;
	surface()->DrawSetColor(6, 8, 10, 220);
	surface()->DrawFilledRect(0, 0, w, h);
}

void CTeamModelPreview::ClearPreviews(float worldWidth)
{
	m_count = 0;
	m_worldWidth = std::max(worldWidth, 1.0f);
	m_animStart = 0.0f;
	m_logged = false;
}

bool CTeamModelPreview::AddPreview(const char *modelPath, const char *weaponPath, float yaw,
	int sequence, float lateralOffset)
{
	if (!modelPath || !modelPath[0] || m_count >= kMaxPreviews)
		return false;
	Preview &preview = m_previews[m_count++];
	preview = Preview{};
	snprintf(preview.path, sizeof(preview.path), "%s", modelPath);
	if (weaponPath)
		snprintf(preview.weapon, sizeof(preview.weapon), "%s", weaponPath);
	preview.yaw = yaw;
	preview.sequence = sequence;
	preview.lateralOffset = lateralOffset;
	preview.visible = true;
	m_logged = false;
	return true;
}

void CTeamModelPreview::SetPreviewVisible(int index, bool visible)
{
	if (index >= 0 && index < m_count)
		m_previews[index].visible = visible;
}

void CTeamModelPreview::Paint()
{
	int w = 0, h = 0;
	GetSize(w, h);
	if (w < 8 || h < 8)
		return;

	int ax = 0, ay = 0;
	LocalToScreen(ax, ay);
	ref_viewpass_t rvp;
	memset(&rvp, 0, sizeof(rvp));
	rvp.viewport[0] = ax;
	// ref_viewpass_t uses top-left screen coordinates; the GL backend performs
	// the bottom-left conversion when installing the actual GL viewport.
	rvp.viewport[1] = ay;
	rvp.viewport[2] = w;
	rvp.viewport[3] = h;
	rvp.fov_x = m_item ? 32.0f : 26.0f;
	rvp.fov_y = FovYFromX(w, h, rvp.fov_x);
	if (rvp.fov_y <= 0.0f)
		return;
	if (!m_item)
		rvp.vieworigin[2] = -5.0f;

	if (m_animStart <= 0.0f && gGlobals)
		m_animStart = gGlobals->time;

	const float distH = DistanceForHeight(m_worldHeight, rvp.fov_y);
	const float distW = DistanceForHeight(m_worldWidth, rvp.fov_x);
	const float dist = std::max(distH, distW) * 1.04f;
	const float now = gGlobals ? gGlobals->time : 0.0f;
	// Rifle aim references are intentionally almost static.  Keep the authored
	// pose and add only a restrained showroom idle, applied once to player and
	// bone-merged weapon so it costs no additional model or texture.
	gEng.pfnClearScene();
	cl_entity_t players[kMaxPreviews];
	cl_entity_t weapons[kMaxPreviews];
	int playerAdded[kMaxPreviews] = {};
	int weaponIndex[kMaxPreviews] = {};
	for (int i = 0; i < m_count; ++i)
	{
		const Preview &preview = m_previews[i];
		if (!preview.visible)
			continue;
		const float phase = static_cast<float>(i) * 0.73f;
		const float idleYaw = m_item ? 0.0f : std::sin(now * 0.85f + phase) * 1.25f;
		const float idleLift = m_item ? 0.0f : std::sin(now * 1.35f + phase) * 0.22f;
		const float place = m_item ? 0.0f : dist;
		SetupStudio(&players[i], preview.path, preview.sequence, preview.yaw + idleYaw,
			place, m_animStart, i + 1);
		if (m_item && !players[i].model)
		{
			if (!m_logged)
				Menu_Con("CSRETRO_BUY_MODEL_MISSING path=%s", preview.path);
			continue;
		}
		if (m_item)
		{
			// World items use the familiar elevated GoldSrc inventory view. Frame
			// that view from the engine-computed model bounds: the old fixed camera
			// made long rifles overflow, while a level camera showed pistols end-on.
			const PreviewModelBounds *model =
				reinterpret_cast<const PreviewModelBounds *>(players[i].model);
			const float spanX = std::max(1.0f, model->maxs[0] - model->mins[0]);
			const float spanY = std::max(1.0f, model->maxs[1] - model->mins[1]);
			const float spanZ = std::max(1.0f, model->maxs[2] - model->mins[2]);
			const float centerX = (model->mins[0] + model->maxs[0]) * 0.5f;
			const float centerY = (model->mins[1] + model->maxs[1]) * 0.5f;
			const float centerZ = (model->mins[2] + model->maxs[2]) * 0.5f;
			const float itemSize = std::max(spanX, std::max(spanY, spanZ)) * 1.08f;
			const float itemDist = std::max(DistanceForHeight(itemSize, rvp.fov_y),
				DistanceForHeight(itemSize, rvp.fov_x));
			const float pitch = 50.0f;
			rvp.viewangles[0] = pitch;
			rvp.vieworigin[0] = centerX - itemDist * std::cos(Deg2Rad(pitch));
			rvp.vieworigin[1] = centerY;
			rvp.vieworigin[2] = centerZ + itemDist * std::sin(Deg2Rad(pitch));
		}
		// Items stay non-player. Characters keep player=true so p_* weapons
		// bone-merge. Buy's dark stage must not disable that.
		players[i].player = !m_item;
		players[i].origin[1] = players[i].curstate.origin[1] = preview.lateralOffset;
		players[i].origin[2] = players[i].curstate.origin[2] = idleLift;
		if (preview.weapon[0])
		{
			SetupStudio(&weapons[i], preview.weapon, 0, preview.yaw + idleYaw,
				dist, m_animStart, i + 1 + kMaxPreviews);
			if (weapons[i].curstate.modelindex > 0)
				weaponIndex[i] = weapons[i].curstate.modelindex;
		}
		players[i].curstate.weaponmodel = weaponIndex[i];
		playerAdded[i] = gEng.CL_CreateVisibleEntity(ET_NORMAL, &players[i]);
	}

	gEng.pfnRenderScene(&rvp);

	if (!m_logged)
	{
		for (int i = 0; i < m_count; ++i)
		{
			const Preview &preview = m_previews[i];
			if (!preview.visible)
				continue;
			Menu_Con("CSRETRO_TEAM_MODEL path=%s weapon=%s player=%d weapon_index=%d seq=%d yaw=%.0f scene=%d",
				preview.path, preview.weapon[0] ? preview.weapon : "-", playerAdded[i],
				weaponIndex[i], preview.sequence, preview.yaw, m_count);
		}
		m_logged = true;
	}
}
