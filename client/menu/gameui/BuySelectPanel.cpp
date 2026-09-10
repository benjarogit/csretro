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
#include "cl_dll/IGameMenuExports.h"
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
BuyHudState g_buyHud = {};
char g_buyClassModel[32] = {};

bool ClassStemForTeam(const char *stem, bool ct)
{
	if (!stem || !stem[0])
		return false;
	static const char *tNames[] = {"terror", "leet", "arctic", "guerilla", "militia"};
	static const char *ctNames[] = {"urban", "gsg9", "sas", "gign", "spetsnaz"};
	const char **names = ct ? ctNames : tNames;
	const int count = ct ? 5 : 5;
	for (int i = 0; i < count; ++i)
	{
		if (!strcasecmp(stem, names[i]))
			return true;
	}
	return false;
}

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

bool IsShieldBuy(const char *cmd)
{
	return cmd && !strcasecmp(cmd, "shield");
}

bool IsNvgBuy(const char *cmd)
{
	return cmd && (!strcasecmp(cmd, "nvgs") || !strcasecmp(cmd, "nightvision"));
}

class CBuySelectPanel;

const char *BuyModel(const char *name)
{
	if (!name || !*name || IsNvgBuy(name))
		return nullptr;
	struct Entry { const char *alias; const char *model; };
	static const Entry aliases[] = {
		{"glock", "glock18"}, {"usp45", "usp"}, {"deserteagle", "deagle"},
		{"fn57", "fiveseven"}, {"flash", "flashbang"}, {"hegren", "hegrenade"},
		{"sgren", "smokegrenade"}, {"molotov", "molotov"}, {"incgrenade", "incgrenade"},
		{"vest", "kevlar"}, {"vesthelm", "assault"},
		{"kevlarhelmet", "assault"}, {"kevlar_helmet", "assault"}, {"mp5navy", "mp5"},
		{"elites", "elite"}, {"defuser", "thighpack"}, {"usp", "usp"},
		{"deagle", "deagle"}
	};
	for (const Entry &entry : aliases)
		if (!strcasecmp(name, entry.alias)) return entry.model;
	return name;
}

const char *BuyDisplayName(const char *command)
{
	struct Entry { const char *command; const char *label; };
	static const Entry names[] = {
		{"vest", "Kevlar Vest"}, {"vesthelm", "Kevlar+Helm"},
		{"defuser", "Defuse Kit"},
		{"glock", "Glock-18"}, {"usp", "USP"}, {"p228", "P228"},
		{"deagle", "Desert Eagle"}, {"elites", "Dual Elites"},
		{"fn57", "Five-SeveN"}, {"m3", "M3"}, {"xm1014", "XM1014"},
		{"mac10", "MAC-10"}, {"tmp", "TMP"}, {"mp5", "MP5"},
		{"ump45", "UMP45"}, {"p90", "P90"}, {"m249", "M249"},
		{"galil", "Galil"}, {"famas", "FAMAS"}, {"ak47", "AK-47"},
		{"m4a1", "M4A1"}, {"scout", "Scout"}, {"sg552", "SG 552"},
		{"aug", "AUG"}, {"awp", "AWP"}, {"g3sg1", "G3SG1"},
		{"sg550", "SG 550"}, {"flash", "Flashbang"},
		{"hegren", "HE Grenade"}, {"sgren", "Smoke"},
		{"molotov", "Molotov"}, {"incgrenade", "Incendiary"},
	};
	if (!command)
		return nullptr;
	for (const Entry &entry : names)
		if (!strcasecmp(command, entry.command))
			return entry.label;
	return nullptr;
}

const char *CompactBuyDisplayName(const std::string &name)
{
	struct Entry { const char *full; const char *compact; };
	static const Entry names[] = {
		{"Kevlar Vest", "Kevlar"}, {"Kevlar+Helm", "Kev+Helm"},
		{"Desert Eagle", "Deagle"},
		{"Dual Elites", "Elites"}, {"Flashbang", "Flash"},
		{"HE Grenade", "HE Gren."},
	};
	for (const Entry &entry : names)
		if (name == entry.full)
			return entry.compact;
	return name.c_str();
}

void SetBuyModel(CTeamModelPreview *preview, const char *name)
{
	const char *model = BuyModel(name);
	preview->SetVisible(model != nullptr);
	if (!model) return;
	char path[96];
	snprintf(path, sizeof(path), "models/w_%s.mdl", model);
	preview->SetItemPreview(path);
}

class CBuyRoundedPanel : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuyRoundedPanel, Panel);

public:
	CBuyRoundedPanel(Panel *parent, const char *name) : BaseClass(parent, name)
	{
		SetPaintBackgroundEnabled(true);
		SetPaintBorderEnabled(false);
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		InGameViewportLook::PaintBuyPlate(w, h);
	}
};

class CBuySectionLabel : public Label
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuySectionLabel, Label);

public:
	CBuySectionLabel(Panel *parent, const char *name, const char *text)
		: BaseClass(parent, name, text)
	{
		SetPaintBackgroundEnabled(true);
		SetPaintBorderEnabled(false);
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		InGameViewportLook::PaintBuyHeader(w, h);
	}
};

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
		m_weaponImage->SetZPos(0);
		m_number = new Label(this, "ItemNumber", "");
		m_number->SetContentAlignment(Label::a_west);
		m_number->SetMouseInputEnabled(false);
		m_number->SetPaintBackgroundEnabled(false);
		m_number->SetVisible(false);
		m_number->SetZPos(2);
		m_name = new Label(this, "ItemName", "");
		m_name->SetContentAlignment(Label::a_west);
		m_name->SetMouseInputEnabled(false);
		m_name->SetPaintBackgroundEnabled(false);
		m_name->SetVisible(false);
		m_name->SetZPos(2);
		m_price = new Label(this, "Price", "");
		m_price->SetContentAlignment(Label::a_east);
		m_price->SetMouseInputEnabled(false);
		m_price->SetPaintBackgroundEnabled(false);
		m_price->SetVisible(false);
		m_price->SetZPos(2);
	}

	void SetFooter(bool footer) { m_isFooter = footer; ApplyLook(); }
	void SetOverviewNumber(int number)
	{
		if (number < 1)
			return;
		char digits[8];
		std::snprintf(digits, sizeof(digits), "%d", number);
		m_number->SetText(digits);
		m_number->SetVisible(true);
		SetText("");
	}
	void ConfigureWeapon(const char *command, const char *label, int cost)
	{
		if (!command || !command[0] || cost <= 0)
			return;
		SetCommand(command);
		const char *display = BuyDisplayName(command);
		const char *productName = display && display[0] ? display : label;
		if (productName && productName[0])
		{
			m_productName = productName;
			m_name->SetText(productName);
			m_name->SetVisible(true);
		}
		SetText("");
		m_isWeaponCard = true;
		m_cost = cost;
		SetBuyModel(m_weaponImage, command);
		char price[24];
		std::snprintf(price, sizeof(price), "$%d", cost);
		m_price->SetText(price);
		m_price->SetVisible(true);
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
			const bool compact = h < 54;
			const int pad = compact ? 3 : 6;
			const int numberW = m_number->IsVisible() ? (compact ? 10 : 16) : 0;
			const int labelH = compact ? 12 : 15;
			const int priceH = compact ? 11 : 14;
			if (!m_productName.empty())
				m_name->SetText(w < 80 ? CompactBuyDisplayName(m_productName) : m_productName.c_str());
			const int imgY = labelH + 1;
			// The price is an overlay, as in the reference. Reserving a second text
			// row here squeezed the real MDL into a thin strip on compact cards.
			const int imgH = std::max(1, h - imgY - (compact ? 3 : 4));
			m_weaponImage->SetBounds(pad, imgY, std::max(1, w - pad * 2), imgH);
			m_number->SetBounds(pad, 3, numberW, labelH);
			m_name->SetBounds(pad + numberW, 3, std::max(1, w - numberW - pad * 2), labelH);
			m_price->SetBounds(std::max(pad, w - 72), h - priceH - 2, 68, priceH);
			const bool dim = Unaffordable();
			m_number->SetFgColor(dim ? Color(168, 170, 174, 220) : Color(210, 210, 214, 220));
			m_name->SetFgColor(dim ? InGameViewportLook::TextDim() : InGameViewportLook::Text());
			m_price->SetFgColor(dim ? Color(196, 168, 64, 230) : InGameViewportLook::BuyGold());
		}
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (m_isFooter)
		{
			if (w > 4 && h > 4)
				InGameViewportLook::PaintRoundedRect(0, 0, w, h, std::min(4, h / 4),
					(IsArmed() || IsDepressed()) ? Color(48, 42, 22, 230) : Color(28, 26, 20, 210));
			return;
		}
		InGameViewportLook::PaintBuyCell(w, h, IsArmed() || IsDepressed(), Unaffordable());
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

private:
	Color m_accent = InGameViewportLook::Text();
	CTeamModelPreview *m_weaponImage = nullptr;
	Label *m_number = nullptr;
	Label *m_name = nullptr;
	Label *m_price = nullptr;
	bool m_isWeaponCard = false;
	bool m_isFooter = false;
	int m_cost = 0;
	std::string m_productName;

	bool Unaffordable() const
	{
		return m_isWeaponCard && m_cost > 0 && g_buyHud.money < m_cost;
	}

	void ApplyLook()
	{
		if (m_isFooter)
		{
			InGameViewportLook::StyleFooterButton(this, m_accent);
			SetPaintBackgroundEnabled(true);
			SetContentAlignment(Label::a_center);
			SetTextInset(0, 0);
			return;
		}
		InGameViewportLook::StyleCardButton(this, InGameViewportLook::BuyGold());
		SetContentAlignment(m_isWeaponCard ? Label::a_northwest : Label::a_west);
		SetTextInset(m_isWeaponCard ? 0 : 12, m_isWeaponCard ? 0 : 0);
		const bool dim = Unaffordable();
		SetFgColor(dim ? InGameViewportLook::TextDim() : InGameViewportLook::Text());
		SetBgColor((IsArmed() || IsDepressed()) ? InGameViewportLook::BuyCellArmed() :
			(dim ? InGameViewportLook::BuyCellDim() : InGameViewportLook::BuyCell()));
		if (m_number)
			m_number->SetFgColor(dim ? Color(168, 170, 174, 220) : Color(210, 210, 214, 220));
		if (m_name)
			m_name->SetFgColor(dim ? InGameViewportLook::TextDim() : InGameViewportLook::Text());
		if (m_price)
			m_price->SetFgColor(dim ? Color(196, 168, 64, 230) : InGameViewportLook::BuyGold());
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
		const bool enteringMain = menuType == MENU_BUY && (!resOverride || !resOverride[0]);
		m_overview.clear();
		std::fill(std::begin(m_overviewTitles), std::end(m_overviewTitles), nullptr);
		m_character = nullptr;
		m_plate = nullptr;
		m_money = nullptr;
		while (GetChildCount() > 0)
			delete GetChild(0);
		m_type = menuType;
		m_pageIsMain = enteringMain;

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
		BuildCharacterStage(enteringMain);
		BuildRuntimeChrome();
		ApplyBuyLabels();
		BlankRawTokens();
		StyleButtons();
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
		ApplyOwnClassPreview();
		LogOpen();
	}

	void SyncCharacterFromHud()
	{
		ApplyOwnClassPreview();
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetPaintBackgroundEnabled(false);
		StyleButtons();
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		UpdateRuntimeChrome();
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
			CloseClientBuyMenu();
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
			CloseClientBuyMenu();
		return;
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_ESCAPE)
		{
			CloseClientBuyMenu();
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

	void CloseClientBuyMenu()
	{
		// This overlay replaces the server-owned Menu_Buy page. Closing only its
		// VGUI controls leaves CBasePlayer::m_iMenu at Menu_Buy; a later `buy`
		// can then be followed by a stale BuyClose. Slot 10 is the protocol-level
		// cancel used by the original menu and clears that state deterministically.
		BuySelect_Hide();
		MenuEngine::ClientCmdNow("menuselect 10\n");
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
	CTeamModelPreview *m_character = nullptr;
	Panel *m_plate = nullptr;
	Label *m_money = nullptr;
	std::string m_characterModel;
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
		const char *titles[5] = {"1  Equipment", "2  Pistols", "3  Mid-Tier", "4  Rifles", "5  Grenades"};
		for (int col = 0; col < 5; ++col)
		{
			char name[32];
			snprintf(name, sizeof(name), "OverviewTitle%d", col);
			m_overviewTitles[col] = new CBuySectionLabel(this, name, titles[col]);
			m_overviewTitles[col]->SetContentAlignment(Label::a_center);
		}

		int nextRow[5] = {};
		auto append = [&](const char *path, int column, int filter) {
			for (const ResField &field : Menu_LoadRes(path))
			{
				if (field.command.empty() || field.cost <= 0 || IsResCommand(field.command.c_str()) ||
					IsShieldBuy(field.command.c_str()) || IsNvgBuy(field.command.c_str()))
					continue;
				const bool grenade = !strcasecmp(field.command.c_str(), "flash") ||
					!strcasecmp(field.command.c_str(), "hegren") || !strcasecmp(field.command.c_str(), "sgren") ||
					!strcasecmp(field.command.c_str(), "molotov") || !strcasecmp(field.command.c_str(), "incgrenade");
				if ((filter == 1 && !grenade) || (filter == 2 && grenade) || nextRow[column] >= 6)
					continue;
				char name[64];
				snprintf(name, sizeof(name), "Overview%d_%d", column, nextRow[column]);
				auto *button = new CBuyHoverButton(this, name);
				button->ConfigureWeapon(field.command.c_str(), field.label.c_str(), field.cost);
				button->SetOverviewNumber(nextRow[column] + 1);
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
		m_plate = new CBuyRoundedPanel(this, "BuyPlate");
		m_plate->SetPaintBackgroundEnabled(true);
		m_plate->SetPaintBorderEnabled(false);
		m_plate->SetBgColor(InGameViewportLook::BuyPlate());
		m_plate->SetMouseInputEnabled(false);
		m_plate->SetKeyBoardInputEnabled(false);
		m_plate->SetZPos(-2);
	}

	static bool ValidPlayerModel(const char *name)
	{
		if (!name || !name[0] || std::strlen(name) >= 32)
			return false;
		for (const char *p = name; *p; ++p)
		{
			const unsigned char c = static_cast<unsigned char>(*p);
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_'))
				return false;
		}
		return true;
	}

	void ApplyOwnClassPreview()
	{
		if (!m_character)
			return;
		const bool ct = m_team == TEAM_CT;
		const char *name = nullptr;
		const char *src = "fallback";
		if (const char *forced = std::getenv("CSRETRO_BUY_PREVIEW_MODEL"))
		{
			if (ValidPlayerModel(forced))
			{
				name = forced;
				src = "env";
			}
		}
		// A class chosen through this UI is exact and immediately authoritative.
		// Playerinfo can lag behind (or briefly expose the team's default model)
		// after joinclass and was therefore showing the wrong Buy character.
		if (!name && ClassStemForTeam(g_buyClassModel, ct))
		{
			name = g_buyClassModel;
			src = "joinclass";
		}
		if (!name && ClassStemForTeam(g_buyHud.model, ct))
		{
			name = g_buyHud.model;
			src = "hud";
		}
		const char *fallback = ct ? "urban" : "terror";
		const char *model = name ? name : fallback;
		if (m_characterModel == model)
			return;
		m_characterModel = model;
		char path[96];
		snprintf(path, sizeof(path), "models/player/%s/%s.mdl", model, model);
		// Same one-figure player/weapon path as Team/Class; only the Buy camera
		// frames it closer to match the supplied composition.
		const float yaw = ct ? 206.0f : 202.0f;
		const int seq = ct ? 33 : 80;
		m_character->SetPreview(path, ct ? "models/p_m4a1.mdl" : "models/p_ak47.mdl", yaw, seq,
			ct ? 4.0f : 0.0f);
		// The reference devotes roughly the full stage height to the character.
		// Width is the limiting camera axis in this wide viewport, so framing only
		// by height leaves the real player MDL visibly too small.
		m_character->SetWorldWidth(44.0f);
		m_character->SetWorldHeight(60.0f);
		Menu_Con("CSRETRO_BUY_CHARACTER team=%d model=%s src=%s remember=%s hud=%s yaw=%.0f seq=%d",
			m_team, model, src, g_buyClassModel[0] ? g_buyClassModel : "-",
			g_buyHud.model[0] ? g_buyHud.model : "-", yaw, seq);
	}

	void BuildCharacterStage(bool randomize)
	{
		(void)randomize;
		m_character = new CTeamModelPreview(this, "BuyCharacter");
		m_character->SetIndependentPlayerState(true);
		m_character->SetMouseInputEnabled(false);
		m_character->SetKeyBoardInputEnabled(false);
		m_character->SetZPos(1);
		m_characterModel.clear();
		ApplyOwnClassPreview();
	}

	void BuildRuntimeChrome()
	{
		m_money = new Label(this, "BuyMoney", "");
		m_money->SetContentAlignment(Label::a_west);
		m_money->SetPaintBackgroundEnabled(false);
		m_money->SetFgColor(InGameViewportLook::BuyGold());
		m_money->SetMouseInputEnabled(false);
		m_money->SetKeyBoardInputEnabled(false);
		UpdateRuntimeChrome();
	}

	void UpdateRuntimeChrome()
	{
		if (m_money)
		{
			char money[32];
			std::snprintf(money, sizeof(money), "$%d", std::max(0, g_buyHud.money));
			m_money->SetText(money);
		}
		if (!m_pageIsMain)
			return;
		auto *title = dynamic_cast<Label *>(FindChildByName("Title"));
		if (!title)
			return;
		const int configured = static_cast<int>(MenuEngine::GetCvarFloat("mp_buytime") * 60.0f + 0.5f);
		int remaining = configured;
		if (configured > 0 && g_buyHud.roundDuration > 0)
		{
			const int elapsed = std::max(0, g_buyHud.roundDuration - g_buyHud.roundRemaining);
			remaining = std::max(0, configured - elapsed);
		}
		char text[80];
		if (configured < 0)
			std::snprintf(text, sizeof(text), "BUY TIME REMAINING  --:--");
		else
			std::snprintf(text, sizeof(text), "BUY TIME REMAINING  %02d:%02d",
				remaining / 60, remaining % 60);
		title->SetText(text);
		title->SetContentAlignment(Label::a_center);
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
			title->SetContentAlignment(Label::a_center);
		}
		if (m_money)
		{
			IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
			if (sch)
			{
				vgui2::HFont font = sch->GetFont("CreditsTitle", IsProportional());
				if (font != INVALID_FONT)
					m_money->SetFont(font);
			}
			m_money->SetFgColor(InGameViewportLook::BuyGold());
		}
		if (Panel *info = FindChildByName("ItemInfo"))
		{
			// The CS:GO-style character is composited directly over the dimmed
			// world.  The legacy Steam ItemInfo rectangle is only a layout host.
			info->SetPaintBackgroundEnabled(false);
			info->SetPaintBorderEnabled(false);
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
		// On sub-720p windows, scaling the 1280 reference literally leaves large
		// dead margins while item names collapse into ellipses. Use the available
		// 4:3 stage more fully there; the 1.0 cap keeps 1280+ layouts from growing.
		const float scale = std::max(0.5f, std::min(1.0f,
			std::min(static_cast<float>(w) / 1066.6667f, static_cast<float>(h) / 720.0f)));
		const int stageW = std::min(w - 16, static_cast<int>(980.0f * scale));
		const int stageH = std::min(h - 16, static_cast<int>(600.0f * scale));
		const int stageX = (w - stageW) / 2;
		const int stageY = (h - stageH) / 2;
		const int footerGap = std::max(4, static_cast<int>(6.0f * scale));
		const int plateX = stageX;
		const int plateW = std::min(stageW, static_cast<int>(600.0f * scale));
		const int titleY = stageY + static_cast<int>(82.0f * scale);
		const int titleH = std::max(22, static_cast<int>(26.0f * scale));
		const int headerY = titleY + titleH;
		const int headerH = std::max(20, static_cast<int>(24.0f * scale));
		const int gridY = headerY + headerH + std::max(3, static_cast<int>(6.0f * scale));
		// Six CS 1.6 entries must occupy roughly the same total height as the
		// five-row reference instead of stretching the plate to the footer.
		const int cellH = std::max(40, static_cast<int>(48.0f * scale));
		const int rowGap = std::max(4, static_cast<int>(6.0f * scale));
		const int gridBottom = gridY + cellH * 6 + rowGap * 5;
		// Measured from the supplied 1280x720 reference. Equal-width columns
		// erase its rhythm; equipment/pistols/grenades are narrow while the two
		// long-name weapon groups intentionally receive more room.
		const int baseX[5] = {9, 116, 224, 355, 497};
		const int baseW[5] = {89, 89, 110, 122, 87};
		int colX[5] = {};
		int colW[5] = {};
		for (int col = 0; col < 5; ++col)
		{
			colX[col] = plateX + static_cast<int>(baseX[col] * scale);
			colW[col] = std::max(42, static_cast<int>(baseW[col] * scale));
		}
		if (auto *title = FindChildByName("Title"))
			title->SetBounds(plateX, titleY, plateW, titleH);
		HideSteamCategoryChrome();
		if (m_plate)
		{
			m_plate->SetVisible(true);
			m_plate->SetBounds(plateX, titleY, plateW, gridBottom - titleY + std::max(4, static_cast<int>(6.0f * scale)));
		}
		for (int col = 0; col < 5; ++col)
		{
			if (m_overviewTitles[col])
				m_overviewTitles[col]->SetBounds(colX[col], headerY, colW[col], headerH);
		}
		for (const OverviewCard &card : m_overview)
			card.button->SetBounds(colX[card.column],
				gridY + card.row * (cellH + rowGap), colW[card.column], cellH);
		if (m_character)
		{
			m_character->SetVisible(true);
			const int charGap = std::max(6, static_cast<int>(8.0f * scale));
			const int charX = plateX + plateW + charGap;
			const int charRight = stageX + stageW - std::max(18, static_cast<int>(35.0f * scale));
			const int charW = charRight - charX;
			const int charY = stageY + static_cast<int>(40.0f * scale);
			const int charBottom = stageY + static_cast<int>(580.0f * scale);
			m_character->SetBounds(charX, charY, std::max(1, charW),
				std::max(1, charBottom - charY));
		}
		if (m_money)
			m_money->SetBounds(std::max(18, w * 3 / 100), h - std::max(70, static_cast<int>(100.0f * scale)),
				std::max(120, static_cast<int>(180.0f * scale)), std::max(36, static_cast<int>(50.0f * scale)));

		std::vector<Panel *> bottom;
		for (const char *name : {"AutobuyButton", "RebuyButton", "CancelButton"})
		{
			if (Panel *p = FindChildByName(name))
				if (p->IsVisible())
					bottom.push_back(p);
		}
		if (bottom.empty())
			return;
		const int by = stageY + static_cast<int>(575.0f * scale);
		const int bh = std::max(28, static_cast<int>(32.0f * scale));
		const int bw = std::max(110, static_cast<int>(145.0f * scale));
		const int totalW = bw * static_cast<int>(bottom.size()) + footerGap * static_cast<int>(bottom.size() - 1);
		const int startX = stageX + (stageW - totalW) / 2;
		for (size_t i = 0; i < bottom.size(); ++i)
			bottom[i]->SetBounds(startX + static_cast<int>(i) * (bw + footerGap), by, bw, bh);
	}

	void RelayoutWeaponList()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 400 || h < 300)
			return;
		const float scale = std::max(0.5f, std::min(1.2f,
			std::min(static_cast<float>(w) / 1280.0f, static_cast<float>(h) / 720.0f)));
		const int stageW = std::min(w - 16, static_cast<int>(980.0f * scale));
		const int stageH = std::min(h - 16, static_cast<int>(600.0f * scale));
		const int stageX = (w - stageW) / 2;
		const int stageY = (h - stageH) / 2;
		const int gap = std::max(4, static_cast<int>(6.0f * scale));
		const int listW = std::min(stageW, static_cast<int>(600.0f * scale));
		const int titleY = stageY + static_cast<int>(80.0f * scale);
		const int titleH = std::max(24, static_cast<int>(28.0f * scale));
		if (auto *title = FindChildByName("Title"))
			title->SetBounds(stageX, titleY, listW, titleH);
		if (auto *cat = FindChildByName("selectCategory"))
			cat->SetVisible(false);

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
				if (btn->IsVisible())
					cancel = btn;
				continue;
			}
			if (name && (!strcasecmp(name, "shield") || !strcasecmp(name, "nvgs") ||
				!strcasecmp(name, "nightvision")))
			{
				btn->SetVisible(false);
				continue;
			}
			if (KeyValues *kv = btn->GetCommand())
			{
				if (IsShieldBuy(kv->GetString("command", "")) || IsNvgBuy(kv->GetString("command", "")))
				{
					btn->SetVisible(false);
					continue;
				}
			}
			if (!btn->IsVisible())
				continue;
			weapons.push_back(btn);
		}
		std::sort(weapons.begin(), weapons.end(), [](Button *a, Button *b) {
			int ax = 0, ay = 0, bx = 0, by = 0;
			a->GetPos(ax, ay);
			b->GetPos(bx, by);
			return ay < by;
		});
		const int listY = titleY + titleH;
		const int listH = static_cast<int>(390.0f * scale);
		const int cols = weapons.size() > 1 ? 2 : 1;
		const int rows = weapons.empty() ? 1 :
			(static_cast<int>(weapons.size()) + cols - 1) / cols;
		const int cardW = (listW - gap * (cols - 1)) / cols;
		const int rowH = (listH - gap * (rows - 1)) / rows;
		for (size_t i = 0; i < weapons.size(); ++i)
		{
			const int col = static_cast<int>(i) % cols;
			const int row = static_cast<int>(i) / cols;
			weapons[i]->SetBounds(stageX + col * (cardW + gap), listY + row * (rowH + gap), cardW, rowH);
		}
		if (cancel)
			cancel->SetBounds(stageX + (stageW - static_cast<int>(145.0f * scale)) / 2,
				stageY + static_cast<int>(575.0f * scale),
				std::max(110, static_cast<int>(145.0f * scale)),
				std::max(28, static_cast<int>(32.0f * scale)));
		if (Panel *info = FindChildByName("ItemInfo"))
		{
			info->SetVisible(true);
			const int infoGap = std::max(16, static_cast<int>(22.0f * scale));
			const int infoX = stageX + listW + infoGap;
			const int infoW = stageX + stageW - infoX;
			const int infoH = titleH + static_cast<int>(390.0f * scale);
			info->SetBounds(infoX, titleY, std::max(1, infoW), std::max(1, infoH));
			if (m_character)
			{
				m_character->SetVisible(true);
				m_character->SetBounds(infoX, titleY, std::max(1, infoW), std::max(1, infoH));
			}
		}
		if (m_money)
			m_money->SetBounds(std::max(18, w * 3 / 100),
				h - std::max(70, static_cast<int>(100.0f * scale)),
				std::max(120, static_cast<int>(180.0f * scale)),
				std::max(36, static_cast<int>(50.0f * scale)));
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
		SetZPos(50);
		SetZPos(50);
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

	// The equipment resource and the character preview are team-specific.
	// Reusing a panel created for the previous team kept showing Molotov to CTs
	// (and could retain the previous team's player model).
	if (g_panel && g_panel->Team() != team)
		DestroyBuyUi();

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
	PauseBackdrop_Invalidate();
	g_overlay->SetMouseInputEnabled(true);
	g_overlay->SetKeyBoardInputEnabled(true);
	g_overlay->SetZPos(50);
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
	PauseBackdrop_Invalidate();
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

void BuySelect_RememberClass(const char *modelStem)
{
	if (!modelStem || !modelStem[0])
	{
		g_buyClassModel[0] = '\0';
		return;
	}
	if (!ClassStemForTeam(modelStem, false) && !ClassStemForTeam(modelStem, true))
		return;
	snprintf(g_buyClassModel, sizeof(g_buyClassModel), "%s", modelStem);
	Menu_Con("CSRETRO_BUY_REMEMBER class=%s", g_buyClassModel);
	if (g_panel && g_panel->IsVisible())
		g_panel->SyncCharacterFromHud();
}

void BuySelect_SetHud(const BuyHudState *state)
{
	if (!state)
		return;
	const bool modelChanged = std::strcmp(g_buyHud.model, state->model) != 0;
	g_buyHud = *state;
	if (modelChanged && g_panel && g_panel->IsVisible())
		g_panel->SyncCharacterFromHud();
}

void BuySelect_GateTick()
{
	if (!getenv("CSRETRO_BUY_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	static bool resizeRequested = false;
	static int resizeBeforeW = 0;
	static int resizeBeforeH = 0;
	static int resizeTargetW = 0;
	static int resizeTargetH = 0;
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
		const bool gateCt = std::getenv("CSRETRO_BUY_GATE_TEAM") &&
			!strcasecmp(std::getenv("CSRETRO_BUY_GATE_TEAM"), "ct");
		const int teamKey = gateCt ? '2' : '1';
		UI_KeyEvent(teamKey, 1);
		UI_KeyEvent(teamKey, 0);
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
		// Deliberately choose the non-default Leet class. This makes the gate
		// prove that Buy uses the active HUD/player model rather than a hard-coded
		// Terror fallback which would coincidentally pass slot 1.
		UI_KeyEvent('2', 1);
		UI_KeyEvent('2', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 2)
	{
		if (ClassSelect_IsActive() || TeamSelect_IsActive())
			return;
		++hold;
		// Give the server's buy-zone/player-spawn state time to settle.  If the
		// synthetic `buy` arrives on the transition frame, BuyClose legitimately
		// closes it again and makes the visual gate intermittent.
		if (hold < 90)
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
			if (getenv("CSRETRO_BUY_RESIZE_GATE"))
			{
				if (!resizeRequested)
				{
					resizeBeforeW = g_panel->GetWide();
					resizeBeforeH = g_panel->GetTall();
					resizeTargetW = resizeBeforeW == 1024 ? 1280 : 1024;
					resizeTargetH = resizeTargetW == 1024 ? 768 : 720;
					char cmd[80];
					std::snprintf(cmd, sizeof(cmd), "vid_setmode %d %d\n", resizeTargetW, resizeTargetH);
					Menu_Con("CSRETRO_BUY_GATE_RESIZE_REQUEST before=%dx%d target=%dx%d",
						resizeBeforeW, resizeBeforeH, resizeTargetW, resizeTargetH);
					MenuEngine::ClientCmdNow(cmd);
					resizeRequested = true;
					hold = 0;
					return;
				}
				if (g_panel->GetWide() != resizeTargetW || g_panel->GetTall() != resizeTargetH)
				{
					if (hold > 180)
						failDone("live resize dimensions");
					return;
				}
				const int resizeFit = g_panel->ButtonsOnPanel() ? 1 : 0;
				Menu_Con("CSRETRO_BUY_GATE_RESIZE before=%dx%d after=%dx%d fit=%d",
					resizeBeforeW, resizeBeforeH, g_panel->GetWide(), g_panel->GetTall(), resizeFit);
				if (!resizeFit)
				{
					failDone("live resize layout clip");
					return;
				}
			}
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
			int shield = 0;
			int molotov = 0;
			int incendiary = 0;
			int nightvision = 0;
			if (Panel *card = g_panel->FindChildByName("shield"))
				shield = card->IsVisible() ? 1 : 0;
			for (int i = 0; i < g_panel->GetChildCount(); ++i)
			{
				auto *btn = dynamic_cast<Button *>(g_panel->GetChild(i));
				if (!btn || !btn->IsVisible())
					continue;
				if (KeyValues *kv = btn->GetCommand())
				{
					const char *cmd = kv->GetString("command", "");
					if (IsShieldBuy(cmd))
						shield = 1;
					if (!strcasecmp(cmd, "molotov"))
						molotov = 1;
					if (!strcasecmp(cmd, "incgrenade"))
						incendiary = 1;
					if (!strcasecmp(cmd, "nvgs") || !strcasecmp(cmd, "nightvision"))
						nightvision = 1;
				}
			}
			Menu_Con("CSRETRO_BUY_GATE_OPEN type=%d visible=1 main=1 buttons=%d title=%d "
				 "pistols=%d shotguns=%d rifles=%d cancel=%d raw=%d team=%d shield=%d "
				 "molotov=%d incendiary=%d nightvision=%d",
				g_panel->MenuType(), g_panel->VisibleButtonCount(), title, pistols,
				shotguns, rifles, cancel, raw, g_panel->Team(), shield,
				molotov, incendiary, nightvision);
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
		const bool gateCt = std::getenv("CSRETRO_BUY_GATE_TEAM") &&
			!strcasecmp(std::getenv("CSRETRO_BUY_GATE_TEAM"), "ct");
		const char *sidearm = gateCt ? "usp" : "glock";
		Menu_Con("CSRETRO_BUY_GATE_DIRECT command=%s main=%d", sidearm,
			g_panel && g_panel->IsMainPage() ? 1 : 0);
		g_panel->OnCommand(sidearm);
		++step;
		hold = 0;
		return;
	}

	if (step == 7)
	{
		++hold;
		if (hold < 20)
			return;
		if (BuySelect_IsActive())
		{
			failDone("direct purchase did not close");
			return;
		}
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_BUY_GATE_DONE");
		step = 99;
	}
}
