#include "CreateGameGameplayPage.h"
#include "CreateGameSettingsList.h"

using namespace vgui2;

CCreateGameGameplayPage::CCreateGameGameplayPage(Panel *parent, csretro::ScrGroup group)
	: BaseClass(parent, group == csretro::ScrGroup::Fairness ? "CreateGameFairnessPage"
								: "CreateGameGameplayPage")
{
	m_pList = new CCreateGameSettingsList(this, "GameOptions", group);
}

void CCreateGameGameplayPage::PerformLayout()
{
	BaseClass::PerformLayout();

	int wide = 0, tall = 0;
	GetSize(wide, tall);
	const int inset = 8;
	if (m_pList)
		m_pList->SetBounds(inset, inset, wide - 2 * inset, tall - 2 * inset);
}

void CCreateGameGameplayPage::OnResetData()
{
	if (m_pList)
		m_pList->OnResetData();
}

void CCreateGameGameplayPage::OnApplyChanges()
{
	if (m_pList)
		m_pList->OnApplyChanges();
}

bool CCreateGameGameplayPage::HasOptions() const
{
	return m_pList && m_pList->Gate_RowCount() > 0;
}

int CCreateGameGameplayPage::Gate_OptionCount() const
{
	return m_pList ? m_pList->Gate_RowCount() : 0;
}

bool CCreateGameGameplayPage::Gate_SetValue(const char *cvar, const char *value)
{
	return m_pList && m_pList->Gate_SetValue(cvar, value);
}

bool CCreateGameGameplayPage::Gate_AuditLabels()
{
	return m_pList && m_pList->Gate_AuditLabels();
}
