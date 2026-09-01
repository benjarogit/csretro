#include "KeyToggleCheckButton.h"
#include "MenuEngine.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"

#include <cstdlib>

using namespace vgui2;

CKeyToggleCheckButton::CKeyToggleCheckButton(Panel *parent, const char *panelName, const char *text,
	const char *keyname, const char *cmdname)
	: CheckButton(parent, panelName, text)
{
	m_pszKeyName = keyname ? strdup(keyname) : nullptr;
	m_pszCmdName = cmdname ? strdup(cmdname) : nullptr;
	if (m_pszKeyName)
		Reset();
	AddActionSignalTarget(this);
}

CKeyToggleCheckButton::~CKeyToggleCheckButton()
{
	free(m_pszKeyName);
	free(m_pszCmdName);
}

void CKeyToggleCheckButton::Paint()
{
	BaseClass::Paint();
	if (!m_pszKeyName)
		return;

	bool isdown = false;
	if (MenuEngine::IsKeyDown(m_pszKeyName, isdown) && m_bStartValue != isdown)
	{
		SetSelected(isdown);
		m_bStartValue = isdown;
	}
}

void CKeyToggleCheckButton::Reset()
{
	if (!m_pszKeyName)
		return;
	bool down = false;
	if (!MenuEngine::IsKeyDown(m_pszKeyName, down))
	{
		// Kein kbutton (z. B. vor Client-Load): UI nicht crashen.
		SetEnabled(false);
		return;
	}
	SetEnabled(true);
	m_bStartValue = down;
	if (IsSelected() != m_bStartValue)
		SetSelected(m_bStartValue);
}

void CKeyToggleCheckButton::ApplyChanges()
{
	if (!m_pszCmdName || !m_pszCmdName[0])
		return;
	char szCommand[256];
	Q_snprintf(szCommand, sizeof(szCommand), "%c%s\n", IsSelected() ? '+' : '-', m_pszCmdName);
	MenuEngine::ClientCmd(szCommand);
	m_bStartValue = IsSelected();
}

bool CKeyToggleCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartValue;
}

void CKeyToggleCheckButton::OnButtonChecked()
{
	if (HasBeenModified())
		PostActionSignal(new KeyValues("ControlModified"));
}
