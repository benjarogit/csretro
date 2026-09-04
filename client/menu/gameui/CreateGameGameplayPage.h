#pragma once

#include <vgui_controls/PropertyPage.h>

#include "ServerSettingsScript.h"

class CCreateGameSettingsList;

// Dünne Registerkarte: hält eine CreateGameSettingsList für eine ScrGroup
// (Rules oder Fairness). Reset/Apply/Gate laufen nur über die Liste.
class CCreateGameGameplayPage : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCreateGameGameplayPage, vgui2::PropertyPage);

public:
	CCreateGameGameplayPage(vgui2::Panel *parent, csretro::ScrGroup group);

	void OnResetData() override;
	void OnApplyChanges() override;
	void PerformLayout() override;

	bool HasOptions() const;
	int Gate_OptionCount() const;
	bool Gate_SetValue(const char *cvar, const char *value);
	bool Gate_AuditLabels();

private:
	CCreateGameSettingsList *m_pList = nullptr;
};
