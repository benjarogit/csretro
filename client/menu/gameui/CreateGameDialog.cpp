#include "CreateGameDialog.h"
#include "CreateGameGameplayPage.h"
#include "CreateGameServerPage.h"

#include "Controls/MenuEngine.h"

#include <vgui_controls/Controls.h>
#include <vgui_controls/PropertySheet.h>

#include "../src/menu_priv.h"
#include "../vgui/vgui_boot.h"
#include "keydefs.h"

#include <cstdlib>
#include <string>

using namespace vgui2;

// Gate-Treiber: echte Menü-Einstiegspunkte statt VGUI-Interna.
void UI_KeyEvent(int key, int down);

namespace
{
// Höher als Options: Server-Seite trägt Map, Identity und Bots untereinander.
const int kPreferredWide = 512;
const int kPreferredTall = 496;
} // namespace

CCreateGameDialog::CCreateGameDialog(Panel *parent)
	: PropertyDialog(parent, "CreateGameDialog")
{
	SetDeleteSelfOnClose(false);
	SetBounds(0, 0, kPreferredWide, kPreferredTall);
	SetSizeable(false);
	SetMoveable(true);
	SetClipToParent(true);
	SetTitle("#GameUI_CreateServer", true);
	SetOKButtonText("#GameUI_Start");
	SetApplyButtonVisible(false);

	if (PropertySheet *sheet = GetPropertySheet())
	{
		sheet->SetTabWidth(84);
		sheet->SetTabHeight(24);
	}

	m_pServerPage = new CCreateGameServerPage(this);
	// Ohne Maps gibt es nichts zu starten — dann bleibt der Dialog aus, statt eine
	// leere Combo anzubieten.
	if (m_pServerPage->HasMaps())
		AddPage(m_pServerPage, "#GameUI_Server");

	m_pGameplayPage = new CCreateGameGameplayPage(this, csretro::ScrGroup::Rules);
	if (m_pGameplayPage->HasOptions())
		AddPage(m_pGameplayPage, "#GameUI_Game");

	m_pFairnessPage = new CCreateGameGameplayPage(this, csretro::ScrGroup::Fairness);
	if (m_pFairnessPage->HasOptions())
		AddPage(m_pFairnessPage, "#CsretroGameUI_Fairness");
}

CCreateGameDialog::~CCreateGameDialog() = default;

bool CCreateGameDialog::HasPages() const
{
	return m_pServerPage && m_pServerPage->HasMaps();
}

void CCreateGameDialog::Activate()
{
	if (m_pServerPage)
		m_pServerPage->OnResetData();
	BaseClass::Activate();
	Menu_Con("CSRETRO_CREATE_VISIBLE %d", IsVisible() ? 1 : 0);
}

void CCreateGameDialog::OnClose()
{
	Menu_Con("CSRETRO_CREATE_CLOSE");
	// Screen-Zustand nicht auf NEWGAME stehen lassen — sonst greift beim nächsten
	// ESC der Interim-Rückweg, obwohl es keinen Interim-Screen mehr gibt.
	if (gScreen == SCREEN_NEWGAME)
		gScreen = SCREEN_MAIN;
	BaseClass::OnClose();
}

bool CCreateGameDialog::OnOK(bool applyOnly)
{
	BaseClass::OnOK(applyOnly);
	if (applyOnly)
		return true;

	// Pages haben ihre Werte bereits ins ServerProfile geschrieben.
	Menu_Con("CSRETRO_CREATE_START map=%s slots=%d bots=%d",
		gProfile.map.c_str(), gProfile.maxplayers, gProfile.bot_quota);
	Profile_Start(&gProfile);
	return true;
}

bool CCreateGameDialog::Gate_SetValue(const char *cvar, const char *value)
{
	if (m_pServerPage && m_pServerPage->Gate_SetValue(cvar, value))
		return true;
	if (m_pGameplayPage && m_pGameplayPage->Gate_SetValue(cvar, value))
		return true;
	if (m_pFairnessPage && m_pFairnessPage->Gate_SetValue(cvar, value))
		return true;
	return false;
}

void CCreateGameDialog::Gate_AuditAllLabels()
{
	if (PropertySheet *sheet = GetPropertySheet())
	{
		if (m_pGameplayPage)
		{
			sheet->SetActivePage(m_pGameplayPage);
			m_pGameplayPage->InvalidateLayout(true);
		}
		if (m_pFairnessPage)
		{
			sheet->SetActivePage(m_pFairnessPage);
			m_pFairnessPage->InvalidateLayout(true);
		}
		if (m_pServerPage)
		{
			sheet->SetActivePage(m_pServerPage);
			m_pServerPage->InvalidateLayout(true);
		}
	}

	if (m_pServerPage)
		m_pServerPage->Gate_AuditLabels();
	if (m_pGameplayPage)
		m_pGameplayPage->Gate_AuditLabels();
	if (m_pFairnessPage)
		m_pFairnessPage->Gate_AuditLabels();
}

// ---------------------------------------------------------------------------
// Gate (CSRETRO_CREATE_GATE): fährt den Dialog einmal durch und protokolliert,
// was ein Mensch sonst nachklicken müsste. Startet bewusst kein Level.
// ---------------------------------------------------------------------------
void CreateGame_GateTick()
{
	static bool enabled = getenv("CSRETRO_CREATE_GATE") != nullptr;
	if (!enabled)
		return;

	static int frame = 0;
	static int step = 0;
	static CCreateGameDialog *dlg = nullptr;
	static std::string navMap, plainMap;
	++frame;
	// Erst laufen lassen, bis Hauptmenü und Schemes stehen.
	if (frame < 90)
		return;

	switch (step)
	{
	case 0:
		GameUI_RunMenuCommand("OpenCreateMultiplayerGameDialog");
		++step;
		break;

	case 1:
	{
		dlg = VGuiXash_GateGetCreateGameDialog();
		if (!dlg || !dlg->IsVisible())
		{
			Menu_Con("CSRETRO_CREATE_GATE_FAIL kein Dialog");
			Menu_Con("CSRETRO_CREATE_GATE_DONE");
			step = 99;
			break;
		}
		CCreateGameServerPage *page = dlg->ServerPage();
		Menu_Con("CSRETRO_CREATE_GATE_OPEN visible=1 maps=%d", page->Gate_MapCount());
		++step;
		break;
	}

	case 2:
	{
		// Map mit Nav-Mesh: Bots müssen wählbar sein.
		CCreateGameServerPage *page = dlg->ServerPage();
		navMap = "de_dust";
		if (!page->Gate_SelectMap(navMap.c_str()))
		{
			Menu_Con("CSRETRO_CREATE_GATE_FAIL Nav-Map fehlt");
			step = 98;
			break;
		}
		Menu_Con("CSRETRO_CREATE_GATE_NAVMAP map=%s bots=%d",
			page->Gate_SelectedMap(), page->Gate_BotsAvailable() ? 1 : 0);
		++step;
		break;
	}

	case 3:
	{
		// Irgendeine Map ohne Nav-Mesh: Bots müssen gesperrt sein.
		CCreateGameServerPage *page = dlg->ServerPage();
		for (const char *cand : {"cs_assault", "de_aztec", "cs_italy", "de_inferno"})
		{
			if (!page->Gate_SelectMap(cand))
				continue;
			if (page->Gate_BotsAvailable())
				continue;
			plainMap = cand;
			break;
		}
		if (plainMap.empty())
			Menu_Con("CSRETRO_CREATE_GATE_NONAV skipped");
		else
			Menu_Con("CSRETRO_CREATE_GATE_NONAV map=%s bots=0", plainMap.c_str());
		++step;
		break;
	}

	case 4:
	{
		// Zurück auf die Nav-Map und Bots einschalten.
		CCreateGameServerPage *page = dlg->ServerPage();
		page->Gate_SelectMap(navMap.c_str());
		page->Gate_SetBotsEnabled(true);
		++step;
		break;
	}

	case 5:
		// Erst im Folgeframe übernehmen: SetControlString läuft über die
		// Nachrichtenschleife, im selben Frame stünde noch der alte Wert im Feld.
		dlg->ServerPage()->OnApplyChanges();
		Menu_Con("CSRETRO_CREATE_GATE_APPLY map=%s bots=%d",
			gProfile.map.c_str(), gProfile.bot_quota);
		++step;
		break;

	case 6:
	{
		CCreateGameGameplayPage *rules = dlg->GameplayPage();
		CCreateGameGameplayPage *fair = dlg->FairnessPage();
		const int identity = dlg->ServerPage()->Gate_IdentityCount();
		const int rulesN = rules ? rules->Gate_OptionCount() : 0;
		const int fairN = fair ? fair->Gate_OptionCount() : 0;
		const int total = identity + rulesN + fairN;
		if (total <= 0)
		{
			Menu_Con("CSRETRO_CREATE_GATE_FAIL keine settings.scr-Eintraege");
			step = 98;
			break;
		}
		Menu_Con("CSRETRO_CREATE_GATE_GAMEPLAY options=%d identity=%d rules=%d fairness=%d",
			total, identity, rulesN, fairN);

		// Werte über die Liste, die den CVar besitzt — nicht mehr nur Game.
		dlg->Gate_SetValue("hostname", "CS Retro Gate");
		dlg->Gate_SetValue("maxplayers", "24");
		dlg->Gate_SetValue("mp_timelimit", "35");
		dlg->Gate_SetValue("mp_friendlyfire", "1");
		dlg->Gate_SetValue("mp_forcecamera", "2");
		// Über dem Maximum aus settings.scr (15) — muss beim Übernehmen gekappt werden.
		dlg->Gate_SetValue("mp_roundtime", "99");
		++step;
		break;
	}

	case 7:
	{
		// Folgeframe, damit SetText durch die Nachrichtenschleife ist.
		// Identity zuerst (über die Server-Seite), dann Rules und Fairness.
		dlg->ServerPage()->OnApplyChanges();
		if (CCreateGameGameplayPage *rules = dlg->GameplayPage())
			rules->OnApplyChanges();
		if (CCreateGameGameplayPage *fair = dlg->FairnessPage())
			fair->OnApplyChanges();

		const char *timelimit = "";
		const char *roundtime = "";
		const char *camera = "";
		const char *ff = "";
		for (const auto &kv : gProfile.gameplay)
		{
			if (kv.first == "mp_timelimit")
				timelimit = kv.second.c_str();
			else if (kv.first == "mp_roundtime")
				roundtime = kv.second.c_str();
			else if (kv.first == "mp_forcecamera")
				camera = kv.second.c_str();
			else if (kv.first == "mp_friendlyfire")
				ff = kv.second.c_str();
		}
		Menu_Con("CSRETRO_CREATE_GATE_GPAPPLY host=\"%s\" slots=%d timelimit=%s "
			 "roundtime=%s camera=%s ff=%s",
			gProfile.hostname.c_str(), gProfile.maxplayers, timelimit, roundtime, camera, ff);
		++step;
		break;
	}

	case 8:
		dlg->InvalidateLayout(true);
		dlg->Gate_AuditAllLabels();
		++step;
		break;

	case 9:
		UI_KeyEvent(K_ESCAPE, 1);
		UI_KeyEvent(K_ESCAPE, 0);
		++step;
		break;

	case 10:
		Menu_Con("CSRETRO_CREATE_GATE_ESC visible=%d",
			VGuiXash_IsCreateGameActive() ? 1 : 0);
		Menu_Con("CSRETRO_CREATE_GATE_DONE");
		// Sauber beenden statt vom Skript abschießen zu lassen: nur so prüft das Gate,
		// dass der Create-Dialog den geordneten Shutdown nicht kaputt macht.
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		step = 99;
		break;

	case 98:
		Menu_Con("CSRETRO_CREATE_GATE_DONE");
		step = 99;
		break;

	default:
		break;
	}
}
