#include "ScoreboardHudPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "HudFrameLook.h"
#include "RadioSelectPanel.h"
#include "TeamSelectPanel.h"

#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>

#include "../src/menu_priv.h"
#include "cl_dll/IGameMenuExports.h"
#include "keydefs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace vgui2;

void UI_KeyEvent(int key, int down);

namespace
{
enum { kRows = 16 };

// Text is laid out in actual cells, not padded with spaces in a proportional font.
class CScoreRow : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CScoreRow, Panel);
public:
	CScoreRow(Panel *parent, const char *name, bool header = false) : BaseClass(parent, name)
	{
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetPaintBackgroundEnabled(false);
		const char *titles[] = {"#GameUI_PlayerName", "K", "D", "PING"};
		for (int i = 0; i < 4; ++i)
		{
			m_cells[i] = new Label(this, titles[i], header ? titles[i] : "");
			m_cells[i]->SetMouseInputEnabled(false);
			m_cells[i]->SetContentAlignment(i ? Label::a_east : Label::a_west);
			m_cells[i]->SetTextInset(4, 0);
		}
	}
	void PerformLayout() override
	{
		const int w = GetWide(), h = GetTall();
		const int numeric = 32, ping = 48;
		const int name = std::max(0, w - 2 * numeric - ping);
		m_cells[0]->SetBounds(0, 0, name, h);
		m_cells[1]->SetBounds(name, 0, numeric, h);
		m_cells[2]->SetBounds(name + numeric, 0, numeric, h);
		m_cells[3]->SetBounds(name + 2 * numeric, 0, ping, h);
	}
	void SetFont(vgui2::HFont font)
	{
		for (auto *cell : m_cells) cell->SetFont(font);
	}
	void SetColor(Color color)
	{
		for (auto *cell : m_cells) HudFrameLook::StyleHudLabel(cell, color);
	}
	void Clear()
	{
		for (auto *cell : m_cells) cell->SetText("");
		SetPaintBackgroundEnabled(false);
	}
	void SetPlayer(const ScoreboardPlayerRow &player, Color team)
	{
		m_cells[0]->SetText(player.name);
		char text[24];
		std::snprintf(text, sizeof(text), "%d", player.frags);
		m_cells[1]->SetText(text);
		std::snprintf(text, sizeof(text), "%d", player.deaths);
		m_cells[2]->SetText(text);
		std::snprintf(text, sizeof(text), "%d", player.ping);
		m_cells[3]->SetText(player.bot ? "BOT" : text);
		SetColor(player.dead ? HudFrameLook::Dead() : player.thisPlayer ? HudFrameLook::Text() : team);
		SetBgColor(Color(team.r(), team.g(), team.b(), 36));
		SetPaintBackgroundEnabled(player.thisPlayer != 0);
	}
private:
	Label *m_cells[4]{};
};

class CCard : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CCard, Panel);

public:
	explicit CCard(Panel *parent) : BaseClass(parent, "ScoreCard")
	{
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		SetBgColor(HudFrameLook::Card());
		SetBorder(nullptr);
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (!vgui2::surface() || w < 1 || h < 1)
			return;
		vgui2::surface()->DrawSetColor(HudFrameLook::Card());
		vgui2::surface()->DrawFilledRect(0, 0, w, h);
		vgui2::surface()->DrawSetColor(HudFrameLook::Terror());
		vgui2::surface()->DrawFilledRect(0, 0, w / 2, 3);
		vgui2::surface()->DrawSetColor(HudFrameLook::CT());
		vgui2::surface()->DrawFilledRect(w / 2, 0, w, 3);
		vgui2::surface()->DrawSetColor(40, 40, 44, 220);
		vgui2::surface()->DrawFilledRect(w / 2 - 1, 8, w / 2 + 1, h - 8);
	}
};

class CScoreboardHudPanel : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CScoreboardHudPanel, Panel);

public:
	explicit CScoreboardHudPanel(Panel *parent) : BaseClass(parent, "ScoreboardHud")
	{
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetVisible(false);

		m_card = new CCard(this);
		m_server = new Label(m_card, "Server", "");
		m_tHead = new Label(m_card, "THead", "Terrorists");
		m_ctHead = new Label(m_card, "CTHead", "Counter-Terrorists");
		m_tCols = new CScoreRow(m_card, "TCols", true);
		m_ctCols = new CScoreRow(m_card, "CTCols", true);
		m_tEmpty = new Label(m_card, "TEmpty", "—");
		m_ctEmpty = new Label(m_card, "CTEmpty", "—");
		m_spec = new Label(m_card, "Spec", "");

		for (int i = 0; i < kRows; ++i)
		{
			char name[16];
			std::snprintf(name, sizeof(name), "TRow%d", i);
			m_tRow[i] = new CScoreRow(m_card, name);
			std::snprintf(name, sizeof(name), "CTRow%d", i);
			m_ctRow[i] = new CScoreRow(m_card, name);
		}

		Label *all[] = {m_server, m_tHead, m_ctHead, m_tEmpty, m_ctEmpty, m_spec};
		for (Label *lab : all)
		{
			lab->SetPaintBackgroundEnabled(false);
			lab->SetMouseInputEnabled(false);
		}
		for (int i = 0; i < kRows; ++i)
		{
			m_tRow[i]->SetPaintBackgroundEnabled(false);
			m_tRow[i]->SetMouseInputEnabled(false);
			m_ctRow[i]->SetPaintBackgroundEnabled(false);
			m_ctRow[i]->SetMouseInputEnabled(false);
		}
		m_server->SetContentAlignment(Label::a_center);
		m_tHead->SetContentAlignment(Label::a_west);
		m_ctHead->SetContentAlignment(Label::a_west);
		m_tEmpty->SetContentAlignment(Label::a_center);
		m_ctEmpty->SetContentAlignment(Label::a_center);
		m_spec->SetContentAlignment(Label::a_west);
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		SetBgColor(Color(0, 0, 0, 0));
		SetBorder(nullptr);
		StyleLabels(scheme);
	}

	void PerformLayout() override
	{
		int w = 0, h = 0;
		HudFrameLook::ViewportSize(w, h, GetParent());
		SetBounds(0, 0, w, h);
		int showRows = m_tCount > m_ctCount ? m_tCount : m_ctCount;
		if (showRows < 4)
			showRows = 4;
		if (showRows > kRows)
			showRows = kRows;
		const int cardW = std::min(w - 32, std::clamp(w * 76 / 100, 608, 1000));
		const int pad = 16;
		const int headH = 28;
		const int colH = 20;
		const int rowH = 20;
		const int teamH = 28;
		const int specH = m_hasSpectators ? 24 : 0;
		const int cardH = 2 * pad + headH + teamH + colH + showRows * rowH + specH;
		const int cx = (w - cardW) / 2;
		const int cy = (h - cardH) / 2;
		m_card->SetBounds(cx, cy, cardW, cardH);

		const int mid = cardW / 2;
		m_server->SetBounds(pad, pad, cardW - pad * 2, headH);
		m_tHead->SetBounds(pad, pad + headH, mid - pad * 2, teamH);
		m_ctHead->SetBounds(mid + pad, pad + headH, mid - pad * 2, teamH);
		m_tCols->SetBounds(pad, pad + headH + teamH, mid - pad * 2, colH);
		m_ctCols->SetBounds(mid + pad, pad + headH + teamH, mid - pad * 2, colH);

		const int rowTop = pad + headH + teamH + colH;
		m_tEmpty->SetBounds(pad, rowTop, mid - 2 * pad, showRows * rowH);
		m_ctEmpty->SetBounds(mid + pad, rowTop, mid - 2 * pad, showRows * rowH);
		m_tEmpty->SetVisible(m_tCount == 0);
		m_ctEmpty->SetVisible(m_ctCount == 0);
		for (int i = 0; i < kRows; ++i)
		{
			if (i < showRows)
			{
				const int y = rowTop + i * rowH;
				m_tRow[i]->SetBounds(pad, y, mid - pad * 2, rowH);
				m_ctRow[i]->SetBounds(mid + pad, y, mid - pad * 2, rowH);
				m_tRow[i]->SetVisible(true);
				m_ctRow[i]->SetVisible(true);
			}
			else
			{
				m_tRow[i]->SetVisible(false);
				m_ctRow[i]->SetVisible(false);
			}
		}
		m_spec->SetBounds(pad, cardH - specH - pad, cardW - pad * 2, specH);
		StyleLabels(GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr);
	}

	void Apply(const ScoreboardHudState &s)
	{
		m_server->SetText(s.server[0] ? s.server : "Scoreboard");
		char buf[64];
		std::snprintf(buf, sizeof(buf), "Terrorists   %d", s.tScore);
		m_tHead->SetText(buf);
		std::snprintf(buf, sizeof(buf), "Counter-Terrorists   %d", s.ctScore);
		m_ctHead->SetText(buf);

		int tN = 0, ctN = 0;
		char specLine[256] = "Spectators";
		int specN = 0;
		for (int i = 0; i < s.playerCount && i < CSRETRO_SCOREBOARD_PLAYERS; ++i)
		{
			const ScoreboardPlayerRow &p = s.players[i];
			if (p.team == 1 && tN < kRows)
			{
				m_tRow[tN]->SetPlayer(p, HudFrameLook::Terror());
				++tN;
			}
			else if (p.team == 2 && ctN < kRows)
			{
				m_ctRow[ctN]->SetPlayer(p, HudFrameLook::CT());
				++ctN;
			}
			else if (p.team == 3 && specN < 6)
			{
				char add[48];
				std::snprintf(add, sizeof(add), "%s%s", specN ? ", " : ": ", p.name);
				std::strncat(specLine, add, sizeof(specLine) - std::strlen(specLine) - 1);
				++specN;
			}
		}
		for (int i = tN; i < kRows; ++i)
			m_tRow[i]->Clear();
		for (int i = ctN; i < kRows; ++i)
			m_ctRow[i]->Clear();
		m_spec->SetText(specN ? specLine : "");
		m_hasSpectators = specN != 0;
		m_tCount = tN;
		m_ctCount = ctN;
		m_state = s;
		InvalidateLayout();
	}

	const ScoreboardHudState &State() const { return m_state; }
	int TCount() const { return m_tCount; }
	int CTCount() const { return m_ctCount; }

	bool TitleLooksLocalized() const
	{
		char text[64] = {0};
		m_tHead->GetText(text, sizeof(text));
		return text[0] && text[0] != '#';
	}

private:
	void StyleLabels(IScheme *scheme)
	{
		vgui2::HFont body = INVALID_FONT;
		vgui2::HFont title = INVALID_FONT;
		if (scheme)
		{
			body = scheme->GetFont("Default", IsProportional());
			title = scheme->GetFont("DefaultLarge", IsProportional());
			if (title == INVALID_FONT)
				title = body;
		}
		auto set = [&](Label *lab, vgui2::HFont font, Color col) {
			if (font != INVALID_FONT)
				lab->SetFont(font);
			HudFrameLook::StyleHudLabel(lab, col);
		};
		set(m_server, title != INVALID_FONT ? title : body, HudFrameLook::Text());
		set(m_tHead, title != INVALID_FONT ? title : body, HudFrameLook::Terror());
		set(m_ctHead, title != INVALID_FONT ? title : body, HudFrameLook::CT());
		set(m_tEmpty, body, HudFrameLook::TextDim());
		set(m_ctEmpty, body, HudFrameLook::TextDim());
		m_tCols->SetColor(HudFrameLook::TextDim());
		m_ctCols->SetColor(HudFrameLook::TextDim());
		set(m_spec, body, HudFrameLook::TextDim());
		if (body != INVALID_FONT)
		{
			m_tCols->SetFont(body);
			m_ctCols->SetFont(body);
			for (int i = 0; i < kRows; ++i)
			{
				m_tRow[i]->SetFont(body);
				m_ctRow[i]->SetFont(body);
			}
		}
	}

	CCard *m_card = nullptr;
	Label *m_server = nullptr;
	Label *m_tHead = nullptr;
	Label *m_ctHead = nullptr;
	CScoreRow *m_tCols = nullptr;
	CScoreRow *m_ctCols = nullptr;
	Label *m_tEmpty = nullptr;
	Label *m_ctEmpty = nullptr;
	Label *m_spec = nullptr;
	CScoreRow *m_tRow[kRows]{};
	CScoreRow *m_ctRow[kRows]{};
	bool m_hasSpectators = false;
	ScoreboardHudState m_state{};
	int m_tCount = 0;
	int m_ctCount = 0;
};

CScoreboardHudPanel *g_panel = nullptr;
Panel *g_host = nullptr;

bool OverlayBlocks()
{
	return gMenuVisible || TeamSelect_IsActive() || ClassSelect_IsActive() ||
		BuySelect_IsActive() || RadioSelect_IsActive();
}
} // namespace

void ScoreboardHud_Set(const ScoreboardHudState *state)
{
	if (!state || !state->visible)
	{
		ScoreboardHud_Hide();
		return;
	}
	if (OverlayBlocks())
	{
		if (g_panel)
			g_panel->SetVisible(false);
		return;
	}
	if (!g_panel)
	{
		Panel *root = g_host;
		if (!root)
			return;
		g_panel = new CScoreboardHudPanel(root);
		g_panel->SetVisible(false);
	}
	if (!g_panel->GetParent() && g_host)
		g_panel->SetParent(g_host);
	int w = 0, h = 0;
	HudFrameLook::ViewportSize(w, h, g_panel->GetParent());
	g_panel->SetBounds(0, 0, w, h);
	const bool was = g_panel->IsVisible();
	g_panel->Apply(*state);
	g_panel->SetVisible(true);
	g_panel->SetZPos(1);
	if (!was)
	{
		Menu_Con("CSRETRO_SCORE_VGUI open players=%d t=%d ct=%d",
			state->playerCount, state->tScore, state->ctScore);
		Menu_Con("CSRETRO_SCORE_LOOK table=1");
	}
}

void ScoreboardHud_Hide()
{
	if (g_panel && g_panel->IsVisible())
		Menu_Con("CSRETRO_SCORE_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
}

bool ScoreboardHud_IsActive()
{
	return g_panel && g_panel->IsVisible();
}

void ScoreboardHud_Shutdown()
{
	g_panel = nullptr;
	g_host = nullptr;
}

void ScoreboardHud_BindHost(Panel *root)
{
	g_host = root;
	if (g_panel && root)
		g_panel->SetParent(root);
}

void ScoreboardHud_GateTick()
{
	if (!getenv("CSRETRO_SCORE_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	if (step >= 99)
		return;

	auto failDone = [](const char *why) {
		Menu_Con("CSRETRO_SCORE_GATE_FAIL %s", why);
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_SCORE_GATE_DONE");
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
		{
			++hold;
			if (hold > 240)
				failDone("class fehlt");
			return;
		}
		++hold;
		if (hold < 20)
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
		{
			++hold;
			if (hold > 240)
				failDone("spawn fehlt");
			return;
		}
		++hold;
		if (hold < 40)
			return;
		MenuEngine::ClientCmdNow("+showscores\n");
		++step;
		hold = 0;
		return;
	}

	if (step == 3)
	{
		++hold;
		if (hold == 90)
			MenuEngine::ClientCmdNow("+showscores\n");
		if (ScoreboardHud_IsActive() && g_panel)
		{
			if (hold < 30)
				return;
			const ScoreboardHudState &s = g_panel->State();
			Menu_Con("CSRETRO_SCORE_GATE_OPEN visible=1 players=%d trows=%d ctrows=%d t=%d ct=%d raw=%d",
				s.playerCount, g_panel->TCount(), g_panel->CTCount(),
				s.tScore, s.ctScore, g_panel->TitleLooksLocalized() ? 0 : 1);
			MenuEngine::ClientCmd("screenshot\n");
			Menu_Con("CSRETRO_SCORE_GATE_SHOT_QUEUED");
			step = 4;
			hold = 0;
			return;
		}
		if (hold > 480)
			failDone("scoreboard fehlt");
	}

	if (step == 4)
	{
		// Let the engine execute and flush the queued screenshot before quit.
		// Sending both commands in one frame produced a green gate without an
		// image, making visual regressions and gamma comparisons unverifiable.
		if (++hold < 20)
			return;
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_SCORE_GATE_DONE");
		step = 99;
	}
}
