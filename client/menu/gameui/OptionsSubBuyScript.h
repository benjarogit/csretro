#pragma once

#include <vgui_controls/PropertyPage.h>

#include <array>
#include <string>

namespace vgui2
{
class ComboBox;
class TextEntry;
}

class COptionsSubBuyScript : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(COptionsSubBuyScript, vgui2::PropertyPage);

public:
	explicit COptionsSubBuyScript(vgui2::Panel *parent);
	~COptionsSubBuyScript() override;

	void OnResetData() override;
	void OnApplyChanges() override;
	void PerformLayout() override;

private:
	const char *CurrentScriptPath() const;
	int CurrentScriptIndex() const;
	void LoadScripts();
	void ShowCurrentScript();
	void StashCurrentScript();
	bool SaveScript(int scriptIndex);

	MESSAGE_FUNC_PARAMS(OnTextChanged, "TextChanged", data);

	vgui2::ComboBox *m_pScriptType = nullptr;
	vgui2::TextEntry *m_pEditor = nullptr;
	std::array<std::string, 2> m_stagedText;
	int m_visibleScript = 0;
};