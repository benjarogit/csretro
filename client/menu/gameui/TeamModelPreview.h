#pragma once

#include <vgui_controls/Panel.h>

// Studio-Preview im Team-Slot. Spieler + knochenverschmolzenes p_-Waffenmodell.
class CTeamModelPreview : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CTeamModelPreview, vgui2::Panel);

public:
	CTeamModelPreview(vgui2::Panel *parent, const char *name);

	void SetPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence);
	void SetItemPreview(const char *modelPath, float worldWidth = 24.0f);
	void SetStageBackdrop(bool enabled);
	void ClearPreviews(float worldWidth);
	void SetWorldWidth(float worldWidth) { m_worldWidth = worldWidth > 1.0f ? worldWidth : 1.0f; }
	void SetWorldHeight(float worldHeight) { m_worldHeight = worldHeight > 1.0f ? worldHeight : 1.0f; }
	void SetIndependentPlayerState(bool enabled) { m_independentPlayerState = enabled; }
	bool AddPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence, float lateralOffset);
	void SetPreviewVisible(int index, bool visible);
	int PreviewCount() const { return m_count; }

	void Paint() override;
	void PaintBackground() override;

private:
	enum { kMaxPreviews = 6 };
	struct Preview
	{
		char path[80] = {};
		char weapon[80] = {};
		float yaw = 180.0f;
		float lateralOffset = 0.0f;
		int sequence = 1;
		bool visible = true;
	};
	Preview m_previews[kMaxPreviews];
	int m_count = 0;
	float m_worldWidth = 50.0f;
	float m_worldHeight = 82.0f;
	float m_animStart = 0.0f;
	bool m_logged = false;
	bool m_item = false;
	bool m_stageBackdrop = false;
	bool m_independentPlayerState = false;
};
