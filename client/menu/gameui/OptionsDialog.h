#pragma once

#include <vgui_controls/PropertyDialog.h>
#include <tier1/utldict.h>

// NextClient-Class-Struktur (COptionsDialog : PropertyDialog).
// Tabs nur anzeigen, wenn echte Subpages existieren — keine Stub-Pages.
class COptionsDialog : public vgui2::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(COptionsDialog, vgui2::PropertyDialog);

	CUtlDict<vgui2::PropertyPage *, unsigned short> m_tabNames;

public:
	explicit COptionsDialog(vgui2::Panel *parent);
	~COptionsDialog() override;

	void OnKeyCodeTyped(vgui2::KeyCode code) override;
	void OpenTab(const char *tabName);
	void Activate() override;
	void OnClose() override;

	void RegisterPage(vgui2::PropertyPage *page, const char *tabKey, const char *tabTitle);
	bool HasPages() const;

	// Functional gate (CSRETRO_OPTIONS_GATE)
	void Gate_Apply();
	void Gate_OK();
	void Gate_Cancel();
};
