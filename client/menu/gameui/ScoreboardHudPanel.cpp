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

using namespace vgui2;

void UI_KeyEvent(int key, int down);

namespace
{
enum { kRows = 16 };

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
		m_tCols = new Label(m_card, "TCols", "K    D    PING");
		m_ctCols = new Label(m_card, "CTCols", "K    D    PING");
		m_spec = new Label(m_card, "Spec", "");

		for (int i = 0; i < kRows; ++i)
		{
			char name[16];
			std::snprintf(name, sizeof(name), "TRow%d", i);
			m_tRow[i] = new Label(m_card, name, "");
			std::snprintf(name, sizeof(name), "CTRow%d", i);
			m_ctRow[i] = new Label(m_card, name, "");
		}

		Label *all[] = {m_server, m_tHead, m_ctHead, m_tCols, m_ctCols, m_spec};
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
		m_tCols->SetContentAlignment(Label::a_east);
		m_ctCols->SetContentAlignment(Label::a_east);
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
		const int cardW = w * 56 / 100;
		const int pad = cardW / 36;
		const int headH = 28;
		const int colH = 20;
		const int rowH = 20;
		const int specH = 22;
		const int cardH = pad + headH + colH + showRows * rowH + specH + pad;
		const int cx = (w - cardW) / 2;
		const int cy = (h - cardH) / 2;
		m_card->SetBounds(cx, cy, cardW, cardH);

		const int mid = cardW / 2;
		m_server->SetBounds(pad, pad / 2, cardW - pad * 2, headH);
		m_tHead->SetBounds(pad, pad / 2 + headH, mid - pad * 2, colH);
		m_ctHead->SetBounds(mid + pad, pad / 2 + headH, mid - pad * 2, colH);
		m_tCols->SetBounds(pad, pad / 2 + headH, mid - pad * 2, colH);
		m_ctCols->SetBounds(mid + pad, pad / 2 + headH, mid - pad * 2, colH);

		const int rowTop = pad / 2 + headH + colH + 4;
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
		m_spec->SetBounds(pad, cardH - specH - pad / 2, cardW - pad * 2, specH);
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
				FillRow(m_tRow[tN], p, HudFrameLook::Terror());
				++tN;
			}
			else if (p.team == 2 && ctN < kRows)
			{
				FillRow(m_ctRow[ctN], p, HudFrameLook::CT());
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
			m_tRow[i]->SetText("");
		for (int i = ctN; i < kRows; ++i)
			m_ctRow[i]->SetText("");
		m_spec->SetText(specN ? specLine : "");
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
	static void FillRow(Label *lab, const ScoreboardPlayerRow &p, Color teamCol)
	{
		char ping[16];
		if (p.bot)
			std::snprintf(ping, sizeof(ping), "BOT");
		else
			std::snprintf(ping, sizeof(ping), "%d", p.ping);
		char line[96];
		std::snprintf(line, sizeof(line), "%-16.16s%s  %3d  %3d  %4s",
			p.name, p.dead ? "*" : " ", p.frags, p.deaths, ping);
		lab->SetText(line);
		if (p.dead)
			HudFrameLook::StyleHudLabel(lab, HudFrameLook::Dead());
		else if (p.thisPlayer)
			HudFrameLook::StyleHudLabel(lab, HudFrameLook::Text());
		else
			HudFrameLook::StyleHudLabel(lab, teamCol);
	}

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
		set(m_tCols, body, HudFrameLook::TextDim());
		set(m_ctCols, body, HudFrameLook::TextDim());
		set(m_spec, body, HudFrameLook::TextDim());
		if (body != INVALID_FONT)
		{
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
	Label *m_tCols = nullptr;
	Label *m_ctCols = nullptr;
	Label *m_spec = nullptr;
	Label *m_tRow[kRows]{};
	Label *m_ctRow[kRows]{};
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
