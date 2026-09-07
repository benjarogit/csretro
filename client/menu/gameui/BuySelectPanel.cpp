#include "BuySelectPanel.h"

#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "InGameViewportLook.h"
#include "RadioSelectPanel.h"
#include "TeamSelectPanel.h"
#include "TeamModelPreview.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Divider.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

#include "../src/menu_priv.h"
#include "../vgui/main_menu.h"
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
	// Steam-.res und CS-1.6-HUD: autobuy/rebuy. Der Client lädt autobuy.txt/rebuy.txt
	// und schickt cl_setautobuy/cl_setrebuy. Direktes cl_autobuy ohne Liste kauft nichts.
	snprintf(out, outsz, "%s", cmd);
}

bool IsResCommand(const char *cmd)
{
	return cmd && (strstr(cmd, ".res") || strstr(cmd, ".RES"));
}

class CBuySelectPanel;

const char *BuyModel(const char *name)
{
	struct Entry { const char *alias; const char *model; };
	static const Entry aliases[] = {
		{"glock", "glock18"}, {"usp45", "usp"}, {"deserteagle", "deagle"},
		{"fn57", "fiveseven"}, {"flash", "flashbang"}, {"hegren", "hegrenade"},
		{"sgren", "smokegrenade"}, {"vest", "kevlar"}, {"vesthelm", "assault"},
		{"kevlarhelmet", "assault"}, {"kevlar_helmet", "assault"}, {"mp5navy", "mp5"},
		{"elites", "elite"}, {"defuser", "thighpack"}, {"usp", "usp"},
		{"deagle", "deagle"}
	};
	for (const Entry &entry : aliases)
		if (!strcasecmp(name, entry.alias)) return entry.model;
	// CS has no standalone night-vision model. Its named purchase card remains.
	if (!strcasecmp(name, "nvgs") || !strcasecmp(name, "nvg") || !strcasecmp(name, "nightvision"))
		return nullptr;
	return name;
}

float BuyWorldWidth(const char *model)
{
	if (!model || !model[0])
		return 24.0f;
	if (!strcasecmp(model, "kevlar") || !strcasecmp(model, "assault") || !strcasecmp(model, "thighpack"))
		return 34.0f;
	if (!strcasecmp(model, "flashbang") || !strcasecmp(model, "hegrenade") || !strcasecmp(model, "smokegrenade"))
		return 16.0f;
	if (!strcasecmp(model, "glock18") || !strcasecmp(model, "usp") || !strcasecmp(model, "deagle") ||
		!strcasecmp(model, "p228") || !strcasecmp(model, "fiveseven") || !strcasecmp(model, "elite"))
		return 12.0f;
	if (!strcasecmp(model, "awp") || !strcasecmp(model, "scout") || !strcasecmp(model, "g3sg1") ||
		!strcasecmp(model, "galil") || !strcasecmp(model, "ak47") || !strcasecmp(model, "m4a1") ||
		!strcasecmp(model, "aug") || !strcasecmp(model, "sg552") || !strcasecmp(model, "sg550") ||
		!strcasecmp(model, "famas") || !strcasecmp(model, "m249"))
		return 28.0f;
	return 22.0f;
}

void SetBuyModel(CTeamModelPreview *preview, const char *name)
{
	const char *model = BuyModel(name);
	preview->SetVisible(model != nullptr);
	if (!model) return;
	char path[96];
	snprintf(path, sizeof(path), "models/w_%s.mdl", model);
	preview->SetItemPreview(path, BuyWorldWidth(model));
}

class CBuyHoverButton : public Button
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuyHoverButton, Button);

public:
	CBuyHoverButton(Panel *parent, const char *name)
		: BaseClass(parent, name, "")
	{
		m_weaponImage = new CTeamModelPreview(this, "WeaponModel");
		m_weaponImage->SetMouseInputEnabled(false);
		m_weaponImage->SetKeyBoardInputEnabled(false);
		m_weaponImage->SetVisible(false);
		m_price = new Label(this, "Price", "");
		m_price->SetContentAlignment(Label::a_east);
		m_price->SetMouseInputEnabled(false);
		m_price->SetVisible(false);
	}

	void SetHost(CBuySelectPanel *host) { m_host = host; }
	void SetPreviewName(const char *name) { m_preview = name ? name : ""; }
	const std::string &PreviewName() const { return m_preview; }
	void SetFooter(bool footer) { m_isFooter = footer; ApplyLook(); }
	void ConfigureWeapon(const char *command, const char *label, int cost)
	{
		if (!command || !command[0] || cost <= 0)
			return;
		SetCommand(command);
		if (label && label[0])
			SetText(label);
		m_isWeaponCard = true;
		SetBuyModel(m_weaponImage, command);
		char price[24];
		std::snprintf(price, sizeof(price), "$%d", cost);
		m_price->SetText(price);
		m_price->SetVisible(true);
		m_preview = command;
		ApplyLook();
	}
	void SetAccent(Color accent)
	{
		m_accent = accent;
		ApplyLook();
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		ApplyLook();
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		ApplyLook();
		int w = 0, h = 0;
		GetSize(w, h);
		if (m_isWeaponCard)
		{
			const bool compact = h < 70;
			const int imageTop = compact ? 12 : 16;
			const int imageBottom = compact ? 8 : 18;
			const int sidePad = compact ? 4 : 8;
			const int maxImageW = std::max(1, w - sidePad * 2);
			const int maxImageH = std::max(1, h - imageTop - imageBottom);
			m_weaponImage->SetBounds(sidePad, imageTop, maxImageW, maxImageH);
			m_price->SetBounds(std::max(4, w - 70), h - (compact ? 15 : 22), 64, compact ? 13 : 18);
			m_price->SetFgColor(InGameViewportLook::BuyGold());
		}
	}

	void PaintBackground() override
	{
		if (m_isFooter)
			return;
		int w = 0, h = 0;
		GetSize(w, h);
		InGameViewportLook::PaintBuyCell(w, h, IsArmed() || IsDepressed());
	}

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
		const int cost = inResourceData->GetInt("cost", 0);
		m_isWeaponCard = cost > 0 && cmd[0] && !IsResCommand(cmd);
		if (m_isWeaponCard)
			ConfigureWeapon(cmd, nullptr, cost);
	}

	void OnCursorEntered() override;

private:
	CBuySelectPanel *m_host = nullptr;
	std::string m_preview;
	Color m_accent = InGameViewportLook::Text();
	CTeamModelPreview *m_weaponImage = nullptr;
	Label *m_price = nullptr;
	bool m_isWeaponCard = false;
	bool m_isFooter = false;

	void ApplyLook()
	{
		if (m_isFooter)
		{
			InGameViewportLook::StyleFooterButton(this, m_accent);
			SetContentAlignment(Label::a_center);
			SetTextInset(0, 0);
			return;
		}
		InGameViewportLook::StyleCardButton(this, InGameViewportLook::BuyGold());
		SetContentAlignment(m_isWeaponCard ? Label::a_northwest : Label::a_west);
		SetTextInset(m_isWeaponCard ? 8 : 12, m_isWeaponCard ? 4 : 0);
		SetFgColor(InGameViewportLook::Text());
		SetBgColor((IsArmed() || IsDepressed()) ? InGameViewportLook::BuyCellArmed() : InGameViewportLook::BuyCell());
		if (m_price)
			m_price->SetFgColor(InGameViewportLook::BuyGold());
	}
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
		m_overview.clear();
		std::fill(std::begin(m_overviewTitles), std::end(m_overviewTitles), nullptr);
		m_character = nullptr;
		m_plate = nullptr;
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
		SetProportional(false);
		ApplySteamCommands();
		if (m_pageIsMain)
			BuildUnifiedOverview();
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
		LayoutFamily();
		SetVisible(true);
		MoveToFront();
		RequestFocus();
		LogOpen();
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetPaintBackgroundEnabled(false);
		StyleButtons();
	}

	void LayoutFamily()
	{
		StyleButtons();
		FitToParent();
		if (m_pageIsMain)
			RelayoutMainGrid();
		else
			RelayoutWeaponList();
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

	void HideSteamCategoryChrome()
	{
		if (m_overview.empty())
			return;
		for (const SlotBind &bind : kMainSlots)
		{
			if (bind.slot == 10)
				continue;
			if (Panel *old = FindChildByName(bind.name))
			{
				old->SetVisible(false);
				old->SetAutoResize(Panel::PIN_TOPLEFT, Panel::AUTORESIZE_NO, 0, 0, 0, 0);
				old->SetBounds(-2000, -2000, 1, 1);
			}
		}
		if (Panel *cat = FindChildByName("selectCategory"))
		{
			cat->SetVisible(false);
			cat->SetBounds(-2000, -2000, 1, 1);
		}
	}

	void ApplySlots()
	{
		if (m_pageIsMain)
		{
			if (!m_overview.empty())
			{
				HideSteamCategoryChrome();
				if (Panel *autoBuy = FindChildByName("AutobuyButton"))
					autoBuy->SetVisible(true);
				if (Panel *rebuy = FindChildByName("RebuyButton"))
					rebuy->SetVisible(true);
				if (Panel *cancel = FindChildByName("CancelButton"))
					cancel->SetVisible(true);
				return;
			}
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
		if (!btn || (!m_pageIsMain && !btn->IsVisible()) || !btn->IsEnabled())
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
		std::string image = imageName;
		std::transform(image.begin(), image.end(), image.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		if (image == "kevlarhelmet" || image == "kevlar_helmet") image = "vesthelm";
		else if (image == "hegrenade") image = "hegren";
		else if (image == "smokegrenade") image = "sgren";
		else if (image == "nightvision" || image == "nvg") image = "nvgs";
		else if (image == "glock18") image = "glock";
		SetBuyModel(m_preview, image.c_str());
		if (Panel *info = FindChildByName("ItemInfo"))
		{
			info->SetVisible(GetWide() >= 960);
			info->SetEnabled(true);
		}
	}

	Panel *CreateControlByName(const char *controlName) override
	{
		if (controlName && !strcasecmp(controlName, "MouseOverPanelButton"))
			return new CBuyHoverButton(nullptr, nullptr);
		if (controlName && !strcasecmp(controlName, "Button"))
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
		// Kategorie bleibt offen. Munition auf der Hauptseite auch (6+7).
		// Waffe / Equipment / Autobuy / Rebuy: Menü zu, wie CS-1.6-Tastatur.
		if (strcasecmp(mapped, "primammo") != 0 && strcasecmp(mapped, "secammo") != 0)
			BuySelect_Hide();
		return;
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_ESCAPE)
		{
			BuySelect_Hide();
			return;
		}
		if (code == KEY_A)
		{
			OnCommand("autobuy");
			return;
		}
		if (code == KEY_R)
		{
			OnCommand("rebuy");
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
		// Steam-.res keeps 640-era child positions. Relayout after every
		// host resize; Open() is not enough if the overlay stays visible.
		LayoutFamily();
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
	CTeamModelPreview *m_preview = nullptr;
	CTeamModelPreview *m_character = nullptr;
	Panel *m_plate = nullptr;
	struct OverviewCard { CBuyHoverButton *button; int column; int row; };
	std::vector<OverviewCard> m_overview;
	Label *m_overviewTitles[5] = {};

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

	void BuildUnifiedOverview()
	{
		const char *titles[5] = {"Equipment", "Pistols", "Mid-Tier", "Rifles", "Grenades"};
		for (int col = 0; col < 5; ++col)
		{
			char name[32];
			snprintf(name, sizeof(name), "OverviewTitle%d", col);
			m_overviewTitles[col] = new Label(this, name, titles[col]);
			m_overviewTitles[col]->SetContentAlignment(Label::a_center);
		}

		int nextRow[5] = {};
		auto append = [&](const char *path, int column, int filter) {
			for (const ResField &field : Menu_LoadRes(path))
			{
				if (field.command.empty() || field.cost <= 0 || IsResCommand(field.command.c_str()))
					continue;
				const bool grenade = !strcasecmp(field.command.c_str(), "flash") ||
					!strcasecmp(field.command.c_str(), "hegren") || !strcasecmp(field.command.c_str(), "sgren");
				if ((filter == 1 && !grenade) || (filter == 2 && grenade) || nextRow[column] >= 6)
					continue;
				char name[64];
				snprintf(name, sizeof(name), "Overview%d_%d", column, nextRow[column]);
				auto *button = new CBuyHoverButton(this, name);
				button->SetHost(this);
				button->SetPreviewName(field.command.c_str());
				button->ConfigureWeapon(field.command.c_str(), field.label.c_str(), field.cost);
				m_overview.push_back({button, column, nextRow[column]++});
			}
		};

		const bool ct = m_team == TEAM_CT;
		const char *equipment = ct ? "resource/UI/BuyEquipment_CT.res" : "resource/UI/BuyEquipment_TER.res";
		append(equipment, 0, 2);
		append(ct ? "resource/UI/BuyPistols_CT.res" : "resource/UI/BuyPistols_TER.res", 1, 0);
		append(ct ? "resource/UI/BuyShotguns_CT.res" : "resource/UI/BuyShotguns_TER.res", 2, 0);
		append(ct ? "resource/UI/BuySubMachineguns_CT.res" : "resource/UI/BuySubMachineguns_TER.res", 2, 0);
		append(ct ? "resource/UI/BuyMachineguns_CT.res" : "resource/UI/BuyMachineguns_TER.res", 2, 0);
		append(ct ? "resource/UI/BuyRifles_CT.res" : "resource/UI/BuyRifles_TER.res", 3, 0);
		append(equipment, 4, 1);

		HideSteamCategoryChrome();
		m_plate = new Panel(this, "BuyPlate");
		m_plate->SetPaintBackgroundEnabled(true);
		m_plate->SetPaintBorderEnabled(false);
		m_plate->SetBgColor(InGameViewportLook::BuyPlate());
		m_plate->SetMouseInputEnabled(false);
		m_plate->SetKeyBoardInputEnabled(false);
		m_plate->SetZPos(-2);
		m_character = new CTeamModelPreview(this, "BuyCharacter");
		static int previous[2] = {-1, -1};
		const int side = ct ? 1 : 0;
		const int pick = previous[side] < 0 ? gEng.pfnRandomLong(0, 3) :
			(previous[side] + gEng.pfnRandomLong(1, 3)) % 4;
		previous[side] = pick;
		const char *terror[] = {"terror", "leet", "arctic", "guerilla"};
		const char *counter[] = {"urban", "gsg9", "sas", "gign"};
		const char *model = ct ? counter[pick] : terror[pick];
		char path[96];
		snprintf(path, sizeof(path), "models/player/%s/%s.mdl", model, model);
		m_character->SetPreview(path, ct ? "models/p_m4a1.mdl" : "models/p_ak47.mdl",
			ct ? 206.0f : 154.0f, ct ? 33 : 80);
		m_character->SetWorldWidth(72.0f);
		m_character->SetMouseInputEnabled(false);
		m_character->SetKeyBoardInputEnabled(false);
		m_character->SetZPos(1);
		Menu_Con("CSRETRO_BUY_CHARACTER team=%d model=%s", m_team, model);
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
		const Color gold = InGameViewportLook::BuyGold();
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn)
				continue;
			const char *name = btn->GetName();
			const bool footer = name && (!strcasecmp(name, "AutobuyButton") ||
				!strcasecmp(name, "RebuyButton") || !strcasecmp(name, "CancelButton") ||
				!strcasecmp(name, "cancelbutton"));
			if (auto *look = dynamic_cast<CBuyHoverButton *>(btn))
			{
				look->SetFooter(footer);
				look->SetAccent(gold);
				continue;
			}
			if (footer)
				InGameViewportLook::StyleFooterButton(btn, gold);
			else
				InGameViewportLook::StyleCardButton(btn, gold);
			btn->SetContentAlignment(footer ? Label::a_center : Label::a_west);
			btn->SetTextInset(footer ? 0 : 12, 0);
		}
		InGameViewportLook::StyleTitle(dynamic_cast<Label *>(FindChildByName("Title")));
		if (auto *cat = dynamic_cast<Label *>(FindChildByName("selectCategory")))
		{
			cat->SetVisible(false);
		}
		for (Label *title : m_overviewTitles)
		{
			if (!title)
				continue;
			title->SetFgColor(InGameViewportLook::TextDim());
			title->SetPaintBackgroundEnabled(false);
			title->SetContentAlignment(Label::a_west);
		}
		if (Panel *info = FindChildByName("ItemInfo"))
		{
			info->SetPaintBackgroundEnabled(true);
			info->SetBgColor(InGameViewportLook::BuyPlate());
		}
		if (Panel *div = FindChildByName("Divider1"))
			div->SetVisible(false);
		if (m_plate)
			m_plate->SetBgColor(InGameViewportLook::BuyPlate());
	}

	void RelayoutMainGrid()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 400 || h < 300)
			return;
		int canvasX = 0, canvasY = 0, canvasW = 0, canvasH = 0;
		InGameViewportLook::ContentCanvas(w, h, canvasX, canvasY, canvasW, canvasH);
		const bool compact = canvasW < 800;
		const int pad = std::max(10, canvasW * 2 / 100);
		const int gap = std::max(4, canvasW / 240);
		if (auto *title = FindChildByName("Title"))
			title->SetBounds(canvasX + pad, canvasY + canvasH * 2 / 100,
				canvasW * 58 / 100, std::max(24, canvasH * 6 / 100));
		HideSteamCategoryChrome();

		const int gridX = canvasX + pad;
		const int gridW = compact ? canvasW - pad * 2 : canvasW * 56 / 100;
		const int headerY = canvasY + canvasH * 9 / 100;
		const int headerH = std::max(20, canvasH * 4 / 100);
		const int gridY = headerY + headerH + gap;
		const int gridBottom = canvasY + canvasH * 86 / 100;
		const int cellW = (gridW - gap * 4) / 5;
		const int cellH = (gridBottom - gridY - gap * 5) / 6;
		if (m_plate)
		{
			m_plate->SetVisible(true);
			m_plate->SetBounds(gridX - gap, headerY - gap,
				gridW + gap * 2, gridBottom - headerY + gap * 2);
		}
		for (int col = 0; col < 5; ++col)
		{
			if (m_overviewTitles[col])
				m_overviewTitles[col]->SetBounds(gridX + col * (cellW + gap), headerY, cellW, headerH);
		}
		for (const OverviewCard &card : m_overview)
			card.button->SetBounds(gridX + card.column * (cellW + gap),
				gridY + card.row * (cellH + gap), cellW, cellH);
		if (m_character)
		{
			m_character->SetVisible(!compact);
			const int x = gridX + gridW + std::max(8, canvasW * 2 / 100);
			const int cw = canvasX + canvasW - pad - x;
			const int charTop = canvasY + canvasH * 6 / 100;
			const int charH = canvasH * 82 / 100;
			m_character->SetBounds(x, charTop, std::max(1, cw), charH);
		}

		std::vector<Panel *> bottom;
		for (const char *name : {"AutobuyButton", "RebuyButton", "CancelButton"})
		{
			if (Panel *p = FindChildByName(name))
				if (p->IsVisible())
					bottom.push_back(p);
		}
		if (bottom.empty())
			return;
		const int by = canvasY + canvasH * 91 / 100;
		const int bh = std::max(28, canvasH * 5 / 100);
		const int bw = std::min(220, (canvasW - pad * 2) / static_cast<int>(bottom.size()));
		const int totalW = bw * static_cast<int>(bottom.size()) + gap * static_cast<int>(bottom.size() - 1);
		const int startX = canvasX + (canvasW - totalW) / 2;
		for (size_t i = 0; i < bottom.size(); ++i)
			bottom[i]->SetBounds(startX + static_cast<int>(i) * (bw + gap), by, bw, bh);
	}

	void RelayoutWeaponList()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 400 || h < 300)
			return;
		int canvasX = 0, canvasY = 0, canvasW = 0, canvasH = 0;
		InGameViewportLook::ContentCanvas(w, h, canvasX, canvasY, canvasW, canvasH);
		const bool compact = canvasW < 900;
		const int pad = std::max(10, canvasW * 3 / 100);
		const int gap = std::max(4, canvasH / 80);
		if (auto *title = FindChildByName("Title"))
			title->SetBounds(canvasX + pad, canvasY, canvasW - pad * 2, std::max(28, canvasH * 7 / 100));
		if (auto *cat = FindChildByName("selectCategory"))
			cat->SetBounds(canvasX + pad, canvasY + canvasH * 7 / 100,
				canvasW - pad * 2, std::max(18, canvasH * 4 / 100));

		std::vector<Button *> weapons;
		Button *cancel = nullptr;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn || !btn->IsVisible())
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
		const int listW = compact ? canvasW - pad * 2 : canvasW * 56 / 100;
		const int listY = canvasY + canvasH * 14 / 100;
		const int listH = canvasH * 70 / 100;
		const int cols = weapons.size() > 1 ? 2 : 1;
		const int rows = weapons.empty() ? 1 :
			(static_cast<int>(weapons.size()) + cols - 1) / cols;
		const int cardW = (listW - gap * (cols - 1)) / cols;
		const int rowH = (listH - gap * (rows - 1)) / rows;
		for (size_t i = 0; i < weapons.size(); ++i)
		{
			const int col = static_cast<int>(i) % cols;
			const int row = static_cast<int>(i) / cols;
			weapons[i]->SetBounds(canvasX + pad + col * (cardW + gap), listY + row * (rowH + gap), cardW, rowH);
		}
		if (cancel)
			cancel->SetBounds(canvasX + pad, canvasY + canvasH * 91 / 100,
				std::min(220, listW), std::max(28, canvasH * 5 / 100));
		if (Panel *info = FindChildByName("ItemInfo"))
		{
			info->SetVisible(!compact);
			const int infoX = canvasX + pad + listW + canvasW * 4 / 100;
			const int infoW = canvasX + canvasW - pad - infoX;
			info->SetBounds(infoX, listY, infoW, listH);
			if (!m_preview)
				m_preview = new CTeamModelPreview(info, "ItemPreview");
			if (m_preview)
			{
				const int iw = std::max(1, infoW - 16);
				const int ih = std::max(1, listH - 16);
				m_preview->SetBounds(8, 8, iw, ih);
			}
		}
	}

	void BindHover()
	{
		std::string firstPreview;
		Panel *info = FindChildByName("ItemInfo");
		if (info && !m_preview)
		{
			info->SetPaintBackgroundEnabled(true);
			info->SetMouseInputEnabled(false);
			info->SetBgColor(InGameViewportLook::Card());
			m_preview = new CTeamModelPreview(info, "ItemPreview");
			m_preview->SetVisible(false);
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
			{
				if (btn->PreviewName().empty())
					btn->SetPreviewName(name);
				if (firstPreview.empty() && !m_pageIsMain)
					firstPreview = btn->PreviewName();
			}
		}
		if (!firstPreview.empty())
			ShowItemPreview(firstPreview.c_str());
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
		int canvasX = 0, canvasY = 0, canvasW = 0, canvasH = 0;
		InGameViewportLook::ContentCanvas(pw, ph, canvasX, canvasY, canvasW, canvasH);
		Menu_Con("CSRETRO_BUY_CANVAS view=%dx%d canvas=%d,%d %dx%d capped=%d compact=%d model=%d",
			pw, ph, canvasX, canvasY, canvasW, canvasH,
			(canvasW < pw * 9 / 10 || canvasH < ph * 9 / 10) ? 1 : 0,
			canvasW < 800 ? 1 : 0, m_character && m_character->IsVisible() ? 1 : 0);
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
		SetPaintBackgroundEnabled(true);
		SetPaintBorderEnabled(false);
		SetBorder(nullptr);
		SetBgColor(Color(0, 0, 0, 0));
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		InGameViewportLook::PaintTeamBackdrop(w, h, PauseBackdrop_IsBlurred());
		if (surface())
		{
			surface()->DrawSetColor(0, 0, 0, 90);
			surface()->DrawFilledRect(0, 0, w, h);
		}
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
			m_buy->LayoutFamily();
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
	MainMenu_SyncDialogVisibility();
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
	MainMenu_SyncDialogVisibility();
	if (g_keyDestPushed && !gMenuVisible && !TeamSelect_IsActive() && !ClassSelect_IsActive() &&
		!RadioSelect_IsActive())
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
			MenuEngine::ClientCmd("screenshot scrshots/buy-pistols.png\n");
			step = 9;
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

	if (step == 9)
	{
		if (++hold < 20)
			return;
		UI_KeyEvent('1', 1);
		UI_KeyEvent('1', 0);
		step = 8;
		hold = 0;
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
