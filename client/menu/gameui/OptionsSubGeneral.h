#pragma once

#include <vgui_controls/PropertyPage.h>

namespace vgui2
{
class ComboBox;
class Label;
}

class COptionsSubGeneral : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(COptionsSubGeneral, vgui2::PropertyPage);

public:
	explicit COptionsSubGeneral(vgui2::Panel *parent);
	~COptionsSubGeneral() override;

	void OnResetData() override;
	void OnApplyChanges() override;
	void PerformLayout() override;

private:
	MESSAGE_FUNC_PTR(OnControlModified, "TextChanged", panel);

	vgui2::Label *m_pLanguageLabel = nullptr;
	vgui2::ComboBox *m_pLanguage = nullptr;
};