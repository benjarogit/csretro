#include "RadioSelectPanel.h"

#include "BuySelectPanel.h"
#include "ClassSelectPanel.h"
#include "Controls/MenuEngine.h"
#include "TeamSelectPanel.h"

#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/KeyCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

#include "../src/menu_priv.h"
#include "cdll_dll.h"
#include "keydefs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

using namespace vgui2;

extern vgui2::ILocalize *g_pVGuiLocalize;

void UI_KeyEvent(int key, int down);


namespace
{
struct RadioItem
{
	int slot;
	const char *alias;
	const char *token;
	const char *fallback;
};

// Texte = Steam cstrike/titles.txt RadioA/B/C (CS 1.6). Aliase = ReGameDLL radioInfo[].
const RadioItem kRadioA[] = {
	{1, "coverme", "Cover_me", "Cover Me"},
	{2, "takepoint", "You_take_the_point", "You Take the Point"},
	{3, "holdpos", "Hold_this_position", "Hold This Position"},
	{4, "regroup", "Regroup_team", "Regroup Team"},
	{5, "followme", "Follow_me", "Follow Me"},
	{6, "takingfire", "Taking_fire", "Taking Fire, Need Assistance"},
};

const RadioItem kRadioB[] = {
	{1, "go", "Go_go_go", "Go"},
	{2, "fallback", "Team_fall_back", "Fall Back"},
	{3, "sticktog", "Stick_together_team", "Stick Together Team"},
	{4, "getinpos", "Get_in_position_and_wait", "Get in Position"},
	{5, "stormfront", "Storm_the_front", "Storm the Front"},
	{6, "report", "Report_in_team", "Report In"},
};

const RadioItem kRadioC[] = {
	{1, "roger", "Affirmative", "Affirmative/Roger"},
	{2, "enemyspot", "Enemy_spotted", "Enemy Spotted"},
	{3, "needbackup", "Need_backup", "Need Backup"},
	{4, "sectorclear", "Sector_clear", "Sector Clear"},
	{5, "inposition", "In_position", "I'm in Position"},
	{6, "reportingin", "Reporting_in", "Reporting In"},
	{7, "getout", "Get_out_of_there", "She's gonna Blow!"},
	{8, "negative", "Negative", "Negative"},
	{9, "enemydown", "Enemy_down", "Enemy Down"},
};

int SlotBit(int slot)
{
	if (slot <= 0)
		return 0;
	if (slot == 10)
		return MENU_KEY_0;
	return 1 << (slot - 1);
}

const RadioItem *ItemsForType(int menuType, int *count)
{
	if (menuType == MENU_RADIOB)
	{
		*count = static_cast<int>(sizeof(kRadioB) / sizeof(kRadioB[0]));
		return kRadioB;
	}
	if (menuType == MENU_RADIOC)
	{
		*count = static_cast<int>(sizeof(kRadioC) / sizeof(kRadioC[0]));
		return kRadioC;
	}
	*count = static_cast<int>(sizeof(kRadioA) / sizeof(kRadioA[0]));
	return kRadioA;
}

const char *TitleForType(int menuType)
{
	if (menuType == MENU_RADIOB)
		return "Group Radio Commands";
	if (menuType == MENU_RADIOC)
		return "Radio Responses/Reports";
	return "Radio Commands";
}

bool LabelHasRawToken(Label *label)
{
	if (!label)
		return false;
	char buf[128] = {};
	label->GetText(buf, sizeof(buf));
	return buf[0] == '#';
}

class CRadioSelectPanel : public EditablePanel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CRadioSelectPanel, EditablePanel);

public:
	explicit CRadioSelectPanel(Panel *parent)
		: BaseClass(parent, "RadioMenu")
	{
		SetProportional(true);
		SetPaintBackgroundEnabled(true);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);

		m_title = new Label(this, "Title", "");
		for (int i = 0; i < kMaxItems; ++i)
		{
			char name[16];
			snprintf(name, sizeof(name), "slot%d", i + 1);
			m_items[i] = new Button(this, name, "");
			m_items[i]->SetVisible(false);
		}
		m_cancel = new Button(this, "CancelButton", "");
		StyleButtons();
	}

	void Open(int menuType, int validSlots)
	{
		m_type = menuType;
		m_slots = validSlots;
		if (m_slots == 0)
			m_slots = MENU_KEY_1 | MENU_KEY_2 | MENU_KEY_3 | MENU_KEY_4 | MENU_KEY_5 | MENU_KEY_6 | MENU_KEY_0;
		ApplyItems();
		SetVisible(true);
		MoveToFront();
		RequestFocus();
		LogOpen();
	}

	int MenuType() const { return m_type; }
	int SlotMask() const { return m_slots; }

	int VisibleButtonCount() const
	{
		int n = 0;
		for (int i = 0; i < kMaxItems; ++i)
		{
			if (m_items[i] && m_items[i]->IsVisible())
				++n;
		}
		if (m_cancel && m_cancel->IsVisible())
			++n;
		return n;
	}

	bool TitleLooksLocalized() const
	{
		return m_title && m_title->IsVisible() && !LabelHasRawToken(m_title);
	}

	bool FirstItemLooksLocalized() const
	{
		return m_items[0] && m_items[0]->IsVisible() && !LabelHasRawToken(m_items[0]);
	}

	bool AnyRawToken() const
	{
		if (LabelHasRawToken(m_title))
			return true;
		for (int i = 0; i < kMaxItems; ++i)
		{
			if (m_items[i] && m_items[i]->IsVisible() && LabelHasRawToken(m_items[i]))
				return true;
		}
		return LabelHasRawToken(m_cancel);
	}

	bool ActivateSlot(int slot)
	{
		if (slot == 10)
		{
			RadioSelect_Hide();
			return true;
		}
		int n = 0;
		const RadioItem *items = ItemsForType(m_type, &n);
		for (int i = 0; i < n; ++i)
		{
			if (items[i].slot != slot)
				continue;
			if ((m_slots & SlotBit(slot)) == 0)
				return false;
			FireAlias(items[i].alias);
			return true;
		}
		return false;
	}

	void OnCommand(const char *command) override
	{
		if (!command || !command[0])
			return;
		if (!strcasecmp(command, "vguicancel"))
		{
			RadioSelect_Hide();
			return;
		}
		if (!strncasecmp(command, "radio ", 6))
		{
			FireAlias(command + 6);
			return;
		}
		BaseClass::OnCommand(command);
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (code == KEY_ESCAPE)
		{
			RadioSelect_Hide();
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

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		// Kein ClientScheme-Orange, kein Tracker-Frame. titles.txt: \y Titel, \w Zeilen.
		SetBgColor(Color(0, 0, 0, 170));
		SetBorder(nullptr);
		StyleButtons();
	}

	void PaintBackground() override
	{
		int w = 0, h = 0;
		GetSize(w, h);
		if (!surface() || w < 1 || h < 1)
			return;
		surface()->DrawSetColor(8, 8, 10, 218);
		surface()->DrawFilledRect(0, 0, w, h);
		surface()->DrawSetColor(218, 174, 54, 230);
		surface()->DrawFilledRect(0, 0, w, 2);
	}

	void PerformLayout() override
	{
		LayoutCard();
		BaseClass::PerformLayout();
	}

private:
	static const int kMaxItems = 9;

	int m_type = MENU_RADIOA;
	int m_slots = 0;
	Label *m_title = nullptr;
	Button *m_items[kMaxItems] = {};
	Button *m_cancel = nullptr;

	void FireAlias(const char *alias)
	{
		if (!alias || !alias[0])
			return;
		Menu_Con("CSRETRO_RADIO_CMD %s", alias);
		char buf[64];
		snprintf(buf, sizeof(buf), "%s\n", alias);
		MenuEngine::ClientCmdNow(buf);
		RadioSelect_Hide();
	}

	void SetSlotText(Label *label, int slot, const char *token, const char *fallback)
	{
		if (!label)
			return;
		const char *phrase = fallback;
		char localized[96] = {};
		if (token && token[0] && g_pVGuiLocalize)
		{
			if (const wchar_t *w = g_pVGuiLocalize->Find(token))
			{
				if (w[0] && w[0] != L'#')
				{
					g_pVGuiLocalize->ConvertUnicodeToANSI(w, localized, sizeof(localized));
					if (localized[0] && localized[0] != '#')
						phrase = localized;
				}
			}
		}
		const int shown = (slot == 10) ? 0 : slot;
		char line[160];
		snprintf(line, sizeof(line), "%d. %s", shown, phrase ? phrase : "");
		label->SetText(line);
	}

	void ApplyItems()
	{
		if (m_title)
			m_title->SetText(TitleForType(m_type));

		int n = 0;
		const RadioItem *items = ItemsForType(m_type, &n);
		for (int i = 0; i < kMaxItems; ++i)
		{
			Button *btn = m_items[i];
			if (!btn)
				continue;
			if (i >= n)
			{
				btn->SetVisible(false);
				btn->SetCommand("");
				continue;
			}
			const RadioItem &item = items[i];
			const bool on = (m_slots & SlotBit(item.slot)) != 0;
			SetSlotText(btn, item.slot, item.token, item.fallback);
			char cmd[48];
			snprintf(cmd, sizeof(cmd), "radio %s", item.alias);
			btn->SetCommand(cmd);
			btn->SetVisible(on);
		}
		if (m_cancel)
		{
			SetSlotText(m_cancel, 10, "Cstrike_Cancel", "Exit");
			m_cancel->SetCommand("vguicancel");
			m_cancel->SetVisible((m_slots & MENU_KEY_0) != 0);
		}
		InvalidateLayout();
	}

	void StyleButtons()
	{
		const Color title(255, 210, 64, 255);
		const Color fg(255, 255, 255, 255);
		const Color armed(255, 210, 64, 255);
		const Color bg(0, 0, 0, 0);
		const Color hover(218, 174, 54, 52);

		auto style = [&](Button *btn) {
			if (!btn)
				return;
			btn->SetPaintBackgroundEnabled(true);
			btn->SetPaintBorderEnabled(false);
			btn->SetDefaultColor(fg, bg);
			btn->SetArmedColor(armed, hover);
			btn->SetDepressedColor(armed, hover);
			btn->SetDefaultBorder(nullptr);
			btn->SetDepressedBorder(nullptr);
			btn->SetKeyFocusBorder(nullptr);
			btn->SetContentAlignment(Label::a_west);
			btn->SetTextInset(8, 0);
			btn->SetButtonActivationType(Button::ACTIVATE_ONPRESSED);
		};

		for (int i = 0; i < kMaxItems; ++i)
			style(m_items[i]);
		style(m_cancel);
		if (m_title)
		{
			m_title->SetFgColor(title);
			m_title->SetPaintBackgroundEnabled(false);
		}
	}

	void LayoutCard()
	{
		int pad = 10;
		int rowH = 22;
		int gap = 2;
		int innerW = 300;
		int marginX = 20;
		if (IsProportional() && scheme())
		{
			pad = scheme()->GetProportionalScaledValue(10);
			rowH = scheme()->GetProportionalScaledValue(22);
			gap = scheme()->GetProportionalScaledValue(2);
			innerW = scheme()->GetProportionalScaledValue(300);
			marginX = scheme()->GetProportionalScaledValue(20);
		}

		int rows = 0;
		if (m_title)
			++rows;
		for (int i = 0; i < kMaxItems; ++i)
		{
			if (m_items[i] && m_items[i]->IsVisible())
				++rows;
		}
		if (m_cancel && m_cancel->IsVisible())
			++rows;
		if (rows == 0)
			rows = 1;

		const int cardW = innerW + pad * 2;
		const int cardH = pad * 2 + rows * rowH + (rows - 1) * gap;
		int parentW = 640;
		int parentH = 480;
		if (Panel *host = GetParent())
			host->GetSize(parentW, parentH);
		int cardY = (parentH / 2) - (cardH / 2) - (rowH * 2);
		if (cardY < pad)
			cardY = pad;
		SetBounds(marginX, cardY, cardW, cardH);

		int x = pad;
		int y = pad;
		if (m_title)
		{
			m_title->SetBounds(x, y, innerW, rowH);
			y += rowH + gap;
		}
		for (int i = 0; i < kMaxItems; ++i)
		{
			if (!m_items[i] || !m_items[i]->IsVisible())
				continue;
			m_items[i]->SetBounds(x, y, innerW, rowH);
			y += rowH + gap;
		}
		if (m_cancel && m_cancel->IsVisible())
			m_cancel->SetBounds(x, y, innerW, rowH);
	}

	void LogOpen()
	{
		Menu_Con("CSRETRO_RADIO_VGUI open type=%d slots=%d buttons=%d",
			m_type, m_slots, VisibleButtonCount());
	}
};

class CRadioSelectOverlay : public Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE(CRadioSelectOverlay, Panel);

public:
	explicit CRadioSelectOverlay(Panel *parent)
		: BaseClass(parent, "RadioSelectOverlay")
	{
		SetPaintBackgroundEnabled(false);
		SetMouseInputEnabled(true);
		SetKeyBoardInputEnabled(true);
	}

	void ApplySchemeSettings(IScheme *pScheme) override
	{
		BaseClass::ApplySchemeSettings(pScheme);
		SetBgColor(Color(0, 0, 0, 0));
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
	}

	void OnKeyCodeTyped(KeyCode code) override
	{
		if (m_radio)
		{
			m_radio->OnKeyCodeTyped(code);
			return;
		}
		BaseClass::OnKeyCodeTyped(code);
	}

	CRadioSelectPanel *m_radio = nullptr;
};

CRadioSelectOverlay *g_overlay = nullptr;
CRadioSelectPanel *g_panel = nullptr;
Panel *g_host = nullptr;
} // namespace

bool RadioSelect_Show(Panel *root, int menuType, int validSlots)
{
	if (!g_overlay)
	{
		if (!root)
			return false;
		g_overlay = new CRadioSelectOverlay(root);
		g_panel = new CRadioSelectPanel(g_overlay);
		g_overlay->m_radio = g_panel;
		g_host = root;
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
	g_panel->Open(menuType, validSlots);
	return true;
}

void RadioSelect_Hide()
{
	if (g_overlay && g_overlay->IsVisible())
		Menu_Con("CSRETRO_RADIO_VGUI close");
	if (g_panel)
		g_panel->SetVisible(false);
	if (g_overlay)
		g_overlay->SetVisible(false);
}

bool RadioSelect_IsActive()
{
	return g_overlay && g_overlay->IsVisible() && g_panel && g_panel->IsVisible();
}

bool RadioSelect_ActivateSlot(int slot)
{
	return g_panel && g_panel->IsVisible() && g_panel->ActivateSlot(slot);
}

int RadioSelect_MenuType()
{
	return g_panel ? g_panel->MenuType() : 0;
}

void RadioSelect_Shutdown()
{
	g_overlay = nullptr;
	g_panel = nullptr;
	g_host = nullptr;
}

void RadioSelect_GateTick()
{
	if (!getenv("CSRETRO_RADIO_GATE"))
		return;

	static int step = 0;
	static int hold = 0;
	if (step >= 99)
		return;

	auto failDone = [](const char *why) {
		Menu_Con("CSRETRO_RADIO_GATE_FAIL %s", why);
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_RADIO_GATE_DONE");
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
		if (ClassSelect_IsActive() || TeamSelect_IsActive() || BuySelect_IsActive())
			return;
		++hold;
		if (hold < 60)
			return;
		MenuEngine::ClientCmdNow("radio1\n");
		Menu_Con("CSRETRO_RADIO_GATE_REQUEST A");
		++step;
		hold = 0;
		return;
	}

	if (step == 3)
	{
		++hold;
		if (RadioSelect_IsActive() && g_panel && g_panel->MenuType() == MENU_RADIOA)
		{
			if (hold < 30)
				return;
			Menu_Con("CSRETRO_RADIO_GATE_OPEN type=%d visible=1 buttons=%d title=%d first=%d raw=%d",
				g_panel->MenuType(), g_panel->VisibleButtonCount(),
				g_panel->TitleLooksLocalized() ? 1 : 0,
				g_panel->FirstItemLooksLocalized() ? 1 : 0,
				g_panel->AnyRawToken() ? 1 : 0);
			MenuEngine::ClientCmd("screenshot\n");
			++step;
			hold = 0;
			return;
		}
		if (hold > 180)
		{
			failDone("radio1 fehlt");
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
		Menu_Con("CSRETRO_RADIO_GATE_ESC visible=%d", RadioSelect_IsActive() ? 1 : 0);
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
		MenuEngine::ClientCmdNow("radio1\n");
		++step;
		hold = 0;
		return;
	}

	if (step == 6)
	{
		++hold;
		if (!RadioSelect_IsActive())
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
		if (RadioSelect_IsActive())
		{
			if (hold > 90)
				failDone("coverme schließt nicht");
			return;
		}
		if (hold < 90)
			return;
		MenuEngine::ClientCmdNow("radio2\n");
		Menu_Con("CSRETRO_RADIO_GATE_REQUEST B");
		++step;
		hold = 0;
		return;
	}

	if (step == 8)
	{
		++hold;
		if (RadioSelect_IsActive() && g_panel && g_panel->MenuType() == MENU_RADIOB)
		{
			if (hold < 20)
				return;
			Menu_Con("CSRETRO_RADIO_GATE_B type=%d visible=1 buttons=%d",
				g_panel->MenuType(), g_panel->VisibleButtonCount());
			UI_KeyEvent(K_ESCAPE, 1);
			UI_KeyEvent(K_ESCAPE, 0);
			++step;
			hold = 0;
			return;
		}
		if (hold > 180)
		{
			failDone("radio2 fehlt");
			return;
		}
		return;
	}

	if (step == 9)
	{
		++hold;
		if (hold < 20)
			return;
		MenuEngine::ClientCmdNow("radio3\n");
		Menu_Con("CSRETRO_RADIO_GATE_REQUEST C");
		++step;
		hold = 0;
		return;
	}

	if (step == 10)
	{
		++hold;
		if (RadioSelect_IsActive() && g_panel && g_panel->MenuType() == MENU_RADIOC)
		{
			if (hold < 20)
				return;
			Menu_Con("CSRETRO_RADIO_GATE_C type=%d visible=1 buttons=%d",
				g_panel->MenuType(), g_panel->VisibleButtonCount());
			UI_KeyEvent('1', 1);
			UI_KeyEvent('1', 0);
			++step;
			hold = 0;
			return;
		}
		if (hold > 180)
		{
			failDone("radio3 fehlt");
			return;
		}
		return;
	}

	if (step == 11)
	{
		++hold;
		if (hold < 20)
			return;
		if (getenv("CSRETRO_GATE_GRACEFUL_QUIT"))
			MenuEngine::ClientCmd("quit\n");
		Menu_Con("CSRETRO_RADIO_GATE_DONE");
		step = 99;
	}
}
