#include "SpectatorHudPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "HudFrameLook.h"
#include "InGameRoster.h"
#include "TeamSelectPanel.h"

#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Panel.h>

#include "../src/menu_priv.h"
#include "cl_dll/IGameMenuExports.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace vgui2;

void UI_KeyEvent(int key, int down);

namespace
{
enum { kRosterRows = 10 };
int TopBarHeight(int viewportHeight)
{
	return std::clamp(viewportHeight * 10 / 100, 56, 84);
}

int BottomBarHeight(int viewportHeight)
{
	return std::clamp(viewportHeight * 8 / 100, 42, 64);
}

const char *ModeLabel(int mode)
{
	switch (mode)
	{
	case 1:
		return "Locked Chase";
	case 2:
		return "Free Chase";
	case 3:
		return "Roaming";
	case 4:
		return "First Person";
	case 5:
		return "Free Map";
	case 6:
		return "Chase Map";
	default:
		return "Spectator";
	}
}

class CBar : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CBar, Panel);

public:
	CBar(Panel *parent, const char *name, bool accent)
		: BaseClass(parent, name)
		, m_accent(accent)
	{
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		SetBgColor(HudFrameLook::Bar());
		SetBorder(nullptr);
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (vgui2::surface() && w > 0 && h > 0)
		{
			vgui2::surface()->DrawSetColor(HudFrameLook::Bar());
			vgui2::surface()->DrawFilledRect(0, 0, w, h);
			if (m_accent)
				HudFrameLook::PaintTeamAccent(w, h);
		}
	}

private:
	bool m_accent = false;
};

class CSpectatorHudPanel : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CSpectatorHudPanel, Panel);

public:
	explicit CSpectatorHudPanel(Panel *parent) : BaseClass(parent, "SpectatorHud")
	{
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(false);
		SetKeyBoardInputEnabled(false);
		SetVisible(false);

		m_top = new CBar(this, "TopBar", true);
		m_bottom = new CBar(this, "BottomBar", false);
		m_tRoster = new CBar(this, "TRoster", false);
		m_ctRoster = new CBar(this, "CTRoster", false);
		m_tName = new Label(m_top, "TName", "Terrorists");
		m_tScore = new Label(m_top, "TScore", "0");
		m_timer = new Label(m_top, "Timer", "0:00");
		m_ctScore = new Label(m_top, "CTScore", "0");
		m_ctName = new Label(m_top, "CTName", "Counter-Terrorists");
		m_map = new Label(m_top, "Map", "");
		m_mode = new Label(m_bottom, "Mode", "");
		m_player = new Label(m_bottom, "Player", "");
		for (int i = 0; i < kRosterRows; ++i)
		{
			char name[24];
			std::snprintf(name, sizeof(name), "TPlayer%d", i);
			m_tPlayers[i] = new Label(m_tRoster, name, "");
			std::snprintf(name, sizeof(name), "CTPlayer%d", i);
			m_ctPlayers[i] = new Label(m_ctRoster, name, "");
			m_tPlayers[i]->SetContentAlignment(Label::a_west);
			m_ctPlayers[i]->SetContentAlignment(Label::a_west);
			m_tPlayers[i]->SetTextInset(8, 0);
			m_ctPlayers[i]->SetTextInset(8, 0);
		}

		Label *labels[] = {m_tName, m_tScore, m_timer, m_ctScore, m_ctName, m_map, m_mode, m_player};
		for (Label *lab : labels)
		{
			lab->SetPaintBackgroundEnabled(false);
			lab->SetMouseInputEnabled(false);
		}
		m_tName->SetContentAlignment(Label::a_east);
		m_tScore->SetContentAlignment(Label::a_center);
		m_timer->SetContentAlignment(Label::a_center);
		m_ctScore->SetContentAlignment(Label::a_center);
		m_ctName->SetContentAlignment(Label::a_west);
		m_map->SetContentAlignment(Label::a_east);
		m_mode->SetContentAlignment(Label::a_west);
		m_player->SetContentAlignment(Label::a_center);
	}

	void ApplySchemeSettings(IScheme *scheme) override
	{
		BaseClass::ApplySchemeSettings(scheme);
		SetBgColor(Color(0, 0, 0, 0));
		SetBorder(nullptr);
		StyleLabels(scheme);
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		const int topH = TopBarHeight(h);
		const int botH = BottomBarHeight(h);
		HudFrameLook::PaintBroadcastFrame(w, h, topH, botH, w / 90);
	}

	void PerformLayout() override
	{
		int w = 0, h = 0;
		HudFrameLook::ViewportSize(w, h, GetParent());
		SetBounds(0, 0, w, h);
		const int topH = TopBarHeight(h);
		const int botH = BottomBarHeight(h);
		m_top->SetBounds(0, 0, w, topH);
		m_bottom->SetBounds(0, h - botH, w, botH);
		const int rosterW = std::clamp(w * 19 / 100, 140, 250);
		const int rosterY = topH + h / 18;
		const int rosterRowH = std::clamp(h / 24, 20, 32);
		const int rosterH = std::max(m_tCount, m_ctCount) * rosterRowH;
		m_tRoster->SetBounds(12, rosterY, rosterW, rosterH);
		m_ctRoster->SetBounds(w - rosterW - 12, rosterY, rosterW, rosterH);
		m_tRoster->SetVisible(m_tCount > 0);
		m_ctRoster->SetVisible(m_ctCount > 0);
		for (int i = 0; i < kRosterRows; ++i)
		{
			m_tPlayers[i]->SetBounds(0, i * rosterRowH, rosterW, rosterRowH);
			m_ctPlayers[i]->SetBounds(0, i * rosterRowH, rosterW, rosterRowH);
		}

		const int pad = std::clamp(w / 40, 12, 32);
		const int scoreW = std::clamp(w / 14, 48, 82);
		const int timerW = std::clamp(w / 9, 84, 132);
		const int nameW = std::clamp(w / 5, 132, 260);
		const int metaH = 17;
		const int cy = metaH;
		const int ch = topH - metaH - 3;
		const int mid = w / 2;
		m_timer->SetBounds(mid - timerW / 2, cy, timerW, ch);
		m_tScore->SetBounds(mid - timerW / 2 - scoreW - 8, cy, scoreW, ch);
		m_ctScore->SetBounds(mid + timerW / 2 + 8, cy, scoreW, ch);
		m_tName->SetBounds(mid - timerW / 2 - scoreW - 8 - nameW - 8, cy, nameW, ch);
		m_ctName->SetBounds(mid + timerW / 2 + 8 + scoreW + 8, cy, nameW, ch);
		// Map information has its own metadata row. It must never overlap the
		// Counter-Terrorist name, even on the classic 800x600 viewport.
		m_map->SetBounds(w - pad - std::clamp(w / 4, 150, 280), 1,
			std::clamp(w / 4, 150, 280), metaH - 2);

		const int bcy = botH / 6;
		const int bch = botH * 2 / 3;
		m_mode->SetBounds(pad, bcy, w / 4, bch);
		m_player->SetBounds(w / 4, bcy, w / 2, bch);
		StyleLabels(GetScheme() ? scheme()->GetIScheme(GetScheme()) : nullptr);
	}

	void Apply(const SpectatorHudState &s)
	{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%d", s.tScore);
		m_tScore->SetText(buf);
		std::snprintf(buf, sizeof(buf), "%d", s.ctScore);
		m_ctScore->SetText(buf);
		m_timer->SetText(s.timer[0] ? s.timer : "0:00");
		m_map->SetText(s.map);
		m_mode->SetText(ModeLabel(s.observerMode));
		if (s.player[0])
		{
			char line[96];
			std::snprintf(line, sizeof(line), "%s   %d", s.player, s.health);
			m_player->SetText(line);
		}
		else
			m_player->SetText("");
		m_tCount = 0;
		m_ctCount = 0;
		for (int i = 0; i < kRosterRows; ++i)
		{
			m_tPlayers[i]->SetVisible(false);
			m_ctPlayers[i]->SetVisible(false);
		}
		for (int i = 0; i < s.playerCount && i < CSRETRO_SCOREBOARD_PLAYERS; ++i)
		{
			const ScoreboardPlayerRow &row = s.players[i];
			Label **target = nullptr;
			int *count = nullptr;
			if (row.team == 1 && m_tCount < kRosterRows)
			{
				target = m_tPlayers;
				count = &m_tCount;
			}
			else if (row.team == 2 && m_ctCount < kRosterRows)
			{
				target = m_ctPlayers;
				count = &m_ctCount;
			}
			if (!target || !count)
				continue;
			char line[96];
			std::snprintf(line, sizeof(line), "%s   %d / %d", row.name, row.frags, row.deaths);
			target[*count]->SetText(line);
			target[*count]->SetVisible(true);
			++*count;
		}
		m_state = s;
		InvalidateLayout();
	}

	const SpectatorHudState &State() const { return m_state; }

	bool TitleLooksLocalized() const
	{
		char text[64] = {0};
		m_tName->GetText(text, sizeof(text));
		return text[0] && text[0] != '#';
	}

	void StyleLabels(IScheme *scheme)
	{
		vgui2::HFont body = INVALID_FONT;
		vgui2::HFont title = INVALID_FONT;
		if (scheme)
		{
			body = scheme->GetFont("DefaultLarge", IsProportional());
			if (body == INVALID_FONT)
				body = scheme->GetFont("Default", IsProportional());
			title = scheme->GetFont("MenuLarge", IsProportional());
			if (title == INVALID_FONT)
				title = body;
		}
		Label *all[] = {m_tName, m_tScore, m_timer, m_ctScore, m_ctName, m_map, m_mode, m_player};
		for (Label *lab : all)
		{
			if (body != INVALID_FONT)
				lab->SetFont(body);
			HudFrameLook::StyleHudLabel(lab, HudFrameLook::Text());
		}
		if (title != INVALID_FONT)
		{
			m_timer->SetFont(title);
			m_tScore->SetFont(title);
			m_ctScore->SetFont(title);
		}
		HudFrameLook::StyleHudLabel(m_tName, HudFrameLook::Terror());
		HudFrameLook::StyleHudLabel(m_tScore, HudFrameLook::Terror());
		HudFrameLook::StyleHudLabel(m_ctName, HudFrameLook::CT());
		HudFrameLook::StyleHudLabel(m_ctScore, HudFrameLook::CT());
		HudFrameLook::StyleHudLabel(m_map, HudFrameLook::TextDim());
		HudFrameLook::StyleHudLabel(m_mode, HudFrameLook::TextDim());
		for (int i = 0; i < kRosterRows; ++i)
		{
			if (body != INVALID_FONT)
			{
				m_tPlayers[i]->SetFont(body);
				m_ctPlayers[i]->SetFont(body);
			}
			HudFrameLook::StyleHudLabel(m_tPlayers[i], HudFrameLook::Terror());
			HudFrameLook::StyleHudLabel(m_ctPlayers[i], HudFrameLook::CT());
		}
	}

private:
	CBar *m_top = nullptr;
	CBar *m_bottom = nullptr;
	CBar *m_tRoster = nullptr;
	CBar *m_ctRoster = nullptr;
	Label *m_tName = nullptr;
	Label *m_tScore = nullptr;
	Label *m_timer = nullptr;
	Label *m_ctScore = nullptr;
	Label *m_ctName = nullptr;
	Label *m_map = nullptr;
	Label *m_mode = nullptr;
	Label *m_player = nullptr;
	Label *m_tPlayers[kRosterRows]{};
	Label *m_ctPlayers[kRosterRows]{};
	int m_tCount = 0;
	int m_ctCount = 0;
	SpectatorHudState m_state{};
};

CSpectatorHudPanel *g_panel = nullptr;
Panel *g_host = nullptr;
} // namespace

void SpectatorHud_Set(const SpectatorHudState *state)
{
	if (state)
		InGameRoster_StoreSpectator(*state);
	if (!state || state->observerMode <= 0)
	{
		SpectatorHud_Hide();
		return;
	}
	if (gMenuVisible || TeamSelect_IsActive() || ClassSelect_IsActive() || BuySelect_IsActive())
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
		g_panel = new CSpectatorHudPanel(root);
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
	g_panel->SetZPos(0);
	if (!was)
	{
		Menu_Con("CSRETRO_SPEC_VGUI open map=%s t=%d ct=%d mode=%d",
			state->map, state->tScore, state->ctScore, state->observerMode);
		Menu_Con("CSRETRO_SPEC_LOOK frame=1");
	}
}

void SpectatorHud_Hide()
{
	if (g_panel && g_panel->IsVisible())
		Menu_Con("CSRETRO_SPEC_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
}

bool SpectatorHud_IsActive()
{
	return g_panel && g_panel->IsVisible();
}

void SpectatorHud_Shutdown()
{
	g_panel = nullptr;
	g_host = nullptr;
}

void SpectatorHud_BindHost(Panel *root)
{
	g_host = root;
	if (g_panel && root)
		g_panel->SetParent(root);
}

void SpectatorHud_GateTick()
{
	if (!getenv("CSRETRO_SPEC_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	if (step >= 99)
		return;

	auto failDone = [](const char *why) {
		Menu_Con("CSRETRO_SPEC_GATE_FAIL %s", why);
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_SPEC_GATE_DONE");
		step = 99;
	};

	if (step == 0)
	{
		if (!TeamSelect_IsActive())
			return;
		++hold;
		if (hold < 20)
			return;
		UI_KeyEvent('6', 1);
		UI_KeyEvent('6', 0);
		++step;
		hold = 0;
		return;
	}

	if (step == 1)
	{
		++hold;
		if (hold == 90)
			MenuEngine::ClientCmdNow("spectate\n");
		if (SpectatorHud_IsActive() && g_panel)
		{
			if (hold < 40)
				return;
			const SpectatorHudState &s = g_panel->State();
			Menu_Con("CSRETRO_SPEC_GATE_OPEN visible=1 map=%s t=%d ct=%d mode=%d raw=%d",
				s.map, s.tScore, s.ctScore, s.observerMode,
				g_panel->TitleLooksLocalized() ? 0 : 1);
			// Keep the diagnostic log, but remove developer notify lines from the
			// image so the complete top bar is actually reviewable.
			MenuEngine::ClientCmdNow("developer 0; clear\n");
			step = 2;
			hold = 0;
			return;
		}
		if (hold > 480)
			failDone("spectator fehlt");
	}

	if (step == 2)
	{
		// Let the startup console finish retracting before capturing the HUD.
		if (++hold < 120)
			return;
		MenuEngine::ClientCmd("screenshot\n");
		step = 3;
		hold = 0;
		return;
	}

	if (step == 3)
	{
		// The screenshot command is queued by the engine. Give it enough frames
		// to render and flush before shutdown, otherwise a visual gate can pass
		// without producing an image.
		if (++hold < 20)
			return;
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_SPEC_GATE_DONE");
		step = 99;
	}
}
