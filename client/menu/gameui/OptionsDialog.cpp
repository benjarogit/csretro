#include "OptionsDialog.h"

#include <vgui/KeyCode.h>
#include <vgui_controls/PropertyPage.h>
#include <vgui_controls/PropertySheet.h>

#include "../src/menu_priv.h"

using namespace vgui2;

COptionsDialog::COptionsDialog(Panel *parent)
	: PropertyDialog(parent, "OptionsDialog")
{
	SetDeleteSelfOnClose(false);
	SetBounds(0, 0, 545, 406);
	SetSizeable(false);
	SetTitle("#GameUI_Options", true);
	SetApplyButtonVisible(true);
	if (GetPropertySheet())
		GetPropertySheet()->SetTabWidth(84);

	// Echte NextClient-Subpages werden hier registriert (zuerst Mouse).
	// Keine Stub-/Dummy-Tabs.
}

COptionsDialog::~COptionsDialog() = default;

void COptionsDialog::RegisterPage(PropertyPage *page, const char *tabKey, const char *tabTitle)
{
	if (!page || !tabKey || !tabTitle)
		return;
	AddPage(page, tabTitle);
	m_tabNames.Insert(tabKey, page);
}

bool COptionsDialog::HasPages() const
{
	return m_tabNames.Count() > 0;
}

void COptionsDialog::OnKeyCodeTyped(KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		Close();
		return;
	}
	BaseClass::OnKeyCodeTyped(code);
}

void COptionsDialog::OpenTab(const char *tabName)
{
	if (!tabName || !GetPropertySheet())
		return;
	const unsigned short idx = m_tabNames.Find(tabName);
	if (m_tabNames.IsValidIndex(idx))
		GetPropertySheet()->SetActivePage(m_tabNames[idx]);
}

void COptionsDialog::Activate()
{
	BaseClass::Activate();
	SetVisible(true);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true);
	MoveToFront();
	Menu_Con("CSRETRO_OPTIONS_VISIBLE");
}

void COptionsDialog::OnClose()
{
	Menu_Con("CSRETRO_OPTIONS_CLOSE");
	BaseClass::OnClose();
}
