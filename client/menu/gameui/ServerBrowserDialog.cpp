#include "ServerBrowserDialog.h"

#include "Controls/MenuEngine.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListPanel.h>

#include "../src/menu_priv.h"

#include "gameinfo.h"
#include "keydefs.h"
#include "net_api.h"
#include "netadr.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

using namespace vgui2;

extern vgui2::ILocalize *g_pVGuiLocalize;

// Gate-Treiber: echte Menü-Einstiegspunkte statt VGUI-Interna.
void UI_KeyEvent(int key, int down);
void UI_AddServerToList(struct netadr_s adr, const char *info);

namespace
{
const int kPreferredWide = 640;
const int kPreferredTall = 424;

// Broadcast-Antworten kommen ohne Abschlusssignal — anders als bei der Masterliste
// gibt es kein pfnResetPing. Nach diesem Fenster gilt der Scan als beendet; später
// eintreffende Server werden weiter aufgenommen, nur die Statuszeile wartet nicht.
const double kScanSeconds = 3.0;

CServerBrowserDialog *g_dialog = nullptr;

const char *InfoValue(const char *info, const char *key)
{
	const char *v = gExtEng.pNetAPI->ValueForKey(info, key);
	return v ? v : "";
}

// Der eigene gamedir ist der einzige Kompatibilitätsanker, den die Broadcast-Antwort
// hergibt. Die Protokollversion prüft die Engine schon vor UI_AddServerToList.
const char *OwnGameFolder()
{
	gameinfo2_t *gi = gExtEng.pfnGetGameInfo(GAMEINFO_VERSION);
	return gi ? gi->gamefolder : "";
}

// Spalten nach der Zeit zu sortieren, die als Text „12 ms“ in der Zelle steht, würde
// alphabetisch sortieren. Die Zahl liegt deshalb zusätzlich als Int in der Zeile.
int __cdecl SortByHiddenInt(ListPanel *, const ListPanelItem &a, const ListPanelItem &b,
	const char *key)
{
	const int v1 = a.kv ? a.kv->GetInt(key, 0) : 0;
	const int v2 = b.kv ? b.kv->GetInt(key, 0) : 0;
	if (v1 != v2)
		return v1 < v2 ? -1 : 1;
	return 0;
}

int __cdecl SortByPlayers(ListPanel *panel, const ListPanelItem &a, const ListPanelItem &b)
{
	return SortByHiddenInt(panel, a, b, "numcl");
}

int __cdecl SortByPing(ListPanel *panel, const ListPanelItem &a, const ListPanelItem &b)
{
	return SortByHiddenInt(panel, a, b, "pingms");
}
} // namespace

CServerBrowserDialog::CServerBrowserDialog(Panel *parent)
	: BaseClass(parent, "ServerBrowserDialog")
{
	SetDeleteSelfOnClose(false);
	SetBounds(0, 0, kPreferredWide, kPreferredTall);
	SetSizeable(false);
	SetMoveable(true);
	SetClipToParent(true);
	SetTitle("#GameUI_GameMenu_FindServers", true);

	m_pGameList = new ListPanel(this, "gamelist");
	m_pGameList->SetMultiselectEnabled(false);
	m_pGameList->SetEmptyListText("#CsretroServerBrowser_NoServers");

	// Spaltenmodell wie der klassische Browser, aber nur mit Feldern, die in der
	// Broadcast-Antwort wirklich stehen. Bots und VAC fehlen dort — deshalb fehlen
	// sie auch hier, statt als leere Spalte mitzulaufen.
	// Steam-#ServerBrowser_* fehlen in der Game-Data-Loc — eigene Tokens, sonst
	// stehen Rohkeys in den Spaltenköpfen.
	m_pGameList->AddColumnHeader(0, "Password", "#CsretroServerBrowser_Password", 70,
		ListPanel::COLUMN_FIXEDSIZE);
	m_pGameList->AddColumnHeader(1, "Name", "#CsretroServerBrowser_Servers", 160, 80, 400,
		ListPanel::COLUMN_RESIZEWITHWINDOW | ListPanel::COLUMN_UNHIDABLE);
	m_pGameList->AddColumnHeader(2, "Address", "#CsretroServerBrowser_IPAddress", 130, 90, 220, 0);
	m_pGameList->AddColumnHeader(3, "Players", "#CsretroServerBrowser_Players", 60, 50, 90, 0);
	m_pGameList->AddColumnHeader(4, "Map", "#CsretroServerBrowser_Map", 110, 70, 200, 0);
	m_pGameList->AddColumnHeader(5, "Ping", "#CsretroServerBrowser_Latency", 60, 50, 90, 0);
	m_pGameList->SetSortFunc(3, SortByPlayers);
	m_pGameList->SetSortFunc(5, SortByPing);
	m_pGameList->SetSortColumn(5);

	m_pStatus = new Label(this, "StatusLabel", "");
	m_pConnect = new Button(this, "ConnectButton", "#CsretroServerBrowser_Connect");
	m_pConnect->SetCommand("Connect");
	Button *refresh = new Button(this, "RefreshButton", "#CsretroServerBrowser_Refresh");
	refresh->SetCommand("Refresh");
	Button *closeBtn = new Button(this, "CloseButton", "#GameUI_Close");
	closeBtn->SetCommand("Close");

	LoadControlSettings("resource/ServerBrowserDialog.res");

	UpdateConnectButton();
}

CServerBrowserDialog::~CServerBrowserDialog()
{
	if (g_dialog == this)
		g_dialog = nullptr;
}

void CServerBrowserDialog::Activate()
{
	BaseClass::Activate();
	Menu_Con("CSRETRO_BROWSER_VISIBLE %d", IsVisible() ? 1 : 0);
	StartRefresh();
}

void CServerBrowserDialog::OnClose()
{
	Menu_Con("CSRETRO_BROWSER_CLOSE");
	m_scanning = false;
	// Screen-Zustand nicht auf BROWSER stehen lassen — sonst zeigt das nächste ESC
	// auf einen Rückweg, den es nicht mehr gibt.
	if (gScreen == SCREEN_BROWSER)
		gScreen = SCREEN_MAIN;
	BaseClass::OnClose();
}

void CServerBrowserDialog::RunFrameScanWindow()
{
	if (!m_scanning)
		return;
	if ((gExtEng.pfnDoubleTime() - m_scanStart) < kScanSeconds)
		return;
	m_scanning = false;
	UpdateStatus();
}

void CServerBrowserDialog::StartRefresh()
{
	if (m_pGameList)
		m_pGameList->RemoveAll();

	m_scanStart = gExtEng.pfnDoubleTime();
	m_scanning = true;
	UpdateStatus();
	UpdateConnectButton();

	// Die Engine broadcastet A2A_INFO an PORT_SERVER..+9; Antworten laufen über
	// UI_AddServerToList zurück. Deshalb kein eigener Socket im Menü.
	MenuEngine::ClientCmd("localservers\n");
	Menu_Con("CSRETRO_BROWSER_SCAN gamedir=%s", OwnGameFolder());
}

void CServerBrowserDialog::AddServer(const char *address, const char *info)
{
	if (!m_pGameList || !address || !*address || !info)
		return;

	// Fremde Mods sind nicht spielbar. Und „gs“ markiert eine GoldSrc-/Steam-Antwort:
	// CS Retro fährt ein eigenes Protokoll, dort kann man nicht mitspielen — so ein
	// Eintrag wäre eine Zeile, die beim Anklicken nur scheitern kann.
	if (InfoValue(info, "gs")[0])
	{
		Menu_Con("CSRETRO_BROWSER_REJECT %s reason=goldsrc", address);
		return;
	}
	const char *gamedir = InfoValue(info, "gamedir");
	if (strcasecmp(gamedir, OwnGameFolder()) != 0)
	{
		Menu_Con("CSRETRO_BROWSER_REJECT %s reason=gamedir(%s)", address, gamedir);
		return;
	}

	const int numcl = atoi(InfoValue(info, "numcl"));
	const int maxcl = atoi(InfoValue(info, "maxcl"));
	// Kein echter RTT: gemessen wird die Zeit vom Broadcast bis zur Antwort. Auf dem
	// LAN ist das im Wesentlichen die Frame-Auflösung — brauchbar zum Sortieren,
	// keine Leitungsmessung. docs/SERVER.md.
	const int pingms =
		std::max(1, static_cast<int>((gExtEng.pfnDoubleTime() - m_scanStart) * 1000.0));

	char players[32];
	snprintf(players, sizeof(players), "%d / %d", numcl, maxcl);
	char ping[32];
	snprintf(ping, sizeof(ping), "%d ms", pingms);

	const bool locked = InfoValue(info, "password")[0] == '1';

	const int existing = FindRow(address);
	if (existing >= 0)
	{
		// Mehrere Ports pro Scan heißt: dieselbe Adresse kann zweimal antworten.
		KeyValues *row = m_pGameList->GetItem(existing);
		if (row)
		{
			row->SetString("Name", InfoValue(info, "host"));
			row->SetString("Map", InfoValue(info, "map"));
			row->SetString("Players", players);
			row->SetString("Ping", ping);
			row->SetString("Password", locked ? "#CsretroServerBrowser_Locked" : "");
			row->SetInt("numcl", numcl);
			row->SetInt("pingms", pingms);
			m_pGameList->ApplyItemChanges(existing);
		}
		UpdateStatus();
		return;
	}

	KeyValues *row = new KeyValues("Server");
	row->SetString("Password", locked ? "#CsretroServerBrowser_Locked" : "");
	row->SetString("Name", InfoValue(info, "host"));
	row->SetString("Address", address);
	row->SetString("Players", players);
	row->SetString("Map", InfoValue(info, "map"));
	row->SetString("Ping", ping);
	row->SetInt("numcl", numcl);
	row->SetInt("pingms", pingms);
	m_pGameList->AddItem(row, 0, false, true);
	row->deleteThis();

	Menu_Con("CSRETRO_BROWSER_ADD %s map=%s players=%d/%d ping=%d rows=%d",
		address, InfoValue(info, "map"), numcl, maxcl, pingms, m_pGameList->GetItemCount());
	UpdateStatus();
	UpdateConnectButton();
}

int CServerBrowserDialog::FindRow(const char *address) const
{
	if (!m_pGameList)
		return -1;
	for (int row = 0; row < m_pGameList->GetItemCount(); ++row)
	{
		const int itemID = m_pGameList->GetItemIDFromRow(row);
		if (itemID < 0)
			continue;
		KeyValues *kv = m_pGameList->GetItem(itemID);
		if (kv && !strcasecmp(kv->GetString("Address", ""), address))
			return itemID;
	}
	return -1;
}

void CServerBrowserDialog::UpdateStatus()
{
	if (!m_pStatus)
		return;

	if (m_scanning)
	{
		m_pStatus->SetText("#CsretroServerBrowser_Scanning");
		if (m_pGameList)
			m_pGameList->SetEmptyListText("");
		return;
	}

	const int count = m_pGameList ? m_pGameList->GetItemCount() : 0;
	if (count <= 0)
	{
		// Eine Aussage reicht: EmptyListText in der Liste, Status bleibt leer.
		m_pStatus->SetText("");
		if (m_pGameList)
			m_pGameList->SetEmptyListText("#CsretroServerBrowser_NoServers");
		return;
	}

	if (m_pGameList)
		m_pGameList->SetEmptyListText("");

	wchar_t *fmt = g_pVGuiLocalize ? g_pVGuiLocalize->Find("CsretroServerBrowser_Found") : nullptr;
	if (!fmt)
	{
		// Fehlender Token wird nicht als roher Text durchgelassen — die Localization-
		// Probe in VGuiXash_Init meldet ihn, die Statuszeile bleibt dann leer.
		m_pStatus->SetText("");
		return;
	}

	char ascii[16];
	snprintf(ascii, sizeof(ascii), "%d", count);
	wchar_t number[16];
	g_pVGuiLocalize->ConvertANSIToUnicode(ascii, number, sizeof(number));

	wchar_t text[256];
	g_pVGuiLocalize->ConstructString(text, sizeof(text), fmt, 1, number);
	m_pStatus->SetText(text);
}

void CServerBrowserDialog::UpdateConnectButton()
{
	if (m_pConnect)
		m_pConnect->SetEnabled(m_pGameList && m_pGameList->GetSelectedItemsCount() > 0);
}

void CServerBrowserDialog::ConnectToSelected()
{
	if (!m_pGameList || m_pGameList->GetSelectedItemsCount() <= 0)
		return;
	KeyValues *kv = m_pGameList->GetItem(m_pGameList->GetSelectedItem(0));
	if (!kv)
		return;
	const char *address = kv->GetString("Address", "");
	if (!address[0])
		return;

	Menu_Con("CSRETRO_BROWSER_CONNECT %s", address);
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "connect %s\n", address);
	MenuEngine::ClientCmd(cmd);
	Close();
}

void CServerBrowserDialog::OnCommand(const char *command)
{
	const char *cmd = command ? command : "";
	if (!strcasecmp(cmd, "Refresh"))
		StartRefresh();
	else if (!strcasecmp(cmd, "Connect"))
		ConnectToSelected();
	else
		BaseClass::OnCommand(command);
}

void CServerBrowserDialog::OnItemSelected()
{
	UpdateConnectButton();
}

void CServerBrowserDialog::OnItemDeselected()
{
	UpdateConnectButton();
}

void CServerBrowserDialog::OnItemChosen(int)
{
	ConnectToSelected();
}

int CServerBrowserDialog::Gate_ServerCount() const
{
	return m_pGameList ? m_pGameList->GetItemCount() : 0;
}

void CServerBrowserDialog::Gate_Refresh()
{
	StartRefresh();
}

bool CServerBrowserDialog::Gate_SelectFirstRow()
{
	if (!m_pGameList || m_pGameList->GetItemCount() <= 0)
		return false;
	m_pGameList->SetSingleSelectedItem(m_pGameList->GetItemIDFromRow(0));
	UpdateConnectButton();
	return true;
}

bool CServerBrowserDialog::Gate_ConnectEnabled() const
{
	return m_pConnect && m_pConnect->IsEnabled();
}

// ---------------------------------------------------------------------------
// Modulschnittstelle
// ---------------------------------------------------------------------------
bool ServerBrowser_BackendReady()
{
	// Alles, was der Browser zwingend braucht: Broadcast auslösen, Infostring lesen,
	// Adresse als Text, Zeitbasis für die Antwortzeit und den eigenen gamedir zum
	// Filtern. Fehlt eines davon, wäre die Liste nicht vertrauenswürdig.
	return gExtEngReady && gEng.pfnClientCmd && gExtEng.pNetAPI &&
		gExtEng.pNetAPI->ValueForKey && gExtEng.pfnAdrToString && gExtEng.pfnDoubleTime &&
		gExtEng.pfnGetGameInfo && OwnGameFolder()[0];
}

bool ServerBrowser_Show(Panel *parent)
{
	if (!parent || !ServerBrowser_BackendReady())
		return false;
	if (!g_dialog)
		g_dialog = new CServerBrowserDialog(parent);

	int sw = 640, sh = 480;
	if (surface())
		surface()->GetScreenSize(sw, sh);
	int w = 0, h = 0;
	g_dialog->GetSize(w, h);
	g_dialog->SetPos((sw - w) / 2, (sh - h) / 2);
	g_dialog->Activate();
	return true;
}

void ServerBrowser_Hide()
{
	if (!g_dialog)
		return;
	g_dialog->SetVisible(false);
	g_dialog->Close();
}

bool ServerBrowser_IsActive()
{
	return g_dialog && g_dialog->IsVisible();
}

void ServerBrowser_AddFromEngine(const char *address, const char *info)
{
	if (!ServerBrowser_IsActive())
		return;
	g_dialog->AddServer(address, info);
}

void ServerBrowser_Shutdown()
{
	if (!g_dialog)
		return;
	CServerBrowserDialog *dialog = g_dialog;
	g_dialog = nullptr;
	delete dialog;
}

// ---------------------------------------------------------------------------
// Gate (CSRETRO_BROWSER_GATE): fährt den Browser einmal durch. Die Broadcast-
// Antwort selbst gehört der Engine — nachgewiesen wird, dass „localservers“ dort
// ankommt und dass alles ab UI_AddServerToList richtig filtert und anzeigt.
// Es wird bewusst nie verbunden.
// ---------------------------------------------------------------------------
namespace
{
// Antwort eines Servers nachbilden und über genau den Weg einspeisen, den die Engine
// benutzt. Ein Testpfad quer durch die Filterlogik wäre kein Nachweis.
void GateInject(const char *addressText, const char *info)
{
	netadr_t adr = {};
	char mutableText[64];
	snprintf(mutableText, sizeof(mutableText), "%s", addressText);
	if (!gExtEng.pNetAPI->StringToAdr(mutableText, &adr))
	{
		Menu_Con("CSRETRO_BROWSER_GATE_FAIL Adresse %s nicht auflösbar", addressText);
		return;
	}
	UI_AddServerToList(adr, info);
}

void GateTick()
{
	static bool enabled = getenv("CSRETRO_BROWSER_GATE") != nullptr &&
		!getenv("CSRETRO_CREATE_GATE") && !getenv("CSRETRO_MAINMENU_GATE") &&
		!getenv("CSRETRO_OPTIONS_GATE") && !getenv("CSRETRO_V1POC");
	if (!enabled)
		return;

	static int frame = 0;
	static int step = 0;
	++frame;
	// Erst laufen lassen, bis Hauptmenü und Schemes stehen.
	if (frame < 90)
		return;

	switch (step)
	{
	case 0:
		GameUI_RunMenuCommand("OpenServerBrowser");
		++step;
		break;

	case 1:
	{
		if (!ServerBrowser_IsActive())
		{
			Menu_Con("CSRETRO_BROWSER_GATE_FAIL kein Dialog");
			Menu_Con("CSRETRO_BROWSER_GATE_DONE");
			step = 99;
			break;
		}
		Menu_Con("CSRETRO_BROWSER_GATE_OPEN visible=1 scanning=%d rows=%d",
			g_dialog->Gate_IsScanning() ? 1 : 0, g_dialog->Gate_ServerCount());
		++step;
		break;
	}

	case 2:
	{
		// Zwei gültige CS-Retro-Server.
		const char *gamedir = OwnGameFolder();
		char info[512];
		snprintf(info, sizeof(info),
			"\\host\\CS Retro Alpha\\map\\de_dust\\numcl\\4\\maxcl\\16\\gamedir\\%s\\password\\0",
			gamedir);
		GateInject("127.0.0.1:27015", info);
		snprintf(info, sizeof(info),
			"\\host\\CS Retro Beta\\map\\cs_assault\\numcl\\1\\maxcl\\10\\gamedir\\%s\\password\\1",
			gamedir);
		GateInject("127.0.0.1:27016", info);
		Menu_Con("CSRETRO_BROWSER_GATE_VALID rows=%d", g_dialog->Gate_ServerCount());
		++step;
		break;
	}

	case 3:
	{
		// Gegenproben: fremder Mod, GoldSrc-/Steam-Antwort, Doppelantwort derselben
		// Adresse. Keine davon darf eine neue Zeile erzeugen.
		GateInject("127.0.0.1:27017",
			"\\host\\Fremder Mod\\map\\crossfire\\numcl\\2\\maxcl\\16\\gamedir\\valve\\password\\0");
		char info[512];
		snprintf(info, sizeof(info),
			"\\host\\Steam CS\\map\\de_dust2\\numcl\\9\\maxcl\\32\\gamedir\\%s\\password\\0\\gs\\1",
			OwnGameFolder());
		GateInject("127.0.0.1:27018", info);
		snprintf(info, sizeof(info),
			"\\host\\CS Retro Alpha\\map\\de_aztec\\numcl\\6\\maxcl\\16\\gamedir\\%s\\password\\0",
			OwnGameFolder());
		GateInject("127.0.0.1:27015", info);
		Menu_Con("CSRETRO_BROWSER_GATE_FILTER rows=%d", g_dialog->Gate_ServerCount());
		++step;
		break;
	}

	case 4:
		Menu_Con("CSRETRO_BROWSER_GATE_SELECT selected=%d connect=%d",
			g_dialog->Gate_SelectFirstRow() ? 1 : 0,
			g_dialog->Gate_ConnectEnabled() ? 1 : 0);
		++step;
		break;

	case 5:
		// Zweiter Scan: die Liste muss dabei leer starten, sonst wachsen Altlasten mit.
		g_dialog->Gate_Refresh();
		Menu_Con("CSRETRO_BROWSER_GATE_RESCAN rows=%d connect=%d",
			g_dialog->Gate_ServerCount(), g_dialog->Gate_ConnectEnabled() ? 1 : 0);
		++step;
		break;

	case 6:
		MenuEngine::ClientCmd("screenshot\n");
		Menu_Con("CSRETRO_BROWSER_GATE_SHOT_TAKEN");
		++step;
		break;

	case 7:
		UI_KeyEvent(K_ESCAPE, 1);
		UI_KeyEvent(K_ESCAPE, 0);
		++step;
		break;

	case 8:
		Menu_Con("CSRETRO_BROWSER_GATE_ESC visible=%d", ServerBrowser_IsActive() ? 1 : 0);
		Menu_Con("CSRETRO_BROWSER_GATE_DONE");
		// Sauber beenden statt vom Skript abschießen zu lassen: nur so prüft das Gate,
		// dass der Browser den geordneten Shutdown nicht kaputt macht.
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		step = 99;
		break;

	default:
		break;
	}
}
} // namespace

void ServerBrowser_RunFrame()
{
	// Scan-Fenster: Broadcast-Antworten kommen ohne Abschlussmeldung.
	if (g_dialog && g_dialog->IsVisible() && g_dialog->Gate_IsScanning())
		g_dialog->RunFrameScanWindow();

	GateTick();
}
