#include "BuySelectPanel.h"

#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "GameConsoleDialog.h"
#include "InGameUi.h"
#include "RadioSelectPanel.h"
#include "TeamSelectPanel.h"
#include "TeamModelPreview.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
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

const char *TeamDefaultStem(bool ct)
{
	return ct ? "urban" : "terror";
}

// Buy is one large figure on the RIGHT of the panel. Team T yaw 150 is the
// LEFT showcase looking inward; the same yaw on the right shows Phoenix's
// back (brown jacket ≈ Guerrilla). Right-side inward facing is 202 for both
// teams — the CT team camera and the rightmost class-lineup card. Do not
// reuse class-lineup yaws (158/169/191) or the left-side team T yaw.
void BuyCameraForClass(const char *stem, bool ct, float *yaw, int *seq)
{
	(void)stem;
	*yaw = 202.0f;
	*seq = ct ? 33 : 80;
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

int BuyCatalogPrice(const char *command)
{
	struct Entry { const char *command; int price; };
	static const Entry prices[] = {
		{"vest", 650}, {"vesthelm", 1000}, {"defuser", 400},
		{"flash", 200}, {"hegren", 300}, {"sgren", 300},
		{"molotov", 400}, {"incgrenade", 500},
		{"glock", 200}, {"usp", 200}, {"p228", 400}, {"deagle", 700},
		{"elites", 500}, {"fn57", 500},
		{"mac10", 1050}, {"tmp", 1100}, {"mp5", 1500}, {"ump45", 1200},
		{"p90", 2350}, {"xm1014", 2000}, {"m3", 1700}, {"m249", 5200},
		{"galil", 1800}, {"famas", 1950}, {"ak47", 2700}, {"m4a1", 2900},
		{"sg552", 3000}, {"aug", 3300}, {"scout", 2750}, {"awp", 4750},
		{"g3sg1", 5000}, {"sg550", 5000},
	};
	if (!command)
		return 0;
	for (const Entry &entry : prices)
		if (!strcasecmp(command, entry.command))
			return entry.price;
	return 0;
}

const char *LoadoutFileName(bool ct)
{
	return ct ? "loadout_ct" : "loadout_t";
}

void DefaultLoadout(bool ct, char mid[5][16], char rifle[5][16])
{
	const char *m[5];
	const char *r[5];
	if (ct)
	{
		const char *cm[] = {"tmp", "mp5", "ump45", "p90", "xm1014"};
		const char *cr[] = {"famas", "m4a1", "scout", "awp", "sg550"};
		for (int i = 0; i < 5; ++i) { m[i] = cm[i]; r[i] = cr[i]; }
	}
	else
	{
		const char *tm[] = {"mac10", "mp5", "ump45", "p90", "xm1014"};
		const char *tr[] = {"galil", "ak47", "scout", "awp", "g3sg1"};
		for (int i = 0; i < 5; ++i) { m[i] = tm[i]; r[i] = tr[i]; }
	}
	for (int i = 0; i < 5; ++i)
	{
		snprintf(mid[i], 16, "%s", m[i]);
		snprintf(rifle[i], 16, "%s", r[i]);
	}
}

void MidPool(bool ct, const char **out, int *n)
{
	static const char *tPool[] = {"mac10", "mp5", "ump45", "p90", "xm1014", "m3", "m249"};
	static const char *ctPool[] = {"tmp", "mp5", "ump45", "p90", "xm1014", "m3", "m249"};
	*n = 7;
	for (int i = 0; i < 7; ++i)
		out[i] = (ct ? ctPool : tPool)[i];
}

void RiflePool(bool ct, const char **out, int *n)
{
	static const char *tPool[] = {"galil", "ak47", "sg552", "scout", "awp", "g3sg1"};
	static const char *ctPool[] = {"famas", "m4a1", "aug", "scout", "awp", "sg550"};
	*n = 6;
	for (int i = 0; i < 6; ++i)
		out[i] = (ct ? ctPool : tPool)[i];
}

bool InLoadoutSlots(char slots[5][16], const char *alias)
{
	for (int i = 0; i < 5; ++i)
		if (!strcasecmp(slots[i], alias))
			return true;
	return false;
}

void LoadoutPath(bool ct, char *out, size_t outsz)
{
	const char *basedir = getenv("XASH3D_BASEDIR");
	if (!basedir || !basedir[0])
		basedir = getenv("CSRETRO_RUN_DIR");
	if (basedir && basedir[0])
		snprintf(out, outsz, "%s/cstrike/%s", basedir, LoadoutFileName(ct));
	else
		snprintf(out, outsz, "%s", LoadoutFileName(ct));
}

void ReadLoadoutFile(bool ct, char mid[5][16], char rifle[5][16])
{
	DefaultLoadout(ct, mid, rifle);
	char path[512];
	LoadoutPath(ct, path, sizeof(path));
	FILE *f = fopen(path, "r");
	if (!f)
		return;
	char got[10][16] = {};
	int n = 0;
	while (n < 10 && fscanf(f, "%15s", got[n]) == 1)
		++n;
	fclose(f);
	if (n != 10)
		return;
	for (int i = 0; i < 5; ++i)
	{
		snprintf(mid[i], 16, "%s", got[i]);
		snprintf(rifle[i], 16, "%s", got[i + 5]);
	}
}

void WriteLoadoutFile(bool ct, char mid[5][16], char rifle[5][16])
{
	char path[512];
	LoadoutPath(ct, path, sizeof(path));
	FILE *f = fopen(path, "w");
	if (!f)
		return;
	fprintf(f, "%s %s %s %s %s %s %s %s %s %s\n",
		mid[0], mid[1], mid[2], mid[3], mid[4],
		rifle[0], rifle[1], rifle[2], rifle[3], rifle[4]);
	fclose(f);
}

void SendLoadoutToServer(char mid[5][16], char rifle[5][16])
{
	char buf[256];
	snprintf(buf, sizeof(buf), "cl_setloadout %s %s %s %s %s %s %s %s %s %s\n",
		mid[0], mid[1], mid[2], mid[3], mid[4],
		rifle[0], rifle[1], rifle[2], rifle[3], rifle[4]);
	MenuEngine::ClientCmdNow(buf);
}

const char *CompactBuyDisplayName(const std::string &name)
{
	struct Entry { const char *full; const char *compact; };
	static const Entry names[] = {
		{"Kevlar Vest", "Kevlar"}, {"Kevlar+Helm", "Kevlar+Helm"},
		{"Desert Eagle", "Deagle"},
		{"Dual Elites", "Elites"}, {"Flashbang", "Flash"},
		{"HE Grenade", "HE"}, {"Incendiary", "Incendiary"},
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
	if (!model)
		return;
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
		InGameUi::PaintBuyPlate(w, h);
	}
};

class CBuyColumnPlate : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBuyColumnPlate, Panel);

public:
	CBuyColumnPlate(Panel *parent, const char *name, int index)
		: BaseClass(parent, name)
		, m_index(index)
	{
		SetPaintBackgroundEnabled(true);
		SetPaintBorderEnabled(false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetZPos(-1);
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		InGameUi::PaintBuyColumn(w, h, m_index);
	}

private:
	int m_index = 0;
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

	void SetAccent(Color accent) { m_accent = accent; }

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		InGameUi::PaintBuyHeader(w, h, m_accent);
	}

private:
	Color m_accent = InGameUi::BuyGold();
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
		// Reference: shortcut at upper left, product name at upper right.
		m_name->SetContentAlignment(Label::a_east);
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
	void ConfigureWeapon(const char *command, const char *label, int cost, bool reserve = false, bool ct = false, bool rifle = false)
	{
		if (!command || !command[0])
			return;
		const int catalog = BuyCatalogPrice(command);
		if (catalog > 0)
			cost = catalog;
		if (cost <= 0 && !reserve)
			return;
		m_alias = command;
		m_reserve = reserve;
		m_ctTeam = ct;
		if (reserve)
		{
			char swap[64];
			snprintf(swap, sizeof(swap), "loadout_swap %s %s", rifle ? "rifle" : "mid", command);
			SetCommand(swap);
		}
		else
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
		m_price->SetVisible(cost > 0);
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

	void OnMousePressed(MouseCode code) override
	{
		if (code == MOUSE_RIGHT && m_isWeaponCard && !m_reserve && !m_alias.empty())
		{
			char buf[80];
			snprintf(buf, sizeof(buf), "buydrop %s\n", m_alias.c_str());
			MenuEngine::ClientCmdNow(buf);
			return;
		}
		BaseClass::OnMousePressed(code);
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
			const int imgH = std::max(1, h - imgY - (compact ? 3 : 4));
			if (m_weaponImage->PreviewCount() > 0)
			{
				m_weaponImage->SetVisible(true);
				m_weaponImage->SetBounds(pad, imgY, std::max(1, w - pad * 2), imgH);
			}
			else
				m_weaponImage->SetVisible(false);
			m_number->SetBounds(pad, 3, numberW, labelH);
			m_name->SetBounds(pad + numberW, 3, std::max(1, w - numberW - pad * 2), labelH);
			m_price->SetBounds(std::max(pad, w - 72), h - priceH - 2, 68, priceH);
			const Color accent = m_ctTeam ? InGameUi::CT() : InGameUi::BuyGold();
			const bool dim = Unaffordable();
			m_number->SetFgColor(dim ? Color(168, 170, 174, 220) : Color(210, 210, 214, 220));
			m_name->SetFgColor(dim ? InGameUi::TextDim() : accent);
			m_price->SetFgColor(dim ? Color(196, 168, 64, 230) : accent);
		}
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (m_isFooter)
		{
			InGameUi::PaintBuyFooter(w, h, m_accent, IsArmed() || IsDepressed());
			return;
		}
		InGameUi::PaintBuyCell(w, h, IsArmed() || IsDepressed(), Unaffordable());
	}

	void ApplySettings(KeyValues *inResourceData) override
	{
		BaseClass::ApplySettings(inResourceData);
		const char *cmd = inResourceData->GetString("command", "");
		if (!cmd[0])
			cmd = inResourceData->GetString("Command", "");
		if (cmd[0])
			SetCommand(cmd);
		const int cost = BuyCatalogPrice(cmd);
		m_isWeaponCard = cost > 0 && cmd[0] && !IsResCommand(cmd);
		if (m_isWeaponCard)
			ConfigureWeapon(cmd, nullptr, cost);
	}

private:
	Color m_accent = InGameUi::Text();
	CTeamModelPreview *m_weaponImage = nullptr;
	Label *m_number = nullptr;
	Label *m_name = nullptr;
	Label *m_price = nullptr;
	bool m_isWeaponCard = false;
	bool m_isFooter = false;
	bool m_reserve = false;
	bool m_ctTeam = false;
	int m_cost = 0;
	std::string m_productName;
	std::string m_alias;

	bool Unaffordable() const
	{
		if (m_reserve)
			return true;
		return m_isWeaponCard && m_cost > 0 && g_buyHud.money < m_cost;
	}

	void ApplyLook()
	{
		if (m_isFooter)
		{
			InGameUi::StyleFooterButton(this, m_accent);
			SetPaintBackgroundEnabled(true);
			SetContentAlignment(Label::a_center);
			SetTextInset(10, 0);
			return;
		}
		InGameUi::StyleCardButton(this, InGameUi::BuyGold());
		SetContentAlignment(m_isWeaponCard ? Label::a_northwest : Label::a_west);
		SetTextInset(m_isWeaponCard ? 0 : 12, m_isWeaponCard ? 0 : 0);
		if (m_isWeaponCard)
			SetText("");
		const bool dim = Unaffordable();
		SetFgColor(dim ? InGameUi::TextDim() : InGameUi::Text());
		SetBgColor((IsArmed() || IsDepressed()) ? InGameUi::BuyCellArmed() :
			(dim ? InGameUi::BuyCellDim() : InGameUi::BuyCell()));
		if (m_number)
			m_number->SetFgColor(dim ? Color(168, 170, 174, 220) : Color(210, 210, 214, 220));
		if (m_name)
			m_name->SetFgColor(dim ? InGameUi::TextDim() :
				(m_ctTeam ? InGameUi::CT() : InGameUi::BuyGold()));
		if (m_price)
			m_price->SetFgColor(dim ? Color(196, 168, 64, 230) :
				(m_ctTeam ? InGameUi::CT() : InGameUi::BuyGold()));
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
		std::fill(std::begin(m_columnPlates), std::end(m_columnPlates), nullptr);
		m_character = nullptr;
		m_classCaption = nullptr;
		m_plate = nullptr;
		m_money = nullptr;
		m_nextMin = nullptr;
		m_mates = nullptr;
		m_refundAll = nullptr;
		for (auto *&b : m_groundBtns)
			b = nullptr;
		m_groundBtns.clear();
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
		if (IsVisible() && m_character)
			ApplyOwnClassPreview();
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

		if (!strncasecmp(command, "loadout_swap ", 13))
		{
			const char *rest = command + 13;
			char kind[16] = {};
			char alias[16] = {};
			sscanf(rest, "%15s %15s", kind, alias);
			if (alias[0])
			{
				const bool rifle = !strcasecmp(kind, "rifle");
				char (*arr)[16] = rifle ? m_loadoutRifle : m_loadoutMid;
				if (!InLoadoutSlots(arr, alias))
					snprintf(arr[4], 16, "%s", alias);
				WriteLoadoutFile(m_team == TEAM_CT, m_loadoutMid, m_loadoutRifle);
				char buf[80];
				snprintf(buf, sizeof(buf), "cl_loadout_swap %s %s\n", rifle ? "rifle" : "mid", alias);
				MenuEngine::ClientCmdNow(buf);
				RebuildLoadoutCards();
			}
			return;
		}
		if (!strcasecmp(command, "refundall"))
		{
			MenuEngine::ClientCmdNow("refundall\n");
			return;
		}
		if (!strncasecmp(command, "buy_ground ", 11))
		{
			char buf[48];
			snprintf(buf, sizeof(buf), "%s\n", command);
			MenuEngine::ClientCmdNow(buf);
			return;
		}

		char mapped[64];
		MapSteamBuyAlias(mapped, sizeof(mapped), command);
		Menu_Con("CSRETRO_BUY_CMD %s", mapped);
		char buf[80];
		snprintf(buf, sizeof(buf), "%s\n", mapped);
		MenuEngine::ClientCmdNow(buf);
		if (!strcasecmp(mapped, "rebuy") || !strcasecmp(mapped, "autobuy") ||
			strcasecmp(mapped, "primammo") == 0 || strcasecmp(mapped, "secammo") == 0)
			return;
		CloseClientBuyMenu();
		return;
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_F12)
		{
			MenuEngine::ClientCmdNow("ready\n");
			return;
		}
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
	Label *m_classCaption = nullptr;
	Panel *m_plate = nullptr;
	Label *m_money = nullptr;
	Label *m_nextMin = nullptr;
	Label *m_mates = nullptr;
	CBuyHoverButton *m_refundAll = nullptr;
	std::vector<CBuyHoverButton *> m_groundBtns;
	char m_loadoutMid[5][16] = {};
	char m_loadoutRifle[5][16] = {};
	std::string m_characterModel;
	struct OverviewCard { CBuyHoverButton *button; int column; int row; };
	std::vector<OverviewCard> m_overview;
	Label *m_overviewTitles[5] = {};
	Panel *m_columnPlates[5] = {};

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
		const Color accent = (m_team == TEAM_CT) ? InGameUi::CT() : InGameUi::BuyGold();
		for (int col = 0; col < 5; ++col)
		{
			char plateName[32];
			snprintf(plateName, sizeof(plateName), "OverviewPlate%d", col);
			m_columnPlates[col] = new CBuyColumnPlate(this, plateName, col);
			char name[32];
			snprintf(name, sizeof(name), "OverviewTitle%d", col);
			auto *title = new CBuySectionLabel(this, name, titles[col]);
			title->SetContentAlignment(Label::a_center);
			title->SetAccent(accent);
			m_overviewTitles[col] = title;
		}

		const bool ct = m_team == TEAM_CT;
		ReadLoadoutFile(ct, m_loadoutMid, m_loadoutRifle);
		SendLoadoutToServer(m_loadoutMid, m_loadoutRifle);

		int nextRow[5] = {};
		auto addCard = [&](const char *cmd, int column, bool reserve, bool rifle) {
			if (!cmd || !cmd[0] || nextRow[column] >= 7)
				return;
			char name[64];
			snprintf(name, sizeof(name), "Overview%d_%d", column, nextRow[column]);
			auto *button = new CBuyHoverButton(this, name);
			button->ConfigureWeapon(cmd, nullptr, BuyCatalogPrice(cmd), reserve, ct, rifle);
			button->SetOverviewNumber(nextRow[column] + 1);
			m_overview.push_back({button, column, nextRow[column]++});
		};

		addCard("vest", 0, false, false);
		addCard("vesthelm", 0, false, false);
		if (ct)
			addCard("defuser", 0, false, false);
		addCard("flash", 4, false, false);
		addCard("hegren", 4, false, false);
		addCard("sgren", 4, false, false);
		addCard(ct ? "incgrenade" : "molotov", 4, false, false);

		if (ct)
		{
			addCard("usp", 1, false, false);
			addCard("glock", 1, false, false);
			addCard("p228", 1, false, false);
			addCard("deagle", 1, false, false);
			addCard("fn57", 1, false, false);
		}
		else
		{
			addCard("glock", 1, false, false);
			addCard("usp", 1, false, false);
			addCard("p228", 1, false, false);
			addCard("deagle", 1, false, false);
			addCard("elites", 1, false, false);
		}

		FillLoadoutColumns(addCard);

		HideSteamCategoryChrome();
		m_plate = new CBuyRoundedPanel(this, "BuyPlate");
		m_plate->SetPaintBackgroundEnabled(true);
		m_plate->SetPaintBorderEnabled(false);
		m_plate->SetBgColor(InGameUi::BuyPlate());
		m_plate->SetMouseInputEnabled(false);
		m_plate->SetKeyBoardInputEnabled(false);
		m_plate->SetZPos(-2);
	}

	template <typename AddFn>
	void FillLoadoutColumns(AddFn addCard)
	{
		const bool ct = m_team == TEAM_CT;
		for (int i = 0; i < 5; ++i)
			addCard(m_loadoutMid[i], 2, false, false);
		for (int i = 0; i < 5; ++i)
			addCard(m_loadoutRifle[i], 3, false, true);
		const char *midPool[8] = {};
		const char *riflePool[8] = {};
		int nMid = 0, nRifle = 0;
		MidPool(ct, midPool, &nMid);
		RiflePool(ct, riflePool, &nRifle);
		for (int i = 0; i < nMid; ++i)
		{
			if (!InLoadoutSlots(m_loadoutMid, midPool[i]))
				addCard(midPool[i], 2, true, false);
		}
		for (int i = 0; i < nRifle; ++i)
		{
			if (!InLoadoutSlots(m_loadoutRifle, riflePool[i]))
				addCard(riflePool[i], 3, true, true);
		}
	}

	void RebuildLoadoutCards()
	{
		std::vector<OverviewCard> kept;
		for (const OverviewCard &card : m_overview)
		{
			if (card.column < 2 || card.column == 4)
				kept.push_back(card);
			else
				delete card.button;
		}
		m_overview.swap(kept);
		int nextRowDummy[5] = {};
		for (const OverviewCard &card : m_overview)
			nextRowDummy[card.column] = std::max(nextRowDummy[card.column], card.row + 1);
		auto addCard = [&](const char *cmd, int column, bool reserve, bool rifle) {
			int row = 0;
			for (const OverviewCard &card : m_overview)
				if (card.column == column)
					row = std::max(row, card.row + 1);
			if (!cmd || !cmd[0] || row >= 7)
				return;
			char name[64];
			snprintf(name, sizeof(name), "Overview%d_%d", column, row);
			auto *button = new CBuyHoverButton(this, name);
			button->ConfigureWeapon(cmd, nullptr, BuyCatalogPrice(cmd), reserve, m_team == TEAM_CT, rifle);
			button->SetOverviewNumber(row + 1);
			button->SetVisible(true);
			m_overview.push_back({button, column, row});
		};
		FillLoadoutColumns(addCard);
		LayoutFamily();
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
		const char *remembered = ClassStemForTeam(g_buyClassModel, ct) ? g_buyClassModel : nullptr;
		const char *hud = ClassStemForTeam(g_buyHud.model, ct) ? g_buyHud.model : nullptr;
		const char *teamDefault = TeamDefaultStem(ct);
		(void)teamDefault;
		// What they clicked in class select is the figure. HUD userinfo can
		// be another player's skin (or lag) and must not replace that.
		if (!name && remembered)
		{
			name = remembered;
			src = "joinclass";
		}
		else if (!name && hud)
		{
			name = hud;
			src = "hud";
		}
		const char *model = name ? name : teamDefault;
		if (!m_characterModel.empty() && m_characterModel == model)
			return;
		float yaw = 0.0f;
		int seq = 0;
		BuyCameraForClass(model, ct, &yaw, &seq);
		m_characterModel = model;
		char path[96];
		snprintf(path, sizeof(path), "models/player/%s/%s.mdl", model, model);
		m_character->SetPreview(path, ct ? "models/p_m4a1.mdl" : "models/p_ak47.mdl", yaw, seq);
		m_character->SetWorldWidth(52.0f);
		m_character->SetWorldHeight(80.0f);
		if (m_classCaption)
			m_classCaption->SetVisible(false);
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
		m_classCaption = new Label(this, "BuyClassName", "");
		m_classCaption->SetContentAlignment(Label::a_center);
		m_classCaption->SetPaintBackgroundEnabled(false);
		m_classCaption->SetFgColor(InGameUi::BuyGold());
		m_classCaption->SetMouseInputEnabled(false);
		m_classCaption->SetKeyBoardInputEnabled(false);
		m_classCaption->SetZPos(2);
		m_characterModel.clear();
		ApplyOwnClassPreview();
	}

	void BuildRuntimeChrome()
	{
		const Color accent = (m_team == TEAM_CT) ? InGameUi::CT() : InGameUi::BuyGold();
		m_money = new Label(this, "BuyMoney", "");
		m_money->SetContentAlignment(Label::a_west);
		m_money->SetPaintBackgroundEnabled(false);
		m_money->SetFgColor(accent);
		m_money->SetMouseInputEnabled(false);
		m_money->SetKeyBoardInputEnabled(false);
		m_nextMin = new Label(this, "BuyNextMin", "");
		m_nextMin->SetContentAlignment(Label::a_west);
		m_nextMin->SetPaintBackgroundEnabled(false);
		m_nextMin->SetFgColor(accent);
		m_nextMin->SetMouseInputEnabled(false);
		m_mates = new Label(this, "BuyTeammates", "");
		m_mates->SetContentAlignment(Label::a_west);
		m_mates->SetPaintBackgroundEnabled(false);
		m_mates->SetFgColor(InGameUi::TextDim());
		m_mates->SetMouseInputEnabled(false);
		m_refundAll = new CBuyHoverButton(this, "RefundAllButton");
		m_refundAll->SetCommand("refundall");
		m_refundAll->SetText("REFUND ALL");
		m_refundAll->SetFooter(true);
		m_refundAll->SetAccent(accent);
		for (int i = 0; i < 4; ++i)
		{
			char name[32];
			snprintf(name, sizeof(name), "BuyGround%d", i);
			auto *btn = new CBuyHoverButton(this, name);
			btn->SetFooter(true);
			btn->SetVisible(false);
			m_groundBtns.push_back(btn);
		}
		UpdateRuntimeChrome();
	}

	void UpdateRuntimeChrome()
	{
		const Color accent = (m_team == TEAM_CT) ? InGameUi::CT() : InGameUi::BuyGold();
		if (m_money)
		{
			char money[32];
			std::snprintf(money, sizeof(money), "$%d", std::max(0, g_buyHud.money));
			m_money->SetText(money);
			m_money->SetFgColor(accent);
		}
		if (m_nextMin)
		{
			char text[48];
			if (g_buyHud.nextRoundMin > 0)
				std::snprintf(text, sizeof(text), "Next round min  $%d", g_buyHud.nextRoundMin);
			else
				std::snprintf(text, sizeof(text), "Next round min  $%d",
					std::max(0, g_buyHud.money + g_buyHud.lossBonus));
			m_nextMin->SetText(text);
			m_nextMin->SetFgColor(accent);
		}
		if (m_mates)
			m_mates->SetText(g_buyHud.teammates[0] ? g_buyHud.teammates : "");
		if (!m_groundBtns.empty())
		{
			char ground[81] = {};
			snprintf(ground, sizeof(ground), "%s", g_buyHud.ground);
			char *save = nullptr;
			char *tok = strtok_r(ground, ",", &save);
			size_t i = 0;
			for (; tok && i < m_groundBtns.size(); ++i, tok = strtok_r(nullptr, ",", &save))
			{
				int ent = 0;
				char alias[16] = {};
				if (sscanf(tok, "%d:%15s", &ent, alias) != 2)
				{
					m_groundBtns[i]->SetVisible(false);
					continue;
				}
				char cmd[32];
				snprintf(cmd, sizeof(cmd), "buy_ground %d", ent);
				m_groundBtns[i]->SetCommand(cmd);
				char label[32];
				snprintf(label, sizeof(label), "PICK %s", alias);
				m_groundBtns[i]->SetText(label);
				m_groundBtns[i]->SetVisible(true);
				m_groundBtns[i]->SetFooter(true);
			}
			for (; i < m_groundBtns.size(); ++i)
				m_groundBtns[i]->SetVisible(false);
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
			std::snprintf(text, sizeof(text), "Buy Time Remaining  --:--");
		else
			std::snprintf(text, sizeof(text), "Buy Time Remaining  %02d:%02d",
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
		const Color gold = InGameUi::BuyGold();
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn)
				continue;
			const char *name = btn->GetName();
			const bool footer = name && (!strcasecmp(name, "AutobuyButton") ||
				!strcasecmp(name, "RebuyButton") || !strcasecmp(name, "CancelButton") ||
				!strcasecmp(name, "cancelbutton") || !strcasecmp(name, "RefundAllButton") ||
				!strncasecmp(name, "BuyGround", 9));
			const Color accent = (m_team == TEAM_CT) ? InGameUi::CT() : InGameUi::BuyGold();
			if (auto *look = dynamic_cast<CBuyHoverButton *>(btn))
			{
				look->SetFooter(footer);
				look->SetAccent(footer ? accent : ((m_team == TEAM_CT) ? InGameUi::CT() : InGameUi::BuyGold()));
				continue;
			}
			if (footer)
				InGameUi::StyleFooterButton(btn, accent);
			else
				InGameUi::StyleCardButton(btn, gold);
			btn->SetContentAlignment(footer ? Label::a_center : Label::a_west);
			btn->SetTextInset(footer ? 0 : 12, 0);
		}
		if (auto *title = dynamic_cast<Label *>(FindChildByName("Title")))
		{
			InGameUi::StyleTitle(title);
			title->SetFgColor(gold);
		}
		if (auto *cat = dynamic_cast<Label *>(FindChildByName("selectCategory")))
		{
			cat->SetVisible(false);
		}
		const Color section = (m_team == TEAM_CT) ? InGameUi::CT() : InGameUi::BuyGold();
		for (Label *title : m_overviewTitles)
		{
			if (!title)
				continue;
			title->SetFgColor(section);
			title->SetPaintBackgroundEnabled(true);
			title->SetContentAlignment(Label::a_center);
			if (auto *look = dynamic_cast<CBuySectionLabel *>(title))
				look->SetAccent(section);
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
			m_money->SetFgColor(InGameUi::BuyGold());
		}
		if (m_classCaption)
			m_classCaption->SetFgColor(InGameUi::BuyGold());
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
			m_plate->SetBgColor(InGameUi::BuyPlate());
	}

	void RelayoutMainGrid()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 400 || h < 300)
			return;
		const float scale = std::max(0.55f, std::min(1.15f,
			std::min(static_cast<float>(w) / 1280.0f, static_cast<float>(h) / 720.0f)));
		const int stageW = std::min(w - 16, static_cast<int>(1200.0f * scale));
		const int stageH = std::min(h - 16, static_cast<int>(680.0f * scale));
		const int stageX = (w - stageW) / 2;
		const int stageY = std::max(8, (h - stageH) / 2);
		const int footerGap = std::max(16, static_cast<int>(22.0f * scale));
		const int bh = std::max(26, static_cast<int>(32.0f * scale));
		const int by = h - bh - std::max(14, static_cast<int>(18.0f * scale));
		const int charGap = std::max(10, static_cast<int>(18.0f * scale));
		const int charW = std::max(180, stageW * 26 / 100);
		const int plateX = stageX;
		const int plateW = std::max(360, stageW - charW - charGap);
		const int titleY = stageY + static_cast<int>(22.0f * scale);
		const int titleH = std::max(22, static_cast<int>(28.0f * scale));
		const int headerY = titleY + titleH;
		const int headerH = std::max(18, static_cast<int>(24.0f * scale));
		const int gridY = headerY + headerH + std::max(4, static_cast<int>(8.0f * scale));
		int maxRows = 1;
		for (const OverviewCard &card : m_overview)
			maxRows = std::max(maxRows, card.row + 1);
		const int rowGap = std::max(3, static_cast<int>(6.0f * scale));
		const int colGap = std::max(6, static_cast<int>(10.0f * scale));
		const float cardAspect = 2.20f;
		int cellW = std::max(88, (plateW - colGap * 4) / 5);
		int cellH = std::max(40, static_cast<int>(static_cast<float>(cellW) / cardAspect + 0.5f));
		const int gridLimit = std::max(gridY + 80, by - std::max(28, static_cast<int>(40.0f * scale)));
		const int naturalH = cellH * maxRows + rowGap * std::max(0, maxRows - 1);
		if (gridY + naturalH > gridLimit && naturalH > 0)
		{
			const float shrink = static_cast<float>(gridLimit - gridY) / static_cast<float>(naturalH);
			cellW = std::max(80, static_cast<int>(static_cast<float>(cellW) * shrink));
			cellH = std::max(40, static_cast<int>(static_cast<float>(cellW) / cardAspect + 0.5f));
		}
		const int usedW = cellW * 5 + colGap * 4;
		const int gridX = plateX + std::max(0, (plateW - usedW) / 2);
		int colX[5] = {};
		int colW[5] = {};
		for (int col = 0; col < 5; ++col)
		{
			colX[col] = gridX + col * (cellW + colGap);
			colW[col] = cellW;
		}
		const int gridBottom = gridY + cellH * maxRows + rowGap * std::max(0, maxRows - 1);
		if (auto *title = FindChildByName("Title"))
			title->SetBounds(plateX, titleY, plateW, titleH);
		if (auto *autoBuy = dynamic_cast<Button *>(FindChildByName("AutobuyButton")))
			autoBuy->SetText("F3  AUTO BUY");
		if (auto *rebuy = dynamic_cast<Button *>(FindChildByName("RebuyButton")))
			rebuy->SetText("F4  REBUY PREVIOUS");
		if (auto *cancel = dynamic_cast<Button *>(FindChildByName("CancelButton")))
			cancel->SetText("ESC  BACK");
		if (m_refundAll)
			m_refundAll->SetText("SELL ALL");
		HideSteamCategoryChrome();
		if (m_plate)
			m_plate->SetVisible(false);
		int colRows[5] = {};
		for (const OverviewCard &card : m_overview)
			colRows[card.column] = std::max(colRows[card.column], card.row + 1);
		const int platePad = std::max(3, colGap / 2);
		for (int col = 0; col < 5; ++col)
		{
			if (m_columnPlates[col])
			{
				if (colRows[col] <= 0)
				{
					m_columnPlates[col]->SetVisible(false);
					continue;
				}
				const int colBottom = gridY + colRows[col] * cellH + rowGap * std::max(0, colRows[col] - 1);
				m_columnPlates[col]->SetVisible(true);
				m_columnPlates[col]->SetBounds(colX[col] - platePad / 2, headerY,
					colW[col] + platePad, colBottom - headerY + platePad);
			}
			if (m_overviewTitles[col])
				m_overviewTitles[col]->SetBounds(colX[col], headerY, colW[col], headerH);
		}
		for (const OverviewCard &card : m_overview)
			card.button->SetBounds(colX[card.column],
				gridY + card.row * (cellH + rowGap), colW[card.column], cellH);
		if (m_money)
			m_money->SetBounds(std::max(24, w * 4 / 100), h - std::max(72, static_cast<int>(80.0f * scale)),
				std::max(140, static_cast<int>(200.0f * scale)), std::max(22, static_cast<int>(28.0f * scale)));
		if (m_nextMin)
			m_nextMin->SetBounds(std::max(24, w * 4 / 100), h - std::max(50, static_cast<int>(56.0f * scale)),
				std::max(180, static_cast<int>(240.0f * scale)), std::max(18, static_cast<int>(22.0f * scale)));
		if (m_mates)
			m_mates->SetBounds(plateX, gridBottom + std::max(4, static_cast<int>(6.0f * scale)),
				plateW, std::max(16, static_cast<int>(18.0f * scale)));

		struct FooterSpec
		{
			const char *name;
			int baseW;
		};
		const FooterSpec specs[] = {
			{"RebuyButton", 160},
			{"AutobuyButton", 120},
			{"RefundAllButton", 100},
			{"CancelButton", 80},
		};
		std::vector<Panel *> bottom;
		std::vector<int> bottomW;
		for (const FooterSpec &spec : specs)
		{
			if (auto *p = dynamic_cast<Button *>(FindChildByName(spec.name)))
			{
				if (!p->IsVisible())
					continue;
				int tw = 0, th = 0;
				p->GetContentSize(tw, th);
				bottom.push_back(p);
				bottomW.push_back(std::max(static_cast<int>(spec.baseW * scale * 0.45f), tw + 24));
			}
		}
		// Footer Y/H already computed above so the weapon cards can sit above it.
		if (!bottom.empty())
		{
			int totalW = footerGap * static_cast<int>(bottom.size() - 1);
			for (int width : bottomW)
				totalW += width;
			int x = stageX + (stageW - totalW) / 2;
			for (size_t i = 0; i < bottom.size(); ++i)
			{
				bottom[i]->SetBounds(x, by, bottomW[i], bh);
				x += bottomW[i] + footerGap;
			}
		}
		int gx = plateX;
		const int gh = std::max(18, static_cast<int>(20.0f * scale));
		const int gy = gridBottom + std::max(22, static_cast<int>(24.0f * scale));
		for (CBuyHoverButton *btn : m_groundBtns)
		{
			if (!btn || !btn->IsVisible())
				continue;
			btn->SetBounds(gx, gy, std::max(70, static_cast<int>(90.0f * scale)), gh);
			gx += std::max(76, static_cast<int>(96.0f * scale));
		}
		if (m_character)
		{
			m_character->SetVisible(true);
			const int charX = plateX + plateW + charGap;
			const int charRight = stageX + stageW - std::max(8, static_cast<int>(12.0f * scale));
			const int charW = charRight - charX;
			const int charY = stageY + static_cast<int>(8.0f * scale);
			const int charBottom = by - std::max(8, static_cast<int>(10.0f * scale));
			m_character->SetBounds(charX, charY, std::max(1, charW),
				std::max(1, charBottom - charY));
			if (m_classCaption)
				m_classCaption->SetVisible(false);
		}
	}

	void RelayoutWeaponList()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 400 || h < 300)
			return;
		const float scale = std::max(0.55f, std::min(1.15f,
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
		const int cols = weapons.size() > 1 ? 2 : 1;
		const int rows = weapons.empty() ? 1 :
			(static_cast<int>(weapons.size()) + cols - 1) / cols;
		const float cardAspect = 2.20f;
		int cardW = (listW - gap * (cols - 1)) / std::max(1, cols);
		int rowH = std::max(40, static_cast<int>(static_cast<float>(cardW) / cardAspect + 0.5f));
		const int listLimit = stageY + stageH - std::max(36, static_cast<int>(44.0f * scale));
		const int naturalH = rowH * rows + gap * std::max(0, rows - 1);
		if (listY + naturalH > listLimit && naturalH > 0)
		{
			const float shrink = static_cast<float>(listLimit - listY) / static_cast<float>(naturalH);
			cardW = std::max(80, static_cast<int>(static_cast<float>(cardW) * shrink));
			rowH = std::max(40, static_cast<int>(static_cast<float>(cardW) / cardAspect));
		}
		const int usedW = cardW * cols + gap * std::max(0, cols - 1);
		const int listX = stageX + std::max(0, (listW - usedW) / 2);
		for (size_t i = 0; i < weapons.size(); ++i)
		{
			const int col = static_cast<int>(i) % cols;
			const int row = static_cast<int>(i) / cols;
			weapons[i]->SetBounds(listX + col * (cardW + gap), listY + row * (rowH + gap), cardW, rowH);
		}
		if (cancel)
		{
			cancel->SetText("ESC  BACK");
			int tw = 0, th = 0;
			cancel->GetContentSize(tw, th);
			const int cancelW = std::max(72, tw + 12);
			cancel->SetBounds(stageX + (stageW - cancelW) / 2,
				stageY + stageH - std::max(28, static_cast<int>(32.0f * scale)),
				cancelW, std::max(22, static_cast<int>(26.0f * scale)));
		}
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
			if (m_classCaption)
				m_classCaption->SetVisible(false);
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
		InGameUi::ContentCanvas(pw, ph, canvasX, canvasY, canvasW, canvasH);
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
		if (!surface())
			return;
		// CS:GO buy: the live map stays sharp. Only a light veil.
		InGameUi::PaintOverlay(w, h);
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
		if (GameConsole_IsActive())
			return;
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
	MenuEngine::ClientCmdNow("-forward;-back;-moveleft;-moveright;-jump;-speed;-duck\n");
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
		!RadioSelect_IsActive() && !GameConsole_IsActive())
	{
		MenuEngine::RestoreGameKeyDest();
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
	g_buyClassModel[0] = '\0';
	g_buyHud = {};
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
	g_keyDestPushed = false;
}

void BuySelect_AfterFrame()
{
	const bool console = GameConsole_IsActive();
	if (g_overlay)
		g_overlay->SetKeyBoardInputEnabled(!console);
	if (g_panel)
		g_panel->SetKeyBoardInputEnabled(!console);
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

const char *BuySelect_PlayerClass(bool ct)
{
	if (ClassStemForTeam(g_buyClassModel, ct))
		return g_buyClassModel;
	if (ClassStemForTeam(g_buyHud.model, ct))
		return g_buyHud.model;
	return nullptr;
}

void BuySelect_SetHud(const BuyHudState *state)
{
	if (!state)
		return;
	const bool modelChanged = std::strcmp(g_buyHud.model, state->model) != 0;
	g_buyHud = *state;
	if (g_panel)
	{
		const bool ct = g_panel->Team() == TEAM_CT;
		// Phoenix/SEAL stems are also the team defaults. HUD userinfo can
		// still be another slot's skin (Arctic bot). Never replace joinclass.
		if (!g_buyClassModel[0] && ClassStemForTeam(g_buyHud.model, ct))
			BuySelect_RememberClass(g_buyHud.model);
	}
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
