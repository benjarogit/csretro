#pragma once

#include <vgui_controls/CheckButton.h>

class CCvarNegateCheckButton : public vgui2::CheckButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCvarNegateCheckButton, vgui2::CheckButton);

public:
	CCvarNegateCheckButton(vgui2::Panel *parent, const char *panelName, const char *text,
		const char *cvarname);
	~CCvarNegateCheckButton() override;

	void SetSelected(bool state) override;
	void Paint() override;

	void Reset();
	void ApplyChanges();
	bool HasBeenModified();

private:
	MESSAGE_FUNC(OnButtonChecked, "CheckButtonChecked");

	char *m_pszCvarName = nullptr;
	bool m_bStartState = false;
};
