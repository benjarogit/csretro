#pragma once

#include <vgui_controls/CheckButton.h>

class CKeyToggleCheckButton : public vgui2::CheckButton
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CKeyToggleCheckButton, vgui2::CheckButton);

public:
	CKeyToggleCheckButton(vgui2::Panel *parent, const char *panelName, const char *text,
		const char *keyname, const char *cmdname);
	~CKeyToggleCheckButton() override;

	void Paint() override;
	void Reset();
	void ApplyChanges();
	bool HasBeenModified();

private:
	MESSAGE_FUNC(OnButtonChecked, "CheckButtonChecked");

	char *m_pszKeyName = nullptr;
	char *m_pszCmdName = nullptr;
	bool m_bStartValue = false;
};
