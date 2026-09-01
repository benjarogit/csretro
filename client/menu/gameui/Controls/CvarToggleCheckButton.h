#pragma once

#include <vgui_controls/CheckButton.h>

class CCvarToggleCheckButton : public vgui2::CheckButton
{
	DECLARE_CLASS_SIMPLE(CCvarToggleCheckButton, vgui2::CheckButton);

public:
	CCvarToggleCheckButton(vgui2::Panel *parent, const char *panelName, const char *text,
		const char *cvarname);
	~CCvarToggleCheckButton() override;

	void SetSelected(bool state) override;
	void Paint() override;

	void Reset();
	void ApplyChanges();
	bool HasBeenModified();
	void ApplySettings(KeyValues *inResourceData) override;

private:
	MESSAGE_FUNC(OnButtonChecked, "CheckButtonChecked");

	char *m_pszCvarName = nullptr;
	bool m_bStartValue = false;
};
