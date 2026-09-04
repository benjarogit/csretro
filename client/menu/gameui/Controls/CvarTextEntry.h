#pragma once

#include <vgui_controls/TextEntry.h>

class CCvarTextEntry : public vgui2::TextEntry
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCvarTextEntry, vgui2::TextEntry);

	static const int MAX_CVAR_TEXT = 64;

public:
	CCvarTextEntry(vgui2::Panel *parent, const char *panelName, const char *cvarname);
	~CCvarTextEntry() override;

	void ApplyChanges(bool immediate = false);
	void ApplySchemeSettings(vgui2::IScheme *pScheme) override;
	void Reset();
	bool HasBeenModified();

private:
	MESSAGE_FUNC(OnTextChanged, "TextChanged");

	char *m_pszCvarName = nullptr;
	char m_pszStartValue[MAX_CVAR_TEXT]{};
};
