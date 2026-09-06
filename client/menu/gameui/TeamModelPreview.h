#pragma once

#include <vgui_controls/Panel.h>

// Studio-Preview im Team-Slot. Spieler + knochenverschmolzenes p_-Waffenmodell.
class CTeamModelPreview : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CTeamModelPreview, vgui2::Panel);

public:
	CTeamModelPreview(vgui2::Panel *parent, const char *name);

	void SetPreview(const char *modelPath, const char *weaponPath, float yaw, int sequence);

	void Paint() override;
	void PaintBackground() override {}

private:
	char m_path[80] = {};
	char m_weapon[80] = {};
	float m_yaw = 180.0f;
	int m_sequence = 1;
	float m_animStart = 0.0f;
	bool m_logged = false;
};
