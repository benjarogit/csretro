#pragma once

#include <vgui_controls/PropertyPage.h>

class CCvarNegateCheckButton;
class CKeyToggleCheckButton;
class CCvarToggleCheckButton;
class CCvarSlider;

namespace vgui2
{
class TextEntry;
}

class COptionsSubMouse : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubMouse, vgui2::PropertyPage);

public:
	explicit COptionsSubMouse(vgui2::Panel *parent);
	~COptionsSubMouse() override;

	void OnPageShow() override;
	void OnResetData() override;
	void OnApplyChanges() override;

protected:
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;

private:
	MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
	MESSAGE_FUNC_PTR(OnCvarChanged, "CvarChanged", panel);
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel);

	void UpdateSensitivityLabel(float value);
	void BoundSensitivityValue();

	CCvarNegateCheckButton *m_pReverseMouseCheckBox = nullptr;
	CKeyToggleCheckButton *m_pMouseLookCheckBox = nullptr;
	CCvarToggleCheckButton *m_pMouseFilterCheckBox = nullptr;
	CCvarToggleCheckButton *m_pMouseRawInputCheckBox = nullptr;
	CCvarToggleCheckButton *m_pJoystickCheckBox = nullptr;
	CKeyToggleCheckButton *m_pJoystickLookCheckBox = nullptr;
	CCvarSlider *m_pMouseSensitivitySlider = nullptr;
	vgui2::TextEntry *m_pMouseSensitivityLabel = nullptr;
	CCvarToggleCheckButton *m_pAutoAimCheckBox = nullptr;
};
