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

void FrameItem(const float mins[3], const float maxs[3], float *pitch, float *yaw, float *roll,
	float *camRoll, float *frameW, float *frameH, float shift[3])
{
	*pitch = 90.0f;
	*yaw = 0.0f;
	*roll = 0.0f;
	*camRoll = 0.0f;

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
		StudioPoint(*pitch, *yaw, in, out);
		xMin = std::min(xMin, out[0]);
		xMax = std::max(xMax, out[0]);
		yMin = std::min(yMin, out[1]);
		yMax = std::max(yMax, out[1]);
		zMin = std::min(zMin, out[2]);
		zMax = std::max(zMax, out[2]);
	}
	const float yExt = std::max(0.1f, yMax - yMin);
	const float zExt = std::max(0.1f, zMax - zMin);
	if (zExt > yExt)
	{
		*camRoll = 90.0f;
		*frameW = zExt;
		*frameH = yExt;
	}
	else
	{
		*frameW = yExt;
		*frameH = zExt;
	}
	shift[0] = -0.5f * (xMin + xMax);
	shift[1] = -0.5f * (yMin + yMax);
	shift[2] = -0.5f * (zMin + zMax);
}

float ItemScreenRoll(const char *path)
{
	// The stock w_*.mdl files do not share one authored orientation. Their
	// sequence boxes describe only an axis-aligned envelope, so they cannot tell
	// whether a pistol grip points up or how far a rifle mesh is diagonal inside
	// that envelope. These small per-asset corrections keep the real MDLs while
	// presenting the same readable, horizontal inventory profile as the reference.
	struct Roll { const char *stem; float degrees; };
	static const Roll rolls[] = {
		{"w_glock18.mdl", 180.0f}, {"w_usp.mdl", 210.0f},
		{"w_p228.mdl", 195.0f}, {"w_deagle.mdl", 180.0f},
		{"w_elite.mdl", 180.0f},
		{"w_m3.mdl", -45.0f}, {"w_xm1014.mdl", 0.0f},
		{"w_mac10.mdl", 0.0f}, {"w_mp5.mdl", 8.0f},
		{"w_ump45.mdl", 20.0f}, {"w_p90.mdl", -45.0f},
		{"w_galil.mdl", 18.0f}, {"w_ak47.mdl", -30.0f},
		{"w_scout.mdl", 25.0f}, {"w_sg552.mdl", -45.0f},
		{"w_awp.mdl", -20.0f}, {"w_g3sg1.mdl", -20.0f},
		{"w_famas.mdl", 18.0f}, {"w_m4a1.mdl", -30.0f},
		{"w_aug.mdl", -45.0f}, {"w_sg550.mdl", -20.0f},
		{"w_tmp.mdl", 0.0f}, {"w_fiveseven.mdl", 180.0f},
		{"w_flashbang.mdl", -90.0f}, {"w_hegrenade.mdl", -90.0f},
		{"w_smokegrenade.mdl", -90.0f},
	};
	for (const Roll &entry : rolls)
		if (path && std::strstr(path, entry.stem))
			return entry.degrees;
	return 0.0f;
}

void ApplyItemScreenRoll(const char *path, float *camRoll, float *frameW, float *frameH)
{
	const float correction = ItemScreenRoll(path);
	if (std::fabs(correction) < 0.01f)
		return;
	*camRoll += correction;
	// The grenade world meshes are much chunkier than weapon silhouettes. Once
	// upright, swap the projected axes and reserve extra framing so they match
	// the reference's smaller icons. Rifle/pistol corrections align their long
	// axis and deliberately keep the already measured frame instead of shrinking
	// the model a second time with a rotated axis-aligned box.
	if (path && (std::strstr(path, "flashbang") || std::strstr(path, "hegrenade") ||
		std::strstr(path, "smokegrenade")))
	{
		const float oldW = *frameW;
		*frameW = *frameH * 0.95f;
		*frameH = oldW * 0.95f;
	}
	// A few stock world models have sequence boxes far larger than their visible
	// mesh. They otherwise remain tiny despite correct centering and rotation.
	// This is camera framing only; the cards still render the original MDLs.
	float framing = 1.0f;
	if (path && std::strstr(path, "w_m3.mdl"))
		framing = 0.72f;
	else if (path && std::strstr(path, "w_sg552.mdl"))
		framing = 0.55f;
	if (framing < 1.0f)
	{
		*frameW *= framing;
		*frameH *= framing;
	}
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
	FrameItem(mins, maxs, &preview.pitch, &preview.yaw, &preview.roll, &preview.camRoll,
		&preview.frameW, &preview.frameH, preview.shift);
	ApplyItemScreenRoll(modelPath, &preview.camRoll, &preview.frameW, &preview.frameH);
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
		const float byWidth = DistanceForHeight(m_previews[0].frameW / 0.94f, rvp.fov_x);
		const float byHeight = DistanceForHeight(m_previews[0].frameH / 0.84f, rvp.fov_y);
		itemDist = std::max(8.0f, std::max(byWidth, byHeight));
		rvp.viewangles[2] = m_previews[0].camRoll;
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
			players[i].curstate.effects |= EF_CSRETRO_ITEM | EF_FULLBRIGHT | EF_NOINTERP;
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
