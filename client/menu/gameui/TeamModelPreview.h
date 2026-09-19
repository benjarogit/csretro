#pragma once

#include <vgui_controls/Panel.h>

// Studio-Preview im Team-Slot. Spieler + knochenverschmolzenes p_-Waffenmodell.
class CTeamModelPreview : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CTeamModelPreview, vgui2::Panel);

public:
	CTeamModelPreview(vgui2::Panel *parent, const char *name);

	void SetPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence,
		float lateralOffset = 0.0f);
	void SetItemPreview(const char *modelPath);
	void SetItemSlotScale(float scale);
	void SetStageBackdrop(bool enabled);
	void ClearPreviews(float worldWidth);
	void SetWorldWidth(float worldWidth) { m_worldWidth = worldWidth > 1.0f ? worldWidth : 1.0f; }
	void SetWorldHeight(float worldHeight) { m_worldHeight = worldHeight > 1.0f ? worldHeight : 1.0f; }
	void SetCameraHeight(float height) { m_cameraHeight = height; }
	void SetIndependentPlayerState(bool enabled) { m_independentPlayerState = enabled; }
	bool AddPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence, float lateralOffset);
	void SetPreviewVisible(int index, bool visible);
	int PreviewCount() const { return m_count; }
	void GetItemFrame(float *frameW, float *frameH) const;

	void Paint() override;
	void PaintBackground() override;

private:
	enum { kMaxPreviews = 6 };
	struct Preview
	{
		char path[80] = {};
		char weapon[80] = {};
		float pitch = 0.0f;
		float yaw = 180.0f;
		float roll = 0.0f;
		float camRoll = 0.0f;
		float lateralOffset = 0.0f;
		float shift[3] = {};
		float frameW = 24.0f;
		float frameH = 24.0f;
		float itemFill = 0.88f;
		float itemZoom = 1.0f;
		int sequence = 1;
		bool itemContain = false;
		bool visible = true;
	};
	Preview m_previews[kMaxPreviews];
	int m_count = 0;
	float m_worldWidth = 50.0f;
	float m_worldHeight = 82.0f;
	float m_cameraHeight = 6.0f;
	float m_animStart = 0.0f;
	bool m_logged = false;
	bool m_item = false;
	float m_itemSlotScale = 1.0f;
	bool m_stageBackdrop = false;
	bool m_independentPlayerState = false;
};
