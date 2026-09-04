#pragma once

#include <vgui_controls/PropertyPage.h>

#include <string>
#include <vector>

class CCreateGameSettingsList;
struct ServerProfile;

namespace vgui2
{
class CheckButton;
class ComboBox;
class Label;
class RadioButton;
}

// Classic „Server“-Seite: Map, Identity (Name/Slots/Passwort aus settings.scr)
// und Bots. Quelle der Wahrheit ist das ServerProfile, nicht die Controls.
class CCreateGameServerPage : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCreateGameServerPage, vgui2::PropertyPage);

public:
	explicit CCreateGameServerPage(vgui2::Panel *parent);
	~CCreateGameServerPage() override;

	void OnResetData() override;
	void OnApplyChanges() override;

	bool HasMaps() const { return !m_maps.empty(); }

	// Gate-Zugriff (CSRETRO_CREATE_GATE)
	int Gate_MapCount() const { return static_cast<int>(m_maps.size()); }
	bool Gate_SelectMap(const char *name);
	const char *Gate_SelectedMap() const;
	bool Gate_BotsAvailable() const;
	void Gate_SetBotsEnabled(bool enabled);
	int Gate_IdentityCount() const;
	bool Gate_SetValue(const char *cvar, const char *value);
	bool Gate_AuditLabels();

private:
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC(OnCheckButtonChecked, "CheckButtonChecked");

	struct MapEntry
	{
		std::string name;
		bool hasNav = false;
	};

	void LoadMapList();
	void UpdateBotControls();
	void EnsureBotQuota(bool botsOn);
	const MapEntry *SelectedEntry() const;

	std::vector<MapEntry> m_maps;

	vgui2::ComboBox *m_pMapList = nullptr;
	vgui2::CheckButton *m_pEnableBots = nullptr;
	vgui2::Label *m_pBotHint = nullptr;
	vgui2::RadioButton *m_pSkill[4] = {nullptr, nullptr, nullptr, nullptr};
	CCreateGameSettingsList *m_pIdentity = nullptr;
};
