#include "CvarNegateCheckButton.h"
#include "MenuEngine.h"

#include "tier1/KeyValues.h"

#include <cmath>
#include <cstdlib>

using namespace vgui2;

CCvarNegateCheckButton::CCvarNegateCheckButton(Panel *parent, const char *panelName, const char *text,
	const char *cvarname)
	: CheckButton(parent, panelName, text)
{
	m_pszCvarName = cvarname ? strdup(cvarname) : nullptr;
	Reset();
	AddActionSignalTarget(this);
}

CCvarNegateCheckButton::~CCvarNegateCheckButton()
{
	free(m_pszCvarName);
}

void CCvarNegateCheckButton::Paint()
{
	if (!m_pszCvarName)
	{
		BaseClass::Paint();
		return;
	}

	const float value = MenuEngine::GetCvarFloat(m_pszCvarName);
	if (value < 0)
	{
		if (!m_bStartState)
		{
			SetSelected(true);
			m_bStartState = true;
		}
	}
	else if (m_bStartState)
	{
		SetSelected(false);
		m_bStartState = false;
	}
	BaseClass::Paint();
}

void CCvarNegateCheckButton::Reset()
{
	if (!m_pszCvarName)
		return;
	m_bStartState = MenuEngine::GetCvarFloat(m_pszCvarName) < 0;
	SetSelected(m_bStartState);
}

bool CCvarNegateCheckButton::HasBeenModified()
{
	return IsSelected() != m_bStartState;
}

void CCvarNegateCheckButton::SetSelected(bool state)
{
	BaseClass::SetSelected(state);
}

void CCvarNegateCheckButton::ApplyChanges()
{
	if (!m_pszCvarName || !m_pszCvarName[0])
		return;

	float value = MenuEngine::GetCvarFloat(m_pszCvarName);
	value = std::fabs(value);
	if (value < 0.00001f)
		value = 0.022f;

	m_bStartState = IsSelected();
	value = -value;
	MenuEngine::CvarSetValue(m_pszCvarName, m_bStartState ? value : -value);
}

void CCvarNegateCheckButton::OnButtonChecked()
{
	if (HasBeenModified())
		PostActionSignal(new KeyValues("ControlModified"));
}
