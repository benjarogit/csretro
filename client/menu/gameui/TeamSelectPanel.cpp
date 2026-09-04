#include "TeamSelectPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RichText.h>

#include "../src/menu_priv.h"
#include "cdll_dll.h"
#include "keydefs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <vector>

using namespace vgui2;

extern vgui2::ILocalize *g_pVGuiLocalize;

void UI_KeyEvent(int key, int down);

#ifndef KEY_DEST_GAME
#define KEY_DEST_GAME 1
#endif
#ifndef KEY_DEST_MENU
#define KEY_DEST_MENU 2
#endif

namespace
{
const char kResource[] = "resource/UI/Teammenu.res";

struct SlotBind
{
	const char *name;
	int slot;
};

const SlotBind kSlots[] = {
	{"terbutton", 1},
	{"ctbutton", 2},
	{"vipbutton", 3},
	{"autobutton", 5},
	{"specbutton", 6},
	{"CancelButton", 10},
};

int SlotBit(int slot)
{
	if (slot <= 0)
		return 0;
	if (slot == 10)
		return MENU_KEY_0;
	return 1 << (slot - 1);
}

void StripMapStem(char *stem, size_t stemSize, const char *raw)
{
	if (!stem || stemSize == 0)
		return;
	stem[0] = '\0';
	if (!raw || !raw[0])
		return;
	const char *name = raw;
	const char *slash = strrchr(raw, '/');
	if (!slash)
		slash = strrchr(raw, '\\');
	if (slash && slash[1])
		name = slash + 1;
	snprintf(stem, stemSize, "%s", name);
	char *dot = strrchr(stem, '.');
	if (dot)
		*dot = '\0';
}

class CTeamSelectPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CTeamSelectPanel, EditablePanel);

public:
	explicit CTeamSelectPanel(Panel *parent)
		: BaseClass(parent, "TeamMenu")
	{
		SetProportional(true);
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		if (scheme())
		{
			const HScheme client = scheme()->GetScheme("ClientScheme");
			if (client)
				SetScheme(client);
		}
		LoadControlSettings(kResource);
		StyleButtons();
	}

	bool HasTeamButtons()
	{
		return FindChildByName("terbutton") && FindChildByName("ctbutton");
	}

	void Open(int validSlots)
	{
		m_slots = validSlots;
		if (m_slots == 0)
			m_slots = MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_5 | MENU_KEY_6;
		ApplySlots();
		LoadMapBriefing();
		SetVisible(true);
		MoveToFront();
		RequestFocus();
		LogOpen();
	}

	void ApplySlots()
	{
		for (const SlotBind &bind : kSlots)
		{
			if (Panel *child = FindChildByName(bind.name))
				child->SetVisible((m_slots & SlotBit(bind.slot)) != 0);
		}
		RelayoutVisibleButtons();
	}

	bool ActivateSlot(int slot)
	{
		Button *btn = ButtonForSlot(slot);
		if (!btn || !btn->IsVisible() || !btn->IsEnabled())
			return false;
		btn->DoClick();
		return true;
	}

	int VisibleButtonCount()
	{
		int n = 0;
		for (const SlotBind &bind : kSlots)
		{
			Panel *child = FindChildByName(bind.name);
			if (child && child->IsVisible())
				++n;
		}
		return n;
	}

	int SlotMask() const { return m_slots; }

	bool LabelLooksLocalized(const char *name)
	{
		auto *label = dynamic_cast<Label *>(FindChildByName(name));
		if (!label)
			return false;
		wchar_t text[128] = {};
		label->GetText(text, sizeof(text));
		return text[0] != L'\0' && text[0] != L'#';
	}

	bool MapInfoHasText()
	{
		auto *info = dynamic_cast<RichText *>(FindChildByName("MapInfo"));
		if (!info)
			return false;
		wchar_t text[32] = {};
		info->GetText(0, text, sizeof(text));
		return text[0] != L'\0';
	}

	Panel *CreateControlByName(const char *controlName) override
	{
		// Steam-Teammenu nutzt HTML für die Kartenbeschreibung. Wir haben
		// keinen HTML-Browser — RichText liest dieselbe maps/*.txt.
		if (controlName && !strcasecmp(controlName, "HTML"))
			return new RichText(nullptr, nullptr);
		return BaseClass::CreateControlByName(controlName);
	}

	void OnCommand(const char *command) override
	{
		if (!command || !command[0])
			return;
		if (!strcasecmp(command, "vguicancel"))
		{
			Menu_Con("CSRETRO_TEAM_CMD vguicancel");
			TeamSelect_Hide();
			return;
		}
		if (!strncasecmp(command, "jointeam ", 9) || !strcasecmp(command, "spectate"))
		{
			Menu_Con("CSRETRO_TEAM_CMD %s", command);
			if (!strncasecmp(command, "jointeam ", 9))
				Menu_NotePlayerTeam(atoi(command + 9));
			char buf[64];
			snprintf(buf, sizeof(buf), "%s\n", command);
			MenuEngine::ClientCmdNow(buf);
			TeamSelect_Hide();
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_ESCAPE)
		{
			TeamSelect_Hide();
			return;
		}
		if (code == KEY_0)
		{
			ActivateSlot(10);
			return;
		}
		if (code >= KEY_1 && code <= KEY_9)
		{
			ActivateSlot(code - KEY_0);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

private:
	int m_slots = 0;

	Button *ButtonForSlot(int slot)
	{
		for (const SlotBind &bind : kSlots)
		{
			if (bind.slot != slot)
				continue;
			return dynamic_cast<Button *>(FindChildByName(bind.name));
		}
		return nullptr;
	}

	void StyleButtons()
	{
		IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
		const Color fg = GetSchemeColor("BrightControlText", Color(255, 176, 0, 255), sch);
		const Color armed = GetSchemeColor("BrightBaseText", Color(255, 220, 80, 255), sch);
		const Color bg(0, 0, 0, 0);
		const Color armedBg = GetSchemeColor("SelectionBG", Color(255, 176, 0, 100), sch);

		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn)
				continue;
			btn->SetPaintBackgroundEnabled(true);
			btn->SetDefaultColor(fg, bg);
			btn->SetArmedColor(armed, armedBg);
			btn->SetDepressedColor(armed, armedBg);
			btn->SetDefaultBorder(nullptr);
			btn->SetDepressedBorder(nullptr);
			btn->SetKeyFocusBorder(nullptr);
			btn->SetContentAlignment(Label::a_west);
			btn->SetButtonActivationType(Button::ACTIVATE_ONPRESSED);
		}

		if (auto *title = dynamic_cast<Label *>(FindChildByName("joinTeam")))
			title->SetFgColor(fg);
	}

	void RelayoutVisibleButtons()
	{
		std::vector<Panel *> visible;
		visible.reserve(6);
		for (const SlotBind &bind : kSlots)
		{
			Panel *child = FindChildByName(bind.name);
			if (child && child->IsVisible())
				visible.push_back(child);
		}
		if (visible.empty())
			return;
		int x = 0, y = 0, w = 0, h = 0;
		visible[0]->GetBounds(x, y, w, h);
		int gap = 12;
		if (IsProportional() && scheme())
			gap = scheme()->GetProportionalScaledValue(12);
		const int step = h > 0 ? h + gap : 32;
		for (size_t i = 0; i < visible.size(); ++i)
		{
			int cx = 0, cy = 0, cw = 0, ch = 0;
			visible[i]->GetBounds(cx, cy, cw, ch);
			visible[i]->SetPos(cx, y + static_cast<int>(i) * step);
		}
	}

	void LoadMapBriefing()
	{
		auto *info = dynamic_cast<RichText *>(FindChildByName("MapInfo"));
		if (!info)
			return;
		info->SetPanelInteractive(false);
		info->SetUnusedScrollbarInvisible(true);
		IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
		info->SetFgColor(GetSchemeColor("MapDescriptionText", Color(255, 176, 0, 255), sch));
		info->SetBgColor(GetSchemeColor("WindowBG", Color(0, 0, 0, 200), sch));
		if (HFont font = sch ? sch->GetFont("Default", IsProportional()) : INVALID_FONT)
		{
			if (font != INVALID_FONT)
				info->SetFont(font);
		}

		char stem[64];
		StripMapStem(stem, sizeof(stem), MenuEngine::GetCvarString("hostmap"));
		char path[96] = {};
		if (stem[0])
			snprintf(path, sizeof(path), "maps/%s.txt", stem);

		int len = 0;
		byte *raw = (path[0] && gEng.COM_LoadFile) ? gEng.COM_LoadFile(path, &len) : nullptr;
		if (raw && len > 0)
		{
			std::string text(reinterpret_cast<char *>(raw), static_cast<size_t>(len));
			gEng.COM_FreeFile(raw);
			info->SetText(text.c_str());
			if (Label *mapName = dynamic_cast<Label *>(FindChildByName("mapname")))
			{
				mapName->SetText(stem);
				mapName->SetVisible(true);
			}
			Menu_Con("CSRETRO_TEAM_MAPINFO map=%s bytes=%d", stem, len);
			return;
		}
		if (raw)
			gEng.COM_FreeFile(raw);

		const wchar_t *missing =
			g_pVGuiLocalize ? g_pVGuiLocalize->Find("#Map_Description_not_available") : nullptr;
		if (missing && missing[0])
			info->SetText(missing);
		else
			info->SetText("");
		Menu_Con("CSRETRO_TEAM_MAPINFO map=%s bytes=0", stem[0] ? stem : "-");
	}

	void LogOpen()
	{
		int vip = 0, spec = 0, cancel = 0, t = 0, ct = 0, autoas = 0;
		if (Panel *p = FindChildByName("terbutton"))
			t = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName("ctbutton"))
			ct = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName("autobutton"))
			autoas = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName("vipbutton"))
			vip = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName("specbutton"))
			spec = p->IsVisible() ? 1 : 0;
		if (Panel *p = FindChildByName("CancelButton"))
			cancel = p->IsVisible() ? 1 : 0;
		Menu_Con("CSRetro-VGUI: %s (%d)", kResource, MENU_TEAM);
		Menu_Con("CSRETRO_TEAM_VGUI open slots=%d t=%d ct=%d auto=%d vip=%d spec=%d cancel=%d",
			m_slots, t, ct, autoas, vip, spec, cancel);
	}
};

class CTeamSelectOverlay : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CTeamSelectOverlay, Panel);

public:
	explicit CTeamSelectOverlay(Panel *parent)
		: BaseClass(parent, "TeamSelectOverlay")
	{
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
		if (scheme())
		{
			const HScheme client = scheme()->GetScheme("ClientScheme");
			if (client)
				SetScheme(client);
		}
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetBgColor(GetSchemeColor("ViewportBG", Color(0, 0, 0, 200), pScheme));
	}

	void PerformLayout() override
	{
		if (Panel *p = GetParent())
		{
			int w = 0, h = 0;
			p->GetSize(w, h);
			SetBounds(0, 0, w, h);
		}
		BaseClass::PerformLayout();
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (m_team)
		{
			m_team->OnKeyCodeTyped(code);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

	CTeamSelectPanel *m_team = nullptr;
};

CTeamSelectOverlay *g_overlay = nullptr;
CTeamSelectPanel *g_panel = nullptr;
Panel *g_host = nullptr;
bool g_keyDestPushed = false;
} // namespace

bool TeamSelect_Show(Panel *root, int validSlots)
{
	if (!g_overlay)
	{
		if (!root)
			return false;
		g_overlay = new CTeamSelectOverlay(root);
		g_panel = new CTeamSelectPanel(g_overlay);
		g_overlay->m_team = g_panel;
		g_host = root;
		if (!g_panel->HasTeamButtons())
		{
			Menu_Con("CSRETRO_TEAM_VGUI fail %s", kResource);
			delete g_overlay;
			g_overlay = nullptr;
			g_panel = nullptr;
			g_host = nullptr;
			return false;
		}
	}
	else if (root)
	{
		g_overlay->SetParent(root);
		g_host = root;
	}

	Panel *host = g_overlay->GetParent();
	if (!host)
		host = g_host;
	if (!host)
		return false;
	if (g_overlay->GetParent() != host)
		g_overlay->SetParent(host);
	int w = 0, h = 0;
	host->GetSize(w, h);
	g_overlay->SetBounds(0, 0, w, h);
	g_overlay->SetVisible(true);
	g_overlay->MoveToFront();
	g_panel->Open(validSlots);
	g_overlay->RequestFocus();
	if (gEng.pfnSetKeyDest)
	{
		gEng.pfnSetKeyDest(KEY_DEST_MENU);
		g_keyDestPushed = true;
	}
	return true;
}

void TeamSelect_Hide()
{
	if (g_overlay && g_overlay->IsVisible())
		Menu_Con("CSRETRO_TEAM_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
	if (g_overlay)
		g_overlay->SetVisible(false);
	if (g_keyDestPushed && !gMenuVisible && !ClassSelect_IsActive() && !BuySelect_IsActive())
	{
		if (gEng.pfnSetKeyDest)
			gEng.pfnSetKeyDest(KEY_DEST_GAME);
		g_keyDestPushed = false;
	}
}

bool TeamSelect_IsActive()
{
	return g_overlay && g_overlay->IsVisible() && g_panel && g_panel->IsVisible();
}

bool TeamSelect_ActivateSlot(int slot)
{
	return g_panel && g_panel->IsVisible() && g_panel->ActivateSlot(slot);
}

void TeamSelect_Shutdown()
{
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
	g_keyDestPushed = false;
}

void TeamSelect_GateTick()
{
	if (!getenv("CSRETRO_TEAM_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	if (step >= 99)
		return;

	if (step == 0)
	{
		if (!TeamSelect_IsActive())
			return;
		++hold;
		if (hold < 45)
			return;
		Menu_Con("CSRETRO_TEAM_GATE_OPEN visible=1 buttons=%d slots=%d mapinfo=%d title=%d t=%d ct=%d",
			g_panel->VisibleButtonCount(), g_panel->SlotMask(),
			g_panel->MapInfoHasText() ? 1 : 0,
			g_panel->LabelLooksLocalized("joinTeam") ? 1 : 0,
			g_panel->LabelLooksLocalized("terbutton") ? 1 : 0,
			g_panel->LabelLooksLocalized("ctbutton") ? 1 : 0);
		if (g_pVGuiLocalize)
		{
			const char *probes[] = {
				"Cstrike_Join_Team",
				"Cstrike_Terrorist_Forces",
				"Cstrike_CT_Forces",
				"Cstrike_Team_AutoAssign",
			};
			for (const char *tok : probes)
			{
				wchar_t *w = g_pVGuiLocalize->Find(tok);
				if (!w || !w[0])
					Menu_Con("CSRETRO_LOC_MISSING %s", tok);
			}
		}
		MenuEngine::ClientCmd("screenshot\n");
		++step;
		hold = 0;
		return;
	}

	if (step == 1)
	{
		++hold;
		if (hold < 20)
			return;
		UI_KeyEvent(K_ESCAPE, 1);
		UI_KeyEvent(K_ESCAPE, 0);
		Menu_Con("CSRETRO_TEAM_GATE_ESC visible=%d", TeamSelect_IsActive() ? 1 : 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 2)
	{
		if (!g_overlay || !g_panel)
		{
			Menu_Con("CSRETRO_TEAM_GATE_FAIL panel weg");
			Menu_Con("CSRETRO_TEAM_GATE_DONE");
			step = 99;
			return;
		}
		Panel *root = g_overlay->GetParent();
		if (!TeamSelect_Show(root, g_panel->SlotMask()))
		{
			Menu_Con("CSRETRO_TEAM_GATE_FAIL reopen");
			Menu_Con("CSRETRO_TEAM_GATE_DONE");
			step = 99;
			return;
		}
		++step;
		hold = 0;
		return;
	}

	if (step == 3)
	{
		++hold;
		if (hold < 15)
			return;
		UI_KeyEvent('2', 1);
		UI_KeyEvent('2', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 4)
	{
		++hold;
		if (hold < 20)
			return;
		Menu_Con("CSRETRO_TEAM_GATE_JOIN visible=%d", TeamSelect_IsActive() ? 1 : 0);
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_TEAM_GATE_DONE");
		step = 99;
	}
}
