#include "CvarTextEntry.h"
#include "MenuEngine.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"

#include <cstdlib>
#include <cstring>

using namespace vgui2;

CCvarTextEntry::CCvarTextEntry(Panel *parent, const char *panelName, const char *cvarname)
	: TextEntry(parent, panelName)
{
	m_pszCvarName = cvarname ? strdup(cvarname) : nullptr;
	m_pszStartValue[0] = 0;
	if (m_pszCvarName)
		Reset();
	AddActionSignalTarget(this);
}

CCvarTextEntry::~CCvarTextEntry()
{
	free(m_pszCvarName);
}

void CCvarTextEntry::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	if (GetMaximumCharCount() < 0 || GetMaximumCharCount() > MAX_CVAR_TEXT)
		SetMaximumCharCount(MAX_CVAR_TEXT - 1);
}

void CCvarTextEntry::ApplyChanges(bool immediate)
{
	if (!m_pszCvarName)
		return;

	char szText[MAX_CVAR_TEXT];
	GetText(szText, MAX_CVAR_TEXT);
	if (!szText[0])
		return;

	if (immediate)
		MenuEngine::CvarSet(m_pszCvarName, szText);
	else
	{
		char szCommand[256];
		Q_snprintf(szCommand, sizeof(szCommand), "%s \"%s\"\n", m_pszCvarName, szText);
		MenuEngine::ClientCmd(szCommand);
	}
	Q_strncpy(m_pszStartValue, szText, sizeof(m_pszStartValue));
}

void CCvarTextEntry::Reset()
{
	if (!m_pszCvarName)
		return;
	const char *value = MenuEngine::GetCvarString(m_pszCvarName);
	if (value && value[0])
	{
		SetText(value);
		Q_strncpy(m_pszStartValue, value, sizeof(m_pszStartValue));
	}
}

bool CCvarTextEntry::HasBeenModified()
{
	char szText[MAX_CVAR_TEXT];
	GetText(szText, MAX_CVAR_TEXT);
	return Q_stricmp(szText, m_pszStartValue) != 0;
}

void CCvarTextEntry::OnTextChanged()
{
	if (!m_pszCvarName)
		return;
	if (HasBeenModified())
		PostActionSignal(new KeyValues("ControlModified"));
}
