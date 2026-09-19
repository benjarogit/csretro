#include "TeamSelectPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "InGameRoster.h"
#include "InGameUi.h"
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

class CTeamRosterBadge : public Label
{
public:
	CTeamRosterBadge(Panel *parent, const char *name) : Label(parent, name, "") {}
	void PaintBackground() override
	{
		surface()->DrawSetColor(15, 19, 23, 220);
		surface()->DrawFilledRect(0, 0, GetWide(), GetTall());
		surface()->DrawSetColor(150, 155, 150, 90);
		surface()->DrawOutlinedRect(0, 0, GetWide(), GetTall());
	}
};

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
			return;
		if (!m_teamChoice)
		{
			InGameUi::PaintCardBackground(w, h, m_accent, active);
			return;
		}
		int cx = 0, cy = 0, radius = 0;
		InGameUi::TeamEmblemMetrics(w, h, cx, cy, radius);
		InGameUi::PaintEmblem(m_teamChoice, cx, cy, radius, m_accent, active, std::max(4, radius / 18));
	}

	void Paint() override
	{
		if (m_teamChoice)
			return;
		BaseClass::Paint();
	}

private:
	Color m_accent = InGameUi::Text();
	int m_teamChoice = 0;
	int m_footer = 0;

	void ApplyLook()
	{
		if (m_footer)
		{
			InGameUi::StyleFooterButton(this, m_accent);
			SetPaintBackgroundEnabled(true);
			SetFgColor((IsArmed() || IsDepressed()) ? InGameUi::Text() : m_accent);
			return;
		}
		InGameUi::StyleCardButton(this, m_accent);
		SetFgColor((IsArmed() || IsDepressed()) ? InGameUi::Text() : m_accent);
		SetBgColor((IsArmed() || IsDepressed()) ? InGameUi::CardArmed() : InGameUi::Card());
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
		RelayoutVisibleButtons();
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
	Label *m_tAvatars[kRosterRows]{};
	Label *m_ctAvatars[kRosterRows]{};
	int m_tPreviewIndex = -1;
	int m_ctPreviewIndex = -1;
	vgui2::HFont m_titleFont = INVALID_FONT;
	vgui2::HFont m_rowFont = INVALID_FONT;
	int m_fontHeight = 0;

	void CreateTeamPreviews()
	{
		m_tModel = new CTeamModelPreview(this, "TerrorModel");
		m_ctModel = new CTeamModelPreview(this, "CTModel");
		m_tModel->SetIndependentPlayerState(true);
		m_ctModel->SetIndependentPlayerState(true);
	}

	void RandomizeTeamPreviews()
	{
		static const char *const terrorModels[] = {
			"models/player/terror/terror.mdl",
			"models/player/leet/leet.mdl",
			"models/player/arctic/arctic.mdl",
			"models/player/guerilla/guerilla.mdl",
		};
		// The single centered preview needs its own camera-facing calibration;
		// class-lineup yaws also compensate for each model's lateral offset.
		static const float terrorYaw = 150.0f;
		static const char *const ctModels[] = {
			"models/player/urban/urban.mdl",
			"models/player/gsg9/gsg9.mdl",
			"models/player/sas/sas.mdl",
			"models/player/gign/gign.mdl",
		};
		static const float ctYaw = 202.0f;
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
		// Once joined, preserve the owner's class; only the other team's
		// representative is random. Opening this menu must not erase joinclass.
		static const char *const tStems[] = {"terror", "leet", "arctic", "guerilla"};
		static const char *const ctStems[] = {"urban", "gsg9", "sas", "gign"};
		int localTeam = 0;
		const ScoreboardHudState &roster = InGameRoster_Get();
		for (int i = 0; i < roster.playerCount && i < CSRETRO_SCOREBOARD_PLAYERS; ++i)
			if (roster.players[i].thisPlayer)
				localTeam = roster.players[i].team;
		const char *ownT = localTeam == 1 ? BuySelect_PlayerClass(false) : nullptr;
		const char *ownCT = localTeam == 2 ? BuySelect_PlayerClass(true) : nullptr;
		for (int i = 0; i < 4; ++i)
		{
			if (ownT && !strcmp(ownT, tStems[i]))
				m_tPreviewIndex = i;
			if (ownCT && !strcmp(ownCT, ctStems[i]))
				m_ctPreviewIndex = i;
		}
		m_tModel->SetPreview(terrorModels[m_tPreviewIndex], "models/p_ak47.mdl",
			terrorYaw, 80);
		m_ctModel->SetPreview(ctModels[m_ctPreviewIndex], "models/p_m4a1.mdl",
			ctYaw, 33);
		m_tModel->SetWorldWidth(44.0f);
		m_tModel->SetWorldHeight(70.0f);
		m_tModel->SetCameraHeight(-2.0f);
		m_ctModel->SetWorldWidth(44.0f);
		m_ctModel->SetWorldHeight(70.0f);
		m_ctModel->SetCameraHeight(-2.0f);
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
				rows[i] = strstr(prefix, "Avatar") ? new CTeamRosterBadge(this, name) : new Label(this, name, "");
				MuteLabel(rows[i]);
				rows[i]->SetContentAlignment(namesEast ? Label::a_east : Label::a_west);
				rows[i]->SetTextInset(8, 0);
				rows[i]->SetVisible(false);
			}
		};
		make("TRoster", m_tPlayers, true);
		make("CTRoster", m_ctPlayers, false);
		make("TRosterAvatar", m_tAvatars, false);
		make("CTRosterAvatar", m_ctAvatars, false);
		for (int i = 0; i < kRosterRows; ++i)
			for (Label *avatar : {m_tAvatars[i], m_ctAvatars[i]})
			{
				avatar->SetContentAlignment(Label::a_center);
				avatar->SetTextInset(0, 0);
				avatar->SetPaintBackgroundEnabled(true);
				avatar->SetBgColor(Color(15, 19, 23, 210));
			}
	}

	void StyleRosterLabel(Label *lab, Color color, bool title)
	{
		if (!lab)
			return;
		const int height = std::max(480, GetTall());
		if (m_fontHeight != height)
		{
			if (m_titleFont == INVALID_FONT) m_titleFont = surface()->CreateFont();
			if (m_rowFont == INVALID_FONT) m_rowFont = surface()->CreateFont();
			surface()->AddGlyphSetToFont(m_titleFont, "Noto Sans", std::max(16, height * 28 / 1000), 800, 0, 0, 0x010, 0, 0xFFFF);
			surface()->AddGlyphSetToFont(m_rowFont, "Noto Sans", std::max(12, height * 20 / 1000), 500, 0, 0, 0x010, 0, 0xFFFF);
			m_fontHeight = height;
		}
		lab->SetFont(title ? m_titleFont : m_rowFont);
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
			m_tAvatars[i]->SetVisible(false);
			m_ctAvatars[i]->SetVisible(false);
		}
		for (int i = 0; i < s.playerCount && i < CSRETRO_SCOREBOARD_PLAYERS; ++i)
		{
			const ScoreboardPlayerRow &row = s.players[i];
			Label **target = nullptr;
			int *count = nullptr;
			Color accent = InGameUi::Text();
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
					accent = InGameUi::Terror();
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
					accent = InGameUi::CT();
				}
			}
			if (!target || !count || !target[*count])
				continue;
			target[*count]->SetText(row.name);
			StyleRosterLabel(target[*count], row.dead ? InGameUi::TextDim() : accent, false);
			target[*count]->SetVisible(true);
			// The roster protocol has no avatar URL/Steam ID. Use honest local
			// placeholders, never fabricated player portraits or network requests.
			Label *avatar = row.team == 1 ? m_tAvatars[*count] : m_ctAvatars[*count];
			avatar->SetText(row.bot ? "BOT" : "?");
			StyleRosterLabel(avatar, accent, false);
			avatar->SetPaintBackgroundEnabled(true);
			avatar->SetVisible(true);
			++*count;
		}
		WriteTeamMeta(m_tCount, tHumans, tBots, InGameUi::TextDim());
		WriteTeamMeta(m_ctCount, ctHumans, ctBots, InGameUi::TextDim());
		StyleRosterLabel(m_tTitle, InGameUi::Terror(), true);
		StyleRosterLabel(m_ctTitle, InGameUi::Text(), true);
	}

	void LayoutPreview(CTeamModelPreview *model, int originX, int originY, int sideW, int sideH)
	{
		if (!model)
			return;
		int sx = 0, sy = 0, sw = 0, sh = 0;
		InGameUi::TeamModelViewport(sideW, sideH, sx, sy, sw, sh);
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
			Color accent = InGameUi::Text();
			if (name && !strcasecmp(name, "terbutton"))
				accent = InGameUi::Terror();
			else if (name && !strcasecmp(name, "ctbutton"))
				accent = InGameUi::CT();
			else if (name && (!strcasecmp(name, "autobutton") || !strcasecmp(name, "specbutton") ||
				!strcasecmp(name, "vipbutton") || !strcasecmp(name, "CancelButton")))
				accent = InGameUi::TextDim();
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
				InGameUi::StyleFooterButton(btn, accent);
			else
				InGameUi::StyleCardButton(btn, accent);
			btn->SetContentAlignment(footer ? Label::a_east : Label::a_center);
			btn->SetTextInset(0, 0);
			if (footer)
			{
				IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
				vgui2::HFont font = m_rowFont != INVALID_FONT ? m_rowFont : (sch ? sch->GetFont("Default", IsProportional()) : INVALID_FONT);
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
		StyleRosterLabel(m_tTitle, InGameUi::Terror(), true);
		StyleRosterLabel(m_ctTitle, InGameUi::Text(), true);
		StyleRosterLabel(m_tCount, InGameUi::TextDim(), false);
		StyleRosterLabel(m_ctCount, InGameUi::TextDim(), false);
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
		InGameUi::TeamStage(w, h, stageX, stageY, stageW, stageH);
		const int col = stageW * 30 / 100;
		const int sideInset = stageW * 12 / 100;
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
		InGameUi::TeamEmblemMetrics(col, stageH, tCx, tCy, tR);
		InGameUi::TeamEmblemMetrics(col, stageH, ctCx, ctCy, ctR);
		const int titleH = std::max(22, stageH * 4 / 100);
		const int metaH = std::max(16, stageH * 25 / 1000);
		const int titleGap = std::max(6, stageH * 1 / 100);
		int titleY = stageY + stageH * 20 / 100;
		if (titleY < stageY + 8)
			titleY = stageY + 8;
		if (m_tTitle)
			m_tTitle->SetBounds(tX, titleY, col, titleH);
		if (m_tCount)
			m_tCount->SetBounds(tX, titleY + titleH, col, metaH);
		if (m_ctTitle)
			m_ctTitle->SetBounds(ctX, titleY, col, titleH);
		if (m_ctCount)
			m_ctCount->SetBounds(ctX, titleY + titleH, col, metaH);

		const int listLeft = stageX + stageW * 40 / 100;
		const int listRight = stageX + stageW * 60 / 100;
		const int listW = std::max(80, listRight - listLeft);
		const int listTop = titleY + titleH + metaH + titleGap;
		const int listH = stageH * 57 / 100;
		const int colW = std::max(40, listW / 2 - 12);
		const int rows = std::max(7, std::max(m_rosterT, m_rosterCT));
		const int rowH = std::max(16, std::min(stageH * 7 / 100, listH / rows));
		const int avatarSize = std::max(12, rowH - 8);
		for (int i = 0; i < kRosterRows; ++i)
		{
			if (m_tPlayers[i])
				m_tPlayers[i]->SetBounds(listLeft, listTop + i * rowH, colW - avatarSize - 4, rowH);
			if (m_ctPlayers[i])
				m_ctPlayers[i]->SetBounds(listRight - colW + avatarSize + 4, listTop + i * rowH, colW - avatarSize - 4, rowH);
			m_tAvatars[i]->SetBounds(listLeft + colW - avatarSize, listTop + i * rowH + 4, avatarSize, avatarSize);
			m_ctAvatars[i]->SetBounds(listRight - colW, listTop + i * rowH + 4, avatarSize, avatarSize);
		}

		const int footerH = std::max(22, stageH * 4 / 100);
		const int footerW = std::max(108, stageW * 11 / 100);
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
		const int w = GetWide(), h = GetTall();
		surface()->DrawSetColor(12, 14, 16, 125);
		surface()->DrawFilledRect(0, h * 91 / 100, w, h);
		surface()->DrawSetColor(220, 225, 225, 28);
		surface()->DrawFilledRect(0, h * 91 / 100, w, h * 91 / 100 + 1);
	}

	void LoadMapBriefing()
	{
		auto *info = dynamic_cast<RichText *>(FindChildByName("MapInfo"));
		if (!info)
			return;
		info->SetPanelInteractive(false);
		info->SetUnusedScrollbarInvisible(true);
		IScheme *sch = GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr;
		info->SetFgColor(InGameUi::TextDim());
		info->SetBgColor(InGameUi::Card());
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
		InGameUi::TeamStage(GetWide(), GetTall(), stageX, stageY, stageW, stageH);
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
		InGameUi::PaintSelectVeil(w, h);
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
	return true;
}

void TeamSelect_Hide(bool restoreKeyDest)
{
	if (g_overlay && g_overlay->IsVisible())
		Menu_Con("CSRETRO_TEAM_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
	if (g_overlay)
	{
		g_overlay->SetMouseInputEnabled(false);
		g_overlay->SetVisible(false);
	}
	PauseBackdrop_Invalidate();
	if (restoreKeyDest && g_keyDestPushed && !gMenuVisible && !ClassSelect_IsActive() && !BuySelect_IsActive() &&
		!RadioSelect_IsActive())
	{
		MenuEngine::RestoreGameKeyDest();
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
