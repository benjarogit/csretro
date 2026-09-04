#pragma once

#include <vgui_controls/Frame.h>

namespace vgui2
{
class Button;
class Label;
class ListPanel;
}

// LAN-Server-Browser als echte VGUI2-Controls.
//
// Bewusst nur LAN: die Engine trägt Broadcast-Discovery vollständig (Kommando
// „localservers“, Antwort zurück über UI_AddServerToList). Ein Internet-Tab hätte
// kein Backend — CS Retro bekommt ein eigenes Netzwerkprotokoll und damit eine
// eigene Serverliste, die es noch nicht gibt. Der Steam-Master ist kein Ziel.
// Richtungsentscheidung und Begründung: docs/SERVER.md.
class CServerBrowserDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CServerBrowserDialog, vgui2::Frame);

public:
	explicit CServerBrowserDialog(vgui2::Panel *parent);
	~CServerBrowserDialog() override;

	void Activate() override;
	void OnClose() override;
	void OnCommand(const char *command) override;
	// Broadcast-Antworten kommen ohne Abschlussmeldung — nach kScanSeconds
	// gilt der Scan als beendet, später eintreffende Server werden weiter aufgenommen.
	void RunFrameScanWindow();

	// Engine-Antwort auf „localservers“, schon als Adress-String aufgelöst.
	void AddServer(const char *address, const char *info);

	// Gate-Zugriff (CSRETRO_BROWSER_GATE)
	int Gate_ServerCount() const;
	bool Gate_IsScanning() const { return m_scanning; }
	void Gate_Refresh();
	bool Gate_SelectFirstRow();
	bool Gate_ConnectEnabled() const;

private:
	MESSAGE_FUNC(OnItemSelected, "ItemSelected");
	MESSAGE_FUNC(OnItemDeselected, "ItemDeselected");
	MESSAGE_FUNC_INT(OnItemChosen, "ListPanelItemChosen", itemID);

	void StartRefresh();
	void UpdateStatus();
	void UpdateConnectButton();
	void ConnectToSelected();
	int FindRow(const char *address) const;

	vgui2::ListPanel *m_pGameList = nullptr;
	vgui2::Label *m_pStatus = nullptr;
	vgui2::Button *m_pConnect = nullptr;

	double m_scanStart = 0.0;
	bool m_scanning = false;
};

// Trägt das Backend? Ohne Extended MenuAPI gibt es keinen Infostring-Parser und
// keine Adressauflösung — dann bleibt der Browser zu statt leer.
bool ServerBrowser_BackendReady();

bool ServerBrowser_Show(vgui2::Panel *parent);
void ServerBrowser_Hide();
bool ServerBrowser_IsActive();

// Weiterleitung des Engine-Callbacks. Ohne offenen Dialog wird verworfen: die
// Engine liefert Broadcast-Antworten unabhängig davon, wer gerade zuhört.
void ServerBrowser_AddFromEngine(const char *address, const char *info);

// Per-Frame-Hook: Scan-Fenster und Gate (Gate nur unter CSRETRO_BROWSER_GATE).
void ServerBrowser_RunFrame();

// Vor dem Löschen des VGUI-Roots: Dialog abbauen, kein dangling Pointer.
void ServerBrowser_Shutdown();
