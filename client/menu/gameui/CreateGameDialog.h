#pragma once

#include <vgui_controls/PropertyDialog.h>

class CCreateGameGameplayPage;
class CCreateGameServerPage;

// Classic „Create Server“-Dialog. Schreibt beim Start ausschließlich über das
// ServerProfile — dieselbe Konfiguration, die später Dedicated benutzen soll.
class CCreateGameDialog : public vgui2::PropertyDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCreateGameDialog, vgui2::PropertyDialog);

public:
	explicit CCreateGameDialog(vgui2::Panel *parent);
	~CCreateGameDialog() override;

	bool HasPages() const;
	void Activate() override;
	void OnClose() override;
	bool OnOK(bool applyOnly) override;

	CCreateGameServerPage *ServerPage() { return m_pServerPage; }
	CCreateGameGameplayPage *GameplayPage() { return m_pGameplayPage; }
	CCreateGameGameplayPage *FairnessPage() { return m_pFairnessPage; }

	// Setzt einen CVar-Wert auf der Liste, die ihn besitzt.
	bool Gate_SetValue(const char *cvar, const char *value);
	void Gate_AuditAllLabels();

private:
	CCreateGameServerPage *m_pServerPage = nullptr;
	CCreateGameGameplayPage *m_pGameplayPage = nullptr;
	CCreateGameGameplayPage *m_pFairnessPage = nullptr;
};

// Per-Frame-Hook, nur aktiv unter CSRETRO_CREATE_GATE.
void CreateGame_GateTick();
