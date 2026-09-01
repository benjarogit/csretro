#include "LabeledCommandComboBox.h"
#include "MenuEngine.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Controls.h"

#include <cstring>

extern vgui2::ILocalize *g_pVGuiLocalize;

using namespace vgui2;

CLabeledCommandComboBox::CLabeledCommandComboBox(Panel *parent, const char *panelName)
	: ComboBox(parent, panelName, 6, false)
{
	AddActionSignalTarget(this);
}

void CLabeledCommandComboBox::DeleteAllItems()
{
	RemoveAll();
	m_Items.RemoveAll();
	m_iCurrentSelection = -1;
	m_iStartSelection = -1;
}

void CLabeledCommandComboBox::AddItem(const char *text, const char *engineCommand)
{
	if (!text || !engineCommand)
		return;

	const int idx = m_Items.AddToTail();
	COMMANDITEM *item = &m_Items[idx];
	item->comboBoxID = ComboBox::AddItem(text, nullptr);
	Q_strncpy(item->name, text, sizeof(item->name));

	if (text[0] == '#' && g_pVGuiLocalize)
	{
		wchar_t *localized = g_pVGuiLocalize->Find(text);
		if (localized)
			g_pVGuiLocalize->ConvertUnicodeToANSI(localized, item->name, sizeof(item->name));
	}
	Q_strncpy(item->command, engineCommand, sizeof(item->command));
}

void CLabeledCommandComboBox::ActivateCommandItem(int index)
{
	if (index < 0 || index >= m_Items.Count())
		return;
	ComboBox::ActivateItem(m_Items[index].comboBoxID);
	m_iCurrentSelection = index;
}

void CLabeledCommandComboBox::SetInitialItem(int index)
{
	if (index < 0 || index >= m_Items.Count())
		return;
	m_iStartSelection = index;
	ActivateCommandItem(index);
}

void CLabeledCommandComboBox::OnTextChanged(const char *text)
{
	if (!text)
		return;
	for (int i = 0; i < m_Items.Count(); ++i)
	{
		if (!Q_stricmp(m_Items[i].name, text))
		{
			m_iCurrentSelection = i;
			break;
		}
	}
	if (HasBeenModified())
		PostActionSignal(new KeyValues("ControlModified"));
}

const char *CLabeledCommandComboBox::GetActiveItemCommand()
{
	if (m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count())
		return nullptr;
	return m_Items[m_iCurrentSelection].command;
}

void CLabeledCommandComboBox::ApplyChanges()
{
	if (m_iCurrentSelection < 0 || m_iCurrentSelection >= m_Items.Count())
		return;
	MenuEngine::ClientCmd(m_Items[m_iCurrentSelection].command);
	MarkApplied();
}

void CLabeledCommandComboBox::MarkApplied()
{
	m_iStartSelection = m_iCurrentSelection;
}

bool CLabeledCommandComboBox::HasBeenModified()
{
	return m_iStartSelection != m_iCurrentSelection;
}

void CLabeledCommandComboBox::Reset()
{
	if (m_iStartSelection >= 0)
		ActivateCommandItem(m_iStartSelection);
}
