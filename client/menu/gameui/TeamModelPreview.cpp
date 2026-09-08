#include "TeamModelPreview.h"

#include "../src/menu_priv.h"

#include "cl_entity.h"
#include "const.h"
#include "entity_types.h"
#include "ref_params.h"

#include <vgui/ISurfaceNext.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

bool ReadStudioIdleBox(const char *path, float mins[3], float maxs[3])
{
	int len = 0;
	byte *raw = (path && path[0] && gEng.COM_LoadFile) ? gEng.COM_LoadFile(path, &len) : nullptr;
	if (!raw || len < 180 || std::memcmp(raw, "IDST", 4) != 0)
	{
		if (raw)
			gEng.COM_FreeFile(raw);
		return false;
	}
	std::int32_t numseq = 0;
	std::int32_t seqindex = 0;
	std::memcpy(&numseq, raw + 164, 4);
	std::memcpy(&seqindex, raw + 168, 4);
	if (numseq < 1 || seqindex < 0 || seqindex + 120 > len)
	{
		gEng.COM_FreeFile(raw);
		return false;
	}
	std::memcpy(mins, raw + seqindex + 96, 12);
	std::memcpy(maxs, raw + seqindex + 108, 12);
	gEng.COM_FreeFile(raw);
	return true;
}

// Camera looks down -Z (view pitch 90). Yaw 90 puts an X-long pancake
// onto world Y so the barrel sits horizontal on screen.
void FrameItem(const float mins[3], const float maxs[3], float *pitch, float *yaw, float *roll,
	float *frameW, float *frameH, float shift[3])
{
	const float dx = std::max(0.1f, maxs[0] - mins[0]);
	const float dy = std::max(0.1f, maxs[1] - mins[1]);
	const float cx = 0.5f * (mins[0] + maxs[0]);
	const float cy = 0.5f * (mins[1] + maxs[1]);
	const float cz = 0.5f * (mins[2] + maxs[2]);
	*pitch = 0.0f;
	*roll = 0.0f;
	if (dx > dy)
	{
		*yaw = 90.0f;
		shift[0] = cy;
		shift[1] = -cx;
		shift[2] = -cz;
	}
	else
	{
		*yaw = 0.0f;
		shift[0] = -cx;
		shift[1] = -cy;
		shift[2] = -cz;
	}
	*frameW = std::max(dx, dy);
	*frameH = std::min(dx, dy);
}

void SetupStudio(cl_entity_t *ent, const char *path, int sequence, float pitch, float yaw, float roll,
	float ox, float oy, float oz, float animStart, int index)
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
	ent->curstate.gaitsequence = 0;
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
	ent->origin[0] = ent->curstate.origin[0] = ox;
	ent->origin[1] = ent->curstate.origin[1] = oy;
	ent->origin[2] = ent->curstate.origin[2] = oz;
	ent->angles[0] = ent->curstate.angles[0] = pitch;
	ent->angles[1] = ent->curstate.angles[1] = yaw;
	ent->angles[2] = ent->curstate.angles[2] = roll;
	ent->latched.prevorigin[0] = ox;
	ent->latched.prevorigin[1] = oy;
	ent->latched.prevorigin[2] = oz;
	ent->latched.prevangles[0] = pitch;
	ent->latched.prevangles[1] = yaw;
	ent->latched.prevangles[2] = roll;
	ent->latched.prevanimtime = animStart;
	ent->latched.sequencetime = animStart;
	ent->latched.prevsequence = sequence;
	ent->latched.prevframe = 0.0f;
	ent->latched.prevseqblending[0] = 127;
	ent->latched.prevseqblending[1] = 127;
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

void CTeamModelPreview::SetItemPreview(const char *modelPath)
{
	ClearPreviews(24.0f);
	m_item = true;
	m_stageBackdrop = false;
	SetPaintBackgroundEnabled(false);
	if (!AddPreview(modelPath, nullptr, 0.0f, 0, 0.0f))
		return;
	Preview &preview = m_previews[m_count - 1];
	float mins[3] = { -12.0f, -12.0f, -2.0f };
	float maxs[3] = { 12.0f, 12.0f, 2.0f };
	if (!ReadStudioIdleBox(modelPath, mins, maxs))
		Menu_Con("CSRETRO_BUY_ITEM_BOX_FALLBACK path=%s", modelPath);
	FrameItem(mins, maxs, &preview.pitch, &preview.yaw, &preview.roll,
		&preview.frameW, &preview.frameH, preview.shift);
	m_worldWidth = preview.frameW;
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
	rvp.fov_x = m_item ? 16.0f : 26.0f;
	rvp.fov_y = FovYFromX(w, h, rvp.fov_x);
	if (rvp.fov_y <= 0.0f)
		return;

	if (m_animStart <= 0.0f && gGlobals)
		m_animStart = gGlobals->time;

	const float distH = DistanceForHeight(m_worldHeight, rvp.fov_y);
	const float distW = DistanceForHeight(m_worldWidth, rvp.fov_x);
	const float now = gGlobals ? gGlobals->time : 0.0f;
	float itemDist = 0.0f;
	if (m_item)
	{
		for (int i = 0; i < m_count; ++i)
		{
			if (!m_previews[i].visible)
				continue;
			const float byWidth = DistanceForHeight(m_previews[i].frameW / 0.92f, rvp.fov_x);
			const float byHeight = DistanceForHeight(m_previews[i].frameH / 0.82f, rvp.fov_y);
			itemDist = std::max(8.0f, std::max(byWidth, byHeight));
			break;
		}
	}
	else
		rvp.vieworigin[2] = -5.0f;

	if (m_item)
	{
		rvp.viewangles[0] = 90.0f;
		rvp.vieworigin[0] = 0.0f;
		rvp.vieworigin[1] = 0.0f;
		rvp.vieworigin[2] = itemDist;
	}

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
		const float dist = m_item ? itemDist : std::max(distH, distW) * 1.04f;
		const int studioIndex = m_independentPlayerState ? (3 - i) : (i + 1);
		const float ox = m_item ? preview.shift[0] : (dist + preview.shift[0]);
		const float oy = m_item ? preview.shift[1] : preview.lateralOffset;
		const float oz = m_item ? preview.shift[2] : idleLift;
		SetupStudio(&players[i], preview.path, preview.sequence, preview.pitch,
			preview.yaw + idleYaw, preview.roll, ox, oy, oz, m_animStart, studioIndex);
		if (!m_item)
			players[i].curstate.effects |= EF_CSRETRO_PREVIEW | EF_NOINTERP;
		if (m_item && !players[i].model)
		{
			if (!m_logged)
				Menu_Con("CSRETRO_BUY_MODEL_MISSING path=%s", preview.path);
			continue;
		}
		if (m_item)
		{
			players[i].index = 0;
			players[i].curstate.number = 0;
			players[i].curstate.effects |= EF_NOINTERP;
			if (!m_logged)
				Menu_Con("CSRETRO_BUY_ITEM path=%s yaw=%.0f frame=%.1fx%.1f dist=%.1f",
					preview.path, preview.yaw, preview.frameW, preview.frameH, dist);
		}
		// Items stay non-player. Characters keep player=true so p_* weapons
		// bone-merge. Buy's dark stage must not disable that.
		players[i].player = !m_item;
		if (preview.weapon[0])
		{
			SetupStudio(&weapons[i], preview.weapon, 0, 0.0f, preview.yaw + idleYaw, 0.0f,
				dist, preview.lateralOffset, idleLift, m_animStart, i + 1 + kMaxPreviews);
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
