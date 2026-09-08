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

// Same yaw/pitch/roll matrix as the studio path, including the Quake pitch flip
// CS 1.6 leaves enabled (ENGINE_COMPENSATE_QUAKE_BUG is off).
void EntityAxes(float pitch, float yaw, float roll, float axis[3][3])
{
	pitch = -pitch;
	const float sy = std::sin(Deg2Rad(yaw));
	const float cy = std::cos(Deg2Rad(yaw));
	const float sp = std::sin(Deg2Rad(pitch));
	const float cp = std::cos(Deg2Rad(pitch));
	const float sr = std::sin(Deg2Rad(roll));
	const float cr = std::cos(Deg2Rad(roll));
	axis[0][0] = cp * cy;
	axis[0][1] = sr * sp * cy + cr * -sy;
	axis[0][2] = cr * sp * cy + -sr * -sy;
	axis[1][0] = cp * sy;
	axis[1][1] = sr * sp * sy + cr * cy;
	axis[1][2] = cr * sp * sy + -sr * cy;
	axis[2][0] = -sp;
	axis[2][1] = sr * cp;
	axis[2][2] = cr * cp;
}

void RotatePoint(const float axis[3][3], const float in[3], float out[3])
{
	out[0] = axis[0][0] * in[0] + axis[0][1] * in[1] + axis[0][2] * in[2];
	out[1] = axis[1][0] * in[0] + axis[1][1] * in[1] + axis[1][2] * in[2];
	out[2] = axis[2][0] * in[0] + axis[2][1] * in[1] + axis[2][2] * in[2];
}

// Class/Team camera looks along +X. Pancake w_*.mdl (rifles, vests) need their
// large XY face in the YZ picture plane, longest axis horizontal. Chunky
// meshes (grenades) stand on the long axis so they are not a lying rectangle.
void FrameItem(const float mins[3], const float maxs[3], float *pitch, float *yaw, float *roll,
	float *frameW, float *frameH, float shift[3])
{
	const float size[3] = {
		std::max(0.1f, maxs[0] - mins[0]),
		std::max(0.1f, maxs[1] - mins[1]),
		std::max(0.1f, maxs[2] - mins[2])
	};
	int order[3] = { 0, 1, 2 };
	for (int i = 0; i < 2; ++i)
	{
		for (int j = i + 1; j < 3; ++j)
		{
			if (size[order[j]] > size[order[i]])
				std::swap(order[i], order[j]);
		}
	}
	const int lng = order[0];
	const int mid = order[1];
	const int thin = order[2];
	const bool pancake = size[thin] < size[mid] * 0.40f;

	float p = 0.0f, y = 0.0f, r = 0.0f;
	if (pancake)
	{
		if (lng == 1)
			p = 90.0f;
		else if (lng == 0)
		{
			y = 90.0f;
			r = 90.0f;
		}
		else
			r = 90.0f;
	}
	else if (lng == 1)
		r = 90.0f;
	else if (lng == 0)
		p = 90.0f;

	float axis[3][3];
	EntityAxes(p, y, r, axis);

	const float center[3] = {
		0.5f * (mins[0] + maxs[0]),
		0.5f * (mins[1] + maxs[1]),
		0.5f * (mins[2] + maxs[2])
	};
	float worldCenter[3];
	RotatePoint(axis, center, worldCenter);

	float minY = 1.0e9f, maxY = -1.0e9f, minZ = 1.0e9f, maxZ = -1.0e9f;
	for (int i = 0; i < 8; ++i)
	{
		const float corner[3] = {
			(i & 1) ? maxs[0] : mins[0],
			(i & 2) ? maxs[1] : mins[1],
			(i & 4) ? maxs[2] : mins[2]
		};
		float world[3];
		RotatePoint(axis, corner, world);
		minY = std::min(minY, world[1]);
		maxY = std::max(maxY, world[1]);
		minZ = std::min(minZ, world[2]);
		maxZ = std::max(maxZ, world[2]);
	}

	*pitch = p;
	*yaw = y;
	*roll = r;
	*frameW = std::max(4.0f, maxY - minY);
	*frameH = std::max(4.0f, maxZ - minZ);
	shift[0] = -worldCenter[0];
	shift[1] = -worldCenter[1];
	shift[2] = -worldCenter[2];
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
	rvp.fov_x = m_item ? 12.0f : 26.0f;
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
		const float ox = dist + preview.shift[0];
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
			players[i].curstate.effects |= EF_CSRETRO_ITEM;
			players[i].curstate.rendermode = kRenderNormal;
			players[i].curstate.renderamt = 255;
			players[i].curstate.rendercolor.r = 255;
			players[i].curstate.rendercolor.g = 196;
			players[i].curstate.rendercolor.b = 48;
			if (!m_logged)
				Menu_Con("CSRETRO_BUY_ITEM path=%s pitch=%.0f yaw=%.0f roll=%.0f frame=%.1fx%.1f dist=%.1f",
					preview.path, preview.pitch, preview.yaw, preview.roll, preview.frameW,
					preview.frameH, dist);
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
