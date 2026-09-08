#include "TeamSelectPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "InGameRoster.h"
#include "InGameViewportLook.h"
#include "TeamModelPreview.h"
#include "RadioSelectPanel.h"

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
#include <algorithm>
#include <strings.h>

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

	void SetTeamChoice(int team) { m_teamChoice = team; }
	void SetFooter(int footer) { m_footer = footer; }

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
		const bool active = IsArmed() || IsDepressed();
		if (m_footer)
		{
			const int iconX = std::max(6, w - (m_footer == 1 ? 100 : 132));
			const int iconY = std::max(3, (h - 10) / 2);
			vgui2::surface()->DrawSetColor(m_accent);
			if (m_footer == 1)
			{
				vgui2::surface()->DrawOutlinedRect(iconX, iconY + 1, iconX + 12, iconY + 9);
				vgui2::surface()->DrawFilledRect(iconX + 12, iconY + 3, iconX + 15, iconY + 7);
			}
			else if (m_footer == 2)
			{
				vgui2::surface()->DrawLine(iconX + 2, iconY + 6, iconX + 4, iconY + 2);
				vgui2::surface()->DrawLine(iconX + 4, iconY + 2, iconX + 9, iconY);
				vgui2::surface()->DrawLine(iconX + 9, iconY, iconX + 13, iconY + 3);
				vgui2::surface()->DrawLine(iconX + 13, iconY + 3, iconX + 13, iconY + 6);
				vgui2::surface()->DrawLine(iconX + 13, iconY + 6, iconX + 10, iconY + 10);
				vgui2::surface()->DrawLine(iconX + 10, iconY + 10, iconX + 5, iconY + 10);
				vgui2::surface()->DrawLine(iconX + 5, iconY + 10, iconX + 2, iconY + 7);
				vgui2::surface()->DrawFilledRect(iconX + 10, iconY + 1, iconX + 15, iconY + 5);
			}
			if (active)
			{
				vgui2::surface()->DrawSetColor(m_accent.r(), m_accent.g(), m_accent.b(), 40);
				vgui2::surface()->DrawFilledRect(0, h - 2, w, h);
			}
			return;
		}
		if (!m_teamChoice)
		{
			InGameViewportLook::PaintCardBackground(w, h, m_accent, active);
			return;
		}
		int cx = 0, cy = 0, radius = 0;
		InGameViewportLook::TeamEmblemMetrics(w, h, cx, cy, radius);
		InGameViewportLook::PaintEmblem(m_teamChoice, cx, cy, radius, m_accent, active);
	}

	void Paint() override
	{
		if (m_teamChoice)
			return;
		BaseClass::Paint();
	}

private:
	Color m_accent = InGameViewportLook::Text();
	int m_teamChoice = 0;
	int m_footer = 0;

	void ApplyLook()
	{
		if (m_footer)
		{
			InGameViewportLook::StyleFooterButton(this, m_accent);
			SetPaintBackgroundEnabled(true);
			SetFgColor((IsArmed() || IsDepressed()) ? InGameViewportLook::Text() : m_accent);
			return;
		}
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
		SetProportional(false);
		StyleButtons();
		CreateTeamPreviews();
		CreateChrome();
		CreateRosters();
	}

	void PerformLayout() override
	{
		RelayoutVisibleButtons();
	}

	void OnThink() override
	{
		BaseClass::OnThink();
		const float now = gGlobals ? gGlobals->time : 0.0f;
		if (now < m_nextRosterRefresh)
			return;
		m_nextRosterRefresh = now + 0.25f;
		RefreshRoster();
	}

	bool HasTeamButtons()
	{
		return FindChildByName("terbutton") && FindChildByName("ctbutton");
	}

	void Open(int validSlots)
	{
		RandomizeTeamPreviews();
		m_slots = validSlots;
		if (m_slots == 0)
			m_slots = MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_5 | MENU_KEY_6;
		ApplySlots();
		LoadMapBriefing();
		RefreshRoster();
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
		RefreshRoster();
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
	int TerrorRosterCount() const { return m_rosterT; }
	int CTRosterCount() const { return m_rosterCT; }
	bool RosterHasName(const char *name) const
	{
		if (!name || !name[0])
			return false;
		for (int i = 0; i < kRosterRows; ++i)
		{
			char text[80] = {};
			if (m_tPlayers[i])
			{
				m_tPlayers[i]->GetText(text, sizeof(text));
				if (!strcmp(text, name))
					return true;
			}
			if (m_ctPlayers[i])
			{
				m_ctPlayers[i]->GetText(text, sizeof(text));
				if (!strcmp(text, name))
					return true;
			}
		}
		return false;
	}

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
	// A regular 32-slot match can expose sixteen names per side without
	// truncating a balanced server.  Team totals below still count every row,
	// including deliberately unbalanced teams larger than this visual column.
	enum { kRosterRows = 16 };

	int m_slots = 0;
	int m_rosterT = 0;
	int m_rosterCT = 0;
	float m_nextRosterRefresh = 0.0f;
	CTeamModelPreview *m_tModel = nullptr;
	CTeamModelPreview *m_ctModel = nullptr;
	Label *m_tTitle = nullptr;
	Label *m_ctTitle = nullptr;
	Label *m_tCount = nullptr;
	Label *m_ctCount = nullptr;
	Label *m_tPlayers[kRosterRows]{};
	Label *m_ctPlayers[kRosterRows]{};
	int m_tPreviewIndex = -1;
	int m_ctPreviewIndex = -1;

	void CreateTeamPreviews()
	{
		m_tModel = new CTeamModelPreview(this, "TerrorModel");
		m_ctModel = new CTeamModelPreview(this, "CTModel");
	}

	void RandomizeTeamPreviews()
	{
		static const char *const terrorModels[] = {
			"models/player/terror/terror.mdl",
			"models/player/leet/leet.mdl",
			"models/player/arctic/arctic.mdl",
			"models/player/guerilla/guerilla.mdl",
		};
		static const char *const ctModels[] = {
			"models/player/urban/urban.mdl",
			"models/player/gsg9/gsg9.mdl",
			"models/player/sas/sas.mdl",
			"models/player/gign/gign.mdl",
		};
		const int oldT = m_tPreviewIndex;
		const int oldCT = m_ctPreviewIndex;
		if (m_tPreviewIndex < 0)
			m_tPreviewIndex = gEng.pfnRandomLong(0, 3);
		else
			m_tPreviewIndex = (m_tPreviewIndex + gEng.pfnRandomLong(1, 3)) % 4;
		if (m_ctPreviewIndex < 0)
			m_ctPreviewIndex = gEng.pfnRandomLong(0, 3);
		else
			m_ctPreviewIndex = (m_ctPreviewIndex + gEng.pfnRandomLong(1, 3)) % 4;
		m_tModel->SetPreview(terrorModels[m_tPreviewIndex], "models/p_ak47.mdl", 154.0f, 80);
		m_ctModel->SetPreview(ctModels[m_ctPreviewIndex], "models/p_m4a1.mdl", 206.0f, 33);
		m_tModel->SetWorldWidth(65.0f);
		m_ctModel->SetWorldWidth(65.0f);
		Menu_Con("CSRETRO_TEAM_RANDOM t=%d ct=%d changed=%d", m_tPreviewIndex,
			m_ctPreviewIndex, oldT >= 0 && oldCT >= 0 && oldT != m_tPreviewIndex && oldCT != m_ctPreviewIndex ? 1 : 0);
	}

	void MuteLabel(Label *lab)
	{
		if (!lab)
			return;
		lab->SetMouseInputEnabled(false);
		lab->SetKeyBoardInputEnabled(false);
		lab->SetPaintBackgroundEnabled(false);
	}

	void CreateChrome()
	{
		m_tTitle = new Label(this, "TTitle", "TERRORISTS");
		m_ctTitle = new Label(this, "CTTitle", "COUNTER-TERRORISTS");
		m_tCount = new Label(this, "TMeta", "0 Players");
		m_ctCount = new Label(this, "CTMeta", "0 Players");
		MuteLabel(m_tTitle);
		MuteLabel(m_ctTitle);
		MuteLabel(m_tCount);
		MuteLabel(m_ctCount);
		m_tTitle->SetContentAlignment(Label::a_center);
		m_ctTitle->SetContentAlignment(Label::a_center);
		m_tCount->SetContentAlignment(Label::a_center);
		m_ctCount->SetContentAlignment(Label::a_center);
		m_tTitle->SetTextInset(8, 0);
		m_ctTitle->SetTextInset(8, 0);
		m_tCount->SetTextInset(8, 0);
		m_ctCount->SetTextInset(8, 0);
	}

	void CreateRosters()
	{
		auto make = [&](const char *prefix, Label **rows, bool namesEast) {
			for (int i = 0; i < kRosterRows; ++i)
			{
				char name[32];
				snprintf(name, sizeof(name), "%s%d", prefix, i);
				rows[i] = new Label(this, name, "");
				MuteLabel(rows[i]);
				rows[i]->SetContentAlignment(namesEast ? Label::a_east : Label::a_west);
				rows[i]->SetTextInset(8, 0);
				rows[i]->SetVisible(false);
			}
		};
		make("TRoster", m_tPlayers, true);
		make("CTRoster", m_ctPlayers, false);
	}

	void StyleRosterLabel(Label *lab, Color color, bool title)
	{
		if (!lab)
			return;
		IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
		vgui2::HFont font = INVALID_FONT;
		if (sch)
		{
			if (title)
			{
				font = sch->GetFont("Title", IsProportional());
				if (font == INVALID_FONT)
					font = sch->GetFont("Default", IsProportional());
			}
			if (font == INVALID_FONT)
				font = sch->GetFont("Default", IsProportional());
		}
		if (font != INVALID_FONT)
			lab->SetFont(font);
		lab->SetFgColor(color);
		lab->SetPaintBackgroundEnabled(false);
	}

	void WriteTeamMeta(Label *lab, int humans, int bots, Color color)
	{
		if (!lab)
			return;
		char buf[48];
		if (bots > 0)
			snprintf(buf, sizeof(buf), "%d Players - %d bot%s", humans, bots, bots == 1 ? "" : "s");
		else
			snprintf(buf, sizeof(buf), "%d Players", humans);
		lab->SetText(buf);
		StyleRosterLabel(lab, color, false);
	}

	void RefreshRoster()
	{
		const ScoreboardHudState &s = InGameRoster_Get();
		m_rosterT = 0;
		m_rosterCT = 0;
		int tHumans = 0, tBots = 0, ctHumans = 0, ctBots = 0;
		for (int i = 0; i < kRosterRows; ++i)
		{
			if (m_tPlayers[i])
				m_tPlayers[i]->SetVisible(false);
			if (m_ctPlayers[i])
				m_ctPlayers[i]->SetVisible(false);
		}
		for (int i = 0; i < s.playerCount && i < CSRETRO_SCOREBOARD_PLAYERS; ++i)
		{
			const ScoreboardPlayerRow &row = s.players[i];
			Label **target = nullptr;
			int *count = nullptr;
			Color accent = InGameViewportLook::Text();
			if (row.team == 1)
			{
				if (row.bot)
					++tBots;
				else
					++tHumans;
				if (m_rosterT < kRosterRows)
				{
					target = m_tPlayers;
					count = &m_rosterT;
					accent = InGameViewportLook::Terror();
				}
			}
			else if (row.team == 2)
			{
				if (row.bot)
					++ctBots;
				else
					++ctHumans;
				if (m_rosterCT < kRosterRows)
				{
					target = m_ctPlayers;
					count = &m_rosterCT;
					accent = InGameViewportLook::CT();
				}
			}
			if (!target || !count || !target[*count])
				continue;
			target[*count]->SetText(row.name);
			StyleRosterLabel(target[*count], row.dead ? InGameViewportLook::TextDim() : accent, false);
			target[*count]->SetVisible(true);
			++*count;
		}
		WriteTeamMeta(m_tCount, tHumans, tBots, InGameViewportLook::TextDim());
		WriteTeamMeta(m_ctCount, ctHumans, ctBots, InGameViewportLook::TextDim());
		StyleRosterLabel(m_tTitle, InGameViewportLook::Terror(), true);
		StyleRosterLabel(m_ctTitle, InGameViewportLook::Text(), true);
	}

	void LayoutPreview(CTeamModelPreview *model, int originX, int originY, int sideW, int sideH)
	{
		if (!model)
			return;
		int sx = 0, sy = 0, sw = 0, sh = 0;
		InGameViewportLook::TeamModelViewport(sideW, sideH, sx, sy, sw, sh);
		model->SetBounds(originX + sx, originY + sy, sw, sh);
		model->SetVisible(true);
	}

	bool KeepTeamChild(const char *name) const
	{
		if (!name || !name[0])
			return false;
		static const char *kKeep[] = {
			"terbutton", "ctbutton", "autobutton", "specbutton", "vipbutton", "CancelButton",
			"TTitle", "CTTitle", "TMeta", "CTMeta", "TerrorModel", "CTModel",
		};
		for (const char *keep : kKeep)
		{
			if (!strcasecmp(name, keep))
				return true;
		}
		return !strncmp(name, "TRoster", 7) || !strncmp(name, "CTRoster", 8);
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
			else if (name && (!strcasecmp(name, "autobutton") || !strcasecmp(name, "specbutton") ||
				!strcasecmp(name, "vipbutton") || !strcasecmp(name, "CancelButton")))
				accent = InGameViewportLook::TextDim();
			const bool footer = name && (!strcasecmp(name, "autobutton") || !strcasecmp(name, "specbutton") ||
				!strcasecmp(name, "vipbutton") || !strcasecmp(name, "CancelButton"));
			if (auto *look = dynamic_cast<CTeamLookButton *>(btn))
			{
				look->SetAccent(accent);
				look->SetTeamChoice(name && !strcasecmp(name, "terbutton") ? 1 :
					(name && !strcasecmp(name, "ctbutton") ? 2 : 0));
				look->SetFooter(name && !strcasecmp(name, "specbutton") ? 1 :
					(name && !strcasecmp(name, "autobutton") ? 2 : (footer ? 3 : 0)));
			}
			else if (footer)
				InGameViewportLook::StyleFooterButton(btn, accent);
			else
				InGameViewportLook::StyleCardButton(btn, accent);
			btn->SetContentAlignment(footer ? Label::a_east : Label::a_center);
			btn->SetTextInset(0, 0);
			if (footer)
			{
				IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
				vgui2::HFont font = sch ? sch->GetFont("Title", IsProportional()) : INVALID_FONT;
				if (font != INVALID_FONT)
					btn->SetFont(font);
			}
		}
		if (auto *autoBtn = dynamic_cast<Button *>(FindChildByName("autobutton")))
			autoBtn->SetText("AUTO SELECT");
		if (auto *specBtn = dynamic_cast<Button *>(FindChildByName("specbutton")))
			specBtn->SetText("SPECTATE");
		if (Panel *title = FindChildByName("joinTeam"))
			title->SetVisible(false);
		if (Panel *map = FindChildByName("mapname"))
			map->SetVisible(false);
		if (auto *info = dynamic_cast<RichText *>(FindChildByName("MapInfo")))
		{
			info->SetVisible(false);
			info->SetPaintBackgroundEnabled(false);
		}
		StyleRosterLabel(m_tTitle, InGameViewportLook::Terror(), true);
		StyleRosterLabel(m_ctTitle, InGameViewportLook::Text(), true);
		StyleRosterLabel(m_tCount, InGameViewportLook::TextDim(), false);
		StyleRosterLabel(m_ctCount, InGameViewportLook::TextDim(), false);
	}

	void RelayoutVisibleButtons()
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (w < 200 || h < 160)
			return;
		for (int i = 0; i < GetChildCount(); ++i)
		{
			Panel *child = GetChild(i);
			if (!child)
				continue;
			if (!KeepTeamChild(child->GetName()))
				child->SetVisible(false);
		}
		int stageX = 0, stageY = 0, stageW = 0, stageH = 0;
		InGameViewportLook::TeamStage(w, h, stageX, stageY, stageW, stageH);
		const int col = stageW * 40 / 100;
		const int sideInset = stageW * 6 / 100;
		const int tX = stageX + sideInset;
		const int ctX = stageX + stageW - sideInset - col;
		if (Panel *t = FindChildByName("terbutton"))
			if (t->IsVisible())
				t->SetBounds(tX, stageY, col, stageH);
		if (Panel *ct = FindChildByName("ctbutton"))
			if (ct->IsVisible())
				ct->SetBounds(ctX, stageY, col, stageH);
		LayoutPreview(m_tModel, tX, stageY, col, stageH);
		LayoutPreview(m_ctModel, ctX, stageY, col, stageH);

		int tCx = 0, tCy = 0, tR = 0;
		int ctCx = 0, ctCy = 0, ctR = 0;
		InGameViewportLook::TeamEmblemMetrics(col, stageH, tCx, tCy, tR);
		InGameViewportLook::TeamEmblemMetrics(col, stageH, ctCx, ctCy, ctR);
		const int titleH = std::max(24, stageH * 7 / 100);
		const int metaH = std::max(16, stageH * 4 / 100);
		const int titleY = stageY + std::max(8, stageH * 14 / 100);
		if (m_tTitle)
			m_tTitle->SetBounds(tX, titleY, col, titleH);
		if (m_tCount)
			m_tCount->SetBounds(tX, titleY + titleH, col, metaH);
		if (m_ctTitle)
			m_ctTitle->SetBounds(ctX, titleY, col, titleH);
		if (m_ctCount)
			m_ctCount->SetBounds(ctX, titleY + titleH, col, metaH);

		const int listLeft = tX + tCx + tR + 12;
		const int listRight = ctX + ctCx - ctR - 12;
		const int listW = std::max(80, listRight - listLeft);
		const int listTop = stageY + std::max(titleY + titleH + metaH + 8, tCy - tR);
		const int listH = std::max(64, (tR * 2) - 8);
		const int colW = std::max(40, listW / 2 - 8);
		const int rowH = std::max(16, listH / kRosterRows);
		for (int i = 0; i < kRosterRows; ++i)
		{
			if (m_tPlayers[i])
				m_tPlayers[i]->SetBounds(listLeft, listTop + i * rowH, colW, rowH);
			if (m_ctPlayers[i])
				m_ctPlayers[i]->SetBounds(listLeft + listW - colW, listTop + i * rowH, colW, rowH);
		}

		const int footerH = std::max(28, stageH * 6 / 100);
		const int footerW = std::max(138, stageW * 14 / 100);
		int fx = stageX + stageW - 20;
		const int fy = stageY + stageH - footerH - std::max(12, stageH * 2 / 100);
		for (const char *name : {"autobutton", "specbutton", "vipbutton", "CancelButton"})
		{
			Panel *p = FindChildByName(name);
			if (!p || !p->IsVisible())
				continue;
			fx -= footerW;
			p->SetBounds(fx, fy, footerW, footerH);
			p->MoveToFront();
			fx -= 10;
		}
		if (m_tTitle)
			m_tTitle->MoveToFront();
		if (m_tCount)
			m_tCount->MoveToFront();
		if (m_ctTitle)
			m_ctTitle->MoveToFront();
		if (m_ctCount)
			m_ctCount->MoveToFront();
	}

	void Paint() override
	{
		BaseClass::Paint();
		int w = 0, h = 0;
		GetSize(w, h);
		if (!vgui2::surface() || w < 8)
			return;
		int sx = 0, sy = 0, sw = 0, sh = 0;
		InGameViewportLook::TeamStage(w, h, sx, sy, sw, sh);
		const int mid = sx + sw / 2;
		vgui2::surface()->DrawSetColor(255, 255, 255, 36);
		vgui2::surface()->DrawFilledRect(mid, sy + sh * 16 / 100, mid + 1, sy + sh * 84 / 100);
		const int footerTop = sy + sh - std::max(62, sh * 10 / 100);
		vgui2::surface()->DrawSetColor(0, 0, 0, 70);
		vgui2::surface()->DrawFilledRect(0, footerTop, w, h);
		vgui2::surface()->DrawSetColor(255, 255, 255, 28);
		vgui2::surface()->DrawFilledRect(0, footerTop, w, footerTop + 1);
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
		if (vgui2::HFont font = sch ? sch->GetFont("Default", IsProportional()) : INVALID_FONT)
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
		Menu_Con("CSRETRO_TEAM_VGUI open slots=%d t=%d ct=%d auto=%d vip=%d spec=%d cancel=%d tplayers=%d ctplayers=%d",
			m_slots, t, ct, autoas, vip, spec, cancel, m_rosterT, m_rosterCT);
		int stageX = 0, stageY = 0, stageW = 0, stageH = 0;
		InGameViewportLook::TeamStage(GetWide(), GetTall(), stageX, stageY, stageW, stageH);
		Menu_Con("CSRETRO_TEAM_CANVAS view=%dx%d canvas=%d,%d %dx%d capped=%d",
			GetWide(), GetTall(), stageX, stageY, stageW, stageH,
			(stageW < GetWide() * 9 / 10 || stageH < GetTall() * 9 / 10) ? 1 : 0);
		Menu_Con("CSRETRO_TEAM_LOOK split=1 roster=1 emblem=1 footer=1 stage=1 models=1");
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
		InGameViewportLook::PaintTeamBackdrop(w, h, PauseBackdrop_IsBlurred());
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
	BuySelect_RememberClass(nullptr);
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
	PauseBackdrop_Invalidate();
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
	PauseBackdrop_Invalidate();
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
	static int captureWait = 0;
	if (step >= 99)
		return;

	if (step == 0)
	{
		if (!TeamSelect_IsActive())
			return;
		++hold;
		if (hold >= 120)
		{
			// Gate-only fixture: proves the centre roster composition without
			// inventing names in normal play. Runtime rows still come exclusively
			// from the client's scoreboard/spectator state.
			ScoreboardHudState fixture{};
			const char *names[] = {"Alpha", "Bravo", "BOT Cedar", "Delta", "Echo", "BOT Fox"};
			fixture.playerCount = 6;
			for (int i = 0; i < fixture.playerCount; ++i)
			{
				snprintf(fixture.players[i].name, sizeof(fixture.players[i].name), "%s", names[i]);
				fixture.players[i].team = i < 3 ? 1 : 2;
				fixture.players[i].bot = (i == 2 || i == 5) ? 1 : 0;
			}
			InGameRoster_StoreScoreboard(fixture);
			g_panel->LayoutFamily();
		}
		if (captureWait > 0)
		{
			if (++captureWait < 90)
				return;
			MenuEngine::ClientCmd("screenshot\n");
			captureWait = 0;
			++step;
			hold = 0;
			return;
		}
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
		Menu_Con("CSRETRO_TEAM_GATE_ROSTER t=%d ct=%d", g_panel->TerrorRosterCount(),
			g_panel->CTRosterCount());
		ScoreboardHudState moved = InGameRoster_Get();
		if (moved.playerCount >= 2)
		{
			moved.players[1].team = 2;
			snprintf(moved.players[1].name, sizeof(moved.players[1].name), "%s", "BravoMoved");
			InGameRoster_StoreScoreboard(moved);
			g_panel->LayoutFamily();
			Menu_Con("CSRETRO_TEAM_GATE_ROSTER_UPDATE t=%d ct=%d moved=%d",
				g_panel->TerrorRosterCount(), g_panel->CTRosterCount(),
				g_panel->RosterHasName("BravoMoved") ? 1 : 0);
			moved.players[1].team = 1;
			snprintf(moved.players[1].name, sizeof(moved.players[1].name), "%s", "Bravo");
			InGameRoster_StoreScoreboard(moved);
			g_panel->LayoutFamily();
		}
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
		MenuEngine::ClientCmdNow("clear\n");
		captureWait = 1;
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
