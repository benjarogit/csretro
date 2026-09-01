#include "CvarToggleCheckButton.h"
#include "MenuEngine.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"

#include <cstdlib>
#include <cstring>

using namespace vgui2;

vgui2::Panel *CvarToggleCheckButton_Factory()
{
	return new CCvarToggleCheckButton(nullptr, nullptr, "CvarToggleCheckButton", nullptr);
}

DECLARE_BUILD_FACTORY_CUSTOM(CCvarToggleCheckButton, CvarToggleCheckButton_Factory);

CCvarToggleCheckButton::CCvarToggleCheckButton(Panel *parent, const char *panelName, const char *text,
	const char *cvarname)
	: CheckButton(parent, panelName, text)
{
	m_pszCvarName = cvarname ? strdup(cvarname) : nullptr;
	if (m_pszCvarName)
		Reset();
	AddActionSignalTarget(this);
}

CCvarToggleCheckButton::~CCvarToggleCheckButton()
{
	free(m_pszCvarName);
}

void CCvarToggleCheckButton::Paint()
{
	if (!m_pszCvarName || !m_pszCvarName[0])
	{
		BaseClass::Paint();
		return;
	}

	const bool value = MenuEngine::GetCvarFloat(m_pszCvarName) > 0.0f;
	if (value != m_bStartValue)
	{
		SetSelected(value);
		m_bStartValue = value;
	}
	BaseClass::Paint();
}

void CCvarToggleCheckButton::ApplyChanges()
{
	if (!m_pszCvarName || !m_pszCvarName[0])
		return;
	m_bStartValue = IsSelected();
	MenuEngine::CvarSetValue(m_pszCvarName, m_bStartValue ? 1.0f : 0.0f);
}

void CCvarToggleCheckButton::Reset()
{
	if (!m_pszCvarName || !m_pszCvarName[0])
		return;
	m_bStartValue = MenuEngine::GetCvarFloat(m_pszCvarName) > 0.0f;
	SetSelected(m_bStartValue);
}

bool CCvarToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}

void CCvarToggleCheckButton::SetSelected(bool state)
{
	BaseClass::SetSelected(state);
}

void CCvarToggleCheckButton::OnButtonChecked()
{
	if (HasBeenModified())
		PostActionSignal(new KeyValues("ControlModified"));
}

void CCvarToggleCheckButton::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	const char *cvarName = inResourceData->GetString("cvar_name", "");
	if (!cvarName[0])
		return;

	free(m_pszCvarName);
	m_pszCvarName = strdup(cvarName);

	if (MenuEngine::GetCvarFloat(m_pszCvarName) != 0)
		SetSelected(true);
	else
		SetSelected(false);
	m_bStartValue = IsSelected();
}
