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

// Camera looks +X (same as Class/Team). Studio pitch is Quake-inverted, so
// entity pitch 90 puts the idle XY pancake into the YZ plane — a side profile
// with the barrel in the picture. Pitch-90 cameras gimbal and look 3/4.
// Portrait pancakes get a camera roll so the barrel stays horizontal.
void StudioPoint(float pitch, float yaw, const float in[3], float out[3])
{
	const float p = Deg2Rad(-pitch);
	const float y = Deg2Rad(yaw);
	const float sp = std::sin(p);
	const float cp = std::cos(p);
	const float sy = std::sin(y);
	const float cy = std::cos(y);
	out[0] = (cp * cy) * in[0] + (-sy) * in[1] + (sp * cy) * in[2];
	out[1] = (cp * sy) * in[0] + (cy) * in[1] + (sp * sy) * in[2];
	out[2] = (-sp) * in[0] + (cp) * in[2];
}

bool PathHasStem(const char *path, const char *stem)
{
	return path && stem && std::strstr(path, stem);
}

// One row per w_*.mdl. fill = card occupancy, zoom = closer if > 1.
// contain = fit both axes (nades/gear). Guns use width only.
struct BuyItemLook
{
	const char *stem;
	float roll;
	float fill;
	float zoom;
	bool upright;
	bool contain;
};

const BuyItemLook *LookupBuyItem(const char *path)
{
	static const BuyItemLook kLooks[] = {
		// Equipment — width-fit. Contain on a 2.20 card kept the vest a postage stamp.
		{"w_kevlar.mdl", 0.0f, 0.74f, 1.00f, false, false},
		{"w_assault.mdl", 0.0f, 0.74f, 1.00f, false, false},
		{"w_thighpack.mdl", 0.0f, 0.70f, 1.00f, false, true},
		// Glock side profile is already correct; 0.50 left it half the USP.
		{"w_glock18.mdl", 180.0f, 0.72f, 1.00f, false, false},
		{"w_usp.mdl", 210.0f, 0.74f, 1.08f, false, false},
		{"w_p228.mdl", 195.0f, 0.76f, 1.00f, false, false},
		{"w_deagle.mdl", 180.0f, 0.78f, 1.00f, false, false},
		{"w_elite.mdl", 180.0f, 0.82f, 1.00f, false, false},
		{"w_fiveseven.mdl", 180.0f, 0.74f, 1.00f, false, false},
		// Mid — MAC-10 side profile; others already readable.
		{"w_mac10.mdl", 180.0f, 0.84f, 1.00f, false, true},
		{"w_mp5.mdl", 8.0f, 0.88f, 1.00f, false, false},
		{"w_ump45.mdl", 200.0f, 0.88f, 1.00f, false, false},
		{"w_p90.mdl", 135.0f, 0.88f, 1.00f, false, false},
		{"w_tmp.mdl", 0.0f, 0.86f, 1.00f, false, false},
		{"w_m3.mdl", 135.0f, 0.88f, 1.00f, false, false},
		{"w_xm1014.mdl", 0.0f, 0.90f, 1.00f, false, false},
		{"w_m249.mdl", 180.0f, 0.88f, 1.00f, false, false},
		// Rifles — Benny rolls stay.
		{"w_galil.mdl", 198.0f, 0.88f, 1.18f, false, false},
		{"w_ak47.mdl", 160.0f, 0.90f, 1.00f, false, false},
		{"w_scout.mdl", 205.0f, 0.90f, 1.00f, false, false},
		{"w_awp.mdl", 160.0f, 0.90f, 1.00f, false, false},
		{"w_g3sg1.mdl", 160.0f, 0.90f, 1.00f, false, false},
		{"w_sg552.mdl", 135.0f, 0.90f, 1.00f, false, false},
		{"w_famas.mdl", 18.0f, 0.88f, 1.00f, false, false},
		{"w_m4a1.mdl", -30.0f, 0.88f, 1.00f, false, false},
		{"w_aug.mdl", -45.0f, 0.88f, 1.00f, false, false},
		{"w_sg550.mdl", -20.0f, 0.88f, 1.00f, false, false},
		// Nades — each can/bottle, not one shared close-up.
		{"w_flashbang.mdl", 0.0f, 0.56f, 1.00f, true, true},
		{"w_hegrenade.mdl", 25.0f, 0.58f, 1.00f, true, true},
		{"w_smokegrenade.mdl", 0.0f, 0.54f, 1.00f, true, true},
		{"w_molotov.mdl", 0.0f, 0.52f, 1.00f, true, true},
		{"w_incgrenade.mdl", 0.0f, 0.50f, 1.00f, true, true},
	};
	for (const BuyItemLook &look : kLooks)
	{
		if (PathHasStem(path, look.stem))
			return &look;
	}
	return nullptr;
}

void MeasureItem(const float mins[3], const float maxs[3], float pitch, float yaw, float camRoll,
	float *frameW, float *frameH, float shift[3])
{
	const float cr = Deg2Rad(camRoll);
	const float c = std::cos(cr);
	const float s = std::sin(cr);
	float xMin = 1.0e9f, xMax = -1.0e9f;
	float yMin = 1.0e9f, yMax = -1.0e9f;
	float zMin = 1.0e9f, zMax = -1.0e9f;
	for (int i = 0; i < 8; ++i)
	{
		const float in[3] = {
			(i & 1) ? maxs[0] : mins[0],
			(i & 2) ? maxs[1] : mins[1],
			(i & 4) ? maxs[2] : mins[2]
		};
		float out[3];
		StudioPoint(pitch, yaw, in, out);
		const float y = out[1] * c - out[2] * s;
		const float z = out[1] * s + out[2] * c;
		xMin = std::min(xMin, out[0]);
		xMax = std::max(xMax, out[0]);
		yMin = std::min(yMin, y);
		yMax = std::max(yMax, y);
		zMin = std::min(zMin, z);
		zMax = std::max(zMax, z);
	}
	*frameW = std::max(0.1f, yMax - yMin);
	*frameH = std::max(0.1f, zMax - zMin);
	shift[0] = -0.5f * (xMin + xMax);
	shift[1] = -0.5f * (yMin + yMax);
	shift[2] = -0.5f * (zMin + zMax);
}

void FrameItem(const float mins[3], const float maxs[3], float *pitch, float *yaw, float *roll,
	float *camRoll, float *frameW, float *frameH, float shift[3], bool upright, float extraRoll)
{
	*pitch = upright ? 0.0f : 90.0f;
	*yaw = 0.0f;
	*roll = 0.0f;
	float yExt = 1.0f, zExt = 1.0f;
	MeasureItem(mins, maxs, *pitch, *yaw, 0.0f, &yExt, &zExt, shift);
	// Longest silhouette axis stays horizontal. Extra per-weapon roll is
	// included in the measured frame so distance matches what the camera sees.
	const float baseRoll = (!upright && zExt > yExt) ? 90.0f : 0.0f;
	*camRoll = baseRoll + extraRoll;
	float rolledShift[3] = {};
	MeasureItem(mins, maxs, *pitch, *yaw, *camRoll, frameW, frameH, rolledShift);
	(void)rolledShift;
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
	ent->curstate.framerate = 0.0f;
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

void CTeamModelPreview::SetPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence,
	float lateralOffset)
{
	m_item = false;
	m_stageBackdrop = false;
	SetPaintBackgroundEnabled(false);
	ClearPreviews(50.0f);
	AddPreview(modelPath, weaponPath, yaw, sequence, lateralOffset);
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
	const BuyItemLook *look = LookupBuyItem(modelPath);
	const bool upright = look && look->upright;
	preview.itemFill = look ? look->fill : 0.88f;
	preview.itemZoom = look ? look->zoom : 1.0f;
	preview.itemContain = look && look->contain;
	FrameItem(mins, maxs, &preview.pitch, &preview.yaw, &preview.roll, &preview.camRoll,
		&preview.frameW, &preview.frameH, preview.shift, upright, look ? look->roll : 0.0f);
	m_worldWidth = preview.frameW;
	m_itemSlotScale = 1.0f;
}

void CTeamModelPreview::SetItemSlotScale(float scale)
{
	m_itemSlotScale = std::max(0.35f, std::min(1.75f, scale));
}

void CTeamModelPreview::GetItemFrame(float *frameW, float *frameH) const
{
	const float w = (m_item && m_count > 0) ? m_previews[0].frameW : 24.0f;
	const float h = (m_item && m_count > 0) ? m_previews[0].frameH : 24.0f;
	if (frameW)
		*frameW = std::max(0.1f, w);
	if (frameH)
		*frameH = std::max(0.1f, h);
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
	rvp.fov_x = m_item ? 13.0f : 26.0f;
	rvp.fov_y = FovYFromX(w, h, rvp.fov_x);
	if (rvp.fov_y <= 0.0f)
		return;

	if (m_animStart <= 0.0f && gGlobals)
		m_animStart = gGlobals->time;

	const float distH = DistanceForHeight(m_worldHeight, rvp.fov_y);
	const float distW = DistanceForHeight(m_worldWidth, rvp.fov_x);
	const float now = gGlobals ? gGlobals->time : 0.0f;
	float itemDist = 24.0f;
	if (m_item && m_count > 0)
	{
		const Preview &item = m_previews[0];
		const float fill = std::max(0.18f, item.itemFill);
		if (item.itemContain)
		{
			itemDist = std::max(
				DistanceForHeight(item.frameW / fill, rvp.fov_x),
				DistanceForHeight(item.frameH / fill, rvp.fov_y));
		}
		else
			itemDist = DistanceForHeight(item.frameW / fill, rvp.fov_x);
		itemDist /= std::max(0.35f, item.itemZoom);
		itemDist = std::max(8.0f, itemDist);
		rvp.viewangles[2] = item.camRoll;
	}
	else
	{
		rvp.vieworigin[0] = 0.0f;
		rvp.vieworigin[1] = 0.0f;
		rvp.vieworigin[2] = m_cameraHeight;
		rvp.viewangles[0] = 0.0f;
		rvp.viewangles[1] = 0.0f;
		rvp.viewangles[2] = 0.0f;
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
		// Indices 8+ collide with networked players (slot 8 is often a bot:
		// Phoenix caption + Arctic mesh). Stay past maxclients so
		// SetupPlayerModel cannot substitute another player's skin.
		const int studioIndex = m_independentPlayerState ? (64 + i) : (i + 1);
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
			players[i].curstate.scale = 1.0f;
			players[i].curstate.effects |= EF_FULLBRIGHT | EF_NOINTERP;
			if (!m_logged)
				Menu_Con("CSRETRO_BUY_ITEM path=%s pitch=%.0f yaw=%.0f camRoll=%.0f frame=%.1fx%.1f dist=%.1f",
					preview.path, preview.pitch, preview.yaw, preview.camRoll,
					preview.frameW, preview.frameH, dist);
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
