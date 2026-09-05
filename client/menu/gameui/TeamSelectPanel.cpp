#include "TeamSelectPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "InGameViewportLook.h"
#include "RadioSelectPanel.h"

#include <tier1/KeyValues.h>
#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/RichText.h>

#include "../src/menu_priv.h"
#include "cdll_dll.h"
#include "keydefs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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

class CTeamLookButton : public Button
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CTeamLookButton, Button);

public:
	CTeamLookButton(Panel *parent, const char *name)
		: BaseClass(parent, name, "")
	{
	}

	void SetAccent(Color accent)
	{
		m_accent = accent;
		ApplyLook();
	}

	void SetTeamChoice(bool teamChoice) { m_teamChoice = teamChoice; }

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		ApplyLook();
	}

	void PerformLayout() override
	{
		BaseClass::PerformLayout();
		ApplyLook();
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (!m_teamChoice)
		{
			InGameViewportLook::PaintCardBackground(w, h, m_accent, IsArmed() || IsDepressed());
			return;
		}
		const bool active = IsArmed() || IsDepressed();
		vgui2::surface()->DrawSetColor(m_accent.r(), m_accent.g(), m_accent.b(), active ? 42 : 12);
		vgui2::surface()->DrawFilledRect(0, 0, w, h);
		vgui2::surface()->DrawSetColor(m_accent.r(), m_accent.g(), m_accent.b(), active ? 255 : 130);
		vgui2::surface()->DrawFilledRect(0, h - (active ? 3 : 2), w, h);
	}

private:
	Color m_accent = InGameViewportLook::Text();
	bool m_teamChoice = false;

	void ApplyLook()
	{
		InGameViewportLook::StyleCardButton(this, m_accent);
		SetFgColor((IsArmed() || IsDepressed()) ? InGameViewportLook::Text() : m_accent);
		SetBgColor((IsArmed() || IsDepressed()) ? InGameViewportLook::CardArmed() : InGameViewportLook::Card());
	}
};

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
		CreateTeamPreviews();
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
		RelayoutVisibleButtons();
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
		if (controlName && !strcasecmp(controlName, "Button"))
			return new CTeamLookButton(nullptr, nullptr);
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
	ImagePanel *m_tPreview = nullptr;
	ImagePanel *m_ctPreview = nullptr;

	void CreateTeamPreviews()
	{
		auto create = [&](const char *buttonName, const char *panelName, const char *image) {
			auto *button = dynamic_cast<Button *>(FindChildByName(buttonName));
			if (!button)
				return static_cast<ImagePanel *>(nullptr);
			button->SetContentAlignment(Label::a_south);
			button->SetTextInset(0, 12);
			auto *preview = new ImagePanel(button, panelName);
			preview->SetImage(image);
			preview->SetShouldScaleImage(true);
			preview->SetMouseInputEnabled(false);
			preview->SetKeyBoardInputEnabled(false);
			return preview;
		};
		m_tPreview = create("terbutton", "TerrorPreview", "gfx/vgui/terror");
		m_ctPreview = create("ctbutton", "CTPreview", "gfx/vgui/urban");
	}

	void LayoutPreview(ImagePanel *preview, int cardW, int cardH)
	{
		if (!preview)
			return;
		const int maxH = std::max(1, cardH - 44);
		const int iw = std::min(cardW - 24, maxH * 256 / 196);
		const int ih = iw * 196 / 256;
		preview->SetBounds((cardW - iw) / 2, 8, iw, ih);
	}

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
		for (int i = 0; i < GetChildCount(); ++i)
		{
			auto *btn = dynamic_cast<Button *>(GetChild(i));
			if (!btn)
				continue;
			const char *name = btn->GetName();
			Color accent = InGameViewportLook::Text();
			if (name && !strcasecmp(name, "terbutton"))
				accent = InGameViewportLook::Terror();
			else if (name && !strcasecmp(name, "ctbutton"))
				accent = InGameViewportLook::CT();
			const bool teamCard = name && (!strcasecmp(name, "terbutton") || !strcasecmp(name, "ctbutton"));
			if (auto *look = dynamic_cast<CTeamLookButton *>(btn))
			{
				look->SetAccent(accent);
				look->SetTeamChoice(teamCard);
			}
			else
				InGameViewportLook::StyleCardButton(btn, accent);
			btn->SetContentAlignment(teamCard ? Label::a_south : Label::a_center);
			btn->SetTextInset(0, teamCard ? 12 : 0);
		}
		InGameViewportLook::StyleTitle(dynamic_cast<Label *>(FindChildByName("joinTeam")));
		if (auto *map = dynamic_cast<Label *>(FindChildByName("mapname")))
			map->SetFgColor(InGameViewportLook::TextDim());
		if (auto *info = dynamic_cast<RichText *>(FindChildByName("MapInfo")))
		{
			info->SetPaintBackgroundEnabled(true);
			info->SetFgColor(InGameViewportLook::TextDim());
			info->SetBgColor(InGameViewportLook::Card());
		}
	}

	void RelayoutVisibleButtons()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 200 || h < 160)
			return;
		const int pad = w / 11;
		const int gap = w / 40;
		const int titleH = h / 12;
		if (auto *title = FindChildByName("joinTeam"))
		{
			title->SetBounds(pad, h / 20, w - pad * 2, titleH);
			if (auto *lab = dynamic_cast<Label *>(title))
				lab->SetContentAlignment(Label::a_center);
		}
		if (auto *map = FindChildByName("mapname"))
			map->SetBounds(pad, h / 20 + titleH, w - pad * 2, titleH / 2);

		const int cardY = h * 16 / 100;
		const int cardH = h * 62 / 100;
		const int cardW = (w - pad * 2 - gap) / 2;
		if (Panel *t = FindChildByName("terbutton"))
			if (t->IsVisible())
				t->SetBounds(pad, cardY, cardW, cardH);
		if (Panel *ct = FindChildByName("ctbutton"))
			if (ct->IsVisible())
				ct->SetBounds(pad + cardW + gap, cardY, cardW, cardH);
		LayoutPreview(m_tPreview, cardW, cardH);
		LayoutPreview(m_ctPreview, cardW, cardH);

		std::vector<Panel *> row;
		for (const char *name : {"vipbutton", "autobutton", "specbutton", "CancelButton"})
		{
			if (Panel *p = FindChildByName(name))
				if (p->IsVisible())
					row.push_back(p);
		}
		const int rowY = h * 82 / 100;
		const int rowH = h * 8 / 100;
		if (!row.empty())
		{
			const int rw = (w - pad * 2 - gap * static_cast<int>(row.size() - 1)) /
				static_cast<int>(row.size());
			for (size_t i = 0; i < row.size(); ++i)
				row[i]->SetBounds(pad + static_cast<int>(i) * (rw + gap), rowY, rw, rowH);
		}

		if (Panel *info = FindChildByName("MapInfo"))
		{
			info->SetVisible(false);
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
		info->SetFgColor(InGameViewportLook::TextDim());
		info->SetBgColor(InGameViewportLook::Card());
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
		Menu_Con("CSRETRO_TEAM_LOOK split=1");
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
		SetPaintBackgroundEnabled(true);
		SetPaintBorderEnabled(false);
		SetBorder(nullptr);
		SetBgColor(InGameViewportLook::OverlayBg());
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		InGameViewportLook::PaintSplitBackdrop(w, h);
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
		if (m_team)
		{
			int w = 0, h = 0;
			GetSize(w, h);
			m_team->SetBounds(0, 0, w, h);
			m_team->LayoutFamily();
		}
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
	if (g_keyDestPushed && !gMenuVisible && !ClassSelect_IsActive() && !BuySelect_IsActive() &&
		!RadioSelect_IsActive())
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
		if (hold == 20)
			MenuEngine::ClientCmdNow("developer 0; clear\n");
		if (hold < 140)
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
