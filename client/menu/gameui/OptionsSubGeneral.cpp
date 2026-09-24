#include "OptionsSubGeneral.h"

#include <tier1/KeyValues.h>
#include <vgui/ISystem.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>

#include "../vgui/steam_language.h"

using namespace vgui2;

extern ISystem *g_pVGuiSystem;

COptionsSubGeneral::COptionsSubGeneral(Panel *parent) : PropertyPage(parent, "OptionsSubGeneral")
{
	m_pLanguageLabel = new Label(this, "LanguageLabel", "#CsretroGameUI_Language");
	m_pLanguage = new ComboBox(this, "Language", 2, false);
	m_pLanguage->AddItem("#CsretroGameUI_LanguageEnglish", new KeyValues("language", "value", "english"));
	m_pLanguage->AddItem("#CsretroGameUI_LanguageGerman", new KeyValues("language", "value", "german"));
	m_pLanguage->AddActionSignalTarget(this);
	OnResetData();
}

COptionsSubGeneral::~COptionsSubGeneral() = default;

void COptionsSubGeneral::OnResetData()
{
	m_pLanguage->ActivateItem(!strcmp(Csretro_GetUiLanguage(), "german") ? 1 : 0);
}

void COptionsSubGeneral::OnApplyChanges()
{
	KeyValues *item = m_pLanguage->GetActiveItemUserData();
	const char *language = item ? item->GetString("value", "english") : "english";
	if (g_pVGuiSystem && Csretro_SetUiLanguage(language))
	{
		g_pVGuiSystem->SetRegistryString("csretro.language", Csretro_GetUiLanguage());
		g_pVGuiSystem->SaveUserConfigFile();
	}
}

void COptionsSubGeneral::PerformLayout()
{
	BaseClass::PerformLayout();
	int wide = 0;
	int tall = 0;
	GetSize(wide, tall);
	m_pLanguageLabel->SetBounds(36, 36, 180, 24);
	m_pLanguage->SetBounds(220, 32, std::max(160, wide - 256), 24);
}

void COptionsSubGeneral::OnControlModified(Panel *panel)
{
	if (panel == m_pLanguage)
		PostActionSignal(new KeyValues("ApplyButtonEnable"));
}