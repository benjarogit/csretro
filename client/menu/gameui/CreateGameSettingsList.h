#pragma once

#include <vgui_controls/Panel.h>

#include <string>
#include <vector>

#include "ServerSettingsScript.h"

namespace vgui2
{
class CheckButton;
class ComboBox;
class Label;
class PanelListPanel;
class TextEntry;
class IScheme;
} // namespace vgui2

// Datengetriebene settings.scr-Liste für eine ScrGroup. Jede Zeile trägt
// Beschriftung und Control selbst — die tote erste Spalte von PanelListPanel
// bleibt ungenutzt (SetFirstColumnWidth(0), AddItem(nullptr, row)).
class CCreateGameSettingsList : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCreateGameSettingsList, vgui2::Panel);

public:
	CCreateGameSettingsList(vgui2::Panel *parent, const char *name, csretro::ScrGroup group);

	void OnResetData();
	void OnApplyChanges();
	void PerformLayout() override;
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

	int Gate_RowCount() const { return static_cast<int>(m_rows.size()); }
	bool Gate_SetValue(const char *cvar, const char *value);
	bool Gate_AuditLabels();

private:
	class SettingsRow;

	void BuildRows();
	std::string ReadRow(const SettingsRow &row) const;
	void WriteRow(SettingsRow &row, const std::string &value);

	csretro::ScrGroup m_group = csretro::ScrGroup::Rules;
	vgui2::PanelListPanel *m_pList = nullptr;
	std::vector<SettingsRow *> m_rows;
};
