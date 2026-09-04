#include "BuySelectPanel.h"

#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "TeamSelectPanel.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Divider.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>

#include "../src/menu_priv.h"
#include "cdll_dll.h"
#include "keydefs.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
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
struct SlotBind
{
	const char *name;
	int slot;
};

const SlotBind kMainSlots[] = {
	{"pistols", 1},
	{"shotguns", 2},
	{"submachineguns", 3},
	{"rifles", 4},
	{"machineguns", 5},
	{"primaryammo", 6},
	{"secammo", 7},
	{"equipment", 8},
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

bool ResLooksLoaded(const char *path)
{
	int len = 0;
	byte *raw = (path && path[0] && gEng.COM_LoadFile) ? gEng.COM_LoadFile(path, &len) : nullptr;
	const bool ok = raw && len > 0;
	if (raw)
		gEng.COM_FreeFile(raw);
	return ok;
}

bool ResolveBuyRes(const char *cmd, int team, char *out, size_t outsz)
{
	if (!cmd || !out || outsz == 0)
		return false;
	char path[256];
	snprintf(path, sizeof(path), "%s", cmd);
	for (char *p = path; *p; ++p)
	{
		if (*p == '\\')
			*p = '/';
	}
	if (!strncasecmp(path, "Resource/", 9))
		path[0] = 'r';

	if (ResLooksLoaded(path))
	{
		snprintf(out, outsz, "%s", path);
		return true;
	}

	char stem[256];
	snprintf(stem, sizeof(stem), "%s", path);
	char *dot = strstr(stem, ".res");
	if (dot)
		*dot = '\0';

	const char *suffix = (team == TEAM_CT) ? "_CT.res" : "_TER.res";
	char tryPath[280];
	snprintf(tryPath, sizeof(tryPath), "%s%s", stem, suffix);
	if (ResLooksLoaded(tryPath))
	{
		snprintf(out, outsz, "%s", tryPath);
		return true;
	}

	char *guns = strstr(stem, "MachineGuns");
	if (guns)
	{
		memcpy(guns, "Machineguns", 11);
		snprintf(tryPath, sizeof(tryPath), "%s%s", stem, suffix);
		if (ResLooksLoaded(tryPath))
		{
			snprintf(out, outsz, "%s", tryPath);
			return true;
		}
	}

	const char *other = (team == TEAM_CT) ? "_TER.res" : "_CT.res";
	snprintf(tryPath, sizeof(tryPath), "%s%s", stem, other);
	if (ResLooksLoaded(tryPath))
	{
		snprintf(out, outsz, "%s", tryPath);
		return true;
	}
	return false;
}

const char *ResForType(int menuType, int team)
{
	const bool ct = (team == TEAM_CT);
	switch (menuType)
	{
	case MENU_BUY:
		return "resource/UI/MainBuyMenu.res";
	case MENU_BUY_PISTOL:
		return ct ? "resource/UI/BuyPistols_CT.res" : "resource/UI/BuyPistols_TER.res";
	case MENU_BUY_SHOTGUN:
		return ct ? "resource/UI/BuyShotguns_CT.res" : "resource/UI/BuyShotguns_TER.res";
	case MENU_BUY_RIFLE:
		return ct ? "resource/UI/BuyRifles_CT.res" : "resource/UI/BuyRifles_TER.res";
	case MENU_BUY_SUBMACHINEGUN:
		return ct ? "resource/UI/BuySubMachineguns_CT.res" : "resource/UI/BuySubMachineguns_TER.res";
	case MENU_BUY_MACHINEGUN:
		return ct ? "resource/UI/BuyMachineguns_CT.res" : "resource/UI/BuyMachineguns_TER.res";
	case MENU_BUY_ITEM:
		return ct ? "resource/UI/BuyEquipment_CT.res" : "resource/UI/BuyEquipment_TER.res";
	default:
		return nullptr;
	}
}

void MapSteamBuyAlias(char *out, size_t outsz, const char *cmd)
{
	if (!out || outsz == 0)
		return;
	if (!cmd || !cmd[0])
	{
		out[0] = '\0';
		return;
	}
	// Steam-.res sagt autobuy/rebuy; ReGameDLL hört auf cl_autobuy / cl_rebuy.
	if (!strcasecmp(cmd, "autobuy"))
		snprintf(out, outsz, "cl_autobuy");
	else if (!strcasecmp(cmd, "rebuy"))
		snprintf(out, outsz, "cl_rebuy");
	else
		snprintf(out, outsz, "%s", cmd);
}

bool IsResCommand(const char *cmd)
{
	return cmd && (strstr(cmd, ".res") || strstr(cmd, ".RES"));
}

class CBuySelectPanel;

class CBuyHoverButton : public Button
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuyHoverButton, Button);

public:
	CBuyHoverButton(Panel *parent, const char *name)
		: BaseClass(parent, name, "")
	{
	}

	void SetHost(CBuySelectPanel *host) { m_host = host; }
	void SetPreviewName(const char *name) { m_preview = name ? name : ""; }

	void ApplySettings(KeyValues *inResourceData) override
	{
		BaseClass::ApplySettings(inResourceData);
		// Steam-Buy-`.res` schreibt "Command" (groß). KeyValues-Symbole
		// sind bei uns case-sensitiv — Button::ApplySettings sucht "command".
		const char *cmd = inResourceData->GetString("command", "");
		if (!cmd[0])
			cmd = inResourceData->GetString("Command", "");
		if (cmd[0])
			SetCommand(cmd);
	}

	void OnCursorEntered() override;

private:
	CBuySelectPanel *m_host = nullptr;
	std::string m_preview;
};

class CBuySelectPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuySelectPanel, EditablePanel);

public:
	CBuySelectPanel(Panel *parent, int menuType, int team)
		: BaseClass(parent, "BuyMenu")
		, m_type(menuType)
		, m_team(team)
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
	}

	bool LoadPage(int menuType, const char *resOverride, int validSlots)
	{
		while (GetChildCount() > 0)
			delete GetChild(0);
		m_preview = nullptr;
		m_type = menuType;
		m_pageIsMain = (menuType == MENU_BUY && (!resOverride || !resOverride[0]));

		const char *res = resOverride;
		char resolved[280] = {};
		if (!res || !res[0])
			res = ResForType(menuType, m_team);
		if (!res)
			return false;
		if (IsResCommand(res) && ResolveBuyRes(res, m_team, resolved, sizeof(resolved)))
			res = resolved;
		snprintf(m_res, sizeof(m_res), "%s", res);
		LoadControlSettings(res);
		ApplySteamCommands();
		ApplyBuyLabels();
		BlankRawTokens();
		StyleButtons();
		BindHover();
		m_pageIsMain = FindChildByName("pistols") != nullptr;
		// Steam-MainBuyMenu.res hat kein BuyMenu/Frame mit wide/tall (anders
		// als TeamMenu/ClassMenu). Default-Panel ist 64×24 — Kinder bei ypos
		// 116+ werden geclippt: Overlay-Dim ohne Buttons. Kinder stehen in
		// Viewport-Koordinaten, also Panel = Overlay (Ref B: Buy-Wizard).
		FitToParent();
		Open(validSlots);
		return HasBuyButtons();
	}

	bool HasBuyButtons()
	{
		if (m_pageIsMain)
			return FindChildByName("pistols") && FindChildByName("shotguns");
		int n = 0;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			if (dynamic_cast<Button *>(GetChild(i)))
				++n;
		}
		return n >= 2;
	}

	void Open(int validSlots)
	{
		FitToParent();
		m_slots = validSlots;
		if (m_slots == 0)
			m_slots = MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_3 | MENU_KEY_4 | MENU_KEY_5 |
				  MENU_KEY_6 | MENU_KEY_7 | MENU_KEY_8 | MENU_KEY_0;
		ApplySlots();
		SetVisible(true);
		MoveToFront();
		RequestFocus();
		LogOpen();
	}

	void FitToParent()
	{
		Panel *p = GetParent();
		if (!p)
			return;
		int w = 0, h = 0;
		p->GetSize(w, h);
		if (w < 1 || h < 1)
			return;
		SetBounds(0, 0, w, h);
	}

	bool ButtonsOnPanel()
	{
		const int pw = GetWide();
		const int ph = GetTall();
		if (pw < 400 || ph < 300)
			return false;
		int checked = 0;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn || !btn->IsVisible())
				continue;
			int x = 0, y = 0, w = 0, h = 0;
			btn->GetBounds(x, y, w, h);
			if (x < 0 || y < 0 || x + w > pw || y + h > ph)
				return false;
			++checked;
		}
		return checked >= 2;
	}

	void ApplySlots()
	{
		if (m_pageIsMain)
		{
			for (const SlotBind &bind : kMainSlots)
			{
				if (Panel *child = FindChildByName(bind.name))
					child->SetVisible((m_slots & SlotBit(bind.slot)) != 0);
			}
			if (Panel *autoBuy = FindChildByName("AutobuyButton"))
				autoBuy->SetVisible(true);
			if (Panel *rebuy = FindChildByName("RebuyButton"))
				rebuy->SetVisible(true);
			return;
		}

		std::vector<Button *> weapons;
		Button *cancel = nullptr;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn)
				continue;
			const char *name = btn->GetName();
			if (name && (!strcasecmp(name, "CancelButton") || !strcasecmp(name, "cancelbutton")))
			{
				cancel = btn;
				continue;
			}
			weapons.push_back(btn);
		}
		std::sort(weapons.begin(), weapons.end(), [](Button *a, Button *b) {
			int ax = 0, ay = 0, bx = 0, by = 0;
			a->GetPos(ax, ay);
			b->GetPos(bx, by);
			return ay < by;
		});
		for (size_t i = 0; i < weapons.size(); ++i)
		{
			const int slot = static_cast<int>(i) + 1;
			weapons[i]->SetVisible((m_slots & SlotBit(slot)) != 0);
		}
		if (cancel)
			cancel->SetVisible(true);
	}

	bool ActivateSlot(int slot)
	{
		Button *btn = ButtonForSlot(slot);
		if (!btn || !btn->IsVisible() || !btn->IsEnabled())
			return false;
		const char *cmd = "";
		if (KeyValues *kv = btn->GetCommand())
			cmd = kv->GetString("command", "");
		Menu_Con("CSRETRO_BUY_SLOT %d %s", slot, (cmd && cmd[0]) ? cmd : "-");
		if (cmd && cmd[0])
		{
			OnCommand(cmd);
			ApplyPendingPage();
		}
		else
			btn->DoClick();
		return true;
	}

	int VisibleButtonCount()
	{
		int n = 0;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (btn && btn->IsVisible())
				++n;
		}
		return n;
	}

	int SlotMask() const { return m_slots; }
	int MenuType() const { return m_type; }
	int Team() const { return m_team; }
	bool IsMainPage() const { return m_pageIsMain; }
	const char *ResPath() const { return m_res; }

	void ApplyPendingPage()
	{
		if (!m_pending)
			return;
		char res[280];
		snprintf(res, sizeof(res), "%s", m_pendingRes);
		const int type = m_pendingType;
		const int slots = m_pendingSlots;
		m_pending = false;
		m_pendingRes[0] = '\0';
		LoadPage(type, res[0] ? res : nullptr, slots);
	}

	bool LabelLooksLocalized(const char *name)
	{
		auto *label = dynamic_cast<Label *>(FindChildByName(name));
		if (!label)
			return false;
		wchar_t text[128] = {};
		label->GetText(text, sizeof(text));
		if (text[0] == L'\0' || text[0] == L'#')
			return false;
		return wcsstr(text, L"Cstrike_") == nullptr;
	}

	bool AnyRawToken()
	{
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *label = dynamic_cast<Label *>(GetChild(i));
			if (!label || !label->IsVisible())
				continue;
			wchar_t text[128] = {};
			label->GetText(text, sizeof(text));
			if (text[0] == L'#' || wcsstr(text, L"Cstrike_") != nullptr)
				return true;
		}
		return false;
	}

	void ShowItemPreview(const char *imageName)
	{
		if (!imageName || !imageName[0] || !m_preview)
			return;
		char path[96];
		snprintf(path, sizeof(path), "gfx/vgui/%s", imageName);
		m_preview->SetImage(path);
		if (Panel *info = FindChildByName("ItemInfo"))
		{
			info->SetVisible(true);
			info->SetEnabled(true);
		}
		m_preview->SetVisible(true);
	}

	Panel *CreateControlByName(const char *controlName) override
	{
		if (controlName && !strcasecmp(controlName, "MouseOverPanelButton"))
			return new CBuyHoverButton(nullptr, nullptr);
		if (controlName && (!strcasecmp(controlName, "WizardSubPanel") ||
				    !strcasecmp(controlName, "WizardPanel") ||
				    !strcasecmp(controlName, "HTML")))
			return new Panel(nullptr, nullptr);
		return BaseClass::CreateControlByName(controlName);
	}

	void OnCommand(const char *command) override
	{
		if (!command || !command[0])
			return;
		if (!strcasecmp(command, "vguicancel"))
		{
			Menu_Con("CSRETRO_BUY_CMD vguicancel");
			if (!m_pageIsMain)
			{
				QueuePage(MENU_BUY, nullptr, MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_3 |
								     MENU_KEY_4 | MENU_KEY_5 | MENU_KEY_6 |
								     MENU_KEY_7 | MENU_KEY_8 | MENU_KEY_0);
				return;
			}
			BuySelect_Hide();
			return;
		}
		if (IsResCommand(command))
		{
			char resolved[280] = {};
			if (!ResolveBuyRes(command, m_team, resolved, sizeof(resolved)))
			{
				Menu_Con("CSRETRO_BUY_VGUI fail %s", command);
				return;
			}
			Menu_Con("CSRETRO_BUY_PAGE %s", resolved);
			int subType = MENU_BUY;
			if (strstr(resolved, "Pistol") || strstr(resolved, "pistol"))
				subType = MENU_BUY_PISTOL;
			else if (strstr(resolved, "Shotgun") || strstr(resolved, "shotgun"))
				subType = MENU_BUY_SHOTGUN;
			else if (strstr(resolved, "SubMachine") || strstr(resolved, "Submachine"))
				subType = MENU_BUY_SUBMACHINEGUN;
			else if (strstr(resolved, "Rifle") || strstr(resolved, "rifle"))
				subType = MENU_BUY_RIFLE;
			else if (strstr(resolved, "Machinegun") || strstr(resolved, "MachineGun"))
				subType = MENU_BUY_MACHINEGUN;
			else if (strstr(resolved, "Equipment") || strstr(resolved, "equipment"))
				subType = MENU_BUY_ITEM;
			QueuePage(subType, resolved, 0x3FF);
			return;
		}

		char mapped[64];
		MapSteamBuyAlias(mapped, sizeof(mapped), command);
		Menu_Con("CSRETRO_BUY_CMD %s", mapped);
		char buf[80];
		snprintf(buf, sizeof(buf), "%s\n", mapped);
		MenuEngine::ClientCmdNow(buf);
		return;
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_ESCAPE)
		{
			BuySelect_Hide();
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

	void PerformLayout() override
	{
		FitToParent();
		BaseClass::PerformLayout();
	}

private:
	int m_type = MENU_BUY;
	int m_team = TEAM_TERRORIST;
	int m_slots = 0;
	bool m_pageIsMain = true;
	char m_res[128] = {};
	char m_pendingRes[280] = {};
	int m_pendingType = MENU_BUY;
	int m_pendingSlots = 0;
	bool m_pending = false;
	ImagePanel *m_preview = nullptr;

	void QueuePage(int menuType, const char *res, int validSlots)
	{
		m_pendingType = menuType;
		m_pendingSlots = validSlots;
		if (res && res[0])
			snprintf(m_pendingRes, sizeof(m_pendingRes), "%s", res);
		else
			m_pendingRes[0] = '\0';
		m_pending = true;
	}

	void ApplySteamCommands()
	{
		// Steam-MainBuyMenu nutzt "Command", Waffen-Unterseiten "command".
		// Menu_LoadRes liest beides (wie der Interim-Parser).
		const std::vector<ResField> fields = Menu_LoadRes(m_res);
		for (const ResField &field : fields)
		{
			if (field.command.empty() || field.name.empty())
				continue;
			auto *btn = dynamic_cast<Button *>(FindChildByName(field.name.c_str()));
			if (btn)
				btn->SetCommand(field.command.c_str());
		}
	}

	Button *ButtonForSlot(int slot)
	{
		if (m_pageIsMain)
		{
			for (const SlotBind &bind : kMainSlots)
			{
				if (bind.slot != slot)
					continue;
				return dynamic_cast<Button *>(FindChildByName(bind.name));
			}
			return nullptr;
		}
		if (slot == 10)
		{
			if (auto *btn = dynamic_cast<Button *>(FindChildByName("CancelButton")))
				return btn;
			return dynamic_cast<Button *>(FindChildByName("cancelbutton"));
		}
		std::vector<Button *> weapons;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn || !btn->IsVisible())
				continue;
			const char *name = btn->GetName();
			if (name && (!strcasecmp(name, "CancelButton") || !strcasecmp(name, "cancelbutton")))
				continue;
			weapons.push_back(btn);
		}
		std::sort(weapons.begin(), weapons.end(), [](Button *a, Button *b) {
			int ax = 0, ay = 0, bx = 0, by = 0;
			a->GetPos(ax, ay);
			b->GetPos(bx, by);
			return ay < by;
		});
		if (slot < 1 || slot > static_cast<int>(weapons.size()))
			return nullptr;
		return weapons[static_cast<size_t>(slot - 1)];
	}

	void ApplyBuyLabels()
	{
		struct LabelFix
		{
			const char *name;
			const char *token;
		};
		const LabelFix fixes[] = {
			{"Title", nullptr},
			{"selectCategory", "#Cstrike_Select_Category"},
			{"pistols", "#Cstrike_Pistols"},
			{"shotguns", "#Cstrike_Shotguns"},
			{"submachineguns", "#Cstrike_SubMachineGuns"},
			{"rifles", "#Cstrike_Rifles"},
			{"machineguns", "#Cstrike_MachineGuns"},
			{"primaryammo", "#Cstrike_Prim_Ammo"},
			{"secammo", "#Cstrike_Sec_Ammo"},
			{"equipment", "#Cstrike_Equipment"},
			{"CancelButton", "#Cstrike_Cancel"},
			{"AutobuyButton", "#Cstrike_BuyMenuAutobuy"},
			{"RebuyButton", "#Cstrike_BuyMenuRebuy"},
		};
		if (auto *title = dynamic_cast<Label *>(FindChildByName("Title")))
		{
			if (m_pageIsMain || FindChildByName("pistols"))
				title->SetText("#Cstrike_Buy_Menu");
		}
		for (const LabelFix &fix : fixes)
		{
			if (!fix.token)
				continue;
			auto *label = dynamic_cast<Label *>(FindChildByName(fix.name));
			if (label)
				label->SetText(fix.token);
		}
	}

	void BlankRawTokens()
	{
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *label = dynamic_cast<Label *>(GetChild(i));
			if (!label)
				continue;
			wchar_t text[128] = {};
			label->GetText(text, sizeof(text));
			if (text[0] == L'#')
				label->SetText("");
		}
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

		if (auto *title = dynamic_cast<Label *>(FindChildByName("Title")))
			title->SetFgColor(fg);
		if (auto *cat = dynamic_cast<Label *>(FindChildByName("selectCategory")))
			cat->SetFgColor(fg);
	}

	void BindHover()
	{
		Panel *info = FindChildByName("ItemInfo");
		if (info)
		{
			int x = 0, y = 0, w = 0, h = 0;
			info->GetBounds(x, y, w, h);
			if (x < 640 && w > 0 && w < 800)
			{
				info->SetPaintBackgroundEnabled(true);
				info->SetMouseInputEnabled(false);
				IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
				info->SetBgColor(GetSchemeColor("WindowBG", Color(0, 0, 0, 200), sch));
				int pad = 8;
				if (IsProportional() && scheme())
					pad = scheme()->GetProportionalScaledValue(8);
				int iw = w > pad * 2 ? w - pad * 2 : w;
				int maxSide = 256;
				if (IsProportional() && scheme())
					maxSide = scheme()->GetProportionalScaledValue(256);
				if (iw > maxSide)
					iw = maxSide;
				m_preview = new ImagePanel(info, "ItemPreview");
				m_preview->SetBounds(pad, pad, iw, iw);
				m_preview->SetShouldScaleImage(true);
				m_preview->SetVisible(false);
			}
		}

		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<CBuyHoverButton *>(GetChild(i));
			if (!btn)
				continue;
			btn->SetHost(this);
			const char *name = btn->GetName();
			if (name && name[0] && strcasecmp(name, "CancelButton") &&
			    strcasecmp(name, "AutobuyButton") && strcasecmp(name, "RebuyButton"))
				btn->SetPreviewName(name);
		}
	}

	void LogOpen()
	{
		int px = 0, py = 0, pw = 0, ph = 0;
		GetBounds(px, py, pw, ph);
		int bx = -1, by = -1, bw = 0, bh = 0;
		if (Panel *pistols = FindChildByName("pistols"))
			pistols->GetBounds(bx, by, bw, bh);
		else if (Panel *glock = FindChildByName("Glock18"))
			glock->GetBounds(bx, by, bw, bh);
		Menu_Con("CSRetro-VGUI: %s (%d)", m_res, m_type);
		Menu_Con("CSRETRO_BUY_VGUI open type=%d team=%d main=%d buttons=%d slots=%d",
			m_type, m_team, m_pageIsMain ? 1 : 0, VisibleButtonCount(), m_slots);
		Menu_Con("CSRETRO_BUY_LAYOUT panel=%d,%d %dx%d child=%d,%d %dx%d fit=%d",
			px, py, pw, ph, bx, by, bw, bh, ButtonsOnPanel() ? 1 : 0);
	}
};

void CBuyHoverButton::OnCursorEntered()
{
	BaseClass::OnCursorEntered();
	if (m_host && !m_preview.empty())
		m_host->ShowItemPreview(m_preview.c_str());
}

class CBuySelectOverlay : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuySelectOverlay, Panel);

public:
	explicit CBuySelectOverlay(Panel *parent)
		: BaseClass(parent, "BuySelectOverlay")
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
		if (m_buy)
		{
			int w = 0, h = 0;
			GetSize(w, h);
			m_buy->SetBounds(0, 0, w, h);
		}
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (m_buy)
		{
			m_buy->OnKeyCodeTyped(code);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

	CBuySelectPanel *m_buy = nullptr;
};

CBuySelectOverlay *g_overlay = nullptr;
CBuySelectPanel *g_panel = nullptr;
Panel *g_host = nullptr;
bool g_keyDestPushed = false;
} // namespace

static void DestroyBuyUi()
{
	delete g_overlay;
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
}

bool BuySelect_Show(Panel *root, int menuType, int validSlots)
{
	if (menuType < MENU_BUY || menuType > MENU_BUY_ITEM)
		return false;

	int team = Menu_LastPlayerTeam();
	if (team != TEAM_TERRORIST && team != TEAM_CT)
	{
		const int cls = ClassSelect_MenuType();
		if (cls == MENU_CLASS_CT)
			team = TEAM_CT;
		else if (cls == MENU_CLASS_T)
			team = TEAM_TERRORIST;
	}
	if (team != TEAM_TERRORIST && team != TEAM_CT)
		team = TEAM_TERRORIST;

	if (!g_overlay)
	{
		if (!root)
			return false;
		g_overlay = new CBuySelectOverlay(root);
		g_panel = new CBuySelectPanel(g_overlay, menuType, team);
		g_overlay->m_buy = g_panel;
		g_host = root;
		if (!g_panel->LoadPage(menuType, nullptr, validSlots))
		{
			Menu_Con("CSRETRO_BUY_VGUI fail %s", ResForType(menuType, team));
			DestroyBuyUi();
			return false;
		}
	}
	else
	{
		if (root)
		{
			g_overlay->SetParent(root);
			g_host = root;
		}
		if (!g_panel->LoadPage(menuType, nullptr, validSlots))
		{
			Menu_Con("CSRETRO_BUY_VGUI fail reload %s", ResForType(menuType, team));
			return false;
		}
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

void BuySelect_Hide()
{
	if (g_overlay && g_overlay->IsVisible())
		Menu_Con("CSRETRO_BUY_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
	if (g_overlay)
		g_overlay->SetVisible(false);
	if (g_keyDestPushed && !gMenuVisible && !TeamSelect_IsActive() && !ClassSelect_IsActive())
	{
		if (gEng.pfnSetKeyDest)
			gEng.pfnSetKeyDest(KEY_DEST_GAME);
		g_keyDestPushed = false;
	}
}

bool BuySelect_IsActive()
{
	return g_overlay && g_overlay->IsVisible() && g_panel && g_panel->IsVisible();
}

bool BuySelect_ActivateSlot(int slot)
{
	return g_panel && g_panel->IsVisible() && g_panel->ActivateSlot(slot);
}

int BuySelect_MenuType()
{
	return g_panel ? g_panel->MenuType() : 0;
}

void BuySelect_Shutdown()
{
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
	g_keyDestPushed = false;
}

void BuySelect_AfterFrame()
{
	if (g_panel && g_panel->IsVisible())
		g_panel->ApplyPendingPage();
}

void BuySelect_GateTick()
{
	if (!getenv("CSRETRO_BUY_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	if (step >= 99)
		return;

	auto failDone = [](const char *why) {
		Menu_Con("CSRETRO_BUY_GATE_FAIL %s", why);
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_BUY_GATE_DONE");
		step = 99;
	};

	if (step == 0)
	{
		if (!TeamSelect_IsActive())
			return;
		++hold;
		if (hold < 20)
			return;
		UI_KeyEvent('1', 1);
		UI_KeyEvent('1', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 1)
	{
		if (!ClassSelect_IsActive())
			return;
		++hold;
		if (hold < 30)
			return;
		UI_KeyEvent('1', 1);
		UI_KeyEvent('1', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 2)
	{
		if (ClassSelect_IsActive() || TeamSelect_IsActive())
			return;
		++hold;
		if (hold < 60)
			return;
		MenuEngine::ClientCmdNow("buy\n");
		Menu_Con("CSRETRO_BUY_GATE_REQUEST");
		++step;
		hold = 0;
		return;
	}

	if (step == 3)
	{
		++hold;
		if (BuySelect_IsActive() && g_panel && g_panel->IsMainPage())
		{
			if (hold < 30)
				return;
			if (!g_panel->ButtonsOnPanel())
			{
				failDone("layout clip");
				return;
			}
			const int title = g_panel->LabelLooksLocalized("Title") ? 1 : 0;
			const int pistols = g_panel->LabelLooksLocalized("pistols") ? 1 : 0;
			const int shotguns = g_panel->LabelLooksLocalized("shotguns") ? 1 : 0;
			const int rifles = g_panel->LabelLooksLocalized("rifles") ? 1 : 0;
			const int cancel = g_panel->LabelLooksLocalized("CancelButton") ? 1 : 0;
			const int raw = g_panel->AnyRawToken() ? 1 : 0;
			Menu_Con("CSRETRO_BUY_GATE_OPEN type=%d visible=1 main=1 buttons=%d title=%d "
				 "pistols=%d shotguns=%d rifles=%d cancel=%d raw=%d team=%d",
				g_panel->MenuType(), g_panel->VisibleButtonCount(), title, pistols,
				shotguns, rifles, cancel, raw, g_panel->Team());
			if (g_pVGuiLocalize)
			{
				const char *probes[] = {
					"Cstrike_Buy_Menu",
					"Cstrike_Pistols",
					"Cstrike_Shotguns",
					"Cstrike_Rifles",
					"Cstrike_Cancel",
					"Cstrike_Glock18",
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
		if (hold > 180)
		{
			failDone("buy menu fehlt");
			return;
		}
		return;
	}

	if (step == 4)
	{
		++hold;
		if (hold < 20)
			return;
		UI_KeyEvent(K_ESCAPE, 1);
		UI_KeyEvent(K_ESCAPE, 0);
		Menu_Con("CSRETRO_BUY_GATE_ESC visible=%d", BuySelect_IsActive() ? 1 : 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 5)
	{
		++hold;
		if (hold < 15)
			return;
		if (!g_overlay || !g_panel)
		{
			failDone("panel weg");
			return;
		}
		MenuEngine::ClientCmdNow("buy\n");
		++step;
		hold = 0;
		return;
	}

	if (step == 6)
	{
		++hold;
		if (!BuySelect_IsActive())
		{
			if (hold > 120)
				failDone("reopen");
			return;
		}
		if (hold < 15)
			return;
		UI_KeyEvent('1', 1);
		UI_KeyEvent('1', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 7)
	{
		++hold;
		if (BuySelect_IsActive() && g_panel && !g_panel->IsMainPage())
		{
			if (hold < 20)
				return;
			if (!g_panel->ButtonsOnPanel())
			{
				failDone("pistol layout clip");
				return;
			}
			const int title = g_panel->LabelLooksLocalized("Title") ? 1 : 0;
			const int glock = g_panel->LabelLooksLocalized("Glock18") ? 1 : 0;
			const int raw = g_panel->AnyRawToken() ? 1 : 0;
			Menu_Con("CSRETRO_BUY_GATE_PISTOL type=%d visible=1 title=%d glock=%d raw=%d",
				g_panel->MenuType(), title, glock, raw);
			UI_KeyEvent('1', 1);
			UI_KeyEvent('1', 0);
			++step;
			hold = 0;
			return;
		}
		if (hold > 120)
		{
			failDone("pistols fehlt");
			return;
		}
		return;
	}

	if (step == 8)
	{
		++hold;
		if (hold < 20)
			return;
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_BUY_GATE_DONE");
		step = 99;
	}
}
